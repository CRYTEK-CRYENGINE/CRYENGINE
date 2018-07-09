// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MNM_HASHCOMPUTER_H
#define __MNM_HASHCOMPUTER_H

#pragma once

#include "MNM.h"

#if !defined(_MSC_VER) && !defined(_rotl)
	#define _rotl(x, k) (((x) << (k)) | ((x) >> (32 - (k))))
#endif

namespace MNM
{
//! HashComputer is used by MNM to detect changes in the input geometry.
//! Implements MurmurHash3 32bit.
struct HashComputer
{
	HashComputer(uint32 seed = 0)
		: hash(seed)
		, len(0)
	{
	}

	inline void Add(float key)
	{
		union f32_u
		{
			float  fv;
			uint32 uv;
		};

		f32_u u;
		u.fv = key;

		Add(u.uv);
	}

	inline void Add(uint32 key)
	{
		key *= 0xcc9e2d51;
		key = _rotl(key, 15);
		key *= 0x1b873593;

		hash ^= key;
		hash = _rotl(hash, 13);
		hash = hash * 5 + 0xe6546b64;

		len += 4;
	}

	inline void Complete()
	{
		hash ^= len;

		hash ^= hash >> 16;
		hash *= 0x85ebca6b;
		hash ^= hash >> 13;
		hash *= 0xc2b2ae35;
		hash ^= hash >> 16;
	}

	inline void Add(const Vec3& key)
	{
		Add(key.x);
		Add(key.y);
		Add(key.z);
	}

	inline void Add(const Quat& key)
	{
		Add(key.v);
		Add(key.w);
	}

	inline void Add(const Matrix34& key)
	{
		Add(key.GetTranslation());
		Add(key.GetColumn0());
		Add(key.GetColumn1());
		Add(key.GetColumn2());
	}

	inline uint32 GetValue() const
	{
		return hash;
	}
private:
	uint32 len;
	uint32 hash;
};
}

#endif  // #ifndef __MNM_HASHCOMPUTER_H
