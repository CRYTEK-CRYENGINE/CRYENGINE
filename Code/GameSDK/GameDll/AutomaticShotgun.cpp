// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description: Automatic shotgun firemode. It works like the shotgun one, spawning
several pellets on a single shot, but doesn't require 'pump' action
and it has a 'single' magazine reload

-------------------------------------------------------------------------
History:
- 14:09:09   Benito Gangoso Rodriguez

*************************************************************************/
#include "StdAfx.h"
#include "AutomaticShotgun.h"
#include "Actor.h"


CRY_IMPLEMENT_GTI(CAutomaticShotgun, CShotgun);



CAutomaticShotgun::CAutomaticShotgun()
	: m_rapidFireCountdown(0.0f)
	, m_firing(false)
{
}


CAutomaticShotgun::~CAutomaticShotgun()
{
}



void CAutomaticShotgun::Update(float frameTime, uint32 frameId)
{
	CShotgun::Update(frameTime, frameId);

	if (m_firing)
	{
		m_rapidFireCountdown -= frameTime;
		if (m_rapidFireCountdown < 0.0f && CanFire(true))
		{
			bool shot = Shoot(true, true);
			if (shot)
				m_rapidFireCountdown += m_next_shot_dt;
		}
	}
}


void CAutomaticShotgun::StartFire()
{
	if (!m_firing && !m_firePending && GetShared()->shotgunparams.fully_automated)
	{
		m_rapidFireCountdown = m_next_shot_dt;
		m_firing = true;
	}

	CShotgun::StartFire();
}


void CAutomaticShotgun::StopFire()
{
	CShotgun::StopFire();
	m_firing = false;;
}


void CAutomaticShotgun::Activate( bool activate )
{
	m_firing = false;
	CSingle::Activate(activate);
}

void CAutomaticShotgun::StartReload(int zoomed)
{
	CSingle::StartReload(zoomed);
}

void CAutomaticShotgun::EndReload( int zoomed )
{
	CSingle::EndReload(zoomed);
}

void CAutomaticShotgun::CancelReload()
{
	CSingle::CancelReload();
}

bool CAutomaticShotgun::CanCancelReload()
{
	return true;
}

