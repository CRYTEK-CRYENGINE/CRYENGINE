// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "CoverUser.h"
#include "CoverSurface.h"
#include "CoverSystem.h"

#include "DebugDrawContext.h"

CoverUser::CoverUser()
	: m_coverID(0)
	, m_nextCoverID(0)
	, m_locationEffectiveHeight(FLT_MAX)
	, m_location(ZERO)
	, m_normal(ZERO)
	, m_compromised(false)
	, m_farFromCoverLocation(false)
{
}

void CoverUser::Reset()
{
	if (m_coverID)
		gAIEnv.pCoverSystem->SetCoverOccupied(m_coverID, false, m_params.userID);

	m_coverID = CoverID();

	ResetState();
}

void CoverUser::ResetState()
{
	m_compromised = false;
	m_farFromCoverLocation = false;
	m_locationEffectiveHeight = FLT_MAX;
	m_location.zero();
	m_normal.zero();
}

void CoverUser::SetCoverID(const CoverID& coverID)
{
	if (m_coverID != coverID)
	{
		if (m_coverID)
			gAIEnv.pCoverSystem->SetCoverOccupied(m_coverID, false, m_params.userID);

		ResetState();

		m_coverID = coverID;

		if (m_coverID)
			gAIEnv.pCoverSystem->SetCoverOccupied(m_coverID, true, m_params.userID);
	}
}

const CoverID& CoverUser::GetCoverID() const
{
	return m_coverID;
}

void CoverUser::SetParams(const Params& params)
{
	m_params = params;
}

const CoverUser::Params& CoverUser::GetParams() const
{
	return m_params;
}

void CoverUser::Update(float updateTime, const Vec3& pos, const Vec3* eyes, uint32 eyeCount, float minEffectiveCoverHeight /*= 0.001f*/)
{
	if (m_coverID)
	{
		FRAME_PROFILER("CoverUser::Update 2", gEnv->pSystem, PROFILE_AI);

		Vec3 coverNormal;
		Vec3 coverLocation = gAIEnv.pCoverSystem->GetCoverLocation(m_coverID, m_params.distanceToCover, 0, &coverNormal);

		const float maxAllowedDistanceFromCoverLocationSq = sqr(1.0f);
		m_farFromCoverLocation = (pos - coverLocation).len2() > maxAllowedDistanceFromCoverLocationSq;

		if (!m_compromised)
			m_compromised = !IsInCover(coverLocation, m_params.inCoverRadius, eyes, eyeCount);

		if (!m_compromised)
		{
			if (coverNormal.dot(eyes[0] - coverLocation) >= 0.0001f)
				m_compromised = true;
		}

		if (!m_compromised)
		{
			m_locationEffectiveHeight = CalculateEffectiveHeightAt(coverLocation, eyes, eyeCount);

			if (m_locationEffectiveHeight < minEffectiveCoverHeight)
				m_compromised = true;

			m_location = pos;
		}
	}
}

void CoverUser::UpdateWhileMoving(float updateTime, const Vec3& pos, const Vec3* eyes, uint32 eyeCount)
{
	if (m_coverID)
	{
		FRAME_PROFILER("CoverUser::Update 2", gEnv->pSystem, PROFILE_AI);

		if (!m_compromised)
		{
			Vec3 coverLocation = gAIEnv.pCoverSystem->GetCoverLocation(m_coverID, m_params.distanceToCover);
			m_locationEffectiveHeight = CalculateEffectiveHeightAt(coverLocation, eyes, eyeCount);

			if (m_locationEffectiveHeight <= 0.001f)
				m_compromised = true;

			m_location = pos;
		}
	}
}

void CoverUser::UpdateNormal(const Vec3& pos)
{
	m_normal.zero();

	if (CoverSurfaceID surfaceID = gAIEnv.pCoverSystem->GetSurfaceID(m_coverID))
	{
		const CoverPath& path = gAIEnv.pCoverSystem->GetCoverPath(surfaceID, m_params.distanceToCover);
		m_normal = -path.GetNormalAt(pos);
	}
}

bool CoverUser::IsCompromised() const
{
	return m_compromised;
}

bool CoverUser::IsFarFromCoverLocation() const
{
	return m_farFromCoverLocation;
}

float CoverUser::GetLocationEffectiveHeight() const
{
	return m_locationEffectiveHeight;
}

void CoverUser::DebugDraw() const
{
	CDebugDrawContext dc;

	if (m_locationEffectiveHeight > 0.0f && m_locationEffectiveHeight < FLT_MAX)
		dc->DrawLine(m_location, Col_LimeGreen, m_location + CoverUp * m_locationEffectiveHeight, Col_LimeGreen, 25.0f);
}

bool CoverUser::IsInCover(const Vec3& pos, float radius, const Vec3* eyes, uint32 eyeCount) const
{
	const CoverSurface& surface = gAIEnv.pCoverSystem->GetCoverSurface(m_coverID);

	if (radius < 0.0001f)
	{
		for (uint32 i = 0; i < eyeCount; ++i)
		{
			if (!surface.IsPointInCover(eyes[i], pos))
				return false;
		}
	}
	else
	{
		for (uint32 i = 0; i < eyeCount; ++i)
		{
			if (!surface.IsCircleInCover(eyes[i], pos, radius))
				return false;
		}
	}

	return true;
}

float CoverUser::CalculateEffectiveHeightAt(const Vec3& pos, const Vec3* eyes, uint32 eyeCount) const
{
	float lowestHeightSq = FLT_MAX;

	const CoverSurface& surface = gAIEnv.pCoverSystem->GetCoverSurface(m_coverID);

	for (uint32 i = 0; i < eyeCount; ++i)
	{
		float heightSq;
		if (!surface.GetCoverOcclusionAt(eyes[i], pos, &heightSq))
			return -1.0f;

		if (heightSq <= lowestHeightSq)
			lowestHeightSq = heightSq;
	}

	return sqrt_tpl(lowestHeightSq);
}

const Vec3& CoverUser::GetCoverNormal() const
{
	return m_normal;
}

Vec3 CoverUser::GetCoverLocation() const
{
	return gAIEnv.pCoverSystem->GetCoverLocation(m_coverID, m_params.distanceToCover);
}
