// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "ICryMannequin.h"

#include <Mannequin/Serialization.h>

#include <CryMath/Random.h>

struct SAimAroundParams : public IProceduralParams
{
	SAimAroundParams()
		: smoothTime(1.f)
		, scopeLayer(0)
		, yawMin(-1.f)
		, yawMax(1.f)
		, pitchMin(0.f)
		, pitchMax(0.5f)
		, timeMin(1.0f)
		, timeMax(2.0f)
	{
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		ar(Serialization::Decorators::AnimationName<SAnimRef>(animRef), "Animation");
		ar(smoothTime, "SmoothTime", "Smooth Time");
		ar(Serialization::Decorators::Range<uint32>(scopeLayer, 0, 15), "ScopeLayer", "Scope Layer");
		ar(yawMin, "YawMin", "Yaw Min");
		ar(yawMax, "YawMax", "Yaw Max");
		ar(pitchMin, "PitchMin", "Pitch Max");
		ar(pitchMax, "PitchMax", "Pitch Max");
		ar(timeMin, "TimeMin", "Time Min");
		ar(timeMax, "TimeMax", "Time Max");
	}

	virtual void GetExtraDebugInfo(StringWrapper& extraInfoOut) const override
	{
		extraInfoOut = animRef.c_str();
	}

	SAnimRef animRef;
	float    smoothTime;
	uint32   scopeLayer;
	float    yawMin;
	float    yawMax;
	float    pitchMin;
	float    pitchMax;
	float    timeMin;
	float    timeMax;
};

class CProceduralClipAimAround : public TProceduralClip<SAimAroundParams>
{
public:
	CProceduralClipAimAround();

	virtual void OnEnter(float blendTime, float duration, const SAimAroundParams& params)
	{
		UpdateLookTarget();
		Vec3 lookPos = m_entity->GetWorldPos();
		lookPos += m_entity->GetRotation() * (m_lookOffset * 10.0f);

		const float smoothTime = params.smoothTime;
		const uint32 ikLayer = m_scope->GetBaseLayer() + params.scopeLayer;
		if (!params.animRef.IsEmpty())
		{
			CryCharAnimationParams animParams;
			animParams.m_fTransTime = blendTime;
			animParams.m_nLayerID = ikLayer;
			animParams.m_nFlags = CA_LOOP_ANIMATION | CA_ALLOW_ANIM_RESTART;
			int animID = m_charInstance->GetIAnimationSet()->GetAnimIDByCRC(params.animRef.crc);
			m_charInstance->GetISkeletonAnim()->StartAnimationById(animID, animParams);
		}

		IAnimationPoseBlenderDir* poseBlenderAim = m_charInstance->GetISkeletonPose()->GetIPoseBlenderAim();
		if (poseBlenderAim)
		{
			poseBlenderAim->SetState(true);
			poseBlenderAim->SetTarget(lookPos);
			poseBlenderAim->SetPolarCoordinatesSmoothTimeSeconds(smoothTime);
			poseBlenderAim->SetLayer(ikLayer);
			poseBlenderAim->SetFadeInSpeed(blendTime);
		}
	}

	virtual void OnExit(float blendTime)
	{
		IAnimationPoseBlenderDir* poseBlenderAim = m_charInstance->GetISkeletonPose()->GetIPoseBlenderAim();
		if (poseBlenderAim)
		{
			poseBlenderAim->SetState(false);
			poseBlenderAim->SetFadeOutSpeed(blendTime);
		}
	}

	virtual void Update(float timePassed)
	{
		m_lookAroundTime -= timePassed;

		if (m_lookAroundTime < 0.0f)
		{
			UpdateLookTarget();
		}

		Vec3 lookPos = m_entity->GetWorldPos();
		lookPos += m_entity->GetRotation() * (m_lookOffset * 10.0f);

		IAnimationPoseBlenderDir* poseBlenderAim = m_charInstance->GetISkeletonPose()->GetIPoseBlenderAim();
		if (poseBlenderAim)
		{
			poseBlenderAim->SetState(true);
			poseBlenderAim->SetTarget(lookPos);
		}
	}

	void UpdateLookTarget()
	{
		const SAimAroundParams& params = GetParams();

		//--- TODO! Context use of random number generator!
		float yaw = cry_random(params.yawMin, params.yawMax);
		float pitch = cry_random(params.pitchMin, params.pitchMax);
		m_lookOffset.Set(sin_tpl(yaw), cos_tpl(yaw), 0.0f);
		m_lookAroundTime = cry_random(params.timeMin, params.timeMax);
	}

public:
	float m_lookAroundTime;
	Vec3  m_lookOffset;
};

CProceduralClipAimAround::CProceduralClipAimAround()
	: m_lookAroundTime(0)
	, m_lookOffset(ZERO)
{
}

REGISTER_PROCEDURAL_CLIP(CProceduralClipAimAround, "RandomAimAround");
