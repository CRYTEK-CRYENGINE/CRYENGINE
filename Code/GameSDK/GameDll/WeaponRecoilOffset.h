// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef _WEAPON_RECOIL_OFFSET_H_
#define _WEAPON_RECOIL_OFFSET_H_

struct SParams_WeaponFPAiming;

#include <CrySerialization/Forward.h>

struct SStaticWeaponRecoilParams
{
	SStaticWeaponRecoilParams();

	void Serialize(Serialization::IArchive& ar);

	float	dampStrength;
	float	fireRecoilTime;
	float	fireRecoilStrengthFirst;
	float	fireRecoilStrength;
	float	angleRecoilStrength;
	float	randomness;
};



class CRecoilOffset
{
public:
	CRecoilOffset();

	QuatT Compute(float frameTime);

	void SetStaticParams(const SStaticWeaponRecoilParams& params);
	void TriggerRecoil(bool firstFire);

private:
	SStaticWeaponRecoilParams m_staticParams;

	Vec3 m_position;
	Vec3 m_fireDirection;
	Ang3 m_angle;
	Ang3 m_angleDirection;
	float m_fireTime;
	bool m_firstFire;
};


#endif
