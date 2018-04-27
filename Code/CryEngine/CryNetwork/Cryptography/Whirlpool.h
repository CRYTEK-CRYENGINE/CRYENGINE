// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __WHIRLPOOL_H__
#define __WHIRLPOOL_H__

#pragma once

#include <CryNetwork/ISerialize.h>

class CWhirlpoolHash
{
public:
	static const int DIGESTBYTES = 64;

	CWhirlpoolHash();
	CWhirlpoolHash(const uint8* input, size_t length);
	CWhirlpoolHash(const string& str);
	string      GetHumanReadable() const;
	void        SerializeWith(TSerialize ser);

	static bool Test();

	ILINE bool  operator==(const CWhirlpoolHash& rhs) const
	{
		return 0 == memcmp(m_hash, rhs.m_hash, DIGESTBYTES);
	}
	ILINE bool operator!=(const CWhirlpoolHash& rhs) const
	{
		return !this->operator==(rhs);
	}

	const uint8* operator()() const { return m_hash; }

private:
	uint8 m_hash[DIGESTBYTES];
};

#endif
