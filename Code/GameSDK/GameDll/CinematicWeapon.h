// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description: Weapon to be used during cut-scenes with restricted player input

-------------------------------------------------------------------------
History:
- 18:06:2012: Created by Benito G.R

*************************************************************************/

#pragma once

#ifndef _CINEMATIC_WEAPON_H_
#define _CINEMATIC_WEAPON_H_

#include "Weapon.h"
#include "Audio/AudioSignalPlayer.h"

class CCinematicWeapon : public CWeapon
{

	enum EInputClass
	{
		eInputClass_None		 = 0,
		eInputClass_Primary = 1,
		eInputClass_Secondary,
	};

	typedef CWeapon BaseClass;

public:
	CCinematicWeapon();
	virtual ~CCinematicWeapon();

	virtual void HandleEvent( const SGameObjectEvent& goEvent );
	virtual void OnAction( EntityId actorId, const ActionId& actionId, int activationMode, float value );

	virtual void StartFire();
	virtual void StopFire();

private:

	void SetupFireSound();

	void SetPrimary();
	void SetSecondary();
	void Disable();

	EInputClass	m_inputClass;
	CAudioSignalPlayer m_fireSound;
	CAudioSignalPlayer m_stopFireTailSound;
};

#endif //_CINEMATIC_WEAPON_H_