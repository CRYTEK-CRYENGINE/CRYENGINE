// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 11:9:2005   15:00 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Automatic.h"
#include "Actor.h"
#include "ScreenEffects.h"

#include "WeaponSharedParams.h"
#include "Game.h"


CRY_IMPLEMENT_GTI(CAutomatic, CSingle);


//------------------------------------------------------------------------
CAutomatic::CAutomatic()
{
}

//------------------------------------------------------------------------
CAutomatic::~CAutomatic()
{
}

//------------------------------------------------------------------------
void CAutomatic::StartReload(int zoomed)
{
	m_firing = false;
	BaseClass::StartReload(zoomed);
}

//------------------------------------------------------------------------
void CAutomatic::StartFire()
{
	BaseClass::StartFire();

	if (m_firing)
		m_pWeapon->PlayAction(GetFragmentIds().spin_down_tail);

	m_pWeapon->RequestStartFire();
}
//------------------------------------------------------------------------
void CAutomatic::Update(float frameTime, uint32 frameId)
{
	BaseClass::Update(frameTime, frameId);

	if (m_firing && CanFire(false))
	{
		m_firing = Shoot(true, m_fireParams->fireparams.autoReload);

		if(!m_firing)
		{
			StopFire();
		}
	}
}

//------------------------------------------------------------------------
void CAutomatic::StopFire()
{
	if(m_firing)
		SmokeEffect();

	m_firing = false;

	m_pWeapon->RequestStopFire();

	BaseClass::StopFire();
}

//---------------------------------------------------
void CAutomatic::GetMemoryUsage(ICrySizer * s) const
{
	s->AddObject(this, sizeof(*this));	
	GetInternalMemoryUsage(s); 
}

void CAutomatic::GetInternalMemoryUsage(ICrySizer * s) const
{
	CSingle::GetInternalMemoryUsage(s);		// collect memory of parent class
}
