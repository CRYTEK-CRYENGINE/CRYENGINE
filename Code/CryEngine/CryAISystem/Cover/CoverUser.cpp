// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CoverUser.h"
#include "CoverSurface.h"
#include "CoverSystem.h"

#include "DebugDrawContext.h"

CoverUser::CoverUser()
	: m_coverID(0)
	, m_nextCoverID(0)
	, m_locationEffectiveHeight(FLT_MAX)
	, m_distanceFromCoverLocationSqr(FLT_MAX)
	, m_compromised(false)
{
}

CoverUser::CoverUser(const Params& params)
	: CoverUser()
{
	SetParams(params);
}

CoverUser::~CoverUser()
{
	Reset();
}

void CoverUser::Reset()
{
	if (m_coverID)
	{ 
		gAIEnv.pCoverSystem->SetCoverOccupied(m_coverID, false, *this);
		m_coverID = CoverID();
	}
	if (m_nextCoverID)
	{
		gAIEnv.pCoverSystem->SetCoverOccupied(m_nextCoverID, false, *this);
		m_nextCoverID = CoverID();
	}

	m_coverBlacklistMap.clear();
	m_eyes.clear();
	m_state.Clear();

	ResetState();
}

void CoverUser::ResetState()
{	
	// Reseting state related to cover location
	m_compromised = false;
	m_locationEffectiveHeight = FLT_MAX;
	m_distanceFromCoverLocationSqr = FLT_MAX;
}

void CoverUser::SetCoverID(const CoverID& coverID)
{
	CRY_ASSERT(coverID || m_state.IsEmpty());

	if (m_nextCoverID == coverID)
	{
		m_nextCoverID = CoverID();
	}
	
	if (m_coverID != coverID)
	{
		if (m_coverID)
			gAIEnv.pCoverSystem->SetCoverOccupied(m_coverID, false, *this);

		ResetState();

		m_coverID = coverID;

		if (m_coverID)
			gAIEnv.pCoverSystem->SetCoverOccupied(m_coverID, true, *this);
	}
}

const CoverID& CoverUser::GetCoverID() const
{
	return m_coverID;
}

void CoverUser::SetNextCoverID(const CoverID& coverID)
{
	if (m_nextCoverID != coverID)
	{
		if (m_nextCoverID)
			gAIEnv.pCoverSystem->SetCoverOccupied(m_nextCoverID, false, *this);

		m_nextCoverID = coverID;

		if (m_nextCoverID)
			gAIEnv.pCoverSystem->SetCoverOccupied(m_nextCoverID, true, *this);
	}
}

const CoverID& CoverUser::GetNextCoverID() const
{
	return m_nextCoverID;
}

void CoverUser::SetParams(const Params& params)
{
	m_params = params;
}

const CoverUser::Params& CoverUser::GetParams() const
{
	return m_params;
}

void CoverUser::SetCoverBlacklisted(const CoverID& coverID, bool blacklist, float time)
{
	if (blacklist)
	{
		std::pair<CoverBlacklistMap::iterator, bool> iresult = m_coverBlacklistMap.insert(CoverBlacklistMap::value_type(coverID, time));
		if (!iresult.second)
			iresult.first->second = time;
	}
	else
	{
		m_coverBlacklistMap.erase(coverID);
	}
}
bool CoverUser::IsCoverBlackListed(const CoverID& coverId) const
{
	return m_coverBlacklistMap.find(coverId) != m_coverBlacklistMap.end();
}

void CoverUser::UpdateCoverEyes()
{
	if (m_params.fillCoverEyesCustomMethod)
	{
		m_eyes.clear();
		m_params.fillCoverEyesCustomMethod(m_eyes);
	}
}

void CoverUser::Update(float timeDelta)
{
	CRY_PROFILE_REGION(PROFILE_AI, "CoverUser::Update");

	UpdateBlacklisted(timeDelta);

	if (!m_state.IsEmpty())
	{
		CRY_ASSERT(m_coverID);
		if (m_coverID)
		{
			UpdateCoverEyes();

			const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_params.userID);
			CRY_ASSERT(pEntity);

			Vec3 coverNormal;
			const Vec3 coverPos = gAIEnv.pCoverSystem->GetCoverLocation(m_coverID, m_params.distanceToCover, 0, &coverNormal);
			const Vec3 currentPos = pEntity->GetWorldPos();

			m_distanceFromCoverLocationSqr = (currentPos - coverPos).len2();

			EntityId compromisedByEntityId = INVALID_ENTITYID;
			bool wasCompromised = m_compromised;
			m_compromised = UpdateCompromised(coverPos, coverNormal, m_params.minEffectiveCoverHeight);
			if (!wasCompromised && m_compromised)
			{
				if (m_params.activeCoverCompromisedCallback)
				{
					m_params.activeCoverCompromisedCallback(m_coverID, this);
				}
			}
		}
	}
}

void CoverUser::UpdateBlacklisted(float timeDelta)
{
	for (auto it = m_coverBlacklistMap.begin(); it != m_coverBlacklistMap.end();)
	{
		float& time = it->second;
		time -= timeDelta;

		if (time <= 0.0f)
		{
			it = m_coverBlacklistMap.erase(it);
		}
		else
		{
			++it;
		}
	}
}

bool CoverUser::UpdateCompromised(const Vec3& coverPos, const Vec3& coverNormal, float minEffectiveCoverHeight /*= 0.001f*/)
{
	if (!m_coverID)
		return false;

	if (m_eyes.size() == 0)
		return false;

	if (m_state.Check(ICoverUser::EStateFlags::InCover))
	{
		const float maxAllowedDistanceFromCoverLocationSq = sqr(1.0f);
		if (m_distanceFromCoverLocationSqr > maxAllowedDistanceFromCoverLocationSq)
			return true;

		//TODO: why it is computing dot only with first eye? This is similar what is happening in TPS generator
		if (coverNormal.dot(m_eyes[0] - coverPos) >= 0.0001f)
			return true;
		
		if (!IsInCover(coverPos, m_params.inCoverRadius, m_eyes.data(), m_eyes.size()))
			return true;
	}

	m_locationEffectiveHeight = CalculateEffectiveHeightAt(coverPos, m_coverID);
	if (m_locationEffectiveHeight < minEffectiveCoverHeight)
		return true;

	return false;
}

bool CoverUser::IsCompromised() const
{
	return m_compromised;
}

float CoverUser::GetLocationEffectiveHeight() const
{
	return m_locationEffectiveHeight;
}

void CoverUser::DebugDraw() const
{
	const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_params.userID);
	CRY_ASSERT(pEntity);

	const Vec3 position = pEntity->GetWorldPos();
	
	CDebugDrawContext dc;

	if (m_locationEffectiveHeight > 0.0f && m_locationEffectiveHeight < FLT_MAX)
		dc->DrawLine(position, Col_LimeGreen, position + CoverUp * m_locationEffectiveHeight, Col_LimeGreen, 25.0f);
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

float CoverUser::CalculateEffectiveHeightAt(const Vec3& pos, const CoverID& coverId) const
{
	float lowestHeightSq = FLT_MAX;
	const CoverSurface& surface = gAIEnv.pCoverSystem->GetCoverSurface(coverId);

	for (const Vec3& eyePos : m_eyes)
	{
		float heightSq;
		if (!surface.GetCoverOcclusionAt(eyePos, pos, &heightSq))
			return -1.0f;

		if (heightSq <= lowestHeightSq)
			lowestHeightSq = heightSq;
	}

	return sqrt_tpl(lowestHeightSq);
}

Vec3 CoverUser::GetCoverNormal(const Vec3& position) const
{
	if (CoverSurfaceID surfaceID = gAIEnv.pCoverSystem->GetSurfaceID(m_coverID))
	{
		const CoverPath& path = gAIEnv.pCoverSystem->GetCoverPath(surfaceID, m_params.distanceToCover);
		return -path.GetNormalAt(position);
	}
	return ZERO;
}

Vec3 CoverUser::GetCoverLocation(const CoverID& coverID) const
{
	return gAIEnv.pCoverSystem->GetCoverLocation(coverID, m_params.distanceToCover);
}

