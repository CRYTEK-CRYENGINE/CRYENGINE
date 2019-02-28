// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 04:05:2012   16:00 : Created by Jan Neugebauer

*************************************************************************/
#include "StdAfx.h"
#include "ThrowIndicator.h"

#include "Throw.h"
#include "LTagSingle.h"

#include "UI/HUD/HUDEventDispatcher.h"

CRY_IMPLEMENT_GTI(CThrowIndicator, CIronSight);

//------------------------------------------------------------------------
CThrowIndicator::CThrowIndicator()
	: m_indicatorShowing(false)
{
}

//------------------------------------------------------------------------
CThrowIndicator::~CThrowIndicator()
{
}

//------------------------------------------------------------------------
bool CThrowIndicator::StartZoom(bool stayZoomed, bool fullZoomout, int zoomStep)
{
	if (m_pWeapon->IsOwnerClient())
	{
		CThrow* pThrow = crygti_cast<CThrow*>(m_pWeapon->GetCFireMode(m_pWeapon->GetCurrentFireMode()));
		if((pThrow != NULL) && !pThrow->OutOfAmmo())
		{
			const float lifetime = pThrow->GetProjectileLifeTime();

			SHUDEvent trajectoryEvent(eHUDEvent_DisplayTrajectory);
			trajectoryEvent.AddData(SHUDEventData(true));
			trajectoryEvent.AddData(SHUDEventData(lifetime));
			CHUDEventDispatcher::CallEvent(trajectoryEvent);
			m_indicatorShowing = true;
		}
	}
	return false;
}

//------------------------------------------------------------------------
void CThrowIndicator::StopZoom()
{
	if (m_pWeapon->IsOwnerClient())
	{
		SHUDEvent trajectoryEvent(eHUDEvent_DisplayTrajectory);
		trajectoryEvent.AddData(SHUDEventData(false));
		trajectoryEvent.AddData(SHUDEventData(0.0f));
		CHUDEventDispatcher::CallEvent(trajectoryEvent);
		m_indicatorShowing = false;
	}
}

//------------------------------------------------------------------------
void CThrowIndicator::ExitZoom(bool force)
{
	StopZoom();
}
