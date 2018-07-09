// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef _PROCEDURAL_WEAPON_ANIMATION_H_
#define _PROCEDURAL_WEAPON_ANIMATION_H_

#include <CryExtension/ClassWeaver.h>

#include "WeaponOffset.h"
#include "WeaponLookOffset.h"
#include "WeaponRecoilOffset.h"
#include "WeaponStrafeOffset.h"
#include "WeaponZoomOffset.h"
#include "WeaponBumpOffset.h"
#include "WeaponOffsetInput.h"

#include <CryAnimation/ICryAnimation.h>

class CPlayer;
class CWeaponOffsetInput;


class CProceduralWeaponAnimation
{
public:
	CProceduralWeaponAnimation();

	void Update(float deltaTime);

	CWeaponZoomOffset& GetZoomOffset() {return m_weaponZoomOffset;}
	CLookOffset& GetLookOffset() {return m_lookOffset;}
	CStrafeOffset& GetStrafeOffset() {return m_strafeOffset;}
	CRecoilOffset& GetRecoilOffset() {return m_recoilOffset;}
	CBumpOffset& GetBumpOffset() {return m_bumpOffset;}
	void AddCustomOffset(const QuatT& offset);

	QuatT GetRightOffset() const {return m_rightOffset;}
	QuatT GetLeftOffset() const {return m_leftOffset;}

private:
	void UpdateDebugState();
	void ComputeOffsets(float deltaTime);
	void ResetCustomOffset();

	CWeaponZoomOffset m_weaponZoomOffset;
	CLookOffset m_lookOffset;
	CStrafeOffset m_strafeOffset;
	CRecoilOffset m_recoilOffset;
	CBumpOffset m_bumpOffset;
	QuatT m_customOffset;

	QuatT m_rightOffset;
	QuatT m_leftOffset;

	CWeaponOffsetInput::TWeaponOffsetInput m_weaponOffsetInput;
	bool m_debugInput;
};



#endif
