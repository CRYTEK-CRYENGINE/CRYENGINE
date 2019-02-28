// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef _WEAPON_BUMP_OFFSET_H_
#define _WEAPON_BUMP_OFFSET_H_

struct SParams_WeaponFPAiming;

class CBumpOffset
{
public:
	CBumpOffset();

	QuatT Compute(float frameTime);

	void AddBump(QuatT direction, float attackTime, float releaseTime, float reboundIntensity);

private:
	QuatT ComputeBump(float frameTime);

	QuatT m_direction;
	float m_attackTime;
	float m_releaseTime;
	float m_rebounceIntensity;
	float m_time;
};


#endif
