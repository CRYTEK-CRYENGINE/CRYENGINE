// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Contains the ray testing code extracted from CPlayer.

-------------------------------------------------------------------------
History:
- 20.10.10: Created by Stephen M. North

*************************************************************************/
#include "StdAfx.h"

#include "PlayerStateSwim_WaterTestProxy.h"
#include "Player.h"

#include "PlayerRotation.h"

#include "GameCVars.h"

float CPlayerStateSwim_WaterTestProxy::s_rayLength = 10.f;

CPlayerStateSwim_WaterTestProxy::CPlayerStateSwim_WaterTestProxy()
	: m_internalState(eProxyInternalState_OutOfWater)
	, m_lastInternalState(eProxyInternalState_OutOfWater)
	, m_lastWaterLevelCheckPosition(ZERO)
	, m_submergedFraction(0.0f)
	, m_bottomLevel(BOTTOM_LEVEL_UNKNOWN)
	, m_relativeBottomLevel(0.0f)
	, m_waterLevel(WATER_LEVEL_UNKNOWN)
	, m_playerWaterLevel(-WATER_LEVEL_UNKNOWN)
	, m_swimmingTimer(-1000.0f)
	, m_drowningTimer(0.0f)
	, m_oxygenLevel(0.0f)
	, m_timeWaterLevelLastUpdated(0.0f)
	, m_lastRayCastResult(BOTTOM_LEVEL_UNKNOWN)
	, m_headUnderwater(false)
	, m_headComingOutOfWater(false)
	, m_shouldSwim(false)
	, m_bottomLevelRayID(0)
{
}

CPlayerStateSwim_WaterTestProxy::~CPlayerStateSwim_WaterTestProxy()
{
	CancelPendingRays();
}

void CPlayerStateSwim_WaterTestProxy::Reset( bool bCancelRays )
{
	if( bCancelRays )
	{
		CancelPendingRays();
		m_bottomLevelRayID = (0);
	}

	m_lastInternalState = m_internalState = eProxyInternalState_OutOfWater;
	m_submergedFraction = 0.0f;
	m_shouldSwim = false;
	m_lastWaterLevelCheckPosition = ZERO;
	m_waterLevel = WATER_LEVEL_UNKNOWN;
	m_bottomLevel = BOTTOM_LEVEL_UNKNOWN;
	m_lastRayCastResult = BOTTOM_LEVEL_UNKNOWN;
	m_relativeBottomLevel = 0.0f;
	m_playerWaterLevel = -WATER_LEVEL_UNKNOWN;
	m_swimmingTimer = 0.0f;
	m_headUnderwater = false;
	m_headComingOutOfWater = false;
}

void CPlayerStateSwim_WaterTestProxy::OnEnterWater(const CPlayer& player)
{
	// On entering water we assume a player submersion depth of 0 meter.
	// This member will be updated as soon as we get the information from the physics system.
	m_playerWaterLevel = 0.0f;
	m_internalState = eProxyInternalState_Swimming;
	m_swimmingTimer = 0.0f;
	
	CRY_ASSERT(m_waterLevel > WATER_LEVEL_UNKNOWN);
	Update(player, 0.0f);
}

void CPlayerStateSwim_WaterTestProxy::OnExitWater(const CPlayer& player)
{
	Reset(true);
}

void CPlayerStateSwim_WaterTestProxy::Update(const CPlayer& player, const float frameTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	if( gEnv->IsEditor() && !gEnv->IsEditorGameMode() )
	{
		m_lastInternalState = m_internalState;
		m_internalState = eProxyInternalState_OutOfWater;
		m_shouldSwim = false;
	}

	float newSwimmingTimer = 0.0f;
	switch(m_internalState)
	{
	case eProxyInternalState_OutOfWater:
		{
			if (m_lastInternalState != eProxyInternalState_OutOfWater)
			{
				Reset(true);
			}
			UpdateOutOfWater(player, frameTime);

			newSwimmingTimer = m_swimmingTimer - frameTime;
		}
		break;

	case eProxyInternalState_PartiallySubmerged:
		{
			UpdateInWater(player, frameTime);

			newSwimmingTimer = m_swimmingTimer - frameTime;
		}
		break;

	case eProxyInternalState_Swimming:
		{
			UpdateInWater(player, frameTime);

			newSwimmingTimer = m_swimmingTimer + frameTime;
		}
		break;
	}

	UpdateDrowningTimer(frameTime);

	m_swimmingTimer = clamp_tpl(newSwimmingTimer, -1000.0f, 1000.0f);
}

void CPlayerStateSwim_WaterTestProxy::UpdateDrowningTimer(const float frameTime)
{
	if (m_headUnderwater)
	{
		m_drowningTimer += frameTime;
	}
	else
	{
		m_drowningTimer = max(0.f, (m_drowningTimer - (frameTime * max(0.1f, g_pGameCVars->pl_drownRecoveryTime))));
	}

	m_oxygenLevel = (1.0f - clamp_tpl((m_drowningTimer / g_pGameCVars->pl_drownTime), 0.0f, 1.0f));
}

void CPlayerStateSwim_WaterTestProxy::ForceUpdateBottomLevel(const CPlayer& player)
{
	if( !IsWaitingForBottomLevelResults() )
	{
		RayTestBottomLevel( player, player.GetEntity()->GetWorldPos(), CPlayerStateSwim_WaterTestProxy::GetRayLength() );
	}
}

void CPlayerStateSwim_WaterTestProxy::PreUpdateNotSwimming(const CPlayer& player, const float frameTime)
{
	const float submergedThreshold = 0.25f;  
	m_lastInternalState = m_internalState;
	m_internalState = ((m_submergedFraction < submergedThreshold)) ? eProxyInternalState_OutOfWater : eProxyInternalState_PartiallySubmerged;
}

void CPlayerStateSwim_WaterTestProxy::PreUpdateSwimming(const CPlayer& player, const float frameTime)
{
	m_lastInternalState = m_internalState;
	m_internalState = eProxyInternalState_Swimming;
}

void CPlayerStateSwim_WaterTestProxy::UpdateOutOfWater(const CPlayer& player, const float frameTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	//Out of water, only query water level to figure out if player/ai is in contact with a water volume
	const Matrix34& playerWorldTM = player.GetEntity()->GetWorldTM();
	const Vec3 playerWorldPos = playerWorldTM.GetTranslation();

	const Vec3 localReferencePos = GetLocalReferencePosition(player);

	//Note: Try to tune value and set to higher value possible which works well
	const bool lastCheckFarAwayEnough = ((m_lastWaterLevelCheckPosition - playerWorldPos).len2() >= sqr(0.6f));

	float worldWaterLevel = m_waterLevel;
	if (lastCheckFarAwayEnough)
	{
		const Vec3 worldReferencePos = playerWorldPos + (Quat(playerWorldTM) * localReferencePos);
		IPhysicalEntity* piPhysEntity = player.GetEntity()->GetPhysics();

		UpdateWaterLevel( worldReferencePos, playerWorldPos, piPhysEntity );
	}

	//Update submerged fraction
	UpdateSubmergedFraction(localReferencePos.z, playerWorldPos.z, worldWaterLevel);
}

void CPlayerStateSwim_WaterTestProxy::UpdateInWater(const CPlayer& player, const float frameTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	const Matrix34& playerWorldTM = player.GetEntity()->GetWorldTM();
	const Vec3 playerWorldPos = playerWorldTM.GetTranslation();

	const Vec3 localReferencePos = GetLocalReferencePosition(player);
	const Vec3 worldReferencePos = playerWorldPos + (Quat(playerWorldTM) * localReferencePos);

	const bool shouldUpdate  = 
		((gEnv->pTimer->GetCurrTime() - m_timeWaterLevelLastUpdated) > 0.3f ) || 
		((m_lastWaterLevelCheckPosition - playerWorldPos).len2() >= sqr(0.35f)) ||
			(m_lastInternalState != m_internalState && m_internalState == eProxyInternalState_PartiallySubmerged); //Just entered partially emerged state

	if (shouldUpdate && !IsWaitingForBottomLevelResults())
	{
		RayTestBottomLevel(player, worldReferencePos, s_rayLength);

		IPhysicalEntity* piPhysEntity = player.GetEntity()->GetPhysics();

		UpdateWaterLevel( worldReferencePos, playerWorldPos, piPhysEntity );

		if (m_waterLevel > WATER_LEVEL_UNKNOWN)
		{
			m_playerWaterLevel = (worldReferencePos.z - m_waterLevel);
		}
		else
		{
			m_playerWaterLevel = -WATER_LEVEL_UNKNOWN;
			m_bottomLevel = BOTTOM_LEVEL_UNKNOWN;
			m_headUnderwater = false;
			m_headComingOutOfWater = false;
		}
	}

	m_relativeBottomLevel = (m_bottomLevel > BOTTOM_LEVEL_UNKNOWN) ? m_waterLevel - m_bottomLevel : 0.0f;

	const float localHeadZ = player.GetLocalEyePos().z + 0.2f;
	const float worldHeadZ = playerWorldPos.z + localHeadZ;

	const bool headWasUnderWater = m_headUnderwater;
	m_headUnderwater = ((worldHeadZ - m_waterLevel) < 0.0f);

	if (m_drowningTimer > g_pGameCVars->pl_drownTime)
	{
		UpdateDrowning(player);
	}

	m_headComingOutOfWater = headWasUnderWater && (!m_headUnderwater);

	if (m_internalState == eProxyInternalState_Swimming)
	{
		m_shouldSwim = ShouldSwim(max(localReferencePos.z, 1.3f));
	}
	else
	{
		UpdateSubmergedFraction(localReferencePos.z, playerWorldPos.z, m_waterLevel);

		m_shouldSwim = ShouldSwim(max(localReferencePos.z, 1.3f)) && (m_swimmingTimer < - 0.5f);
	}
#ifdef STATE_DEBUG
	DebugDraw(player, worldReferencePos);
#endif
}

void CPlayerStateSwim_WaterTestProxy::UpdateDrowning(const CPlayer& player)
{
	if (m_drowningTimer > (g_pGameCVars->pl_drownTime + g_pGameCVars->pl_drownDamageInterval))
	{
		const float damage = (player.GetMaxHealth() / 100) * g_pGameCVars->pl_drownDamage;
		if (g_pGame->GetGameRules() && damage > 0.f)
		{
			HitInfo drowningInfo(player.GetEntityId(), player.GetEntityId(), player.GetEntityId(), 
				damage, 0, 0, -1, CGameRules::EHitType::Punish, ZERO, ZERO, ZERO);

			g_pGame->GetGameRules()->ClientHit(drowningInfo);
		}

		player.IsDead() ? m_drowningTimer = 0.f : m_drowningTimer = g_pGameCVars->pl_drownTime;
	}
}

void CPlayerStateSwim_WaterTestProxy::UpdateSubmergedFraction(const float referenceHeight, const float playerHeight, const float waterLevel)
{
	const float referenceHeightFinal = max(referenceHeight, 1.3f);
	const float submergedTotal = playerHeight - waterLevel;
	const float submergedFraction = (float)__fsel(submergedTotal, 0.0f, clamp_tpl(-submergedTotal * __fres(referenceHeightFinal), 0.0f, 1.0f));

	SetSubmergedFraction(submergedFraction);
}

bool CPlayerStateSwim_WaterTestProxy::ShouldSwim(const float referenceHeight) const
{
	if (m_waterLevel > WATER_LEVEL_UNKNOWN)
	{
		const float bottomDepth = m_waterLevel - m_bottomLevel;

		if (m_internalState == eProxyInternalState_PartiallySubmerged)
		{
			return ((bottomDepth > referenceHeight)) 
				&& ((m_submergedFraction > 0.4f) || m_headUnderwater)
				&& ((!IsWaitingForBottomLevelResults() && m_bottomLevel != BOTTOM_LEVEL_UNKNOWN) || m_headUnderwater); //Without this check, whilst waiting for the initial ray cast result to find m_bottomLevel this function can incorrectly return true in shallow water
		}
		else
		{
			//In water already
			return (bottomDepth > referenceHeight);
		}
	}

	return false;
}

Vec3 CPlayerStateSwim_WaterTestProxy::GetLocalReferencePosition( const CPlayer& player )
{
	const float CAMERA_SURFACE_OFFSET = -0.2f;

	Vec3 localReferencePos = ZERO;
	if (!player.IsThirdPerson())
	{
		//--- We get a smoother experience in FP if we work relative to the camera
		localReferencePos = player.GetFPCameraPosition(false);
		localReferencePos.z += CAMERA_SURFACE_OFFSET;
	}
	else if (player.HasBoneID(BONE_SPINE3))
	{
		localReferencePos.x = 0.0f;
		localReferencePos.y = 0.0f;
		localReferencePos.z = player.GetBoneTransform(BONE_SPINE3).t.z;

#if !defined(_RELEASE)
		bool bLocalRefPosValid = localReferencePos.IsValid();
		CRY_ASSERT(bLocalRefPosValid);
		if (!bLocalRefPosValid)
		{
			localReferencePos = ZERO;
		}
#endif
	}

	return localReferencePos;
}

void CPlayerStateSwim_WaterTestProxy::OnRayCastBottomLevelDataReceived(const QueuedRayID& rayID, const RayCastResult& result)
{
	CRY_ASSERT(m_bottomLevelRayID == rayID);

	m_bottomLevelRayID = 0;

	if (result.hitCount > 0)
	{
		m_bottomLevel = m_lastRayCastResult = result.hits[0].pt.z;
	}
	else
	{
		m_bottomLevel = BOTTOM_LEVEL_UNKNOWN;
	}
}

///////////////////////////////////////////////////////////////////////////
void CPlayerStateSwim_WaterTestProxy::RayTestBottomLevel( const CPlayer& player, const Vec3& referencePosition, float maxRelevantDepth )
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	const float terrainWorldZ = gEnv->p3DEngine->GetTerrainElevation(referencePosition.x, referencePosition.y);

	int rayFlags = geom_colltype_player<<rwi_colltype_bit | rwi_stop_at_pierceable;
	int entityFlags = ent_terrain|ent_static|ent_sleeping_rigid|ent_rigid;
	const float padding = 0.2f;
	// NOTE: Terrain is above m_referencePos, so m_referencePos is probably inside a voxel or something.
	const float fPosWorldDiff = referencePosition.z - terrainWorldZ;
	float rayLength = (float)__fsel(fPosWorldDiff, min(maxRelevantDepth, fPosWorldDiff), maxRelevantDepth) + (padding * 2.0f);

	//We should not have entered this function if still waiting for the last result
	CRY_ASSERT(m_bottomLevelRayID == 0); 

	m_bottomLevelRayID = g_pGame->GetRayCaster().Queue(
		player.IsClient() ? RayCastRequest::HighPriority : RayCastRequest::MediumPriority,
		RayCastRequest(referencePosition + Vec3(0,0,padding), Vec3(0,0,-rayLength),
		entityFlags,
		rayFlags,
		0,
		0),
		functor(*this, &CPlayerStateSwim_WaterTestProxy::OnRayCastBottomLevelDataReceived));
}

void CPlayerStateSwim_WaterTestProxy::CancelPendingRays()
{
	if (m_bottomLevelRayID != 0)
	{
		g_pGame->GetRayCaster().Cancel(m_bottomLevelRayID);
	}
	m_bottomLevelRayID = 0;
}

void CPlayerStateSwim_WaterTestProxy::UpdateWaterLevel( const Vec3& worldReferencePos, const Vec3& playerWorldPos, IPhysicalEntity* piPhysEntity )
{
	m_waterLevel = gEnv->p3DEngine->GetWaterLevel(&worldReferencePos, piPhysEntity);

	m_timeWaterLevelLastUpdated = gEnv->pTimer->GetCurrTime();

	m_lastWaterLevelCheckPosition = playerWorldPos;
}

#ifdef STATE_DEBUG
void CPlayerStateSwim_WaterTestProxy::DebugDraw(const CPlayer& player, const Vec3& referencePosition)
{
	// DEBUG RENDERING
	const SPlayerStats& stats = *player.GetActorStats();
	const bool debugSwimming = (g_pGameCVars->cl_debugSwimming != 0);
	if (debugSwimming && (m_playerWaterLevel > -10.0f) && (m_playerWaterLevel < 10.0f))
	{
		const Vec3 surfacePosition(referencePosition.x, referencePosition.y, m_waterLevel);

		const Vec3 vRight(player.GetBaseQuat().GetColumn0());

		const static ColorF referenceColor(1,1,1,1);
		const static ColorF surfaceColor1(0,0.5f,1,1);
		const static ColorF surfaceColor0(0,0,0.5f,0);
		const static ColorF bottomColor(0,0.5f,0,1);

		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(referencePosition, 0.1f, referenceColor);

		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(referencePosition, surfaceColor1, surfacePosition, surfaceColor1, 2.0f);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(surfacePosition, 0.2f, surfaceColor1);
		IRenderAuxText::DrawLabelF(referencePosition + vRight * 0.5f, 1.5f, "WaterLevel %3.2f (Head underwater: %d)", m_playerWaterLevel, m_headUnderwater ? 1 : 0);
		IRenderAuxText::DrawLabelF(referencePosition + vRight * 0.5f - Vec3(0.f,0.f,0.2f), 1.5f, "OxygenLevel %1.2f", GetOxygenLevel());

		const static int lines = 16;
		const static float radius0 = 0.5f;
		const static float radius1 = 1.0f;
		const static float radius2 = 2.0f;
		for (int i = 0; i < lines; ++i)
		{
			float radians = ((float)i / (float)lines) * gf_PI2;
			Vec3 offset0(radius0 * cos_tpl(radians), radius0 * sin_tpl(radians), 0);
			Vec3 offset1(radius1 * cos_tpl(radians), radius1 * sin_tpl(radians), 0);
			Vec3 offset2(radius2 * cos_tpl(radians), radius2 * sin_tpl(radians), 0);
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(surfacePosition+offset0, surfaceColor0, surfacePosition+offset1, surfaceColor1, 2.0f);
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(surfacePosition+offset1, surfaceColor1, surfacePosition+offset2, surfaceColor0, 2.0f);
		}

		if (m_bottomLevel > 0.0f)
		{
			const Vec3 bottomPosition(referencePosition.x, referencePosition.y, m_bottomLevel);

			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(referencePosition, bottomColor, bottomPosition, bottomColor, 2.0f);
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(bottomPosition, 0.2f, bottomColor);
			IRenderAuxText::DrawLabelF(bottomPosition + Vec3(0,0,0.5f) - vRight * 0.5f, 1.5f, "BottomDepth %3.3f", m_waterLevel - m_bottomLevel);
		}
	}
}
#endif