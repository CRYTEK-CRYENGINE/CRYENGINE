// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#ifndef __FIRST_PERSON_HAND_IK_CONTEXT_H__
#define __FIRST_PERSON_HAND_IK_CONTEXT_H__

#include <CryExtension/ClassWeaver.h>
#include <CryAnimation/ICryAnimation.h>
#include "ICryMannequin.h"

class CFirstPersonHandIKContext : public IProceduralContext
{
private:
	struct SParams
	{
		SParams();
		SParams(IDefaultSkeleton* pIDefaultSkeleton);

		int m_weaponTargetIdx;
		int m_leftHandTargetIdx;
		int m_rightBlendIkIdx;
	};

	CFirstPersonHandIKContext();
	virtual ~CFirstPersonHandIKContext() {}

public:
	PROCEDURAL_CONTEXT(CFirstPersonHandIKContext, "FirstPersonHandIK", "d8a55b34-9caa-4b53-89bc-f1708d565bc3"_cry_guid);

	virtual void Initialize(ICharacterInstance* pCharacterInstance);
	virtual void Finalize();
	virtual void Update(float timePassed) override;

	virtual void SetAimDirection(Vec3 aimDirection);
	virtual void AddRightOffset(QuatT offset);
	virtual void AddLeftOffset(QuatT offset);

private:
	SParams                    m_params;
	IAnimationOperatorQueuePtr m_pPoseModifier;
	ICharacterInstance*        m_pCharacterInstance;

	QuatT                      m_rightOffset;
	QuatT                      m_leftOffset;
	Vec3                       m_aimDirection;
	int                        m_instanceCount;
};

#endif
