// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:
	Part of Player that controls stealth killing.

-------------------------------------------------------------------------
History:
- 15:10:2009: Created by Sven Van Soom

*************************************************************************/
#include "StdAfx.h"
#include "Player.h"
#include "StealthKill.h"
#include "GameRules.h"
#include "Game.h"
#include "GameCVars.h"
#include "AutoAimManager.h"
#include "GameRulesModules/IGameRulesDamageHandlingModule.h"
#include "AI/GameAIEnv.h"
#include <CryAISystem/IAIObject.h>
#include "Item.h"
#include "PlayerAnimation.h"
#include "HitDeathReactions.h"
#include "PlayerStateEvents.h"
#include "NetPlayerInput.h"

#include "Network/Lobby/GameAchievements.h"

CStealthKill::TStealthKillParamsVector CStealthKill::s_stealthKillParams;
bool CStealthKill::s_dataLoaded = false;
float CStealthKill::s_stealthKill_attemptRange = 0.25f;
float CStealthKill::s_stealthKill_attemptTime = 0.1f;
float CStealthKill::s_stealthKill_attemptSpeed = 10.f;

CStealthKill::CStealthKill()
: m_pPlayer(NULL)
, m_attemptTimer(0.f)
, m_isBusy(false)
, m_targetId(0)
, m_targetPhysCounter(0)
, m_isDeathBlowDone(false)
, m_playerAnimQuatT(IDENTITY)
, m_animIndex(0)
, m_bCollisionsIgnored(false)
{
}

void CStealthKill::Init(CPlayer* pPlayer)
{
	CRY_ASSERT(m_pPlayer == NULL);
	if (m_pPlayer != NULL)
		return;

	m_pPlayer = pPlayer;
}

//-----------------------------------------------------------------------
CStealthKill::ECanExecuteOn CStealthKill::CanExecuteOn(CActor* pTarget, bool checkExtendedRange) const
{
	if(!IsKillerAbleToExecute())
	{
		return CE_NO;
	}

	IEntity* pTargetEntity = pTarget->GetEntity();
	IEntity* pPlayerEntity = m_pPlayer->GetEntity();
	const EntityId targetId = pTargetEntity->GetId();

	const SAutoaimTarget* pTargetInfo = g_pGame->GetAutoAimManager().GetTargetInfo(targetId);

	const bool isValidTarget = gEnv->bMultiplayer ? !pTarget->IsFriendlyEntity(pPlayerEntity->GetId()) 
																					      : pTargetInfo && pTargetInfo->HasFlagsSet((int8)(eAATF_AIHostile|eAATF_StealthKillable));
	
	const SStealthKillParams* pStealthKillParams = GetParamsForClass(pTargetEntity->GetClass());

	if (isValidTarget && pStealthKillParams)
	{
		// can't stealth kill actors in vehicles
		if(pTarget->GetLinkedVehicle())
			return CE_NO;

		// can't stealth kill fallen down actors
		if (pTarget->IsFallen())
			return CE_NO;

		if(gEnv->bMultiplayer)
		{
			if(pTarget->IsDead())
			{
				return CE_NO;
			}
			
			//check if they are already involved in a stealth kill
			// only in mp at the moment as in sp there should only ever be one stealth kill active on the local player
			if(SActorStats* pStats = pTarget->GetActorStats())
			{
				if(pStats->bStealthKilling || pStats->bStealthKilled || pStats->onGround <= 0.f)
				{
					return CE_NO;
				}

				if(checkExtendedRange)
				{
					// For initial 'show prompt' check do a velocity check too.  If our target is a remote player then we 
					// use the requested velocity instead of the actual since the local player can push the target which 
					// affects the velocity - can cause the stealth kill prompt to appear briefly when it shouldn't.
					float velocitySquared;

					IPlayerInput *pInput = static_cast<CPlayer*>(pTarget)->GetPlayerInput();
					if (pInput->GetType() == IPlayerInput::NETPLAYER_INPUT)
					{
						velocitySquared = static_cast<CNetPlayerInput*>(pInput)->GetRequestedVelocity().GetLengthSquared();
					}
					else
					{
						velocitySquared = pTarget->GetActorPhysics().velocity.GetLengthSquared();
					}

					if(velocitySquared > g_pGameCVars->pl_stealthKill_maxVelocitySquared)
					{
						return CE_NO;
					}
				}
			}

			if(m_pPlayer->IsOnGround() == false)
			{
				return CE_NO;
			}

			//If the killer or the victim are not standing, we need to make sure that they can stand before killing
			if(m_pPlayer->GetStance() != STANCE_STAND)
			{
				if(!m_pPlayer->TrySetStance(STANCE_STAND))
				{
					return CE_NO;
				}				
			}

			if(pTarget->GetStance() != STANCE_STAND)
			{
				if(!pTarget->TrySetStance(STANCE_STAND))
				{
					return CE_NO;
				}
			}

			// No Stealth Kills on VTOLs / moving objects.
			if(m_pPlayer->GetActorPhysics().groundColliderId!=0)
			{
				return CE_NO;
			}
		}

		// this player needs to be within a certain angle and distance behind the target in 2D
		const float behindTargetAngleDegrees = pStealthKillParams->angleRange;
		const float maxDistance = checkExtendedRange ? pStealthKillParams->extendedMaxDist : pStealthKillParams->maxDist;

		Vec3 v3PlayerToTarget = pPlayerEntity->GetWorldPos() - pTargetEntity->GetWorldPos();
		
		if(sqr(v3PlayerToTarget.z) < sqr(pStealthKillParams->maxHeightDiff))
		{
			Vec2 vPlayerToTarget(v3PlayerToTarget);
			float cosineBehindTargetHalfAngleRadians = cos(DEG2RAD(behindTargetAngleDegrees/2.0f));

			Vec2 vBehindTargetDir = -Vec2(pTarget->GetAnimatedCharacter()->GetAnimLocation().GetColumn1()).GetNormalizedSafe(); // In decoupled catchup mode we need the animated character's orientation

			if(gEnv->bMultiplayer)
			{
				Vec2	vSpineDir = -Vec2((pTargetEntity->GetRotation() * pTarget->GetBoneTransform(BONE_SPINE).q).GetColumn1()).GetNormalizedSafe();

				Vec2 finalDirection = LERP(vBehindTargetDir, vSpineDir, g_pGameCVars->pl_stealthKill_aimVsSpineLerp);

				finalDirection.Normalize();

#if !defined(_RELEASE)
				if(g_pGameCVars->pl_stealthKill_debug)
				{
					Vec3 targetpos = pTargetEntity->GetWorldPos();
					targetpos.z += 0.2f;

					gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(targetpos, ColorB(255, 0, 0), targetpos + finalDirection, ColorB(255, 0, 0));
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(targetpos, ColorB(255, 0, 255), targetpos + vBehindTargetDir, ColorB(255, 0, 255));
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(targetpos, ColorB(0, 255, 255), targetpos + vSpineDir, ColorB(0, 255, 255));
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(targetpos, ColorB(0, 0, 255), targetpos + vPlayerToTarget, ColorB(0, 0, 255));
				}
#endif
				vBehindTargetDir = finalDirection;
			}

			float dot = vPlayerToTarget.GetNormalizedSafe() * vBehindTargetDir;

			if (dot <= cosineBehindTargetHalfAngleRadians)
			{
				return CE_NO;
			}

			float distSquared = vPlayerToTarget.GetLength2();
			if (distSquared >= sqr(maxDistance))
			{
				return CE_NO_BUT_DONT_DO_OTHER_ACTIONS_ON_IT; // when the only reason why we cant stealth kill is the distance, we dont want other actions to be done on the target.
			}

			if (checkExtendedRange && g_pGameCVars->pl_stealthKill_usePhysicsCheck)
			{
				const float maxNormalDistance = pStealthKillParams->maxDist;
				if (distSquared >= sqr(maxNormalDistance))
				{
					pe_player_dimensions dimensions;
					IPhysicalEntity *pPhysics = m_pPlayer->GetEntity()->GetPhysics();
					if (pPhysics && pPhysics->GetParams(&dimensions))
					{
						if (dimensions.bUseCapsule)
						{
							primitives::capsule physPrimitive;
							physPrimitive.center = m_pPlayer->GetEntity()->GetPos();
							physPrimitive.center.z += dimensions.heightCollider;
							physPrimitive.axis.Set(0.f, 0.f, 1.f);
							physPrimitive.r = dimensions.sizeCollider.x * 0.1f;
							physPrimitive.hh = dimensions.sizeCollider.z;

							IPhysicalEntity *pSkipEntities[5];
							int numSkipEntities = 0;

							AddPhysics(pSkipEntities, numSkipEntities, m_pPlayer);
							AddPhysics(pSkipEntities, numSkipEntities, pTarget);
							
							// Ignore any held objects (e.g. pick n throw held objects)
							numSkipEntities += pTarget->GetPhysicalSkipEntities(&pSkipEntities[numSkipEntities], 1);

							CRY_ASSERT(numSkipEntities <= (sizeof(pSkipEntities) / sizeof *(pSkipEntities)));

							float distance = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(physPrimitive.type, &physPrimitive, -v3PlayerToTarget, ent_all, NULL, 0, geom_colltype0|geom_colltype_player, NULL, NULL, 0, pSkipEntities, numSkipEntities);
							if (distance == 0.f)
							{
								return CE_YES;
							}
							else
							{
								return CE_NO;
							}
						}
					}
				}
			}

			return CE_YES;
		}
	}

	return CE_NO;
}


//-----------------------------------------------------------------------
void CStealthKill::Update(float frameTime)
{
	CActor* pTargetActor = (m_targetId != 0) ? static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_targetId)) : NULL;

	if (pTargetActor != NULL)
	{
		if (m_isBusy)
		{
			if( CHitDeathReactionsPtr pHitDeathReactions = static_cast<CPlayer*> (pTargetActor)->GetHitDeathReactions() )
			{
				if( !pHitDeathReactions->IsInReaction() )
				{
					Leave(pTargetActor);
				}
			}
		}
		else if(m_pPlayer->IsClient())
		{
			SActorStats* pStats = m_pPlayer->GetActorStats();

			if(pStats && pStats->bAttemptingStealthKill)
			{
				if(m_attemptTimer > 0.f)
				{
					if(CanExecuteOn(pTargetActor, false)==CE_YES)
					{
						CHANGED_NETWORK_STATE(m_pPlayer, CPlayer::ASPECT_SNAP_TARGET);

						EnableActorCollisions();
						Enter(m_targetId);
						m_attemptTimer = 0.f;
					}
					else
					{
						m_attemptTimer -= frameTime;
						g_pGame->GetAutoAimManager().SetCloseCombatSnapTarget(m_targetId, s_stealthKill_attemptRange, s_stealthKill_attemptSpeed);
					}			
				}
				else
				{
					CHANGED_NETWORK_STATE(m_pPlayer, CPlayer::ASPECT_SNAP_TARGET);

					m_targetId = 0;
					EnableActorCollisions();
					m_pPlayer->FailedStealthKill();
				}
			}
		}
	}
	
	// StealthKill::Leave() is triggered BY CActionController during its EndActionsOnScope() Call. We must process the svHit 1 frame AFTER StealthKill::Leave() called 
	// otherwise we can ultimately end up calling Reset() on CActionController. When control returns to EndActionsOnScope() Explosions will ensue if it has been reset mid flow. 
	SActorStats* pStats = m_pPlayer->GetActorStats();
	if(pStats && !pStats->bStealthKilling && gEnv->bMultiplayer && gEnv->bServer)
	{
		HitInfo& pKillInfo = m_pPlayer->GetDelayedKillingHitInfo();

		if(pKillInfo.damage > 0.f)
		{
			IGameRulesDamageHandlingModule* damHanModule = g_pGame->GetGameRules()->GetDamageHandlingModule();

			if(damHanModule)
			{
				damHanModule->SvOnHitScaled(pKillInfo);
			}
		}

		pKillInfo.damage = 0.f;
	}
	
}

//-----------------------------------------------------------------------
void CStealthKill::ConstructHitInfo(EntityId killerId, EntityId victimId, const Vec3 &forwardDir, HitInfo &result)
{
	result.shooterId = killerId;
	result.weaponId = killerId;
	result.targetId = victimId;
	result.damage = 99999.0f; // CERTAIN_DEATH
	result.dir = forwardDir;
	result.normal = -forwardDir; // this has to be in an opposite direction from the hitInfo.dir or the hit is ignored as a 'backface' hit
	result.type = CGameRules::EHitType::StealthKill;
}


//-----------------------------------------------------------------------
void CStealthKill::DoDeathBlow(CActor* pTarget)
{
	if(pTarget)
	{
		Vec3 targetDir = pTarget->GetEntity()->GetForwardDir();

		if(gEnv->bServer)
		{
			CGameRules *pGameRules = g_pGame->GetGameRules();

			HitInfo hitInfo;
			ConstructHitInfo(m_pPlayer->GetEntityId(), pTarget->GetEntityId(), targetDir, hitInfo);
			g_pGame->GetGameRules()->ClientHit(hitInfo);

			pTarget->GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Ragdoll);
		}
		if( !pTarget->GetActorStats()->isRagDoll )
		{
			// Make sure the target doesn't get ragdollized again if he has revived
			if (m_targetPhysCounter == pTarget->GetNetPhysCounter())
			{
				pTarget->RagDollize(false);
			}
			else
			{
				CryLog("CStealthKill::DeathBlow tried to ragdollize player '%s' but he has already revived", pTarget->GetEntity()->GetName());
			}
		}

		if(!gEnv->bMultiplayer)
		{	
			// Give a small nudge in the hit direction to make the target fall over
			const SStealthKillParams* pStealthKillParams = GetParamsForClass(pTarget->GetEntity()->GetClass());

			if (pStealthKillParams)
			{
				ICharacterInstance* pCharacter = pTarget->GetEntity()->GetCharacter(0);
				int boneId = pCharacter ? pCharacter->GetIDefaultSkeleton().GetJointIDByName("Spine04") : -1;
				if (boneId != -1)
				{
					const float stealthKillDeathBlowVelocity = pStealthKillParams->impulseScale; // desired velocity after impulse in meters per second

					IPhysicalEntity* pTargetPhysics = pTarget->GetEntity()->GetPhysics();
					if (pTargetPhysics)
					{
						pe_player_dynamics simulationParams;
						if( pTargetPhysics->GetParams(&simulationParams) > 0 )
						{
							pe_action_impulse impulse;
							impulse.partid = boneId;
							impulse.impulse = targetDir*stealthKillDeathBlowVelocity*simulationParams.mass; // RagDollize reset the entity's rotation so I have to use the value I cached earlier
							pTargetPhysics->Action(&impulse);
						}
					}
				}
			}
#ifdef STEALTH_KILL_DEBUG
			else
			{
				GameWarning("StealthKill (DeathBlow): params for class '%s' not found! This code shouldn't have executed.", pTarget->GetEntity()->GetClass()->GetName());
			}
#endif
		}
	}
}

//-----------------------------------------------------------------------
bool CStealthKill::IsKillerAbleToExecute() const
{
	if(gEnv->bMultiplayer)
	{
		return (g_pGameCVars->pl_stealthKill_allowInMP == 1)
			|| (g_pGameCVars->pl_stealthKill_allowInMP == 2);
	}
	else
	{
		return (m_pPlayer->IsCinematicFlagActive(SPlayerStats::eCinematicFlag_LowerWeapon) == false);
	}
}

//-----------------------------------------------------------------------
void CStealthKill::DeathBlow(CActor* pTarget)
{
	CRY_ASSERT_MESSAGE(m_isBusy, "Stealth kill should be in progress when triggering the death blow");
	if (!m_isBusy)
		return;

	if (m_isDeathBlowDone)
		return;

	DoDeathBlow(pTarget);

	m_isDeathBlowDone = true;
}

void CStealthKill::Abort(EntityId victim, EntityId victimMountedWeapon)
{
	EntityId victimId = victim != 0 ? victim : m_targetId;

	CActor* pTarget = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(victimId));

	if(pTarget)
	{
		SActorStats* pPlayerStats = m_pPlayer->GetActorStats();
		if(pPlayerStats && !pPlayerStats->bStealthKilled)
		{
			static_cast<CPlayer*> (pTarget)->GetHitDeathReactions()->EndCurrentReaction();
		}

		if(pTarget->IsPlayer() && !pTarget->IsDead())
		{
			CPlayer* pTargetPlayer = static_cast<CPlayer*>(pTarget);
			pTargetPlayer->UnRagdollize();

			SActorStats* pTargetStats = pTarget->GetActorStats();

			if(pTargetStats && pTargetPlayer->GetStealthKilledBy() == m_pPlayer->GetEntityId())
			{
				pTargetPlayer->SetStealthKilledBy(0);
				pTargetStats->bStealthKilled = false;
			}
			
			if(victimMountedWeapon)
			{
				CItem* pMountedWeapon = pTargetPlayer->GetItem(victimMountedWeapon);
				if(pMountedWeapon)
				{
					pMountedWeapon->StartUse(victimId);
				}
			}
			else if(IItem* pItem = pTargetPlayer->GetCurrentItem())
			{
				pItem->HideItem(false);
			}
		}

		m_targetId = 0;

		if(m_isBusy)
		{
			ResetState(pTarget);
		}
	}
}

void CStealthKill::ResetState(IActor* pTarget)
{
	m_isBusy = false;
	m_targetPhysCounter = 0;
	m_isDeathBlowDone = false;
	m_playerAnimQuatT.SetIdentity();
	m_animIndex = 0;
	EnableActorCollisions();

	m_pPlayer->HolsterItem(false);

	// Enable AI on the target again (for what it's worth - this helps editor)
	if (pTarget && !pTarget->IsPlayer() && pTarget->GetEntity()->GetAI())
	{
		pTarget->GetEntity()->GetAI()->Event(AIEVENT_ENABLE, 0);
	}

	SetStatsViewMode(false);

	SActorStats* pStats = m_pPlayer->GetActorStats();

	if(pStats)
	{
		pStats->bStealthKilling = false;
	}

	// Update visibility to change render mode of 1st person character
	m_pPlayer->RefreshVisibilityState();
}

//-----------------------------------------------------------------------
void CStealthKill::Leave(CActor* pTarget)
{
	CRY_ASSERT_MESSAGE(m_isBusy, "Stealth kill cannot be stopped if it is not in progress");
	if (!m_isBusy)
		return;

	if (pTarget)
	{
		static_cast<CPlayer*> (pTarget)->GetHitDeathReactions()->EndCurrentReaction();
	}

	DeathBlow(pTarget); // Call this in case the notification from the animation system got skipped or missed for some reason

	ResetState(pTarget);

	// incase something failed, send the exit event to the movement HSM.
	m_pPlayer->StateMachineHandleEventMovement( SStateEventCoopAnim(m_targetId) );
	m_targetId = 0;

	// Set the view orientation to whatever the animation was at this point
	if(m_pPlayer->IsClient())
	{
		m_pPlayer->SetViewRotation( Quat::CreateRotationVDir( gEnv->pSystem->GetViewCamera().GetViewdir() ) );
	}

	CHANGED_NETWORK_STATE(m_pPlayer, CPlayer::ASPECT_STEALTH_KILL);

}


//-----------------------------------------------------------------------
void CStealthKill::Enter(int targetEntityId, int animIndex)
{
	CRY_ASSERT_MESSAGE(!m_isBusy, "Stealth kill should not be initiated while a stealth kill is already in progress");
	if (m_isBusy)
		return;

	CActor *pTargetActor = static_cast<CActor*>(gEnv->pGameFramework->GetIActorSystem()->GetActor(targetEntityId));
	CRY_ASSERT_MESSAGE(pTargetActor != NULL, "Stealth kill should act upon an actor");
	if (pTargetActor == NULL)
		return;

	CRY_ASSERT_MESSAGE(pTargetActor->GetActorClass() == CPlayer::GetActorClassType(), "TargetActor is not a CPlayer");

	int randomAnim = 0;
	{
		if (gEnv->bMultiplayer == false)
		{
			//Note: This has to be set before the hit in SP
			SActorStats* pTargetStats = pTargetActor->GetActorStats();
			if(pTargetStats)
			{
				pTargetStats->bStealthKilled = true;
			}
		}

		// I'd prefer this to be the other way around - that this class is owned by the stealthkill state
		// that change is a bit more involved so is best left for another changelist, if at all.
		// would save a lot of memory + pointless checks though.
		m_pPlayer->StateMachineHandleEventMovement( PLAYER_EVENT_STEALTHKILL );

		HitInfo hitInfo;
		ConstructHitInfo(m_pPlayer->GetEntityId(), pTargetActor->GetEntityId(), m_pPlayer->GetEntity()->GetForwardDir(), hitInfo);
		static_cast<CPlayer*> (pTargetActor)->GetHitDeathReactions()->OnReaction( hitInfo, &animIndex );

		CRY_ASSERT_MESSAGE( !gEnv->bMultiplayer || animIndex < 4, "NetSerialize_StealthKill doesn't support more than 4 animation variation for stealthkill!!" );
		randomAnim = animIndex;
	}

	SActorStats* pStats = m_pPlayer->GetActorStats();

	if(pStats)
	{
		pStats->bStealthKilling = true;
		pStats->bAttemptingStealthKill = false;
	}

	m_animIndex = randomAnim;
	m_targetId = targetEntityId;
	m_targetPhysCounter = pTargetActor->GetNetPhysCounter();
	m_isBusy = true;

	if(gEnv->bMultiplayer)
	{
		if(pTargetActor->IsPlayer())
		{
			CPlayer *pPlayer = static_cast<CPlayer*>(pTargetActor);
			pPlayer->CaughtInStealthKill(m_pPlayer->GetEntityId());
			pPlayer->SetStealthKilledBy(m_pPlayer->GetEntityId());
		}
	}
	
	IInventory *pInventory = pTargetActor->GetInventory();
	EntityId itemId = pInventory ? pInventory->GetCurrentItem() : 0;
	if (itemId)
	{
		CItem *pItem = pTargetActor->GetItem(itemId);
		if (pItem && pItem->IsUsed())
		{
			pItem->StopUse(pTargetActor->GetEntityId());
		}
		else if (pItem && !pTargetActor->IsPlayer())
		{
			const bool dropByDeath = (gEnv->bMultiplayer); 
			pTargetActor->DropItem(itemId, 1.0f, false, dropByDeath);
			pTargetActor->DropAttachedItems();
		}
	}

	if(!gEnv->bServer && m_pPlayer->IsClient())
	{
		CPlayer::SStealthKillRequestParams requestParams(m_targetId, m_animIndex);
		m_pPlayer->GetGameObject()->InvokeRMI(CPlayer::SvRequestStealthKill(), requestParams, eRMI_ToServer);
	}

	CHANGED_NETWORK_STATE(m_pPlayer, CPlayer::ASPECT_STEALTH_KILL);
}

const CStealthKill::SStealthKillParams* CStealthKill::GetParamsForClass( IEntityClass* pTargetClass )
{
	CRY_ASSERT(pTargetClass);

	const int paramsCount = s_stealthKillParams.size();
	for (int i = 0; i < paramsCount; ++i)
	{
		if (pTargetClass == s_stealthKillParams[i].pEnemyClass)
		{
			return &(s_stealthKillParams[i]);
		}
	}

	return NULL;
}

void CStealthKill::ReadXmlData( const IItemParamsNode* pRootNode, bool reloading /*= false*/ )
{
	if (!reloading && s_dataLoaded)
	{
		return;
	}

	const IItemParamsNode* pParams = pRootNode->GetChild("StealthKill");
	if (!pParams)
		return;

	s_dataLoaded = true;

	pParams->GetAttribute("attackAttemptRange", s_stealthKill_attemptRange);
	pParams->GetAttribute("attackAttemptTime", s_stealthKill_attemptTime);
	pParams->GetAttribute("attackAttemptSpeed", s_stealthKill_attemptSpeed);

	const int childCount = pParams->GetChildCount();
	s_stealthKillParams.clear();
	s_stealthKillParams.reserve(childCount);

	for (int i = 0; i < childCount; ++i)
	{
		const IItemParamsNode* pTargetParams = pParams->GetChild(i);
		CRY_ASSERT(pTargetParams);

		IEntityClass* pTargetClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(pTargetParams->GetName());
		if (pTargetClass)
		{
			SStealthKillParams targetParams;
			const IItemParamsNode* pChildParamsNode = pTargetParams->GetChild("Params");
			const IItemParamsNode* pChildAnimsNode = pTargetParams->GetChild("Anims");

			targetParams.pEnemyClass = pTargetClass;
			
			if(pChildParamsNode)
			{
				targetParams.impulseBone = pChildParamsNode->GetAttributeSafe("impulseBone");
				pChildParamsNode->GetAttribute("impulseScale", targetParams.impulseScale);
				pChildParamsNode->GetAttribute("maxDist", targetParams.maxDist);
				pChildParamsNode->GetAttribute("angleRange", targetParams.angleRange);
				
				int requiresCloak = 1;
				pChildParamsNode->GetAttribute("requiresCloak", requiresCloak);
				targetParams.requiresCloak = (requiresCloak != 0);

				int cloakInterference = 0;
				pChildParamsNode->GetAttribute("cloakInterference", cloakInterference);
				targetParams.cloakInterference = (cloakInterference != 0);

				targetParams.extendedMaxDist = targetParams.maxDist; //ensure if no extended dist is requested that it equals the maxDist 
				pChildParamsNode->GetAttribute("extendedMaxDist", targetParams.extendedMaxDist);
				pChildParamsNode->GetAttribute("maxHeightDiff", targetParams.maxHeightDiff);
			}
			
			if(pChildAnimsNode)
			{
				const int animCount = pChildAnimsNode->GetChildCount();

				targetParams.animations.reserve(animCount);

				for(int j = 0; j < animCount; j++)
				{
					const IItemParamsNode* pAnimNode = pChildAnimsNode->GetChild(j);

					if(pAnimNode)
					{
						SStealthKillAnimation newAnimation;

						int useKnife = 1;
						
						pAnimNode->GetAttribute("useKnife", useKnife);
						
						newAnimation.playerAnimation = pAnimNode->GetAttributeSafe("playerAnimation");
						newAnimation.enemyAnimation = pAnimNode->GetAttributeSafe("enemyAnimation");	
						newAnimation.useKnife = (useKnife != 0);

						targetParams.animations.push_back(newAnimation);
					}
				}
			}
			
			s_stealthKillParams.push_back(targetParams);
		}
#ifdef STEALTH_KILL_DEBUG
		else
		{
			GameWarning("Stealth Kill: Couldn't find entity of class '%s', skipping", pTargetParams->GetName());
		}
#endif
	}
}

void CStealthKill::CleanUp()
{
	stl::free_container(s_stealthKillParams);
	s_dataLoaded = false;
}

void CStealthKill::SetStatsViewMode( bool followCameraBone )
{
	SPlayerStats* pStats = static_cast<SPlayerStats*>(m_pPlayer->GetActorStats());
	if (pStats)
	{
		pStats->followCharacterHead = followCameraBone;
	}
}

CActor* CStealthKill::GetTarget()
{
	if(m_isBusy)
	{
		return static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_targetId));
	}

	return NULL;
}

EntityId CStealthKill::GetTargetId() const
{
	if(m_isBusy)
	{
		return m_targetId;
	}

	return 0;
}

QuatT CStealthKill::GetPlayerAnimQuatT()
{
	return m_playerAnimQuatT;
}

uint8 CStealthKill::GetAnimIndex()
{
	return m_animIndex;
}

void CStealthKill::SetTargetForAttackAttempt(EntityId targetId)
{
	CRY_ASSERT_MESSAGE(!m_isBusy, "Trying to set an attempted target while mid attack");
	CRY_ASSERT_MESSAGE(m_targetId == 0, "Trying to override an existing target");

	CHANGED_NETWORK_STATE(m_pPlayer, CPlayer::ASPECT_SNAP_TARGET);
	m_targetId = targetId;
	m_attemptTimer = s_stealthKill_attemptTime;

	DisableActorCollisions();
}

void CStealthKill::Serialize( TSerialize ser )
{
	CRY_ASSERT(ser.GetSerializationTarget() != eST_Network);
	//usually saving should not change anything, but it shouldn't have to deal with a stealth kill animation in progress either
	ser.Value("m_isBusy", m_isBusy);
	if(ser.IsWriting() && m_pPlayer && m_pPlayer->IsClient() && IsBusy())
	{
		Leave(GetTarget());
	}
	else if(ser.IsReading())
	{
		if(m_isBusy && m_pPlayer)
			m_pPlayer->HolsterItem(false);
		m_isBusy = false;
	}
}

void CStealthKill::ForceFinishKill()
{
	CActor* pCurrentTarget = GetTarget();
	if (pCurrentTarget)
	{
		Vec3 targetDir = pCurrentTarget->GetEntity()->GetForwardDir();

		HitInfo hitInfo;
		CStealthKill::ConstructHitInfo(m_pPlayer->GetEntityId(), pCurrentTarget->GetEntityId(), m_pPlayer->GetEntity()->GetForwardDir(), hitInfo);
		g_pGame->GetGameRules()->KillPlayer(pCurrentTarget, true, true, hitInfo);
	}
}

void CStealthKill::AddPhysics(IPhysicalEntity **pArr, int &pos, CActor *pPlayer)
{
	IPhysicalEntity *pPlayerPhysics = pPlayer->GetEntity()->GetPhysics();
	if (pPlayerPhysics)
	{
		pArr[pos ++] = pPlayerPhysics;
	}
	ICharacterInstance *pChar = pPlayer->GetEntity()->GetCharacter(0);
	if (pChar)
	{
		ISkeletonPose *pPose = pChar->GetISkeletonPose();
		if (pPose)
		{
			pArr[pos ++] = pPose->GetCharacterPhysics();
		}
	}
}

void CStealthKill::DisableActorCollisions()
{
	if (!m_bCollisionsIgnored)
	{
		IEntity *pTargetEntity = gEnv->pEntitySystem->GetEntity(m_targetId);
		if (pTargetEntity)
		{
			IPhysicalEntity *pPlayerPE = m_pPlayer->GetEntity()->GetPhysics();
			IPhysicalEntity *pTargetPE = pTargetEntity->GetPhysics();
			if (pPlayerPE && pTargetPE)
			{
				pe_player_dynamics ppd;
				ppd.pLivingEntToIgnore = pTargetPE;

				m_bCollisionsIgnored = (pPlayerPE->SetParams(&ppd) != 0);
			}
		}
	}
}

void CStealthKill::EnableActorCollisions()
{
	if (m_bCollisionsIgnored)
	{
		IPhysicalEntity *pPlayerPE = m_pPlayer->GetEntity()->GetPhysics();
		if (pPlayerPE)
		{
			pe_player_dynamics ppd;
			ppd.pLivingEntToIgnore = NULL;
			pPlayerPE->SetParams(&ppd);

			m_bCollisionsIgnored = false;
		}
	}
}
