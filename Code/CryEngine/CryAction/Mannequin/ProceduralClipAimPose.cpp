// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "ICryMannequin.h"

#include <Mannequin/Serialization.h>

#define PROC_CLIP_AIM_POSE_TOKEN_BASE 140000

struct SAimIKParams : public IProceduralParams
{
	SAimIKParams()
		: blendTime(1.0f)
		, layer(4)
	{
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		ar(Serialization::Decorators::AnimationName<SAnimRef>(animRef), "Animation", "Animation");
		ar(blendTime, "BlendTime", "Blend Time");
		ar(Serialization::Decorators::Range<uint32>(layer, 0, 15), "AnimationLayer", "Animation Layer");
	}

	virtual void GetExtraDebugInfo(StringWrapper& extraInfoOut) const override
	{
		extraInfoOut = animRef.c_str();
	}

	SAnimRef animRef;
	float    blendTime;
	uint32   layer;
};

class CProceduralClipAimPose : public TProceduralClip<SAimIKParams>
{
public:
	CProceduralClipAimPose()
		: m_IKLayer(0)
		, m_token(0)
	{
	}

	virtual void OnEnter(float blendTime, float duration, const SAimIKParams& params)
	{
		if (!m_charInstance)
			return;

		Vec3 lookPos;
		if (!GetParam("AimTarget", lookPos))
		{
			lookPos = m_entity->GetWorldPos();
			lookPos += m_entity->GetForwardDir() * 10.0f;
		}

		const float smoothTime = params.blendTime;
		m_IKLayer = (uint32)params.layer;
		if (IAnimationPoseBlenderDir* poseBlenderAim = m_charInstance->GetISkeletonPose()->GetIPoseBlenderAim())
		{
			poseBlenderAim->SetState(true);
			const bool wasPoseBlenderActive = (0.01f < poseBlenderAim->GetBlend());
			if (!wasPoseBlenderActive)
			{
				poseBlenderAim->SetTarget(lookPos);
			}
			poseBlenderAim->SetPolarCoordinatesSmoothTimeSeconds(smoothTime);
			poseBlenderAim->SetLayer(m_IKLayer);
			poseBlenderAim->SetFadeInSpeed(blendTime);

			StartAimAnimation(blendTime);
		}
	}

	virtual void OnExit(float blendTime)
	{
		if (!m_charInstance)
			return;

		IAnimationPoseBlenderDir* poseBlenderAim = m_charInstance->GetISkeletonPose()->GetIPoseBlenderAim();

		StopAimAnimation(blendTime);

		if (poseBlenderAim)
		{
			poseBlenderAim->SetState(false);
			poseBlenderAim->SetFadeOutSpeed(blendTime);
		}
	}

	virtual void Update(float timePassed)
	{
		if (!m_charInstance)
			return;

		QuatT target;
		if (GetParam("AimTarget", target))
		{
			IAnimationPoseBlenderDir* pPoseBlenderAim = m_charInstance->GetISkeletonPose()->GetIPoseBlenderAim();
			if (pPoseBlenderAim)
			{
				pPoseBlenderAim->SetTarget(target.t);
			}
		}
	}

private:
	void StartAimAnimation(const float blendTime)
	{
		const SAimIKParams& params = GetParams();
		if (params.animRef.IsEmpty())
		{
			return;
		}

		m_token = GetNextToken();

		CryCharAnimationParams animParams;
		animParams.m_fTransTime = blendTime;
		animParams.m_nLayerID = m_IKLayer;
		animParams.m_nUserToken = m_token;
		animParams.m_nFlags = CA_LOOP_ANIMATION | CA_ALLOW_ANIM_RESTART;
		int animID = m_charInstance->GetIAnimationSet()->GetAnimIDByCRC(params.animRef.crc);
		m_charInstance->GetISkeletonAnim()->StartAnimationById(animID, animParams);
	}

	void StopAimAnimation(const float blendTime)
	{
		const SAimIKParams& params = GetParams();
		if (params.animRef.IsEmpty())
		{
			return;
		}

		CRY_ASSERT(m_token != 0);

		ISkeletonAnim& skeletonAnimation = *m_charInstance->GetISkeletonAnim();

		const int animationCount = skeletonAnimation.GetNumAnimsInFIFO(m_IKLayer);
		for (int i = 0; i < animationCount; ++i)
		{
			const CAnimation& animation = skeletonAnimation.GetAnimFromFIFO(m_IKLayer, i);
			const uint32 animationToken = animation.GetUserToken();
			if (animationToken == m_token)
			{
				const bool isTopAnimation = (i == (animationCount - 1));
				if (isTopAnimation)
				{
					skeletonAnimation.StopAnimationInLayer(m_IKLayer, blendTime);
				}
				// If we found an proc clip aim animation not in the top it might be indicative that we're reusing this layer for non aim animations,
				// but we can assume that in 99% of cases we're already blending out, since there's a top animation that should be blending in.
				return;
			}
		}
	}

private:
	static uint32 GetNextToken()
	{
		static uint8 s_currentToken = 0;
		s_currentToken++;
		return PROC_CLIP_AIM_POSE_TOKEN_BASE + s_currentToken;
	}

protected:
	uint32 m_IKLayer;
	uint32 m_token;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipAimPose, "AimPose");
