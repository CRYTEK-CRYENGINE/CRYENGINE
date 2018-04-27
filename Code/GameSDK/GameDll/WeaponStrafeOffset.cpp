// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "WeaponStrafeOffset.h"
#include "Weapon.h"
#include "Utility/Hermite.h"
#include "WeaponFPAiming.h"
#include "WeaponOffset.h"


namespace
{


	float SignedPow(float linearVal, float curvePower)
	{
		return pow_tpl(fabs_tpl(linearVal), curvePower) * fsgnnz(linearVal);
	}


}



CStrafeOffset::CStrafeOffset()
:	m_smoothedVelocity(ZERO)
,	m_lastVelocity(ZERO)
,	m_interpFront(0.0f)
,	m_interpSide(0.0f)
,	m_runFactor(0.0f)
,	m_sprintFactor(0.0f)
,	m_noiseFactor(0.0f)
{
}



QuatT CStrafeOffset::Compute(float frameTime)
{
	const float STAP_MF_Front				= 1.0f;
	const float STAP_MF_Back				= 1.0f;
	const float STAP_MF_StrafeLeft			= 1.0f;
	const float STAP_MF_StrafeRight			= 1.0f;

	static const float MIN_VERT_DIR = 0.2f;
	static const float MAX_VERT_DIR = 1.0f;
	static const float MIN_HORIZ_DIR  = 0.2f;
	static const float MAX_HORIZ_DIR  = 3.0f;
	static const float runEaseFactor = 0.2f;

	const float MIN_SPEED = 0.2f;
	const float MAX_SPEED = 4.0f;

	float rotationFactor = 1.0f;
	float strafeFactor = 1.0f;

	Ang3 inputRot = m_gameParams.inputRot;
	float absInputRotHoriz = fabs_tpl(inputRot.z) * rotationFactor;

	float horizSpeed = m_gameParams.velocity.GetLength2D();
	float runFactor	= (horizSpeed - MIN_SPEED) / (MAX_SPEED - MIN_SPEED);
	const float horizRotFactor = clamp_tpl(-m_gameParams.aimDirection.z * (absInputRotHoriz - MIN_HORIZ_DIR) / (MAX_HORIZ_DIR - MIN_HORIZ_DIR), 0.0f, 1.0f);
	runFactor = max(runFactor, horizRotFactor);
	runFactor = clamp_tpl(runFactor, 0.0f, 1.0f);
	m_runFactor = ((runFactor * runEaseFactor) + (m_runFactor * (1.0f - runEaseFactor)));

	//--- Calculate movement acceleration
	const float invFrontAugmentation = m_staticParams.accelerationFrontAugmentation != 0.0 ? (1.0f / m_staticParams.accelerationFrontAugmentation) : 1.0f;
	const Vec3 inputMoveAugmented = Vec3(
		m_gameParams.inputMove.x * invFrontAugmentation,
		m_gameParams.inputMove.y * m_staticParams.accelerationFrontAugmentation,
		0.0f);
	Interpolate(m_smoothedVelocity, inputMoveAugmented, m_staticParams.velocityLowPassFilter, frameTime);

	const Vec3 velocityDerivative = (frameTime > 0.0) ? ((m_smoothedVelocity - m_lastVelocity) / frameTime) : Vec3(ZERO);
	m_lastVelocity = m_smoothedVelocity;

	float interpFront = SignedPow(clamp_tpl(velocityDerivative.y * m_staticParams.velocityInterpolationMultiplier, -1.0f, 1.0f), m_staticParams.accelerationSmoothing);
	interpFront *= (float)__fsel(interpFront, STAP_MF_Front, STAP_MF_Back) * strafeFactor;
	Interpolate(m_interpFront, interpFront, m_staticParams.velocityLowPassFilter, frameTime);
	float interpSide = SignedPow(clamp_tpl(velocityDerivative.x * m_staticParams.velocityInterpolationMultiplier, -1.0f, 1.0f), m_staticParams.accelerationSmoothing);
	interpSide *= (float)__fsel(interpSide, STAP_MF_StrafeLeft, STAP_MF_StrafeRight) * strafeFactor;
	Interpolate(m_interpSide, interpSide, m_staticParams.velocityLowPassFilter, frameTime);

	// create offsets
	QuatT strafeOffset(IDENTITY);

	Vec3 posSize = m_staticParams.strafe_offset;
	Ang3 sideRotSize = m_staticParams.side_strafe_rot;
	Ang3 frontRotSize = m_staticParams.front_strafe_rot;

	const float cmToMeter = 1.0f / 100.0f;
	const float degreeToRadians = 3.14159f / 180.0f;

	strafeOffset.t.x -= m_interpSide * posSize.x * cmToMeter;
	strafeOffset.t.y -= m_interpFront * posSize.y * cmToMeter;
	strafeOffset.t.z -= max(std::abs(m_interpFront), std::abs(m_interpSide)) * posSize.z * cmToMeter;
	strafeOffset.q *= Quat(sideRotSize * m_interpSide * degreeToRadians);
	strafeOffset.q *= Quat(frontRotSize * m_interpFront * degreeToRadians);

	return strafeOffset;
}



void CStrafeOffset::SetStaticParams(const SStaticWeaponSwayParams& params)
{
	m_staticParams = params;
}



void CStrafeOffset::SetGameParams(const SGameWeaponSwayParams& params)
{
	m_gameParams = params;
}
