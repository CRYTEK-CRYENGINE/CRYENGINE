// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef _WEAPON_STRAFE_OFFSET_H_
#define _WEAPON_STRAFE_OFFSET_H_

#include "WeaponLookOffset.h"



class CStrafeOffset
{
public:
	CStrafeOffset();

	void SetStaticParams(const SStaticWeaponSwayParams& params);
	void SetGameParams(const SGameWeaponSwayParams& params);

	QuatT Compute(float frameTime);

private:
	SStaticWeaponSwayParams m_staticParams;
	SGameWeaponSwayParams m_gameParams;

	Vec3  m_smoothedVelocity;
	Vec3  m_lastVelocity;
	float m_interpFront;
	float m_interpSide;
	float m_runFactor;
	float m_sprintFactor;
	float m_noiseFactor;
};



#endif
