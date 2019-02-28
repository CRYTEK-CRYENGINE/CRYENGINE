// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __Cover_h__
#define __Cover_h__

#pragma once

#include <CryAISystem/ICoverSystem.h>

enum ECoverUsageType
{
	eCU_Hide = 0,
	eCU_UnHide,
};

const float LowCoverMinHeight           = 0.75f;
const float LowCoverMaxHeight           = 1.1f;
const float HighCoverMaxHeight          = 2.0f;
const float CoverOffsetUp               = 0.085f;
const float CoverExtraClearance         = 0.15f;
const float CoverSamplerLoopExtraOffset = 0.15f;

const Vec3 CoverSamplerBiasUp           = Vec3(0.0f, 0.0f, 0.005f);
const Vec3 CoverUp                      = Vec3(0.0f, 0.0f, 1.0f);

const uint32 CoverCollisionEntities     = ent_static | ent_terrain | ent_ignore_noncolliding;

struct CoverInterval
{
	CoverInterval()
		: left(0.0f)
		, right(0.0f)
	{
	}

	CoverInterval(float l, float r)
		: left(l)
		, right(r)
	{
	}

	float       left;
	float       right;

	inline bool Empty() const
	{
		return (right - left) <= 0.0001f;
	}

	inline float Width() const
	{
		return std::max<float>(0.0f, (right - left));
	}

	inline bool Intersect(const CoverInterval& other)
	{
		left = std::max<float>(left, other.left);
		right = std::min<float>(right, other.right);

		return !Empty();
	}

	inline CoverInterval Intersection(const CoverInterval& other) const
	{
		CoverInterval result = *this;
		result.Intersect(other);

		return result;
	}
};

#endif
