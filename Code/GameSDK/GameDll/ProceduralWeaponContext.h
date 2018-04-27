// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef _PROCEDURAL_WEAPON_CONTEXT_H_
#define _PROCEDURAL_WEAPON_CONTEXT_H_

#include "ICryMannequin.h"
#include "ProceduralWeaponAnimation.h"



class CProceduralWeaponAnimationContext : public IProceduralContext
{
private:
	struct SParams
	{
		SParams();
		SParams(IDefaultSkeleton& pSkeleton);

		int m_weaponTargetIdx;
		int m_leftHandTargetIdx;
		int m_rightBlendIkIdx;
	};

public:
	PROCEDURAL_CONTEXT(CProceduralWeaponAnimationContext, "ProceduralWeaponAnimationContext", "df1d02e0-5f40-48a1-bc77-59dcc568aa7f"_cry_guid);

	CProceduralWeaponAnimationContext();
	virtual ~CProceduralWeaponAnimationContext() {}

	virtual void Update(float timePassed) override;
	void SetAimDirection(Vec3 direction);
	void Initialize(IScope* pScope);
	void Finalize();
	CProceduralWeaponAnimation& GetWeaponSway() {return m_weaponSway;}

private:
	SParams m_params;
	IAnimationOperatorQueuePtr m_pPoseModifier;
	IScope* m_pScope;

	CProceduralWeaponAnimation m_weaponSway;
	Vec3 m_aimDirection;
	int m_instanceCount;
};



#endif
