// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Helper class to setup / update ik torso aim for first person 

-------------------------------------------------------------------------
History:
- 20-8-2009		Benito Gangoso Rodriguez

*************************************************************************/

#include "StdAfx.h"

#include <CryAnimation/ICryAnimation.h>
#include "IKTorsoAim_Helper.h"
#include <CryExtension/CryCreateClassInstance.h>

#include "GameCVars.h"
#include "WeaponFPAiming.h"

CIKTorsoAim_Helper::CIKTorsoAim_Helper()
: m_initialized(false)
, m_enabled(false)
, m_blendFactor(0.0f)
, m_blendFactorPosition(0.0f)
{
	CryCreateClassInstanceForInterface(cryiidof<IAnimationPoseModifierTorsoAim>(), m_ikTorsoAim);
	CryCreateClassInstanceForInterface(cryiidof<ITransformationPinning>(), m_transformationPin);
}

void CIKTorsoAim_Helper::Update( CIKTorsoAim_Helper::SIKTorsoParams& ikTorsoParams )
{
	if (!m_initialized)
	{
		Init(ikTorsoParams);
	}

	const int  STAPLayer	= GetGameConstCVar(g_stapLayer);

	const bool justTurnedOn = (m_blendFactor == 0.0f) && m_enabled;
	const float frameTime = gEnv->pTimer->GetFrameTime();
	const float delta = (frameTime * ikTorsoParams.blendRate);
	const float newBlendFactor = m_enabled ? m_blendFactor + delta : m_blendFactor - delta;
	m_blendFactor = clamp_tpl(newBlendFactor, 0.0f, 1.0f);
	const bool blendPosition = ikTorsoParams.needsSTAPPosition;
	if (justTurnedOn)
	{
		m_blendFactorPosition = blendPosition ? 1.0f : 0.0f;
	}
	else
	{
		const float newBlendFactorPos = blendPosition ? m_blendFactorPosition + delta : m_blendFactorPosition - delta;
		m_blendFactorPosition = clamp_tpl(newBlendFactorPos, 0.0f, 1.0f);
	}

	//const float XPOS = 200.0f;
	//const float YPOS = 110.0f;
	//const float FONT_SIZE = 4.0f;
	//const float FONT_COLOUR[4] = {1,1,1,1};
	//IRenderAuxText::Draw2dLabel(XPOS, YPOS, FONT_SIZE, FONT_COLOUR, false, "CIKTorsoAim_Helper::Update: %s", m_blendTime > 0.0f ? "update" : "dont update");

	if (m_blendFactor <= 0.0f)
		return;

	CIKTorsoAim *torsoAim = m_ikTorsoAim.get();

	torsoAim->SetBlendWeight(m_blendFactor);
	torsoAim->SetBlendWeightPosition(m_blendFactorPosition);
	torsoAim->SetTargetDirection(ikTorsoParams.aimDirection);
	torsoAim->SetViewOffset(ikTorsoParams.viewOffset);
	torsoAim->SetAbsoluteTargetPosition(ikTorsoParams.targetPosition);

	ikTorsoParams.character->GetISkeletonAnim()->PushPoseModifier(STAPLayer, cryinterface_cast<IAnimationPoseModifier>(m_ikTorsoAim), "IKTorsoAimHelper");

	if (ikTorsoParams.shadowCharacter)
	{
		m_transformationPin->SetBlendWeight(ikTorsoParams.updateTranslationPinning ? m_blendFactor : 0.0f);
		ikTorsoParams.character->GetISkeletonAnim()->PushPoseModifier(15, cryinterface_cast<IAnimationPoseModifier>(m_transformationPin), "IKTorsoAimHelper");
	}
}


void CIKTorsoAim_Helper::Init( CIKTorsoAim_Helper::SIKTorsoParams& ikTorsoParams )
{
	m_initialized = true;

	int effectorJoint = ikTorsoParams.effectorJoint;
	if (effectorJoint == -1)
		effectorJoint = ikTorsoParams.character->GetIDefaultSkeleton().GetJointIDByName("Bip01 Camera");

	int aimJoint = ikTorsoParams.aimJoint;
	if (aimJoint == -1)
		aimJoint = ikTorsoParams.character->GetIDefaultSkeleton().GetJointIDByName("Bip01 Spine2");

	int pinJoint = ikTorsoParams.pinJoint;
	if (pinJoint == -1)
		pinJoint = ikTorsoParams.character->GetIDefaultSkeleton().GetJointIDByName("Bip01 Spine1");

	assert(effectorJoint != -1);
	assert(aimJoint != -1);
	assert(pinJoint != -1);

	m_ikTorsoAim->SetJoints(effectorJoint, aimJoint);

	const uint32 numWeights = 3;
	f32 weights[numWeights] = {0.4f, 0.75f, 1.0f};
	
	m_ikTorsoAim->SetFeatherWeights(numWeights, weights);

	if (ikTorsoParams.shadowCharacter)
	{
		m_transformationPin->SetSource(ikTorsoParams.shadowCharacter);
		m_transformationPin->SetJoint(pinJoint);
	}
}


void CIKTorsoAim_Helper::Enable(bool snap)
{
	m_enabled = true;
	if (snap)
	{
		m_blendFactor = 1.0f;
	}
}

void CIKTorsoAim_Helper::Disable(bool snap)
{
	m_enabled = false;
	if (snap)
	{
		m_blendFactor = 0.0f;
	}
}

const QuatT &CIKTorsoAim_Helper::GetLastEffectorTransform() const
{
	return m_ikTorsoAim->GetLastProcessedEffector();
}
