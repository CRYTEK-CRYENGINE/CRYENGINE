// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __BOOLCOMPRESS2_H__
#define __BOOLCOMPRESS2_H__

#pragma once

#include "Streams/ByteStream.h"
#include "Streams/CommStream.h"

class CNetInputSerializeImpl;
class CNetOutputSerializeImpl;

class CBoolCompress2
{
public:
	CBoolCompress2() {}
#if USE_MEMENTO_PREDICTORS
	struct TMemento
	{
		uint8 prob;
		bool  lastValue;
	};

	static const int   AdaptBits = 5;
	static const uint8 StateRange = (1u << AdaptBits) - 1;
	static const uint8 StateMidpoint = (1u << (AdaptBits - 1)) - 1;

	void ReadMemento(TMemento& memento, CByteInputStream& in) const;
	void WriteMemento(TMemento memento, CByteOutputStream& out) const;
	void InitMemento(TMemento& memento) const;

	void UpdateMemento(TMemento& memento, bool newValue) const;
#endif

#if USE_ARITHSTREAM
	bool ReadValue(TMemento memento, CCommInputStream& in, bool& value) const;
	bool WriteValue(TMemento memento, CCommOutputStream& out, bool value) const;
#else
	#if USE_MEMENTO_PREDICTORS
	bool ReadValue(TMemento memento, CNetInputSerializeImpl* in, bool& value) const;
	bool WriteValue(TMemento memento, CNetOutputSerializeImpl* out, bool value) const;
	#else
	bool ReadValue(CNetInputSerializeImpl* in, bool& value) const;
	bool WriteValue(CNetOutputSerializeImpl* out, bool value) const;
	#endif
#endif
#if NET_PROFILE_ENABLE
	int GetBitCount();
#endif

private:
#if USE_MEMENTO_PREDICTORS
	static const uint8 LAST_VALUE_BIT = 0x80;

	struct SSymLow;
	SSymLow GetSymLow(bool isLastValue, uint8 prob) const;
#endif
};

#endif
