// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Controls player movement when using a mounted gun

-------------------------------------------------------------------------
History:
- 2:10:2009   Created by Benito Gangoso Rodriguez

*************************************************************************/

#pragma once

#ifndef _MOUNTED_GUN_CONTROLLER_H_
#define _MOUNTED_GUN_CONTROLLER_H_

#include "PlayerAnimation.h"

class CPlayer;
class CItem;


class CMountedGunController
{
public:
	CMountedGunController()
		: m_pControlledPlayer(NULL)
		, m_pMovementAction(NULL)
		, m_pMannequinParams(NULL)
		, m_PreviousThirdPersonState(false)
	{
	}

	~CMountedGunController() { SAFE_RELEASE(m_pMovementAction); }

	ILINE void InitWithPlayer(CPlayer* pPlayer){ assert(m_pControlledPlayer == NULL); m_pControlledPlayer = pPlayer; }
	void InitMannequinParams();

	void OnEnter(EntityId mountedGunID);
	void OnLeave();
	void Update(EntityId mountedGunID, float frameTime);

	static float CalculateAnimationTime(float aimRad);

	static void ReplayStartThirdPersonAnimations(ICharacterInstance* pCharacter, int upAnimId, int downAnimId);
	static void ReplayStopThirdPersonAnimations(ICharacterInstance* pCharacter);
	static void ReplayUpdateThirdPersonAnimations(ICharacterInstance* pCharacter, float aimRad, float aimUp, float aimDown, bool firstPerson = false);

private:

	void UpdateGunnerLocation(CItem* pMountedGun, IEntity* pParent, const Vec3& bodyDirection);
	void UpdateFirstPersonAnimations(CItem* pMountedGun, const Vec3 &aimDirection);
	void UpdateIKMounted(CItem* pMountedGun);
	Vec3 GetMountDirection(CItem* pMountedGun, IEntity* pParent) const;
	Vec3 GetMountedGunPosition(CItem* pMountedGun, IEntity* pParent) const;

	CPlayer*		m_pControlledPlayer;
	TPlayerAction* m_pMovementAction;
	const struct SMannequinMountedGunParams* m_pMannequinParams;
	bool m_PreviousThirdPersonState;
};

#endif
