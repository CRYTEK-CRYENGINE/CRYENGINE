// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CryGUID.h
//  Version:     v1.00
//  Created:     02/25/2009 by CarstenW
//  Description: Part of CryEngine's extension framework.
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef _CRYGUID_H_
#define _CRYGUID_H_

#pragma once

#include <CrySerialization/IArchive.h>
#include <CryMath/Random.h>

#include <functional>

struct CryGUID
{
	uint64 hipart;
	uint64 lopart;

	// !!! Do NOT turn CryGUID into a non-aggregate !!!
	// It will prevent inlining and type list unrolling opportunities within
	// cryinterface_cast<T>() and cryiidof<T>(). As such prevent constructors,
	// non-public members, base classes and virtual functions!

	//CryGUID() : hipart(0), lopart(0) {}
	//CryGUID(uint64 h, uint64 l) : hipart(h), lopart(l) {}

	static CryGUID Construct(const uint64& hipart, const uint64& lopart)
	{
		CryGUID guid = { hipart, lopart };
		return guid;
	}

	static CryGUID Create()
	{
		CryGUID guid;
		gEnv->pSystem->FillRandomMT(reinterpret_cast<uint32*>(&guid), sizeof(guid) / sizeof(uint32));
		MEMORY_RW_REORDERING_BARRIER;
		return guid;
	}

	static CryGUID Null()
	{
		return Construct(0, 0);
	}

	bool operator==(const CryGUID& rhs) const { return hipart == rhs.hipart && lopart == rhs.lopart; }
	bool operator!=(const CryGUID& rhs) const { return hipart != rhs.hipart || lopart != rhs.lopart; }
	bool operator<(const CryGUID& rhs) const  { return hipart == rhs.hipart ? lopart < rhs.lopart : hipart < rhs.hipart; }

	void Serialize(Serialization::IArchive& ar)
	{
		if (ar.isInput())
		{
			uint32 dwords[4];
			ar(dwords, "guid");
			lopart = (((uint64)dwords[1]) << 32) | (uint64)dwords[0];
			hipart = (((uint64)dwords[3]) << 32) | (uint64)dwords[2];
		}
		else
		{
			uint32 guid[4] = {
				(uint32)(lopart & 0xFFFFFFFF), (uint32)((lopart >> 32) & 0xFFFFFFFF),
				(uint32)(hipart & 0xFFFFFFFF), (uint32)((hipart >> 32) & 0xFFFFFFFF)
			};
			ar(guid, "guid");
		}
	}
};

// This is only used by the editor where we use C++ 11.
#if __cplusplus > 199711L
namespace std
{
template<> struct hash<CryGUID>
{
public:
	size_t operator()(const CryGUID& guid) const
	{
		std::hash<uint64> hasher;
		return hasher(guid.lopart) ^ hasher(guid.hipart);
	}
};
}
#endif

#define MAKE_CRYGUID(high, low) CryGUID::Construct((uint64) high ## LL, (uint64) low ## LL)

#endif // #ifndef _CRYGUID_H_
