// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __STATIONARYINTEGER_H__
#define __STATIONARYINTEGER_H__

#pragma once

#include "Streams/CommStream.h"
#include "Streams/ByteStream.h"

class CNetOutputSerializeImpl;
class CNetInputSerializeImpl;

class CStationaryInteger
{
public:
	CStationaryInteger(int32 nMin, int32 nMax);
	CStationaryInteger();

	void SetValues(int32 nMin, int32 nMax)
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
	void  WriteValue(CCommOutputStream& stm, int32 value) const;
	int32 ReadValue(CCommInputStream& stm) const;
#else
	void  WriteValue(CNetOutputSerializeImpl* stm, int32 value) const;
	int32 ReadValue(CNetInputSerializeImpl* stm) const;
#endif
#if NET_PROFILE_ENABLE
	int GetBitCount();
#endif

private:
	uint32 Quantize(int32 x) const;
	int32  Dequantize(uint32 x) const;

	// static data
	int64  m_nMin;
	int64  m_nMax;
	uint32 m_numBits;

#if USE_MEMENTO_PREDICTORS
	// memento data
	mutable uint32 m_oldValue;
	mutable uint32 m_probabilitySame;
	mutable bool   m_haveMemento;
#endif
};

#endif
