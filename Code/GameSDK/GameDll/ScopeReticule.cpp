// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Iron Sight

-------------------------------------------------------------------------
History:
- 17:09:2010   Created by Filipe Amim

*************************************************************************/
#include "StdAfx.h"
#include "ScopeReticule.h"
#include "WeaponSharedParams.h"



CScopeReticule::CScopeReticule()
:	m_disabledTimeOut(0.0f)
,	m_blinkFrequency(4.0f)
{
}



void CScopeReticule::SetMaterial(IMaterial* pMaterial)
{
	m_scopeReticuleMaterial = pMaterial;
}



void CScopeReticule::SetBlinkFrequency(float blink)
{
	m_blinkFrequency = blink;
}



void CScopeReticule::Disable(float time)
{
	m_disabledTimeOut = time;
}



void CScopeReticule::Update(CWeapon* pWeapon)
{
	if (!m_scopeReticuleMaterial || (m_blinkFrequency==0.f))
		return;

	const float frequency = (float)__fsel(-m_blinkFrequency, 12.0f, m_blinkFrequency);
	const float maxOpacity = 0.99f;
	float opacity = maxOpacity;
	if (!pWeapon->CanFire() && !pWeapon->IsDeselecting())
		opacity = (float)__fsel(fmod_tpl(gEnv->pTimer->GetCurrTime(), 1.0f/frequency)*frequency-0.5f, maxOpacity, 0.0f);
	m_disabledTimeOut = max(m_disabledTimeOut - gEnv->pTimer->GetFrameTime(), 0.0f);
	opacity = (float)__fsel(-m_disabledTimeOut, opacity, 0.0f);

	m_scopeReticuleMaterial->SetGetMaterialParamFloat("opacity", opacity, false);
}
