// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SpectacularKill.h"

#include <iterator>
#include "Player.h"
#include "GameRules.h"
#include "ICooperativeAnimationManager.h"
#include "Game.h"
#include "AutoAimManager.h"
#include "GameRulesModules/IGameRulesDamageHandlingModule.h"
#include "HitDeathReactions.h"
#include "StatsRecordingMgr.h"

#ifndef _RELEASE 
	#define SK_DEBUG_LOG(...) CSpectacularKill::DebugLog(__VA_ARGS__)
#else
	#define SK_DEBUG_LOG(...) {}
#endif

DEFINE_SHARED_PARAMS_TYPE_INFO(CSpectacularKill::SSharedSpectacularParams)

CSpectacularKill::SLastKillInfo CSpectacularKill::s_lastKillInfo;

namespace
{
	const string PERSISTANT_DEBUG_NOT_VALID_ANIM_TAG("SpectacularKill_NotValidAnim");
}

//-----------------------------------------------------------------------
struct CSpectacularKill::SPredNotValidAnim : public std::unary_function<bool, const SSpectacularKillAnimation&>
{
	SPredNotValidAnim(const CSpectacularKill& spectacularKill, const CActor* pTarget) : m_spectacularKill(spectacularKill), m_pTarget(pTarget) {}

	bool operator() (const SSpectacularKillAnimation& killAnim) const
	{
		const SSpectacularKillCVars& skCVars = g_pGameCVars->g_spectacularKill;
		const CActor* pOwner = m_spectacularKill.m_pOwner;

		// 0. the anim shouldn't be redundant
		if (((gEnv->pTimer->GetFrameStartTime().GetSeconds() - s_lastKillInfo.timeStamp) <= skCVars.minTimeBetweenSameKills) &&
			(killAnim.killerAnimation.compare(s_lastKillInfo.killerAnim)))
		{
			SK_DEBUG_LOG("GetValidAnim - %s is not valid: This animation was last played %.1f ago, a minimum time of %.1f is required", 
				killAnim.killerAnimation.c_str(), (gEnv->pTimer->GetFrameStartTime().GetSeconds() - s_lastKillInfo.timeStamp), skCVars.minTimeBetweenSameKills);

			return true;
		}


		// 1. the killer needs to be within a certain distance from the target
		IEntity* pTargetEntity = m_pTarget->GetEntity();
		IEntity* pKillerEntity = pOwner->GetEntity();

		const QuatT& killerTransform = pOwner->GetAnimatedCharacter()->GetAnimLocation();
		const QuatT& targetTransform = m_pTarget->GetAnimatedCharacter()->GetAnimLocation();
		const Vec3& vKillerPos = killerTransform.t;
		const Vec3& vTargetPos = targetTransform.t;

		Vec2 vKillerToTarget = Vec2(vTargetPos) - Vec2(vKillerPos);
		float distance = vKillerToTarget.GetLength();

		const float optimalDistance = killAnim.optimalDist;
		if ((optimalDistance > 0.0f) && (fabs_tpl(distance - optimalDistance) > skCVars.maxDistanceError))
		{
#ifndef _RELEASE
			if (g_pGameCVars->g_spectacularKill.debug > 1)
			{
				// visually shows why it failed
				IPersistantDebug* pPersistantDebug = m_spectacularKill.BeginPersistantDebug();
				const float fConeHeight = killAnim.optimalDist + skCVars.maxDistanceError;
				pPersistantDebug->AddPlanarDisc(vTargetPos, killAnim.optimalDist - skCVars.maxDistanceError, killAnim.optimalDist + skCVars.maxDistanceError, Col_Coral, 6.0f);
				pPersistantDebug->AddLine(vKillerPos, vKillerPos + Vec3(0.f, 0.f, 5.0f), Col_Red, 6.f);
			}

			SK_DEBUG_LOG("GetValidAnim - %s is not valid: Distance between actors should be %.2f, is %.2f (max error is %f)", 
				killAnim.killerAnimation.c_str(), optimalDistance, distance, skCVars.maxDistanceError);
#endif

			return true;
		}


		// 2. The killer needs to be facing the target within cosLookToConeHalfAngleRadians angle
		Vec2 vKillerDir(killerTransform.GetColumn1()); // In decoupled catchup mode we need the animated character's orientation
		vKillerDir.Normalize();
		if (vKillerToTarget.GetNormalizedSafe().Dot(vKillerDir) <= skCVars.minKillerToTargetDotProduct)
		{
			SK_DEBUG_LOG("GetValidAnim - %s is not valid: Killer is not looking within %.2f degrees towards the target", 
				killAnim.killerAnimation.c_str(), RAD2DEG(acos_tpl(skCVars.minKillerToTargetDotProduct) * 2.0f));

			return true;
		}


		// 3. If specified, the killer needs to be within a certain angle range from a given reference orientation from the target
		// e.g. Specifying referenceAngle 180 means using the back of the target as the center of the angle range 
		// (imagine it as a cone) where the killer has to be for the kill to be valid
		if (killAnim.targetToKillerAngle >= 0.f)
		{
			const float referenceAngle = killAnim.targetToKillerAngle;

			// Find the reference vector which will be the center of the allowed angle range
			Vec2 vTargetDir(targetTransform.GetColumn1());
			vTargetDir.Normalize();

			// 2D rotation
			Vec2 vReferenceDir((vTargetDir.x * cosf(referenceAngle)) - (vTargetDir.y * sinf(referenceAngle)), 
				(vTargetDir.y * cosf(referenceAngle)) + (vTargetDir.x * sinf(referenceAngle)));

			if (vKillerToTarget.GetNormalizedSafe().Dot(-vReferenceDir) <= killAnim.targetToKillerMinDot)
			{
#ifndef _RELEASE
				if (g_pGameCVars->g_spectacularKill.debug > 1)
				{
					// visually shows why it failed
					IPersistantDebug* pPersistantDebug = m_spectacularKill.BeginPersistantDebug();
					const float fConeHeight = killAnim.optimalDist + skCVars.maxDistanceError;
					pPersistantDebug->AddCone(vTargetPos + (vReferenceDir * fConeHeight), -vReferenceDir, killAnim.targetToKillerMinDot * fConeHeight * 2.0f, fConeHeight, Col_Coral, 6.f);
					pPersistantDebug->AddLine(vKillerPos, vKillerPos + Vec3(0.f, 0.f, 5.0f), Col_Red, 6.f);
				}

				float targetToKillerDot = vTargetDir.GetNormalizedSafe().Dot(-vKillerToTarget);
				SK_DEBUG_LOG("GetValidAnim - %s is not valid: Killer is not within a %.2f degrees cone centered on the target's %.2f degrees. Killer is at %.2f angles respect the target", 
					killAnim.killerAnimation.c_str(), RAD2DEG(acos_tpl(killAnim.targetToKillerMinDot) * 2.0f), RAD2DEG(killAnim.targetToKillerAngle), RAD2DEG(acos_tpl(targetToKillerDot)));
#endif

				return true;
			}
		}

		SK_DEBUG_LOG("GetValidAnim - %s is valid", killAnim.killerAnimation.c_str());
		return false;
	}

	const CSpectacularKill& m_spectacularKill;
	const CActor* m_pTarget;
};

//-----------------------------------------------------------------------
void CSpectacularKill::DebugLog(const char* szFormat, ...)
{
#ifndef _RELEASE
	if (g_pGameCVars->g_spectacularKill.debug)
	{
		va_list	args;
		va_start(args, szFormat);
		gEnv->pLog->LogV( ILog::eAlways, (string("[SpectacularKill] ") + szFormat).c_str(), args);
		va_end(args);
	}
#endif
}

//-----------------------------------------------------------------------
CSpectacularKill::CSpectacularKill()
: m_pOwner(NULL)
, m_isBusy(false)
, m_targetId(0)
, m_deathBlowState(eDBS_None)
{
}

//-----------------------------------------------------------------------
CSpectacularKill::~CSpectacularKill()
{
	if (IsBusy())
		End(true);
}

//-----------------------------------------------------------------------
void CSpectacularKill::Init(CPlayer* pOwner)
{
	CRY_ASSERT(m_pOwner == NULL);
	if (m_pOwner != NULL)
		return;

	m_pOwner = pOwner;
}

//-----------------------------------------------------------------------
void CSpectacularKill::CleanUp()
{
	stl::free_container(s_lastKillInfo.killerAnim);
}

//-----------------------------------------------------------------------
bool CSpectacularKill::CanExecuteOnTarget(const CActor* pTarget, const SSpectacularKillAnimation& anim) const
{
	CRY_ASSERT(pTarget);

	// can't spectacular kill actors in vehicles
	if(pTarget->GetLinkedVehicle())
		return false;

	// can't spectacular kill when in a hit/death reaction
	if (pTarget->GetActorClass() == CPlayer::GetActorClassType())
	{
		 CHitDeathReactionsConstPtr pHitDeathReactions = static_cast<const CPlayer*>(pTarget)->GetHitDeathReactions();
		 if (pHitDeathReactions && pHitDeathReactions->IsInReaction() && pHitDeathReactions->AreReactionsForbidden())
		 {
			 SK_DEBUG_LOG("Can't start from %s to %s: the target is playing an uninterruptible hit/death reaction", m_pOwner->GetEntity()->GetName(), pTarget->GetEntity()->GetName());

			 return false;
		 }
	}

	if (!CanSpectacularKillOn(pTarget))
		return false;

	// Can't start if they are taking part on other cooperative animation
	ICooperativeAnimationManager* pCooperativeAnimationManager = gEnv->pGameFramework->GetICooperativeAnimationManager();
	if (pCooperativeAnimationManager->IsActorBusy(m_pOwner->GetEntityId()) ||	pCooperativeAnimationManager->IsActorBusy(m_targetId))
	{
		SK_DEBUG_LOG("Can't start from %s to %s: some of them are taking part in a cooperative animation already", m_pOwner->GetEntity()->GetName(), pTarget->GetEntity()->GetName());

		return false;
	}

	const Vec3 vKillerPos = m_pOwner->GetEntity()->GetWorldPos();
	const Vec3 vTargetPos = pTarget->GetEntity()->GetWorldPos();

	// The height distance between killer and victim needs to be acceptably small (simple check for terrain flatness)
	//  [11/08/2010 davidr] ToDo: Use deferred primitive world intersection checks with asset-dependant dimensions for height 
	// and obstacles detection
	if (fabs_tpl(vKillerPos.z - vTargetPos.z) > g_pGameCVars->g_spectacularKill.maxHeightBetweenActors)
	{
		SK_DEBUG_LOG("Can't start from %s to %s: Ground between killer and target is not flat (height distance is %f)", 
			m_pOwner->GetEntity()->GetName(), pTarget->GetEntity()->GetName(), fabs_tpl(vKillerPos.z - vTargetPos.z));

		return false;
	}

	// Obstacle check
	if (ObstacleCheck(vKillerPos, vTargetPos, anim))
	{
		SK_DEBUG_LOG("Can't start from %s to %s: Obstacles have been found between the actors", 
			m_pOwner->GetEntity()->GetName(), pTarget->GetEntity()->GetName());

		return false;
	}

	return true;
}

//-----------------------------------------------------------------------
bool CSpectacularKill::GetValidAnim(const CActor* pTarget, SSpectacularKillAnimation& anim) const
{
	CRY_ASSERT(pTarget);

	bool bSuccess = false;

	const SSpectacularKillParams* pTargetClassParams = GetParamsForClass(pTarget->GetEntity()->GetClass());
	if (pTargetClassParams)
	{
		TSpectacularKillAnimVector validAnims;
		std::remove_copy_if(pTargetClassParams->animations.begin(), pTargetClassParams->animations.end(), std::back_inserter(validAnims), SPredNotValidAnim(*this, pTarget));
		int iNumValidAnims = static_cast<int>(validAnims.size());
		if (iNumValidAnims > 0)
		{
			anim = validAnims[cry_random(0, iNumValidAnims - 1)];
			bSuccess = true;
		}
		else
		{
			DebugLog("Can't start from %s to %s: no animation matches current context", m_pOwner->GetEntity()->GetName(), pTarget->GetEntity()->GetName());
		}
	}
	else
	{
		DebugLog("Can't start from %s to %s: no parameters for target class %s", m_pOwner->GetEntity()->GetName(), pTarget->GetEntity()->GetName(), pTarget->GetEntity()->GetClass()->GetName());
	}

	return bSuccess;
}


//-----------------------------------------------------------------------
void CSpectacularKill::Update(float frameTime)
{
	CActor* pTargetActor = GetTarget();

	if (IsBusy())
	{
		ICooperativeAnimationManager* pCooperativeAnimationManager = gEnv->pGameFramework->GetICooperativeAnimationManager();

		if (!pTargetActor || 
			(!pCooperativeAnimationManager->IsActorBusy(m_pOwner->GetEntityId()) &&
			!pCooperativeAnimationManager->IsActorBusy(pTargetActor->GetEntityId())) ||
			(pTargetActor->IsDead() && (m_deathBlowState == eDBS_None)))
		{
			End();
		}
	}
}

//-----------------------------------------------------------------------
void CSpectacularKill::Reset()
{
	ClearState();
}


//-----------------------------------------------------------------------
void CSpectacularKill::DeathBlow(CActor& targetActor)
{
	CRY_ASSERT_MESSAGE(m_isBusy, "spectacular kill should be in progress when triggering the death blow");
	if (!m_isBusy)
		return;

	if (targetActor.IsDead())
		return;

	Vec3 targetDir = targetActor.GetEntity()->GetForwardDir();

	{
		HitInfo hitInfo;
		hitInfo.shooterId = m_pOwner->GetEntityId();
		hitInfo.targetId = targetActor.GetEntityId();
		hitInfo.damage = 99999.0f; // CERTAIN_DEATH
		hitInfo.dir = targetDir;
		hitInfo.normal = -hitInfo.dir; // this has to be in an opposite direction from the hitInfo.dir or the hit is ignored as a 'backface' hit
		hitInfo.type = CGameRules::EHitType::StealthKill;

		g_pGame->GetGameRules()->ClientHit(hitInfo);
	}

	// WARNING: RagDollize resets the entity's rotation!
	//  [7/30/2010 davidr] FixMe: If the entity isn't dead here (because is immortal or any other reason) ragdollizing it will
	// leave it on an inconsistent state (usually only reproducible on debug scenarios)
	targetActor.GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Ragdoll);

	// Give a small nudge in the hit direction to make the target fall over
	const SSpectacularKillParams* pSpectacularKillParams = GetParamsForClass(targetActor.GetEntity()->GetClass());
	CRY_ASSERT(pSpectacularKillParams);
	if (pSpectacularKillParams && (pSpectacularKillParams->impulseScale > 0.0f) && (pSpectacularKillParams->impulseBone != -1))
	{
		const float killDeathBlowVelocity = pSpectacularKillParams->impulseScale; // desired velocity after impulse in meters per second

		IPhysicalEntity* pTargetPhysics = targetActor.GetEntity()->GetPhysics();
		if (pTargetPhysics)
		{
			pe_simulation_params simulationParams;
			pTargetPhysics->GetParams(&simulationParams);

			pe_action_impulse impulse;
			impulse.partid = pSpectacularKillParams->impulseBone;
			impulse.impulse = targetDir*killDeathBlowVelocity*simulationParams.mass; // RagDollize reset the entity's rotation so I have to use the value I cached earlier
			pTargetPhysics->Action(&impulse);
		}
	}

	m_deathBlowState = eDBS_Done;
}

//-----------------------------------------------------------------------
void CSpectacularKill::End(bool bKillerDied/* = false*/)
{
	CRY_ASSERT_MESSAGE(IsBusy(), "spectacular kill cannot be stopped if it is not in progress");
	if (!IsBusy())
		return;

	ICooperativeAnimationManager* pCooperativeAnimationManager = gEnv->pGameFramework->GetICooperativeAnimationManager();

	CActor* pTarget = GetTarget();
	CRY_ASSERT(pTarget);
	if(pTarget)
	{
		pCooperativeAnimationManager->StopCooperativeAnimationOnActor(m_pOwner->GetAnimatedCharacter(), pTarget->GetAnimatedCharacter());

		// Enable AI again (for what it's worth - this helps editor)
		if (!pTarget->IsPlayer() && pTarget->GetEntity()->GetAI())
			pTarget->GetEntity()->GetAI()->Event(AIEVENT_ENABLE, 0);

		if (bKillerDied && (m_deathBlowState == eDBS_None) && static_cast<CPlayer*> (pTarget)->CanFall())
		{
			// Enable Fall n Play on target if killer dies before death blowing it
			pTarget->Fall();
		}
		else if (m_deathBlowState != eDBS_Done)
		{
			DeathBlow(*pTarget); // Call this in case the notification from the animation system got skipped or missed for some reason
		}

		SActorStats* pTargetStats = pTarget->GetActorStats();
		pTargetStats->spectacularKillPartner = 0;
	}
	else
	{
		pCooperativeAnimationManager->StopCooperativeAnimationOnActor(m_pOwner->GetAnimatedCharacter());
	}

	// Enable AI again (for what it's worth - this helps editor)
	if (m_pOwner && m_pOwner->GetEntity()->GetAI())
		m_pOwner->GetEntity()->GetAI()->Event(AIEVENT_ENABLE, 0);

	ClearState();

  assert(m_pOwner);
	SActorStats* pStats = m_pOwner->GetActorStats();
	if(pStats)
		pStats->spectacularKillPartner = 0;
}

//-----------------------------------------------------------------------
bool CSpectacularKill::StartOnTarget(EntityId targetId)
{
	return StartOnTarget(static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(targetId)));
}

//-----------------------------------------------------------------------
bool CSpectacularKill::StartOnTarget(CActor* pTargetActor)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	CRY_ASSERT(pTargetActor);
	CRY_ASSERT_MESSAGE(!IsBusy(), "spectacular kill should not be initiated while a spectacular kill is already in progress");

	SSpectacularKillAnimation anim;
	if (!IsBusy() && pTargetActor && GetValidAnim(pTargetActor, anim) && CanExecuteOnTarget(pTargetActor, anim))
	{
		// Disable AI
		if (!pTargetActor->IsPlayer() && pTargetActor->GetEntity()->GetAI())
			pTargetActor->GetEntity()->GetAI()->Event(AIEVENT_DISABLE, 0);

		if (!m_pOwner->IsPlayer() && m_pOwner->GetEntity()->GetAI())
			m_pOwner->GetEntity()->GetAI()->Event(AIEVENT_DISABLE, 0);

		// make sure they aren't firing when the anim starts
		{
			IItem* pItem = pTargetActor->GetCurrentItem();
			IWeapon* pWeapon = pItem ? pItem->GetIWeapon() : NULL;
			if (pWeapon)
				pWeapon->StopFire();
		}
		{
			IItem* pItem = m_pOwner->GetCurrentItem();
			IWeapon* pWeapon = pItem ? pItem->GetIWeapon() : NULL;
			if (pWeapon)
				pWeapon->StopFire();
		}

		SActorStats* pStats = m_pOwner->GetActorStats();
		if(pStats)
		{
			pStats->spectacularKillPartner = pTargetActor->GetEntityId();
		}

		SActorStats* pTargetStats = pTargetActor->GetActorStats();
		if(pTargetStats)
		{
			pTargetStats->spectacularKillPartner = m_pOwner->GetEntityId();
		}

		const float slideTime = 0.2f;

		const char* pKillerAnim = anim.killerAnimation.c_str();
		const char* pVictimAnim = anim.victimAnimation.c_str();

		SCharacterParams killerParams(m_pOwner->GetAnimatedCharacter(), pKillerAnim, /*allowHPhysics*/false, slideTime);
		SCharacterParams targetParams(pTargetActor->GetAnimatedCharacter(), pVictimAnim, /*allowHPhysics*/false, slideTime);
		SCooperativeAnimParams animParams(/*forceStart*/true, /*looping*/ false, /*alignment*/ /*eAF_FirstActorNoRot*/eAF_FirstActor);
		animParams.bIgnoreCharacterDeath = true;
		animParams.bPreventFallingThroughTerrain = false;
		animParams.bNoCollisionsBetweenFirstActorAndRest = true;

		ICooperativeAnimationManager* pCooperativeAnimationManager = gEnv->pGameFramework->GetICooperativeAnimationManager();
		bool bStarted = pCooperativeAnimationManager->StartNewCooperativeAnimation(killerParams, targetParams, animParams);
		if (bStarted)
		{
			m_targetId = pTargetActor->GetEntityId();
			m_isBusy = true;

			// Register the killing
			s_lastKillInfo.killerAnim = pKillerAnim;
			s_lastKillInfo.timeStamp = gEnv->pTimer->GetFrameStartTime().GetSeconds();

#ifndef _RELEASE
			// Clean persistant debug information
			IPersistantDebug* pPersistantDebug = BeginPersistantDebug();

			// Send telemetry event
			CStatsRecordingMgr* pRecordingMgr = g_pGame->GetStatsRecorder();
			IStatsTracker* pTracker = pRecordingMgr ? pRecordingMgr->GetStatsTracker(m_pOwner) : NULL;
			if (pTracker)
			{
				EGameStatisticEvent eventType = eGSE_SpectacularKill;

				if(pRecordingMgr->ShouldRecordEvent(eventType, m_pOwner))
				{
					pTracker->Event(eventType, (anim.killerAnimation + " -> " + anim.victimAnimation).c_str()); 
				}
			}
#endif

			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------
bool CSpectacularKill::AnimationEvent(ICharacterInstance *pCharacter, const AnimEventInstance &event)
{
	if (!m_isBusy)
		return false;

	if (pCharacter == m_pOwner->GetEntity()->GetCharacter(0))
	{
		if (m_pOwner->GetAnimationEventsTable().m_deathBlow == event.m_EventNameLowercaseCRC32)
		{
			// If a custom parameter exists and != 0 then we keep playing the cooperative animation but mark the
			// character as dead (so if the action gets interrupted before the deathblow the character dies instead of
			// fall and play)
			if (m_deathBlowState != eDBS_Done)
			{
				if (event.m_CustomParameter && (event.m_CustomParameter[0]) && (event.m_CustomParameter[0] == '1'))
				{
					m_deathBlowState = eDBS_Pending;
				}
				else
				{
					if (CActor* pTargetActor = GetTarget())
					{
						DeathBlow(*pTargetActor);
					}
				}
			}

			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------
const CSpectacularKill::SSpectacularKillParams* CSpectacularKill::GetParamsForClass( IEntityClass* pTargetClass ) const
{
	CRY_ASSERT_TRACE(m_pParams != NULL, ("Trying to get params from a spectacular kill object without params. Check if entity class %s has a param file in Scripts/actor/parameters", m_pOwner->GetEntityClassName()));
	if (m_pParams)
	{
		CRY_ASSERT(pTargetClass);

		TSpectacularKillParamsVector::const_iterator itEnd = m_pParams->paramsList.end();
		for (TSpectacularKillParamsVector::const_iterator it = m_pParams->paramsList.begin(); it != itEnd; ++it)
		{
			if (pTargetClass == it->pEnemyClass)
			{
				return &(*it);
			}
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------
bool CSpectacularKill::CanSpectacularKillOn(EntityId targetId) const
{
	return CanSpectacularKillOn(static_cast<const CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(targetId)));
}

//-----------------------------------------------------------------------
bool CSpectacularKill::CanSpectacularKillOn(const CActor* pTarget) const
{
	const SSpectacularKillCVars& skCVars = g_pGameCVars->g_spectacularKill;

	bool bReturn = false;

	if (pTarget)
	{
		IEntity* pTargetEntity = pTarget->GetEntity();
		// Check params existence for the target class
		const SSpectacularKillParams* pTargetParams = GetParamsForClass(pTargetEntity->GetClass());
		if (pTargetParams)
		{
			// Check redundancy, number of anims
			const float fElapsedSinceLastKill = gEnv->pTimer->GetFrameStartTime().GetSeconds() - s_lastKillInfo.timeStamp;
			if (fElapsedSinceLastKill > skCVars.minTimeBetweenKills)
			{
				const int numAnims = static_cast<int>(pTargetParams->animations.size());
				if (numAnims > 0)
				{
					const SSpectacularKillAnimation& animData = pTargetParams->animations[0];
					if ((numAnims > 1) || ((fElapsedSinceLastKill > skCVars.minTimeBetweenSameKills) ||
						(animData.killerAnimation.compare(s_lastKillInfo.killerAnim) != 0)))
					{
						// Check distance with the player
						IEntity* pLocalPlayerEntity = gEnv->pEntitySystem->GetEntity(g_pGame->GetIGameFramework()->GetClientActorId());
						CRY_ASSERT(pLocalPlayerEntity);
						if (!pLocalPlayerEntity || (skCVars.sqMaxDistanceFromPlayer < 0.0f) || 
							(pLocalPlayerEntity->GetWorldPos().GetSquaredDistance(pTarget->GetEntity()->GetWorldPos()) < skCVars.sqMaxDistanceFromPlayer))
						{
							// Check if target is not invulnerable
							bool bIsInvulnerable = false;
							if (IScriptTable* pTargetScript = pTargetEntity->GetScriptTable())
							{
								HSCRIPTFUNCTION isInvulnerableFunc = NULL;
								if (pTargetScript->GetValue("IsInvulnerable", isInvulnerableFunc) && isInvulnerableFunc)
									Script::CallReturn(gEnv->pScriptSystem, isInvulnerableFunc, pTargetScript, bIsInvulnerable);
							}
							if(!bIsInvulnerable)
							{
								// Check player's visTable to maximize the opportunities for him to actually see this?
								bReturn = true;
							}
						}
					}
				}
			}
		}
	}

	return bReturn;
}

//-----------------------------------------------------------------------
void CSpectacularKill::ReadXmlData( const IItemParamsNode* pRootNode)
{
	CRY_ASSERT(pRootNode);

	ISharedParamsManager* pSharedParamsManager = gEnv->pGameFramework->GetISharedParamsManager();
	CRY_ASSERT(pSharedParamsManager);

	// If we change the SharedParamsManager to accept CRCs on its interface we could compute this once and store
	// the name's CRC32 instead of constructing it here each time this method is invoked (it shouldn't be invoked 
	// too often, though)
	const char* szEntityClassName = m_pOwner->GetEntityClassName();
	CryFixedStringT<64>	sharedParamsName;
	sharedParamsName.Format("%s_%s", SSharedSpectacularParams::s_typeInfo.GetName(), szEntityClassName);

	ISharedParamsConstPtr pSharedParams = pSharedParamsManager->Get(sharedParamsName);
	if (pSharedParams)
	{
		m_pParams = CastSharedParamsPtr<SSharedSpectacularParams>(pSharedParams);
		return;
	}

	m_pParams.reset();

	const IItemParamsNode* pParams = pRootNode->GetChild("SpectacularKill");
	if (pParams)
	{
		SSharedSpectacularParams newParams;

		const int childCount = pParams->GetChildCount();
		newParams.paramsList.reserve(childCount);

		for (int i = 0; i < childCount; ++i)
		{
			const IItemParamsNode* pTargetParams = pParams->GetChild(i);
			CRY_ASSERT(pTargetParams);

			IEntityClass* pTargetClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(pTargetParams->GetName());
			if (pTargetClass)
			{
				SSpectacularKillParams targetParams;
				const IItemParamsNode* pChildParamsNode = pTargetParams->GetChild("Params");
				const IItemParamsNode* pChildAnimsNode = pTargetParams->GetChild("Anims");

				targetParams.pEnemyClass = pTargetClass;

				if(pChildParamsNode)
				{
					pChildParamsNode->GetAttribute("impulseScale", targetParams.impulseScale);

					const char* szImpulseBone = pChildParamsNode->GetAttributeSafe("impulseBone");
					ICharacterInstance* pCharacter = m_pOwner->GetEntity()->GetCharacter(0);
					targetParams.impulseBone = pCharacter ? pCharacter->GetIDefaultSkeleton().GetJointIDByName(szImpulseBone) : -1;
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
							SSpectacularKillAnimation newAnimation;

							newAnimation.victimAnimation = pAnimNode->GetAttributeSafe("victimAnimation");
							newAnimation.killerAnimation = pAnimNode->GetAttributeSafe("killerAnimation");

							pAnimNode->GetAttribute("optimalDist", newAnimation.optimalDist);

							if (pAnimNode->GetAttribute("targetToKillerAngle", newAnimation.targetToKillerAngle))
								newAnimation.targetToKillerAngle = DEG2RAD(newAnimation.targetToKillerAngle);

							if (pAnimNode->GetAttribute("targetToKillerAngleRange", newAnimation.targetToKillerMinDot))
								newAnimation.targetToKillerMinDot = cos_tpl(DEG2RAD(newAnimation.targetToKillerMinDot) / 2.0f);

							pAnimNode->GetAttribute("obstacleCheckStartOffset", newAnimation.vKillerObstacleCheckOffset);
							pAnimNode->GetAttribute("obstacleCheckLength", newAnimation.fObstacleCheckLength);

							targetParams.animations.push_back(newAnimation);
						}
					}
				}

				CRY_ASSERT_MESSAGE(targetParams.animations.size() > 0, string().Format("No Animations defined for %s spectacular kill", pTargetClass->GetName()));

				newParams.paramsList.push_back(targetParams);
			}
#ifdef SPECTACULAR_KILL_DEBUG
			else
			{
				GameWarning("spectacular Kill: Couldn't find entity of class '%s', skipping", pTargetParams->GetName());
			}
#endif
		}

		m_pParams = CastSharedParamsPtr<SSharedSpectacularParams>(pSharedParamsManager->Register(sharedParamsName, newParams));
	}
}

//-----------------------------------------------------------------------
const CActor* CSpectacularKill::GetTarget() const
{
	return static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_targetId));
}

//-----------------------------------------------------------------------
void CSpectacularKill::ClearState()
{
	m_isBusy = false;
	m_targetId = 0;
	m_deathBlowState = eDBS_None;
}

//-----------------------------------------------------------------------
bool CSpectacularKill::ObstacleCheck(const Vec3& vKillerPos, const Vec3& vTargetPos, const SSpectacularKillAnimation& anim) const
{
	// [*DavidR | 13/Sep/2010] ToDo: Find a way to make this asynchronously
	const float OBSTACLE_CHECK_RADIUS = 0.6f;
	const float OBSTACLE_CHECK_GROUND_OFFSET = 0.2f;

	const Vec3& vCapsuleKillerEnd = vKillerPos + anim.vKillerObstacleCheckOffset;

	primitives::capsule capsPrim;
	const Vec3& vKillerToTargetDist = vTargetPos - vCapsuleKillerEnd;
	capsPrim.axis = vKillerToTargetDist.GetNormalized();
	capsPrim.r = OBSTACLE_CHECK_RADIUS;

	// hh is actually half the total height (it's measured from the center)
	capsPrim.hh = static_cast<float>(__fsel(anim.fObstacleCheckLength, 
				(anim.fObstacleCheckLength * 0.5f) - capsPrim.r,
				(min(g_pGameCVars->g_spectacularKill.maxDistanceError, vKillerToTargetDist.GetLength()) * 0.5f) - capsPrim.r));

	capsPrim.center = vCapsuleKillerEnd + (capsPrim.axis * (capsPrim.hh + capsPrim.r));
	capsPrim.center.z += capsPrim.r + OBSTACLE_CHECK_GROUND_OFFSET;

	geom_contact* pContact = NULL;
	int collisionEntityTypes = ent_static | ent_terrain | ent_sleeping_rigid | ent_ignore_noncolliding;
	float d = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(capsPrim.type, &capsPrim, Vec3Constants<float>::fVec3_Zero, collisionEntityTypes, &pContact, 0, geom_colltype0);

	bool bObstacleFound = (d != 0.0f) && pContact;

#ifndef _RELEASE
	if (bObstacleFound && (g_pGameCVars->g_spectacularKill.debug > 1))
	{
		const float fTime = 6.0f;

		// visually show why it failed
		IPersistantDebug* pPersistantDebug = BeginPersistantDebug();

		// Draw a capsule using a cylinder and two spheres
		const ColorF debugColor = Col_Coral * ColorF(1.0f, 1.0f, 1.0f, 0.6f);
		pPersistantDebug->AddCylinder(capsPrim.center, capsPrim.axis, capsPrim.r, capsPrim.hh * 2.0f, debugColor, fTime);
		pPersistantDebug->AddSphere(capsPrim.center - (capsPrim.axis * capsPrim.hh), capsPrim.r, debugColor, fTime);
		pPersistantDebug->AddSphere(capsPrim.center + (capsPrim.axis * capsPrim.hh), capsPrim.r, debugColor, fTime);

		for (int i = 0; i < (int)d; ++i)
		{
		// Draw the collision point
			geom_contact collisionData(pContact[i]);
		pPersistantDebug->AddCone(collisionData.pt, -collisionData.n, 0.1f, 0.8f, Col_Red, fTime + 2.0f);
	}
	}
#endif

	return bObstacleFound;
}

//-----------------------------------------------------------------------
IPersistantDebug*	CSpectacularKill::BeginPersistantDebug() const
{
#ifndef _RELEASE
	IPersistantDebug* pPersistantDebug = g_pGame->GetIGameFramework()->GetIPersistantDebug();
	pPersistantDebug->Begin((PERSISTANT_DEBUG_NOT_VALID_ANIM_TAG + m_pOwner->GetEntity()->GetName()).c_str(), true);

	return pPersistantDebug;
#else
	return NULL;
#endif
}
