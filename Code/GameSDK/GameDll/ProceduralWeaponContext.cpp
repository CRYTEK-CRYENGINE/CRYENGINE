// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProceduralWeaponContext.h"
#include "GameCVars.h"
#include <CryExtension/ClassWeaver.h>
#include <CryExtension/CryCreateClassInstance.h>



CProceduralWeaponAnimationContext::SParams::SParams()
	:	m_weaponTargetIdx(-1)
	,	m_rightBlendIkIdx(-1)
	,	m_leftHandTargetIdx(-1)
{
}



CProceduralWeaponAnimationContext::SParams::SParams(IDefaultSkeleton& rIDefaultSkeleton)
{
	m_weaponTargetIdx =   rIDefaultSkeleton.GetJointIDByName("Bip01 RHand2RiflePos_IKTarget");
	m_leftHandTargetIdx = rIDefaultSkeleton.GetJointIDByName("Bip01 LHand2Weapon_IKTarget");
	m_rightBlendIkIdx =   rIDefaultSkeleton.GetJointIDByName("Bip01 RHand2RiflePos_IKBlend");

	assert(m_weaponTargetIdx != -1);
	assert(m_leftHandTargetIdx != -1);
	assert(m_rightBlendIkIdx != -1);
}



CProceduralWeaponAnimationContext::CProceduralWeaponAnimationContext()
	:	m_aimDirection(0.0f, 1.0f, 0.0f)
	,	m_pScope(0)
	,	m_instanceCount(0)
{
}



void CProceduralWeaponAnimationContext::Update(float timePassed)
{
	ICharacterInstance* pCharacter = m_pScope->GetCharInst();
	if (m_instanceCount <= 0 || pCharacter == 0)
		return;

	m_weaponSway.Update(timePassed);
	const QuatT rightOffset = m_weaponSway.GetRightOffset();
	const QuatT leftOffset = m_weaponSway.GetLeftOffset();

	const int PWALayer = GetGameConstCVar(g_pwaLayer);
	pCharacter->GetISkeletonAnim()->PushPoseModifier(PWALayer, cryinterface_cast<IAnimationPoseModifier>(m_pPoseModifier), "ProceduralWeapon");

	m_pPoseModifier->Clear();

	const IAnimationOperatorQueue::EOp set = IAnimationOperatorQueue::eOp_OverrideRelative;
	const IAnimationOperatorQueue::EOp additive = IAnimationOperatorQueue::eOp_Additive;

	ISkeletonPose* pPose = pCharacter->GetISkeletonPose();
	Vec3 relBlendPos = Vec3(1.0f, 0.0f, 0.0f);
	m_pPoseModifier->PushPosition(m_params.m_rightBlendIkIdx, set, relBlendPos);

	Quat view = Quat::CreateRotationVDir(m_aimDirection);
	Quat invView = view.GetInverted();

	QuatT rightHand;
	rightHand.t = view * rightOffset.t;
	rightHand.q = view * (rightOffset.q * invView);
	m_pPoseModifier->PushPosition(m_params.m_weaponTargetIdx, additive, rightHand.t);
	m_pPoseModifier->PushOrientation(m_params.m_weaponTargetIdx, additive, rightHand.q);

	QuatT leftHand;
	leftHand.t = view * leftOffset.t;
	leftHand.q = view * (leftOffset.q * invView);
	m_pPoseModifier->PushPosition(m_params.m_leftHandTargetIdx, additive, leftHand.t);
	m_pPoseModifier->PushOrientation(m_params.m_leftHandTargetIdx, additive, leftHand.q);
}



void CProceduralWeaponAnimationContext::SetAimDirection(Vec3 direction)
{
	m_aimDirection = direction;
}



void CProceduralWeaponAnimationContext::Initialize(IScope* pScope)
{
	++m_instanceCount;

	if (m_pScope != 0)
		return;

	m_pScope = pScope;
	ICharacterInstance* pCharacter = m_pScope->GetCharInst();
	if (pCharacter)
	{
		CryCreateClassInstanceForInterface(cryiidof<IAnimationOperatorQueue>(), m_pPoseModifier);
		m_params = SParams(pCharacter->GetIDefaultSkeleton());
	}
}



void CProceduralWeaponAnimationContext::Finalize()
{
	--m_instanceCount;
	CRY_ASSERT(m_instanceCount >= 0);
}


CRYREGISTER_CLASS(CProceduralWeaponAnimationContext);
