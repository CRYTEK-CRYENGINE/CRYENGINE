// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "WeaponRecoilOffset.h"
#include "Weapon.h"
#include "Recoil.h"
#include "IronSight.h"
#include "FireMode.h"
#include "WeaponFPAiming.h"
#include "Mannequin/Serialization.h"



SStaticWeaponRecoilParams::SStaticWeaponRecoilParams()
	:	dampStrength(0.0f)
	,	fireRecoilTime(0.0f)
	,	fireRecoilStrengthFirst(0.0f)
	,	fireRecoilStrength(0.0f)
	,	angleRecoilStrength(0.0f)
	,	randomness(0.0f)
{
}



CRecoilOffset::CRecoilOffset()
:	m_position(ZERO)
,	m_fireDirection(ZERO)
,	m_fireTime(0.0f)
,	m_angle(ZERO)
,	m_angleDirection(ZERO)
,	m_firstFire(false)
{
}



QuatT CRecoilOffset::Compute(float frameTime)
{
	Interpolate(m_position, Vec3(ZERO), m_staticParams.dampStrength, frameTime);
	Interpolate(m_angle, Ang3(ZERO), m_staticParams.dampStrength, frameTime);

	if (m_fireTime > 0.0f)
	{
		m_fireTime -= frameTime;
		if (m_fireTime < 0.0f)
			m_fireTime = 0.0f;
		float strength = m_firstFire ? m_staticParams.fireRecoilStrengthFirst : m_staticParams.fireRecoilStrength;
		m_position += m_fireDirection * m_staticParams.fireRecoilStrength * frameTime;
		m_angle += m_angleDirection * m_staticParams.angleRecoilStrength * frameTime;
	}

	QuatT result(IDENTITY);
	result.t = m_position;
	result.q = Quat(m_angle);

	return result;
}



void CRecoilOffset::SetStaticParams(const SStaticWeaponRecoilParams& params)
{
	m_staticParams = params;
}



void CRecoilOffset::TriggerRecoil(bool firstFire)
{
	m_fireTime = m_staticParams.fireRecoilTime;

	m_fireDirection = Vec3(0.0f, -1.0f, 0.0f);
	m_fireDirection.x = cry_random(-1.0f, 1.0f) * m_staticParams.randomness;
	m_fireDirection.z = cry_random(-1.0f, 1.0f) * m_staticParams.randomness;

	Vec3 randAng;
	randAng.x = std::abs(m_fireDirection.x);
	randAng.y = cry_random(-1.0f, 1.0f) * m_staticParams.randomness * 0.5f;
	randAng.z = -m_fireDirection.z * 0.2f;

	m_fireDirection.Normalize();
	randAng.Normalize();

	m_angleDirection = Ang3(randAng);

	m_firstFire = firstFire;
}

void SStaticWeaponRecoilParams::Serialize(Serialization::IArchive& ar)
{
	ar(dampStrength, "DampStrength", "Damp Strength");
	ar(fireRecoilTime, "FireRecoilTime", "Fire Recoil Time");
	ar(fireRecoilStrengthFirst, "FireRecoilStrengthFirst", "Fire Recoil Strength First");
	ar(fireRecoilStrength, "FireRecoilStrength", "Fire Recoil Strength");
	ar(angleRecoilStrength, "AngleRecoilStrength", "Angle Recoil Strength");
	ar(randomness, "Randomness", "Randomness");
}
