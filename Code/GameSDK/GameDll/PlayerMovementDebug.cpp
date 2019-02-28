// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Player movement debug for non-release builds
-------------------------------------------------------------------------
History:
- 16:07:2010   Extracted from CPlayer code by Benito Gangoso Rodriguez

*************************************************************************/

#include "StdAfx.h"
#include "PlayerMovementDebug.h"

#ifdef PLAYER_MOVEMENT_DEBUG_ENABLED
#include "GameCVars.h"

#include <CryAction/IDebugHistory.h>

CPlayerDebugMovement::CPlayerDebugMovement()
: m_pDebugHistoryManager(NULL)
{

}

CPlayerDebugMovement::~CPlayerDebugMovement()
{
	SAFE_RELEASE(m_pDebugHistoryManager);
}


void CPlayerDebugMovement::DebugGraph_AddValue( const char* id, float value ) const
{

	if ((m_pDebugHistoryManager == NULL) || (id == NULL))
		return;

	// NOTE: It's alright to violate the const here. The player is a good common owner for debug graphs, 
	// but it's also not non-const in all places, even though graphs might want to be added from those places.
	IDebugHistory* pDebugHistory = const_cast<IDebugHistoryManager*>(m_pDebugHistoryManager)->GetHistory(id);	
	if (pDebugHistory)
	{
		pDebugHistory->AddValue(value);
	}

}

void CPlayerDebugMovement::Debug( const IEntity* pPlayerEntity )
{
	CRY_ASSERT(pPlayerEntity);

	bool debug = true;

	const char* filter = g_pGameCVars->pl_debug_filter->GetString();
	const char* name = pPlayerEntity->GetName();
	if ((strcmp(filter, "0") != 0) && (strcmp(filter, name) != 0))
	{
		debug = false;
	}

	if (!debug)
	{
		if (m_pDebugHistoryManager)
		{
			m_pDebugHistoryManager->Clear();
		}
		return;
	}

	if (m_pDebugHistoryManager == NULL)
	{
		m_pDebugHistoryManager = g_pGame->GetIGameFramework()->CreateDebugHistoryManager();
	}
	CRY_ASSERT(m_pDebugHistoryManager);

	bool showReqVelo = (g_pGameCVars->pl_debug_movement != 0);
	m_pDebugHistoryManager->LayoutHelper("ReqVelo", NULL, showReqVelo, -20, 20, 0, 5, 0.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("ReqVeloX", NULL, showReqVelo, -20, 20, -5, 5, 1.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("ReqVeloY", NULL, showReqVelo, -20, 20, -5, 5, 2.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("ReqVeloZ", NULL, showReqVelo, -20, 20, -5, 5, 3.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("ReqRotZ", NULL, showReqVelo, -360, 360, -5, 5, 4.0f, 0.0f);

	m_pDebugHistoryManager->LayoutHelper("PhysVelReq", NULL, showReqVelo, -20, 20, 0, 5, 0.0f, 1.0f, 1,0.9f);
	m_pDebugHistoryManager->LayoutHelper("PhysVelReqX", NULL, showReqVelo, -20, 20, -5, 5, 1.0f, 1.0f, 1,0.9f);
	m_pDebugHistoryManager->LayoutHelper("PhysVelReqY", NULL, showReqVelo, -20, 20, -5, 5, 2.0f, 1.0f, 1,0.9f);
	m_pDebugHistoryManager->LayoutHelper("PhysVelReqZ", NULL, showReqVelo, -20, 20, -5, 5, 3.0f, 1.0f, 1,0.9f);

	m_pDebugHistoryManager->LayoutHelper("PhysVelo", NULL, showReqVelo, -20, 20, 0, 5, 0.0f, 1.9f, 1,0.9f);
	m_pDebugHistoryManager->LayoutHelper("PhysVeloX", NULL, showReqVelo, -20, 20, -5, 5, 1.0f, 1.9f, 1,0.9f);
	m_pDebugHistoryManager->LayoutHelper("PhysVeloY", NULL, showReqVelo, -20, 20, -5, 5, 2.0f, 1.9f, 1,0.9f);
	m_pDebugHistoryManager->LayoutHelper("PhysVeloZ", NULL, showReqVelo, -20, 20, -5, 5, 3.0f, 1.9f, 1,0.9f);

	m_pDebugHistoryManager->LayoutHelper("PhysVeloUn", NULL, showReqVelo, -20, 20, 0, 5, 0.0f, 2.8f, 1,0.2f);
	m_pDebugHistoryManager->LayoutHelper("PhysVeloUnX", NULL, showReqVelo, -20, 20, -5, 5, 1.0f, 2.8f, 1,0.2f);
	m_pDebugHistoryManager->LayoutHelper("PhysVeloUnY", NULL, showReqVelo, -20, 20, -5, 5, 2.0f, 2.8f, 1,0.2f);
	m_pDebugHistoryManager->LayoutHelper("PhysVeloUnZ", NULL, showReqVelo, -20, 20, -5, 5, 3.0f, 2.8f, 1,0.2f);

	bool showReqAim = (g_pGameCVars->pl_debug_aiming != 0);
	m_pDebugHistoryManager->LayoutHelper("AimH", NULL, showReqAim, -0.2f, 0.2f, -0.1f, 0.1f, 0.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("AxxAimH", NULL, showReqAim, -1.0f, 1.0f, -0.1f, 0.1f, 1.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("AimHAxxMul", NULL, showReqAim, 0, 10, 0, 1, 2.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("AimHAxxFrac", NULL, showReqAim, 0, 1, 0, 1, 3.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("AimHAxxTime", NULL, showReqAim, 0, 2, 0, 1, 4.0f, 0.0f);

	m_pDebugHistoryManager->LayoutHelper("AimV", NULL, showReqAim, -0.2f, 0.2f, -0.1f, 0.1f, 0.0f, 1.0f);
	m_pDebugHistoryManager->LayoutHelper("AxxAimV", NULL, showReqAim, -1.0f, 1.0f, -0.1f, 0.1f, 1.0f, 1.0f);
	m_pDebugHistoryManager->LayoutHelper("AimVAxxMul", NULL, showReqAim, 0, 10, 0, 1, 2.0f, 1.0f);
	m_pDebugHistoryManager->LayoutHelper("AimVAxxFrac", NULL, showReqAim, 0, 1, 0, 1, 3.0f, 1.0f);
	m_pDebugHistoryManager->LayoutHelper("AimVAxxTime", NULL, showReqAim, 0, 2, 0, 1, 4.0f, 1.0f);

	bool showAimAssist = false;
	m_pDebugHistoryManager->LayoutHelper("AimFollowH", NULL, showAimAssist, -0.05f, 0.05f, -0.05f, 0.05f, 0.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("AimFollowV", NULL, showAimAssist, -0.05f, 0.05f, -0.05f, 0.05f, 1.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("AimScale", NULL, showAimAssist, 0, 1, 0, 1, 2.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("AimDeltaH", NULL, showAimAssist, -0.03f, +0.03f, -0.03f, +0.03f, 0.0f, 1.0f);
	m_pDebugHistoryManager->LayoutHelper("AimDeltaV", NULL, showAimAssist, -0.03f, +0.03f, -0.03f, +0.03f, 1.0f, 1.0f);

	m_pDebugHistoryManager->LayoutHelper("FollowCoolDownH", NULL, showAimAssist, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 2.0f);
	m_pDebugHistoryManager->LayoutHelper("FollowCoolDownV", NULL, showAimAssist, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 2.0f);

	/*
	m_pDebugHistoryManager->LayoutHelper("InputMoveX", NULL, showReqVelo, -1, 1, -1, 1, 0.0f, 1.0f);
	m_pDebugHistoryManager->LayoutHelper("InputMoveY", NULL, showReqVelo, -1, 1, -1, 1, 1.0f, 1.0f);
	/**/

	/*
	bool showVelo = true;
	m_pDebugHistoryManager->LayoutHelper("Velo", NULL, showVelo, -20, 20, 0, 8, 0.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("VeloX", NULL, showVelo, -20, 20, -5, 5, 1.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("VeloY", NULL, showVelo, -20, 20, -5, 5, 2.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("VeloZ", NULL, showVelo, -20, 20, -5, 5, 3.0f, 0.0f);
	/**/

	/*
	m_pDebugHistoryManager->LayoutHelper("Axx", NULL, showVelo, -20, 20, -1, 1, 0.0f, 1.0f);
	m_pDebugHistoryManager->LayoutHelper("AxxX", NULL, showVelo, -20, 20, -1, 1, 1.0f, 1.0f);
	m_pDebugHistoryManager->LayoutHelper("AxxY", NULL, showVelo, -20, 20, -1, 1, 2.0f, 1.0f);
	m_pDebugHistoryManager->LayoutHelper("AxxZ", NULL, showVelo, -20, 20, -1, 1, 3.0f, 1.0f);
	/**/

	//m_pDebugHistoryManager->LayoutHelper("ModelOffsetX", NULL, true, 0, 1, -0.5, 0.5, 5.0f, 0.5f);
	//m_pDebugHistoryManager->LayoutHelper("ModelOffsetY", NULL, true, 0, 1, 0, 1, 5.0f, 1.5f, 1.0f, 0.5f);

	//*
	bool showJump = (g_pGameCVars->pl_debug_jumping != 0);
	m_pDebugHistoryManager->LayoutHelper("OnGround", NULL, showJump, 0, 1, 0, 1, 5.0f, 0.5f, 1.0f, 0.5f);
	m_pDebugHistoryManager->LayoutHelper("Jumping", NULL, showJump, 0, 1, 0, 1, 5.0f, 1.0f, 1.0f, 0.5f);
	m_pDebugHistoryManager->LayoutHelper("Flying", NULL, showJump, 0, 1, 0, 1, 5.0f, 1.5f, 1.0f, 0.5f);
	m_pDebugHistoryManager->LayoutHelper("StuckTimer", NULL, showJump, 0, 0.5, 0, 0.5, 5.0f, 2.0f, 1.0f, 1.0f);
	m_pDebugHistoryManager->LayoutHelper("InAirTimer", NULL, showJump, 0, 5, 0, 5, 4.0f, 2.0f, 1.0f, 1.0f);
	m_pDebugHistoryManager->LayoutHelper("InWaterTimer", NULL, showJump, -5, 5, -0.5, 0.5, 4, 3);
	m_pDebugHistoryManager->LayoutHelper("OnGroundTimer", NULL, showJump, 0, 5, 0, 5, 4.0f, 1.0f, 1.0f, 1.0f);
	/**/

	//*
	m_pDebugHistoryManager->LayoutHelper("GroundSlope", NULL, showJump, 0, 90, 0, 90, 0, 3);
	m_pDebugHistoryManager->LayoutHelper("GroundSlopeMod", NULL, showJump, 0, 90, 0, 90, 1, 3);
	/**/

	//m_pDebugHistoryManager->LayoutHelper("ZGDashTimer", NULL, showVelo, -20, 20, -0.5, 0.5, 5.0f, 0.5f);
	/*
	m_pDebugHistoryManager->LayoutHelper("StartTimer", NULL, showVelo, -20, 20, -0.5, 0.5, 5.0f, 0.5f);
	m_pDebugHistoryManager->LayoutHelper("DodgeFraction", NULL, showVelo, 0, 1, 0, 1, 5.0f, 1.5f, 1.0f, 0.5f);
	m_pDebugHistoryManager->LayoutHelper("RampFraction", NULL, showVelo, 0, 1, 0, 1, 5.0f, 2.0f, 1.0f, 0.5f);
	m_pDebugHistoryManager->LayoutHelper("ThrustAmp", NULL, showVelo, 0, 5, 0, 5, 5.0f, 2.5f, 1.0f, 0.5f);
	*/

}

void CPlayerDebugMovement::LogFallDamage( const IEntity* pPlayerEntity, const float velocityFraction, const float impactVelocity, const float damage )
{
	CRY_ASSERT(pPlayerEntity);

	if (g_pGameCVars->pl_health.debug_FallDamage != 0)
	{
		const char* side = gEnv->bServer ? "Server" : "Client";

		const char* color = "";
		if (velocityFraction < 0.33f)
			color = "$6"; // Yellow
		else if (velocityFraction < 0.66f)
			color = "$8"; // Orange
		else
			color = "$4"; // Red

		CryLog("%s[%s][%s] ImpactVelo=%3.2f, FallDamage=%3.1f", color, side, pPlayerEntity->GetName(), impactVelocity, damage);
	}
}

void CPlayerDebugMovement::LogFallDamageNone( const IEntity* pPlayerEntity, const float impactVelocity )
{
	CRY_ASSERT(pPlayerEntity);

	if (g_pGameCVars->pl_health.debug_FallDamage != 0)
	{
		if (impactVelocity > 0.5f)
		{
			const char* side = gEnv->bServer ? "Server" : "Client";
			const char* color = "$3"; // Green
			CryLog("%s[%s][%s] ImpactVelo=%3.2f, FallDamage: NONE", 
				color, side, pPlayerEntity->GetName(), impactVelocity);
		}
	}
}

void CPlayerDebugMovement::LogVelocityStats( const IEntity* pPlayerEntity, const pe_status_living& livStat, const float fallSpeed, const float impactVelocity )
{
	CRY_ASSERT(pPlayerEntity);

	if (g_pGameCVars->pl_health.debug_FallDamage == 2)
	{
		const Vec3 pos = pPlayerEntity->GetWorldPos();
		const char* side = gEnv->bServer ? "Server" : "Client";
		CryLog("[%s] liv.vel=%0.1f,%0.1f,%3.2f liv.velU=%0.1f,%0.1f,%3.2f impactVel=%3.2f posZ=%3.2f (liv.velReq=%0.1f,%0.1f,%3.2f) (fallspeed=%3.2f) gt=%3.3f, pt=%3.3f", 
			side, 
			livStat.vel.x, livStat.vel.y, livStat.vel.z, 
			livStat.velUnconstrained.x, livStat.velUnconstrained.y, livStat.velUnconstrained.z, 
			impactVelocity, 
			/*pos.x, pos.y,*/ pos.z, 
			livStat.velRequested.x, livStat.velRequested.y, livStat.velRequested.z, 
			fallSpeed, 
			gEnv->pTimer->GetCurrTime(), gEnv->pPhysicalWorld->GetPhysicsTime());
	}
}

void CPlayerDebugMovement::GetInternalMemoryUsage( ICrySizer * pSizer ) const
{
	pSizer->AddObject(m_pDebugHistoryManager);
}

#endif //PLAYER_MOVEMENT_DEBUG_ENABLED