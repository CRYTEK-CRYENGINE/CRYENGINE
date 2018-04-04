// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef __CoverUser_h__
#define __CoverUser_h__

#pragma once

#include "Cover.h"
#include "CoverPath.h"

class CoverUser
{
public:
	CoverUser();

	struct Params
	{
		Params()
			: distanceToCover(0.4f)
			, inCoverRadius(0.3f)
			, userID(0)
		{
		}

		float       distanceToCover;
		float       inCoverRadius;
		tAIObjectID userID;
	};

	void           Reset();
	void           ResetState();

	void           SetCoverID(const CoverID& coverID);
	const CoverID& GetCoverID() const;

	void           SetParams(const Params& params);
	const Params& GetParams() const;

	void          Update(float updateTime, const Vec3& pos, const Vec3* eyes, uint32 eyeCount, float minEffectiveCoverHeight = 0.001f);
	void          UpdateWhileMoving(float updateTime, const Vec3& pos, const Vec3* eyes, uint32 eyeCount);
	void          UpdateNormal(const Vec3& pos);

	bool          IsCompromised() const;
	bool          IsFarFromCoverLocation() const;
	float         CalculateEffectiveHeightAt(const Vec3& pos, const Vec3* eyes, uint32 eyeCount) const;
	float         GetLocationEffectiveHeight() const;
	const Vec3& GetCoverNormal() const; // normal pointing out of the cover surface
	Vec3        GetCoverLocation() const;

	void        DebugDraw() const;

private:
	bool IsInCover(const Vec3& pos, float radius, const Vec3* eyes, uint32 eyeCount) const;

	CoverID m_coverID;
	CoverID m_nextCoverID;

	float   m_locationEffectiveHeight;
	Vec3    m_location;
	Vec3    m_normal;

	bool    m_compromised;
	bool    m_farFromCoverLocation;

	Params  m_params;
};

#endif
