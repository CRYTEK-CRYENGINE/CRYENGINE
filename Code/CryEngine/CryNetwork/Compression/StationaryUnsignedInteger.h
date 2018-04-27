// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __STATIONARYUNSIGNEDINTEGER_H__
#define __STATIONARYUNSIGNEDINTEGER_H__

#pragma once

#include "Streams/CommStream.h"
#include "Streams/ByteStream.h"

class CNetOutputSerializeImpl;
class CNetInputSerializeImpl;

class CStationaryUnsignedInteger
{
public:
	CStationaryUnsignedInteger(uint32 nMin, uint32 nMax);
	CStationaryUnsignedInteger();

	void SetValues(uint32 nMin, uint32 nMax)
	{
		m_nMin = nMin;
		m_nMax = nMax;
		PreComputeBits();
		NET_ASSERT(m_nMax > m_nMin);
	}

	void PreComputeBits()
	{
		m_numBits = IntegerLog2_RoundUp(uint32(m_nMax - m_nMin) + 1);
	}

	bool Load(XmlNodeRef node, const string& filename, const string& child = "Params");
#if USE_MEMENTO_PREDICTORS
	bool WriteMemento(CByteOutputStream& stm) const;
	bool ReadMemento(CByteInputStream& stm) const;
	void NoMemento() const;
#endif

#if USE_ARITHSTREAM
	void   WriteValue(CCommOutputStream& stm, uint32 value) const;
	uint32 ReadValue(CCommInputStream& stm) const;
#else
	void   WriteValue(CNetOutputSerializeImpl* stm, uint32 value) const;
	uint32 ReadValue(CNetInputSerializeImpl* stm) const;
#endif
#if NET_PROFILE_ENABLE
	int GetBitCount();
#endif

private:
	uint32 Quantize(uint32 x) const;
	uint32 Dequantize(uint32 x) const;

	// static data
	uint64 m_nMin;
	uint64 m_nMax;
	uint32 m_numBits;

#if USE_MEMENTO_PREDICTORS
	// memento data
	mutable uint32 m_oldValue;
	mutable uint32 m_probabilitySame;
	mutable bool   m_haveMemento;
#endif
};

#endif
