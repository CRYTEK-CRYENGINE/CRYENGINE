// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   CryGame Source File.
   -------------------------------------------------------------------------
   File name:   AIHandler.cpp
   Version:     v1.00
   Description:

   -------------------------------------------------------------------------
   History:
   - 8:10:2004   12:05 : Created by Kirill Bulatsev

 *********************************************************************/

#include "StdAfx.h"
#include <CryAISystem/IAISystem.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryNetwork/ISerialize.h>
#include <IActorSystem.h>
#include <IAnimationGraph.h>
#include "AIHandler.h"
#include "AIProxy.h"
#include "PersistantDebug.h"
#include "CryActionCVars.h"
#include <CryAISystem/IBlackBoard.h>
#include <CryAISystem/IAIObject.h>
#include <CryAISystem/IAIActor.h>
//#include <ILipSync.h>
#if defined(__GNUC__)
	#include <float.h>
#endif
#include <CryAISystem/IPathfinder.h>

#include <CryAISystem/IAIActor.h>
#include "Mannequin/AnimActionTriState.h"

// ============================================================================
// ============================================================================

static const int MANNEQUIN_NAVSO_EXACTPOS_PRIORITY = 4; // currently equal to PP_UrgentAction, just above normal actions, but underneath hit death, etc.
static const int MANNEQUIN_EXACTPOS_PRIORITY = 3;       // currently equal to PP_Action, just above movement but underneath urgent actions, hit death, etc.

// ============================================================================
// ============================================================================

class CAnimActionExactPositioning : public CAnimActionTriState
{
public:
	typedef CAnimActionTriState TBase;

	DEFINE_ACTION("ExactPositioning");

	CAnimActionExactPositioning(FragmentID fragmentID, IAnimatedCharacter& animChar, bool isOneShot, float maxMiddleDuration, bool isNavSO, const QuatT& targetLocation, IAnimActionTriStateListener* pListener);

public:
	inline static bool IsExactPositioningAction(const CAnimActionTriState* pAnimActionTriState)
	{
		return pAnimActionTriState->GetName()[0] == 'E'; // Horrible, poor man's RTTI
	}

private:
	// CAnimActionTriState implementation ---------------------------------------
	virtual void OnEnter() override;
	virtual void OnExit() override;
	// ~CAnimActionTriState implementation --------------------------------------

private:
	bool m_isNavSO;
};

// ============================================================================
// ============================================================================

// ----------------------------------------------------------------------------
CAnimActionExactPositioning::CAnimActionExactPositioning(FragmentID fragmentID, IAnimatedCharacter& animChar, bool isOneShot, float maxMiddleDuration, bool isNavSO, const QuatT& targetLocation, IAnimActionTriStateListener* pListener)
	:
	TBase(
	  isNavSO ? MANNEQUIN_NAVSO_EXACTPOS_PRIORITY : MANNEQUIN_EXACTPOS_PRIORITY,
	  fragmentID,
	  animChar,
	  isOneShot,
	  maxMiddleDuration,
	  false,
	  pListener
	  ),
	m_isNavSO(isNavSO)
{
	if (m_isNavSO)
	{
		InitMovementControlMethods(eMCM_Animation, eMCM_Animation);
		InitTargetLocation(targetLocation);
	}
	else
	{
		InitMovementControlMethods(eMCM_AnimationHCollision, eMCM_Entity);
	}
}

// ----------------------------------------------------------------------------
void CAnimActionExactPositioning::OnEnter()
{
	if (m_isNavSO)
	{
		m_animChar.RequestPhysicalColliderMode(eColliderMode_Disabled, eColliderModeLayer_Game, "CAnimActionExactPositioning::Enter()");
	}
}

// ----------------------------------------------------------------------------
void CAnimActionExactPositioning::OnExit()
{
	if (m_isNavSO)
	{
		m_animChar.ForceRefreshPhysicalColliderMode();
		m_animChar.RequestPhysicalColliderMode(eColliderMode_Undefined, eColliderModeLayer_Game, "CAnimActionExactPositioning::Exit()");
	}
}

// ============================================================================
// ============================================================================

//
//------------------------------------------------------------------------------
CAIHandler::CAIHandler(IGameObject* pGameObject)
	: m_bSoundFinished(false),
	m_pAGState(NULL),
	m_actorTargetStartedQueryID(0),
	m_actorTargetEndQueryID(0),
	m_eActorTargetPhase(eATP_None),
	m_playingSignalAnimation(false),
	m_playingActionAnimation(false),
	m_actorTargetId(0),
	m_pGameObject(pGameObject),
	m_pEntity(pGameObject->GetEntity()),
	m_FaceManager(pGameObject->GetEntity()),
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	m_bDelayedCharacterConstructor(false),
#endif
	m_bDelayedBehaviorConstructor(false),
	m_vAnimationTargetPosition(ZERO),
	m_lastTargetType(AITARGET_NONE),
	m_lastTargetThreat(AITHREAT_NONE),
	m_timeSinceEvent(0.0f),
	m_lastTargetID(0),
	m_lastTargetPos(ZERO),
	m_pScriptObject(NULL),
	m_CurrentAlertness(0),
	m_CurrentExclusive(false),
	m_bSignaledAnimationStarted(false),
	m_bAnimationStarted(false)
{
	m_curActorTargetStartedQueryID = &m_actorTargetStartedQueryID;
	m_curActorTargetEndQueryID = &m_actorTargetEndQueryID;
}

//
//------------------------------------------------------------------------------
CAIHandler::~CAIHandler(void)
{
	m_animActionTracker.ReleaseAll();

	if (m_pAGState)
	{
		m_pAGState->RemoveListener(this);
	}

	IGameObject* pGameObject = gEnv->pGameFramework->GetGameObject(m_pEntity->GetId());
	IMovementController* pMC = (pGameObject ? pGameObject->GetMovementController() : NULL);
	if (pMC)
	{
		pMC->SetExactPositioningListener(NULL);
	}

	if (*m_pPreviousBehavior == *m_pBehavior)
		m_pPreviousBehavior = 0;
}

#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
//
//------------------------------------------------------------------------------
const char* CAIHandler::GetInitialCharacterName()
{
	const char* szAICharacterName = 0;
	SmartScriptTable pEntityProperties;

	if (!m_pScriptObject->GetValue("Properties", pEntityProperties))
	{
		AIWarningID("<CAIHandler> ", "can't find Properties. Entity %s", m_pEntity->GetEntityTextDescription().c_str());
		return 0;
	}

	if (!pEntityProperties->GetValue("aicharacter_character", szAICharacterName))
	{
		AIWarningID("<CAIHandler> ", "can't find aicharacter_character. Entity %s", m_pEntity->GetEntityTextDescription().c_str());
		return 0;
	}

	return szAICharacterName;
}
#endif

//
//------------------------------------------------------------------------------
const char* CAIHandler::GetInitialBehaviorName()
{
	static const ICVar* pModularBehaviorTreeCVar = gEnv->pConsole->GetCVar("ai_ModularBehaviorTree");
	const bool isModularBehaviorTreeEnabled = (pModularBehaviorTreeCVar && pModularBehaviorTreeCVar->GetIVal() != 0);
	if (isModularBehaviorTreeEnabled)
	{
		SmartScriptTable pEntityProperties;
		if (m_pScriptObject)
		{
			m_pScriptObject->GetValue("Properties", pEntityProperties);
			if (pEntityProperties)
			{
				const char* behaviorTreeName = 0;
				if (pEntityProperties->GetValue("esModularBehaviorTree", behaviorTreeName))
				{
					const bool hasValidModularBehaviorTreeName = (behaviorTreeName && behaviorTreeName[0] != 0);
					if (hasValidModularBehaviorTreeName)
					{
						return 0;
					}
				}
			}
		}
	}

	SmartScriptTable pEntityPropertiesInstance;

	if (!m_pScriptObject || !m_pScriptObject->GetValue("PropertiesInstance", pEntityPropertiesInstance))
	{
		AIWarningID("<CAIHandler> ", "can't find PropertiesInstance. Entity %s", m_pEntity->GetName());
		return 0;
	}

	const char* szAIBehaviorName = 0;
	if (!pEntityPropertiesInstance.GetPtr())
		return 0;
	if (!pEntityPropertiesInstance->GetValue("aibehavior_behaviour", szAIBehaviorName))
		return 0;

	return szAIBehaviorName;
}

//
//------------------------------------------------------------------------------
void CAIHandler::Init()
{
	m_sQueriedActionAnimation.clear();
	m_sQueriedSignalAnimation.clear();
	m_bSignaledAnimationStarted = false;
	m_setPlayedSignalAnimations.clear();
	m_setStartedActionAnimations.clear();
	m_animActionTracker.ReleaseAll();

	m_pScriptObject = m_pEntity->GetScriptTable();

	if (!SetCommonTables())
		return;

	SetInitialBehaviorAndCharacter();

	m_pPreviousBehavior = 0;
	m_CurrentAlertness = 0;
	m_CurrentExclusive = false;

	// Precache facial animations.
	m_FaceManager.PrecacheSequences();
}

//
//------------------------------------------------------------------------------
void CAIHandler::ResetCommonTables()
{
	m_pBehaviorTable = 0;
	m_pBehaviorTableAVAILABLE = 0;
	m_pBehaviorTableINTERNAL = 0;
	m_pDEFAULTDefaultBehavior = 0;
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	m_pDefaultCharacter = 0;
#endif
}

//
//------------------------------------------------------------------------------
bool CAIHandler::SetCommonTables()
{
	// Get common tables
	if (!gEnv->pScriptSystem->GetGlobalValue("AIBehavior", m_pBehaviorTable)
	    && !gEnv->pScriptSystem->GetGlobalValue("AIBehaviour", m_pBehaviorTable))
	{
		AIWarningID("<CAIHandler> ", "Can't find AIBehavior table!");
		return false;
	}

	m_pBehaviorTable->GetValue("AVAILABLE", m_pBehaviorTableAVAILABLE);
	m_pBehaviorTable->GetValue("INTERNAL", m_pBehaviorTableINTERNAL);

	if (!m_pBehaviorTable->GetValue("DEFAULT", m_pDEFAULTDefaultBehavior))
	{
		AIWarningID("<CAIHandler> ", "can't find DEFAULT. Entity %s", m_pEntity->GetName());
		return false;
	}

#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	SmartScriptTable pCharacterTable;
	if (!gEnv->pScriptSystem->GetGlobalValue("AICharacter", pCharacterTable))
	{
		AIWarningID("<CAIHandler> ", "can't find AICharacter table. Entity %s", m_pEntity->GetName());
		return false;
	}

	if (!pCharacterTable->GetValue("DEFAULT", m_pDefaultCharacter))
	{
		AIWarningID("<CAIHandler> ", "can't find DEFAULT character. Entity %s", m_pEntity->GetName());
		return false;
	}
#endif

	return true;
}

//
//------------------------------------------------------------------------------
void CAIHandler::SetInitialBehaviorAndCharacter()
{
	const IActor* actor = GetActor();
	const bool isPlayer = actor ? actor->IsPlayer() : false;

	if (!isPlayer)
	{
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
		ResetCharacter();
#endif

		ResetBehavior();

#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
		const char* szDefaultCharacter = GetInitialCharacterName();
		m_sFirstCharacterName = szDefaultCharacter;
		if (!SetCharacter(szDefaultCharacter, SET_DELAYED) && !m_sFirstCharacterName.empty())
			AIWarningID("<CAIHandler> ", "Could not set initial character: %s on Entity: %s",
			            szDefaultCharacter, m_pEntity->GetEntityTextDescription().c_str());
#endif

		const char* szDefaultBehavior = GetInitialBehaviorName();
		m_sFirstBehaviorName = szDefaultBehavior;
		SetBehavior(szDefaultBehavior, 0, SET_DELAYED);
	}
}

//
//------------------------------------------------------------------------------
void CAIHandler::ResetAnimationData()
{
	m_sQueriedActionAnimation.clear();
	m_sQueriedSignalAnimation.clear();
	m_bSignaledAnimationStarted = false;
	m_setPlayedSignalAnimations.clear();
	m_setStartedActionAnimations.clear();

	m_playingSignalAnimation = false;
	m_currentSignalAnimName.clear();
	m_playingActionAnimation = false;
	m_currentActionAnimName.clear();
	m_sAGActionSOAutoState.clear();

	m_eActorTargetPhase = eATP_None;
	m_actorTargetId = 0;
	m_actorTargetStartedQueryID = 0;
	m_actorTargetEndQueryID = 0;
	m_curActorTargetStartedQueryID = &m_actorTargetStartedQueryID;
	m_curActorTargetEndQueryID = &m_actorTargetEndQueryID;
	m_vAnimationTargetPosition.zero();
	if (IMovementController* pMC = (m_pGameObject ? m_pGameObject->GetMovementController() : NULL))
	{
		// always clear actor target
		CMovementRequest mr;
		mr.ClearActorTarget();
		pMC->RequestMovement(mr);
	}
	m_animActionTracker.ReleaseAll();
}

//
//------------------------------------------------------------------------------
void CAIHandler::Reset(EObjectResetType type)
{
	m_FaceManager.Reset();

	ResetAnimationData();

	m_bSoundFinished = false;

	m_lastTargetType = AITARGET_NONE;
	m_lastTargetThreat = AITHREAT_NONE;
	m_lastTargetID = 0;
	m_lastTargetPos.zero();

	m_curActorTargetStartedQueryID = &m_actorTargetStartedQueryID;
	m_curActorTargetEndQueryID = &m_actorTargetEndQueryID;

	if (m_pAGState)
	{
		m_pAGState->RemoveListener(this);
		m_pAGState = NULL;
	}

	IMovementController* pMC = (m_pGameObject ? m_pGameObject->GetMovementController() : NULL);
	if (pMC)
	{
		pMC->SetExactPositioningListener(this);
	}

	m_actorTargetStartedQueryID = 0;
	m_actorTargetEndQueryID = 0;
	m_eActorTargetPhase = eATP_None;

	m_CurrentAlertness = 0;
	m_CurrentExclusive = false;

	m_pScriptObject = m_pEntity->GetScriptTable();

	if (type == AIOBJRESET_SHUTDOWN)
	{
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
		ResetCharacter();
#endif
		ResetBehavior();
		ResetCommonTables();
	}
	else
	{
		SetCommonTables();
		SetInitialBehaviorAndCharacter();
	}

	m_pPreviousBehavior = 0;
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	m_sPrevCharacterName.clear();
#endif

	m_timeSinceEvent = 0.0f;
}

//
//------------------------------------------------------------------------------
void CAIHandler::OnReused(IGameObject* pGameObject)
{
	assert(pGameObject);

	m_pGameObject = pGameObject;
	m_pEntity = pGameObject->GetEntity();

	m_FaceManager.OnReused(m_pEntity);

	// Reinit
	Init();
}

//
//------------------------------------------------------------------------------
void CAIHandler::AIMind(SOBJECTSTATE& state)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	bool sameTarget = (m_lastTargetID == state.eTargetID);
	float distSq = m_lastTargetPos.GetSquaredDistance(state.vTargetPos);

	/*Note: This needs to be event driven, not done in an update - Morgan 01/10/2011
	   //New targets should always signal
	   //Using distance in addition here, because group targets get cleared one frame AFTER individual targets.
	   //Group target ids will not match for that frame.
	 */
	if (sameTarget || distSq < 0.15)
	{
		//Sounds/Memories/Suspect targets can resignal after a period of time if the target has moved or anything within the same.
		if ((state.eTargetType == AITARGET_SOUND) ||
		    ((m_lastTargetType == AITARGET_MEMORY) && (state.eTargetType == AITARGET_MEMORY)) ||
		    (state.eTargetThreat == AITHREAT_SUSPECT))
		{
			if (!(state.eTargetType > m_lastTargetType || state.eTargetThreat > m_lastTargetThreat))
			{
				m_timeSinceEvent += gEnv->pTimer->GetFrameTime();

				if (m_timeSinceEvent < 2.0f || distSq < 0.2f)
					return;
			}
		}
		//Otherwise use old system, and assume the same target with the same threat/type should not signal.
		else if ((m_lastTargetType == state.eTargetType) && (m_lastTargetThreat == state.eTargetThreat))
		{
			m_timeSinceEvent += gEnv->pTimer->GetFrameTime();

			return;
		}
	}

	m_timeSinceEvent = 0.0f;

	EAITargetType prevType = m_lastTargetType;

	m_lastTargetType = state.eTargetType;
	m_lastTargetThreat = state.eTargetThreat;
	m_lastTargetID = state.eTargetID;
	m_lastTargetPos = state.vTargetPos;

	const char* eventString = 0;
	float* value = 0;

	IAISignalExtraData* pExtraData = NULL;
	IAIObject* pEntityAI = m_pEntity->GetAI();

	switch (state.eTargetType)
	{
	case AITARGET_MEMORY:
		//If this a fresh memory, then signal for it
		if (prevType != AITARGET_MEMORY)
		{
			if (state.eTargetThreat == AITHREAT_THREATENING)
			{
				value = &state.fDistanceFromTarget;
				eventString = "OnEnemyMemory";
			}
			else if (state.eTargetThreat == AITHREAT_AGGRESSIVE)
			{
				value = &state.fDistanceFromTarget;
				eventString = "OnLostSightOfTarget";
			}
		}
		else
		{
			if (state.eTargetThreat == AITHREAT_AGGRESSIVE)
			{
				value = &state.fDistanceFromTarget;
				eventString = "OnMemoryMoved";
			}
		}

		break;
	case AITARGET_SOUND:
		if (state.eTargetThreat >= AITHREAT_AGGRESSIVE)
		{
			value = &state.fDistanceFromTarget;
			eventString = "OnEnemyHeard";
		}
		else if (state.eTargetThreat == AITHREAT_THREATENING)
		{
			value = &state.fDistanceFromTarget;
			eventString = "OnThreateningSoundHeard";
		}
		else if (state.eTargetThreat == AITHREAT_INTERESTING)
		{
			value = &state.fDistanceFromTarget;
			eventString = "OnInterestingSoundHeard";
		}
		else if (state.eTargetThreat == AITHREAT_SUSPECT)
		{
			value = &state.fDistanceFromTarget;
			eventString = "OnSuspectedSoundHeard";
		}
		break;
	case AITARGET_VISUAL:

		if (state.nTargetType > AIOBJECT_PLAYER)   //-- grenade (or any other registered object type) seen
		{
			CRY_PROFILE_REGION(PROFILE_AI, "AI_OnObjectSeen");

			value = &state.fDistanceFromTarget;
			if (!pExtraData)
				pExtraData = gEnv->pAISystem->CreateSignalExtraData();
			pExtraData->iValue = state.nTargetType;

			IAIActor* pAIActor = CastToIAIActorSafe(pEntityAI);
			if (pAIActor)
			{
				IAIObject* pTarget = pAIActor->GetAttentionTarget();
				if (pTarget)
				{
					IEntity* pTargetEntity = (IEntity*)(pTarget->GetEntity());
					if (pTargetEntity)
					{
						pExtraData->nID = pTargetEntity->GetId();
						pExtraData->point = pTarget->GetPos();
						pe_status_dynamics sd;
						if (pTargetEntity->GetPhysics())
						{
							pTargetEntity->GetPhysics()->GetStatus(&sd);
							pExtraData->point2 = sd.v;
						}
						else
							pExtraData->point2 = ZERO;
					}
				}
			}
			eventString = "OnObjectSeen";
		}
		else if (state.eTargetThreat == AITHREAT_AGGRESSIVE)
		{
			if (!pExtraData)
				pExtraData = gEnv->pAISystem->CreateSignalExtraData();
			pExtraData->iValue = (int)state.eTargetStuntReaction;
			state.eTargetStuntReaction = AITSR_NONE;

			CRY_PROFILE_REGION(PROFILE_AI, "AI_OnPlayerSeen2");
			value = &state.fDistanceFromTarget;

			if (IAIActor* pAIActor = CastToIAIActorSafe(pEntityAI))
			{
				IAIObject* pTarget = pAIActor->GetAttentionTarget();
				// sends OnSeenByEnemy to the target
				// only if the target can acquire the sender as his target as well
				IAIActor* pTargetActor = CastToIAIActorSafe(pTarget);
				if (pTargetActor && pTargetActor->CanAcquireTarget(pEntityAI))
				{
					if (IAIObject::eFOV_Outside != pTarget->IsPointInFOV(pEntityAI->GetPos()))
					{
						gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER, 1, "OnSeenByEnemy", pTarget);
					}
				}
			}

			bool targetIsCloaked = false;
			if (IAIActor* aiActor = CastToIAIActorSafe(pEntityAI))
				if (IAIObject* targetAIObecjt = aiActor->GetAttentionTarget())
					if (IAIActor* targetAIActor = CastToIAIActorSafe(targetAIObecjt))
						targetIsCloaked = targetAIActor->GetParameters().m_bCloaked;

			if (targetIsCloaked)
			{
				const char* cloakedTargetSeenSignal = "OnCloakedTargetSeen";
				AISignal(AISIGNAL_DEFAULT, cloakedTargetSeenSignal, CCrc32::Compute(cloakedTargetSeenSignal), NULL, pExtraData);
			}

			eventString = "OnEnemySeen";
		}
		else if (state.eTargetThreat == AITHREAT_THREATENING)
		{
			eventString = "OnThreateningSeen";
		}
		else if (state.eTargetThreat == AITHREAT_INTERESTING)
		{
			eventString = "OnSomethingSeen";
		}
		else if (state.eTargetThreat == AITHREAT_SUSPECT)
		{
			eventString = "OnSuspectedSeen";
		}
		break;
	case AITARGET_NONE:
		eventString = "OnNoTarget";
		break;
	}

	if (eventString)
	{
		gEnv->pAISystem->Record(pEntityAI, IAIRecordable::E_HANDLERNEVENT, eventString);

		IAIRecordable::RecorderEventData recorderEventData(eventString);
		pEntityAI->RecordEvent(IAIRecordable::E_SIGNALRECIEVED, &recorderEventData);

		AISignal(AISIGNAL_DEFAULT, eventString, CCrc32::Compute(eventString), NULL, pExtraData);
	}

	if (pExtraData)
		gEnv->pAISystem->FreeSignalExtraData(pExtraData);
}

//
//------------------------------------------------------------------------------
void CAIHandler::AISignal(int signalID, const char* signalText, uint32 crc, IEntity* pSender, const IAISignalExtraData* pData)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(crc != 0);

	if (!signalText)
		return;

	if (signalID != AISIGNAL_PROCESS_NEXT_UPDATE)
	{
		IAIRecordable::RecorderEventData recorderEventData(signalText);
		m_pEntity->GetAI()->RecordEvent(IAIRecordable::E_SIGNALEXECUTING, &recorderEventData);
		gEnv->pAISystem->Record(m_pEntity->GetAI(), IAIRecordable::E_SIGNALEXECUTING, signalText);
	}

	if (!CallScript(m_pBehavior, signalText, NULL, pSender, pData))
	{
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
		if (!CallScript(m_pDefaultBehavior, signalText, NULL, pSender, pData))
		{
			if (CallScript(m_pDEFAULTDefaultBehavior, signalText, NULL, pSender, pData))
			{
				gEnv->pAISystem->Record(m_pEntity->GetAI(), IAIRecordable::E_SIGNALEXECUTING, "from defaultDefault behaviour");
			}
		}
		else
		{
			gEnv->pAISystem->Record(m_pEntity->GetAI(), IAIRecordable::E_SIGNALEXECUTING, "from default behaviour");
		}
#else
		if (CallScript(m_pDEFAULTDefaultBehavior, signalText, NULL, pSender, pData))
		{
			gEnv->pAISystem->Record(m_pEntity->GetAI(), IAIRecordable::E_SIGNALEXECUTING, "from defaultDefault behaviour");
		}
#endif
	}

	if (m_pEntity)
	{
		CallScript(m_pEntity->GetScriptTable(), signalText, NULL, pSender, pData);

		assert(m_pEntity->GetAI() != NULL);
		IAIActor* aiActor = m_pEntity->GetAI()->CastToIAIActor();
		if (aiActor != NULL)
		{
			aiActor->OnAIHandlerSentSignal(signalText, crc);
		}
	}

#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	if (const char* szNextBehavior = CheckAndGetBehaviorTransition(signalText))
	{
		SetBehavior(szNextBehavior, pData);
	}
#endif
}

#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
//
//------------------------------------------------------------------------------
const char* CAIHandler::CheckAndGetBehaviorTransition(const char* szSignalText) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (!szSignalText || strlen(szSignalText) < 2)
		return 0;

	SmartScriptTable pCharacterTable;
	SmartScriptTable pCharacterDefaultTable;
	SmartScriptTable pNextBehavior;
	const char* szBehaviorName = 0;
	const char* szNextBehaviorName = 0;

	if (*m_pBehavior && *m_pCharacter)
	{
		m_pBehavior->GetValue("Name", szBehaviorName);
		bool bTable = m_pCharacter->GetValue(szBehaviorName, pCharacterTable);

		if (bTable && pCharacterTable->GetValue(szSignalText, szNextBehaviorName) && !*szNextBehaviorName)
			return 0;

		bool bDefaultTable = m_pCharacter->GetValue("AnyBehavior", pCharacterDefaultTable);

		if (bTable || bDefaultTable)
		{
			if ((bTable && (pCharacterTable->GetValue(szSignalText, szNextBehaviorName) || pCharacterTable->GetValue("_ANY_", szNextBehaviorName))) ||
			    (bDefaultTable && (pCharacterDefaultTable->GetValue(szSignalText, szNextBehaviorName) || pCharacterDefaultTable->GetValue("_ANY_", szNextBehaviorName))))
			{
				if (*szNextBehaviorName)
				{
					CRY_PROFILE_REGION(PROFILE_AI, "Logging of the character change");
					if (m_pEntity && m_pEntity->GetName())
					{
						AILogCommentID("<CAIHandler> ", "Entity %s changing behavior from %s to %s on signal %s",
						               m_pEntity->GetName(), szBehaviorName ? szBehaviorName : "<null>",
						               szNextBehaviorName ? szNextBehaviorName : "<null>", szSignalText ? szSignalText : "<null>");
					}
					return szNextBehaviorName;
				}
			}
		}
	}

	if (!m_pDefaultCharacter.GetPtr())
		return 0;

	bool tableIsValid = false;
	if (*m_pBehavior)
	{
		szBehaviorName = m_sBehaviorName.c_str();
		if (!(tableIsValid = m_pDefaultCharacter->GetValue(szBehaviorName, pCharacterTable)))
			tableIsValid = m_pDefaultCharacter->GetValue("NoBehaviorFound", pCharacterTable);
	}
	else
		tableIsValid = m_pDefaultCharacter->GetValue("NoBehaviorFound", pCharacterTable);

	if (tableIsValid && pCharacterTable->GetValue(szSignalText, szNextBehaviorName) && *szNextBehaviorName)
	{
		CRY_PROFILE_REGION(PROFILE_AI, "Logging of DEFAULT character change");
		if (m_pEntity && m_pEntity->GetName())
		{
			AILogCommentID("<CAIHandler> ", "Entity %s changing behavior from %s to %s on signal %s [DEFAULT character]",
			               m_pEntity->GetName(), szBehaviorName ? szBehaviorName : "<null>",
			               szNextBehaviorName ? szNextBehaviorName : "<null>", szSignalText ? szSignalText : "<null>");
		}
		return szNextBehaviorName;
	}

	return 0;
}

//
//------------------------------------------------------------------------------
void CAIHandler::ResetCharacter()
{
	// call destructor of current character
	if (m_pCharacter.GetPtr())
	{
		HSCRIPTFUNCTION pDestructor = 0;
		if (m_pCharacter->GetValue("Destructor", pDestructor))
		{
			gEnv->pScriptSystem->BeginCall(pDestructor);
			gEnv->pScriptSystem->PushFuncParam(m_pScriptObject);
			gEnv->pScriptSystem->EndCall();
		}
	}

	m_pCharacter = 0;
	m_sCharacterName.clear();
	m_sDefaultBehaviorName.clear();
	if (m_pScriptObject)
		m_pScriptObject->SetValue("DefaultBehaviour", m_sDefaultBehaviorName.c_str());
	m_pDefaultBehavior = 0;
	m_bDelayedCharacterConstructor = false;
}

//
//------------------------------------------------------------------------------
bool CAIHandler::SetCharacter(const char* szCharacter, ESetFlags setFlags)
{
	if (!szCharacter || !*szCharacter)
	{
		ResetCharacter();
		return false;
	}

	// only if it's a different one
	if (m_sCharacterName == szCharacter)
		return true;

	SmartScriptTable pCharacterTable; // points to global table AICharacter
	if (!gEnv->pScriptSystem->GetGlobalValue("AICharacter", pCharacterTable))
	{
		ResetCharacter();
		return false;
	}

	// check is specified character already loaded
	SmartScriptTable pCharacter; // should point to next character table
	if (!pCharacterTable->GetValue(szCharacter, pCharacter))
	{
		SmartScriptTable pAvailableCharacter;
		if (!pCharacterTable->GetValue("AVAILABLE", pAvailableCharacter) && !pCharacterTable->GetValue("INTERNAL", pAvailableCharacter))
		{
			ResetCharacter();
			return false;
		}

		// get file name for specified character
		const char* szFileName = 0;
		if (!pAvailableCharacter->GetValue(szCharacter, szFileName))
		{
			ResetCharacter();
			return false;
		}

		// load the character table
		if (!gEnv->pScriptSystem->ExecuteFile(szFileName, false, true))
		{
			ResetCharacter();
			return false;
		}

		// now try to get the table once again
		if (!pCharacterTable->GetValue(szCharacter, pCharacter))
		{
			ResetCharacter();
			return false;
		}
	}

	// call destructor of current character
	if (setFlags != SET_ON_SERILIAZE)
	{
		if (m_pCharacter.GetPtr())
		{
			HSCRIPTFUNCTION pDestructor = 0;
			if (m_pCharacter->GetValue("Destructor", pDestructor))
			{
				gEnv->pScriptSystem->BeginCall(pDestructor);
				gEnv->pScriptSystem->PushFuncParam(m_pScriptObject);
				gEnv->pScriptSystem->EndCall();
			}
		}
	}

	m_sPrevCharacterName = m_sCharacterName;
	m_sCharacterName = szCharacter;

	m_pCharacter = pCharacter;

	// adjust default behavior name entry in entity script table
	m_sDefaultBehaviorName = szCharacter;
	m_sDefaultBehaviorName += "Idle";
	m_pScriptObject->SetValue("DefaultBehaviour", m_sDefaultBehaviorName.c_str());
	if (!m_pBehaviorTable.GetPtr())
	{
		AIWarningID("<CAIHandler> ", "%s m_pBehaviorTable not set up", m_pEntity->GetName());
		return false;
	}
	if (!FindOrLoadTable(m_pBehaviorTable, m_sDefaultBehaviorName.c_str(), m_pDefaultBehavior))
		AIWarningID("<CAIHandler> ", "can't find default behaviour %s", m_sDefaultBehaviorName.c_str());

	if (setFlags == SET_DELAYED)
	{
		m_bDelayedCharacterConstructor = true;
	}
	else if (setFlags == SET_IMMEDIATE)
	{
		m_bDelayedCharacterConstructor = false;
		CallCharacterConstructor();
	}

	return true;
}

//
//------------------------------------------------------------------------------
const char* CAIHandler::GetCharacter()
{
	return m_sCharacterName.c_str();
}
#endif

//
//------------------------------------------------------------------------------
void CAIHandler::ResetBehavior()
{
	// Finish up with the old behavior.
	if (m_pBehavior.GetPtr())
	{
		int noPrevious = 0;
		if (!m_pBehavior->GetValue("NOPREVIOUS", noPrevious))
			m_pPreviousBehavior = m_pBehavior;
		// executing behavior destructor
		CallScript(m_pBehavior, "Destructor", 0, 0, 0);
		gEnv->pAISystem->Record(m_pEntity->GetAI(), IAIRecordable::E_BEHAVIORDESTRUCTOR, m_sBehaviorName);
	}

	m_sBehaviorName.clear();
	m_pBehavior = 0;
	SetAlertness(0, false);
	m_CurrentExclusive = false;
	m_bDelayedBehaviorConstructor = false;
}

//
//------------------------------------------------------------------------------
void CAIHandler::SetBehavior(const char* szNextBehaviorName, const IAISignalExtraData* pData, ESetFlags setFlags)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	SmartScriptTable pNextBehavior;

	if (szNextBehaviorName)
	{
		if (strcmp(szNextBehaviorName, "PREVIOUS") == 0)
		{
			pNextBehavior = m_pPreviousBehavior;
			if (pNextBehavior.GetPtr())
			{
				const char* szCurBehaviorName = "";
				pNextBehavior->GetValue("name", szCurBehaviorName);

				// Skip ctor/dtor if previous is the same as current behavior.
				if (m_sBehaviorName == szCurBehaviorName)
					return;

				m_sBehaviorName = szNextBehaviorName;
			}
		}
		else
		{
			if (strcmp(szNextBehaviorName, "FIRST") == 0)
				szNextBehaviorName = m_sFirstBehaviorName.c_str();

			FindOrLoadBehavior(szNextBehaviorName, pNextBehavior);
		}
	}

	IAIObject* aiObject = m_pEntity->GetAI();

	// Finish up with the old behavior.
	if (setFlags != SET_ON_SERILIAZE)
	{
		if (m_pBehavior.GetPtr())
		{
			int noPrevious = 0;
			if (!m_pBehavior->GetValue("NOPREVIOUS", noPrevious))
			{
				m_pPreviousBehavior = m_pBehavior;
			}

			// executing behavior destructor
			CallScript(m_pBehavior, "Destructor", 0, 0, pData);

			gEnv->pAISystem->Record(aiObject, IAIRecordable::E_BEHAVIORDESTRUCTOR, m_sBehaviorName);

			if (aiObject)
			{
				IAIRecordable::RecorderEventData recorderEventData(m_sBehaviorName);
				aiObject->RecordEvent(IAIRecordable::E_BEHAVIORDESTRUCTOR, &recorderEventData);
			}
		}
	}

	// Switch behavior
	m_sBehaviorName = szNextBehaviorName;
	m_pBehavior = pNextBehavior;

	if (m_pScriptObject)
	{
		m_pScriptObject->SetValue("Behavior", m_pBehavior);
	}

	gEnv->pAISystem->Record(aiObject, IAIRecordable::E_BEHAVIORSELECTED, m_sBehaviorName);

	if (aiObject)
	{
		IAIRecordable::RecorderEventData recorderEventData(m_sBehaviorName);
		aiObject->RecordEvent(IAIRecordable::E_BEHAVIORSELECTED, &recorderEventData);
	}

	int alertness = m_CurrentAlertness;
	int exclusive = 0;

	// Initialize the new behavior
	if (m_pBehavior.GetPtr())
	{
		m_pBehavior->GetValue("Alertness", alertness) || m_pBehavior->GetValue("alertness", alertness);
		m_pBehavior->GetValue("Exclusive", exclusive) || m_pBehavior->GetValue("exclusive", exclusive);

		bool leaveCoverOnStart = true;
		m_pBehavior->GetValue("LeaveCoverOnStart", leaveCoverOnStart);
		if (leaveCoverOnStart)
		{
			if (IPipeUser* pipeUser = CastToIPipeUserSafe(m_pEntity->GetAI()))
				pipeUser->SetInCover(false);
		}

		if (setFlags == SET_DELAYED)
		{
			m_bDelayedBehaviorConstructor = true;
			if (pData)
			{
				AIWarningID("<CAIHandler::CallScript> ", "SetBehavior: signal extra data ignored for delayed constructor (PipeUser: '%s'; Behaviour: '%s')",
				            m_pEntity->GetName(), szNextBehaviorName);
			}
		}
		else if (setFlags == SET_IMMEDIATE)
		{
			// executing behavior constructor
			IAIRecordable::RecorderEventData recorderEventData(szNextBehaviorName);
			aiObject->RecordEvent(IAIRecordable::E_BEHAVIORCONSTRUCTOR, &recorderEventData);

			m_bDelayedBehaviorConstructor = false;
			CallBehaviorConstructor(pData);
		}
	}

	if (aiObject)
	{
		if (IAIActorProxy* proxy = aiObject->GetProxy())
		{
			if (IAIActor* actor = aiObject->CastToIAIActor())
			{
				// TODO(márcio): Save a ref here instead of going around the world to get to the other side
				const char* currentName = proxy->GetCurrentBehaviorName();
				const char* previousName = proxy->GetPreviousBehaviorName();

				actor->BehaviorChanged(currentName, previousName);
			}
		}
	}

	// Update alertness levels
	bool bOnAlert = m_CurrentAlertness == 0 && alertness > 0;
	SetAlertness(alertness, false);

	// Update behavior exclusive flag
	m_CurrentExclusive = exclusive > 0;

	if (setFlags != SET_ON_SERILIAZE)
	{
		if (bOnAlert)
		{
			SEntityEvent event(ENTITY_EVENT_SCRIPT_EVENT);
			event.nParam[0] = (INT_PTR)"OnAlert";
			event.nParam[1] = IEntityClass::EVT_BOOL;
			bool bValue = true;
			event.nParam[2] = (INT_PTR)&bValue;
			m_pEntity->SendEvent(event);
		}
	}
}

//
//-------------------------------------------------------------------------------------------------
void CAIHandler::FindOrLoadBehavior(const char* szBehaviorName, SmartScriptTable& pBehaviorTable)
{
	if (!szBehaviorName || !szBehaviorName[0])
	{
		pBehaviorTable = 0;
		return;
	}

	if (!m_pBehaviorTable.GetPtr())
		return;

	if (!m_pBehaviorTable->GetValue(szBehaviorName, pBehaviorTable))
	{
		//[petar] if behaviour not preloaded then force loading of it
		CRY_PROFILE_REGION(PROFILE_AI, "On-DemandBehaviourLoading");
		const char* szAIBehaviorFileName = GetBehaviorFileName(szBehaviorName);
		if (szAIBehaviorFileName)
		{
			//fixme - problem with reloading!!!!
			gEnv->pScriptSystem->ExecuteFile(szAIBehaviorFileName, true, true);

			// Márcio: We need to load base behaviors here too!
		}

		if (!m_pBehaviorTable->GetValue(szBehaviorName, pBehaviorTable))
		{
			if (m_pEntity && m_pEntity->GetName())
			{
				AIWarningID("<CAIHandler> ", "entity %s failed to change behavior to %s.",
				            m_pEntity->GetName(), szBehaviorName ? szBehaviorName : "<null>");
			}
		}
	}
}

#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
//
//------------------------------------------------------------------------------
void CAIHandler::CallCharacterConstructor()
{
	// call character constructor
	if (m_pCharacter.GetPtr())
	{
		// execute character constructor
		HSCRIPTFUNCTION pConstructor = 0;
		if (m_pCharacter->GetValue("Constructor", pConstructor))
		{
			gEnv->pScriptSystem->BeginCall(pConstructor);
			gEnv->pScriptSystem->PushFuncParam(m_pCharacter);
			gEnv->pScriptSystem->PushFuncParam(m_pScriptObject);
			gEnv->pScriptSystem->EndCall();
			gEnv->pScriptSystem->ReleaseFunc(pConstructor);
		}
	}
}
#endif

//
//------------------------------------------------------------------------------
void CAIHandler::CallBehaviorConstructor(const IAISignalExtraData* pData)
{
	if (m_pBehavior.GetPtr())
	{
		gEnv->pAISystem->Record(m_pEntity->GetAI(), IAIRecordable::E_BEHAVIORCONSTRUCTOR, m_sBehaviorName);

		IAIActor* pActor = m_pEntity->GetAI()->CastToIAIActor();
		if (pActor != NULL)
		{
			//bool bHas = m_pBehavior->HaveValue( "BlackBoardInit" );

			SmartScriptTable behBBInit;
			if (m_pBehavior->GetValue("BlackBoardInit", behBBInit) == false)
			{
				pActor->GetBehaviorBlackBoard()->Clear();
			}
			else
			{
				pActor->GetBehaviorBlackBoard()->SetFromScript(behBBInit);
			}
		}

		CallScript(m_pBehavior, "Constructor", 0, 0, pData);

		const char* szEventToCallName = 0;
		if (m_pScriptObject->GetValue("EventToCall", szEventToCallName) && *szEventToCallName)
		{
			CallScript(m_pBehavior, szEventToCallName);
			m_pScriptObject->SetValue("EventToCall", "");
		}
	}
}

//
//------------------------------------------------------------------------------
void CAIHandler::ResendTargetSignalsNextFrame()
{
	m_lastTargetID = 0;
	m_lastTargetPos.zero();
}

//
//------------------------------------------------------------------------------
const char* CAIHandler::GetBehaviorFileName(const char* szBehaviorName)
{
	const char* szFileName = 0;
	if (m_pBehaviorTableAVAILABLE.GetPtr() && m_pBehaviorTableAVAILABLE->GetValue(szBehaviorName, szFileName))
		return szFileName;
	else if (m_pBehaviorTableINTERNAL.GetPtr() && m_pBehaviorTableINTERNAL->GetValue(szBehaviorName, szFileName))
		return szFileName;
	return 0;
}

//
//------------------------------------------------------------------------------
IActor* CAIHandler::GetActor() const
{
	CRY_ASSERT(m_pEntity);
	if (!m_pEntity)
		return NULL;
	IActorSystem* pASystem = gEnv->pGameFramework->GetIActorSystem();
	if (!pASystem)
		return NULL;
	return pASystem->GetActor(m_pEntity->GetId());
}

//
//------------------------------------------------------------------------------
bool CAIHandler::CallScript(IScriptTable* scriptTable, const char* funcName, float* pValue, IEntity* pSender, const IAISignalExtraData* pData)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (scriptTable)
	{
		HSCRIPTFUNCTION functionToCall = 0;

		bool bFound = scriptTable->GetValue(funcName, functionToCall);
		if (!bFound)
		{
			//try parent behaviours
			const char* szTableBase;
			if (scriptTable->GetValue("Base", szTableBase))
			{
				SmartScriptTable scriptTableBase;
				if (m_pBehaviorTable->GetValue(szTableBase, scriptTableBase))
					return CallScript(scriptTableBase, funcName, pValue, pSender, pData);
			}
		}

		if (bFound)
		{
#if !defined(_RELEASE) && !defined(CONSOLE_CONST_CVAR_MODE)
			IPersistantDebug* pPD = CCryAction::GetCryAction()->GetIPersistantDebug();

			// TODO : (MATT) I don't think we usually do this with statics {2008/01/23:16:30:25}
			static ICVar* pAIShowBehaviorCalls = NULL;
			if (!pAIShowBehaviorCalls)
				pAIShowBehaviorCalls = gEnv->pConsole->GetCVar("ai_ShowBehaviorCalls");

			if (pAIShowBehaviorCalls && pAIShowBehaviorCalls->GetIVal())
			{
				SEntityTagParams params;
				char* behaviour = "";
				scriptTable->GetValue("Name", behaviour);
				params.entity = m_pEntity->GetId();
				params.text.Format("%s::%s", behaviour, funcName);
				params.fadeTime = 10.0f;
				params.visibleTime = 5.0f;
				pPD->AddEntityTag(params);
			}
#endif
			//			string str="Calling behavior >> ";
			//			str+= funcName;
			//		CRY_PROFILE_REGION(PROFILE_AI, "Calling behavior signal");
			//		CRY_PROFILE_REGION(PROFILE_AI, str.c_str());

			// only use strings which are known at compile time...
			// not doing so causes a stack corruption in the frame profiler -- CW
			//		FRAME_PROFILER( funcName,m_pGame->GetSystem(),PROFILE_AI );
			//cry_sprintf(m_szSignalName,"AISIGNAL: %s",funcName);
			CRY_PROFILE_REGION(PROFILE_AI, "AISIGNAL");

#ifdef AI_LOG_SIGNALS
			static ICVar* pCVarAILogging = NULL;
			CTimeValue start;
			uint64 pagefaults = 0;
			if (CCryActionCVars::Get().aiLogSignals)
			{
				if (!pCVarAILogging)
					pCVarAILogging = gEnv->pConsole->GetCVar("ai_enablewarningserrors");
				//float start = pCVarAILogging && pCVarAILogging->GetIVal() ? gEnv->pTimer->GetAsyncCurTime() : 0;

				if (pCVarAILogging && pCVarAILogging->GetIVal())
				{
					start = gEnv->pTimer->GetAsyncTime();
					IMemoryManager::SProcessMemInfo memCounters;
					gEnv->pSystem->GetIMemoryManager()->GetProcessMemInfo(memCounters);
					pagefaults = memCounters.PageFaultCount;
				}
			}
#endif

			IScriptSystem* pScriptSystem = gEnv->pScriptSystem;
			pScriptSystem->BeginCall(functionToCall);

			/*
			   Francesco: This parameter was used to pass the behavior table as a first
			   parameter of  LUA functions. Since now we call functions also on the entity
			   table we can have situation in which the scriptTable and the m_pScriptObject
			   are the same LUA table.
			   We can avoid then to push scriptTable as a parameter to allow writing lua functions
			   of the type

			   OnFunctionCall = function(entity, sender, data)
			   ...
			   end,
			 */

			if (scriptTable != m_pScriptObject)
				pScriptSystem->PushFuncParam(scriptTable);          // self

			pScriptSystem->PushFuncParam(m_pScriptObject);

			IScriptTable* pScriptTable = pSender ? pSender->GetScriptTable() : NULL;
			if (pScriptTable)
			{
				pScriptSystem->PushFuncParam(pScriptTable);
			}
			else
			{
				if (pValue)
				{
					pScriptSystem->PushFuncParam(*pValue);
				}
				else
				{
					pScriptSystem->PushFuncParamAny(EScriptAnyType::Nil);
				}
			}

			if (pData)
			{
				SmartScriptTable pScriptEData(pScriptSystem);
				pData->ToScriptTable(pScriptEData);
				pScriptSystem->PushFuncParam(*pScriptEData);
			}

			pScriptSystem->EndCall();
			pScriptSystem->ReleaseFunc(functionToCall);

#ifdef AI_LOG_SIGNALS
			if (CCryActionCVars::Get().aiLogSignals)
			{
				if (pCVarAILogging && pCVarAILogging->GetIVal())
				{
					start = gEnv->pTimer->GetAsyncTime() - start;
					IMemoryManager::SProcessMemInfo memCounters;
					gEnv->pSystem->GetIMemoryManager()->GetProcessMemInfo(memCounters);
					pagefaults = memCounters.PageFaultCount - pagefaults;

				}
				if (start.GetMilliSeconds() > CCryActionCVars::Get().aiMaxSignalDuration)
				{
					char* behaviour = "";
					scriptTable->GetValue("Name", behaviour);
					AIWarningID("<CAIHandler::CallScript> ", "Handling AISIGNAL '%s' takes %g ms! (PipeUser: '%s'; Behaviour: '%s')",
					            funcName, start.GetMilliSeconds(), m_pEntity->GetName(), behaviour);

					char buffer[256];
					cry_sprintf(buffer, "%s.%s,%g,%d,%s", behaviour, funcName, start.GetMilliSeconds(), (int)pagefaults, m_pEntity->GetName());
					gEnv->pAISystem->Record(m_pEntity->GetAI(), IAIRecordable::E_SIGNALEXECUTEDWARNING, buffer);
				}
			}
#endif

			return true;
		}
	}
	return false;
}

//
//------------------------------------------------------------------------------
void CAIHandler::Release()
{
	delete this;
}

//
//------------------------------------------------------------------------------
//IScriptTable* CAIHandler::GetMostLikelyTable( IScriptTable* table )
bool CAIHandler::GetMostLikelyTable(IScriptTable* table, SmartScriptTable& destTable)
{
	int numAnims = table->Count();
	if (!numAnims)
		return false;

	int rangeMin = 0;
	int rangeMax = 0;
	int selAnim = -1;

	int usedCount = 0;
	int maxProb = 0;
	int totalProb = 0;

	// Check the available animations
	//	CryLog("GetMostLikelyTable:");
	for (int i = 0; i < numAnims; ++i)
	{
		table->GetAt(i + 1, destTable);
		float fProb = 0;
		destTable->GetValue("PROBABILITY", fProb);
		//		CryLog("%d - %d", i+1, (int)fProb);
		totalProb += (int)fProb;

		int isUsed = 0;
		if (destTable->GetValue("USED", isUsed))
		{
			//			CryLog("%d - USED", i+1);
			usedCount++;
			continue;
		}

		maxProb += (int)fProb;
	}

	if (totalProb < 1000)
		maxProb += 1000 - totalProb;

	// If all anims has been used already, reset.
	if (usedCount == numAnims)
	{
		for (int i = 0; i < numAnims; ++i)
		{
			table->GetAt(i + 1, destTable);
			destTable->SetToNull("USED");
		}
		maxProb = 1000;
	}

	// Choose among the possible choices
	int probability = cry_random(0, maxProb - 1);

	for (int i = 0; i < numAnims; ++i)
	{
		table->GetAt(i + 1, destTable);

		// Skip already used ones.
		int isUsed = 0;
		if (destTable->GetValue("USED", isUsed))
			continue;

		float fProb = 0;
		destTable->GetValue("PROBABILITY", fProb);
		rangeMin = rangeMax;
		rangeMax += (int)fProb;

		if (probability >= rangeMin && probability < rangeMax)
		{
			selAnim = i + 1;
			break;
		}
	}

	if (selAnim == -1)
	{
		//		CryLog(">> not found!");
		return false;
	}

	//	CryLog(">> found=%d", selAnim);

	table->GetAt(selAnim, destTable);
	destTable->SetValue("USED", 1);

	return true;
}

void CAIHandler::SetAlertness(int value, bool triggerEvent /*=false*/)
{
	assert(value <= 2);

	if (value == -1 || m_CurrentAlertness == value)
		return;

	bool switchToIdle = m_CurrentAlertness > 0 && value == 0;
	bool switchToWeaponAlerted = m_CurrentAlertness == 0 && value > 0;

	m_CurrentAlertness = value;

	if (triggerEvent && switchToWeaponAlerted)
	{
		SEntityEvent event(ENTITY_EVENT_SCRIPT_EVENT);
		event.nParam[0] = (INT_PTR)"OnAlert";
		event.nParam[1] = IEntityClass::EVT_BOOL;
		bool bValue = true;
		event.nParam[2] = (INT_PTR)&bValue;
		m_pEntity->SendEvent(event);
	}

	IActor* const pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(m_pEntity->GetId());
	if (pActor)
	{
		if (switchToWeaponAlerted)
		{
			SEntityEvent radarEvent(ENTITY_EVENT_ADD_TO_RADAR);
			pActor->ProcessEvent(radarEvent);
		}
		else if (switchToIdle)
		{
			SEntityEvent radarEvent(ENTITY_EVENT_REMOVE_FROM_RADAR);
			pActor->ProcessEvent(radarEvent);
		}
	}

	CAIFaceManager::e_ExpressionEvent expression(CAIFaceManager::EE_NONE);
	const char* sStates = "";
	switch (m_CurrentAlertness)
	{
	case 0: // Idle
		sStates = "Idle-Alerted,Combat";
		expression = CAIFaceManager::EE_IDLE;
		break;
	case 1: // Alerted
		sStates = "Alerted-Idle,Combat";
		expression = CAIFaceManager::EE_ALERT;
		break;
	case 2: // Combat
		sStates = "Combat-Idle,Alerted";
		expression = CAIFaceManager::EE_COMBAT;
		break;
	}
	MakeFace(expression);

	if (sStates)
		gEnv->pAISystem->GetSmartObjectManager()->ModifySmartObjectStates(m_pEntity, sStates);
}

//
//------------------------------------------------------------------------------
IActionController* CAIHandler::GetActionController()
{
	IActor* pActor = GetActor();
	if (!pActor)
		return NULL;

	IAnimatedCharacter* pAnimatedCharacter = pActor->GetAnimatedCharacter();
	if (!pAnimatedCharacter)
		return NULL;

	IActionController* pActionController = pAnimatedCharacter->GetActionController();
	return pActionController;
}

//
//------------------------------------------------------------------------------
IAnimationGraphState* CAIHandler::GetAGState()
{
	if (m_pAGState)
		return m_pAGState;

	IActor* pActor = GetActor();
	if (!pActor)
		return NULL;
	m_pAGState = pActor->GetAnimationGraphState();
	if (!m_pAGState)
		return NULL;

	m_pAGState->AddListener("AIHandler", this);
	return m_pAGState;
}

bool CAIHandler::IsAnimationBlockingMovement() const
{
	IActor* pActor = const_cast<IActor*>(GetActor());
	if (pActor)
	{
		const IAnimatedCharacter* pAnimChar = pActor->GetAnimatedCharacter();
		return pAnimChar && (pAnimChar->GetMCMH() == eMCM_Animation || pAnimChar->GetMCMH() == eMCM_AnimationHCollision)
		       && (m_bSignaledAnimationStarted || m_playingActionAnimation);
	}
	else
	{
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIHandler::HandleCoverRequest(SOBJECTSTATE& state, CMovementRequest& mr)
{
	IActionController* pActionController = GetActionController();
	if (pActionController)
	{
		// Safety check to handle invalid coverlocations
		{
			const EStance stance = (EStance)state.bodystate;
			const bool isInCoverStance = (stance == STANCE_HIGH_COVER) || (stance == STANCE_LOW_COVER);
			if (isInCoverStance)
			{
				if (state.coverRequest.coverLocationRequest == eCoverLocationRequest_Set)
				{
					QuatT& coverLocation = state.coverRequest.coverLocation;
					CRY_ASSERT(coverLocation.IsValid());

					const float TOO_FAR_AWAY = 50.0f;
					if (coverLocation.t.GetSquaredDistance2D(m_pEntity->GetPos()) >= sqr(TOO_FAR_AWAY))
					{
						CRY_ASSERT_MESSAGE(false, "Cover stance requested but CoverLocation is too far away from the entity");
						state.coverRequest.coverLocationRequest = eCoverLocationRequest_Clear;
					}
				}
			}
		}

		mr.SetAICoverRequest(state.coverRequest);
	}

	state.coverRequest = SAICoverRequest();
}

//////////////////////////////////////////////////////////////////////////
void CAIHandler::HandleMannequinRequest(SOBJECTSTATE& state, CMovementRequest& mr)
{
	SMannequinTagRequest tagRequest;

	IActionController* pActionController = GetActionController();
	if (pActionController)
	{
		const CTagDefinition& tagDef = pActionController->GetContext().controllerDef.m_tags;
		const aiMannequin::SCommand* pCommand = state.mannequinRequest.GetFirstCommand();
		while (pCommand)
		{
			const uint16 commandType = pCommand->m_type;
			switch (commandType)
			{
			case aiMannequin::eMC_SetTag:
				{
					const aiMannequin::STagCommand* pTagCommand = static_cast<const aiMannequin::STagCommand*>(pCommand);
					const uint32 tagCrc = pTagCommand->m_tagCrc;
					const TagID tagId = tagDef.Find(tagCrc);
					if (tagId != TAG_ID_INVALID)
					{
						tagDef.Set(tagRequest.setTags, tagId, true);
					}
				}
				break;

			case aiMannequin::eMC_ClearTag:
				{
					const aiMannequin::STagCommand* pTagCommand = static_cast<const aiMannequin::STagCommand*>(pCommand);
					const uint32 tagCrc = pTagCommand->m_tagCrc;
					const TagID tagId = tagDef.Find(tagCrc);
					if (tagId != TAG_ID_INVALID)
					{
						tagDef.Set(tagRequest.clearTags, tagId, true);
						tagDef.Set(tagRequest.setTags, tagId, false);
					}
				}
				break;

			default:
				CRY_ASSERT(false);
			}

			pCommand = state.mannequinRequest.GetNextCommand(pCommand);
		}
	}

	mr.SetMannequinTagRequest(tagRequest);
	state.mannequinRequest.ClearCommands();
}

// ----------------------------------------------------------------------------
CAnimActionExactPositioning* CAIHandler::CreateExactPositioningAction(bool isOneShot, const float loopDuration, const char* szFragmentID, bool isNavigationalSO, const QuatT& exactStartLocation)
{
	IActor* pActor = GetActor();
	if (!pActor)
		return NULL;

	IAnimatedCharacter* pAnimatedCharacter = pActor->GetAnimatedCharacter();
	if (!pAnimatedCharacter)
		return NULL;

	IActionController* pActionController = pAnimatedCharacter->GetActionController();
	if (!pActionController)
		return NULL;

	const uint32 valueCRC = CCrc32::ComputeLowercase(szFragmentID);

	bool isDefaultValue = true;
	if (isOneShot)
	{
		static const uint32 noneCRC = CCrc32::ComputeLowercase("none");
		isDefaultValue = (valueCRC == noneCRC);
	}
	else
	{
		static const uint32 idleCRC = CCrc32::ComputeLowercase("idle");
		isDefaultValue = (valueCRC == idleCRC);
	}

	if (!valueCRC)
	{
		CRY_ASSERT(false); // no signal or action was selected, or we're extremely unlucky and hit a 0 CRC
		return NULL;
	}

	CRY_ASSERT(!isDefaultValue);
	if (isDefaultValue)
		return NULL;

	FragmentID fragmentID = pActionController->GetContext().controllerDef.m_fragmentIDs.Find(valueCRC);
	if (fragmentID == FRAGMENT_ID_INVALID)
	{
		const IEntity* pEntity = pAnimatedCharacter->GetEntity();

		CryWarning(
		  VALIDATOR_MODULE_GAME,
		  VALIDATOR_WARNING,
		  "AIHandler: Fragment '%s' not found (entity = '%s'; controllerDef = '%s')",
		  szFragmentID,
		  pEntity ? pEntity->GetName() : "<invalid>",
		  pActionController->GetContext().controllerDef.m_filename.c_str());

		return NULL;
	}

	const bool keepTrackOfAction = !isNavigationalSO;
	CAnimActionExactPositioning* pAction = new CAnimActionExactPositioning(fragmentID, *pAnimatedCharacter, isOneShot, isOneShot ? -1 : loopDuration, isNavigationalSO, exactStartLocation, keepTrackOfAction ? this : NULL);

	if (keepTrackOfAction)
		m_animActionTracker.Add(pAction);

	return pAction;
}

//
//------------------------------------------------------------------------------
void CAIHandler::HandleExactPositioning(SOBJECTSTATE& state, CMovementRequest& mr)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	IMovementController* pMC = (m_pGameObject ? m_pGameObject->GetMovementController() : NULL);
	if (!pMC)
		return;

	// Register this as the target point verifier to ensure we know when EP position target has been reached
	pMC->SetExactPositioningListener(this);

	// The AIsystem is reported back the same phase that we handle here.
	state.curActorTargetPhase = m_eActorTargetPhase;

	if (m_actorTargetId != state.actorTargetReq.id)
	{
		const SExactPositioningTarget* animTgt = pMC->GetExactPositioningTarget();

		if ((!animTgt || !animTgt->activated) &&
		    state.curActorTargetPhase != eATP_Started &&
		    state.curActorTargetPhase != eATP_Playing &&
		    state.curActorTargetPhase != eATP_StartedAndFinished &&
		    state.curActorTargetPhase != eATP_Finished)
		{
			if (state.actorTargetReq.id == 0)
			{
				m_eActorTargetPhase = eATP_None;
				*m_curActorTargetStartedQueryID = 0;
				*m_curActorTargetEndQueryID = 0;
				mr.ClearActorTarget();
			}
			else
			{
				m_eActorTargetPhase = eATP_Waiting;

				// if the requester passes us IDs use them instead of the local ones.
				if (state.actorTargetReq.pQueryStart && state.actorTargetReq.pQueryEnd)
				{
					m_curActorTargetStartedQueryID = state.actorTargetReq.pQueryStart;
					m_curActorTargetEndQueryID = state.actorTargetReq.pQueryEnd;
				}
				else
				{
					m_curActorTargetStartedQueryID = &m_actorTargetStartedQueryID;
					m_curActorTargetEndQueryID = &m_actorTargetEndQueryID;
				}

				*m_curActorTargetStartedQueryID = 0;
				*m_curActorTargetEndQueryID = 0;

				SActorTargetParams actorTarget;
				actorTarget.location = state.actorTargetReq.animLocation;
				actorTarget.startArcAngle = state.actorTargetReq.startArcAngle;
				actorTarget.startWidth = state.actorTargetReq.startWidth;
				actorTarget.direction = state.actorTargetReq.animDirection.GetNormalizedSafe(ZERO);
				actorTarget.directionTolerance = state.actorTargetReq.directionTolerance;
				if (actorTarget.direction.IsZero())
				{
					actorTarget.direction = FORWARD_DIRECTION;
					actorTarget.directionTolerance = gf_PI;
				}
				actorTarget.speed = -1;
				actorTarget.vehicleName = state.actorTargetReq.vehicleName;
				actorTarget.vehicleSeat = state.actorTargetReq.vehicleSeat;
				actorTarget.projectEnd = actorTarget.vehicleName.empty() && state.actorTargetReq.projectEndPoint;
				actorTarget.navSO = state.actorTargetReq.lowerPrecision;

				actorTarget.stance = state.actorTargetReq.stance;
				actorTarget.pQueryStart = m_curActorTargetStartedQueryID;
				actorTarget.pQueryEnd = m_curActorTargetEndQueryID;

				CAnimActionExactPositioning* pAction = CreateExactPositioningAction(
				  state.actorTargetReq.signalAnimation,
				  state.actorTargetReq.loopDuration,
				  state.actorTargetReq.animation,
				  actorTarget.navSO,
				  QuatT(actorTarget.location, Quat::CreateRotationV0V1(FORWARD_DIRECTION, actorTarget.direction)));

				actorTarget.pAction = pAction;

				actorTarget.completeWhenActivated = false;

				mr.SetActorTarget(actorTarget);
			}

			m_vAnimationTargetPosition.zero();
			state.curActorTargetPhase = m_eActorTargetPhase;
			m_actorTargetId = state.actorTargetReq.id;
			return;
		}
	}

	if (m_eActorTargetPhase == eATP_Waiting)
	{
		//		if(state.actorTargetPhase == eATP_Waiting)
		{
			const SExactPositioningTarget* animTgt = pMC->GetExactPositioningTarget();

			if (!animTgt)
			{
				// exact positioning request was made but was invalid or cleared after some time without any notification :-(

				// Jonas: This failure is not acceptable.
				assert(0);
				m_eActorTargetPhase = eATP_Error;
				state.curActorTargetPhase = eATP_Error;

				//				m_actorTargetStartedQueryID = 0;
				//			m_bAnimationStarted = false;
			}
		}
	}
	else if (m_eActorTargetPhase == eATP_Starting)
	{
		// Waiting for starting position.
	}
	else if (m_eActorTargetPhase == eATP_Started)
	{
		// Wait until we get a animation target back too.
		m_eActorTargetPhase = eATP_Playing;
	}
	else if (m_eActorTargetPhase == eATP_Playing)
	{
		// empty
	}
	else if (m_eActorTargetPhase == eATP_StartedAndFinished)
	{
		m_eActorTargetPhase = eATP_None;
	}
	else if (m_eActorTargetPhase == eATP_Finished)
	{
		m_eActorTargetPhase = eATP_None;
	}
	else if (m_eActorTargetPhase == eATP_Error)
	{
		if (state.actorTargetReq.id == 0)
			m_eActorTargetPhase = eATP_None;
	}

	state.curActorTargetFinishPos = m_vAnimationTargetPosition;

	if (m_eActorTargetPhase == eATP_None)
	{
		// make sure there's nothing hanging around in the system
		mr.ClearActorTarget();
		m_vAnimationTargetPosition.zero();
		m_actorTargetStartedQueryID = 0;
		m_actorTargetEndQueryID = 0;
	}
}

void CAIHandler::QueryComplete(TAnimationGraphQueryID queryID, bool succeeded)
{
	if (queryID == *m_curActorTargetStartedQueryID)
	{
		if (succeeded)
		{
			if (m_eActorTargetPhase == eATP_Waiting)
			{
				m_eActorTargetPhase = eATP_Starting;
				m_vAnimationTargetPosition.zero();
				m_qAnimationTargetOrientation.SetIdentity();
			}
			else
			{
				// Jonas: This is not acceptable.
				m_eActorTargetPhase = eATP_Error;
			}
		}
		else
		{
			// Jonas: This is not acceptable.
			assert(0);
			m_eActorTargetPhase = eATP_Error;
		}
		//		*m_curActorTargetStartedQueryID = 0;
	}
	else if (queryID == *m_curActorTargetEndQueryID)
	{
		if (succeeded)
		{
			// It is possible the the animation starts and stops before the AI gets a change to update.
			if (m_eActorTargetPhase == eATP_Starting || m_eActorTargetPhase == eATP_Started)
			{
				m_eActorTargetPhase = eATP_StartedAndFinished;
			}
			else if (m_eActorTargetPhase == eATP_Playing)
			{
				m_eActorTargetPhase = eATP_Finished;
			}
			else
			{
				CRY_ASSERT(0);
				m_eActorTargetPhase = eATP_Error;
			}
		}
		else
		{
			// Jonas: This is not acceptable.
			assert(0);
			m_eActorTargetPhase = eATP_Error;
		}
		//		*m_curActorTargetEndQueryID = 0;
	}
}

void CAIHandler::DestroyedState(IAnimationGraphState* pAGState)
{
	if (pAGState == m_pAGState)
		m_pAGState = NULL;
}

//
//----------------------------------------------------------------------------------------------------------
void CAIHandler::ExactPositioningNotifyFinishPoint(const Vec3& pt)
{
	m_vAnimationTargetPosition = pt;
	if (m_eActorTargetPhase == eATP_Starting || m_eActorTargetPhase == eATP_Waiting)
		m_eActorTargetPhase = eATP_Started;
}

void CAIHandler::ExactPositioningQueryComplete(TExactPositioningQueryID queryID, bool succeeded)
{
	if (queryID == *m_curActorTargetStartedQueryID)
	{
		if (succeeded)
		{
			if (m_eActorTargetPhase == eATP_Waiting)
			{
				m_eActorTargetPhase = eATP_Starting;
				m_vAnimationTargetPosition.zero();
				m_qAnimationTargetOrientation.SetIdentity();
			}
			else
			{
				// Jonas: This is not acceptable.
				m_eActorTargetPhase = eATP_Error;
			}
		}
		else
		{
			// Jonas: This is not acceptable.
			assert(0);
			m_eActorTargetPhase = eATP_Error;
		}
		//		*m_curActorTargetStartedQueryID = 0;
	}
	else if (queryID == *m_curActorTargetEndQueryID)
	{
		if (succeeded)
		{
			// It is possible the the animation starts and stops before the AI gets a change to update.
			if (m_eActorTargetPhase == eATP_Starting || m_eActorTargetPhase == eATP_Started)
			{
				m_eActorTargetPhase = eATP_StartedAndFinished;
			}
			else if (m_eActorTargetPhase == eATP_Playing)
			{
				m_eActorTargetPhase = eATP_Finished;
			}
			else
			{
				CRY_ASSERT(0);
				m_eActorTargetPhase = eATP_Error;
			}
		}
		else
		{
			// Jonas: This is not acceptable.
			assert(0);
			m_eActorTargetPhase = eATP_Error;
		}
		//		*m_curActorTargetEndQueryID = 0;
	}
}

//
//------------------------------------------------------------------------------
CTimeValue CAIHandler::GetEstimatedAGAnimationLength(EAIAGInput input, const char* value)
{
	CTimeValue length;

	IAnimationGraphState* pAGState = GetAGState();
	if (!pAGState)
		return length;

	IAnimationGraphExistanceQuery* pQuery = NULL;
	for (int layerIndex = 0; pQuery = pAGState->CreateExistanceQuery(layerIndex); layerIndex++)
	{
		pQuery->SetInput(GetAGInputName(input), value);

		if (pQuery->Complete())
		{
			length = max<CTimeValue>(length, pQuery->GetAnimationLength());
		}

		pQuery->Release();
	}

	return length;
}

//
//------------------------------------------------------------------------------
bool CAIHandler::SetAGInput(EAIAGInput input, const char* value, const bool isUrgent)
{
	IActionController* pActionController = GetActionController();
	if (pActionController)
		return SetMannequinAGInput(pActionController, input, value, isUrgent);

	return false;
}

bool CAIHandler::ResetAGInput(EAIAGInput input)
{
	IActionController* pActionController = GetActionController();
	if (pActionController)
		return ResetMannequinAGInput(input);

	return false;
}

bool CAIHandler::IsSignalAnimationPlayed(const char* value)
{
	if (stl::member_find_and_erase(m_setPlayedSignalAnimations, value))
	{
		return true;
	}
	return m_sQueriedSignalAnimation != value;
}

bool CAIHandler::IsActionAnimationStarted(const char* value)
{
	if (stl::member_find_and_erase(m_setStartedActionAnimations, value))
	{
		return true;
	}
	return m_sQueriedActionAnimation != value;
}

//
//------------------------------------------------------------------------------
bool CAIHandler::FindOrLoadTable(IScriptTable* pGlobalTable, const char* szTableName, SmartScriptTable& tableOut)
{
	if (pGlobalTable->GetValue(szTableName, tableOut))
		return true;

	SmartScriptTable pAvailableTable;
	if (!pGlobalTable->GetValue("AVAILABLE", pAvailableTable))
	{
		AIWarningID("<CAIHandler> ", "AVAILABLE table not found. Aborting loading table");
		return false;
	}

	const char* szFileName = 0;
	if (!pAvailableTable->GetValue(szTableName, szFileName))
	{
		SmartScriptTable pInternalTable;
		if (!pGlobalTable->GetValue("INTERNAL", pInternalTable) ||
		    !pInternalTable->GetValue(szTableName, szFileName))
		{
			return false;
		}
	}

	if (gEnv->pScriptSystem->ExecuteFile(szFileName, true, false))
	{
		if (pGlobalTable->GetValue(szTableName, tableOut))
			return true;
	}

	// could not load script for character
	AIWarningID("<CAIHandler> ", "can't load script for character [%s]. Entity %s", szFileName, m_pEntity->GetName());

	return false;
}

//
//------------------------------------------------------------------------------
void CAIHandler::OnDialogLoaded(struct ILipSync* pLipSync)
{
}

//
//----------------------------------------------------------------------------------------------------------
void CAIHandler::OnDialogFailed(struct ILipSync* pLipSync)
{
}

//
//----------------------------------------------------------------------------------------------------------
void CAIHandler::Serialize(TSerialize ser)
{
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	ser.Value("m_sCharacterName", m_sCharacterName);
#endif
	// Must be serialized before the character/behavior is set below.
	ser.Value("m_CurrentAlertness", m_CurrentAlertness);
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	ser.Value("m_bDelayedCharacterConstructor", m_bDelayedCharacterConstructor);
#endif
	ser.Value("m_bDelayedBehaviorConstructor", m_bDelayedBehaviorConstructor);
	ser.Value("m_CurrentExclusive", m_CurrentExclusive);
	ser.Value("m_bSoundFinished", m_bSoundFinished);
	ser.Value("m_timeSinceEvent", m_timeSinceEvent);
	ser.Value("m_lastTargetID", m_lastTargetID);
	ser.Value("m_lastTargetPos", m_lastTargetPos);

	if (ser.IsReading())
	{
		m_lastTargetThreat = AITHREAT_NONE;
		m_lastTargetType = AITARGET_NONE;
	}

	// serialize behavior
	if (ser.IsReading())
	{
		SetInitialBehaviorAndCharacter();
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
		SetCharacter(m_sCharacterName.c_str(), SET_ON_SERILIAZE);
#endif
	}

	SerializeScriptAI(ser);

	IAnimationGraphState* pAGState = GetAGState();
	if (ser.IsReading())
	{
		// TODO: Serialize the face manager instead of calling Reset().
		m_FaceManager.Reset();

		ResetAnimationData();
	}

	ser.EnumValue("m_eActorTargetPhase", m_eActorTargetPhase, eATP_None, eATP_Error);
	ser.Value("m_actorTargetId", m_actorTargetId);
	ser.Value("m_vAnimationTargetPosition", m_vAnimationTargetPosition);
	if (ser.IsReading())
	{
		switch (m_eActorTargetPhase)
		{
		case eATP_Waiting:
			// exact positioning animation has been requested but not started yet.
			// since it isn't serialized in the animation graph we have to request it again
			m_eActorTargetPhase = eATP_None;
			m_actorTargetId = 0;
			break;
		case eATP_Starting:
		case eATP_Started:
		case eATP_Playing:
			// exact positioning animation has been started and playing.
			// since it isn't serialized in the animation graph we need to know when animations is done.
			// TODO: try to find a better way to track the progress of animation.
			// TODO: make this work with vehicles.
			if (pAGState)
			{
				if (GetActionController())
				{
					m_eActorTargetPhase = eATP_None;
					m_actorTargetId = 0;
				}
				else
				{
					m_curActorTargetEndQueryID = &m_actorTargetEndQueryID;
					pAGState->QueryLeaveState(
					  m_curActorTargetEndQueryID);
				}
			}
			break;
		}
	}
}

//
//------------------------------------------------------------------------------
void CAIHandler::SerializeScriptAI(TSerialize& ser)
{
	CHECK_SCRIPT_STACK;

	if (m_pScriptObject && m_pScriptObject->HaveValue("OnSaveAI") && m_pScriptObject->HaveValue("OnLoadAI"))
	{
		SmartScriptTable persistTable(m_pScriptObject->GetScriptSystem());
		if (ser.IsWriting())
			Script::CallMethod(m_pScriptObject, "OnSaveAI", persistTable);
		ser.Value("ScriptData", persistTable.GetPtr());
		if (ser.IsReading())
			Script::CallMethod(m_pScriptObject, "OnLoadAI", persistTable);
	}
}

//
//------------------------------------------------------------------------------
void CAIHandler::Update()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	if (m_bDelayedCharacterConstructor)
	{
		CallCharacterConstructor();
		m_bDelayedCharacterConstructor = false;
	}
#endif

	if (m_bDelayedBehaviorConstructor)
	{
		CallBehaviorConstructor(0);
		m_bDelayedBehaviorConstructor = false;
	}

	/*	if (m_sCharacterName.empty())
	   {
	    const char* szDefaultCharacter = GetInitialCharacterName();
	    m_sFirstCharacterName = szDefaultCharacter;
	    SetCharacter(szDefaultCharacter);
	   }

	   if (m_sBehaviorName.empty())
	   {
	    const char* szDefaultBehavior = GetInitialBehaviorName();
	    m_sFirstBehaviorName = szDefaultBehavior;
	    m_pScriptObject->SetValue("DefaultBehaviour", szDefaultBehavior);
	    SetBehavior(szDefaultBehavior);
	   }*/

	m_FaceManager.Update();
}

//
//------------------------------------------------------------------------------
void CAIHandler::MakeFace(CAIFaceManager::e_ExpressionEvent expression)
{
	m_FaceManager.SetExpression(expression);
}

//
//------------------------------------------------------------------------------
/*static*/ const char* CAIHandler::GetAGInputName(EAIAGInput input)
{
	switch (input)
	{
	case AIAG_SIGNAL:
		return "Signal";
	case AIAG_ACTION:
		return "Action";
	default:
		{
			CRY_ASSERT_MESSAGE(false, "Invalid input name");
			return NULL;
		}
	}
}

//
//------------------------------------------------------------------------------
void CAIHandler::DoReadibilityPackForAIObjectsOfType(unsigned short int nType, const char* szText, float fResponseDelay)
{
	IAIObject* pAI = m_pEntity->GetAI();
	if (!pAI || !pAI->IsEnabled())
	{
		return;
	}

	IAIActor* pAIActor = pAI->CastToIAIActor();
	int groupId = pAI->GetGroupId();
	IAISystem* pAISystem = gEnv->pAISystem;
	int count = pAISystem->GetGroupCount(groupId, IAISystem::GROUP_ENABLED, nType);
	float nearestDist = FLT_MAX;
	IAIObject* nearestInGroup = 0;

	for (int i = 0; i < count; ++i)
	{
		IAIObject* responder = pAISystem->GetGroupMember(groupId, i, IAISystem::GROUP_ENABLED, nType);
		// Skip self and puppets without proxies
		if (responder == pAI || !responder->GetProxy()) continue;
		CAIProxy* proxy = (CAIProxy*)responder->GetProxy();
		if (!proxy->GetAIHandler()) continue;
		// Choose nearest
		float d = Distance::Point_PointSq(pAI->GetPos(), responder->GetPos());
		if (d < nearestDist)
		{
			nearestDist = d;
			nearestInGroup = responder;
		}
	}
	// Send response request.
	if (nearestInGroup)
	{
		IAISignalExtraData* pData = pAISystem->CreateSignalExtraData();
		pData->iValue = 0;                                                              // Default Priority.
		pData->fValue = fResponseDelay > 0.0f ? fResponseDelay : cry_random(2.5f, 4.f); // Delay
		pAISystem->SendSignal(SIGNALFILTER_READABILITYRESPONSE, 1, szText, nearestInGroup, pData);
	}
}

//
//------------------------------------------------------------------------------
void CAIHandler::OnAnimActionTriStateEnter(CAnimActionTriState* pActionTriState)
{
	if (CAnimActionExactPositioning::IsExactPositioningAction(pActionTriState))
		return;

	CAnimActionAIAction* pAction = static_cast<CAnimActionAIAction*>(pActionTriState);

	string value = pAction->GetValue();
	if (pAction->GetType() == CAnimActionAIAction::EAT_Looping)
	{
		m_currentActionAnimName = value;
		m_playingActionAnimation = true;
	}
	else if (pAction->GetType() == CAnimActionAIAction::EAT_OneShot)
	{
		m_currentSignalAnimName = value;
		m_playingSignalAnimation = true;
	}
}

//
//------------------------------------------------------------------------------
void CAIHandler::OnAnimActionTriStateMiddle(CAnimActionTriState* pActionTriState)
{
	if (CAnimActionExactPositioning::IsExactPositioningAction(pActionTriState))
		return;

	CAnimActionAIAction* pAction = static_cast<CAnimActionAIAction*>(pActionTriState);

	string value = pAction->GetValue();
	if (pAction->GetType() == CAnimActionAIAction::EAT_Looping)
	{
		CRY_ASSERT(m_currentActionAnimName == value);
		CRY_ASSERT(m_playingActionAnimation);

		m_setStartedActionAnimations.insert(value);
		if (m_sQueriedActionAnimation == value)
			m_sQueriedActionAnimation.clear();
	}
	else if (pAction->GetType() == CAnimActionAIAction::EAT_OneShot)
	{
		CRY_ASSERT(m_currentSignalAnimName == value);
		CRY_ASSERT(m_playingSignalAnimation);

		m_playingSignalAnimation = false;
		m_bSignaledAnimationStarted = true;
	}
}

//
//------------------------------------------------------------------------------
void CAIHandler::OnAnimActionTriStateExit(CAnimActionTriState* pActionTriState)
{
	if (!CAnimActionExactPositioning::IsExactPositioningAction(pActionTriState))
	{
		CAnimActionAIAction* pAction = static_cast<CAnimActionAIAction*>(pActionTriState);

		string value = pAction->GetValue();
		if (pAction->GetType() == CAnimActionAIAction::EAT_Looping)
		{
			if (m_currentActionAnimName == value)
			{
				m_playingActionAnimation = false;
				m_currentActionAnimName.clear();
			}
		}
		else if (pAction->GetType() == CAnimActionAIAction::EAT_OneShot)
		{
			m_setPlayedSignalAnimations.insert(value);

			// TODO: Check out the special case where you select the same signal
			// animation while it is still playing
			if (m_sQueriedSignalAnimation == value)
				m_sQueriedSignalAnimation.clear();

			if (m_currentSignalAnimName == value)
			{
				m_bSignaledAnimationStarted = false;
				m_currentSignalAnimName.clear();
			}
		}
	}

	m_animActionTracker.Remove(pActionTriState);
}

//
//------------------------------------------------------------------------------
bool CAIHandler::SetMannequinAGInput(IActionController* pActionController, EAIAGInput input, const char* value, const bool isUrgent)
{
	const uint32 valueCRC = CCrc32::ComputeLowercase(value);
	const FragmentID fragmentID = pActionController->GetContext().controllerDef.m_fragmentIDs.Find(valueCRC);

	if (input == AIAG_ACTION)
	{
		static const uint32 idleCRC = CCrc32::ComputeLowercase("idle");
		const bool isIdleValue = (valueCRC == idleCRC);
		if ((fragmentID != FRAGMENT_ID_INVALID) && !isIdleValue)
		{
			stl::member_find_and_erase(m_setStartedActionAnimations, m_sQueriedActionAnimation);
			m_sQueriedActionAnimation = value;
			CAnimActionAIAction* pAnimAction = new CAnimActionAIAction(isUrgent, fragmentID, *GetActor()->GetAnimatedCharacter(), CAnimActionAIAction::EAT_Looping, value, *this);
			m_animActionTracker.Add(pAnimAction);
			pActionController->Queue(*pAnimAction);

			return true;
		}
		else
		{
			if (!isIdleValue)
			{
				//if (IPipeUser* pPipeUser = CastToIPipeUserSafe(m_pEntity->GetAI()))
				//{
				//	string message = string::Format("I cannot play looping animation '%s'", value);
				//	AIQueueBubbleMessage("CAIHandler::SetAGInput AIAG_ACTION Invalid value", pPipeUser, message.c_str(), eBNS_Log|eBNS_Balloon);
				//}
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CAIHandler::SetAGInput: Unable to find looping animation '%s'", value);
			}

			// Do we really want to cancel the currently playing action here in case of invalid fragmentid?
			if (m_animActionTracker.IsNotEmpty())
			{
				stl::member_find_and_erase(m_setStartedActionAnimations, m_sQueriedActionAnimation);
				if (isUrgent)
					m_animActionTracker.ForceFinishAll();
				else
					m_animActionTracker.StopAll();
			}

			m_sQueriedActionAnimation.clear();

			if (isIdleValue)
			{
				FinishRunningAGActions();
			}

			return false;
		}
	} // if ACTION
	else if (input == AIAG_SIGNAL)
	{
		//if ( m_sQueriedSignalAnimation == value )
		//{
		//	// Really? Do we really want this?  Don't we want the signal to trigger the animation AGAIN?
		//	return true;
		//}
		static const uint32 noneCRC = CCrc32::ComputeLowercase("none");
		const bool isNoneValue = (valueCRC == noneCRC);
		if ((fragmentID != FRAGMENT_ID_INVALID) && !isNoneValue)
		{
			stl::member_find_and_erase(m_setPlayedSignalAnimations, value);
			m_sQueriedSignalAnimation = value;

			CAnimActionAIAction* pAnimAction = new CAnimActionAIAction(isUrgent, fragmentID, *GetActor()->GetAnimatedCharacter(), CAnimActionAIAction::EAT_OneShot, value, *this);
			m_animActionTracker.Add(pAnimAction);
			pActionController->Queue(*pAnimAction);

			return true;
		}
		else
		{
			if (!isNoneValue)
			{
				//if ( IPipeUser* pPipeUser = CastToIPipeUserSafe( m_pEntity->GetAI() ) )
				//{
				//	string message = string::Format( "I cannot play looping animation '%s'", value );
				//	AIQueueBubbleMessage( "CAIHandler::SetAGInput AIAG_SIGNAL Invalid value", pPipeUser, message.c_str(), eBNS_Log|eBNS_Balloon );
				//}
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CAIHandler::SetAGInput: Unable to find one-shot animation '%s'", value);
			}

			// Do we really want to cancel the currently playing action here when we have an invalid fragmentid
			if (m_animActionTracker.IsNotEmpty())
			{
				stl::member_find_and_erase(m_setPlayedSignalAnimations, m_sQueriedSignalAnimation);
				if (isUrgent)
					m_animActionTracker.ForceFinishAll();
				else
					m_animActionTracker.StopAll();
			}

			m_sQueriedSignalAnimation.clear();

			return false;
		}
	} // if SIGNAL
	else
	{
		return false;
	}
}

//
//------------------------------------------------------------------------------
bool CAIHandler::ResetMannequinAGInput(EAIAGInput input)
{
	if (input == AIAG_ACTION)
	{
		if (m_animActionTracker.IsNotEmpty())
		{
			stl::member_find_and_erase(m_setStartedActionAnimations, m_sQueriedActionAnimation);   // wasn't this missing in the normal implementation?
			m_animActionTracker.StopAllActionsOfType(false /*Looping*/);
		}
	}
	else if (input == AIAG_SIGNAL)
	{
		if (m_animActionTracker.IsNotEmpty())
		{
			stl::member_find_and_erase(m_setStartedActionAnimations, m_sQueriedActionAnimation);   // wasn't this missing in the normal implementation?
			m_animActionTracker.StopAllActionsOfType(true /*OneShot*/);
		}
	}

	// What about the other inputs?

	return true;
}

//
//------------------------------------------------------------------------------
void CAIHandler::FinishRunningAGActions()
{
	IAnimationGraphState* pAGState = GetAGState();
	if (pAGState)
		pAGState->SetInput(GetAGInputName(AIAG_ACTION), "idle");
}

//
//------------------------------------------------------------------------------
void CAIHandler::SAnimActionTracker::ReleaseAll()
{
	TAnimActionVector::iterator itEnd = m_animActions.end();
	for (TAnimActionVector::iterator it = m_animActions.begin(); it != itEnd; ++it)
	{
		AnimActionPtr& animAction = *it;
		animAction->UnregisterListener();
		if (animAction->GetStatus() != IAction::None)
			animAction->ForceFinish();
	}
	m_animActions.clear();
}

//
//------------------------------------------------------------------------------
void CAIHandler::SAnimActionTracker::ForceFinishAll()
{
	for (TAnimActionVector::iterator it = m_animActions.begin(); it != m_animActions.end(); )
	{
		AnimActionPtr& animAction = *it;
		if (animAction->GetStatus() == IAction::None)
		{
			// let go of action as the exit callback won't get called
			it = m_animActions.erase(it);
		}
		else if (animAction->GetStatus() == IAction::Pending)
		{
			// let go of action as the exit callback won't get called
			it = m_animActions.erase(it);
			animAction->ForceFinish();
		}
		else
		{
			animAction->ForceFinish();
			++it;
		}
	}
}

//
//------------------------------------------------------------------------------
void CAIHandler::SAnimActionTracker::StopAll()
{
	for (TAnimActionVector::iterator it = m_animActions.begin(); it != m_animActions.end(); )
	{
		AnimActionPtr& animAction = *it;
		if (animAction->GetStatus() == IAction::None)
		{
			// let go of action as the exit callback won't get called
			it = m_animActions.erase(it);
		}
		else if (animAction->GetStatus() == IAction::Pending)
		{
			// let go of action as the exit callback won't get called
			it = m_animActions.erase(it);
			animAction->Stop();
		}
		else
		{
			animAction->Stop();
			++it;
		}
	}
}

//
//------------------------------------------------------------------------------
void CAIHandler::SAnimActionTracker::StopAllActionsOfType(bool isOneShot)
{
	for (TAnimActionVector::iterator it = m_animActions.begin(); it != m_animActions.end(); )
	{
		AnimActionPtr& animAction = *it;
		if (animAction->IsOneShot() == isOneShot)
		{
			if (animAction->GetStatus() == IAction::None)
			{
				// let go of action as the exit callback won't get called
				it = m_animActions.erase(it);
			}
			else if (animAction->GetStatus() == IAction::Pending)
			{
				// let go of action as the exit callback won't get called
				it = m_animActions.erase(it);
				animAction->Stop();
			}
			else
			{
				animAction->Stop();
				++it;
			}
		}
		else
		{
			++it;
		}
	}
}

//
//------------------------------------------------------------------------------
void CAIHandler::SAnimActionTracker::Remove(CAnimActionTriState* pAction)
{
	TAnimActionVector::iterator itEnd = m_animActions.end();
	for (TAnimActionVector::iterator it = m_animActions.begin(); it != itEnd; ++it)
	{
		AnimActionPtr& animAction = *it;
		if (animAction.get() == pAction)
		{
			m_animActions.erase(it);
			break;
		}
	}
}

//
//------------------------------------------------------------------------------
void CAIHandler::SAnimActionTracker::Add(CAnimActionTriState* pAction)
{
	m_animActions.push_back(AnimActionPtr(pAction));
}
