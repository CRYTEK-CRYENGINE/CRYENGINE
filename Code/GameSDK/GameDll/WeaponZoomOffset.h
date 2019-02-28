// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef _WEAPON_ZOOM_OFFSET_H_
#define _WEAPON_ZOOM_OFFSET_H_

#include "WeaponOffset.h"

struct SParams_WeaponFPAiming;



class CWeaponZoomOffset
{
public:
	CWeaponZoomOffset();

	QuatT Compute(float frameTime);
	QuatT GetLeftHandOffset(float frameTime);

	CWeaponOffsetStack& GetShoulderOffset() {return m_shoulderOffset;}
	CWeaponOffsetStack& GetLeftHandOffset() {return m_leftHandOffset;}
	SWeaponOffset& GetZommOffset() {return m_zoomOffset;}

	void SetZoomTransition(float transition);
	void SetZoomTransitionRotation(float rotation);

private:
	CWeaponOffsetStack m_shoulderOffset;
	CWeaponOffsetStack m_leftHandOffset;
	SWeaponOffset m_zoomOffset;

	float m_zoomRotation;
	float m_zoomTransition;
};



#endif
