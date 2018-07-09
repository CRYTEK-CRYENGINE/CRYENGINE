// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "PredictiveFloatTracker.h"

///////////////////////////////////////////////////////////////////////////////////////////
// CPredictiveFloatTracker::Sequence
///////////////////////////////////////////////////////////////////////////////////////////
void CPredictiveFloatTracker::Sequence::Use()
{
	m_lastUseTime = gEnv->pTimer->GetAsyncTime();
}

float CPredictiveFloatTracker::Sequence::GetSecondsFromLastUse() const
{
	return gEnv->pTimer->GetAsyncTime().GetDifferenceInSeconds(m_lastUseTime);
}

///////////////////////////////////////////////////////////////////////////////////////////
// CPredictiveFloatTracker
///////////////////////////////////////////////////////////////////////////////////////////
CPredictiveFloatTracker::CPredictiveFloatTracker(const string& dir, int32 errorThreshold, float quantizationScale)
{
	m_quantizationScale = quantizationScale;
	m_errorThreshold = errorThreshold;
	m_directory = dir;
}

CPredictiveFloatTracker::~CPredictiveFloatTracker()
{
	CryAutoLock<CryMutex> lock(m_mutex);

	for (auto k : m_sequences)
		delete k.second;

	m_sequences.clear();
}

CPredictiveFloatTracker::Sequence* CPredictiveFloatTracker::GetSequence(int32 mementoId)
{
	auto it = m_sequences.find(mementoId);
	if (it != m_sequences.end())
	{
		it->second->Use();
		return it->second;
	}

	Sequence* s = new Sequence;
	s->Use();

	m_sequences[mementoId] = s;

	return s;
}

void CPredictiveFloatTracker::LogSequence(Sequence* pSequence, const string& file)
{
	FILE* f = gEnv->pCryPak->FOpen(file.c_str(), "wb");
	if (!f)
		return;

	float size = 0.f;
	for (auto e : pSequence->mEntries)
		size += e.size;

	gEnv->pCryPak->FPrintf(f, "Avg size: %3.3f\n", size / (float)pSequence->mEntries.size());

	for (auto e : pSequence->mEntries)
	{
		int32 error = e.predicted - e.quantized;
		gEnv->pCryPak->FPrintf(f, "BSEQ: % 5i SEQ: % 5i E: % 7i P: % 3.3f V: % 3.3f Q: % 3.3f DV: % 3.3f A: % 2i dT: % 7i T: %07i s: %0.3f sym: %i %i\n", e.sequenceId - e.mementoAge, e.sequenceId, error, e.predicted * m_quantizationScale, e.lastValue * m_quantizationScale, e.quantized * m_quantizationScale, (e.quantized - (int32)e.lastValue) * m_quantizationScale, e.mementoAge, e.timeFraction32 - e.lastTime, e.timeFraction32 & 0xfffff, e.size, e.symSize, e.totalSize);
	}

	gEnv->pCryPak->FClose(f);
}

void CPredictiveFloatTracker::Manage(const char* szPolicyName, int channel)
{
	m_mutex.Lock();
	std::map<int32, Sequence*> map_copy = m_sequences;
	m_mutex.Unlock();

	for (auto entry : map_copy)
	{
		string file;
		Sequence copy;
		{
			CryAutoLock<CryMutex> lock(m_mutex);

			Sequence* s = entry.second;
			file.Format("%s%s_%i_%i.log", m_directory.c_str(), szPolicyName, channel, entry.first);

			copy.mEntries = s->mEntries;
		}


		LogSequence(&copy, file);
	}
}

void CPredictiveFloatTracker::Track(int32 mementoId, int32 mementoSequence, uint32 lastValue, uint32 lastDelta, uint32 lastTime, int32 quantized, int32 predicted, uint32 mementoAge, uint32 timeFraction32, float size, uint32 symSize, uint32 totalSize)
{
/*	if (quantized == lastValue)
		return;

	if (abs(quantized - predicted) < m_errorThreshold)
		return;*/

	CryAutoLock<CryMutex> lock(m_mutex);

	SequenceEntry entry = {
		mementoSequence,
		lastValue,
		lastDelta,
		lastTime,
		quantized,
		predicted,
		mementoAge,
		timeFraction32,
		size,
		symSize, 
		totalSize
	};

	Sequence* s = GetSequence(mementoId);
	s->mEntries.push_back(entry);
}
