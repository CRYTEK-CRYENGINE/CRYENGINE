// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __BOOLCOMPRESS_H__
#define __BOOLCOMPRESS_H__

#pragma once

#include "Streams/CommStream.h"
#include "Streams/ByteStream.h"

class CArithModel;

class CNetInputSerializeImpl;
class CNetOutputSerializeImpl;

class CBoolCompress
{
public:
	CBoolCompress();

#if USE_MEMENTO_PREDICTORS
	static const int   AdaptBits = 5;
	static const uint8 StateRange = (1u << AdaptBits) - 1;
	static const uint8 StateMidpoint = (1u << (AdaptBits - 1)) - 1;

	void ReadMemento(CByteInputStream& stm) const;
	void WriteMemento(CByteOutputStream& stm) const;
	void NoMemento() const;
#endif

#if USE_ARITHSTREAM
	bool ReadValue(CCommInputStream& stm) const;
	void WriteValue(CCommOutputStream& stm, bool value) const;
#else
	bool ReadValue(CNetInputSerializeImpl* stm) const;
	void WriteValue(CNetOutputSerializeImpl* stm, bool value) const;
#endif
#if NET_PROFILE_ENABLE
	int GetBitCount();
#endif

private:
#if USE_MEMENTO_PREDICTORS
	static const uint8 LAST_VALUE_BIT = 0x80;

	mutable bool  m_lastValue;
	mutable uint8 m_prob;
#endif
};

#endif
