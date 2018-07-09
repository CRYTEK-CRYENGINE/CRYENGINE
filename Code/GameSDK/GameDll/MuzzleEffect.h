// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MUZZLEEFFECT_H__
#define __MUZZLEEFFECT_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "EntityUtility/EntityEffects.h"

class CWeapon;
class CFireMode;
struct SFireModeParams;



class CMuzzleEffect
{
private:
	struct SMuzzleEffectInfo
	{
		EntityEffects::TAttachedEffectId mfId[2];

		SMuzzleEffectInfo()
		{
			mfId[0] = mfId[1] = 0;
		}

		void GetMemoryUsage(ICrySizer *pSizer) const{}
	};

public:
	CMuzzleEffect();

	void Initialize(CFireMode* pFireMode);
	void AttachEmitters(CFireMode* pFireMode, int barrel);
	void DetachEmitters(CFireMode* pFireMode, int barrel);

	void StartFire(CFireMode* pFireMode);
	void StopFire(CFireMode* pFireMode);
	void Shoot(CFireMode* pFireMode, Vec3 potentialTarget, int barrel);

	void GetMemoryUsage(ICrySizer * s) const;

private:
	void MuzzleFlashEffect(CFireMode* pFireMode, int barrel);
	bool GetMuzzleFireLocation(const CFireMode* pFireMode, QuatTS* location) const;
	void MuzzleBeamEffect(bool attach, CFireMode* pFireMode);
	void SetBeamTarget(CFireMode* pFireMode, Vec3 potentialTarget);

	std::vector<SMuzzleEffectInfo> m_mfIds;
	uint32 m_beamFxId[2];
};


#endif
