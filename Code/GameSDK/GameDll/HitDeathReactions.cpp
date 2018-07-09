// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "HitDeathReactions.h"
#include "HitDeathReactionsSystem.h"
#include "ScriptBind_HitDeathReactions.h"

#include <CryAnimation/IFacialAnimation.h>
#include <CooperativeAnimationManager/CooperativeAnimationManager.h>
#include "Item.h"
#include "WeaponSystem.h"
#include <CryAISystem/IAIActor.h>
#include <CryAISystem/INavigation.h>
#include "GameCVars.h"
#include "GameRules.h"
#include "StatsRecordingMgr.h"
#include "Player.h"
#include "PlayerAnimation.h"

#include "GameCodeCoverage/GameCodeCoverageTracker.h"

#include "ActionCoopAnim.h"
#include "ProceduralContextRagdoll.h"

// As defined in SkeletonAnim.h (no other way to know the number of animation layers)
#define numVIRTUALLAYERS (0x10)

#ifndef _RELEASE
#define HITDEATH_DEBUG
#endif //_RELEASE

#ifdef HITDEATH_DEBUG
#define HitDeathLog(...) { CryLogAlways("[HITDEATHREACTIONS]" __VA_ARGS__); }
#else
#define HitDeathLog(...) {}
#endif //HITDEATH_DEBUG


// Unnamed namespace for constants
namespace
{
	const char DEFAULT_KILL_REACTION_FUNCTION[] = "DefaultKillReaction";
	const char DEFAULT_HIT_REACTION_FUNCTION[] = "DefaultHitReaction";
	const char DEFAULT_VALIDATION_FUNCTION[] = "DefaultValidation";

	const char AI_SIGNAL_ONHIT_MOUNTED_GUN[] = "OnHitMountedGun";
	const char AI_SIGNAL_FINISHED_REACTION[] = "OnFinishedHitDeathReaction";
	const char AI_SIGNAL_INTERRUPTED_REACTION[] = "OnHitDeathReactionInterrupted";

	const char DEAD_FACIAL_EXPRESSION[] = "Exp_Dead";
	const char PAIN_FACIAL_EXPRESSION[] = "Exp_Pain";
	const uint32 INVALID_FACIAL_CHANNEL_ID = ~0;

	const uint32 ANIM_USER_TOKEN = 'HdRA';
}

class CHitDeathPlayerAction : public TPlayerAction
{
	typedef TPlayerAction TBaseAction;
public:
	DEFINE_ACTION("HitDeath")

	CHitDeathPlayerAction(bool isDeathReaction, FragmentID fragmentID = FRAGMENT_ID_INVALID, const TagState fragTags = TAG_STATE_EMPTY)
	: TBaseAction(isDeathReaction ? PP_Death : PP_HitReaction, fragmentID, fragTags, isDeathReaction ? IAction::NoAutoBlendOut : 0)
	, m_isDeathReaction(isDeathReaction)
	, m_forceRagdollAwake(true)
	{
	}

protected:

	virtual void OnFragmentStarted() override
	{
		SetAnimationControlledMovement();
	}

	virtual void Exit() override
	{
		NotifyReactionAnimationEnd();
		SetEntityControlledMovement();

		if( gEnv->bMultiplayer && m_isDeathReaction )
		{
			const EntityId entID = m_rootScope->GetEntityId();
			IActor* piActor = gEnv->pGameFramework->GetIActorSystem()->GetActor( entID );
			if( piActor )
			{
				if( gEnv->bServer )
				{
					piActor->GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Ragdoll);
				}
				else
				{
					static_cast<CPlayer*>(piActor)->CPlayer::RagDollize( false );
				}
			}
		}

		TBaseAction::Exit();
	}

	virtual EStatus Update(float elapsedTime) override
	{
		IActor* piActor = GetRootActor();
		if (!gEnv->bMultiplayer && m_isDeathReaction && piActor && !piActor->IsPlayer() && GetRootScope().CalculateFragmentTimeRemaining() < 0.1f )
		{
			IProceduralContext* piProcContext = m_rootScope->GetActionController().FindOrCreateProceduralContext(CProceduralContextRagdoll::GetCID());
			if( piProcContext && !static_cast<CProceduralContextRagdoll*> (piProcContext)->IsInRagdoll() )
			{
				static_cast<CPlayer*>(piActor)->CPlayer::RagDollize( false );
			}
		}

		return TBaseAction::Update( elapsedTime );
	}

	virtual void OnSequenceFinished(int layer, uint32 scopeID) override
	{
		if (GetRootScope().GetID() == scopeID)
		{
			if( layer == 0 && m_isDeathReaction )
			{
				if( gEnv->bMultiplayer )
				{
					ForceFinish();
				}
				else
				{
					IActor* piActor = GetRootActor();
					if( piActor && !piActor->IsPlayer() && !piActor->IsClient() )
					{
						IProceduralContext* piProcContext = m_rootScope->GetActionController().FindOrCreateProceduralContext(CProceduralContextRagdoll::GetCID());
						if( piProcContext )
						{
							static_cast<CProceduralContextRagdoll*> (piProcContext)->ForceRagdollFinish(piActor, true);
						}
					}
				}
			}
			m_forceRagdollAwake = false;
		}
	}

private:
	bool m_isDeathReaction;
	bool m_forceRagdollAwake;

	IActor* GetRootActor() const
	{
		const EntityId entityId = GetRootScope().GetEntityId();
		IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(entityId);
		return pActor;
	}

	void NotifyReactionAnimationEnd()
	{
		IActor* pActor = GetRootActor();
		if (pActor && !m_isDeathReaction)
		{
			SGameObjectEvent event(eCGE_ReactionEnd, eGOEF_ToExtensions);
			event.param = this;
			pActor->HandleEvent(event);
		}
	}

	void SetAnimationControlledMovement()
	{
		SetMovementControlMethods(eMCM_AnimationHCollision, eMCM_Entity);
	}

	void SetEntityControlledMovement()
	{
		SetMovementControlMethods(eMCM_Entity, eMCM_Entity);
	}

	void SetMovementControlMethods(const EMovementControlMethod horizontalMcm, const EMovementControlMethod verticalMcm)
	{
		IActor* pActor = GetRootActor();
		if (pActor)
		{
			IAnimatedCharacter* pAnimatedCharacter = pActor->GetAnimatedCharacter();
			if (pAnimatedCharacter)
			{
				pAnimatedCharacter->SetMovementControlMethods(horizontalMcm, verticalMcm);
			}
		}
	}
};


class CAnimActionRagdoll : public TPlayerAction
{
	typedef TPlayerAction TBaseAction;
public:
	DEFINE_ACTION("Ragdoll");

	CAnimActionRagdoll(bool isForced, FragmentID fragmentID = FRAGMENT_ID_INVALID, const TagState fragTags = TAG_STATE_EMPTY)
		: TBaseAction(isForced ? PP_PostDeath : PP_Death, fragmentID, fragTags, IAction::NoAutoBlendOut, isForced ? 0xffffffff : 0u)
		, m_isForced(isForced)
		, m_timeInRagdoll(0.0f)
		, m_timeToEndRagdoll(-1.0f)
	{
	}

	virtual EStatus Update(float timePassed) override
	{
		m_timeInRagdoll += timePassed;

		if( (m_timeToEndRagdoll > 0.0f) && (m_timeInRagdoll > m_timeToEndRagdoll) )
		{
			IPhysicalEntity* pent = GetRootScope().GetEntity().GetPhysics();
			if( pent && pent->GetType() == PE_ARTICULATED )
			{
				pe_params_articulated_body params;

				// Forces the ragdoll to rest
				params.minEnergyLyingMode = 10.0f;
				pent->SetParams( &params );

				m_timeToEndRagdoll = -1.f;
			}
		}

		return TBaseAction::Update(timePassed);
	}

	virtual EStatus UpdatePending(float timePassed) override
	{
		bool bUseFallback = true;

		if( m_rootScope && !m_isForced )
		{
			SFragTagState fragTagState( TAG_STATE_FULL, m_fragTags );
			SFragTagState matchedTagState;
			const uint32 numMatchingTags = m_rootScope->GetDatabase().FindBestMatchingTag( SFragmentQuery(m_fragmentID, fragTagState), &matchedTagState );
			if( numMatchingTags != 0 )
			{
				const bool containsTags = m_rootScope->GetDatabase().GetFragmentDefs().GetSubTagDefinition( m_fragmentID )->Contains( m_fragTags, matchedTagState.fragmentTags );
				if( containsTags )
				{
					bUseFallback = false;
				}
			}
		}
		if( m_rootScope && (bUseFallback || m_isForced) )
		{
			IProceduralContext* piProcContext = m_rootScope->GetActionController().FindOrCreateProceduralContext(CProceduralContextRagdoll::GetCID());
			static_cast<CProceduralContextRagdoll*> (piProcContext)->EnableRagdoll( m_rootScope->GetEntityId(), false, 20.f );
		}
		return TBaseAction::UpdatePending(timePassed);
	}

	void InitEndRagdollTime( float fTime )
	{
		m_timeToEndRagdoll = fTime;
	}

protected:
	float m_timeInRagdoll;
	float m_timeToEndRagdoll;
	bool m_isForced;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct CHitDeathReactions::SPredFindValidReaction : public std::unary_function<bool, const SReactionParams&>
{
	SPredFindValidReaction(const CHitDeathReactions& owner, const HitInfo& hitInfo, float fCausedDamage = 0.0f) : m_owner(owner), m_hitInfo(hitInfo), m_fCausedDamage(fCausedDamage) {}

	bool operator ()(const SReactionParams& reactionParams) const
	{
		typedef SReactionParams::ValidationParamsList::const_iterator validationIt;

		bool bFound = false;
		validationIt itEnd = reactionParams.validationParams.end();
		for (validationIt it = reactionParams.validationParams.begin(); (it != itEnd) && !bFound; ++it)
		{
			bFound = EvaluateValidationParams(*it, reactionParams);
		}

		if( bFound && gEnv->bMultiplayer && reactionParams.mannequinData.actionType == SReactionParams::SMannequinData::EActionType_Coop )
		{
			reactionParams.mannequinData.m_animIndexMP = 1;

			SFragTagState fragTagState( TAG_STATE_FULL, reactionParams.mannequinData.tagState );
			uint32 numAnims = 0u;
			const IAnimationDatabase* piOptionalAnimationADB = m_owner.m_pHitDeathReactionsConfig->piOptionalAnimationADB;
			if ( piOptionalAnimationADB != NULL )
			{
				SFragTagState matchedTagState; 
				const uint32 numAnimsPotential = piOptionalAnimationADB->FindBestMatchingTag( SFragmentQuery(PlayerMannequin.fragmentIDs.hitDeath, fragTagState), &matchedTagState );
				const bool containsTags = piOptionalAnimationADB->GetFragmentDefs().GetSubTagDefinition( PlayerMannequin.fragmentIDs.hitDeath )->Contains( fragTagState.fragmentTags, matchedTagState.fragmentTags );
				if( containsTags )
				{
					numAnims = numAnimsPotential;
				}
			}
			else
			{
				HitDeathLog("CHitDeathReactions::SPredFindValidReaction: Trying to use cooperative/slaved mannequin animation but no slave animation ADB is present! (Check your 'manqReaction' XML tags in the HitDeath config!)");
				assert(false);
			}
			if( numAnims )
			{
				reactionParams.mannequinData.m_animIndexMP = static_cast<int>(m_owner.GetRandomProbability()*100.f) % numAnims;
			}
		}

		return reactionParams.validationParams.empty() || bFound;
	}

	bool EvaluateValidationParams(const SReactionParams::SValidationParams& validationParams, const SReactionParams& reactionParams) const
	{
		bool bResult = false;
		bool bSuccess = false;

		if (!validationParams.sCustomValidationFunc.empty())
		{
			const CCustomReactionFunctions& customReactions = g_pGame->GetHitDeathReactionsSystem().GetCustomReactionFunctions();
			bSuccess = customReactions.CallCustomValidationFunction(bResult, m_owner.m_pSelfTable, m_owner.m_actor, validationParams, m_hitInfo, m_fCausedDamage);
		}

		if (!bSuccess)
		{
			if (g_pGameCVars->g_hitDeathReactions_useLuaDefaultFunctions)
			{
				HSCRIPTFUNCTION validationFunc = NULL;
				if (m_owner.m_pSelfTable->GetValue(DEFAULT_VALIDATION_FUNCTION, validationFunc))
				{
					ScriptTablePtr scriptHitInfo(m_owner.m_pScriptSystem);
					g_pGame->GetGameRules()->CreateScriptHitInfo(scriptHitInfo, m_hitInfo);

					bSuccess = Script::CallReturn(m_owner.m_pScriptSystem, validationFunc, m_owner.m_pSelfTable, validationParams.validationParamsScriptTable, scriptHitInfo, m_fCausedDamage, bResult);
					m_owner.m_pScriptSystem->ReleaseFunc(validationFunc);
				}
				else
					CHitDeathReactionsSystem::Warning("Couldn't find default LUA method (method HitDeathReactions:%s)", DEFAULT_VALIDATION_FUNCTION);
			}

			if (!bSuccess)
			{
				// Default C++ validation
				bResult = m_owner.IsValidReaction(m_hitInfo, validationParams, m_fCausedDamage);

				if( bResult )
				{
					if( reactionParams.mannequinData.actionType == SReactionParams::SMannequinData::EActionType_Fragment )
					{
						if( reactionParams.mannequinData.GetCurrentReactionAnimIndex() == OPTION_IDX_INVALID )
						{
							bResult = false;
						}
					}
				}
			}
		}

		return bResult;
	}

private:
	const CHitDeathReactions& m_owner;
	const HitInfo&						m_hitInfo;
	float											m_fCausedDamage;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::SCustomAnim::SetAnimName(int animID, IAnimationSet* pAnimSet)
{
	sAnimName = pAnimSet->GetNameByAnimID(animID);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
inline void AnimIDError_Dup(const char *animName)
{
	if (g_pGameCVars->g_animatorDebug)
	{
		static const ColorF col (1.0f, 0.0f, 0.0f, 1.0f);
		g_pGame->GetIGameFramework()->GetIPersistantDebug()->Add2DText(string().Format("Missing %s", animName).c_str(), 1.0f, col, 10.0f);
	}

	CHitDeathReactionsSystem::Warning("Missing anim: %s", animName);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CHitDeathReactions::CHitDeathReactions(CActor& actor) : m_pScriptSystem(gEnv->pScriptSystem), m_actor(actor), 
m_fReactionCounter(-1.0f), m_iGoalPipeId(0), m_AIPausedState(eAIP_NotPaused), m_currentReaction(eRT_None), m_fReactionEndTime(-1.0f), 
m_pHitInfo(NULL), m_reactionOnCollision(NO_COLLISION_REACTION), m_currentExecutionType(eET_None),
m_reactionFlags(0), m_effectorChannel(INVALID_FACIAL_CHANNEL_ID), m_pCurrentReactionParams(NULL), m_pseudoRandom(g_pGame->GetHitDeathReactionsSystem().GetRandomGenerator()),
m_profileId(INVALID_PROFILE_ID), m_postDeathTagRagdoll(TAG_STATE_EMPTY),
m_bInSmartObject(false)
{
	m_pSelfTable.Create(m_pScriptSystem);

	//LoadData(m_pSelfTable, false);

	OnActorReused();

	m_primitiveIntersectionQueue.InitWithOwner(this);

	if (gEnv->bServer && gEnv->pAISystem)
	{
		m_iGoalPipeId = gEnv->pAISystem->AllocGoalPipeId();
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::OnActorReused()
{
	// If the actor was on a hit reaction and the new actor is not valid (e.g., a PoolEntity) this could lead to 
	// crashes. To avoid it, just finish the reaction now.
	if (IsInReaction())
		EndCurrentReaction();

	LoadData(m_pSelfTable, true);

	// Add the methods from the scriptbind file to a "binds" table
	SmartScriptTable binds(m_pScriptSystem);
	binds->Delegate(g_pGame->GetHitDeathReactionsScriptBind()->GetMethodsTable());
	binds->SetValue("__actor", ScriptHandle(m_actor.GetEntityId()));
	m_pSelfTable->SetValue("binds", binds);

	// adding a hitDeathReactions subtable to the actor script so it can access it if needed 
	ScriptTablePtr pActorScriptTable = m_actor.GetEntity()->GetScriptTable();
  if (pActorScriptTable)
    pActorScriptTable->SetValue("hitDeathReactions", m_pSelfTable);

	// We need to specify eRRF_Alive. Entities may be reused from the pool before having their HitDeathReactions
	// Instantiated, so when the CPlayer::Revive() won't be able to call the OnRevive method. It's sure that any
	// entity being reused is also alive, though, since PostReloadExtension always calls Revive, so the line
	// below is perfectly safe
	RequestReactionAnims(eRRF_OutFromPool | eRRF_Alive);
}

//////////////////////////////////////////////////////////////////////////
// Called right after the actor's entity is returned to the entity pool
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::OnActorReturned()
{
	// The actors returned to the pool will get their entityId changed, so 
	// we need to totally remove them from the list of actors using the profile
	g_pGame->GetHitDeathReactionsSystem().ReleaseReactionAnimsForActor(m_actor, eRRF_Alive | eRRF_OutFromPool | eRRF_AIEnabled);
	m_profileId = INVALID_PROFILE_ID;
	m_bInSmartObject = false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::FullSerialize( TSerialize ser )
{
	if (ser.IsReading())
	{
		ClearState();

		bool bShouldTriggerDeathRagdoll = false;
		ser.Value("shouldTriggerDeathRagdoll", bShouldTriggerDeathRagdoll, 'bool');
		if (bShouldTriggerDeathRagdoll)
		{
			// We need to enable the ragdoll after serialization. The GameObject serialization thinks this
			// character's physic aspect profile is "alive" (since that's what it saved, despite being on a death 
			// reaction that will forcefully end up in ragdoll) so it will force that physic aspect profile
			// during the serialization
			m_reactionFlags |= SReactionParams::TriggerRagdollAfterSerializing;
		}
	}
	else
	{	
		bool bShouldTriggerDeathRagdoll = IsInDeathReaction() && DeathReactionEndsInRagdoll();
		ser.Value("shouldTriggerDeathRagdoll", bShouldTriggerDeathRagdoll, 'bool');
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::PostSerialize()
{
	if (m_reactionFlags & SReactionParams::TriggerRagdollAfterSerializing)
	{
		m_reactionFlags &= ~SReactionParams::TriggerRagdollAfterSerializing;

		m_actor.GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Ragdoll);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::RequestReactionAnims(uint32 requestFlags)
{
	// Never request anims for a pool entity
	if (!m_actor.IsPoolEntity())
		g_pGame->GetHitDeathReactionsSystem().RequestReactionAnimsForActor(m_actor, requestFlags);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::ReleaseReactionAnims(uint32 requestFlags)
{
	// Never release anims for a pool entity
	if (!m_actor.IsPoolEntity())
		g_pGame->GetHitDeathReactionsSystem().ReleaseReactionAnimsForActor(m_actor, requestFlags);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::Update(float fFrameTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	if (IsInReaction())
	{
#ifdef _DEBUG
		// Debug check: If marked to be disabled, the AI SHOULD be disabled while playing a reaction
		if (eAIP_PipeUserPaused == m_AIPausedState)
		{
			IPipeUser* pPipeUser = GetAIPipeUser();
			CRY_ASSERT_TRACE(!pPipeUser || pPipeUser->IsPaused(), ("The actor %s pipe user has been resumed before the reaction has ended! Investigate", m_actor.GetEntity()->GetName()));
		}
#endif

		// Protection against bad animation graph setups or other errors
		m_fReactionCounter += fFrameTime;
		if ((m_fReactionEndTime >= 0.0f) && (m_fReactionCounter > m_fReactionEndTime))
		{
			EndCurrentReactionInternal(true, false);
		}
		else if (m_fReactionCounter > m_pHitDeathReactionsConfig->fMaximumReactionTime)
		{
			if( m_currentExecutionType != eET_Mannequin )
			{
#ifndef _RELEASE
				const char* szExecutionType = NULL;
				switch (m_currentExecutionType)
				{
				case eET_None: szExecutionType= "Undefined"; break;
				case eET_Custom: szExecutionType= "Custom"; break;
				case eET_AnimationGraphAnim: szExecutionType= (string("AnimationGraph(") + GetCurrentAGReactionAnimName() + ")").c_str(); break;
				case eET_ReactionAnim: szExecutionType= (string("ReactionAnim(") + m_currentCustomAnim.GetAnimName() + ")").c_str(); break;
				case eET_Mannequin: szExecutionType= (string("Mannequin(") + "SomeTags" + ")").c_str(); break;
				default: break;
				}
				CHitDeathReactionsSystem::Warning("Current %s %s reaction for entity %s was" 
					" taking too long (more than %.2f seconds) so its end was forced. Talk with David Ramos if its long duration was intended", 
					IsInDeathReaction() ? "Death" : "Hit", szExecutionType, m_actor.GetEntity()->GetName(), m_pHitDeathReactionsConfig->fMaximumReactionTime);
#endif

				EndCurrentReactionInternal(false, false);
			}
		}
		else if (m_reactionOnCollision != NO_COLLISION_REACTION)
		{
			DoCollisionCheck();
		}
	}

	if (CustomAnimHasFinished())
	{
		// If no animation graph is working on that layer stop the animation, otherwise it will be played there forever
		OnCustomAnimFinished(false);
	}

#ifndef _RELEASE
	if (g_pGameCVars->g_hitDeathReactions_debug)
	{
		DrawDebugInfo();
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::OnRagdollize( bool forceFallback )
{
	IActionController* piActionController = m_actor.GetAnimatedCharacter()->GetActionController();

	if( piActionController )
	{
		const FragmentID fragID = GetHitDeathFragmentID( piActionController );

		

		IAction* piAction = NULL;
		if( forceFallback )
		{
			piAction = new CAnimActionRagdoll( forceFallback );
			static_cast<CAnimActionRagdoll*> (piAction)->InitEndRagdollTime( m_pHitDeathReactionsConfig->fEndRagdollTime );
		}
		else
		{
			piAction = new CAnimActionRagdoll( forceFallback, fragID, m_postDeathTagRagdoll );
		}

		piActionController->Queue( *piAction );	
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
FragmentID CHitDeathReactions::GetHitDeathFragmentID( const IActionController* piActionController )
{
	CRY_ASSERT( piActionController );

	const uint32 FRAGID_HITDEATH = CCrc32::ComputeLowercase( "hitDeath" );
	const FragmentID fragID = piActionController->GetFragID( FRAGID_HITDEATH );

	return fragID;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::OnKill(const CActor::KillParams& killParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	CRY_ASSERT(m_actor.IsDead());

	bool bSuccess = false;

	if (CanPlayDeathReaction() || killParams.forceLocalKill)
	{
		HitInfo hitInfo;
		m_actor.FillHitInfoFromKillParams(killParams, hitInfo);

#if !defined(_RELEASE)
		g_pGame->GetGameRules()->SanityCheckHitInfo(hitInfo, "CPlayer::NetKill");
#endif

		bSuccess = OnKill(hitInfo);
	}

	if(g_pGameCVars->g_hitDeathReactions_debug)
	{
		HitDeathLog("%s", bSuccess?"Success: Performing HitDeathReaction.":"Failure: Not performing HitDeathReaction.");
	}

	if (!IsInDeathReaction())
	{
		// actor is dead, release anims
		ReleaseReactionAnims(eRRF_Alive);
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::OnReaction(const HitInfo& hitInfo, int* pAnimIndex)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	bool bSuccess = false;

	// Generate and set seed for this event
	uint32 uSeed = GetSynchedSeed(true);
	m_pseudoRandom.seed(gEnv->bNoRandomSeed?0:uSeed);

	// Choose proper death reaction
	ReactionsContainer::const_iterator itBestFit = std::find_if(m_pDeathReactions->begin(), m_pDeathReactions->end(), SPredFindValidReaction(*this, hitInfo));

	// If found a fit, execute it
	if (itBestFit != m_pDeathReactions->end())
	{
		if( pAnimIndex && itBestFit->mannequinData.actionType != SReactionParams::SMannequinData::EActionType_Invalid )
		{
			if( !gEnv->bMultiplayer )
			{
				// Implies anim index is being driven, e.g. net deserialize.
				if( *pAnimIndex >= 0 )
				{
					itBestFit->mannequinData.m_iNextOptionIndex = *pAnimIndex;
				}
				else
				{
					*pAnimIndex = itBestFit->mannequinData.m_iNextOptionIndex;
				}
			}
			else
			{
				// MP purely used for StealthKill net serialization.
				if( *pAnimIndex >= 0 )
				{
					itBestFit->mannequinData.m_animIndexMP = *pAnimIndex;
				}
				else
				{
					*pAnimIndex = itBestFit->mannequinData.m_animIndexMP;
				}
			}
		}

		bSuccess = StartDeathReaction(hitInfo, *itBestFit);

		PausePipeUser(true);
	}

	return bSuccess;
}


bool CHitDeathReactions::OnHit(const HitInfo& hitInfo, float fCausedDamage/* = 0.0f*/)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	bool bSuccess = false;

	// In single player only the player's hits will trigger hit reactions
	if (!gEnv->bMultiplayer)
	{
		if( (hitInfo.shooterId != LOCAL_PLAYER_ENTITY_ID) && (gEnv->pGameFramework->GetIActorSystem()->GetActor( hitInfo.shooterId ) != NULL) )
		{
			return false;
		}
	}

	m_bInSmartObject = static_cast<CPlayer&>(m_actor).IsPlayingSmartObjectAction();

	if (CanPlayHitReaction())
	{
		// Generate and set seed for this event
		uint32 uSeed = GetSynchedSeed(false);
		m_pseudoRandom.seed(gEnv->bNoRandomSeed?0:uSeed);

		// Choose proper hit reaction
		ReactionsContainer::const_iterator itBestFit =  std::find_if(m_pHitReactions->begin(), m_pHitReactions->end(), SPredFindValidReaction(*this, hitInfo, fCausedDamage));

		// If found a fit, execute it
		if (itBestFit != m_pHitReactions->end())
		{
			bSuccess = StartHitReaction(hitInfo, *itBestFit);
		}	
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::OnRevive()
{
	ClearState();

	if (!m_actor.IsDead())
		RequestReactionAnims(eRRF_Alive);

	m_bInSmartObject = false;
}

//////////////////////////////////////////////////////////////////////////
// returns true if processed this event
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::OnAnimationEvent(const AnimEventInstance &event)
{
	bool bSuccessfullyProcessed = false;

	if (IsInReaction())
	{
		if (m_actor.GetAnimationEventsTable().m_deathReactionEndId == event.m_EventNameLowercaseCRC32)
		{
			if (DoesAnimEventWantToSleepRagdoll(event))
				m_reactionFlags |= SReactionParams::SleepRagdoll;

			// Immediately processing the AnimationEvent was causing stalls on the Main Thread in MP games 
			// due network aspect profile changes inside the EndCurrentReactionInternal happening when the network 
			// thread has the lock (inside CryAction::PostUpdate)
			// bSuccessfullyProcessed = EndCurrentReactionInternal(true, false);

			// The reaction will be finished on the next update
			m_fReactionEndTime = m_fReactionCounter;
			bSuccessfullyProcessed = true;
		}
		else if (m_actor.GetAnimationEventsTable().m_ragdollStartId == event.m_EventNameLowercaseCRC32)
		{
			CRY_ASSERT_MESSAGE(m_actor.GetAnimatedCharacter(), "This class assumes that if this code is called, the actor has a valid animated character");

			//--- Initiate rag-doll between now & the end of the animation
			CAnimationPlayerProxy *animPlayerProxy = m_actor.GetAnimatedCharacter()->GetAnimationPlayerProxy(0);
			const float expectedDurationSeconds = animPlayerProxy ? animPlayerProxy->GetTopAnimExpectedSecondsLeft(m_actor.GetEntity(), 0) : 0.0f;
			if (0 <= expectedDurationSeconds)
			{
				m_fReactionEndTime = m_fReactionCounter + (GetRandomProbability() * expectedDurationSeconds);

				if (DoesAnimEventWantToSleepRagdoll(event))
					m_reactionFlags |= SReactionParams::SleepRagdoll;

				bSuccessfullyProcessed = true;
			}
		}
		else if (m_actor.GetAnimationEventsTable().m_reactionOnCollision == event.m_EventNameLowercaseCRC32)
		{
			// Change the current state of the reactionOnCollision flag
			if (event.m_CustomParameter && *event.m_CustomParameter)
			{
				m_reactionOnCollision = atoi(event.m_CustomParameter);

				bSuccessfullyProcessed = true;
			}
		}
		else if (m_actor.GetAnimationEventsTable().m_forbidReactionsId == event.m_EventNameLowercaseCRC32)
		{
			if (event.m_CustomParameter && *event.m_CustomParameter)
			{
				const bool bForbidReactions = atoi(event.m_CustomParameter) != 0;
				if (bForbidReactions)
					m_reactionFlags |= SReactionParams::ReactionsForbidden;
				else
					m_reactionFlags &= ~SReactionParams::ReactionsForbidden;
			}
		}
	}

	return bSuccessfullyProcessed;
}

/////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::HandleEvent(const SGameObjectEvent& event)
{
	bool bProcessed = false;

	switch(event.event)
	{
	case eCGE_ReactionEnd:
		{
			const bool bForceFinishMannequinAction = (event.param == 0 || event.param == m_pAction.get());
			bProcessed = EndCurrentReactionInternal(false, true, bForceFinishMannequinAction);
		}
		break;
	default:
		// [*DavidR | 24/Nov/2009] ToDo: Invoke an event handler function within HitDeathReactions.lua?
		bProcessed = false;
		break;
	}

	return bProcessed;
}

/////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::Reload()
{
	// Reloading invalidates the pointer to the shared reaction params
	m_pCurrentReactionParams = NULL;

	// Reload the data structure and scripts
	LoadData(m_pSelfTable, true);

	// Not the perfect solution (we could be requesting anims for dead actors, but is a reasonable 
	// compromise for a debug-only function)
	RequestReactionAnims(eRRF_OutFromPool | eRRF_Alive | eRRF_AIEnabled);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::IsValidReaction(const HitInfo& hitInfo, const SReactionParams::SValidationParams& validationParams, float fCausedDamage) const
{
	// Check speed
	const SActorStats* pActorStats = m_actor.GetActorStats();
	CRY_ASSERT(pActorStats);

	IF_UNLIKELY (validationParams.fMinimumSpeedAllowed > pActorStats->speedFlat)
		return false;

	IF_UNLIKELY (pActorStats->speedFlat > validationParams.fMaximumSpeedAllowed)
		return false;

	// Check damage
	IF_UNLIKELY (validationParams.fMinimumDamageAllowed > hitInfo.damage)
		return false;

	IF_UNLIKELY (hitInfo.damage > validationParams.fMaximumDamageAllowed)
		return false;

	// Check damage thresholds
	IF_UNLIKELY(!validationParams.healthThresholds.empty())
	{
		if (fCausedDamage > 0.0f)
		{
			const float fHealthPostHit = m_actor.GetHealth();
			SReactionParams::ThresholdsContainer::const_iterator itThreshold = validationParams.healthThresholds.upper_bound(fHealthPostHit);
			if ((itThreshold == validationParams.healthThresholds.end()) || ((fHealthPostHit + fCausedDamage) < *itThreshold))
				return false;
		}
		else
			return false;
	}

	// Check distance
	const bool bMinDistanceCheck = (validationParams.fMinimumDistance > 0.0f);
	const bool bMaxDistanceCheck = (validationParams.fMaximumDistance > 0.0f);
	IF_UNLIKELY (bMinDistanceCheck || bMaxDistanceCheck)
	{
		IEntity* pShooter = gEnv->pEntitySystem->GetEntity(hitInfo.shooterId);
		if (pShooter)
		{
			float fDistanceToShooter = m_actor.GetEntity()->GetPos().GetDistance(pShooter->GetPos());
			if ((bMinDistanceCheck && (validationParams.fMinimumDistance > fDistanceToShooter)) ||
				(bMaxDistanceCheck && (fDistanceToShooter > validationParams.fMaximumDistance)))
				return false;
		}
		else
		{
#ifndef _RELEASE
			CRY_ASSERT_MESSAGE(false, "Couldn't find shooter entity while checking distance to shooter criteria");
			CHitDeathReactionsSystem::Warning("Couldn't find shooter entity while checking distance to shooter criteria");
#endif

			return false;
		}
	}

	// Check movement direction
	IF_UNLIKELY (validationParams.movementDir != eCD_Invalid)
	{
		IMovementController* pMovementController = m_actor.GetMovementController();
		if (pMovementController)
		{
			// Get move direction of entity
			SMovementState state;
			pMovementController->GetMovementState(state);
			Vec2 vMoveDir(state.movementDirection);
			if (vMoveDir.IsValid() && !vMoveDir.IsZero(0.00001f))
			{
				if (!CheckCardinalDirection2D(validationParams.movementDir, Vec2(m_actor.GetEntity()->GetForwardDir()), vMoveDir))
					return false;
			}
			else
				return false;
		}
	}

	// Check hit joint (if the allowedPartIds set is empty, any part will be valid)
	const SReactionParams::IdContainer& allowedPartIds = validationParams.allowedPartIds;
	IF_UNLIKELY (!allowedPartIds.empty() && (allowedPartIds.find(hitInfo.partId) == allowedPartIds.end()))
		return false;

	// Check shot origin
	IF_UNLIKELY (validationParams.shotOrigin != eCD_Invalid)
	{
		Vec2 v2DShotDir(hitInfo.dir);
		if (v2DShotDir.IsValid())
		{
			CRY_ASSERT_MESSAGE(m_actor.GetAnimatedCharacter(), "This class assumes that if this code is called, the actor has a valid animated character");
			// [*DavidR | 15/Oct/2010] Note: Using the animated character location to determine shot origin, since it's more accurate.
			// for now, though, I keep the old method in multiplayer, since it's safer (both client and server side having the same animation orientation
			// when processing the same hit is tricky, and having the client playing a different animation than the server is bound to cause problems)
			Vec2 vForwardDir(gEnv->bMultiplayer ? m_actor.GetEntity()->GetForwardDir() : m_actor.GetAnimatedCharacter()->GetAnimLocation().GetColumn1());
			if (!CheckCardinalDirection2D(validationParams.shotOrigin, vForwardDir, -v2DShotDir))
				return false;
		}
		else
			return false;
	}

	// Check probability
	IF_UNLIKELY ((validationParams.fProbability != 1.0f) && (GetRandomProbability() > validationParams.fProbability))
		return false;
	
	// Check stance
	const SReactionParams::IdContainer& allowedStances = validationParams.allowedStances;
	CRY_ASSERT_TRACE(allowedStances.empty() || (m_actor.GetStance() != STANCE_NULL), ("%s's current stance is NULL! This actor state is probably incoherent!", m_actor.GetEntity()->GetName()));
	IF_UNLIKELY (!allowedStances.empty() && (allowedStances.find(m_actor.GetStance()) == allowedStances.end()))
		return false;

	// Check hit type
	const SReactionParams::IdContainer& allowedHitTypes = validationParams.allowedHitTypes;
	if(!allowedHitTypes.empty())
	{
#ifdef HITDEATH_DEBUG
		if(g_pGameCVars->g_hitDeathReactions_debug)
		{
			if(CGameRules* pGameRules = g_pGame->GetGameRules())
			{
				HitDeathLog("Looking for HitType: [%s]", pGameRules->GetHitType(hitInfo.type));
				for(SReactionParams::IdContainer::const_iterator it = allowedHitTypes.begin(); it!=allowedHitTypes.end(); it++)
				{
					HitDeathLog("Allowed HitType: [%s]", pGameRules->GetHitType(*it));
				}
			}
		}
#endif //HITDEATH_DEBUG
		IF_UNLIKELY ( (allowedHitTypes.find(hitInfo.type) == allowedHitTypes.end()))
			return false;
	}

	// Check projectile class
	const SReactionParams::IdContainer& allowedProjectiles = validationParams.allowedProjectiles;
	IF_UNLIKELY (!allowedProjectiles.empty() && (allowedProjectiles.find(hitInfo.projectileClassId) == allowedProjectiles.end()))
		return false;

	// Check weapon class
	const SReactionParams::IdContainer& allowedWeapons = validationParams.allowedWeapons;
	IF_UNLIKELY (!allowedWeapons.empty() && (allowedWeapons.find(hitInfo.weaponClassId) == allowedWeapons.end()))
		return false;

	// Check using mounted item
	IF_UNLIKELY (validationParams.bAllowOnlyWhenUsingMountedItems && (pActorStats->mountedWeaponID == 0))
		return false;

	// Check if is in the Air or on Ground.
	if(validationParams.airState!=eAS_Unset)
	{
		const bool onObject =  m_actor.GetActorPhysics().groundColliderId!=0;
		if(validationParams.airState==eAS_OnObject)
		{
			if(!onObject)
				return false;
		}
		else
		{
			const bool onGround = pActorStats->onGround>0.f && !onObject;
			if(validationParams.airState==eAS_OnGround)
			{
				if(!onGround)
				{
					return false;
				}
			}
			else if(validationParams.airState==eAS_OnGroundOrObject)
			{
				 if(!(onGround||onObject))
					return false;
			}
			else
			{
				const bool inAir = pActorStats->inAir>0.f && !onGround && !onObject;
				if(validationParams.airState==eAS_InAir && !inAir)
					return false;
			}
		}
	}

	// Check destructible parts event
	IF_UNLIKELY ((validationParams.destructibleEvent != 0) && 
		(validationParams.destructibleEvent != m_actor.GetBodyDestructibilityInstance().GetLastDestructionEventForHitReactions()))
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::StartHitReaction(const HitInfo& hitInfo, const SReactionParams& reactionParams)
{
	return StartReaction(eRT_Hit, hitInfo, reactionParams);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::StartCollisionReaction(const HitInfo& hitInfo, const SReactionParams& reactionParams)
{
	return StartReaction(eRT_Collision, hitInfo, reactionParams);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::StartDeathReaction(const HitInfo& hitInfo, const SReactionParams& reactionParams)
{
	if( !m_bInSmartObject )
	{
		return StartReaction(eRT_Death, hitInfo, reactionParams);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::StartReaction(EReactionType reactionType, const HitInfo& hitInfo, const SReactionParams& reactionParams)
{
	CRY_ASSERT((reactionType == eRT_Death) || (reactionType == eRT_Hit) || (reactionType == eRT_Collision));

#ifdef HITDEATH_DEBUG
	if(g_pGameCVars->g_hitDeathReactions_debug)
	{
		CGameRules* pGameRules = g_pGame->GetGameRules();
		HitDeathLog("Starting Reaction: Type[%s] HitType[%s] Server[%s] Actor[%s]"
			, reactionType==eRT_Death?"Death":reactionType==eRT_Hit?"Hit":reactionType==eRT_Collision?"Collision":"INVALID"
			, pGameRules?pGameRules->GetHitType(hitInfo.type):"INVALID"
			, gEnv->bServer?"true":"false"
			, m_actor.GetEntity()->GetName()
			);
	}
#endif //HITDEATH_DEBUG

	bool bSuccess = false;

	EndCurrentReactionInternal(false, false);
	SetInReaction(reactionType);
	m_reactionOnCollision = reactionParams.reactionOnCollision;
	m_reactionFlags = reactionParams.flags;

	// We need to store a pointer to the hit info so when executing custom/default LUA execution functions we can avoid constructing/deconstructing
	// the hitInfo table (which will be prohibitively expensive)
	CRY_ASSERT(!m_pHitInfo);
	m_pHitInfo = &hitInfo;
	m_lastHitInfo = hitInfo;

	// [*DavidR | 2/Jun/2010] Note: Storing the current reaction params here renders the ReactionID logic (for getting 
	// C++ reactionParams struct without parsing the reactionParams script tables on LUA calls) almost useless. Think about
	// removing it (it adds a bit of overhead on LUA calls, but enforces integrity)
	m_pCurrentReactionParams = &reactionParams; 

	if (!reactionParams.sCustomExecutionFunc.empty())
	{
		const CCustomReactionFunctions& customReactions = g_pGame->GetHitDeathReactionsSystem().GetCustomReactionFunctions();
		bSuccess = customReactions.CallCustomExecutionFunction(m_pSelfTable, reactionParams.sCustomExecutionFunc, m_actor, reactionParams, hitInfo);
		// Only set Custom execution type if the execution has not internally caused a reactionAnim or AnimationGraph execution type
		if (bSuccess && IsExecutionType(eET_None))
		{
			SetExecutionType(eET_Custom);
		}
	}
	else
	{
		const char* szDefaultReactionFnc = (reactionType == eRT_Hit) ? DEFAULT_HIT_REACTION_FUNCTION : DEFAULT_KILL_REACTION_FUNCTION;
		bSuccess = g_pGameCVars->g_hitDeathReactions_useLuaDefaultFunctions ? Script::CallMethod(m_pSelfTable, szDefaultReactionFnc, reactionParams.reactionScriptTable) : false;
		if (!bSuccess)
		{
			CRY_ASSERT_MESSAGE(!g_pGameCVars->g_hitDeathReactions_useLuaDefaultFunctions, "Can't run default hit reaction lua method. Check HitDeathReactions.lua");

			// Default execution
			switch( reactionType )
			{
			case eRT_Hit: bSuccess = ExecuteHitReaction(reactionParams); break;
			case eRT_Collision: bSuccess = ExecuteHitReaction(reactionParams); break;
			default: bSuccess = ExecuteDeathReaction(reactionParams); break;
			}
		}
	}


	if (bSuccess && 
		!IsExecutionType(eET_None) && IsInReaction()) // Custom reactions can invoke EndCurrentReaction inside them if they fail, so we need to react accordingly
	{
		m_fReactionCounter = 0.0f;
		m_fReactionEndTime = -1.0f;

		if (IsAI())
		{
			HandleAIStartReaction(hitInfo, reactionParams);

			if( m_actor.GetActorStats()->mountedWeaponID != 0 )
			{
				SendAISignal( AI_SIGNAL_ONHIT_MOUNTED_GUN, NULL );
			}
		}
	}
	else
	{
		ClearState();
	}

	// We can't store this pointer
	m_pHitInfo = NULL;

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::HandleAIStartReaction(const HitInfo& hitInfo, const SReactionParams& reactionParams)
{
	CRY_ASSERT(IsAI());

	if (gEnv->pAISystem && !reactionParams.sCustomAISignal.empty())
	{
		IAISignalExtraData *pData = gEnv->pAISystem->CreateSignalExtraData();
		if (pData)
		{
			pData->iValue = m_iGoalPipeId;
			pData->point = hitInfo.pos;
			pData->point2 = hitInfo.dir;
		}

		SendAISignal(reactionParams.sCustomAISignal.c_str(), pData);

		m_AIPausedState = eAIP_ExecutingCustomSignal;
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::HandleAIEndReaction()
{
	CRY_ASSERT(IsAI());

	if (gEnv->pAISystem && IsInReaction())
	{
		IAISignalExtraData *pData = gEnv->pAISystem->CreateSignalExtraData();
		if (pData)
		{
			pData->iValue = m_iGoalPipeId;
		}

		SendAISignal(AI_SIGNAL_FINISHED_REACTION, pData);
	}

	if (eAIP_ExecutingCustomSignal != m_AIPausedState)
	{
		PausePipeUser(false);
	}

	m_AIPausedState = eAIP_NotPaused;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::HandleAIReactionInterrupted()
{
	CRY_ASSERT(IsAI());

	if (gEnv->pAISystem && IsInReaction())
	{
		IAISignalExtraData *pData = gEnv->pAISystem->CreateSignalExtraData();
		if (pData)
		{
			pData->iValue = m_iGoalPipeId;
		}

		SendAISignal(AI_SIGNAL_INTERRUPTED_REACTION, pData);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::ExecuteHitReaction(const SReactionParams& reactionParams)
{
	bool bSuccess = false;

	CCCPOINT(HitDeath_ExecuteHitReaction);

	if (!IsInDeathReaction())
	{
		bSuccess = ExecuteReactionCommon(reactionParams);
	}
	else
	{  
		CHitDeathReactionsSystem::Warning("[CHitDeathReactions::ExecuteHitReaction] Can't execute a hit reaction when "
			"not in Hit Reaction mode. If you invoked this function from lua, make sure you're doing it from a reactionFunc method inside HitDeathReactions.lua");
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::ExecuteDeathReaction(const SReactionParams& reactionParams)
{
	bool bSuccess = false;

	CCCPOINT(HitDeath_ExecuteDeathReaction);

	if (IsInDeathReaction())
	{
		bSuccess = ExecuteReactionCommon(reactionParams);
	}
	else
	{
		CHitDeathReactionsSystem::Warning("Can't execute a death reaction when "
			"not in Death Reaction mode. If you invoked this function from lua, make sure you're doing it from a reactionFunc method inside HitDeathReactions.lua");
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// Suppress passedByValue for smart pointers like ScriptTablePtr
// cppcheck-suppress passedByValue
bool CHitDeathReactions::IsValidReaction(const HitInfo& hitInfo, const ScriptTablePtr pScriptTable, float fCausedDamage) const
{
	bool bIsValidReaction = false;

	ReactionId reactionId = INVALID_REACTION_ID;
	if (pScriptTable->GetValue(REACTION_ID, reactionId) && IsValidReactionId(reactionId))
	{
		const SReactionParams& reactionParams = GetReactionParamsById(reactionId);
		int iValidationIdx = -1;
		if (pScriptTable->GetValue(VALIDATION_ID, iValidationIdx))
		{
			CRY_ASSERT((iValidationIdx >= 0) && (iValidationIdx < static_cast<int>(reactionParams.validationParams.size())));

			// The table is the validationParams table
			if ((iValidationIdx >= 0) && (iValidationIdx < static_cast<int>(reactionParams.validationParams.size())))
				bIsValidReaction = IsValidReaction(hitInfo, reactionParams.validationParams[iValidationIdx], fCausedDamage);
		}
		else
		{
			// The table is the reactionParams table
			bIsValidReaction = (SPredFindValidReaction(*this, hitInfo, fCausedDamage))(reactionParams);
		}
	}

	return bIsValidReaction;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// Suppress passedByValue for smart pointers like ScriptTablePtr
// cppcheck-suppress passedByValue
bool CHitDeathReactions::ExecuteHitReaction(const ScriptTablePtr pScriptTable)
{
	bool bRet = false;

	ReactionId reactionId = INVALID_REACTION_ID;
	if (pScriptTable->GetValue(REACTION_ID, reactionId) && IsValidReactionId(reactionId))
		bRet = ExecuteHitReaction(GetReactionParamsById(reactionId));

	return bRet;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// Suppress passedByValue for smart pointers like ScriptTablePtr
// cppcheck-suppress passedByValue
bool CHitDeathReactions::ExecuteDeathReaction(const ScriptTablePtr pScriptTable)
{
	bool bRet = false;

	ReactionId reactionId = INVALID_REACTION_ID;
	if (pScriptTable->GetValue(REACTION_ID, reactionId) && IsValidReactionId(reactionId))
		bRet = ExecuteDeathReaction(GetReactionParamsById(reactionId));

	return bRet;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::EndCurrentReaction()
{
	return EndCurrentReactionInternal(false, false);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::StartReactionAnim(const string& sAnimName, bool bLoop/* = false*/, float fBlendTime/* = 0.2f*/, int iSlot/* = 0*/, int iLayer/* = 0*/, uint32 animFlags /*= (DEFAULT_REACTION_ANIM_FLAGS)*/, float fAdditiveWeight/* = 0.0f*/, float fAniSpeed/* = 1.0f*/, bool bNoAnimCamera/*=false*/)
{
	bool bSuccess = false;

	ICharacterInstance *pMainChar = m_actor.GetEntity()->GetCharacter(0);
	CRY_ASSERT(pMainChar);
	IAnimationSet *pAnimSet = pMainChar ? pMainChar->GetIAnimationSet() : NULL;
	if (pAnimSet)
	{
		int animID = pAnimSet->GetAnimIDByName(sAnimName.c_str());
		if (animID >= 0)
		{
			bSuccess = StartReactionAnimByID(animID, bLoop, fBlendTime, iSlot, iLayer, animFlags, fAdditiveWeight, fAniSpeed, bNoAnimCamera);
		}
		else
			AnimIDError_Dup(sAnimName.c_str());
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::StartReactionAnimByID(int animID, bool bLoop/* = false*/, float fBlendTime/* = 0.2f*/, int iSlot/* = 0*/, int iLayer/* = 0*/, uint32 animFlags /*= (DEFAULT_REACTION_ANIM_FLAGS)*/, float fAdditiveWeight/* = 0.0f*/, float fAniSpeed/* = 1.0f*/, bool bNoAnimCamera/*=false*/)
{
	CRY_ASSERT(fAniSpeed > 0.0f);

	CRY_ASSERT_MESSAGE(IsInReaction(), "This method should be called only during a hit or death reaction");
	if (!IsInReaction())
	{
		CHitDeathReactionsSystem::Warning("Can't start an animation through HitDeathReactions system "
			"outside of a reaction. If you invoked this function from lua, make sure you're doing it from a reactionFunc method inside HitDeathReactions.lua");

		return false;
	}

	bool bResult = false;

	ICharacterInstance* pCharacter = m_actor.GetEntity()->GetCharacter(iSlot);
	if (pCharacter)
	{
		CryCharAnimationParams animParams;
		animParams.m_nLayerID = iLayer;
		animParams.m_fTransTime = fBlendTime;
		animParams.m_nFlags = animFlags;
		animParams.m_nFlags |= bLoop ? CA_LOOP_ANIMATION : 0;
		animParams.m_nUserToken = ANIM_USER_TOKEN;

		// [*DavidR | 19/Feb/2010] ToDo: !canMix states on the AG or lower layers animations with CA_DISABLE_MULTILAYER
		// can negate the effects of partial/additive anims on higher layers. Find a solution to this

		IAnimatedCharacter* pAnimatedCharacter = m_actor.GetAnimatedCharacter();
		CRY_ASSERT_MESSAGE(pAnimatedCharacter, "This class assumes that if this code is called, the actor has a valid animated character");

		// As we're doing later with the animation graph, we need to take a similar approach to pausing if we're using an action controller.
		// But we need to do this before tampering with the animation queues directly.
		const bool pauseActionController = (animParams.m_nLayerID == eAnimationGraphLayer_FullBody);
		bool allowStartPlayAnimation = true;
		if (pauseActionController)
		{
			IActionController* pActionController = pAnimatedCharacter->GetActionController();
			if (pActionController)
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Trying to play a fullbody animation hit death reaction. This is not supported!");
				allowStartPlayAnimation = false;
			}
		}

		if (allowStartPlayAnimation)
		{
			ISkeletonAnim* pISkeletonAnim = pCharacter->GetISkeletonAnim();
			CAnimationPlayerProxy* pPlayerProxy = pAnimatedCharacter->GetAnimationPlayerProxy(0);
			if (pPlayerProxy)
			{
				bResult = pPlayerProxy->StartAnimationById(m_actor.GetEntity(), animID, animParams, fAniSpeed);
			}
			else
			{
				bResult = pISkeletonAnim->StartAnimationById(animID, animParams);
			}

			if (bResult)
			{
				bool bAdditive = fAdditiveWeight > 0.0f;
				if (bAdditive)
				{
					pISkeletonAnim->SetLayerBlendWeight(animParams.m_nLayerID, fAdditiveWeight);
				}
				else
				{
					// If not additive, stop higher layers
					StopHigherLayers(iSlot, animParams.m_nLayerID, animParams.m_fTransTime);
				}

				if (!pPlayerProxy)
				{
					pISkeletonAnim->SetLayerPlaybackScale(animParams.m_nLayerID, fAniSpeed);
				}

				// Pause animation graph when playing animations on layer 0 (otherwise they will be overwritten by state changes)
				if (animParams.m_nLayerID == eAnimationGraphLayer_FullBody)
				{
					IAnimationGraphState* pAnimationGraphState = m_actor.GetAnimationGraphState();
					if (pAnimationGraphState)
					{
						pAnimationGraphState->Pause(true, eAGP_HitDeathReactions);
					}

					// Set default movement control method and collider mode for fullbody animations
					pAnimatedCharacter->SetMovementControlMethods(eMCM_AnimationHCollision, eMCM_SmoothedEntity);
					pAnimatedCharacter->RequestPhysicalColliderMode(eColliderMode_Pushable, eColliderModeLayer_Game);

					// Disable lookIk/aimIk
					// [*DavidR | 18/Feb/2010] ToDo: Optionally?
					// Make sure this is done after pausing the graph, just in case pausing changes the state (far-fetched)
					pAnimatedCharacter->AllowLookIk(false);
					pAnimatedCharacter->AllowAimIk(false);
				}

				// If the player and in 1st person camera, activate animated controlled camera if allowed
				if (!m_actor.IsThirdPerson() && !bNoAnimCamera)
				{
					EnableFirstPersonAnimation(true);
				}

				StartFacialAnimation(pCharacter, PAIN_FACIAL_EXPRESSION);

				m_currentCustomAnim.SetAnimName(animID, pCharacter->GetIAnimationSet());
				m_currentCustomAnim.iLayer = animParams.m_nLayerID;
			}
			else
			{
				CHitDeathReactionsSystem::Warning("Failed to start reaction anim %s on character %s", pCharacter->GetIAnimationSet()->GetNameByAnimID(animID), m_actor.GetEntity()->GetName());
			}
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::EndReactionAnim()
{
	if (!IsInDeathReaction())
	{
		if (IsPlayingReactionAnim())
		{
			OnCustomAnimFinished(true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::IsPlayingReactionAnim() const
{
	bool bPlayingAnim = false;

	if (IsExecutionType(eET_ReactionAnim))
	{
		CRY_ASSERT_MESSAGE(m_actor.GetAnimatedCharacter(), "This class assumes that if this code is called, the actor has a valid animated character");
		CAnimationPlayerProxy* pPlayerProxy = m_actor.GetAnimatedCharacter()->GetAnimationPlayerProxy(0);
		if (pPlayerProxy)
		{
			const CAnimation *anim = pPlayerProxy->GetTopAnimation(m_actor.GetEntity(), m_currentCustomAnim.iLayer);
			const float animationTime = pPlayerProxy->GetTopAnimNormalizedTime(m_actor.GetEntity(), m_currentCustomAnim.iLayer);

			bPlayingAnim = anim && anim->HasUserToken(ANIM_USER_TOKEN) && (animationTime < 1.0f);
		}
		else
		{
			ICharacterInstance* pCharacter = m_actor.GetEntity()->GetCharacter(0);
			ISkeletonAnim* pSkel = pCharacter ? pCharacter->GetISkeletonAnim() : NULL;
			const int iNumAnims = pSkel ? pSkel->GetNumAnimsInFIFO(m_currentCustomAnim.iLayer) : 0;
			if (iNumAnims > 0)
			{
				const CAnimation& anim = pSkel->GetAnimFromFIFO(  m_currentCustomAnim.iLayer, iNumAnims - 1 );

				// If the latest anim on the Anim Queue of this layer is a Reaction anim and hasn't reached its end, is
				// being played
				const float animationNormalizedTime = pSkel->GetAnimationNormalizedTime(&anim);
				bPlayingAnim = anim.HasUserToken(ANIM_USER_TOKEN) && (animationNormalizedTime < 0.98f);
			}
		}
	}

	return bPlayingAnim;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::GetMemoryUsage( ICrySizer *pSizer ) const
{
	pSizer->AddObject(this, sizeof(*this));

	pSizer->AddObject(&m_currentCustomAnim, sizeof(m_currentCustomAnim));
	pSizer->AddObject(m_currentCustomAnim.GetAnimName());
	pSizer->AddObject(&m_primitiveIntersectionQueue, sizeof(m_primitiveIntersectionQueue));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::OnKill(const HitInfo& hitInfo)
{
	CRY_ASSERT(CanPlayDeathReaction() || hitInfo.forceLocalKill);

	bool bSuccess = false;

	// Generate and set seed for this event
	uint32 uSeed = GetSynchedSeed(true);
	m_pseudoRandom.seed(gEnv->bNoRandomSeed?0:uSeed);

	// Choose proper death reaction
	ReactionsContainer::const_iterator end = m_pDeathReactions->end();
	ReactionsContainer::const_iterator itBestFit = std::find_if(m_pDeathReactions->begin(), end, SPredFindValidReaction(*this, hitInfo));

	// If found a fit, execute it
	if (itBestFit != end)
	{
		bSuccess = StartDeathReaction(hitInfo, *itBestFit);

		PausePipeUser(true);
	}

	m_bInSmartObject = false;
	
	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::ClearState()
{
	SetInReaction(eRT_None);
	SetExecutionType(eET_None);
	m_fReactionCounter = -1.0f;	// we give it a -1.0f value to be more readable when debugging. Could be 0.0f 
	m_fReactionEndTime = -1.0f;
	m_pHitInfo = NULL;
	m_reactionOnCollision = NO_COLLISION_REACTION;
	m_reactionFlags = 0;
	m_pCurrentReactionParams = NULL;
	m_currentCustomAnim.Invalidate();
	m_AIPausedState = eAIP_NotPaused;
	m_pAction.reset();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::CanPlayHitReaction() const
{
	const SActorStats* pActorStats = m_actor.GetActorStats();

	return (g_pGameCVars->g_hitDeathReactions_enable != 0) && 
		!AreReactionsForbidden() &&
		m_actor.ShouldPlayHitReactions() &&
		!m_actor.IsDead() &&
		!m_actor.GetLinkedVehicle() && // [*DavidR | 17/Feb/2010] Temporary: No reactions in vehicles
		!IsInReaction() &&
		!m_actor.IsFallen() && // Don't try reactions when the actor is in Fall and Play fall stage
		(!pActorStats || (!pActorStats->isGrabbed &&											// Not grabbed by pick and throw
											!pActorStats->isInBlendRagdoll &&								// Not in a ragdoll blend (FallAndPlay).
											(pActorStats->spectacularKillPartner == 0))) && // Not taking part on a spectacular kill
		!gEnv->pGameFramework->GetICooperativeAnimationManager()->IsActorBusy(m_actor.GetEntityId()) && // Not in a cooperative animation
		((m_actor.GetActorClass() != CPlayer::GetActorClassType()) || !static_cast<CPlayer&>(m_actor).IsPlayingSmartObjectAction()) // Block hit reactions based on if the actor is playing smart objects
		;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::CanPlayDeathReaction() const
{
	const SActorStats* pActorStats = m_actor.GetActorStats();

	static bool canPlayDeathReactionInVehicle = false;
	bool canPlayDeathReaction = canPlayDeathReactionInVehicle || !m_actor.GetLinkedVehicle(); // [*DavidR | 17/Feb/2010] Temporary: No reactions in vehicles

	canPlayDeathReaction = canPlayDeathReaction && (g_pGameCVars->g_hitDeathReactions_enable != 0) && 
		!AreReactionsForbidden() &&
		m_actor.IsDead() && 
		!IsInDeathReaction() &&
		(!pActorStats || (!pActorStats->isGrabbed &&										// Not grabbed by pick and throw
											(pActorStats->spectacularKillPartner == 0) && // Not taking part on a spectacular kill
											!pActorStats->isInBlendRagdoll &&							// Not in a ragdoll blend (FallAndPlay).
											!pActorStats->bStealthKilling &&							// Not a stealthkill victim.
											!pActorStats->bStealthKilled &&								// Not a stealthkill killer.
											!pActorStats->isRagDoll)) &&									// Not ragdollized already
		!gEnv->pGameFramework->GetICooperativeAnimationManager()->IsActorBusy(m_actor.GetEntityId()) && // Not in a cooperative animation
		!m_bInSmartObject &&
		( m_actor.GetAnimatedCharacter()->GetActionController() != NULL )		// AC might be NULL if returned to the EntityPool - defend it.
		;

	return canPlayDeathReaction;
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::SetInReaction(EReactionType reactionType)
{
	m_currentReaction = reactionType;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::EndCurrentReactionInternal(bool bForceRagdollOnHit, bool bFinished, bool bForceFinishMannequinAction)
{
	bool bSuccess = true;
	bool bEndingDeathReaction = IsInDeathReaction();
	bool bEndingHitReaction = IsInHitReaction();

	ICharacterInstance* pCharacter = m_actor.GetEntity()->GetCharacter(0);

	if (IsAI())
	{
		HandleAIEndReaction();
	}

	if (IsPlayingReactionAnim())
	{
		EndReactionAnim();
	}
	else if (IsExecutionType(eET_AnimationGraphAnim))
	{
		// Leave reaction state
		if (pCharacter)
		{
			if (!bFinished)
			{
				IAnimationGraphState* pGraph = m_actor.GetAnimationGraphState();
				if (pGraph)
				{
					pGraph->ForceTeleportToQueriedState();
				}
			}
		}	
	}
	else if (IsPlayingMannequinReactionAnim())
	{
		if (bForceFinishMannequinAction)
		{
			m_pAction.reset();
		}
	}

	//--- Clear the reaction now, the SetAspectProfile flushes the animations, this can cause a call back into here 
	//--- to say the animation has finished & so recursively end the reaction & trigger the ragdoll again
	SetInReaction(eRT_None);

	if (bEndingDeathReaction)
	{
		// when finishing the death reaction, we left the actor in a ragdoll state, with the "dead" facial expression
		if (pCharacter)
			StartFacialAnimation(pCharacter, DEAD_FACIAL_EXPRESSION);

		if (DeathReactionEndsInRagdoll() && m_currentExecutionType != eET_Mannequin)
		{
			// Death reactions always end with a ragdoll
			if(gEnv->bServer)
			{
				bSuccess = m_actor.GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Ragdoll);
			}
			else
			{
				m_actor.RagDollize(false);
				bSuccess = true;
			}

			if (m_reactionFlags & SReactionParams::SleepRagdoll)
				SleepRagdoll();	
		}

		// actor is dead, release anims
		ReleaseReactionAnims(eRRF_Alive);
	}
	else if (bEndingHitReaction)
	{
		if (bFinished)
		{
			const bool bAnimationBasedReaction = (m_currentCustomAnim.IsValid() || IsExecutionType(eET_AnimationGraphAnim)); 
			const bool bAdditive = m_pCurrentReactionParams ? m_pCurrentReactionParams->reactionAnim->bAdditive : false;
			const bool bAnimFinishedInAimingPose = bAnimationBasedReaction && !bAdditive && ((m_reactionFlags & SReactionParams::ReactionFinishesNotAiming) == 0);

			// Force Aim IK-Blend to 1.0f so we can blend into anims with aim-poses smoothly. 
			// [*DavidR | 7/May/2010] Warning: aim pose preservation won't work on partial anims affecting the aim pose (is not a likely scenario anyway)
			if (pCharacter && bAnimFinishedInAimingPose)
			{
				IAnimationPoseBlenderDir* pIPoseBlenderAim = pCharacter->GetISkeletonPose()->GetIPoseBlenderAim();
				if (pIPoseBlenderAim)
				{
					pIPoseBlenderAim->SetState(true);
					pIPoseBlenderAim->SetTarget(m_actor.GetEntity()->GetWorldPos() + m_actor.GetEntity()->GetForwardDir());
				}
			}

			// Apply desired end velocity
			if (m_pCurrentReactionParams && !m_pCurrentReactionParams->endVelocity.IsZeroFast())
			{
				SCharacterMoveRequest movementRequest;
				movementRequest.type = eCMT_Fly;
				movementRequest.jumping = false;
				movementRequest.allowStrafe = true;
				movementRequest.rotation = Quat::CreateIdentity();

				// Set move direction and speed of entity
				movementRequest.velocity =  m_actor.GetEntity()->GetWorldTM().TransformVector(m_pCurrentReactionParams->endVelocity);

				CRY_ASSERT_MESSAGE(m_actor.GetAnimatedCharacter(), "This class assumes that if this code is called, the actor has a valid animated character");
				m_actor.GetAnimatedCharacter()->AddMovement(movementRequest);
			}
		}

		// Hit reactions can trigger ragdoll, but in the form of Fall and Play
		if (bForceRagdollOnHit && !(m_reactionFlags & SReactionParams::NoRagdollOnEnd) && static_cast<CPlayer&> (m_actor).CanFall())
			m_actor.Fall();
	}

	if ((m_pCurrentReactionParams != NULL) && (m_pCurrentReactionParams->sCustomExecutionEndFunc.empty() == false))
	{
		const CCustomReactionFunctions& customReactions = g_pGame->GetHitDeathReactionsSystem().GetCustomReactionFunctions();
		customReactions.CallCustomExecutionFunction(m_pSelfTable, m_pCurrentReactionParams->sCustomExecutionEndFunc, m_actor, *m_pCurrentReactionParams, m_lastHitInfo);
	}

	ClearState();

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::IsAI() const
{
	return (gEnv->bServer && !m_actor.IsPlayer());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
IPipeUser* CHitDeathReactions::GetAIPipeUser() const
{
	IPipeUser *pPipeUser = NULL;

	if (IsAI())
	{
		IAIObject* pAI = m_actor.GetEntity()->GetAI();
		pPipeUser = CastToIPipeUserSafe(pAI);
	}

	return pPipeUser;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::SendAISignal(const char* szSignal, IAISignalExtraData *pData, bool bWaitOpOnly) const
{
	CRY_ASSERT(szSignal && szSignal[0]);

	bool bResult = false;

	IAIObject *pAI = (IsAI() ? m_actor.GetEntity()->GetAI() : NULL);
	if (pAI && gEnv->pAISystem)
	{
		if (!bWaitOpOnly)
		{
			gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER, 0, szSignal, pAI, pData);
			bResult = true;
		}
		else if (IAIActor *pAIActor = CastToIAIActorSafe(pAI))
		{
			pAIActor->SetSignal(AISIGNAL_NOTIFY_ONLY, szSignal, m_actor.GetEntity(), pData);
			bResult = true;
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::ExecuteReactionCommon(const SReactionParams& reactionParams)
{
	CRY_ASSERT(m_pHitInfo);
	bool bSuccess = false;

	if (!reactionParams.agReaction.sAGInputValue.empty())
	{
		CryFatalError("HitDeathReactions: AnimationGraph Reactions are not supported anymore");
	}
	else if (!reactionParams.reactionAnim->animCRCs.empty())
	{
		ICharacterInstance* pMainChar = m_actor.GetEntity()->GetCharacter(0);
		IAnimationSet* pAnimSet = pMainChar ? pMainChar->GetIAnimationSet() : NULL;
		CRY_ASSERT(pAnimSet);
		if (pAnimSet)
		{
			const SReactionParams::SReactionAnim& reactionAnim = *reactionParams.reactionAnim;
			int animID = reactionAnim.GetNextReactionAnimId(pAnimSet);
			if (animID >= 0)
			{
				bSuccess = StartReactionAnimByID(animID, false, 0.1f, 0, reactionAnim.iLayer, reactionAnim.animFlags, reactionAnim.bAdditive ? 1.0f : 0.0f, 1.0f, reactionAnim.bNoAnimCamera);
				if (bSuccess)
				{
					m_currentCustomAnim.fOverrideTransTimeToAG = reactionAnim.fOverrideTransTimeToAG;

					SetExecutionType(eET_ReactionAnim);

					reactionAnim.RequestNextAnim(pAnimSet);
				}
			}
			else
			{
				// Just in case it hasn't been requested, request it
				reactionAnim.RequestNextAnim(pAnimSet);
			}
		}
	}
	else if(
		reactionParams.mannequinData.actionType != SReactionParams::SMannequinData::EActionType_Invalid ) 
	{
		TPlayerAction* pPlayerAction = NULL;
		const IActionController* piActionControllerForPreCache = NULL;
	
		switch( reactionParams.mannequinData.actionType )
		{
		case SReactionParams::SMannequinData::EActionType_Fragment:
			{
				IActionController* piActionController = m_actor.GetAnimatedCharacter()->GetActionController();
				const FragmentID fragID = GetHitDeathFragmentID( piActionController );
				const bool isDeathReaction = ( m_currentReaction == eRT_Death );
				pPlayerAction = new CHitDeathPlayerAction( isDeathReaction, fragID, reactionParams.mannequinData.tagState );
				pPlayerAction->SetOptionIdx( reactionParams.mannequinData.GetNextReactionAnimIndex() );

				CRY_ASSERT( reactionParams.mannequinData.IsCurrentFragmentLoaded() );

				if( m_currentReaction == eRT_Collision )
				{
					uint32 optionIndex = m_reactionOnCollision - 1;
					pPlayerAction->SetOptionIdx(optionIndex);
				}

				piActionController->Queue( *pPlayerAction );
				piActionControllerForPreCache = piActionController;
			}
			break;
		case SReactionParams::SMannequinData::EActionType_FragTagCopyingFragment:
			{
				IActionController* piActionController = m_actor.GetAnimatedCharacter()->GetActionController();
				const FragmentID fragID = GetHitDeathFragmentID( piActionController );

				TagState newFragTagState = MergeCurrentFragTagState( piActionController, fragID, reactionParams.mannequinData.tagState );

				const bool isDeathReaction = ( m_currentReaction == eRT_Death );
				pPlayerAction = new CHitDeathPlayerAction( isDeathReaction, fragID, newFragTagState );
				pPlayerAction->SetOptionIdx( reactionParams.mannequinData.GetNextReactionAnimIndex() );
				piActionController->Queue( *pPlayerAction );
				piActionControllerForPreCache = piActionController;
			}
			break;
		case SReactionParams::SMannequinData::EActionType_Coop:
			{
				IActor* piActorInitiator = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor( m_pHitInfo->shooterId );
				if( piActorInitiator && piActorInitiator->GetActorClass() == CPlayer::GetActorClassType() )
				{
					IActionController*  piActorInitiatorActionController = piActorInitiator->GetAnimatedCharacter()->GetActionController();

					// have to dynamically generate the frags/tag IDs as we never have the initiator until now.
					TagID targetTagID = piActorInitiatorActionController->GetGlobalTagID( m_pHitDeathReactionsConfig->manqTargetCRC );
					const FragmentID fragID = GetHitDeathFragmentID( piActorInitiatorActionController );

					pPlayerAction = new CActionCoopAnimation(
						CActionCoopAnimation::SActionCoopParams(
							static_cast<CPlayer&> (*piActorInitiator),
							m_actor,
							fragID,
							reactionParams.mannequinData.tagState,
							targetTagID,
							m_pHitDeathReactionsConfig->piOptionalAnimationADB ) );

					pPlayerAction->SetOptionIdx( reactionParams.mannequinData.GetNextReactionAnimIndex() );
					piActorInitiatorActionController->Queue( *pPlayerAction );
					piActionControllerForPreCache = piActorInitiatorActionController;
				}
			}
			break;
		case SReactionParams::SMannequinData::EActionType_DoNothing:
			// Mannequin plays a frame of t-pose despite the fact the fragment doesn't exist.
			bSuccess = true;
			break;
		default: CRY_ASSERT(false);
		}

		if( pPlayerAction )
		{
			if( piActionControllerForPreCache )
			{
				reactionParams.mannequinData.RequestNextAnim( piActionControllerForPreCache );
			}

			SetExecutionType(eET_Mannequin);
			if (m_pAction)
			{
				m_pAction->ForceFinish();
			}
			m_pAction = pPlayerAction;

			bSuccess = true;
		}
	}

	if (bSuccess) 
	{
		CRY_ASSERT_MESSAGE(m_actor.GetAnimatedCharacter(), "This class assumes that if this code is called, the actor has a valid animated character");
		if (reactionParams.flags & SReactionParams::OrientateToMovementDir)
		{
			IMovementController* pMovementController = m_actor.GetMovementController();
			if (pMovementController)
			{
				// Get move direction of entity
				SMovementState state;
				pMovementController->GetMovementState(state);
				Vec2 vMoveDir(state.movementDirection);
				if (vMoveDir.IsValid() && !vMoveDir.IsZero(0.00001f))
				{
					Ang3 angFacing;
					angFacing.Set(0.0f, 0.0f, atan2_tpl(-vMoveDir.x, vMoveDir.y) + reactionParams.orientationSnapAngle);
					angFacing.RangePI();

					IEntity *pEntity = m_actor.GetEntity();
					pEntity->SetRotation(Quat::CreateRotationXYZ( angFacing ));
				}
			}
		}
		else if (reactionParams.flags & SReactionParams::OrientateToHitDir)
		{
			Ang3 angFacing;
			angFacing.Set(0.0f, 0.0f, atan2_tpl(m_pHitInfo->dir.x, -m_pHitInfo->dir.y) + reactionParams.orientationSnapAngle);
			angFacing.RangePI();

			IEntity *pEntity = m_actor.GetEntity();
			pEntity->SetRotation(Quat::CreateRotationXYZ( angFacing ));

#ifdef HITDEATH_DEBUG
			if (g_pGameCVars->g_hitDeathReactions_debug)
			{
				HitDeathLog("Oriented to (%f, %f, %f)", angFacing.x, angFacing.y, angFacing.z);
			}
#endif //HITDEATH_DEBUG
		}
	}

#ifndef _RELEASE
	// Send telemetry event
	if (bSuccess)
	{
		CStatsRecordingMgr* pRecordingMgr = g_pGame->GetStatsRecorder();
		IStatsTracker* pTracker = pRecordingMgr ? pRecordingMgr->GetStatsTracker(&m_actor) : NULL;
		if (pTracker)
		{
			EGameStatisticEvent eventType = IsInHitReaction() ? eGSE_HitReactionAnim : eGSE_DeathReactionAnim;

			if(pRecordingMgr->ShouldRecordEvent(eventType, &m_actor))
			{
				switch (m_currentExecutionType)
				{
					case eET_ReactionAnim: 
						pTracker->Event(eventType, m_currentCustomAnim.GetAnimName().c_str()); 
						break;
					case eET_AnimationGraphAnim: 
						pTracker->Event(eventType, (string("AG: ")+ GetCurrentAGReactionAnimName()).c_str()); 
						break;
					case eET_Mannequin:
						pTracker->Event(eventType, (string("Manq: ") + "SomeTags").c_str());
						break;
					case eET_Custom: 
						{
							ReactionId reactionId = INVALID_REACTION_ID;
							reactionParams.reactionScriptTable->GetValue(REACTION_ID, reactionId);
							CRY_ASSERT(IsValidReactionId(reactionId));

							pTracker->Event(eventType, string().Format("Custom reaction (%d)", reactionId).c_str()); 
						}
						break;
				}
			}
		}
	}
#endif

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::PausePipeUser(bool bPause)
{
	if(!gEnv->bMultiplayer)
	{
		IF_UNLIKELY(!IsAI())
			return;

		// This function should not be called if our current paused state is currently executing a custom signal.
		//	The reason for this is the AI are right now creating custom behavior, and pausing the pipeuser will
		//	disable that custom behavior from executing properly. (Kevin)
		CRY_ASSERT_MESSAGE(eAIP_ExecutingCustomSignal != m_AIPausedState, "CHitDeathReactions::PausePipeUser Attempting to pause while the current pause state is 'eAIP_ExecutingCustomSignal'. This must not happen!");

		// Special handling for AI on server
		if (bPause != (eAIP_PipeUserPaused == m_AIPausedState))
		{
			IPipeUser* pPipeUser = GetAIPipeUser();
			if (pPipeUser && (!bPause || g_pGameCVars->g_hitDeathReactions_disable_ai))
			{
				pPipeUser->Pause(bPause);

				m_AIPausedState = bPause ? eAIP_PipeUserPaused : eAIP_NotPaused;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::SetVariations(const SReactionParams::SAnimGraphReaction::VariationsContainer& variations) const
{
	IAnimationGraphState* pAnimationGraphState = m_actor.GetAnimationGraphState();
	if (!variations.empty() && pAnimationGraphState)
	{
		SReactionParams::SAnimGraphReaction::VariationsContainer::const_iterator itEnd = variations.end();
		for (SReactionParams::SAnimGraphReaction::VariationsContainer::const_iterator it = variations.begin(); it != itEnd; ++it)
		{
			pAnimationGraphState->SetVariationInput(it->sName, it->sValue);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::LoadData(SmartScriptTable pSelfTable, bool bForceReload)
{
	SmartScriptTable aux;
	if (m_pScriptSystem->GetGlobalValue(HIT_DEATH_REACTIONS_SCRIPT_TABLE, aux))
	{
		// This way we will have different instances of the HitDeathReactions global table sharing the same functions and
		// current elements (but in LUA we can do self.variable = 0; to create a "variable" field only in that instance)
		pSelfTable->Delegate(aux);

		// Set an entity field in the self table with the associated entity so we can easily access it from LUA
		SmartScriptTable pActorScriptTable = m_actor.GetEntity()->GetScriptTable();
		CRY_ASSERT(pActorScriptTable.GetPtr());
		pSelfTable->SetValue("entity", pActorScriptTable);

		// Invalidate the cached profile Id so it gets re-calculated again in HitDeathReactionsSystem
		m_profileId = INVALID_PROFILE_ID;
		m_profileId = g_pGame->GetHitDeathReactionsSystem().GetReactionParamsForActor(m_actor, m_pHitReactions, m_pDeathReactions, m_pCollisionReactions, m_pHitDeathReactionsConfig);
		CRY_ASSERT(m_pHitReactions && m_pDeathReactions && m_pCollisionReactions && m_pHitDeathReactionsConfig);
	}
	else
		CHitDeathReactionsSystem::Warning("Can't find HitDeathReactions global script table!");

	GeneratePostDeathTags();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::OnCustomAnimFinished(bool bInterrupted)
{
	ICharacterInstance* pMainChar = m_actor.GetEntity()->GetCharacter(0);
	if (pMainChar)
	{
		// Stop facial expression
		StopFacialAnimation(pMainChar);
	}

	IAnimatedCharacter* pAnimatedCharacter = m_actor.GetAnimatedCharacter();
	CRY_ASSERT_MESSAGE(pAnimatedCharacter, "This class assumes that if this code is called, the actor has a valid animated character");

	// Make sure it gets removed from the animation FIFO. Since we play the anims with repeat last key flag enabled, they
	// will remain there unless other system overwrites them (Animation Graph) or we clear them
	if ((m_currentCustomAnim.iLayer != eAnimationGraphLayer_FullBody) && (m_currentCustomAnim.iLayer != eAnimationGraphLayer_UpperBody))
	{
		CAnimationPlayerProxy* pPlayerProxy = pAnimatedCharacter->GetAnimationPlayerProxy(0);
		if (pPlayerProxy)
		{
			pPlayerProxy->StopAnimationInLayer(m_actor.GetEntity(), m_currentCustomAnim.iLayer, 0.2f);
		}
		else if (pMainChar && !IsInDeathReaction())
		{
			pMainChar->GetISkeletonAnim()->StopAnimationInLayer(m_currentCustomAnim.iLayer, 0.2f);
		}
	}

	// Re-enable lookIk/aimIk (do this before un-pausing the graph)
	pAnimatedCharacter->AllowLookIk(true);
	pAnimatedCharacter->AllowAimIk(true);

	// If anim is finished, resume animation graph (if is unpaused it won't do anything)
	// [*DavidR | 19/Jul/2010] FIXME: If ending a death reaction, we don't need to resume the anim graph (this assumption 
	// is not very generic, though it may be true for the 99% of the cases)
	if (!IsInDeathReaction())
	{
		IAnimationGraphState* pGraph = m_actor.GetAnimationGraphState();
		if (pGraph)
		{
			pGraph->Pause(false, eAGP_HitDeathReactions, bInterrupted ? -1.0f : m_currentCustomAnim.fOverrideTransTimeToAG);
		}
	}

	// If the player and in 1st person camera, deactivate animated controlled camera
	if (!m_actor.IsThirdPerson())
	{
		EnableFirstPersonAnimation(false);		
	}

	CRY_ASSERT(IsExecutionType(eET_ReactionAnim));
	SetExecutionType(eET_None);

	// End current reaction
	EndCurrentReactionInternal(false, !bInterrupted);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::EnableFirstPersonAnimation(bool bEnable)
{
	CRY_ASSERT(!m_actor.IsThirdPerson());

	// for 1st person deaths with no ragdoll we don't want to disable first person animation mode (the idea is
	// that the camera stays on the last frame of the animation)
	if (bEnable || !(IsInDeathReaction() && (m_reactionFlags & SReactionParams::NoRagdollOnEnd)))
	{
		CRY_ASSERT(CPlayer::GetActorClassType() == m_actor.GetActorClass());
		CPlayer& player = static_cast<CPlayer&>(m_actor);
		player.AnimationControlled(bEnable, !bEnable);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::DoCollisionCheck()
{
	const float RAGDOLL_TEST_RADIUS = m_pHitDeathReactionsConfig->fCollisionRadius;
	const float RAGDOLL_TEST_GROUND_OFFSET = (m_reactionFlags & SReactionParams::CollisionCheckIntersectionWithGround) ? -0.1f : m_pHitDeathReactionsConfig->fCollisionVerticalOffset;

	ICharacterInstance* pCharacter = m_actor.GetEntity()->GetCharacter(0);
	CRY_ASSERT(pCharacter);
	if (!pCharacter)
		return;

	Vec3 vRootBone = pCharacter->GetISkeletonPose()->GetAbsJointByID(m_pHitDeathReactionsConfig->iCollisionBoneId).t; 
	vRootBone.z=0;

	primitives::sphere sphPrim;
	sphPrim.center = m_actor.GetEntity()->GetWorldTM() * vRootBone;
	sphPrim.center.z += RAGDOLL_TEST_RADIUS + RAGDOLL_TEST_GROUND_OFFSET;
	sphPrim.r = RAGDOLL_TEST_RADIUS;

	const int collisionEntityTypes = ent_static | ent_terrain | ent_ignore_noncolliding;

	m_primitiveIntersectionQueue.Queue(sphPrim.type, sphPrim, ZERO, collisionEntityTypes, geom_colltype0);

#ifndef _RELEASE
	if (g_pGameCVars->g_hitDeathReactions_debug)
	{
		IRenderAuxGeom* pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
		const SAuxGeomRenderFlags oldFlags = pRenderAuxGeom->GetRenderFlags();

		ColorB col(0xff, 0, 0, 0xA0);
		pRenderAuxGeom->SetRenderFlags(oldFlags.m_renderFlags | e_AlphaBlended);
		pRenderAuxGeom->DrawSphere(sphPrim.center, sphPrim.r, col);
		pRenderAuxGeom->SetRenderFlags(oldFlags);
	}
#endif 
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::StartCollisionReaction(const Vec3& vNormal, const Vec3& vPoint)
{
	// m_reactionOnCollision == 
	// 0 -> no reaction on collision at all
	// 1 -> immediate reaction end on death reactions (so it enables ragdoll right away) OR
	//	 -> collision reaction specified on the data file with index 1 (intended for FnP) if present, reaction end if not
	// >1-> collision reaction specified on the data file with that index if present, reaction end if not

	const bool bEndCurrentReaction = IsInDeathReaction() && (m_reactionOnCollision == 1);
	if (bEndCurrentReaction)
	{
		EndCurrentReactionInternal(false, false);
	}
	else
	{
		if (m_reactionOnCollision != 1)
		{
			// Position the entity at the right distance
			IEntity* pEntity = m_actor.GetEntity();
			const Vec3& vPos = pEntity->GetWorldPos();
			Vec3 vPlainNormal(vNormal.x, vNormal.y, 0.0f);
			CRY_ASSERT_MESSAGE(vPlainNormal.IsValid() && !vPlainNormal.IsZero(), "Vertical normals shouldn't validate a Collision Reaction Investigate!");
			vPlainNormal.NormalizeSafe(Vec3Constants<float>::fVec3_OneZ);
			Vec3 vStartPos = vPoint - (vPlainNormal * m_pHitDeathReactionsConfig->fCollReactionStartDist);
			vStartPos.z = vPos.z;
			pEntity->SetPos(vStartPos);

			CRY_ASSERT_MESSAGE(m_actor.GetAnimatedCharacter(), "This class assumes that if this code is called, the actor has a valid animated character");
		}

		// Play a collision reaction, this will interrupt the previous one. 
		CRY_ASSERT(m_reactionOnCollision != NO_COLLISION_REACTION);

		if (m_reactionOnCollision <= m_pCollisionReactions->size())
		{
			if (IsAI())
			{
				HandleAIReactionInterrupted();
			}

			// Use a fake hitInfo for the initial execution
			HitInfo fakeHitInfo;
			fakeHitInfo.pos = vPoint;
			fakeHitInfo.dir = -vNormal;

			m_pCurrentReactionParams = NULL;
			if (IsInHitReaction())
			{
				StartCollisionReaction(fakeHitInfo, m_pCollisionReactions->at(m_reactionOnCollision - 1));
			}
			else
			{
				assert(IsInDeathReaction());

				// so the ragdoll is not activated on the EndCurrentReaction at the beginning of the Start
				SetInReaction(eRT_Hit);
				StartDeathReaction(fakeHitInfo, m_pCollisionReactions->at(m_reactionOnCollision - 1));
			}
		}
		else
		{
			// Just end the reaction (if no collision reaction and "1" is specified we allow it as "end reaction when collision happens")
			if (m_reactionOnCollision != 1)
				CHitDeathReactionsSystem::Warning("Invalid collision reaction index %d. Check HitDeathReactions data file", m_reactionOnCollision);

			EndCurrentReactionInternal(false, false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
/// GetRelativeCardinalDirection returns which 90° cone (for forward, back,
/// left and right) the direction vector vDir2 is pointing to compared to
/// direction vector vDir1
//////////////////////////////////////////////////////////////////////////
ECardinalDirection CHitDeathReactions::GetRelativeCardinalDirection2D(const Vec2& vDir1, const Vec2& vDir2) const
{
	float fDotForward = vDir1.Dot(vDir2);
	float fDotLeft = vDir1.Cross(vDir2); // the same as: vDir1.Perp().Dot(vDir2);

	if (fabs_tpl(fDotForward) > fabs_tpl(fDotLeft))
	{
		if (fDotForward > 0.0f)
			return eCD_Forward;
		else
			return eCD_Back;
	}
	else
	{
		if (fDotLeft > 0.0f)
			return eCD_Left;
		else
			return eCD_Right;
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::CheckCardinalDirection2D(ECardinalDirection direction, const Vec2& vDir1, const Vec2& vDir2) const
{
	bool bCheck = false;

	switch(direction)
	{
	case eCD_Ahead:
		{
			float fDot = vDir1.Dot(vDir2);
			bCheck = (fDot >= 0);
		} break;
	case eCD_Behind:
		{
			float fDot = vDir1.Dot(vDir2);
			bCheck = (fDot < 0);
		} break;

	case eCD_RightSide:
		{
			float fCross = vDir1.Cross(vDir2);
			bCheck = (fCross < 0);
		} break;
	case eCD_LeftSide:
		{
			float fCross = vDir1.Cross(vDir2);
			bCheck = (fCross >= 0);
		} break;

	default:
		bCheck = (direction == GetRelativeCardinalDirection2D(vDir1, vDir2));
	}

	return bCheck;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
uint32 CHitDeathReactions::GetSynchedSeed(bool bKillReaction) const
{	
	// [*DavidR | 9/Dec/2009] This method returns a seed that must be the same
	// between calls for the same OnHit/OnKill event between side and server, 
	// but different between successive calls

	// Calculate it using the name of the target + current health
	// [*DavidR | 9/Dec/2009] ToDo: Try to find a better way to have a synced seed
	// [*DavidR | 9/Dec/2009] FixMe: This means the exact same actor is always going to use the same seed (which is a problem
	// if the actor respawns/revive with the same entity name)
	// [*SandyB | 16/Nov/2012] This will need a proper multiplayer fix 
	// because it breaks for characters that have health-gates or are otherwise
	// invulnerable. If such characters do not take damage, their pseudo-randomizer
	// will be re-seeded with the same value over and over, and thus pick
	// the same identical random value for deciding to play a hit-reaction or not!
	return bKillReaction ? 
		CCrc32::Compute(m_actor.GetEntity()->GetName()) :
	CCrc32::Compute(m_actor.GetEntity()->GetName()) + static_cast<uint32>(m_actor.GetHealth() * 100.f);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
float CHitDeathReactions::GetRandomProbability() const
{
	if (!(gEnv->bMultiplayer))
	{
		return cry_random(0.0f, 1.0f);
	}

	return m_pseudoRandom.GenerateFloat();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::CustomAnimHasFinished() const
{
	return IsExecutionType(eET_ReactionAnim) && !IsPlayingReactionAnim();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::StopHigherLayers(int iSlot, int iLayer, float fBlendOut)
{
	if (iLayer < (numVIRTUALLAYERS - 1))
	{
		ICharacterInstance* pCharacter = m_actor.GetEntity()->GetCharacter(iSlot);
		ISkeletonAnim* pISkeletonAnim = pCharacter ? pCharacter->GetISkeletonAnim() : NULL;
		if (pISkeletonAnim)
		{
			for (int i = iLayer + 1; i < numVIRTUALLAYERS; ++i)
			{
				pISkeletonAnim->StopAnimationInLayer(i, fBlendOut);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
#ifndef _RELEASE
void CHitDeathReactions::DrawDebugInfo()
{
	float fFontSize = 1.2f;
	float standbyColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	float reactingColor[4] = {Col_GreenYellow.r, Col_GreenYellow.g, Col_GreenYellow.b, Col_GreenYellow.a};
	float* drawColor = standbyColor;

	string sMsg(string().Format("Hit/Death reactions system is [%s]\nNumber of Hit reactions: %u. Number of Death reactions %u",
		g_pGameCVars->g_hitDeathReactions_enable ? " enabled" : "disabled", m_pHitReactions->size(), m_pDeathReactions->size()));

	Vec3 vDrawPos = m_actor.GetEntity()->GetWorldPos() + m_actor.GetLocalEyePos();
	if (g_pGameCVars->g_hitDeathReactions_enable)
	{
		string sMsg2("\n\n\n\n");

		sMsg += string().Format("\nCan trigger Hit reactions? [%s]\nCan trigger Death reactions? [%s]", CanPlayHitReaction() ? "YES" : " NO", CanPlayDeathReaction() ? "YES" : " NO");		
		if (IsInReaction())
		{
			drawColor = reactingColor;

			sMsg += string().Format("\nExecuting %s reaction (%f)", IsInHitReaction() ? "Hit" : "Death", m_fReactionCounter);

			// The following lines will most probably make the sMsg grow bigger than 256, the current hardcoded limit for DrawLabel,
			// so let's use a different string that will be printed on a different DrawLabel call
			if (IsExecutionType(eET_ReactionAnim))
			{
				sMsg2 += string().Format("\nExecuting Reaction anim: %s", m_currentCustomAnim.GetAnimName().c_str());
			}
			else if (IsExecutionType(eET_AnimationGraphAnim))
			{
				string sAnimName = GetCurrentAGReactionAnimName();
				sMsg2 += string().Format("\nExecuting Animation graph-based reaction: %s", sAnimName.c_str());
			}
			else if (IsExecutionType(eET_Mannequin))
			{
				sMsg2 += string().Format("\nExecuting Mannequin reaction.");
			}
			else if (IsExecutionType(eET_Custom))
			{
				sMsg2 += string().Format("\nExecuting custom LUA reaction code");
			}
			else
			{
				sMsg2 += string().Format("\nTHIS SHOULDN'T HAPPEN. WHAT ARE WE EXECUTING?");
			}
		}
		// Print actor's speed, for helping tweaking speed ranges for reactions
		const SActorStats* pActorStats = m_actor.GetActorStats();
		CRY_ASSERT(pActorStats);
		sMsg2 += string().Format("\nPos (%.2f, %.2f, %.2f) RotZ (%f)%s. speedFlat: %.2f\nName: \"%s\". Health: %.1f", 
			m_actor.GetEntity()->GetPos().x, m_actor.GetEntity()->GetPos().y, m_actor.GetEntity()->GetPos().z, m_actor.GetEntity()->GetRotation().GetRotZ(), m_actor.GetActorStats()->isRagDoll ? " RAGDOLL" : "", pActorStats->speedFlat.Value(), m_actor.GetEntity()->GetName(), m_actor.GetHealth());

		IRenderAuxText::DrawLabelEx(vDrawPos, fFontSize, drawColor, true, false, sMsg2.c_str());
	}

	IRenderAuxText::DrawLabelEx(vDrawPos, fFontSize, drawColor, true, false, sMsg.c_str());

	if ((g_pGameCVars->g_hitDeathReactions_debug > 1) && IsInDeathReaction())
	{
		//--- Draw a ragdoll indicator to aid animators to visualize the transitions
		IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
		ColorB colour = (m_fReactionEndTime >= 0.0f) ? ColorB(0,0,255,255) : ColorB(255, 0, 0, 255);
		pAuxGeom->DrawSphere(m_actor.GetEntity()->GetWorldPos() + m_actor.GetLocalEyePos()+Vec3(0.0f, 0.0f, 0.5f), 0.25f, colour);
	}
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::IsValidReactionId(ReactionId reactionId) const
{
	// A reaction id is invalid if any of the following is true:
	//* reactionId is the invalid reaction id constant
	//* reactionId is greater than the size of the reaction params container is associated to
	//* The associated reaction params container is empty
	if (reactionId == INVALID_REACTION_ID)
		return false;

	ReactionId absReactionId = abs(reactionId);

	ReactionsContainerConstPtr pReactionList;
	if (reactionId < 0)
	{
		int deathReactionsSize = static_cast<int>(m_pDeathReactions->size());
		if (absReactionId > deathReactionsSize)
		{
			pReactionList = m_pCollisionReactions;
			absReactionId -= deathReactionsSize;
		}
		else
			pReactionList = m_pDeathReactions;
	}
	else
		pReactionList = m_pHitReactions;

	return ((absReactionId <= static_cast<int>(pReactionList->size())) && 
					(!pReactionList->empty()));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
const SReactionParams& CHitDeathReactions::GetReactionParamsById(ReactionId reactionId) const
{
	CRY_ASSERT_MESSAGE(IsValidReactionId(reactionId), "this method shouldn't be called if IsValidReactionId is false");

	ReactionId absReactionId = abs(reactionId);

	ReactionsContainerConstPtr pReactionList;
	if (reactionId < 0)
	{
		int deathReactionsSize = static_cast<int>(m_pDeathReactions->size());
		if (absReactionId > deathReactionsSize)
		{
			pReactionList = m_pCollisionReactions;
			absReactionId -= deathReactionsSize;
		}
		else
			pReactionList = m_pDeathReactions;
	}
	else
		pReactionList = m_pHitReactions;

	return pReactionList->at(absReactionId - 1);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
string CHitDeathReactions::GetCurrentAGReactionAnimName() const
{
#ifndef _RELEASE
	// Obtain anim name. We are assuming the animation graph already transitioned to the new hit reaction state and
	// put its animation already on the queue, which could be a wild assumption
	ICharacterInstance* pMainChar = m_actor.GetEntity()->GetCharacter(0);
	CRY_ASSERT(pMainChar);
	if (pMainChar)
	{
		CRY_ASSERT_MESSAGE(m_actor.GetAnimatedCharacter(), "This class assumes that if this code is called, the actor has a valid animated character");

		const CAnimation* pAnim = NULL;
		CAnimationPlayerProxy* pPlayerProxy = m_actor.GetAnimatedCharacter()->GetAnimationPlayerProxy(0);
		if (pPlayerProxy)
		{
			pAnim = pPlayerProxy->GetTopAnimation(m_actor.GetEntity(), eAnimationGraphLayer_FullBody);
		}
		else
		{
			int iNumAnims = pMainChar->GetISkeletonAnim()->GetNumAnimsInFIFO(eAnimationGraphLayer_FullBody);
			if (iNumAnims > 0)
				pAnim = &pMainChar->GetISkeletonAnim()->GetAnimFromFIFO(eAnimationGraphLayer_FullBody, iNumAnims - 1);
		}

		CRY_ASSERT(pAnim);
		if (pAnim)
		{
			const CAnimation& anim = *pAnim;
			int32 topAnimId = anim.GetAnimationId();

#ifdef STORE_ANIMATION_NAMES
			IAnimationSet* pAnimSet = pMainChar->GetIAnimationSet();
			return pAnimSet->GetNameByAnimID(topAnimId);
#else
			return string().Format("%i", topAnimId);
#endif
		}
	}
#endif

	return "UNKNOWN";
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactions::StartFacialAnimation(ICharacterInstance* pCharacter, const char* szEffectorName, float fWeight, float fFadeTime)
{
	CRY_ASSERT(pCharacter);

	// [*DavidR | 29/Apr/2010] Warning: If this facial animations start getting called from different places we'd need some kind 
	// of manager for this stuff
	IFacialInstance* pFacialInstance = pCharacter->GetFacialInstance();
	IFacialModel* pFacialModel = pFacialInstance ? pFacialInstance->GetFacialModel() : NULL;
	IFacialEffectorsLibrary* pLibrary = pFacialModel ? pFacialModel->GetLibrary() : NULL;
	if (pLibrary)
	{
		IFacialEffector* pEffector = pLibrary->Find(szEffectorName);
		if (pEffector)
		{
			if (m_effectorChannel != INVALID_FACIAL_CHANNEL_ID)
			{
				// we fade out with the same fadeTime as fade in
				pFacialInstance->StopEffectorChannel(m_effectorChannel, fFadeTime);
				m_effectorChannel = INVALID_FACIAL_CHANNEL_ID;
			}

			m_effectorChannel = pFacialInstance->StartEffectorChannel(pEffector, fWeight, fFadeTime);
		}
	}

	return (m_effectorChannel != INVALID_FACIAL_CHANNEL_ID);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::StopFacialAnimation(ICharacterInstance* pCharacter, float fFadeOutTime)
{
	CRY_ASSERT(pCharacter);

	if(m_effectorChannel != INVALID_FACIAL_CHANNEL_ID)
	{
		if (IFacialInstance* pInstance = pCharacter->GetFacialInstance())
		{
			pInstance->StopEffectorChannel(m_effectorChannel, fFadeOutTime);
			m_effectorChannel = INVALID_FACIAL_CHANNEL_ID;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactions::SleepRagdoll()
{
	CRY_ASSERT(m_actor.GetGameObject()->GetAspectProfile(eEA_Physics) == eAP_Ragdoll);

	IPhysicalEntity* pPhysicalEntity = m_actor.GetEntity()->GetPhysics();
	if (pPhysicalEntity)
	{
		// Reenable auto-impulses on particle impacts so the ragdoll awakes when impacted by them
		pe_params_part colltype;
		colltype.flagsColliderAND = ~geom_no_particle_impulse;

		pPhysicalEntity->SetParams(&colltype);

		// Sleep physical entity
		pe_action_awake actionAwake;
		actionAwake.bAwake = 0;
		pPhysicalEntity->Action(&actionAwake);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
uint32 CHitDeathReactions::GetRootScopeID( const IActionController* piActionController, const FragmentID fragID, const TagState hitDeathFragTagState ) const
{
	const SAnimationContext& context = piActionController->GetContext();
	ActionScopes scopeMask = context.controllerDef.GetScopeMask(fragID, SFragTagState(context.state.GetMask(), hitDeathFragTagState));
	if (!scopeMask)
	{
		return SCOPE_ID_INVALID;
	}

	uint32 rootScopeID = 0;
	while( !( scopeMask & 1 ) )
	{
		scopeMask >>= 1;
		++rootScopeID;
	}

	return rootScopeID;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
TagState CHitDeathReactions::MergeCurrentFragTagState( const IActionController* piActionController, const FragmentID fragID, const TagState hitDeathFragTagState ) const
{
	const uint32 rootScopeID = GetRootScopeID( piActionController, fragID, hitDeathFragTagState );
	if ( rootScopeID == SCOPE_ID_INVALID )
	{
		return hitDeathFragTagState;
	}

	const IScope* pScope = piActionController->GetScope( rootScopeID );
	const FragmentID lastFragID = pScope->GetLastFragmentID();

	if ( lastFragID == FRAGMENT_ID_INVALID )
	{
		return hitDeathFragTagState;
	}

	const CTagDefinition* pLastFragTagDef = piActionController->GetTagDefinition( lastFragID );
	if ( !pLastFragTagDef )
	{
		return hitDeathFragTagState;
	}

	TagState newFragTagState = hitDeathFragTagState;

	const CTagDefinition* pHitDeathFragTagDef = piActionController->GetTagDefinition( fragID );
	CRY_ASSERT( pHitDeathFragTagDef );

	const TagState lastFragTagState = pScope->GetLastTagState().fragmentTags;

	for( TagID i = 0; i < pLastFragTagDef->GetNum(); ++i )
	{
		if ( !pLastFragTagDef->IsSet( lastFragTagState, i ) )
		{
			continue;
		}

		const uint32 tagCRC = pLastFragTagDef->GetTagCRC( i );
		CRY_ASSERT( tagCRC );

		const TagID newTagID = pHitDeathFragTagDef->Find( tagCRC );
		if ( newTagID == TAG_ID_INVALID )
		{
			continue;
		}

		pHitDeathFragTagDef->Set( newFragTagState, newTagID, true );
	}

	return newFragTagState;
}

void CHitDeathReactions::GeneratePostDeathTags()
{
	const CTagDefinition* pTagDefinition = gEnv->pGameFramework->GetMannequinInterface().GetAnimationDatabaseManager().LoadTagDefs( "Animations/Mannequin/ADB/hitDeathTags.xml", true );
	
	if (!pTagDefinition)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Missing Animations/Mannequin/ADB/hitDeathTags.xml tag definition file");
		return;
	}

	const TagID tagID = pTagDefinition->Find( "post_death" );

	if (tagID != TAG_ID_INVALID)
	{
		pTagDefinition->Set( m_postDeathTagRagdoll, tagID, true );
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Could not generate \"post_death\" mannequin tags");
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CHitDeathReactionsPhysics::CHitDeathReactionsPhysics( )
: m_pOwnerHitReactions(NULL)
, m_requestCounter(0)
{

}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CHitDeathReactionsPhysics::~CHitDeathReactionsPhysics()
{
	for (int i = 0; i < kMaxQueuedPrimitives; ++i)
	{
		m_queuedPrimitives[i].Reset();
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactionsPhysics::InitWithOwner( CHitDeathReactions* pOwner )
{
	CRY_ASSERT(m_pOwnerHitReactions == NULL);
	CRY_ASSERT(pOwner != NULL);

	m_pOwnerHitReactions = pOwner;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactionsPhysics::Queue( int primitiveType, primitives::primitive &primitive, const Vec3 &sweepDir, int objTypes, int geomFlags )
{
	CRY_ASSERT(m_pOwnerHitReactions);

	int slot = GetPrimitiveRequestSlot();

	CRY_ASSERT(slot != -1);

	m_queuedPrimitives[slot].queuedId = g_pGame->GetIntersectionTester().Queue(IntersectionTestRequest::HighPriority,
											IntersectionTestRequest(primitiveType, primitive, sweepDir, objTypes, 0, geomFlags),
											functor(*this, &CHitDeathReactionsPhysics::IntersectionTestComplete));

	m_queuedPrimitives[slot].counter = ++m_requestCounter;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactionsPhysics::IntersectionTestComplete( const QueuedIntersectionID& intID, const IntersectionTestResult& result )
{
	CRY_ASSERT(m_pOwnerHitReactions);

	int slot = GetSlotFromPrimitiveId(intID);
	CRY_ASSERT(slot != -1);

	if (slot != -1)
	{
		m_queuedPrimitives[slot].queuedId = 0;
		m_queuedPrimitives[slot].counter = 0;
	}

	if ((result.distance <= 0.0f)	||	(m_pOwnerHitReactions->m_reactionOnCollision == NO_COLLISION_REACTION))		// No Collision if we are not supposed to
		return;

	CRY_ASSERT_MESSAGE(result.normal.IsValid() && !result.normal.IsZero() && result.point.IsValid(), "Receiving incorrect data from physics events, check CPhysicalWorld::TracePendingRays method");

	CActor& ownerActor = m_pOwnerHitReactions->m_actor;

	ICharacterInstance* pCharacter = ownerActor.GetEntity()->GetCharacter(0);
	CRY_ASSERT(pCharacter);

	bool bValidCollision = (m_pOwnerHitReactions->m_reactionOnCollision == 1); // [*DavidR | 26/Mar/2010] ToDo: Tidy-up this assumption of reaction on collision number 1 being the FnP/ragdoll
	if (!bValidCollision && pCharacter)
	{
		// In hit reactions we need to be more accurate with the checks, check:
		// - collision normal horizontality (reaction collisions are designed for perfect vertical obstacles)
		// - collision normal reasonably opposing movement direction (should we take velocity into account?)
		// - height?
		const float fMaxZ = m_pOwnerHitReactions->m_pHitDeathReactionsConfig->fCollMaxHorzAngleSin;
		const bool bHorizontalCheck = fabs_tpl(result.normal.z) < fMaxZ;

		const Vec3& vRelDir = pCharacter->GetISkeletonAnim()->GetRelMovement().t.GetNormalized();
		const Vec3& vAbsDir = ownerActor.GetEntity()->GetWorldTM().TransformVector(vRelDir);

		const float fMinDot = m_pOwnerHitReactions->m_pHitDeathReactionsConfig->fCollMaxMovAngleCos;
		const bool bDirectCollisionCheck = result.normal.Dot(vAbsDir) > fMinDot;

		bValidCollision = bHorizontalCheck && bDirectCollisionCheck;

		if (!bValidCollision && m_pOwnerHitReactions->IsInDeathReaction())
		{
			// In death reactions we want to fall back to ragdoll if the animated collision reaction didn't fulfil the requirements
			bValidCollision = true;
			m_pOwnerHitReactions->m_reactionOnCollision = 1;
		}
	}

#ifndef _RELEASE
	if (g_pGameCVars->g_hitDeathReactions_debug && pCharacter)
	{
		// Let's draw the normal vector and the movement vector for every collision detected, if the collision
		// is valid we draw it for more time (so it's easy to see which one was the validated)
		const Vec3& vConeDrawPos = result.point + Vec3(0.0f, 0.0f, 2.0f); 
		float fTime = bValidCollision ? 4.0f : 2.0f;
		const Vec3& vRelDir = pCharacter->GetISkeletonAnim()->GetRelMovement().t.GetNormalized();
		const Vec3& vAbsDir = ownerActor.GetEntity()->GetWorldTM().TransformVector(vRelDir);

		IPersistantDebug* pPersistantDebug = gEnv->pGameFramework->GetIPersistantDebug();
		pPersistantDebug->Begin("CHitDeathReactions::OnDataReceived", false);
		pPersistantDebug->AddCone(vConeDrawPos, result.normal, 0.1f, 0.8f, Col_Red, fTime);
		pPersistantDebug->AddCone(vConeDrawPos, vAbsDir, 0.1f, 0.8f, Col_Blue, fTime);
	}
#endif // _RELEASE

	if (bValidCollision)
	{
		m_pOwnerHitReactions->StartCollisionReaction(result.normal, result.point);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
int CHitDeathReactionsPhysics::GetPrimitiveRequestSlot()
{
	for (int i = 0; i < kMaxQueuedPrimitives; ++i)
	{
		if (m_queuedPrimitives[i].queuedId == 0)
		{
			return i;
		}
	}

	//Too many primitive requested, cancel oldest one and re-use slot
	uint32 oldestCounter = m_requestCounter + 1;
	int oldestSlot = 0;
	for (int i = 0; i < kMaxQueuedPrimitives; ++i)
	{
		if (m_queuedPrimitives[i].counter < oldestCounter)
		{
			oldestCounter = m_queuedPrimitives[i].counter;
			oldestSlot = i;
		}
	}

	m_queuedPrimitives[oldestSlot].Reset();

	return oldestSlot;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
int CHitDeathReactionsPhysics::GetSlotFromPrimitiveId( const QueuedIntersectionID& intID )
{
	for (int i = 0; i < kMaxQueuedPrimitives; ++i)
	{
		if (m_queuedPrimitives[i].queuedId == intID)
		{
			return i;
		}
	}

	return -1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactionsPhysics::SPrimitiveRequest::Reset()
{
	if (queuedId != 0)
	{
		g_pGame->GetIntersectionTester().Cancel(queuedId);
		queuedId = 0;
	}
	counter = 0;
}
