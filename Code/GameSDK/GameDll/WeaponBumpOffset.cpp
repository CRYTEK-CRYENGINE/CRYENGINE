// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "WeaponBumpOffset.h"
#include "Utility/Hermite.h"



CBumpOffset::CBumpOffset()
	:	m_direction(IDENTITY)
	,	m_attackTime(0.0f)
	,	m_releaseTime(0.0f)
	,	m_rebounceIntensity(0.0f)
	,	m_time(-1.0f)
{
}



QuatT CBumpOffset::Compute(float frameTime)
{
	QuatT result = QuatT(IDENTITY);

	if (m_time >= 0.0f)
	{
		if (m_time <= m_attackTime + m_releaseTime)
			result = ComputeBump(frameTime);
		else
			m_time = -1.0f;
	}

	return result;
}




void CBumpOffset::AddBump(QuatT direction, float attackTime, float releaseTime, float reboundIntensity)
{
	m_direction = direction;
	m_attackTime = attackTime;
	m_releaseTime = releaseTime;
	m_rebounceIntensity = reboundIntensity;
	m_time = 0.0f;
}



QuatT CBumpOffset::ComputeBump(float frameTime)
{
	float intensity = 0.0f;

	if (m_time <= m_attackTime)
	{
		float t = m_time / m_attackTime;
		intensity = HermiteInterpolate(0.0f, 0.75f, 1.0f, 0.0f, t);
	}
	else
	{
		if (m_rebounceIntensity == 0.0f)
		{
			float t = (m_time-m_attackTime) / m_releaseTime;
			intensity = HermiteInterpolate(1.0f, 0.0f, 0.0f, 0.0f, t);
		}
		else
		{
			if (m_time <= m_attackTime + m_releaseTime * 0.5f)
			{
				float t =
					(m_time-m_attackTime) /
					(m_releaseTime * 0.5f);
				intensity = HermiteInterpolate(1.0f, 0.0f, -m_rebounceIntensity, 0.0f, t);
			}
			else
			{
				float t =
					(m_time-m_attackTime-m_releaseTime*0.5f) /
					(m_releaseTime * 0.5f);
				intensity = HermiteInterpolate(-m_rebounceIntensity, 0.0f, 0.0f, 0.0f, t);
			}
		}
	}

	m_time += frameTime;

	QuatT result = QuatT(IDENTITY);

	if (intensity != 0.0f)
	{
		result.t = m_direction.t * intensity;
		result.q = Quat::CreateSlerp(Quat(IDENTITY), m_direction.q, intensity);
	}

	return result;
}
