// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EMPGrenade.h"
#include "Player.h"

CEMPGrenade::CEMPGrenade() : m_bActive(false), m_pulsePos(ZERO)
{
	m_postExplosionLifetime = g_pGameCVars->g_empOverTimeGrenadeLife;
}

CEMPGrenade::~CEMPGrenade()
{
}

void CEMPGrenade::Update( SEntityUpdateContext &ctx, int updateSlot )
{
	if (m_bActive)
	{
		const Matrix34& worldMat = GetEntity()->GetWorldTM();

		if(gEnv->bServer && m_postExplosionLifetime > 0.f)
		{
			m_postExplosionLifetime -= ctx.fFrameTime;
			if(m_postExplosionLifetime <= 0.f)
			{
				Destroy();
			}
		}
	}

	BaseClass::Update(ctx, updateSlot);
}

void CEMPGrenade::HandleEvent( const SGameObjectEvent &event )
{
	CGrenade::HandleEvent(event);

	if (event.event == eGFE_OnCollision)
	{
		if(!m_bActive)
		{

			m_bActive = true;

			const Matrix34& worldMat = GetEntity()->GetWorldTM();
			m_pulsePos = worldMat.GetColumn3();
		}
	}
}
