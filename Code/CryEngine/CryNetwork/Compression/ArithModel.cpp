// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  implementation of CryNetwork IArithModel
   -------------------------------------------------------------------------
   History:
   - 26/07/2004   10:34 : Created by Craig Tiller
*************************************************************************/
#include "StdAfx.h"

#include "ArithModel.h"

int CCheckForNullEntityEncoding::m_check = 0;

#if USE_ARITHSTREAM
	#include "ArithAlphabet.h"
	#include <CryEntitySystem/IEntity.h>
	#include <CryNetwork/INetwork.h>
	#include "ArithPrimitives.h"

	#include <CryMemory/CrySizer.h>
	#include "Context/NetContextState.h"

enum ESpecialEntityIds
{
	eSEI_NullId = 0,
	eSEI_NewId,
	NUM_SPECIAL_ENTITY_IDS
};

static ILINE unsigned CharToAlphabet(char c)
{
	return (unsigned)(unsigned char) c;
}

static ILINE char AlphabetToChar(unsigned a)
{
	return (char)(unsigned char) a;
}

CArithModel::CArithModel()
	: m_alphabet(256)
	, m_entityIDAlphabet(2)
	, m_pNetContext(nullptr)
{
	m_timeFraction32 = 0;
	ZeroMemory(&m_vTimeAdaption, sizeof(m_vTimeAdaption));
}

CArithModel::~CArithModel()
{
}

CArithModel::CArithModel(const CArithModel& mdl)
	: m_alphabet(mdl.m_alphabet)
	, m_entityIDAlphabet(mdl.m_entityIDAlphabet)
	, m_stringTable(mdl.m_stringTable)
	, m_pNetContext(nullptr)
{
	for (int i = 0; i < NUM_TIME_STREAMS; i++)
		m_vTimeAdaption[i] = mdl.m_vTimeAdaption[i];
}

CArithModel& CArithModel::operator=(const CArithModel& mdl)
{
	m_alphabet = mdl.m_alphabet;
	m_entityIDAlphabet = mdl.m_entityIDAlphabet;
	m_stringTable = mdl.m_stringTable;
	m_pNetContext = mdl.m_pNetContext;
	for (int i = 0; i < NUM_TIME_STREAMS; i++)
		m_vTimeAdaption[i] = mdl.m_vTimeAdaption[i];
	return *this;
}

void CArithModel::WriteString(CCommOutputStream& stm, const string& szString)
{
	uint32 size = szString.size();
	NET_ASSERT(size < (1 << 12));
	stm.WriteBits(size, 12);
	for (string::const_iterator iter = szString.begin(); iter != szString.end(); ++iter)
		m_alphabet.WriteSymbol(stm, CharToAlphabet(*iter));
}

void CArithModel::WriteChar(CCommOutputStream& stm, char c)
{
	m_alphabet.WriteSymbol(stm, CharToAlphabet(c));
}

bool CArithModel::ReadString(CCommInputStream& stm, string& szString)
{
	szString.resize(0);
	uint32 size = stm.ReadBits(12);
	szString.reserve(size);
	while (size--)
		szString += AlphabetToChar(m_alphabet.ReadSymbol(stm));
	NetLogPacketDebug("ReadString %s (%f)", szString.c_str(), stm.GetBitSize());
	return true;
}

char CArithModel::ReadChar(CCommInputStream& stm)
{
	return AlphabetToChar(m_alphabet.ReadSymbol(stm));
}

void CArithModel::RecalculateProbabilities()
{
	m_alphabet.RecalculateProbabilities();
	m_entityIDAlphabet.RecalculateProbabilities();

	for (int i = 0; i < NUM_TIME_STREAMS; i++)
	{
		m_vTimeAdaption[i].isInFrame = false;
	}
}

void CArithModel::GetMemoryStatistics(ICrySizer* pSizer, bool countingThis)
{
	SIZER_COMPONENT_NAME(pSizer, "CArithModel");

	if (countingThis)
		pSizer->Add(*this);
	m_alphabet.GetMemoryStatistics(pSizer);
	m_entityIDAlphabet.GetMemoryStatistics(pSizer);
}

void CArithModel::STimeAdaption::STimeDelta::Update(uint16 encodedDelta, bool hit)
{
	//	NetLog( "in - rate:%.4x error:%.4x encodedDelta:%.4x [%s]",
	//		rate, error, encodedDelta, hit? "hit" : "miss" );

	uint32 newRate = (7 * rate + encodedDelta) / 8;

	if (newRate == rate)
	{
		if (encodedDelta > newRate)
			newRate++;
		else if (encodedDelta < newRate)
			newRate--;
	}

	int32 e = abs((int32)rate - (int32)encodedDelta);
	rate = newRate;

	if (e < 5 && error > 50) //after Reset() when calculated error is low again we don't want to wait very long until error decays with 7/8 interpolation
		error /= 2;
	else
		error = min((7 * error + e * 2) / 8, 32767u);

	//	NetLog("%.4x -> %.4x %.4x %s", encodedDelta, rate, error, hit?"hit":"miss");
}

void CArithModel::STimeAdaption::STimeDelta::Reset()
{
	error = 16384;
	rate = 32768;
}

bool CArithModel::IsTimeInFrame(ETimeStream time) const
{
	const STimeAdaption& adapt = m_vTimeAdaption[time];

	return adapt.isInFrame;
}

int64 CArithModel::WriteGetTimeDelta(ETimeStream time, const CTimeValue& value, STimeAdaption::STimeDelta** pOutDeltaInfo)
{
	STimeAdaption& adapt = m_vTimeAdaption[time];

	CTimeValue oldTime;
	STimeAdaption::STimeDelta* pDeltaInfo;
	if (!adapt.isInFrame)
	{
		oldTime = adapt.startTime;
		adapt.startTime = value;
		pDeltaInfo = &adapt.startDelta;
	}
	else
	{
		oldTime = adapt.lastTime;
		pDeltaInfo = &adapt.frameDelta;
	}

	if (pOutDeltaInfo)
		*pOutDeltaInfo = pDeltaInfo;

#if DEBUG_TIME_COMPRESSION
	stm.WriteBitsLarge(oldTime.GetMilliSecondsAsInt64(), 64);
#endif

	int32 dt = 0;
	if (adapt.timeFraction32 != 0)
		dt = (m_timeFraction32 - adapt.timeFraction32) * 1000 / (int32)REPLICATION_TIME_PRECISION;

	adapt.timeFraction32 = m_timeFraction32;

	int64 delta = value.GetMilliSecondsAsInt64() - (oldTime.GetMilliSecondsAsInt64() + (int64)dt);

	if (delta < -32768 || delta > 32767)
		pDeltaInfo->Reset();

	adapt.isInFrame = true;
	adapt.lastTime = value;

	return delta;
}

void CArithModel::WriteTime(CCommOutputStream& stm, ETimeStream time, CTimeValue value)
{
	STimeAdaption::STimeDelta* pDeltaInfo;
	int64 delta = WriteGetTimeDelta(time, value, &pDeltaInfo);
	uint16 encodedDelta;
	bool isLarge = false;
	if (delta < -32768)
		isLarge = true;
	else if (delta > 32767)
		isLarge = true;
	else
		encodedDelta = uint16(delta + 32768);

	if (isLarge)
	{
		stm.EncodeShift(16, 65535, 1);
		stm.WriteBitsLarge(value.GetMilliSecondsAsInt64(), 64);
	}
	else
	{
		bool hit;
		stm.EncodeShift(16, 0, 65535);

	#if DEBUG_TIME_COMPRESSION
		stm.WriteBits(encodedDelta, 16);
		stm.WriteBits(pDeltaInfo->Left(), 16);
		stm.WriteBits(pDeltaInfo->Right(), 16);

		NET_ASSERT(oldTime.GetMilliSecondsAsInt64() + encodedDelta - 32768 == value.GetMilliSecondsAsInt64());
	#endif

		SquarePulseProbabilityWriteImproved(stm,
		                                    encodedDelta, pDeltaInfo->Left(), pDeltaInfo->Right(), 102400, 65535, 99, 16, &hit);
		//		NetLog( "WRITE: encodedDelta:%.4x rate:%.4x error:%.4x [%s]",
		//			encodedDelta, pDeltaInfo->rate, pDeltaInfo->error, hit? "hit" : "miss" );
		
		pDeltaInfo->Update(encodedDelta, hit);
	}

	#if DEBUG_TIME_COMPRESSION
	stm.WriteBitsLarge(value.GetMilliSecondsAsInt64(), 64);
	#endif

	//	NetLog( "WRITE %.16I64x goesto %.16I64x", oldTime.GetMilliSecondsAsInt64(), value.GetMilliSecondsAsInt64() );
}

CTimeValue CArithModel::ReadTimeWithDelta(CCommInputStream& stm, ETimeStream time, int64 delta)
{
	STimeAdaption& adapt = m_vTimeAdaption[time];

	CTimeValue oldTime;
	STimeAdaption::STimeDelta* pDeltaInfo;
	if (!adapt.isInFrame)
	{
		oldTime = adapt.startTime;
		pDeltaInfo = &adapt.startDelta;
	}
	else
	{
		oldTime = adapt.lastTime;
		pDeltaInfo = &adapt.frameDelta;
	}

#if DEBUG_TIME_COMPRESSION
	int64 remoteOldTime = stm.ReadBitsLarge(64);
	NET_ASSERT(oldTime.GetMilliSecondsAsInt64() == remoteOldTime);
	uint16 reallyEncoded, left, right;
#endif

	int32 dt = 0;
	if (adapt.timeFraction32 != 0)
		dt = (m_timeFraction32 - adapt.timeFraction32) * 1000 / (int32)REPLICATION_TIME_PRECISION;

	adapt.timeFraction32 = m_timeFraction32;

	int64 millis = (oldTime.GetMilliSecondsAsInt64() + (int64)dt) + delta;

	if (!adapt.isInFrame)
		adapt.startTime.SetMilliSeconds(millis);
	adapt.isInFrame = true;
	adapt.lastTime.SetMilliSeconds(millis);

	CTimeValue out;
	out.SetMilliSeconds(millis);

#if DEBUG_TIME_COMPRESSION
	int64 checkMillis = stm.ReadBitsLarge(64);
	NET_ASSERT(out.GetMilliSecondsAsInt64() == checkMillis);
#endif

	return out;
}

CTimeValue CArithModel::ReadTime(CCommInputStream& stm, ETimeStream time)
{
	STimeAdaption& adapt = m_vTimeAdaption[time];

	CTimeValue oldTime;
	STimeAdaption::STimeDelta* pDeltaInfo;
	if (!adapt.isInFrame)
	{
		oldTime = adapt.startTime;
		pDeltaInfo = &adapt.startDelta;
	}
	else
	{
		oldTime = adapt.lastTime;
		pDeltaInfo = &adapt.frameDelta;
	}

	#if DEBUG_TIME_COMPRESSION
	int64 remoteOldTime = stm.ReadBitsLarge(64);
	NET_ASSERT(oldTime.GetMilliSecondsAsInt64() == remoteOldTime);
	uint16 reallyEncoded, left, right;
	#endif

	int32 dt = 0;
	if (adapt.timeFraction32 != 0)
		dt = (m_timeFraction32 - adapt.timeFraction32) * 1000 / (int32)REPLICATION_TIME_PRECISION;

	adapt.timeFraction32 = m_timeFraction32;

	bool isLarge = stm.DecodeShift(16) == 65535;
	int64 millis;
	uint16 encodedDelta; // so it can be read later, in a debugger
	if (isLarge)
	{
		stm.UpdateShift(16, 65535, 1);
		millis = stm.ReadBitsLarge(64);
		pDeltaInfo->Reset();
	}
	else
	{
		stm.UpdateShift(16, 0, 65535);

	#if DEBUG_TIME_COMPRESSION
		reallyEncoded = stm.ReadBits(16);
		left = stm.ReadBits(16);
		right = stm.ReadBits(16);
		NET_ASSERT(left == pDeltaInfo->Left());
		NET_ASSERT(right == pDeltaInfo->Right());
	#endif

		bool hit;
		uint32 value;
		if (!SquarePulseProbabilityReadImproved(value, stm,
		                                        pDeltaInfo->Left(), pDeltaInfo->Right(), 102400, 65535, 99, 16, &hit))
			encodedDelta = 32767;
		else
			encodedDelta = value;
		//		NetLog( "READ: encodedDelta:%.4x rate:%.4x error:%.4x [%s]",
		//			encodedDelta, pDeltaInfo->rate, pDeltaInfo->error, hit? "hit" : "miss" );
		millis = (oldTime.GetMilliSecondsAsInt64() + (int64)dt) + encodedDelta - 32768;
		pDeltaInfo->Update(encodedDelta, hit);

	#if DEBUG_TIME_COMPRESSION
		NET_ASSERT(encodedDelta == reallyEncoded);
	#endif
	}

	if (!adapt.isInFrame)
		adapt.startTime.SetMilliSeconds(millis);
	adapt.isInFrame = true;
	adapt.lastTime.SetMilliSeconds(millis);

	//	NetLog( "READ %.16I64x goesto %.16I64x", oldTime.GetMilliSecondsAsInt64(), millis );

	CTimeValue out;
	out.SetMilliSeconds(millis);

	#if DEBUG_TIME_COMPRESSION
	int64 checkMillis = stm.ReadBitsLarge(64);
	NET_ASSERT(out.GetMilliSecondsAsInt64() == checkMillis);
	#endif

	return out;
}

float CArithModel::EstimateSizeOfTime(ETimeStream time, CTimeValue value) const
{
	float sum = 0.0f;

	const STimeAdaption& adapt = m_vTimeAdaption[time];

	CTimeValue oldTime;
	const STimeAdaption::STimeDelta* pDeltaInfo;
	if (!adapt.isInFrame)
	{
		oldTime = adapt.startTime;
		pDeltaInfo = &adapt.startDelta;
	}
	else
	{
		oldTime = adapt.lastTime;
		pDeltaInfo = &adapt.frameDelta;
	}

	int64 delta = (value - oldTime).GetMilliSecondsAsInt64();
	uint16 encodedDelta;
	bool isLarge = false;
	if (delta < -32768)
		isLarge = true;
	else if (delta > 32767)
		isLarge = true;
	else
		encodedDelta = uint16(delta + 32768);

	if (isLarge)
	{
		sum += CCommOutputStream::EstimateArithSizeInBitsShift(16, 1);
		sum += 64.0f;
	}
	else
	{
		sum += CCommOutputStream::EstimateArithSizeInBitsShift(16, 65535);
		sum += SquarePulseProbabilityEstimate(
		  encodedDelta, pDeltaInfo->Left(), pDeltaInfo->Right(), 1, 65535);
	}

	return sum;
}

void CArithModel::WriteNetId(CCommOutputStream& stm, SNetObjectID netID)
{
	if (!netID)
	{
		m_entityIDAlphabet.WriteSymbol(stm, eSEI_NullId);
		CCheckForNullEntityEncoding::EncodedNull();
	}
	else
	{
		if (netID.id + unsigned(NUM_SPECIAL_ENTITY_IDS) >= m_entityIDAlphabet.GetNumSymbols())
		{
			m_entityIDAlphabet.WriteSymbol(stm, eSEI_NewId);
			stm.WriteBits(netID.id, 16);
			m_entityIDAlphabet.Resize(netID.id + 1 + NUM_SPECIAL_ENTITY_IDS);
		}
		else
		{
			m_entityIDAlphabet.WriteSymbol(stm, netID.id + NUM_SPECIAL_ENTITY_IDS);
		}
	}
}

SNetObjectID CArithModel::ReadNetId(CCommInputStream& stm)
{
	uint32 sym = m_entityIDAlphabet.ReadSymbol(stm);
	SNetObjectID netID;
	switch (sym)
	{
	case eSEI_NullId:
		break;
	case eSEI_NewId:
		netID.id = stm.ReadBits(16);
		m_entityIDAlphabet.Resize(netID.id + 1 + NUM_SPECIAL_ENTITY_IDS);
		break;
	default:
		netID.id = sym - NUM_SPECIAL_ENTITY_IDS;
		break;
	}
	m_pNetContext->Resaltify(netID);
	return netID;
}

size_t CArithModel::GetSize()
{
	return sizeof(*this) /*+ m_alphabet.GetMemorySize() + m_entityIDAlphabet.GetMemorySize()*/;
}

void CArithModel::SetTimeFraction(uint32 timeFraction32)
{
	m_timeFraction32 = timeFraction32;
}

uint32 CArithModel::GetTimeFraction() const
{
	return m_timeFraction32;
}

#endif
