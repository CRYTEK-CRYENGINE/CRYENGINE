// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "WeaponLookOffset.h"
#include "Weapon.h"
#include "Utility/Hermite.h"
#include "WeaponFPAiming.h"
#include "WeaponOffset.h"
#include "Mannequin/Serialization.h"




CLookOffset::CLookOffset()
:	m_interpVert(0.0f)
,	m_interpHoriz(0.0f)
, m_hasStaticParams(false)
, m_hasGameParams(false)
{
}



QuatT CLookOffset::Compute(float frameTime)
{
	if (m_hasStaticParams && m_hasGameParams)
	{
		const float STAP_MF_Up					= 1.0f;
		const float STAP_MF_Down				= 1.0f;
		const float STAP_MF_Left				= 1.0f;
		const float STAP_MF_Right				= 1.0f;
		const float STAP_MF_VertMotion			= 0.4f;
		const float STAP_MF_VelFactorVert		= 0.4f;
		const float STAP_MF_VelFactorHoriz		= 0.3f;

		static const float MIN_VERT_DIR = 0.2f;
		static const float MAX_VERT_DIR = 1.0f;
		static const float MIN_HORIZ_DIR  = 0.2f;
		static const float MAX_HORIZ_DIR  = 3.0f;
		static const float POW_VERT   = 1.0f;
		static const float POW_HORIZ  = 2.0f;
		static const float HORIZ_VEL_SCALE = 0.2f;
		static const float runEaseFactor = 0.2f;

		float rotationFactor = 1.0f;
		float vertFactor = 1.0f;


		Ang3 inputRot = m_gameParams.inputRot;

		float horizInterp = 0.0f;
		float vertInterp  = 0.0f;
		//--- Generate our horizontal & vertical target additive factors
		float absAimDirVert = fabs_tpl(m_gameParams.aimDirection.z) * rotationFactor;
		if (absAimDirVert > MIN_VERT_DIR)
		{
			float factor = (float)__fsel(m_gameParams.aimDirection.z, STAP_MF_Up, -STAP_MF_Down);
			vertInterp = vertFactor * factor * pow_tpl(min((absAimDirVert - MIN_VERT_DIR) / (MAX_VERT_DIR - MIN_VERT_DIR), 1.0f), POW_VERT);
		}
		vertInterp  += rotationFactor * STAP_MF_VelFactorVert * clamp_tpl(m_gameParams.velocity.z * m_staticParams.verticalVelocityScale, -1.0f, 1.0f);
		vertInterp  += (inputRot.x * STAP_MF_VertMotion * rotationFactor);
		vertInterp = clamp_tpl(vertInterp, -1.0f, 1.0f);

		float absInputRotHoriz = fabs_tpl(inputRot.z) * rotationFactor;
		if (absInputRotHoriz > MIN_HORIZ_DIR)
		{
			float factor = (float)__fsel(inputRot.z, STAP_MF_Left, -STAP_MF_Right);
			horizInterp = rotationFactor * factor * pow_tpl(min((absInputRotHoriz - MIN_HORIZ_DIR) / (MAX_HORIZ_DIR - MIN_HORIZ_DIR), 1.0f), POW_HORIZ);
		}
		Vec3 up(0.0f, 0.0f, 1.0f);
		Vec3 idealRight = up.Cross(m_gameParams.aimDirection);
		idealRight.NormalizeSafe();
		float rightVel = idealRight.Dot(m_gameParams.velocity);
		horizInterp  += rotationFactor * STAP_MF_VelFactorHoriz * clamp_tpl(rightVel * HORIZ_VEL_SCALE, -1.0f, 1.0f); 
		horizInterp = clamp_tpl(horizInterp, -1.0f, 1.0f);

		//--- Interpolate from our previous factors
		if ((m_interpVert < -1.0f) && (m_interpHoriz < -1.0f))
		{
			m_interpVert = vertInterp;
			m_interpHoriz = horizInterp;
		}
		else
		{
			const float decStep = (m_staticParams.easeFactorDec * frameTime);
			const float incStep = (m_staticParams.easeFactorInc * frameTime);
			float easeFactor = (float)__fsel(fabs_tpl(m_interpVert) - fabs_tpl(vertInterp), decStep, incStep);
			easeFactor = clamp_tpl(easeFactor, 0.0f, 1.0f);
			vertInterp  = m_interpVert = (vertInterp * easeFactor) + (m_interpVert * (1.0f - easeFactor));

			easeFactor = (float)__fsel(fabs_tpl(m_interpHoriz) - fabs_tpl(horizInterp), decStep, incStep);
			easeFactor = clamp_tpl(easeFactor, 0.0f, 1.0f);
			horizInterp = m_interpHoriz = (horizInterp * easeFactor) + (m_interpHoriz * (1.0f - easeFactor));
		}

		// create offsets
		QuatT lookOffset(IDENTITY);

		Vec2 posSize = m_staticParams.look_offset;
		Ang3 horizRotSize = m_staticParams.horiz_look_rot;
		Ang3 vertRotSize = m_staticParams.vert_look_rot;

		const float cmToMeter = 1.0f / 100.0f;
		const float degreeToRadians = 3.14159f / 180.0f;

		lookOffset.t.x += m_interpHoriz * posSize.x * cmToMeter;
		lookOffset.t.z += -m_interpVert * posSize.y * cmToMeter;
		lookOffset.q *= Quat(horizRotSize * m_interpHoriz * degreeToRadians);
		lookOffset.q *= Quat(vertRotSize * m_interpVert * degreeToRadians);

		return lookOffset;
	}
	else
	{
		return QuatT(IDENTITY);
	}
}



void CLookOffset::SetStaticParams(const SStaticWeaponSwayParams& params)
{
	m_staticParams = params;
	m_hasStaticParams = true;
}



void CLookOffset::SetGameParams(const SGameWeaponSwayParams& params)
{
	m_gameParams = params;
	m_hasGameParams = true;
}

void SStaticWeaponSwayParams::Serialize(Serialization::IArchive& ar)
{
	ar(easeFactorInc, "EaseFactorInc", "Ease Factor Inc");
	ar(easeFactorDec, "EaseFactorDec", "Ease Factor Dec");
	ar(strafeScopeFactor, "StrafeScopeFactor", "Strafe Scope Factor");
	ar(rotateScopeFactor, "RotateScopeFactor", "Rotate Scope Factor");
	ar(velocityInterpolationMultiplier, "VelocityInterpolationMultiplier", "Velocity Interpolation Multiplier");
	ar(velocityLowPassFilter, "VelocityLowPassFilter", "Velocity Low Pass Filter");
	ar(accelerationSmoothing, "AccelerationSmoothing", "Acceleration Smoothing");
	ar(accelerationFrontAugmentation, "AccelerationFrontAugmentation", "Acceleration Front Augmentation");
	ar(verticalVelocityScale, "VerticalVelocityScale", "Vertical Velocity Scale");
	ar(sprintCameraAnimation, "SprintCameraAnimation", "Sprint Camera Animation");
	ar(look_offset, "LookOffset", "Look Offset");
	ar(horiz_look_rot, "HorizLookRot", "Horiz Look Rot");
	ar(vert_look_rot, "VertLookRot", "Vert Look Rot");
	ar(strafe_offset, "StrafeOffset", "Strafe Offset");
	ar(side_strafe_rot, "SideStrafeRot", "Side Strafe Rot");
	ar(front_strafe_rot, "FrontStrafeRot", "Front Strafe Rot");
}


void SGameWeaponSwayParams::Serialize(Serialization::IArchive& ar)
{
	ar(inputMove, "InputMove", "Input Move");
	ar(inputRot, "InputRot", "Input Rot");
	ar(aimDirection, "AimDirection", "Aim Direction");
	ar(velocity, "Velocity", "Velocity");
}