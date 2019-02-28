// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProjectileAutoAimHelper.h"

#include "PlayerVisTable.h"
#include "Player.h"
#include "FireModePluginParams.h"
#include "FireMode.h"
#include "Weapon.h"
#include "Utility/CryWatch.h"

CRY_IMPLEMENT_GTI(CFireModePlugin_AutoAim, IFireModePlugin);

#ifndef _RELEASE
#	define DEBUG_DRAW
#endif


CFireModePlugin_AutoAim::CFireModePlugin_AutoAim()
	: m_bEnabled(true)
{

}

CFireModePlugin_AutoAim::~CFireModePlugin_AutoAim()
{

}

bool CFireModePlugin_AutoAim::Init( CFireMode* pFiremode, IFireModePluginParams* pPluginParams )
{
	SetEnabled(false);
	return Parent::Init(pFiremode, pPluginParams);
}

void CFireModePlugin_AutoAim::Activate( bool activate )
{
	SetEnabled( activate && m_pParams && (m_pParams->m_normalParams.m_enabled || m_pParams->m_zoomedParams.m_enabled) );
}

bool CFireModePlugin_AutoAim::Update( float frameTime, uint32 frameId )
{
#ifdef DEBUG_DRAW
	if(g_pGameCVars->pl_debug_projectileAimHelper)
	{
		// Force update of auto aim helper code through regular flow so we can see debug visuals
		Vec3 hit(ZERO);
		Vec3 pos = m_pOwnerFiremode->GetFiringPos(hit);
		Vec3 dir = m_pOwnerFiremode->GetFiringDir(hit, pos);		
		AlterFiringDirection(pos,dir);
	}
#endif

#if ALLOW_PROJECTILEHELPER_DEBUGGING
	if(g_pGameCVars->pl_watch_projectileAimHelper)
	{
		CryWatch("[Last Shot Auto Aimed]: %s", m_lastShotAutoAimedStatus.c_str());
	}
#endif // #if ALLOW_PROJECTILEHELPER_DEBUGGING

	return true;
}

void CFireModePlugin_AutoAim::AlterFiringDirection( const Vec3& firingPos, Vec3& rFiringDir ) const
{
#if ALLOW_PROJECTILEHELPER_DEBUGGING
	m_lastShotAutoAimedStatus.clear();
	m_lastTargetRejectionReason.clear(); 
#endif //#if ALLOW_PROJECTILEHELPER_DEBUGGING

	if( GetEnabled() )
	{
		if(CWeapon* pWeapon = m_pOwnerFiremode->GetWeapon())
		{
			if(EntityId ownerId = pWeapon->GetOwnerId())
			{
				AdjustFiringDirection(firingPos, rFiringDir, pWeapon->IsZoomed()||pWeapon->IsZoomingIn(), ownerId);
			}
		}
	}

#if ALLOW_PROJECTILEHELPER_DEBUGGING
	if(m_lastShotAutoAimedStatus.empty())
	{
		m_lastShotAutoAimedStatus.append("FALSE");
	}
#endif //#if ALLOW_PROJECTILEHELPER_DEBUGGING
}

//---------------------------------------------------------------------------
IEntity* CFireModePlugin_AutoAim::CalculateBestProjectileAutoAimTarget(const Vec3& attackerPos, const Vec3& attackerDir, const bool bCurrentlyZoomed, const EntityId ownerId) const
{

#if ALLOW_PROJECTILEHELPER_DEBUGGING
	static const ColorB red(127,0,0);
	static const ColorB green(0,127,0);
	static const ColorB brightGreen(0,255,0);
	static const float s_sphereDebugRad = 0.15f;
#endif


	IEntity* pBestTarget = NULL;
	float    fBestScore  = 0.0f;

	const TAutoaimTargets& players = g_pGame->GetAutoAimManager().GetAutoAimTargets();

	// Cache commonly required constants for scoring
	const ConeParams& aimConeSettings = GetAimConeSettings(bCurrentlyZoomed);

	const float minAutoAimDistSqrd   = sqr(aimConeSettings.m_minDistance);
	const float maxAutoAimDistSqrd   = sqr(aimConeSettings.m_maxDistance);
	const float  coneSizeRads		 = aimConeSettings.m_outerConeRads;
	IEntitySystem* pEntitySystem	 = gEnv->pEntitySystem;
	CPlayerVisTable* pVisTable		 = g_pGame->GetPlayerVisTable();

	const float distanceConstant	 = __fres(max(aimConeSettings.m_maxDistance, FLT_EPSILON));
	const float	coneAngleConstant	 = __fres(max(coneSizeRads*0.5f, FLT_EPSILON));

	// For each potential target we do a dist + cone check
	TAutoaimTargets::const_iterator endIter = players.end();
	for(TAutoaimTargets::const_iterator iter = players.begin(); iter != endIter; ++iter)
	{
		// If entity exists and we are allowed to target them
		EntityId targetEntityId = iter->entityId;
		IEntity* pEntity = pEntitySystem->GetEntity(targetEntityId);
		if(pEntity && AllowedToTargetPlayer(ownerId,targetEntityId))
		{
			// If further than allowed dist, discard
			Vec3 targetTestPos;

			// Test against primary Auto aim position
			const SAutoaimTarget* pAutoAimInfo = g_pGame->GetAutoAimManager().GetTargetInfo(targetEntityId);
			if(pAutoAimInfo)
			{
				targetTestPos = pAutoAimInfo->primaryAimPosition; 
			}
			else
			{
				// Then  ABBB centre as backup
				{
					AABB aabb;
					pEntity->GetPhysicsWorldBounds(aabb);
					targetTestPos = aabb.GetCenter();
				}
			}

			Vec3 toTarget = targetTestPos - attackerPos;
			float distSqrd = toTarget.GetLengthSquared();
			if( distSqrd >= minAutoAimDistSqrd &&
				distSqrd <= maxAutoAimDistSqrd )
			{

				// If not within cone.. discard
				float theta = 0.0f;
				if(TargetPositionWithinFrontalCone(attackerPos,targetTestPos, attackerDir,coneSizeRads, theta))
				{
					// If cant see them .. next
					if(!pVisTable->CanLocalPlayerSee(targetEntityId, 5))
					{

#if ALLOW_PROJECTILEHELPER_DEBUGGING
						m_lastTargetRejectionReason.append("VISION BLOCKED"); 	
#endif // #if ALLOW_PROJECTILEHELPER_DEBUGGING

						continue;
					}

					// For each candidate, generate their Auto Aim score.

					// 1) [0.0f,1.0f] score comprised of DISTANCE based score (closer is better)
					float targetDistScore    =  1.0f - ( sqrtf(distSqrd) *  distanceConstant );

					// Lets try squaring this to make candidates with only small gaps between them reflect distance scoring better and reduce the importance of distance at super long ranges
					targetDistScore *= targetDistScore;

					// 2) +  [0.0f,1.0f]  cone based score (central is better)
					const float targetConeScore		 =  1.0f - ( theta * coneAngleConstant );

					// Factor in designer controlled multipliers
					const float finalScore = (targetDistScore * g_pGameCVars->pl_pickAndThrow.chargedThrowAutoAimDistanceHeuristicWeighting) +  // TODO - move these weightings into params
						(targetConeScore * g_pGameCVars->pl_pickAndThrow.chargedThrowAutoAimAngleHeuristicWeighting);

					if(finalScore > fBestScore)
					{
						fBestScore  = finalScore;
						pBestTarget = pEntity;
					}

					// Debug rendering!
#if ALLOW_PROJECTILEHELPER_DEBUGGING
					if(g_pGameCVars->pl_debug_projectileAimHelper)
					{
						CryWatch("Entity [%s - %d] DistScore [%.2f] , ConeScore[%.2f], totalScore [%.3f]", pEntity->GetName(), pEntity->GetId(), targetDistScore, targetConeScore, finalScore);
						
						// Draw a green sphere to indicate valid
						gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(targetTestPos, s_sphereDebugRad,green);
					}
#endif //#if ALLOW_PROJECTILEHELPER_DEBUGGING

				}
#if ALLOW_PROJECTILEHELPER_DEBUGGING
				else
				{
					m_lastTargetRejectionReason.Format("OUTSIDE CONE [%.3f]",RAD2DEG(theta));

					if(g_pGameCVars->pl_debug_projectileAimHelper)
					{
						// Draw a red sphere to indicate not valid
						gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(targetTestPos, s_sphereDebugRad, red);
					}
				}
#endif //#if ALLOW_PROJECTILEHELPER_DEBUGGING
			}
#if ALLOW_PROJECTILEHELPER_DEBUGGING
			else
			{
				if(distSqrd >= minAutoAimDistSqrd)
				{
					m_lastTargetRejectionReason.Format("TOO CLOSE [%.3f]", sqrt_fast_tpl(distSqrd)); 
				}
				else
				{
					m_lastTargetRejectionReason.Format("TOO FAR [%.3f]",sqrt_fast_tpl(distSqrd));  
				}
			}
#endif //#if ALLOW_PROJECTILEHELPER_DEBUGGING
		}
	}

	// Draw a Really bright sphere on BEST target
#if ALLOW_PROJECTILEHELPER_DEBUGGING
	if(pBestTarget && g_pGameCVars->pl_debug_projectileAimHelper)
	{
		// If further than allowed dist, discard
		Vec3 targetTestPos = pBestTarget->GetPos();

		// We use aabb centre to reduce error
		{
			AABB aabb;
			pBestTarget->GetPhysicsWorldBounds(aabb);
			targetTestPos = aabb.GetCenter();
		}

		// Draw a bright green sphere to indicate target chosen
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(targetTestPos, s_sphereDebugRad*1.05f, brightGreen);
	}
#endif //#if ALLOW_PROJECTILEHELPER_DEBUGGING

	return pBestTarget;
}

bool CFireModePlugin_AutoAim::AllowedToTargetPlayer( const EntityId attackerId, const EntityId victimEntityId ) const
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	if(pGameRules && pGameRules->IsTeamGame())
	{
		const int clientTeamId = pGameRules->GetTeam(attackerId);
		const int victimTeamId = pGameRules->GetTeam(victimEntityId);

		return (clientTeamId != victimTeamId) || (g_pGameCVars->g_friendlyfireratio > 0.0f);
	}

	return true;
}

bool CFireModePlugin_AutoAim::TargetPositionWithinFrontalCone( const Vec3& attackerLocation,const Vec3& victimLocation,const Vec3& attackerFacingdir, const float targetConeRads, float& theta ) const
{
	const Vec3 vecAttackerToVictim	= victimLocation - attackerLocation;
	theta = acos(attackerFacingdir.dot(vecAttackerToVictim.GetNormalized()));

	return ( theta < (0.5f * targetConeRads) );
}

void CFireModePlugin_AutoAim::AdjustFiringDirection( const Vec3& attackerPos, Vec3& firingDirToAdjust, const bool bCurrentlyZoomed, const EntityId ownerId ) const
{
	CPlayer* pAttackingPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(ownerId));
	if (pAttackingPlayer && pAttackingPlayer->IsPlayer())
	{
		const ConeParams& aimConeSettings = GetAimConeSettings(bCurrentlyZoomed);

		// Don't do any projectile adjusting if the player already has a target for themselves, and is on target (e.g. manually scoping to get a headshot)	
		if( m_targetSelectionParams.m_bDisableAutoAimIfPlayerAlreadyHasTarget && pAttackingPlayer->GetCurrentTargetEntityId() ||
			!aimConeSettings.m_enabled)
		{

#if ALLOW_PROJECTILEHELPER_DEBUGGING
			m_lastShotAutoAimedStatus.append("FALSE - [Reason]: Player already on target");
#endif //#if #endif //#if ALLOW_PROJECTILEHELPER_DEBUGGING

			return;
		}

		float incomingDirLength = firingDirToAdjust.GetLength();
		CRY_ASSERT(incomingDirLength>0.f);
		Vec3 firingDirToAdjustNorm(firingDirToAdjust*__fres(incomingDirLength));

	#if ALLOW_PROJECTILEHELPER_DEBUGGING
		// DEBUG RENDER
		if (g_pGameCVars->pl_debug_projectileAimHelper)
		{
			// Draw Target acquisition cone
			float length = aimConeSettings.m_maxDistance;
			float radius = length * tan(aimConeSettings.m_outerConeRads * 0.5f);

			SAuxGeomRenderFlags originalFlags = gEnv->pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
			SAuxGeomRenderFlags newFlags = originalFlags;
			newFlags.SetCullMode(e_CullModeNone);
			newFlags.SetFillMode(e_FillModeWireframe);
			newFlags.SetAlphaBlendMode(e_AlphaBlended);

			gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(newFlags);
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawCone(attackerPos + (firingDirToAdjustNorm*aimConeSettings.m_maxDistance),-firingDirToAdjustNorm, radius , length , ColorB(132,190,255,120), true );

			// Draw projectile adjust cone
			radius = length * tan(aimConeSettings.m_innerConeRads * 0.5f);
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawCone(attackerPos + (firingDirToAdjustNorm*aimConeSettings.m_maxDistance),-firingDirToAdjustNorm, radius , length ,ColorB(0,0,127,120), true );
		
			// Restore render flags
			gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(originalFlags);
		}
	#endif //#if ALLOW_PROJECTILEHELPER_DEBUGGING

		IEntity* pTargetPlayer = CalculateBestProjectileAutoAimTarget(attackerPos, firingDirToAdjustNorm, bCurrentlyZoomed, ownerId);
		if(pTargetPlayer)
		{
			const SAutoaimTarget* pAutoAimInfo = g_pGame->GetAutoAimManager().GetTargetInfo(pTargetPlayer->GetId());
			if(pAutoAimInfo)
			{
				Vec3 desiredFiringDir = ( pAutoAimInfo->primaryAimPosition - attackerPos ).GetNormalized();

				// Make sure final firing dir still constrained to valid cone
				float vecDot = firingDirToAdjustNorm.Dot(desiredFiringDir);
				float maxConeAngle = cos(0.5f * aimConeSettings.m_innerConeRads);
				if(vecDot >= maxConeAngle)
				{
					// within cone
					firingDirToAdjustNorm = desiredFiringDir;

#if ALLOW_PROJECTILEHELPER_DEBUGGING
					m_lastShotAutoAimedStatus.append("TRUE + desired dir fully WITHIN allowed adjust cone");
#endif //#if ALLOW_PROJECTILEHELPER_DEBUGGING

				}
				else
				{
					// constrain (generally working with small angles, nLerp should be fine + cheap)
					const float invConeDot = 1.0f - maxConeAngle;
					const float invVecDot  = 1.0f - vecDot;
					float zeroToOne  = invConeDot / invVecDot;
					Vec3 finalVec	  = (zeroToOne * desiredFiringDir) + ((1.0f - zeroToOne) * firingDirToAdjustNorm);
					finalVec.Normalize();
					firingDirToAdjustNorm = finalVec;

#if ALLOW_PROJECTILEHELPER_DEBUGGING
					m_lastShotAutoAimedStatus.Format("TRUE + desired dir CONSTRAINED to allowed cone [desired]: %.3f deg [constrained To]: %.3f deg", RAD2DEG(acos(vecDot)), 0.5f * RAD2DEG(aimConeSettings.m_innerConeRads));
#endif //#if ALLOW_PROJECTILEHELPER_DEBUGGING
				}
				
			}
		}
#if ALLOW_PROJECTILEHELPER_DEBUGGING
		else
		{
			m_lastShotAutoAimedStatus.Format("FALSE - CalculateBestProjectileAutoAimTarget NULL [Last reason]: %s", m_lastTargetRejectionReason.c_str());
		}

		// Draw adjusted vec
		if (g_pGameCVars->pl_debug_projectileAimHelper)
		{
			float length = aimConeSettings.m_maxDistance;
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(attackerPos,ColorB(255,0,255,0),attackerPos + firingDirToAdjustNorm * length,ColorB(255,0,255,0));
		}
#endif //#if ALLOW_PROJECTILEHELPER_DEBUGGING

		firingDirToAdjust = firingDirToAdjustNorm*incomingDirLength;

	}
}




