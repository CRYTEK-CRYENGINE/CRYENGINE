// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 23:5:2006   9:27 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "ScriptBind_GameRules.h"
#include "GameRules.h"
#include "GameActions.h"
#include "Game.h"
#include "GameCVars.h"
#include "Actor.h"
#include "ActorImpulseHandler.h"
#include "Player.h"
#include "Turret/Turret/Turret.h"

#include "IVehicleSystem.h"
#include "IItemSystem.h"
#include <CryAction/IMaterialEffects.h>

#include "WeaponSystem.h"
#include "Audio/AudioSignalPlayer.h"
#include "IWorldQuery.h"

#include "Effects/GameEffects/ExplosionGameEffect.h"
#include "UI/UIManager.h"
#include "UI/HUD/HUDMissionObjectiveSystem.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "UI/WarningsManager.h"
#include "UI/HUD/HUDUtils.h"
#include "Utility/CryWatch.h"

#include <CryCore/StlUtils.h>
#include <CryMath/Cry_Math.h>
#include "GameRulesModules/IGameRulesPlayerSetupModule.h"
#include "GameRulesModules/IGameRulesDamageHandlingModule.h"
#include "GameRulesModules/IGameRulesSpawningModule.h"
#include "GameRulesModules/IGameRulesVictoryConditionsModule.h"
#include "GameRulesModules/IGameRulesSpectatorModule.h"
#include "GameRulesModules/IGameRulesAssistScoringModule.h"
#include "GameRulesModules/IGameRulesTeamChangedListener.h"
#include "GameRulesModules/IGameRulesModuleRMIListener.h"
#include "GameRulesModules/IGameRulesClientConnectionListener.h"
#include "GameRulesModules/IGameRulesPlayerStatsModule.h"
#include "GameRulesModules/IGameRulesStateModule.h"

#include "EquipmentLoadout.h"

#include "GameCodeCoverage/GameCodeCoverageTracker.h"

#include "Projectile.h"
#include "Melee.h"

#include "Battlechatter.h"
#include "PlayerProgression.h"
#include "HitDeathReactions.h"
#include "PersistantStats.h"
#include "AI/GameAISystem.h"

#include "UI/WarningsManager.h"
#include "LagOMeter.h"
#include "Network/Lobby/GameLobby.h"
#include "PlaylistManager.h"
#include "TacticalManager.h"

#include "PlayerMovementController.h"
#include "ClientHitEffectsMP.h"

#include "MPTrackViewManager.h"
#include "MPPathFollowingManager.h"
#include "VTOLVehicleManager/VTOLVehicleManager.h"
#include "PickAndThrowWeapon.h"

#include <CryRenderer/IRenderMesh.h>
#include "MovingPlatforms/MovingPlatformMgr.h"
#include "Utility/DesignerWarning.h"

static const int sSimulateExplosionMaxEntitiesToSkip = 20;
#include "SkillKill.h"
#include "EnvironmentalWeapon.h"

#include <IPerceptionManager.h>

//------------------------------------------------------------------------
// Our local client has hit something locally
void CGameRules::ClientHit(const HitInfo &hitInfo)
{
	// [*DavidR | 8/Dec/2009] WARNING: This function is called from a collision event in an ammo class
	// Unfortunately, collisions are not guaranteed to happen in both server and client side. So if we
	// are calling this ClientHit from the side that originated the hit we take it into account, other-
	// wise is dismissed (we don't "ServerProcess" the hit, but we still send this to the hit listeners)
	// [*DavidR | 1/Jun/2010] ToDo: This method would need a queue to handle concurrent calls like ServerHit does
	// (ProcessClientHit would be perfect name for this method in that pipeline)

	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	const IActor *pClientActor = g_pGame->GetIGameFramework()->GetClientActor();
	const IEntity *pTarget = m_pEntitySystem->GetEntity(hitInfo.targetId);
	const IEntity *pShooter =	m_pEntitySystem->GetEntity(hitInfo.shooterId);

	const IVehicle *pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(hitInfo.targetId);
	CActor *pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(hitInfo.targetId));


//CryLogAlways ("ClientHit: %s damaged %s", pShooter ? pShooter->GetName() : "NULL", pTarget ? pTarget->GetName() : "NULL");
//CryWatch3DAdd(GetHitType(hitInfo.type), hitInfo.pos, 2.f, NULL, -1.f);
	CPlayer *pPlayer = (pActor != NULL && pActor->IsPlayer()) ? static_cast<CPlayer*>(pActor) : NULL;
	
	if (pPlayer && pPlayer->ShouldFilterOutHit(hitInfo))
		return;

	bool isOtherHitable = false;
	if(pTarget)
	{
		const IEntityClass* pTargetClass = pTarget->GetClass();
		isOtherHitable = ShouldGiveLocalPlayerHitableFeedbackForEntityClass(pTargetClass);
	}

	float healthBefore = 0.0f;
	bool dead = false;
	RetrieveCurrentHealthAndDeathForTarget(pTarget, pActor, pVehicle, &healthBefore, &dead);

	SanityCheckHitInfo(hitInfo, "CGameRules::ClientHit");

	CEnvironmentalWeapon* pEnvWeap = static_cast<CEnvironmentalWeapon*>(g_pGame->GetIGameFramework()->QueryGameObjectExtension(hitInfo.targetId, "EnvironmentalWeapon"));

	EGameRulesTargetType targetType = EGRTT_Neutral;

	const bool displayHudFeedback = (!hitInfo.hitViaProxy && pClientActor != NULL && pClientActor->GetEntity() == pShooter && pTarget != NULL && (pVehicle != NULL || pActor != NULL || pEnvWeap != NULL || isOtherHitable) && !dead);
	if (displayHudFeedback)
	{	
		if ((!pVehicle && !pEnvWeap) || (!gEnv->bMultiplayer && isOtherHitable))
		{
			assert (pClientActor->IsPlayer());
			targetType = ((CPlayer*)(pClientActor))->IsFriendlyEntity(hitInfo.targetId) ? EGRTT_Friendly : EGRTT_Hostile;
		}
		else if(gEnv->bMultiplayer)
		{
			// Show hit indicators for shields if held by somebody
			if(pEnvWeap && pEnvWeap->ShouldShowShieldedHitIndicator() && pEnvWeap->GetOwner())
			{
				targetType = EGRTT_ShieldedEntity;
			}
			else
			{
				if(pVehicle)
				{
					if( IActor* pDriver = pVehicle->GetDriver() )
					{
						if(GetTeamCount() > 1)
						{
							int shooterTeamId = GetTeam( hitInfo.shooterId );
							int targetTeamId = GetTeam( pDriver->GetEntityId() );
							targetType = shooterTeamId == targetTeamId ? EGRTT_Friendly: EGRTT_NeutralVehicle;
						}
						else
						{
							targetType = EGRTT_NeutralVehicle;
						}
					}
					else
					{
						CVTOLVehicleManager *pVTOLVehicleManager = g_pGame->GetGameRules()->GetVTOLVehicleManager();
						CRY_ASSERT(pVTOLVehicleManager);
						if (pVTOLVehicleManager && pVTOLVehicleManager->IsVTOL(pVehicle->GetEntityId()))
						{
							targetType = EGRTT_NeutralVehicle;
						}
					}
				}
			}
		}

		if(pPlayer && gEnv->bMultiplayer && hitInfo.targetId != hitInfo.shooterId)
		{
			if (!g_pGameCVars->g_mpHeadshotsOnly || pPlayer->IsHeadShot(hitInfo))
			{
				m_pClientHitEffectsMP->Feedback(this, pPlayer, hitInfo);
			}
		}
	}

	if(pPlayer)
	{
		if(hitInfo.shooterId && !dead)
		{
			//Avoids shouting about taking fire or 'Contact!' if our own team are hitting us
			//	doesn't apply in modes with no teams anyway as we've got no-one to shout at.
			if(GetTeam(hitInfo.targetId) != GetTeam(hitInfo.shooterId))
			{
				if(CBattlechatter * pBattleChatter = GetBattlechatter())
				{
					pBattleChatter->StopAllBattleChatter(*pActor);

					if((healthBefore * 2.0f) < pActor->GetMaxHealth())
					{
						pBattleChatter->Event(BC_LowHealth, hitInfo.targetId);
					}
					else
					{
						pBattleChatter->Event(BC_TakingFire, hitInfo.targetId);
					}
				}				
			}
		}
	}

	IActor *pShooterActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(hitInfo.shooterId);
	
	if (!gEnv->bMultiplayer)
	{
		// We are using a separate ScriptTable from ServerHit here to avoid problems when ServerHits LUA processing cause 
		// ClientHits and we overwrite the contents of the hit info table on the LUA callstack in the CreateScriptHitInfo
		CreateScriptHitInfo(m_scriptClientHitInfo, hitInfo);
		CallScript(m_clientStateScript, "OnHit", m_scriptClientHitInfo);
	}

	bool backface = hitInfo.dir.Dot(hitInfo.normal)>0;
	if (!hitInfo.remote && hitInfo.targetId && !backface && !dead)
	{
		HitInfo hitToSend(hitInfo);

		if (hitToSend.projectileId)
		{
			uint16 projectileClass = ~uint16(0);
			CProjectile *pProjectile = g_pGame->GetWeaponSystem()->GetProjectile(hitToSend.projectileId);
			if (pProjectile)
			{
				g_pGame->GetIGameFramework()->GetNetworkSafeClassId(projectileClass, pProjectile->GetEntity()->GetClass()->GetName());
			}
			hitToSend.projectileClassId = projectileClass;
		}

		if(hitToSend.weaponId)
		{
			uint16 weaponClass = ~uint16(0);
			const IEntity* pWeaponEntity = gEnv->pEntitySystem->GetEntity(hitInfo.weaponId);
			if (pWeaponEntity)
			{
				g_pGame->GetIGameFramework()->GetNetworkSafeClassId(weaponClass, pWeaponEntity->GetClass()->GetName());
			}
			hitToSend.weaponClassId = weaponClass;
		}

		CPersistantStats::GetInstance()->ClientHit(hitToSend);

		if (!gEnv->bServer)
		{
			ProcessLocalHit(hitToSend);
#if USE_LAGOMETER
			CLagOMeter* pLagOMeter = g_pGame->GetLagOMeter();
			if (pLagOMeter)
			{
				pLagOMeter->OnClientRequestHit(hitToSend);
			}
#endif
			GetGameObject()->InvokeRMI(SvRequestHit(), hitToSend, eRMI_ToServer);

			if (gEnv->bMultiplayer && g_pGameCVars->g_useNetSyncToSpeedUpRMIs)
			{
				gEnv->pNetwork->SyncWithGame(eNGS_ForceChannelTick);
			}
		}
		else
		{
			ServerHit(hitToSend);
		}
	}
	else if (dead && !gEnv->bMultiplayer && pActor)
	{
		//Allow to destroy body parts after death
		pActor->ProcessDestructiblesHit(hitInfo, 0.0f, 0.0f);
	}

	if (hitInfo.targetId && !backface && !dead)
	{
		if (pShooterActor && pShooterActor->IsPlayer() && pTarget && !hitInfo.remote)
		{
			IGameRulesAssistScoringModule *assistScoringModule = GetAssistScoringModule();
			if (assistScoringModule)
			{
				assistScoringModule->OnEntityHit(hitInfo);
			}
		}
	}

	if (displayHudFeedback)
	{
		if (!gEnv->bMultiplayer)
		{
			// AI in single player can manipulate the damage they took via scripting
			// or even choose to ignore it completely. So the only way to see if we
			// did damage is to compare the before and after health.
			float healthAfter = 0.0f;
			RetrieveCurrentHealthAndDeathForTarget(pTarget, pActor, pVehicle, &healthAfter, NULL);

			const float actualDamageInflicted = healthBefore - healthAfter;
			if (ShouldGiveLocalPlayerHitFeedback(CGameRules::eLocalPlayerHitFeedbackChannel_HUD, actualDamageInflicted))
			{
				SHUDEventWrapper::HitTarget( targetType, static_cast<int>(eHUDEventHT_Bullet), hitInfo.targetId );
			}
		}
		else
		{
			if (!g_pGameCVars->g_mpHeadshotsOnly || !pPlayer || pPlayer->IsHeadShot(hitInfo) )
			{		
				SHUDEventWrapper::HitTarget( targetType, static_cast<int>(eHUDEventHT_Bullet), hitInfo.targetId );
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::KnockActorDown( EntityId actorEntityId )
{
	// Forbid fall and play if the actor is playing a hit/death reaction
	CActor* pHitActor = static_cast<CActor*>(gEnv->pGameFramework->GetIActorSystem()->GetActor( actorEntityId ));
	if (pHitActor)
	{
		// Don't trigger Fall and Play if the actor is playing a hit reaction
		CPlayer* pHitPlayer = static_cast<CPlayer*>(pHitActor);
		CHitDeathReactionsConstPtr pHitDeathReactions = pHitPlayer->GetHitDeathReactions();
		if (!pHitDeathReactions || !pHitDeathReactions->IsInReaction() && pHitPlayer->CanFall())
			pHitActor->Fall();
	}
}


//------------------------------------------------------------------------
// Server controls everything, whether we are a client & server and have hit locally or whether a remote client has 
// told us they've hit someone. Here, we now handle the hit with authority on the server.
void CGameRules::ServerHit(const HitInfo& hitInfo)
{
	// This may be called from other places (Vehicle code) and not directly from ClientHit or RMI.
	//	In such cases, we go through the same process of queuing up the hit, minus any specific handling
	//	cases as defined from Client Hit. This shouldn't be virtual and outside Gamerules should have
	//	a more generic way of requesting hits! (Kevin)
	if (m_processingHit)
	{
		m_queuedHits.push(hitInfo);
		return;
	}

	++m_processingHit;

	ProcessServerHit(hitInfo);

	while (!m_queuedHits.empty())
	{
		const HitInfo& info(m_queuedHits.front());
		ProcessServerHit(info);
		m_queuedHits.pop();
	}

	--m_processingHit;
}

//------------------------------------------------------------------------
// Actually process the server hits. Some hits can be queued and processed later
void CGameRules::ProcessServerHit(const HitInfo& hitInfo)
{
	bool bHandleRequest = true;

	IActor *pTarget = GetActorByEntityId(hitInfo.targetId);

	if (pTarget && pTarget->GetSpectatorMode())
	{
		bHandleRequest = false;
	}

	if (!gEnv->bMultiplayer)
	{
		const bool boolShooterIsClient = g_pGame->GetIGameFramework()->GetClientActorId() == hitInfo.shooterId;
		if (boolShooterIsClient && pTarget)
		{
			const SAutoaimTarget* pTargetInfo = g_pGame->GetAutoAimManager().GetTargetInfo(hitInfo.targetId);
			if (pTargetInfo != NULL && !pTargetInfo->HasFlagSet(eAATF_AIHostile))
				bHandleRequest = false;
		}
	}
	else if(pTarget && hitInfo.type == CGameRules::EHitType::Melee && g_pGameCVars->pl_melee.mp_knockback_enabled && pTarget->IsPlayer())
	{
		//Tell everyone to apply an impulse with the weapon's strength
		CItem* pItem = static_cast<CItem*>(g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(hitInfo.weaponId));
		if(pItem)
		{
			if( CMelee* pMelee = static_cast<CWeapon*>(pItem->GetIWeapon())->GetMelee() )
			{
				float strength = pMelee->GetImpulseStrength();
				static_cast<CPlayer*>(pTarget)->ApplyMeleeImpulse(hitInfo.dir, strength);
				pTarget->GetGameObject()->InvokeRMIWithDependentObject(CPlayer::ClApplyMeleeImpulse(), CPlayer::SPlayerMeleeImpulseParams(hitInfo.dir, strength), eRMI_ToRemoteClients, hitInfo.targetId);
			}
		}
	}

	if (bHandleRequest)
	{
		float fTargetHealthBeforeHit = 0.0f;
		if(pTarget)
		{
			static_cast<CActor*>(pTarget)->GetDamageEffectController().OnHit(&hitInfo);

			fTargetHealthBeforeHit = pTarget->GetHealth();
		}

		bool bActorKilled = false;
		IGameRulesDamageHandlingModule * pDamageHandler = GetDamageHandlingModule();
		CRY_ASSERT_MESSAGE(pDamageHandler, "No Damage handling module found!");
		if (pDamageHandler)
		{
			bActorKilled = pDamageHandler->SvOnHit(hitInfo);
		}

		if (!bActorKilled)
		{
			const float fCausedDamage = pTarget ? (fTargetHealthBeforeHit - pTarget->GetHealth()) : 0.0f;
			ProcessLocalHit(hitInfo, fCausedDamage);
		}

		// call hit listeners if any
		if (m_hitListeners.empty() == false)
		{
			THitListenerVec::iterator iter = m_hitListeners.begin();
			THitListenerVec::iterator end = m_hitListeners.end();
			for(; iter != end; ++iter)
				(*iter)->OnHit(hitInfo);
		}

		if(bActorKilled && pTarget)
		{
			PostHitKillCleanup(pTarget);
		}

		IActor *pShooter = GetActorByEntityId(hitInfo.shooterId);
		if (pShooter)
		{
			if (hitInfo.shooterId!=hitInfo.targetId && hitInfo.weaponId!=hitInfo.shooterId && hitInfo.weaponId!=hitInfo.targetId && hitInfo.damage>=0)
			{
				EntityId params[2];
				params[0] = hitInfo.weaponId;
				params[1] = hitInfo.targetId;
				m_pGameplayRecorder->Event(pShooter->GetEntity(), GameplayEvent(eGE_WeaponHit, 0, 0, (void *)params));
			}

			m_pGameplayRecorder->Event(pShooter->GetEntity(), GameplayEvent(eGE_Hit, 0, 0, (void *)(EXPAND_PTR)hitInfo.weaponId));
			m_pGameplayRecorder->Event(pShooter->GetEntity(), GameplayEvent(eGE_Damage, 0, hitInfo.damage, (void *)(EXPAND_PTR)hitInfo.weaponId));
		}
			
		// knockdown is delayed because when the actor is ragdolized, all pending physic events on that actor are flushed. If the hit was part of a barrage, the other hits would be lost
		if (hitInfo.knocksDown &&  pTarget && !pTarget->IsDead())
			m_pendingActorsToBeKnockedDown.push_back( hitInfo.targetId ); 
	}
}

//------------------------------------------------------------------------
void CGameRules::ProcessLocalHit(const HitInfo& hitInfo, float fCausedDamage/* = 0.0f*/)
{
	// [*DavidR | 8/Dec/2009]: Place the code that should be invoked in both server and client sides here
	IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(hitInfo.targetId);
	if (pActor != NULL && (pActor->GetActorClass() == CPlayer::GetActorClassType()))
	{

		// Clients sometimes want to force a hit death reaction to play (when the victim isnt actually dead.. but will be when the server responds to a hit req)
		CHitDeathReactionsPtr pHitDeathReactions = static_cast<CPlayer*>(pActor)->GetHitDeathReactions();
		if (pHitDeathReactions)
		{
			// If the user has requested the player be force killed, then we need to *react* like this was a kill
			bool bReturn = false;
			if(hitInfo.forceLocalKill)
			{
				// Force the hit death reaction to react as if was a kill 
				CActor::KillParams params;
				params.shooterId					= hitInfo.shooterId;
				params.targetId						= hitInfo.targetId;
				params.weaponId						= hitInfo.weaponId;
				params.projectileId				= hitInfo.projectileId;
				params.itemIdToDrop				= -1;
				params.weaponClassId			= hitInfo.weaponClassId;
				params.damage							= hitInfo.damage;
				params.material						= -1;
				params.hit_type						= hitInfo.type;
				params.hit_joint					= hitInfo.partId;
				params.projectileClassId	= hitInfo.projectileClassId;
				params.penetration				= hitInfo.penetrationCount;
				params.firstKill					= false;
				params.killViaProxy				= hitInfo.hitViaProxy;
				params.impulseScale				= hitInfo.impulseScale; 
				params.forceLocalKill			= hitInfo.forceLocalKill; 

				bReturn = pHitDeathReactions->OnKill(params);
			}
			else
			{
				// Proceed as normal
				bReturn = pHitDeathReactions->OnHit(hitInfo, fCausedDamage);
			}

			if(bReturn)
				return;
		}
	}

	// If HitDeathReactions hasn't processed this local hit, add a physic impulse
	AddLocalHitImpulse(hitInfo);
}

//------------------------------------------------------------------------
void CGameRules::AddLocalHitImpulse(const HitInfo& hitInfo)
{
	// play an impulse using hit data (similar to old code on SinglePlayer.lua:ProcessActorDamage)
	CActor *pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(hitInfo.targetId));

	bool isValidEntity = pActor != NULL && (gEnv->bMultiplayer ? g_pGameCVars->pl_doLocalHitImpulsesMP != 0 : !pActor->IsPlayer());
	if (isValidEntity && (hitInfo.type != CGameRules::EHitType::Melee) && (hitInfo.type != CGameRules::EHitType::Mike_Burn))
	{
		if (gEnv->bMultiplayer)
		{
			// Check for friendly fire
			if ((hitInfo.targetId != hitInfo.shooterId) && (GetTeamCount() > 1) && (GetFriendlyFireRatio() <= 0.0f))
			{
				int shooterTeamId = GetTeam(hitInfo.shooterId);
				int targetTeamId = GetTeam(hitInfo.targetId);

				if (shooterTeamId && (shooterTeamId == targetTeamId))
					return;
			}
		}

		pActor->AddLocalHitImpulse(SHitImpulse(hitInfo.partId, hitInfo.material, hitInfo.pos, hitInfo.dir * min(200.0f, hitInfo.damage), 0.0f, hitInfo.projectileClassId));
	}
}

//------------------------------------------------------------------------
int CGameRules::GetFreeExplosionIndex()
{
	int index = -1;
	for(int i = 0; i < MAX_CONCURRENT_EXPLOSIONS; i++)
	{
		if(!m_explosionValidities[i])
		{
			index = i;
			break;
		}
	}

	return index;
}

//------------------------------------------------------------------------

void CGameRules::QueueExplosion(const ExplosionInfo &explosionInfo)
{
	CRY_ASSERT_TRACE (explosionInfo.type, ("Queueing an explosion with an invalid hit type! (effect='%s'/'%s')", explosionInfo.effect_name.c_str(), explosionInfo.effect_class.c_str()));

	int index = GetFreeExplosionIndex();

	if(index != -1)
	{
		m_explosions[index].m_explosionInfo = explosionInfo;
		m_explosionValidities[index] = true;
		m_explosions[index].m_mfxInfo.Reset();

		m_queuedExplosions.push(&m_explosions[index]);
	}
	else
	{
		GameWarning("Out of explosion indicies to use!");
	}
}

//------------------------------------------------------------------------
void CGameRules::ProcessServerExplosion(SExplosionContainer &explosionInfo)
{
	if (!gEnv->bServer )
	{
		assert(0);
		return;
	}

	if(explosionInfo.m_explosionInfo.propogate)
	{
		GetGameObject()->InvokeRMI(ClExplosion(), explosionInfo.m_explosionInfo, eRMI_ToRemoteClients);
	}
  
	ClientExplosion(explosionInfo);  
}

//------------------------------------------------------------------------
void CGameRules::ProcessQueuedExplosions()
{
  const static uint8 nMaxExp = 1;

	if(gEnv->bServer)
	{
		for (uint8 exp=0; !m_queuedExplosions.empty() && exp<nMaxExp; ++exp)
		{ 
			SExplosionContainer& info = *m_queuedExplosions.front();
			ProcessServerExplosion(info);

			if(info.m_mfxInfo.m_state == eDeferredMfxExplosionState_Dispatched)
			{
				m_queuedExplosionsAwaitingRaycasts.push(&info);
			}
			else
			{
				ClearExplosion(&info);
			}

			m_queuedExplosions.pop();
		}
	}
	else
	{
		for (uint8 exp=0; !m_queuedExplosions.empty() && exp<nMaxExp; ++exp)
		{ 
			SExplosionContainer& info = *m_queuedExplosions.front();
			ClientExplosion(info);

			if(info.m_mfxInfo.m_state == eDeferredMfxExplosionState_Dispatched)
			{
				m_queuedExplosionsAwaitingRaycasts.push(&info);
			}
			else
			{
				ClearExplosion(&info);
			}

			m_queuedExplosions.pop();
		}
	}

	ProcessDeferredMaterialEffects();
}

//------------------------------------------------------------------------
void CGameRules::ProcessDeferredMaterialEffects()
{
	if(m_pExplosionGameEffect)
	{
		const uint32 numExplosionsAwaitingLinetests = m_queuedExplosionsAwaitingRaycasts.size();
		for (uint32 i = 0; i < numExplosionsAwaitingLinetests; i++)
		{
			SExplosionContainer * pInfo = m_queuedExplosionsAwaitingRaycasts.front();
			if(pInfo->m_mfxInfo.m_state >= eDeferredMfxExplosionState_ResultImpact)
			{
				SExplosionContainer& info = *pInfo;
				m_pExplosionGameEffect->SpawnMaterialEffect(info);
				m_queuedExplosionsAwaitingRaycasts.pop();
				
				ClearExplosion(pInfo);
			}
		}
	}
}

//------------------------------------------------------------------------
ILINE void CGameRules::ClearExplosion(SExplosionContainer * pExplosionInfo)
{
	int index = (int)(pExplosionInfo - m_explosions);
	m_explosionValidities[index]	= false;
	m_explosions[index].m_mfxInfo.Reset();
}

//------------------------------------------------------------------------
void CGameRules::CullEntitiesInExplosion(const ExplosionInfo &explosionInfo)
{
	if (!g_pGameCVars->g_ec_enable || explosionInfo.damage <= 0.1f)
		return;

	IPhysicalEntity **pents;
	float radiusScale = g_pGameCVars->g_ec_radiusScale;
	float minVolume = g_pGameCVars->g_ec_volume;
	float minExtent = g_pGameCVars->g_ec_extent;
	int   removeThreshold = max(1, g_pGameCVars->g_ec_removeThreshold);

	IActor *pClientActor = g_pGame->GetIGameFramework()->GetClientActor();

	Vec3 radiusVec(radiusScale * explosionInfo.physRadius);
	int i = gEnv->pPhysicalWorld->GetEntitiesInBox(explosionInfo.pos-radiusVec,explosionInfo.pos+radiusVec,pents, ent_rigid|ent_sleeping_rigid);
	int removedCount = 0;

	static IEntityClass* s_pInteractiveEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("InteractiveEntity");
	static IEntityClass* s_pDeadBodyClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("DeadBody");
	static IEntityClass* s_pConstraintClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Constraint");

	if (i > removeThreshold)
	{
		int entitiesToRemove = i - removeThreshold;
		for(--i;i>=0;i--)
		{
			if(removedCount>=entitiesToRemove)
				break;

			IEntity * pEntity = (IEntity*) pents[i]->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
			if (pEntity)
			{
				// don't remove items/pickups
				if (g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pEntity->GetId()))
				{
					continue;
				}
				// don't remove enemies/ragdolls
				if (g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId()))
				{
					continue;
				}

				// if there is a flowgraph attached, never remove!
				if (pEntity->GetProxy(ENTITY_PROXY_FLOWGRAPH) != 0)
					continue;

				IEntityClass* pClass = pEntity->GetClass();
				if (pClass == s_pInteractiveEntityClass || pClass == s_pDeadBodyClass || pClass == s_pConstraintClass)
					continue;

				// get bounding box
				{
					AABB aabb;
					pEntity->GetPhysicsWorldBounds(aabb);

					// don't remove objects which are larger than a predefined minimum volume
					if (aabb.GetVolume() > minVolume)
						continue;

					// don't remove objects which are larger than a predefined minimum volume
					Vec3 size(aabb.GetSize().abs());
					if (size.x > minExtent || size.y > minExtent || size.z > minExtent)
						continue;
				}

				IScriptTable* entityScript = pEntity->GetScriptTable();
				SmartScriptTable properties;
				if (entityScript != NULL && entityScript->GetValue("Properties", properties))
				{
					bool isMissionCritical = false;
					if (properties->GetValue("bMissionCritical", isMissionCritical) && isMissionCritical)
						continue;
				}

				// marcok: somehow editor doesn't handle deleting non-dynamic entities very well
				// but craig says, hiding is not synchronized for DX10 breakable MP, so we remove entities only when playing pure game
				// alexl: in SinglePlayer, we also currently only hide the object because it could be part of flowgraph logic
				//        which would break if Entity was removed and could not propagate events anymore
				if (gEnv->bMultiplayer == false || gEnv->IsEditor())
				{
					pEntity->Hide(true);
				}
				else
				{
					//THIS WAS CAUSING CRASHING IN MP DUE TO REMOVAL OF ENTITIES THAT THE PHYSICS WAS HOLDING POINTERS TO
					gEnv->pEntitySystem->RemoveEntity(pEntity->GetId());
				}
				removedCount++;
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::ClientExplosion(SExplosionContainer &explosionContainer)
{
	ExplosionInfo& explosionInfo = explosionContainer.m_explosionInfo;
	TExplosionAffectedEntities affectedEntities;
		
	CalculateExplosionAffectedEntities(explosionInfo, affectedEntities);

	if (gEnv->bServer)
	{

		IGameRulesDamageHandlingModule* pDamageHandling = GetDamageHandlingModule();
		CRY_ASSERT_MESSAGE(pDamageHandling, "No Damage handling module found!");
		if (pDamageHandling)
		{
			pDamageHandling->SvOnExplosion(explosionInfo, affectedEntities);
		}

		// call hit listeners if any
		if (m_hitListeners.empty() == false)
		{
			THitListenerVec::iterator iter = m_hitListeners.begin();
			while (iter != m_hitListeners.end())
			{
				(*iter)->OnServerExplosion(explosionInfo);
				++iter;
			}
		}
  }

	if (gEnv->IsClient())
	{
		// call hit listeners if any
		if (m_hitListeners.empty() == false)
		{
			THitListenerVec::iterator iter = m_hitListeners.begin();
			while (iter != m_hitListeners.end())
			{
				(*iter)->OnExplosion(explosionInfo);
				++iter;
			}
		}

		IActor *pClientActor = g_pGame->GetIGameFramework()->GetClientActor();
		if (pClientActor)
		{
			CRY_ASSERT (pClientActor->IsPlayer());
			CPlayer* pPlayer = static_cast<CPlayer*>(pClientActor);

			//const EntityId clientId = pClientActor->GetEntityId();
			const Vec3 playerPos = pClientActor->GetEntity()->GetWorldPos();

			const float distSq = (playerPos - explosionInfo.pos).len2();

			//if(distSq < explosionInfo.radius * explosionInfo.radius)
			//{
			//	// this will work as long as the values defined for this mood in the .xml are 0 for fadein, and some value >0 for the fadeout
			//	m_explosionSoundmoodEnter.Play(clientId);
			//	m_explosionSoundmoodExit.Play(clientId);
			//}

			//if(gEnv->bMultiplayer && (explosionInfo.type == CGameRules::EHitType::Frag || explosionInfo.type == CGameRules::EHitType::Explosion))
			//{
			//	m_explosionFeedback.Play(pPlayer->GetEntityId(), "range", distSq);
			//}
		}
	}

	if(m_pExplosionGameEffect)
	{
		m_pExplosionGameEffect->Explode(explosionContainer);
	}

	IPerceptionManager* pPerceptionManager = IPerceptionManager::GetInstance();
	if (pPerceptionManager && !gEnv->bMultiplayer)
	{
		if (explosionInfo.damage > 0.0f)
		{
			// Associate event with vehicle if the shooter is in a vehicle (tank cannon shot, etc)
			EntityId sourceId = (explosionInfo.shooterId == LOCAL_PLAYER_ENTITY_ID) ? 0 : explosionInfo.shooterId;
			IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(explosionInfo.shooterId);
			if (pActor != NULL && pActor->GetLinkedVehicle() && pActor->GetLinkedVehicle()->GetEntityId())
				sourceId = pActor->GetLinkedVehicle()->GetEntityId();

			SAIStimulus stim(AISTIM_EXPLOSION, 0, sourceId, 0,
				explosionInfo.pos, ZERO, explosionInfo.radius);
			pPerceptionManager->RegisterStimulus(stim);

			float fSoundRadius = explosionInfo.soundRadius;
			if (fSoundRadius <= FLT_EPSILON)
			{
				fSoundRadius = explosionInfo.radius * 3.0f;
			}

			SAIStimulus stimSound(AISTIM_SOUND, AISOUND_EXPLOSION, sourceId, 0,
				explosionInfo.pos, ZERO, fSoundRadius, AISTIMPROC_FILTER_LINK_WITH_PREVIOUS);
			pPerceptionManager->RegisterStimulus(stimSound);
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::ProjectileExplosion(const SProjectileExplosionParams &projectileExplosionInfo)
{
	char projectileNetClassName[256];
	if(g_pGame->GetIGameFramework()->GetNetworkSafeClassName(projectileNetClassName, sizeof(projectileNetClassName), projectileExplosionInfo.m_projectileClass))
	{
		IEntityClass *pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(projectileNetClassName);
		const SAmmoParams *pAmmoParams = pClass ? g_pGame->GetWeaponSystem()->GetAmmoParams(pClass) : NULL;
		const SExplosionParams *pExplosionParams = pAmmoParams ? pAmmoParams->pExplosion : NULL;

		if(pExplosionParams)
		{
			float minRadius = pExplosionParams->minRadius;
			float maxRadius = pExplosionParams->maxRadius;
			
			if (pAmmoParams->pFlashbang)
			{
				CCCPOINT(Projectile_FlashbangExplode);
				minRadius = pAmmoParams->pFlashbang->maxRadius;
				maxRadius = pAmmoParams->pFlashbang->maxRadius;
			}
			else
			{
				CCCPOINT(Projectile_Explode);
			}

			CRY_ASSERT_MESSAGE(pExplosionParams->hitTypeId, string().Format("Invalid hit type '%s' in explosion params for '%s'", pExplosionParams->type.c_str(), projectileNetClassName));
			CRY_ASSERT_MESSAGE(pExplosionParams->hitTypeId == GetHitTypeId(pExplosionParams->type.c_str()), "Sanity Check Failed: Stored explosion type id does not match the type string, possibly PreCacheLevelResources wasn't called on this ammo type");

			ExplosionInfo explosionInfo(
					projectileExplosionInfo.m_shooterId, 
					projectileExplosionInfo.m_weaponId, 
					projectileExplosionInfo.m_projectileId, 
					projectileExplosionInfo.m_damage, 
					projectileExplosionInfo.m_pos, 
					projectileExplosionInfo.m_dir, 
					minRadius, 
					maxRadius, 
					pExplosionParams->minPhysRadius, 
					pExplosionParams->maxPhysRadius, 
					0.0f, 
					pExplosionParams->pressure, 
					pExplosionParams->holeSize, 
					pExplosionParams->hitTypeId);

			explosionInfo.projectileClassId = projectileExplosionInfo.m_projectileClass;

			if(pAmmoParams->pFlashbang)
			{
				float blindAmountMult = 1.f;
				float blindLengthMult = 1.f;

				if( CPlayer* pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetClientActor()) )
				{
					const CPlayerModifiableValues& modifiableValues = pPlayer->GetModifiableValues();
					blindAmountMult = modifiableValues.GetValue(kPMV_FlashBangBlindAmountMultiplier);
					blindLengthMult = modifiableValues.GetValue(kPMV_FlashBangBlindLengthMultiplier);
				}

				explosionInfo.SetEffect(pExplosionParams->effectName.c_str(), pExplosionParams->effectScale, pExplosionParams->maxblurdist, pAmmoParams->pFlashbang->blindAmount * blindAmountMult, pAmmoParams->pFlashbang->flashbangBaseTime * blindLengthMult);
			}
			else
				explosionInfo.SetEffect(pExplosionParams->effectName.c_str(), pExplosionParams->effectScale, pExplosionParams->maxblurdist);
			if (projectileExplosionInfo.m_overrideEffectClassName.empty())
				explosionInfo.SetEffectClass(pAmmoParams->pEntityClass->GetName());
			else
				explosionInfo.SetEffectClass(projectileExplosionInfo.m_overrideEffectClassName.c_str());

			if (projectileExplosionInfo.m_impact)
				explosionInfo.SetImpact(projectileExplosionInfo.m_impactDir, projectileExplosionInfo.m_impactVel, projectileExplosionInfo.m_impactId);

			explosionInfo.SetFriendlyFire(pExplosionParams->friendlyFire);
			explosionInfo.soundRadius = pExplosionParams->soundRadius;

			explosionInfo.propogate = false;
			explosionInfo.explosionViaProxy = projectileExplosionInfo.m_isProxyExplosion;

			QueueExplosion(explosionInfo);

			/*CryLogAlways("CGameRules::ExplosionInfo %d, %d, %d, %f, %f %f %f, %f %f %f, %f, %f, %f, %f, %f, %f, %f, %d", 
					explosionInfo.shooterId, explosionInfo.weaponId, explosionInfo.projectileId,
					explosionInfo.damage, explosionInfo.pos.x, explosionInfo.pos.y, explosionInfo.pos.z, 
					explosionInfo.dir.x, explosionInfo.dir.y, explosionInfo.dir.z, explosionInfo.minRadius, explosionInfo.radius,
					explosionInfo.minPhysRadius, explosionInfo.physRadius, explosionInfo.angle, explosionInfo.pressure,
					explosionInfo.hole_size, explosionInfo.type);*/

			if(gEnv->bServer)
			{
				if(!projectileExplosionInfo.m_impact)
					GetGameObject()->InvokeRMI(ClProjectileExplosion(), projectileExplosionInfo, eRMI_ToRemoteClients);
				else
				{
					SProjectileExplosionParams_Impact projectileExplosionInfo_Impact(projectileExplosionInfo);
					GetGameObject()->InvokeRMI(ClProjectileExplosion_Impact(), projectileExplosionInfo_Impact, eRMI_ToRemoteClients);
				}
			}
		}
	}
}

//-------------------------------------------
void CGameRules::CalculateExplosionAffectedEntities(const ExplosionInfo &explosionInfo, TExplosionAffectedEntities& affectedEntities )
{
	// Simulations are now processed on both clients and server.
	// However, damage etc is only done on the server, whereas
	// only the explosion physic is simulated on the clients

	if (!gEnv->bMultiplayer && gEnv->bServer)
		CullEntitiesInExplosion(explosionInfo);

	pe_explosion explosion;
	explosion.epicenter = explosionInfo.pos;
	explosion.rmin = explosionInfo.minRadius;
	explosion.rmax = explosionInfo.radius;
	explosion.rmax = max(explosion.rmax, 0.0001f);
	explosion.r = explosion.rmin;
	explosion.impulsivePressureAtR = explosionInfo.pressure;
	explosion.epicenterImp = explosionInfo.pos;
	explosion.explDir = explosionInfo.dir;
	explosion.nGrow = 0;	// This is not a good idea when using a low res occlusion map!
	explosion.rminOcc = 0.07f;

	int skipCount = 0;
	IPhysicalEntity* pSkipList[sSimulateExplosionMaxEntitiesToSkip];
	GetEntitiesToSkipByExplosion(explosionInfo, pSkipList, skipCount);

	// we separate the calls to SimulateExplosion so that we can define different radii for AI and physics bodies
	explosion.holeSize = 0.0f;
	explosion.nOccRes = explosion.rmax>50.0f ? 0:16;

	gEnv->pPhysicalWorld->SimulateExplosion( &explosion, pSkipList, skipCount, explosionInfo.firstPassPhysicsEntities);

	if (gEnv->bServer)
	{
		UpdateAffectedEntitiesSet(affectedEntities, explosion);

		// check vehicles
		IVehicleSystem *pVehicleSystem = g_pGame->GetIGameFramework()->GetIVehicleSystem();
		uint32 vcount = pVehicleSystem->GetVehicleCount();
		if (vcount > 0)
		{
			IVehicleIteratorPtr iter = g_pGame->GetIGameFramework()->GetIVehicleSystem()->CreateVehicleIterator();
			while (IVehicle* pVehicle = iter->Next())
			{
				if(IEntity *pEntity = pVehicle->GetEntity())
				{
					if (pEntity->IsHidden())
						continue;
					AABB aabb;
					pEntity->GetWorldBounds(aabb);
					IPhysicalEntity* pEnt = pEntity->GetPhysics();
					if (pEnt && aabb.GetDistanceSqr(explosionInfo.pos) <= explosionInfo.radius*explosionInfo.radius)
					{
						float affected = gEnv->pPhysicalWorld->CalculateExplosionExposure(&explosion, pEnt);
						AddOrUpdateAffectedEntity(affectedEntities, pEntity, affected);
					}
				}
			}
		}
	}

	explosion.rmin = explosionInfo.minPhysRadius;
	explosion.rmax = explosionInfo.physRadius;
	if (explosion.rmax==0)
		explosion.rmax=0.0001f;
	explosion.r = explosion.rmin;
	explosion.holeSize = explosionInfo.hole_size;
	explosion.nOccRes = -1;	// makes second call re-use occlusion info

	gEnv->pPhysicalWorld->SimulateExplosion( &explosion, pSkipList, skipCount, ent_rigid | ent_sleeping_rigid | ent_independent|ent_static|ent_delayed_deformations);

	if (gEnv->bServer)
	{
		UpdateAffectedEntitiesSet(affectedEntities, explosion);

		const bool bIsClient = (explosionInfo.shooterId == g_pGame->GetIGameFramework()->GetClientActorId());
		const bool bEnableFriendlyHit = (bIsClient ? g_pGameCVars->g_enableFriendlyPlayerHits : g_pGameCVars->g_enableFriendlyAIHits) != 0;
		if (!bEnableFriendlyHit)
		{
			RemoveFriendlyAffectedEntities(explosionInfo, affectedEntities);
		}
	}
}

//-------------------------------------------
void CGameRules::SuccessfulFlashBang(const ExplosionInfo &explosionInfo, float time)
{
	GetGameObject()->InvokeRMI(SvSuccessfulFlashBang(), SSuccessfulFlashBangParams(explosionInfo.shooterId, explosionInfo.damage, time), eRMI_ToServer);
}

//-----------------------------------------------------
void CGameRules::GetEntitiesToSkipByExplosion(const ExplosionInfo& explosionInfo, IPhysicalEntity** skipList, int& skipListSize) const
{
	skipListSize = 0;

	//For the microwave beam we are going to ignore any complex static geometry
	if(explosionInfo.type == CGameRules::EHitType::Mike_Burn)
	{
		IPhysicalEntity *pEnts[sSimulateExplosionMaxEntitiesToSkip];
		IPhysicalEntity **ppEnts = pEnts;
		float radius = max(explosionInfo.physRadius, 0.0001f);
		int numEntities = gEnv->pPhysicalWorld->GetEntitiesInBox(explosionInfo.pos-Vec3(1,1,1)*radius, explosionInfo.pos+Vec3(1,1,1)*radius, ppEnts, ent_static, sSimulateExplosionMaxEntitiesToSkip);

		int chunkThreshold = g_pGameCVars->g_MicrowaveBeamStaticObjectMaxChunkThreshold;

		for(int i = 0; i < numEntities; ++i)
		{
			IEntity *pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(ppEnts[i]);
			if (!pEntity)
			{
				if(ppEnts[i]->GetiForeignData() == PHYS_FOREIGN_ID_STATIC)
				{
					IRenderNode* pRenderNode = (IRenderNode*)ppEnts[i]->GetForeignData(PHYS_FOREIGN_ID_STATIC);
					if(pRenderNode != NULL && pRenderNode->GetRenderNodeType() == eERType_Brush)
					{
						IStatObj* pStatObj = pRenderNode->GetEntityStatObj();
						IRenderMesh* pRenderMesh = (IRenderMesh*)pStatObj->GetRenderMesh();
						if(pRenderMesh != NULL && pRenderMesh->GetChunksSubObjects().size() > chunkThreshold)
						{
							skipList[skipListSize] = ppEnts[i];
							++skipListSize;

							if(skipListSize == sSimulateExplosionMaxEntitiesToSkip)
							{
								break;
							}
						}
					}
				}
			}
		}
	}
	else if(gEnv->bMultiplayer && explosionInfo.projectileId==0 && explosionInfo.type == CGameRules::EHitType::Explosion)
	{
		if(IEntity* pEntity = gEnv->pEntitySystem->GetEntity(explosionInfo.weaponId))
		{
			if( IPhysicalEntity* piPhysicalEntity = pEntity != NULL ? pEntity->GetPhysics() : NULL )
			{
				skipList[skipListSize] = piPhysicalEntity;
				skipListSize++;
			}
		}
	}
}

void CGameRules::UpdateNetLimbo()
{
	if (gEnv->bMultiplayer && !gEnv->bServer && (g_pGame->GetHostMigrationState() == CGame::eHMS_NotMigrating))
	{
		// Get the time since we were last here and add it to the timeout - makes the check stall resistant
		CTimeValue now = gEnv->pTimer->GetAsyncTime();
		static CTimeValue timeLastCalled = now;

		CTimeValue timeSinceLastCalled = (now - timeLastCalled);
		timeLastCalled = now;

		CTimeValue timeOut(g_pGameCVars->sv_netLimboTimeout);
		timeOut += timeSinceLastCalled;

		if (GetStateModule() && GetStateModule()->GetGameState() != IGameRulesStateModule::EGRS_InGame)
		{
			static CTimeValue END_OF_ROUND_TIMEOUT_BOOST(10.0f);
			timeOut += END_OF_ROUND_TIMEOUT_BOOST;
		}

		ICryLobby *pLobby = gEnv->pNetwork->GetLobby();
		if (pLobby)
		{
			ICryMatchMaking *pMatchMaking = pLobby->GetMatchMaking();
			if (pMatchMaking)
			{
				CGameLobby *pGameLobby = g_pGame->GetGameLobby();
				if (pGameLobby)
				{
					CrySessionHandle gh = pGameLobby->GetCurrentSessionHandle();

					CTimeValue packetFromServerTime;
					
					packetFromServerTime.SetMilliSeconds(pMatchMaking->GetTimeSincePacketFromServerMS(gh));

					static CTimeValue oldPacketFromServerTime = packetFromServerTime;
					if (packetFromServerTime > timeOut)
					{
						CryLog("CGameRules::UpdateNetLimbo() GameState:%d timing out server connection, actual timeSinceReceived=%" PRIi64, (int)(GetStateModule()->GetGameState()), packetFromServerTime.GetMilliSecondsAsInt64());

						pMatchMaking->DisconnectFromServer(gh);
					}
					else if (packetFromServerTime > 5.0f)
					{
						if (oldPacketFromServerTime <= 5.0f)
						{
							CryLog("CGameRules::UpdateNetLimbo() GameState:%d 5.0f seconds timing out server connection, actual timeSinceReceived=%" PRIi64, (int)(GetStateModule()->GetGameState()), packetFromServerTime.GetMilliSecondsAsInt64());
						}
					}
					else if (packetFromServerTime > 3.0f)
					{
						if (oldPacketFromServerTime <= 3.0f)
						{
							CryLog("CGameRules::UpdateNetLimbo() GameState:%d 3.0f seconds timing out server connection, actual timeSinceReceived=%" PRIi64, (int)(GetStateModule()->GetGameState()), packetFromServerTime.GetMilliSecondsAsInt64());
						}
					}
					else if (packetFromServerTime > 2.0f)
					{
						if (oldPacketFromServerTime <= 2.0f)
						{
							CryLog("CGameRules::UpdateNetLimbo() GameState:%d 2.0f seconds timing out server connection, actual timeSinceReceived=%" PRIi64, (int)(GetStateModule()->GetGameState()), packetFromServerTime.GetMilliSecondsAsInt64());
						}
					}
					oldPacketFromServerTime = packetFromServerTime;
				}
			}
		}
	}
}

void CGameRules::UpdateIdleKick(float frametime)
{
	if (gEnv->bMultiplayer && (g_pGameCVars->g_idleKickTime > 0))
	{
		IGameRulesStateModule *pStateModule = GetStateModule();
		if (pStateModule && (pStateModule->GetGameState() == IGameRulesStateModule::EGRS_InGame))
		{
			CGameLobby* pGameLobby = g_pGame->GetGameLobby();
			// Idle kick doesn't apply to private games or LAN games
			if (pGameLobby && !pGameLobby->IsPrivateGame() && pGameLobby->IsOnlineGame())
			{
				IActor* pActor = g_pGame->GetIGameFramework()->GetClientActor();
				if (pActor && !pActor->IsDead())
				{
					// Only increment idle time while alive
					m_idleTime += frametime;
					if (m_idleTime > g_pGameCVars->g_idleKickTime)
					{
						CryLog("You have been kicked because you were idle for %f seconds", m_idleTime);
						gEnv->pConsole->ExecuteString("disconnect", false, true);
						g_pGame->GetWarnings()->AddGameWarning("IdleTimeout", NULL, NULL);
					}
				}
			}
		}
	}
}


// ===========================================================================
// Retrieve the current health and death state for a target entity.
//
//  In:     The entity (NULL will abort!)
//  In:     The actor (NULL in case the entity is not an actor).
//  Out:    The current health of the target (NULL will ignore).
//  Out:    The current death state of the target (NULL will ignore).
//
void CGameRules::RetrieveCurrentHealthAndDeathForTarget(
	const IEntity *pEntity, const IActor *pActor, const IVehicle* pVehicle, 
	float* resultHealth, bool* resultDead) const
{
	float health = 0.0f;
	bool dead = false;

	IF_LIKELY(pEntity != NULL)
	{
		IF_LIKELY (pActor != NULL)
		{
			health = pActor->GetHealth();
			dead = pActor->IsDead();
		}
		else if(pVehicle != NULL)
		{
			health = pVehicle->GetStatus().health;
			dead = (health <= 0);
		}
		else
		{
			IEntityClass* pTargetClass = pEntity->GetClass();
			if (pTargetClass == s_pTurretClass)
			{
				assert(g_pGame != NULL);

				IGameFramework* pGameFramework = g_pGame->GetIGameFramework();
				assert(pGameFramework != NULL);

				IGameObject* pGameObject = pGameFramework->GetGameObject(pEntity->GetId());
				if (pGameObject != NULL)
				{
					IGameObjectExtension* pTurretExtension = pGameObject->QueryExtension("Turret");
					if (pTurretExtension != NULL)
					{
						CTurret* pTurret = static_cast<CTurret*>(pTurretExtension);
						health = pTurret->GetHealth();
						dead = pTurret->IsDead();					
					}
				}
			}
		}
	}

	if (resultHealth != NULL)
	{
		*resultHealth = health;
	}
	if (resultDead != NULL)
	{
		*resultDead = dead;
	}
}

//--------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------
// RMI
//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvRequestRename)
{
	IActor *pActor = GetActorByEntityId(params.entityId);
	if (!pActor)
		return true;

	RenamePlayer(pActor, params.name.c_str());

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClRenameEntity)
{
	IEntity *pEntity=gEnv->pEntitySystem->GetEntity(params.entityId);
	if (pEntity)
	{
		string old=pEntity->GetName();
		pEntity->SetName(params.name.c_str());

		CryLogAlways("$8%s$o renamed to $8%s", old.c_str(), params.name.c_str());
		CCCPOINT(GameRules_ClRenameEntity);

		SHUDEventWrapper::PlayerRename(pEntity->GetId());
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvRequestChangeTeam)
{
	IActor *pActor = GetActorByEntityId(params.entityId);
	if (!pActor)
		return true;

	ChangeTeam(pActor, params.teamId, params.onlyIfUnassigned);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvRequestSpectatorMode)
{
	IActor *pActor = GetActorByEntityId(params.entityId);
	if (!pActor)
		return true;

	IGameRulesSpectatorModule*  specmod = GetSpectatorModule();
	if (!specmod)
		return true;

	//revived since last died
	if(!params.force)
	{
		IGameRulesSpawningModule* spawnMod = GetSpawningModule();
		if(spawnMod)
		{
			const IGameRulesSpawningModule::TPlayerDataMap* playerData = spawnMod->GetPlayerValuesMap();
		
			IGameRulesSpawningModule::TPlayerDataMap::const_iterator  it = playerData->find(params.entityId);
			if (it != playerData->end())
			{
				if(it->second.lastRevivedTime >= it->second.deathTime)
				{
					bool  denyRequest = true;

					if (gEnv->bMultiplayer)
					{
						CRY_ASSERT(pActor->GetEntityId() != g_pGame->GetIGameFramework()->GetClientActorId());  // i'm not expecting this RMI to get sent to the server from the server for the server's client

						if (specmod->GetServerAlwaysAllowsSpectatorModeChange())
						{
							denyRequest=false;
						}
						else if ((spawnMod->SvIsMidRoundJoiningAllowed() == false) && (it->second.lastRevivedTime <= 0.f) && (it->second.deathTime <= 0.f) && (params.mode != CActor::eASM_None))
						{
							denyRequest = false;
						}
						else if(static_cast<CActor*>(pActor)->GetSpectatorState() == CActor::eASS_SpectatorMode)
						{
							denyRequest = false;
						}
					}

					if (denyRequest)
					{
						CryLog("%s requested spectator mode %d but the request was denied", pActor->GetEntity()->GetName(), params.mode);
						return true;
					}
				}
			}
		}
	}
	
	CryLog("%s requested spectator mode %d, (forced - %s)", pActor->GetEntity()->GetName(), params.mode, params.force ? "True" : "False");
	specmod->ChangeSpectatorMode(pActor, params.mode, params.targetId, params.resetAll);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvSetSpectatorState )
{
	CActor *pActor = static_cast<CActor*>( gEnv->pGameFramework->GetIActorSystem()->GetActor( params.entityId ) );
	if (pActor)
	{
		CActor::EActorSpectatorState prevState = pActor->GetSpectatorState();

		IGameRulesSpectatorModule *pSpectatorModule = GetSpectatorModule();
		if( pSpectatorModule )
		{
			pSpectatorModule->ChangeSpectatorMode( pActor, params.mode, 0, false );
		}

		pActor->SetSpectatorState( params.state );

		if(prevState != params.state && (prevState == CActor::eASS_SpectatorMode || params.state == CActor::eASS_SpectatorMode))
		{
			pActor->OnSpectateModeStatusChanged(params.state == CActor::eASS_SpectatorMode);
		}
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClTeamFull)
{
	int teamId = params.param;
	if (teamId==1||teamId==2)
	{
		if (g_pGame->GetWarnings())
		{
			g_pGame->GetWarnings()->AddGameWarning("TeamFull", (teamId==1)?"@ui_hud_team_1":"@ui_hud_team_2", NULL);
		}
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClSetTeam)
{
	ClDoSetTeam(params.teamId, params.entityId);

	return true;
}

//------------------------------------------------------------------------
void CGameRules::ClDoSetTeam(int teamId, EntityId entityId)
{
	bool bIsPlayer;
	SetTeam_Common(teamId, entityId, bIsPlayer);
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClTextMessage)
{
	OnTextMessage((ETextMessageType)params.type, params.msg.c_str(), 
		params.params[0].empty()?0:params.params[0].c_str(),
		params.params[1].empty()?0:params.params[1].c_str(),
		params.params[2].empty()?0:params.params[2].c_str(),
		params.params[3].empty()?0:params.params[3].c_str()
		);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClNetConsoleCommand)
{
	CRY_TODO(05, 02, 2010, "This needs to be completely #if'd out for final release.");
#if !defined(_RELEASE)
	gEnv->pConsole->ExecuteString(params.m_commandString.c_str());
#endif

	return true;
}

//------------------------------------------------------------------------
// a remote client has told the server they've hit something
IMPLEMENT_RMI(CGameRules, SvRequestHit)
{
	HitInfo info(params);
	info.remote=true;

	ServerHit(info);
	IGameRulesAssistScoringModule *assistScoringModule = GetAssistScoringModule();
	if (assistScoringModule)
	{
		assistScoringModule->OnEntityHit(info);
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClExplosion)
{
	QueueExplosion(params);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClProjectileExplosion)
{
	ProjectileExplosion(params);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClProjectileExplosion_Impact)
{
	ProjectileExplosion(params.m_params);

	return true;
}

//-----------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClPostInit)
{
	int64 secondsSinceStarted = (int64) params.timeSinceGameStarted;
	CTimeValue serverTime = g_pGame->GetIGameFramework()->GetServerTime();
	CTimeValue timeSinceStarted;
	timeSinceStarted.SetSeconds(secondsSinceStarted);
	m_gameStartedTime = serverTime - timeSinceStarted;

	SkillKill::SetFirstBlood(params.firstBlood);
	m_uSecurity = params.uSecurity;
	m_bSecurityInitialized = true;

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClSetGameStartedTime)
{
	m_gameStartedTime = params.time;

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClSetGameStartTimer)
{
	m_gameStartTime = params.time;
		
	IGameRulesStateModule *pStateModule = GetStateModule();
	if (pStateModule)
	{
		pStateModule->StartCountdown(true);
	}
	
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClTaggedEntity)
{
	if (!params.targetId)
	{
		return true;
	}

	CPlayer* pClientPlayer = static_cast<CPlayer*>( gEnv->pGameFramework->GetClientActor() );
	if( !pClientPlayer )
	{
		return true;
	}

	const bool bTaggedLocalPlayer = (pClientPlayer->GetEntityId()==params.targetId);

	bool doRadarOnly = false; // onShot in SP does radar only.
	switch( params.m_reason )
	{		
	case eRTR_Tagging : // Tagging via visor/scan mode
	{
		if(!bTaggedLocalPlayer)
		{
			if(params.shooterId == pClientPlayer->GetEntityId())
			{
				CAudioSignalPlayer::JustPlay("Tagged");
			}

			BATTLECHATTER(BC_TaggedHostiles, params.shooterId);
		}
		break;
	}
	// Tagging because they were shot, only uses radar
	case eRTR_OnShot :
	case eRTR_RadarOnly :
		{
			// Should not get this in MP if 'hud_onshot_enabled' is off or 'hud_onshot_radarEnabled' is off (the only desired/supported variation currently) is off.
			doRadarOnly = true;
		}
		break;
	case eRTR_OnShoot :
	default :
		{
			// should not get here, should be handled by SvAddTaggedEntity
			CRY_ASSERT_MESSAGE(0, "ClTaggedEntity: Unhandled reason in tagging RMI, ClTaggedEntity." );
		}
		break;
	}

	if(bTaggedLocalPlayer)
	{
		// Local Actor has been tagged.
		SHUDEvent eventOnLocalActorTagged(eHUDEvent_OnLocalActorTagged);
		eventOnLocalActorTagged.AddData( params.m_time );
		CHUDEventDispatcher::CallEvent(eventOnLocalActorTagged);
	}
	else
	{
		// Multiplayer sets the entity tagged here
		if (!doRadarOnly)
		{
			g_pGame->GetTacticalManager()->SetEntityTagged(params.targetId, true);
		}

		// Show entity on radar and maybe via tag-names.
		SHUDEvent eventTempAddToRadar(eHUDEvent_TemporarilyTrackEntity);
		eventTempAddToRadar.AddData( static_cast<int>(params.targetId) );
		eventTempAddToRadar.AddData( params.m_time );
		eventTempAddToRadar.AddData( doRadarOnly );
		eventTempAddToRadar.AddData( false );			// do not force to be no updating pos
		CHUDEventDispatcher::CallEvent(eventTempAddToRadar);
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvRequestTagEntity)
{
	CryLog("[tlh] @ (CGameRules RMI) SvRequestTagEntity()");
	assert(gEnv->bServer);
	RequestTagEntity( params.shooterId, params.targetId, params.m_time, params.m_reason );

	return true;
}

//------------------------------------------------------------------------
// NB : Also see ActorDamageEffectController.h
// Our local client player is being told that they've been hit by something
IMPLEMENT_RMI(CGameRules, ClProcessHit)
{
	IActor *pClientActor = m_pGameFramework->GetClientActor();
  
  IGameRulesDamageHandlingModule* pDmgMo=GetDamageHandlingModule();
  if (pDmgMo != NULL && pClientActor && params.hitTypeId)
  {
    pDmgMo->ClProcessHit(params.dir, params.shooterId, params.weaponId, params.damage, params.projectileClassId, params.hitTypeId);
  }

	if(pClientActor)
	{
		pClientActor = !pClientActor->IsDead() ? pClientActor : NULL; // Is alive.
	}

	{
		Vec3 vecToUse = params.dir;
		EHUDEventType typeToUse = eHUDEvent_OnShowHitIndicator;
		if (gEnv->bMultiplayer)
		{
			IEntity * pShooterEntity = gEnv->pEntitySystem->GetEntity(params.shooterId);

			if (pShooterEntity)
			{
				vecToUse = pShooterEntity->GetWorldPos();
				typeToUse = eHUDEvent_OnShowHitIndicatorPlayerUpdated;
			}
		}

		SHUDEvent hitEvent(typeToUse);
		hitEvent.ReserveData(3);
		hitEvent.AddData(vecToUse.x);
		hitEvent.AddData(vecToUse.y);
		hitEvent.AddData(vecToUse.z);
		CHUDEventDispatcher::CallEvent(hitEvent);
	}

	CActor *pActor = static_cast<CActor *>(pClientActor);
	if (pActor)
	{
		IEntityClass *pProjectileClass = NULL;
		if(params.projectileClassId != 0xFFFF)
		{
			char projectileClassName[128];
			if (g_pGame->GetIGameFramework()->GetNetworkSafeClassName(projectileClassName, sizeof(projectileClassName), params.projectileClassId))
			{
				pProjectileClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(projectileClassName);
			}
		}

		CRY_ASSERT_TRACE (params.hitTypeId, ("%s '%s' is taking damage of an unknown type %u - not informing entity it's been hit!", pActor->GetEntity()->GetClass()->GetName(), pActor->GetEntity()->GetName(), params.hitTypeId));

		if (params.hitTypeId)
		{
			pActor->DamageInfo(params.shooterId, params.weaponId, pProjectileClass, params.damage, params.hitTypeId, params.dir);

			CRY_ASSERT(pActor->IsClient());
			CPersistantStats::GetInstance()->ClientShot(this, params.hitTypeId, params.dir);
			
			if ( gEnv->bMultiplayer )
			{
				CPersistantStats::GetInstance()->IncrementClientStats(EFPS_DamageTaken,params.damage);
			}
		}

		if (params.hitTypeId == EHitType::Electricity)
		{
			NET_BATTLECHATTER(BC_Electrocuted, pActor);	// battlechatter data will throttle this being sent as required
		}
	}	

	//SAFE_HUD_FUNC(ShowTargettingAI(params.shooterId));
	return true;
}

//------------------------------------------------------------------------

IMPLEMENT_RMI(CGameRules, SvVote)
{
	IActor* pActor = GetActorByChannelId(m_pGameFramework->GetGameChannelId(pNetChannel));
	if(pActor)
		Vote(pActor, true);
	return true;
}

IMPLEMENT_RMI(CGameRules, SvVoteNo)
{
	IActor* pActor = GetActorByChannelId(m_pGameFramework->GetGameChannelId(pNetChannel));
	if(pActor)
		Vote(pActor, false);
	return true;
}

IMPLEMENT_RMI(CGameRules, SvStartVoting)
{
  IActor* pActor = GetActorByChannelId(m_pGameFramework->GetGameChannelId(pNetChannel));
  if(pActor)
    StartVoting(pActor,params.vote_type,params.entityId,params.param);
  return true;
}

IMPLEMENT_RMI(CGameRules, ClKickVoteStatus)
{
	CGameLobby * pGameLobby = g_pGame->GetGameLobby();
	IEntity * pEntity = gEnv->pEntitySystem->GetEntity(params.entityId);
	CGameLobby::SChatMessage message;
	if(pGameLobby)
	{
		const EntityId clientActorId = gEnv->pGameFramework->GetClientActorId();
		switch(params.kickState)
		{
		case eKS_StartVote:
			{
				if (pEntity)
				{
					message.Set(CryMatchMakingInvalidConnectionUID, -1, CHUDUtils::LocalizeString("@mp_vote_initialized_kick", pEntity->GetName()));
					pGameLobby->LocalMessage(&message);

					m_bClientKickVoteActive = true;
					m_bClientKickVoteSent = ((params.lastVoterId == clientActorId) || (params.entityId == clientActorId)); // Either vote starter, or player being voted
					if (params.lastVoterId == clientActorId)
					{
						const float f_sv_votingCooldown = (float)g_pGame->GetCVars()->sv_votingCooldown;
						m_ClientCooldownEndTime = gEnv->pTimer->GetFrameStartTime() + (CTimeValue)f_sv_votingCooldown;
					}

					SHUDEvent hudEvent(eHUDEvent_OnVotingStarted);
					hudEvent.AddData(SHUDEventData(int(params.entityId)));
					hudEvent.AddData(SHUDEventData(int(params.lastVoterId)));
					hudEvent.AddData(SHUDEventData(params.voteEndTime));
					hudEvent.AddData(SHUDEventData(int(params.totalRequiredVotes)));
					hudEvent.AddData(SHUDEventData(int(params.votesFor)));
					hudEvent.AddData(SHUDEventData(int(params.votesAgainst)));
#if ENABLE_HUD_EXTRA_DEBUG
					hudEvent.AddData(SHUDEventData(false));
#endif
					CHUDEventDispatcher::CallEvent(hudEvent);
				}
			}
			break;
		case eKS_VoteProgress:
			{
				if (!m_bClientKickVoteSent)
				{
					m_bClientKickVoteSent = (params.lastVoterId == clientActorId);
				}

				SHUDEvent hudEvent(eHUDEvent_OnVotingProgressUpdate);
				hudEvent.AddData(SHUDEventData(int(params.lastVoterId)));
				hudEvent.AddData(SHUDEventData(int(params.totalRequiredVotes)));
				hudEvent.AddData(SHUDEventData(int(params.votesFor)));
				hudEvent.AddData(SHUDEventData(int(params.votesAgainst)));
				CHUDEventDispatcher::CallEvent(hudEvent);
			}
			break;
		case eKS_VoteEnd_Success:
			{
				m_bClientKickVoteActive = false;

				if (pEntity)
				{
					message.Set(CryMatchMakingInvalidConnectionUID, -1, CHUDUtils::LocalizeString("@mp_vote_initialized_kick_success", pEntity->GetName()));
					pGameLobby->LocalMessage(&message);
				}

				SHUDEvent hudEvent(eHUDEvent_OnVotingEnded);
				hudEvent.AddData(SHUDEventData(true));
				CHUDEventDispatcher::CallEvent(hudEvent);
			}
			break;
		case eKS_VoteEnd_Failure:
			{
				m_bClientKickVoteActive = false;

				if (pEntity)
				{
					message.Set(CryMatchMakingInvalidConnectionUID, -1, CHUDUtils::LocalizeString("@mp_vote_initialized_kick_fail"));
					pGameLobby->LocalMessage(&message);
				}

				SHUDEvent hudEvent(eHUDEvent_OnVotingEnded);
				hudEvent.AddData(SHUDEventData(false));
				CHUDEventDispatcher::CallEvent(hudEvent);
			}
			break;
		default:
			CRY_ASSERT(!"INCORRECT KICK STATUS ON UPDATE!");
			break;
		}
	}	

	return true;
}


IMPLEMENT_RMI(CGameRules, ClEnteredGame)
{
	CPlayer *pClientActor = static_cast<CPlayer*>(m_pGameFramework->GetClientActor());

	if (pClientActor)
	{
		IEntity *pClientEntity = pClientActor->GetEntity();
		const EntityId clientEntityId = pClientEntity->GetId();

		if(!gEnv->bServer)
		{
			int status[2];
			status[0] = GetTeam(clientEntityId);
			status[1] = pClientActor->GetSpectatorMode();
			m_pGameplayRecorder->Event(pClientEntity, GameplayEvent(eGE_Connected, 0, 0, (void*)status));
		}

		if (g_pGame->GetHostMigrationState() != CGame::eHMS_NotMigrating)
		{
			//-- code moved to CGameRules::OnComplete.
		}
		else
		{
			EnteredGame();
		}
	}

	return true;
}

//------------------------------------------------------------------------
void CGameRules::OnHostMigrationGotLocalPlayer(CPlayer *pPlayer)
{
	if (m_pHostMigrationParams)
	{
		pPlayer->SetMigrating(true);
	}
}


// This will only ever be called on the machine on which the scoring player is the client!
IMPLEMENT_RMI(CGameRules, ClAddPoints)
{
	CryLog("ClAddPoints: Local player has scored %d points", params.m_changeToScore);
	INDENT_LOG_DURING_SCOPE();

	CCCPOINT(GameRules_ClModifyScore);

	SGameRulesScoreInfo scoreInfo( (EGameRulesScoreType)params.m_type, (TGameRulesScoreInt)params.m_changeToScore );
	scoreInfo.AttachVictim( (EntityId)params.m_killedEntityId );

	SHUDEvent newScoreEvent(eHUDEvent_OnNewScore);
	newScoreEvent.AddData( (void*)(&scoreInfo) );
	CHUDEventDispatcher::CallEvent(newScoreEvent);

	if(scoreInfo.type == EGRST_PlayerKillAssist)
	{
		BATTLECHATTER_NEAREST_ACTOR(BC_AssistCongrats);
	}

	if(!gEnv->bServer)
	{
		ClientScoreEvent(params.m_type, params.m_changeToScore, params.m_reason, params.m_currentTeamScore);
	}

	return true;
}

IMPLEMENT_RMI(CGameRules, SvRequestRevive)
{
	CryLog("CGameRules:SvRequestRevive");
	IGameRulesSpawningModule *pSpawningModule = GetSpawningModule();
	if (pSpawningModule)
	{
		EntityId spawnEntity = pSpawningModule->GetNthSpawnLocation(params.index); 
		pSpawningModule->SvRequestRevive(params.entityId, spawnEntity);
	}
	else
	{
		CRY_ASSERT_MESSAGE(0, "CGameRules::SvRequestRevive() RMI - failed to find our spawning module, this is required");
	}

	return true;
}
//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClVictoryTeam)
{
	EGameOverReason reason = (EGameOverReason)params.reason;
	SetPausedGameTimer(true, reason);

	TTeamScoresMap::iterator it = m_teamscores.find(1);
	if (it != m_teamscores.end())
	{
		it->second.m_teamScore = params.team1Score;
	}

	it = m_teamscores.find(2);
	if (it != m_teamscores.end())
	{
		it->second.m_teamScore = params.team2Score;
	}

	if (m_playerStatsModule)
	{
		m_playerStatsModule->SetEndOfGameStats(params.m_playerStats);
	}

	if (m_victoryConditionsModule)
	{
		m_victoryConditionsModule->ClVictoryTeam(params.winningTeamId, reason, static_cast<ESVC_DrawResolution>(params.drawLevel), params.level1, params.level2, params.killedEntity, params.shooterEntity);
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClVictoryPlayer)
{
	EGameOverReason reason = (EGameOverReason)params.reason;
	SetPausedGameTimer(true, reason);

	if (m_playerStatsModule)
	{
		m_playerStatsModule->SetEndOfGameStats(params.m_playerStats);
	}

	if (m_victoryConditionsModule)
		m_victoryConditionsModule->ClVictoryPlayer(params.playerId, reason, params.killedEntity, params.shooterEntity);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClModuleRMISingleEntity)
{
	assert((int)m_moduleRMIListenersVec.size() >	params.m_listenerIndex);

	IGameRulesModuleRMIListener *pListener = m_moduleRMIListenersVec[params.m_listenerIndex];
	if (pListener)		// pListener could be NULL if we get a load of lag and end up receiving an RMI after we've removed the listener
	{
		pListener->OnSingleEntityRMI(params);
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClModuleRMIDoubleEntity)
{
	assert((int)m_moduleRMIListenersVec.size() >	params.m_listenerIndex);

	IGameRulesModuleRMIListener *pListener = m_moduleRMIListenersVec[params.m_listenerIndex];
	if (pListener)		// pListener could be NULL if we get a load of lag and end up receiving an RMI after we've removed the listener
	{
		pListener->OnDoubleEntityRMI(params);
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClModuleRMIEntityWithTime)
{
	assert((int)m_moduleRMIListenersVec.size() >	params.m_listenerIndex);

	IGameRulesModuleRMIListener *pListener = m_moduleRMIListenersVec[params.m_listenerIndex];
	if (pListener)		// pListener could be NULL if we get a load of lag and end up receiving an RMI after we've removed the listener
	{
		pListener->OnEntityWithTimeRMI(params);
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvModuleRMISingleEntity)
{
	assert((int)m_moduleRMIListenersVec.size() >	params.m_listenerIndex);

	IGameRulesModuleRMIListener *pListener = m_moduleRMIListenersVec[params.m_listenerIndex];
	if (pListener)		// pListener could be NULL if we get a load of lag and end up receiving an RMI after we've removed the listener
	{
		pListener->OnSingleEntityRMI(params);
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClVehicleDestroyed)
{
	CPersistantStats::GetInstance()->OnClientDestroyedVehicle(params);

	return true;
}


//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvModuleRMIOnAction)
{
	assert(0 <=	params.m_listenerIndex);
	assert((int)m_moduleRMIListenersVec.size() >	params.m_listenerIndex);

	if (IActor* pActor=GetActorByChannelId(m_pGameFramework->GetGameChannelId(pNetChannel)))  // sender client could've disconnected since sending RMI (possibly?)
	{
		if (IGameRulesModuleRMIListener* pListener=m_moduleRMIListenersVec[params.m_listenerIndex])  // pListener could be NULL if we get a load of lag and end up receiving an RMI after we've removed the listener
		{
			pListener->OnSvClientActionRMI(params, pActor->GetEntityId());
		}
	}

	return true;
}


//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvAfterMatchAwardsWorking)
{
	if (CPersistantStats *persistantStats = CPersistantStats::GetInstance())
	{
		if (CAfterMatchAwards *afterMatchAwards = persistantStats->GetAfterMatchAwards())
		{
			afterMatchAwards->ServerReceivedAwardsWorkingFromPlayer(params.m_playerEntityId, params.m_numAwards, params.m_awards);
		}
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClAfterMatchAwards)
{
	assert(gEnv->IsClient());

	CPersistantStats *persistantStats = CPersistantStats::GetInstance();
	if (persistantStats)
	{
		CAfterMatchAwards *afterMatchAwards = persistantStats->GetAfterMatchAwards();

		if (afterMatchAwards)
		{
			afterMatchAwards->ClientReceivedAwards(params.m_numAwards, params.m_awards);
		}
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvSetEquipmentLoadout)
{
	CEquipmentLoadout *pEquipmentLoadout = g_pGame->GetEquipmentLoadout();
	if (pEquipmentLoadout)
	{
		pEquipmentLoadout->SvSetClientLoadout(m_pGameFramework->GetGameChannelId(pNetChannel), params);
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvSuccessfulFlashBang)
{
	if(IGameRulesPlayerStatsModule *statsModule = GetPlayerStatsModule())
	{
    uint16 classid = 0;
    m_pGameFramework->GetNetworkSafeClassId(classid,"flashbang");
		statsModule->ProcessSuccessfulExplosion(params.shooterId, params.damage, classid);
	}

	uint16 channelId = m_pGameFramework->GetGameChannelId(pNetChannel);

	IActor *pActor = m_pGameFramework->GetIActorSystem()->GetActorByChannelId(channelId);
	if (pActor)
	{
		((CActor*)pActor)->GetTelemetry()->OnFlashed(params.time, params.shooterId);
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvHostMigrationRequestSetup)
{
	uint16 channelId = m_pGameFramework->GetGameChannelId(pNetChannel);
	CPlayer* pPlayer = static_cast<CPlayer*>(m_pGameFramework->GetIActorSystem()->GetActorByChannelId(channelId));
	if(pPlayer)
	{
		const EntityId playerPickAndThrowEntity = pPlayer->GetPickAndThrowEntity();
		if(params.m_environmentalWeaponId != playerPickAndThrowEntity)
		{
			const char* const name = "PickAndThrowWeapon";
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass( name );
			EntityId pickAndThrowWeaponId = pPlayer->GetInventory()->GetItemByClass( pClass );

			if(params.m_environmentalWeaponId)
			{
				if(CPickAndThrowWeapon* pPickAndThrowWeapon = static_cast<CPickAndThrowWeapon*>(pPlayer->GetItem(pickAndThrowWeaponId)))
				{
					CryLog("IMPLEMENT_RMI(CGameRules, SvHostMigrationRequestSetup) - Client thinks they're holding a pick & throw weapon %i", params.m_environmentalWeaponId);
					pPickAndThrowWeapon->QuickAttach();
				}
			}
			else
			{
				CryLog("IMPLEMENT_RMI(CGameRules, SvHostMigrationRequestSetup) - Client thinks they've dropped their pick & throw weapon %i", playerPickAndThrowEntity);
				pPlayer->ExitPickAndThrow(true);
			}

			CEnvironmentalWeapon *pEnvWeapon = static_cast<CEnvironmentalWeapon*>(m_pGameFramework->QueryGameObjectExtension(params.m_environmentalWeaponId ? params.m_environmentalWeaponId : playerPickAndThrowEntity, "EnvironmentalWeapon"));
			if(pEnvWeapon)
			{
				pEnvWeapon->OnHostMigration(params.m_environmentalWeaponRot, params.m_environmentalWeaponPos, params.m_environmentalWeaponVel);
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClHostMigrationFinished)
{
	g_pGame->SetHostMigrationState(CGame::eHMS_Resuming);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClPredictionFailed)
{
#if !defined(_RELEASE)
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(params.predictionHandle);
	INetContext *pNetContext = g_pGame->GetIGameFramework()->GetNetContext();
	assert(pEntity && pNetContext && !pNetContext->IsBound(params.predictionHandle));

	CryLog("ClPredictionFailed name %s id %d", pEntity ? pEntity->GetName() : "<null entity>", params.predictionHandle);
#endif

	gEnv->pEntitySystem->RemoveEntity(params.predictionHandle);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClMidMigrationJoin)
{
	CryLog("CGameRules::ClMidMigrationJoin() state=%i, timeSinceChange=%f", params.m_state, params.m_timeSinceStateChanged);
	CGame::EHostMigrationState newState = CGame::EHostMigrationState(params.m_state);
	float timeOfChange = gEnv->pTimer->GetAsyncCurTime() - params.m_timeSinceStateChanged;

	g_pGame->SetHostMigrationStateAndTime(newState, timeOfChange);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClHostMigrationPlayerJoined)
{
	const EntityId playerId = params.entityId;

#if !defined(_RELEASE)
	IEntity *pPlayer = gEnv->pEntitySystem->GetEntity(playerId);
	CryLog("CGameRules::ClHostMigrationPlayerJoined() '%s'", pPlayer ? pPlayer->GetName() : "<NULL>");
#endif

	SHUDEvent hostMigrationOnNewPlayer(eHUDEvent_HostMigrationOnNewPlayer);
	hostMigrationOnNewPlayer.AddData(SHUDEventData(int(playerId)));
	CHUDEventDispatcher::CallEvent(hostMigrationOnNewPlayer);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClUpdateRespawnData)
{
	uint32 hashId = (uint32) params.m_respawnHashId;
	SEntityRespawnData *pData = GetEntityRespawnDataByHashId(hashId);
	CRY_ASSERT(pData);
	if (pData)
	{
		if (pData->m_currentEntityId != params.m_respawnEntityId)
		{
			const EntityId previousId = pData->m_currentEntityId;

			pData->m_currentEntityId = params.m_respawnEntityId;
			pData->m_bHasRespawned = true;

			if (pData->properties.GetPtr())
			{
				IEntity *pEntity = m_pEntitySystem->GetEntity(params.m_respawnEntityId);
				if (pEntity)
				{
					SmartScriptTable properties;
					IScriptTable *pScriptTable=pEntity->GetScriptTable();
					if (pScriptTable != NULL && pScriptTable->GetValue("Properties", properties))
					{
						if (properties.GetPtr())
						{
							properties->Clone(pData->properties, true);

							CItem *pItem = static_cast<CItem*>(g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pEntity->GetId()));
							if (pItem)
							{
								// Have to tell the item to read the properties again so that the respawn data gets setup correctly
								pItem->ReadProperties(properties);
							}
						}

						if(pScriptTable)
						{
							HSCRIPTFUNCTION respawnFunction = 0;
							if( pScriptTable->GetValue("ClientOnRespawn", respawnFunction) )
							{
								Script::CallMethod(pScriptTable, respawnFunction, pScriptTable);
								gEnv->pScriptSystem->ReleaseFunc(respawnFunction);
							}
						}
					}
				}
			}

			for (TEntityRespawnMap::iterator it=m_respawns.begin(); it!=m_respawns.end(); ++ it)
			{
				EntityId id = it->first;
				if (id == previousId)
				{
					m_respawns.erase(it);
					break;
				}
			}
		}
	}

	return true;
}

IMPLEMENT_RMI(CGameRules, ClTrackViewSynchAnimations)
{
	m_mpTrackViewManager->Client_SynchAnimationTimes(params);

	return true;
}

IMPLEMENT_RMI(CGameRules, SvTrackViewRequestAnimation)
{
	m_mpTrackViewManager->AnimationRequested(params);

	return true;
}

IMPLEMENT_RMI(CGameRules, ClActivateHitIndicator)
{
	SHUDEvent hitEvent(eHUDEvent_OnShowHitIndicatorPlayerUpdated);
	hitEvent.ReserveData(3);
	hitEvent.AddData(params.originPos.x);
	hitEvent.AddData(params.originPos.y);
	hitEvent.AddData(params.originPos.z);
	CHUDEventDispatcher::CallEvent(hitEvent);

	return true;
}

#if USE_PC_PREMATCH
IMPLEMENT_RMI(CGameRules, ClStartingPrematchCountDown)
{
	if (m_prematchState != ePS_Match)
	{
		m_prematchState = ePS_Countdown;

		g_pGameActions->FilterMPPreGameFreeze()->Enable(true);

		SHUDEvent event(eHUDEvent_OnUpdateGameStartMessage);
		event.ReserveData(2);
		event.AddData(params.m_timerLength);
		event.AddData(ePreGameCountdown_MatchStarting);
		CHUDEventDispatcher::CallEvent(event);

		m_previousNumRequiredPlayers = -1;
		m_waitingForPlayerMessage1.clear();
		m_waitingForPlayerMessage2.clear();
		SHUDEventWrapper::OnBigWarningMessage(m_waitingForPlayerMessage1.c_str(), m_waitingForPlayerMessage2.c_str());

		if (!gEnv->bServer)
		{
			m_finishPrematchTime = gEnv->pTimer->GetFrameStartTime().GetSeconds() + params.m_timerLength;
			ChangePrematchState(ePS_Countdown);
		}

		g_pGame->GetUI()->ActivateDefaultState();
	}

	return true;
}
#endif

IMPLEMENT_RMI(CGameRules, ClPathFollowingAttachToPath)
{
	m_mpPathFollowingManager->RequestAttachEntityToPath(params);

	return true;
}

#if defined(DEV_CHEAT_HANDLING)
IMPLEMENT_RMI(CGameRules, ClHandleCheatAccusation)
{
	DesignerWarning(false, params.message);
	CryFatalError("%s", params.message.c_str());

	return true;
}
#endif

#ifdef INCLUDE_GAME_AI_RECORDER_NET

IMPLEMENT_RMI(CGameRules, SvRequestRecorderBookmark)
{
#ifdef INCLUDE_GAME_AI_RECORDER
	assert(gEnv->bServer);
	if (gEnv->bServer)
	{
		IActor* pActor = GetActorByChannelId(m_pGameFramework->GetGameChannelId(pNetChannel));
		if (pActor)
		{
			g_pGame->GetGameAISystem()->GetGameAIRecorder().AddRecordBookmark(pActor->GetEntityId());
		}
	}
#endif //INCLUDE_GAME_AI_RECORDER

	return true;
}

IMPLEMENT_RMI(CGameRules, SvRequestRecorderComment)
{
#ifdef INCLUDE_GAME_AI_RECORDER
	assert(gEnv->bServer);
	if (gEnv->bServer)
	{
		IActor* pActor = GetActorByChannelId(m_pGameFramework->GetGameChannelId(pNetChannel));
		if (pActor)
		{
			g_pGame->GetGameAISystem()->GetGameAIRecorder().AddRecordComment(pActor->GetEntityId(), params.str.c_str());
		}
	}
#endif //INCLUDE_GAME_AI_RECORDER

	return true;
}

#endif //INCLUDE_GAME_AI_RECORDER_NET
