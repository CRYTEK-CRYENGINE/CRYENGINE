// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include <CryCore/Platform/platform.h>
#include "AnimUtils.h"
#include <CryAnimation/ICryAnimation.h>

void AnimUtils::StartAnimation(ICharacterInstance* pCharacter, const char* pAnimName)
{
	CryCharAnimationParams params(0);

	ISkeletonAnim* pISkeletonAnim = (pCharacter ? pCharacter->GetISkeletonAnim() : 0);

	if (pISkeletonAnim)
	{
		pISkeletonAnim->StopAnimationsAllLayers();
	}

	params.m_nFlags |= (CA_MANUAL_UPDATE | CA_REPEAT_LAST_KEY);

	if (pISkeletonAnim)
	{
		pISkeletonAnim->StartAnimation(pAnimName, params);
	}
}

void AnimUtils::SetAnimationTime(ICharacterInstance* pCharacter, const nTime& fNormalizedTime)
{
	assert(fNormalizedTime >= 0 && fNormalizedTime <= 1);
	ISkeletonAnim* pISkeletonAnim = (pCharacter ? pCharacter->GetISkeletonAnim() : 0);
	nTime timeToSet = max(nTime(0), fNormalizedTime);

	if (pISkeletonAnim)
	{
		pISkeletonAnim->SetLayerNormalizedTime(0, timeToSet);
	}
}

void AnimUtils::StopAnimations(ICharacterInstance* pCharacter)
{
	ISkeletonAnim* pISkeletonAnim = (pCharacter ? pCharacter->GetISkeletonAnim() : 0);

	if (pISkeletonAnim)
	{
		pISkeletonAnim->StopAnimationsAllLayers();
	}
}

