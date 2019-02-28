// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Item.h"
#include "ProceduralWeaponContext.h"
#include "Utility/Wiggle.h"
#include <CrySerialization/IArchive.h>

template<typename WeaponOffsetParams>
struct SWeaponProceduralClipParams : public IProceduralParams
{
	virtual void Serialize(Serialization::IArchive& ar)
	{
		m_offsetParams.Serialize(ar);
	}

	WeaponOffsetParams m_offsetParams;
};

struct SStaticWeaponPoseParams
{
	enum EPoseType
	{
		EPT_RightHand=0,
		EPT_LeftHand=2,
		EPT_Zoom=1,
	};

	SStaticWeaponPoseParams()
		: pose_type(0)
		, zoom_transition_angle(0)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(pose_type, "poseType", "Pose Type");
		ar(zoom_transition_angle, "zoomTransitionAngle", "Zoom Transition Angle");
		m_offset.Serialize(ar);
	}

	float	pose_type;						// should be EPoseType
	float	zoom_transition_angle;
	SWeaponOffset m_offset;

	EPoseType GetPoseType() const {return EPoseType(int(pose_type));}
};

class CWeaponPoseOffset : public TProceduralContextualClip<CProceduralWeaponAnimationContext, SWeaponProceduralClipParams<SStaticWeaponPoseParams> >
{
public:
	CWeaponPoseOffset()
		:	m_offsetId(-1)
		,	m_poseType(SStaticWeaponPoseParams::EPT_RightHand)
	{
	}

	virtual void OnEnter(float blendTime, float duration, const SWeaponProceduralClipParams<SStaticWeaponPoseParams>& staticParams)
	{
		m_context->Initialize(m_scope);

		CWeaponOffsetStack& shoulderOffset = m_context->GetWeaponSway().GetZoomOffset().GetShoulderOffset();
		CWeaponOffsetStack& leftHandOffset = m_context->GetWeaponSway().GetZoomOffset().GetLeftHandOffset();
		SWeaponOffset& zoomOffset = m_context->GetWeaponSway().GetZoomOffset().GetZommOffset();
		m_poseType = staticParams.m_offsetParams.GetPoseType();

		if (m_poseType == SStaticWeaponPoseParams::EPT_RightHand)
		{
			m_offsetId = shoulderOffset.PushOffset(
				ToRadians(staticParams.m_offsetParams.m_offset),
				m_scope->GetBaseLayer(),
				blendTime);
		}
		else if (m_poseType == SStaticWeaponPoseParams::EPT_LeftHand)
		{
			m_offsetId = leftHandOffset.PushOffset(
				ToRadians(staticParams.m_offsetParams.m_offset),
				m_scope->GetBaseLayer(),
				blendTime);
		}
		else if (m_poseType == SStaticWeaponPoseParams::EPT_Zoom)
		{
			zoomOffset = ToRadians(staticParams.m_offsetParams.m_offset);
			m_context->GetWeaponSway().GetZoomOffset().SetZoomTransitionRotation(
				DEG2RAD(staticParams.m_offsetParams.zoom_transition_angle));
		}
	}

	virtual void OnExit(float blendTime)
	{
		CWeaponOffsetStack& shoulderOffset = m_context->GetWeaponSway().GetZoomOffset().GetShoulderOffset();
		CWeaponOffsetStack& leftHandOffset = m_context->GetWeaponSway().GetZoomOffset().GetLeftHandOffset();

		if (m_poseType == SStaticWeaponPoseParams::EPT_RightHand)
			shoulderOffset.PopOffset(m_offsetId, blendTime);
		else if (m_poseType == SStaticWeaponPoseParams::EPT_LeftHand)
			leftHandOffset.PopOffset(m_offsetId, blendTime);

		m_context->Finalize();
	}

	virtual void Update(float timePassed)
	{
		Vec3 aimDirection = Vec3(0.0f, 1.0f, 0.0f);
		if (GetParam(CItem::sActionParamCRCs.aimDirection, aimDirection))
			m_context->SetAimDirection(aimDirection);
		float zoomTransition;
		if (GetParam(CItem::sActionParamCRCs.zoomTransition, zoomTransition))
			m_context->GetWeaponSway().GetZoomOffset().SetZoomTransition(zoomTransition);
	}

private:
	CWeaponOffsetStack::TOffsetId m_offsetId;
	SStaticWeaponPoseParams::EPoseType m_poseType;
};

REGISTER_PROCEDURAL_CLIP(CWeaponPoseOffset, "WeaponPose");

class CWeaponSwayOffset : public TProceduralContextualClip<CProceduralWeaponAnimationContext, SWeaponProceduralClipParams<SStaticWeaponSwayParams> >
{
public:
	virtual void OnEnter(float blendTime, float duration, const SWeaponProceduralClipParams<SStaticWeaponSwayParams>& staticParams)
	{
		m_context->Initialize(m_scope);
		m_lastSwayParams = m_context->GetWeaponSway().GetLookOffset().GetCurrentStaticParams();
		m_context->GetWeaponSway().GetLookOffset().SetStaticParams(staticParams.m_offsetParams);
		m_context->GetWeaponSway().GetStrafeOffset().SetStaticParams(staticParams.m_offsetParams);
	}

	virtual void OnExit(float blendTime)
	{
		m_context->GetWeaponSway().GetLookOffset().SetStaticParams(m_lastSwayParams);
		m_context->GetWeaponSway().GetStrafeOffset().SetStaticParams(m_lastSwayParams);
		m_context->Finalize();
	}

	virtual void Update(float timePassed)
	{
		SGameWeaponSwayParams gameParams;
		GetParam(CItem::sActionParamCRCs.aimDirection, gameParams.aimDirection);
		GetParam(CItem::sActionParamCRCs.inputMove, gameParams.inputMove);
		GetParam(CItem::sActionParamCRCs.inputRot, gameParams.inputRot);
		GetParam(CItem::sActionParamCRCs.velocity, gameParams.velocity);
		m_context->GetWeaponSway().GetLookOffset().SetGameParams(gameParams);
		m_context->GetWeaponSway().GetStrafeOffset().SetGameParams(gameParams);
	}

private:
	SStaticWeaponSwayParams m_lastSwayParams;
};

REGISTER_PROCEDURAL_CLIP(CWeaponSwayOffset, "WeaponSway");

class CWeaponRecoilOffset : public TProceduralContextualClip<CProceduralWeaponAnimationContext, SWeaponProceduralClipParams<SStaticWeaponRecoilParams> >
{
public:
	virtual void OnEnter(float blendTime, float duration, const SWeaponProceduralClipParams<SStaticWeaponRecoilParams>& staticParams)
	{
		m_context->Initialize(m_scope);
		m_context->GetWeaponSway().GetRecoilOffset().SetStaticParams(staticParams.m_offsetParams);

		const bool firstFire = true;
		m_context->GetWeaponSway().GetRecoilOffset().TriggerRecoil(firstFire);
	}

	virtual void OnExit(float blendTime)
	{
		m_context->Finalize();
	}

	virtual void Update(float timePassed)
	{
		bool fired = false;
		bool firstFire = false;
		GetParam(CItem::sActionParamCRCs.fired, fired);
		GetParam(CItem::sActionParamCRCs.firstFire, firstFire);
		if (fired)
			m_context->GetWeaponSway().GetRecoilOffset().TriggerRecoil(firstFire);
	}
};

REGISTER_PROCEDURAL_CLIP(CWeaponRecoilOffset, "WeaponRecoil");

struct SStaticBumpParams
{
	SStaticBumpParams()
		: time(0.0f)
		, shift(0.0f)
		, rotation(0.0f)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(time, "Time", "Time");
		ar(shift, "Shift", "Shift");
		ar(rotation, "Rotation", "Rotation");
	}

	float	time;
	float	shift;
	float	rotation;
};

class CWeaponBumpOffset : public TProceduralContextualClip<CProceduralWeaponAnimationContext, SWeaponProceduralClipParams<SStaticBumpParams> >
{
public:
	virtual void OnEnter(float blendTime, float duration, const SWeaponProceduralClipParams<SStaticBumpParams>& staticParams)
	{
		m_context->Initialize(m_scope);

		float fallFactor = 0.0;
		GetParam(CItem::sActionParamCRCs.fallFactor, fallFactor);

		QuatT bump(IDENTITY);
		bump.t.z = -staticParams.m_offsetParams.shift * fallFactor;
		bump.q = Quat::CreateRotationX(DEG2RAD(-staticParams.m_offsetParams.rotation));
		float attack = staticParams.m_offsetParams.time * 0.35f;
		float release = staticParams.m_offsetParams.time * 0.65f;
		float rebounce = 0.25f;

		m_context->GetWeaponSway().GetBumpOffset().AddBump(bump, attack, release, rebounce);
	}

	virtual void OnExit(float blendTime)
	{
		m_context->Finalize();
	}

	virtual void Update(float timePassed)
	{
	}
};

REGISTER_PROCEDURAL_CLIP(CWeaponBumpOffset, "WeaponBump");

struct SStaticWiggleParams
{
	SStaticWiggleParams()
		: frequency(0)
		, intensity(0)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(frequency, "Frequency", "Frequency");
		ar(intensity, "Intensity", "Intensity");
	}

	float frequency;
	float intensity;
};


class CWeaponWiggleOffset : public TProceduralContextualClip<CProceduralWeaponAnimationContext, SWeaponProceduralClipParams<SStaticWiggleParams> >
{
public:
	CWeaponWiggleOffset()
		:	m_blendTime(0.0f)
		,	m_timePassed(0.0f)
		,	m_blendingIn(false)
	{
	}

	virtual void OnEnter(float blendTime, float duration, const SWeaponProceduralClipParams<SStaticWiggleParams>& staticParams)
	{
		m_context->Initialize(m_scope);
		m_blendingIn = true;
		m_timePassed = 0.0f;
		m_blendTime = blendTime;
	}

	virtual void OnExit(float blendTime)
	{
		m_context->Finalize();
		m_blendingIn = false;
		m_timePassed = 0.0f;
		m_blendTime = blendTime;
	}

	virtual void Update(float timePassed)
	{
		m_timePassed += timePassed;

		QuatT shakeOffset(IDENTITY);

		m_wiggler.SetParams(GetParams().m_offsetParams.frequency);
		const float intensity = BlendIntensity() * GetParams().m_offsetParams.intensity;

		Vec3 wiggle = m_wiggler.Update(timePassed) * 2.0f - Vec3(1.0f, 1.0f, 1.0f);
		wiggle.Normalize();
		shakeOffset.t.x = wiggle.x * intensity;
		shakeOffset.t.z = wiggle.y * intensity;
		shakeOffset.q = Quat::CreateRotationY(wiggle.z * intensity);

		m_context->GetWeaponSway().AddCustomOffset(shakeOffset);
	}

private:
	float BlendIntensity() const
	{
		float result = 1.0f;

		if (m_blendTime >= 0.0f)
			result = m_timePassed / m_blendTime;

		if (!m_blendingIn)
			result = 1.0f - result;

		return SATURATE(result);
	}

	CWiggleVec3 m_wiggler;
	float m_blendTime;
	float m_timePassed;
	bool m_blendingIn;
};

REGISTER_PROCEDURAL_CLIP(CWeaponWiggleOffset, "WeaponWiggle");
