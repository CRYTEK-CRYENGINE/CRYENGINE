// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __UTILS_H__
#define __UTILS_H__

#pragma once

#include "Config.h"

bool   StringToKey(const char* s, uint32& key);
string KeyToString(uint32 key);
void   KeyToString(uint32 key, char* buffer);

template<class T>
bool equiv(const T& a, const T& b)
{
	std::less<T> f;
	if (f(a, b))
		return false;
	else if (f(b, a))
		return false;
	else
		return true;
}

class CCRC8
{
public:
	CCRC8() : m_crc(~0) {}

	ILINE void Add(uint8 x)
	{
		m_crc = m_table[m_crc ^ x];
	}

	ILINE void Add32(uint32 x)
	{
		Add(x >> 24);
		Add(x >> 16);
		Add(x >> 8);
		Add(x);
	}

	ILINE void Add64(uint64 x)
	{
		Add32((uint32)(x >> 32));
		Add32((uint32)x);
	}

	uint8 Result() const
	{
		return m_crc;
	}

private:
	static const uint8 m_table[256];
	uint8              m_crc;
};

#if ENABLE_DEBUG_KIT
#include <CryMath/MTPseudoRandom.h>

class CAutoCorruptAndRestore
{
public:
	CAutoCorruptAndRestore(const uint8* pBuffer, size_t nLength, bool corrupt) : m_buf(NULL), m_bit(-1)
	{
		if (corrupt)
		{
			m_buf = const_cast<uint8*>(pBuffer);
			m_bit = CMTRand_int32().GenerateUint32() % (nLength * 8);
			m_buf[m_bit / 8] ^= (1 << m_bit % 8);
		}
	}

	~CAutoCorruptAndRestore()
	{
		if (m_buf)
			m_buf[m_bit / 8] ^= (1 << m_bit % 8);
	}

private:
	uint8* m_buf;
	size_t m_bit;
};
#endif

#endif
