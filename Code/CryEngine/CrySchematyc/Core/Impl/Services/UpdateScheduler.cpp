// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UpdateScheduler.h"

namespace Schematyc
{
namespace
{
enum : uint32
{
	DefaultBucketSize = 128u,
	InvalidBucketIdx = ~0u
};
}

CUpdateScheduler::SSlot::SSlot()
	: firstBucketIdx(InvalidBucketIdx)
	, frequency(EUpdateFrequency::Invalid)
	, priority(EUpdatePriority::Default)
{}

CUpdateScheduler::SSlot::SSlot(uint32 _firstBucketIdx, UpdateFrequency _frequency, UpdatePriority _priority)
	: firstBucketIdx(_firstBucketIdx)
	, frequency(_frequency)
	, priority(_priority)
{}

CUpdateScheduler::SObserver::SObserver(const CConnectionScope* _pScope, const UpdateCallback& _callback, UpdateFrequency _frequency, UpdatePriority _priority, const UpdateFilter& _filter)
	: pScope(_pScope)
	, callback(_callback)
	, frequency(_frequency)
	, currentPriority(_priority)
	, newPriority(_priority)
	, filter(_filter)
{}

CUpdateScheduler::CBucket::CBucket()
	: m_dirtyCount(0)
	, m_garbageCount(0)
	, m_pos(0)
{
	m_observers.reserve(DefaultBucketSize);
}

void CUpdateScheduler::CBucket::Connect(CConnectionScope& scope, const UpdateCallback& callback, UpdateFrequency frequency, UpdatePriority priority, const UpdateFilter& filter)
{
	m_observers.push_back(SObserver(&scope, callback, frequency, priority, filter));
	++m_dirtyCount;
}

void CUpdateScheduler::CBucket::Disconnect(CConnectionScope& scope, UpdatePriority priority)
{
	SObserver* pObserver = FindObserver(scope, priority);
	CRY_ASSERT(pObserver);
	if (pObserver)
	{
		pObserver->pScope = nullptr;
		pObserver->callback = UpdateCallback();
		pObserver->frequency = EUpdateFrequency::Invalid;
		pObserver->newPriority = EUpdatePriority::Dirty;
		pObserver->filter = UpdateFilter();
		++m_garbageCount;
	}
}

void CUpdateScheduler::CBucket::BeginUpdate()
{
	// Sort observer bindings in order of priority and memory address.
	// The sort process automatically pushes dirty observer bindings to the back so they can be erased in a single block.
	if ((m_dirtyCount > 0) || (m_garbageCount > 0))
	{
		ObserverVector::iterator itBeginObserver = m_observers.begin();
		ObserverVector::iterator itEndObserver = m_observers.end();
		for (ObserverVector::iterator itObserver = itBeginObserver; itObserver != itEndObserver; ++itObserver)
		{
			SObserver& observer = *itObserver;
			observer.currentPriority = observer.newPriority;
		}
		std::sort(itBeginObserver, itEndObserver, SSortObservers());
		if (m_garbageCount > 0)
		{
			m_observers.erase(itEndObserver - m_garbageCount, itEndObserver);
		}
		m_dirtyCount = 0;
		m_garbageCount = 0;
	}
}

void CUpdateScheduler::CBucket::Update(const SUpdateContext(&frequencyUpdateContexts)[EUpdateFrequency::Count], UpdatePriority beginPriority, UpdatePriority endPriority)
{
	// Update all remaining observers if valid and not filtered.
	const uint32 endPos = m_observers.size() - m_dirtyCount;
	for (; m_pos != endPos; ++m_pos)
	{
		const SObserver& observer = m_observers[m_pos];
		if (observer.currentPriority <= endPriority)
		{
			break;
		}
		else if (observer.currentPriority <= beginPriority)
		{
			if (observer.callback && (!observer.filter || observer.filter()))
			{
				observer.callback(frequencyUpdateContexts[observer.frequency]);
			}
		}
	}
}

void CUpdateScheduler::CBucket::EndUpdate()
{
	m_pos = 0;
}

void CUpdateScheduler::CBucket::Reset()
{
	m_observers.clear();
	m_dirtyCount = 0;
	m_garbageCount = 0;
	m_pos = 0;
}

CUpdateScheduler::SObserver* CUpdateScheduler::CBucket::FindObserver(CConnectionScope& scope, UpdatePriority priority)
{
	ObserverVector::iterator itBeginObserver = m_observers.begin();
	ObserverVector::iterator itEndObserver = m_observers.end();
	ObserverVector::iterator itFirstDirtyObserver = itEndObserver - m_dirtyCount;
	ObserverVector::iterator itObserver = std::lower_bound(itBeginObserver, itFirstDirtyObserver, priority, SLowerBoundObserver());
	if (itObserver != itFirstDirtyObserver)
	{
		for (; itObserver->currentPriority == priority; ++itObserver)
		{
			SObserver& observer = *itObserver;
			if (observer.pScope == &scope)
			{
				return &observer;
			}
		}
	}
	for (itObserver = itFirstDirtyObserver; itObserver != itEndObserver; ++itObserver)
	{
		SObserver& observer = *itObserver;
		if (observer.pScope == &scope)
		{
			return &observer;
		}
	}
	return nullptr;
}

CUpdateScheduler::CUpdateScheduler()
	: m_currentBucketIdx(0)
	, m_bInFrame(false)
{
	for (uint32 bucketIdx = 0; bucketIdx < BucketCount; ++bucketIdx)
	{
		m_frameTimes[bucketIdx] = 0.0f;
	}
}

CUpdateScheduler::~CUpdateScheduler()
{
	for (SSlot& slot : m_slots)
	{
		slot.connection.Disconnect();
	}
}

bool CUpdateScheduler::Connect(const SUpdateParams& params)
{
	Disconnect(params.scope);
	if ((params.frequency >= EUpdateFrequency::EveryFrame) && (params.frequency < EUpdateFrequency::Count))
	{
		// Find group of buckets containing least observers.
		const uint32 bucketStride = uint32(1) << params.frequency;
		uint32 firstBucketIdx = 0;
		if (params.frequency > EUpdateFrequency::EveryFrame)
		{
			uint32 bucketGroupSize[BucketCount] = {};
			for (uint32 bucketIdx = 0; bucketIdx < BucketCount; ++bucketIdx)
			{
				bucketGroupSize[bucketIdx % bucketStride] += m_buckets[bucketIdx].GetSize();
			}
			for (uint32 bucketGroupIdx = 1; bucketGroupIdx < bucketStride; ++bucketGroupIdx)
			{
				if (bucketGroupSize[bucketGroupIdx] < bucketGroupSize[firstBucketIdx])
				{
					firstBucketIdx = bucketGroupIdx;
				}
			}
		}
		// Connect to slot and to appropriate buckets.
		m_slots.push_back(SSlot(static_cast<uint32>(firstBucketIdx), params.frequency, params.priority));
		m_slots.back().connection.Connect(params.scope, SCHEMATYC_MEMBER_DELEGATE(&CUpdateScheduler::Disconnect, *this));
		for (CBucket* pBucket = m_buckets + firstBucketIdx, * pEndBucket = m_buckets + BucketCount; pBucket < pEndBucket; pBucket += bucketStride)
		{
			pBucket->Connect(params.scope, params.callback, params.frequency, params.priority, params.filter);
		}
		return true;
	}
	return false;
}

void CUpdateScheduler::Disconnect(CConnectionScope& scope)
{
	if (scope.HasConnections())
	{
		SlotVector::iterator itSlot = std::find_if(m_slots.begin(), m_slots.end(), SFindSlot(&scope));
		if (itSlot != m_slots.end())
		{
			// Disconnect from appropriate buckets.
			const uint32 bucketStride = uint32(1) << itSlot->frequency;
			const UpdatePriority priority = itSlot->priority;
			for (CBucket* pBucket = m_buckets + itSlot->firstBucketIdx, * pEndBucket = m_buckets + BucketCount; pBucket < pEndBucket; pBucket += bucketStride)
			{
				pBucket->Disconnect(scope, priority);
			}
			m_slots.erase(itSlot);
		}
	}
}

bool CUpdateScheduler::InFrame() const
{
	return m_bInFrame;
}

bool CUpdateScheduler::BeginFrame(float frameTime)
{
	CRY_ASSERT(!m_bInFrame);
	if (!m_bInFrame)
	{
		m_buckets[m_currentBucketIdx].BeginUpdate();
		m_frameTimes[m_currentBucketIdx] = frameTime;
		m_bInFrame = true;
		return true;
	}
	return false;
}

bool CUpdateScheduler::Update(UpdatePriority beginPriority, UpdatePriority endPriority)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);
	if (m_bInFrame)
	{
		// Calculate cumulative frame times.
		SUpdateContext frequencyUpdateContexts[EUpdateFrequency::Count];
		for (UpdateFrequency frequency = EUpdateFrequency::EveryFrame; frequency < EUpdateFrequency::Count; ++frequency)
		{
			const uint32 frameCount = uint32(1) << frequency;
			for (uint32 frameIdx = 0; frameIdx < frameCount; ++frameIdx)
			{
				frequencyUpdateContexts[frequency].cumulativeFrameTime += m_frameTimes[(m_currentBucketIdx - frameIdx) % BucketCount];
			}
			frequencyUpdateContexts[frequency].frameTime = m_frameTimes[m_currentBucketIdx];
		}
		// Update contents of current bucket.
		m_buckets[m_currentBucketIdx].Update(frequencyUpdateContexts, beginPriority, endPriority);
		return true;
	}
	return false;
}

bool CUpdateScheduler::EndFrame()
{
	CRY_ASSERT(m_bInFrame);
	if (m_bInFrame)
	{
		m_buckets[m_currentBucketIdx].EndUpdate();
		m_currentBucketIdx = (m_currentBucketIdx + 1) % BucketCount;
		m_bInFrame = false;
		return true;
	}
	return false;
}

void CUpdateScheduler::VerifyCleanup() {}

void CUpdateScheduler::Reset()
{
	for (uint32 bucketIdx = 0; bucketIdx < BucketCount; ++bucketIdx)
	{
		m_buckets[bucketIdx].Reset();
		m_frameTimes[bucketIdx] = 0.0f;
	}
	m_currentBucketIdx = 0;
	m_bInFrame = false;
}
} // Schematyc
