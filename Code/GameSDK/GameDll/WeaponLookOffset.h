// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef _WEAPON_LOOK_OFFSET_H_
#define _WEAPON_LOOK_OFFSET_H_

#include "ICryMannequin.h"


struct SParams_WeaponFPAiming;


struct SStaticWeaponSwayParams
{
	SStaticWeaponSwayParams()
		:	easeFactorInc(0.0f)
		,	easeFactorDec(0.0f)
		,	strafeScopeFactor(0.0f)
		,	rotateScopeFactor(0.0f)
		,	velocityInterpolationMultiplier(0.0f)
		,	velocityLowPassFilter(0.0f)
		,	accelerationSmoothing(0.0f)
		,	accelerationFrontAugmentation(0.0f)
		,	verticalVelocityScale(0.0f)
		,	sprintCameraAnimation(0.0f)
		,	look_offset(ZERO)
		,	horiz_look_rot(ZERO)
		,	vert_look_rot(ZERO)
		,	strafe_offset(ZERO)
		,	side_strafe_rot(ZERO)
		,	front_strafe_rot(ZERO) {}

	void Serialize(Serialization::IArchive& ar);

	float	easeFactorInc;
	float	easeFactorDec;
	float	strafeScopeFactor;
	float	rotateScopeFactor;
	float	velocityInterpolationMultiplier;
	float	velocityLowPassFilter;
	float	accelerationSmoothing;
	float	accelerationFrontAugmentation;
	float	verticalVelocityScale;
	float	sprintCameraAnimation;	// should be bool
	Vec2	look_offset;
	Ang3	horiz_look_rot;
	Ang3	vert_look_rot;
	Vec3	strafe_offset;
	Ang3	side_strafe_rot;
	Ang3	front_strafe_rot;
};



struct SGameWeaponSwayParams
{
	SGameWeaponSwayParams()
		:	inputMove(ZERO)
		,	inputRot(ZERO)
		,	velocity(ZERO)
		,	aimDirection(1.0f, 0.0f, 0.0f) {}

	void Serialize(Serialization::IArchive& ar);

	Vec3	inputMove;
	Ang3	inputRot;
	Vec3	aimDirection;
	Vec3	velocity;
};


class CLookOffset
{
public:
	CLookOffset();

	SStaticWeaponSwayParams GetCurrentStaticParams() const {return m_staticParams;}
	void SetStaticParams(const SStaticWeaponSwayParams& params);
	void SetGameParams(const SGameWeaponSwayParams& params);

	QuatT Compute(float frameTime);

private:
	SStaticWeaponSwayParams m_staticParams;
	SGameWeaponSwayParams m_gameParams;

	float m_interpHoriz;
	float m_interpVert;

	bool m_hasStaticParams;
	bool m_hasGameParams;
};



#endif
