// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Implementation of power struggle objectives
	-------------------------------------------------------------------------
	History:
	- 21:03:2011  : Created by Colin Gulliver

*************************************************************************/

#include "StdAfx.h"
#include "GameRulesObjective_PowerStruggle.h"
#include <CrySystem/XML/IXml.h>
#include "GameRules.h"
#include "Player.h"
#include "GameRulesModules/IGameRulesScoringModule.h"
#include "GameRulesModules/IGameRulesSpawningModule.h"
#include "GameRulesModules/IGameRulesStateModule.h"
#include "GameRulesModules/GameRulesStandardVictoryConditionsTeam.h"
#include "Utility/CryWatch.h"
#include "Utility/CryDebugLog.h"
#include "Utility/DesignerWarning.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "Audio/Announcer.h"
#include "Game.h"
#include "PersistantStats.h"

#include "StatsRecordingMgr.h"

CGameRulesObjective_PowerStruggle::SNodeIdentityInfo CGameRulesObjective_PowerStruggle::s_nodeIdentityInfo[] = 
	{ SNodeIdentityInfo(eAII_unknown_id, "")
	, SNodeIdentityInfo(eAII_spearA_id, "A")
	, SNodeIdentityInfo(eAII_spearB_id, "B")
	, SNodeIdentityInfo(eAII_spearC_id, "C")};

#define DbgLog(...) CRY_DEBUG_LOG(GAMEMODE_POWERSTRUGGLE, __VA_ARGS__)
#define DbgLogAlways(...) CRY_DEBUG_LOG_ALWAYS(GAMEMODE_POWERSTRUGGLE, __VA_ARGS__)

	#define FATAL_ASSERT(expr) if (!(expr)) { CryFatalError("CGameRulesObjective_PowerStruggle - FatalError"); }

#if NUM_ASPECTS > 8
	#define POWER_STRUGGLE_ASPECT										eEA_GameServerA
#else
	#define POWER_STRUGGLE_ASPECT										eEA_GameServerStatic
#endif

#define ICON_STRING_WHITE_COLOUR		"#FFFFFF"

void CGameRulesObjective_PowerStruggle::SNodeInfo::SetState(ENodeState inState)
{
	if (inState != m_state)
	{
		CGameRules *pGameRules = g_pGame->GetGameRules();	// TODO cache or get access to our parent m_pGameRules
		CryLog("CGameRulesObjective_PowerStruggle::SNodeInfo::SetState() entity=%s state %d -> %d", pGameRules->GetEntityName(m_id), m_state, inState);

		m_state = inState;

		if (gEnv->bServer)
		{
			CHANGED_NETWORK_STATE(pGameRules, POWER_STRUGGLE_ASPECT);
		}

		if( m_state == ENS_captured || m_state == ENS_neutral )
		{
			m_soloCaptureState = ESolo_Available;
			m_soloCapturerId = -1;
		}
	}
}

const float k_manage_active_spears_initial_time = 0.1f; 

//------------------------------------------------------------------------
CGameRulesObjective_PowerStruggle::CGameRulesObjective_PowerStruggle()
	: m_pGameRules(NULL),
		m_iconNeutralNode(EGRMO_Unknown),
		m_iconFriendlyNode(EGRMO_Unknown),
		m_iconHostileNode(EGRMO_Unknown),
		m_iconFriendlyCapturingNode(EGRMO_Unknown),
		m_iconHostileCapturingNode(EGRMO_Unknown),
		m_iconFriendlyCapturingNodeFromHostile(EGRMO_Unknown),
		m_iconHostileCapturingNodeFromFriendly(EGRMO_Unknown),
		m_iconContentionOverNode(EGRMO_Unknown),
		m_captureSignalId(INVALID_AUDIOSIGNAL_ID),
		m_spear_going_up(INVALID_AUDIOSIGNAL_ID),
		m_positiveSignalId(INVALID_AUDIOSIGNAL_ID),
		m_negativeSignalId(INVALID_AUDIOSIGNAL_ID),
		m_scoreTimerLength(1.0f),
		m_timeTillScore(0.0f),
		m_invCaptureIncrementMultiplier(0.0f),
		m_captureDecayMultiplier(0.0f),
		m_chargeSpeed(0.0f),
		m_captureTimePercentageReduction(0.0f),
		m_avoidSpawnPOIDistance(0.0f),
		m_timeTillManageActiveNodes(0.0f),
		m_manageActiveSpearsTimer(k_manage_active_spears_initial_time),
		m_haveDisplayedInitialSpearsEmergingMsg(false),
		m_spearCameraShakeIdToUse(666),
		m_haveDisplayedProgressBar(false),
		m_setupHasHadNodeWithNoLetterSet(false),
		m_setupIsSpecifyingNodeLetters(false)
{
}

//------------------------------------------------------------------------
CGameRulesObjective_PowerStruggle::~CGameRulesObjective_PowerStruggle()
{
	IGameRulesSpawningModule *pSpawningModule = m_pGameRules->GetSpawningModule();

	for (int i = 0; i < MAX_NODES; ++ i)
	{
		SNodeInfo *pNodeInfo = &m_nodes[i];

		EntityId nodeId = pNodeInfo->m_id;

		gEnv->pEntitySystem->RemoveEntityEventListener(nodeId, ENTITY_EVENT_ENTERAREA, this);
		gEnv->pEntitySystem->RemoveEntityEventListener(nodeId, ENTITY_EVENT_LEAVEAREA, this);

		if (pSpawningModule)
		{
			pSpawningModule->RemovePOI(nodeId);
		}
	}

	m_pGameRules->UnRegisterClientConnectionListener(this);
	m_pGameRules->UnRegisterKillListener(this);
}

//------------------------------------------------------------------------
void CGameRulesObjective_PowerStruggle::Init( XmlNodeRef xml )
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	m_pGameRules = pGameRules;
	FATAL_ASSERT(m_pGameRules);

	int numChildren = xml->getChildCount();
	for (int i = 0; i < numChildren; ++ i)
	{
		XmlNodeRef xmlChild = xml->getChild(i);
		const char *pTag = xmlChild->getTag();
		if (!stricmp(pTag, "Params"))
		{
			xmlChild->getAttr("scoreTimerLength", m_scoreTimerLength);

			float captureIncrementMultiplier = 0.0f;
			xmlChild->getAttr("captureIncrementMultiplier", captureIncrementMultiplier);
			m_invCaptureIncrementMultiplier = 1.0f / captureIncrementMultiplier;

			xmlChild->getAttr("captureDecayMultiplier", m_captureDecayMultiplier);
			xmlChild->getAttr("chargeSpeed", m_chargeSpeed);
			xmlChild->getAttr("additionalAttackerTimeReductionPercentage", m_captureTimePercentageReduction);
			xmlChild->getAttr("avoidSpawnPOIDistance", m_avoidSpawnPOIDistance);
		}
		else if (!stricmp(pTag, "Icons"))
		{
			m_iconNeutralNode						= SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("neutralNode"));
			m_iconFriendlyNode					= SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("friendlyNode"));
			m_iconHostileNode						= SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("hostileNode"));
			m_iconFriendlyCapturingNode	= SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("friendlyCapturingNode"));
			m_iconHostileCapturingNode	= SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("hostileCapturingNode"));
			m_iconFriendlyCapturingNodeFromHostile = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("friendlyCapturingNodeFromHostile"));
			m_iconHostileCapturingNodeFromFriendly = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("hostileCapturingNodeFromFriendly"));
			m_iconContentionOverNode = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("contentionOverNode"));
		}
		else if (!stricmp(pTag, "Strings"))
		{
			const char *pString = NULL;

#define GET_STRING_FROM_XML(name, result) \
			if (xmlChild->getAttr(name, &pString))	\
			{	\
				result.Format("@%s", pString);	\
			} \
			else \
			{ \
				DesignerWarning(0, string().Format("failed to find expected stringID %s within PowerStruggle.xml", name)); \
			} \


			GET_STRING_FROM_XML("friendlyCaptured", m_stringFriendlyCaptured);
			GET_STRING_FROM_XML("hostileCaptured", m_stringHostileCaptured);
			GET_STRING_FROM_XML("enemyCapturingASpear", m_stringEnemyCapturingASpear);
			GET_STRING_FROM_XML("friendlyCapturingASpear", m_stringFriendlyCapturingASpear);
			GET_STRING_FROM_XML("weHaveNoSpears", m_stringWeHaveNoSpears);
			GET_STRING_FROM_XML("weHave1Spear", m_stringWeHave1Spear);
			GET_STRING_FROM_XML("weHaveNSpears", m_stringWeHaveNSpears);
			GET_STRING_FROM_XML("newSpearsEmerging", m_stringNewSpearsEmerging);
			GET_STRING_FROM_XML("friendlyCharged20", m_stringFriendlyCharged20);
			GET_STRING_FROM_XML("friendlyCharged40", m_stringFriendlyCharged40);
			GET_STRING_FROM_XML("friendlyCharged60", m_stringFriendlyCharged60);
			GET_STRING_FROM_XML("friendlyCharged80", m_stringFriendlyCharged80);
			GET_STRING_FROM_XML("friendlyCharged90", m_stringFriendlyCharged90);
			GET_STRING_FROM_XML("enemyCharged20", m_stringEnemyCharged20);
			GET_STRING_FROM_XML("enemyCharged40", m_stringEnemyCharged40);
			GET_STRING_FROM_XML("enemyCharged60", m_stringEnemyCharged60);
			GET_STRING_FROM_XML("enemyCharged80", m_stringEnemyCharged80);
			GET_STRING_FROM_XML("enemyCharged90", m_stringEnemyCharged90);
			GET_STRING_FROM_XML("captureTheSpears", m_stringCaptureTheSpears);
			GET_STRING_FROM_XML("captured", m_stringCaptured);
			GET_STRING_FROM_XML("capturing", m_stringCapturing);

#undef GET_STRING_FROM_XML
		}
		else if(!strcmp(pTag,"Audio"))
		{
			const char *pString = NULL;
			CGameAudio *pGameAudio = g_pGame->GetGameAudio();

#define GET_SOUND_FROM_XML(name, result) \
			if (xmlChild->getAttr(name, &pString))	\
			{	\
				result = pGameAudio->GetSignalID(pString);	\
			} \
			else \
			{ \
				DesignerWarning(0, string().Format("failed to find expected signalID %s within Powerstruggle.xml", name)); \
			} \

			GET_SOUND_FROM_XML("capturedLoop", m_captureSignalId);
			GET_SOUND_FROM_XML("spearGoingUp", m_spear_going_up);
			GET_SOUND_FROM_XML("positiveSignal", m_positiveSignalId);
			GET_SOUND_FROM_XML("negativeSignal", m_negativeSignalId);

#undef GET_SOUND_FROM_XML
		}
	}

	pGameRules->RegisterKillListener(this);
	pGameRules->RegisterClientConnectionListener(this);
}

//------------------------------------------------------------------------
float CGameRulesObjective_PowerStruggle::CalculateCapturePointQuantity(const SNodeInfo *pNodeInfo, float frameTime)
{
	///////////////////////////
	//	Calc num attackers	 //
	///////////////////////////
	assert(pNodeInfo);
	
	// How many more players does the more numerous team have at the point than the competitors
	const int team1Count = pNodeInfo->m_insideCount[0];
	const int team2Count = pNodeInfo->m_insideCount[1];
	int		deltaNumPlayersCapturing = abs(team1Count - team2Count); 

	///////////////////////////////////////
	//	Calc bonus attacker multiplier	 //
	///////////////////////////////////////

	float totalCaptureTimeMultiplier = 1.0f;
	float bonusCaptureTimeMultiplier = 1.0f;
	if(deltaNumPlayersCapturing > 1)
	{
		// Calculate our bonus time reduction multiplier for first additional attacker
		// E.G, if data specifies we should reduce total time to capture by 25% for each additional attacker
		bonusCaptureTimeMultiplier = clamp_tpl(m_captureTimePercentageReduction,0.0f,100.0f);

		// Take inverse, e.g. to reduce by 25% we want to multiply base capture time by 75%
		bonusCaptureTimeMultiplier = 100.0f - bonusCaptureTimeMultiplier; 

		// Convert into 0->1 range (e.g. 0.75f) to get our multiplier
		bonusCaptureTimeMultiplier *= 0.01f; //[0.0f,1.0f]
	}
	
	// For each additional player above the first, the total time to capture is decreased (diminishing returns)
	for( int i = 1; i < deltaNumPlayersCapturing; ++i)
	{
		// e.g. 1.0  * total capture time for 1 player,
		//      0.75 * total capture time for 2 players, (1.0f * 0.75f)
		//			0.5625 * total capture time for 3 players etc (1.0f * 0.75f * 0.75f) 
		totalCaptureTimeMultiplier *= bonusCaptureTimeMultiplier; 
	}

	//////////////////////////////////////////////////////
	//	Calc point rate per sec to hit desired cap time //
	//////////////////////////////////////////////////////
	
	static const float kMaxCapturePointCapacity			= 1.0f; // We currently work with an implied capture capacity of 1.0f. 

	// e.g. 10 seconds to capture with no additional attackers above first
	const float standardTotalTimeToCapture  = kMaxCapturePointCapacity * m_invCaptureIncrementMultiplier;

	// e.g with 2 additional attackers , 10 seconds * 0.5625f = we want 5.625 seconds total to capture
	float desiredTotalTimeToCapture				  = standardTotalTimeToCapture * totalCaptureTimeMultiplier;

	// e.g.  1.0f max capacity / 5.625secs  =	requires 0.177 capture POINTS per sec
	float pointIncrementPerSecRequired			= kMaxCapturePointCapacity / desiredTotalTimeToCapture;

	// return total (time scaled)
	return pointIncrementPerSecRequired * frameTime; 
}


CGameRulesObjective_PowerStruggle::SNodeInfo *CGameRulesObjective_PowerStruggle::Common_FindNodeFromEntityId(EntityId entityId)
{
	SNodeInfo *foundNode=NULL;

	for (int i = 0; i < MAX_NODES; ++ i)
	{
		SNodeInfo *pNodeInfo = &m_nodes[i];

		if (pNodeInfo->m_id == entityId)
		{
			foundNode = pNodeInfo;
			break;
		}
	}

	return foundNode;
}

float CGameRulesObjective_PowerStruggle::SmoothlyUpdateValue(float currentVal, float desiredVal, float updateRate, float frameTime)
{
	float fDiff = desiredVal - currentVal;
 	float fSignedUpdateRate			= fsgnf(fDiff) * updateRate;

	//We only want to clamp to the limit in the direction we are blending
	float fMax = (float)__fsel( fDiff, desiredVal, FLT_MAX);
	float fMin = (float)__fsel(-fDiff, desiredVal, FLT_MIN);
 
	return clamp_tpl<float>( currentVal + (fSignedUpdateRate * frameTime), fMin, fMax );
}

void CGameRulesObjective_PowerStruggle::Client_UpdateSoundSignalForNode(SNodeInfo *pNodeInfo, float frameTime)
{
	CRY_ASSERT(gEnv->IsClient());

	if (!pNodeInfo->m_signalPlayer.IsPlaying())
	{
		CryWatch("Spear=%s is not playing audio signal!", m_pGameRules->GetEntityName(pNodeInfo->m_id));

		CRY_ASSERT_MESSAGE(!pNodeInfo->m_havePlayedSignal, "we should only ever not be playing our signals if we've missed playing them by being a late joiner. Any other cases indicate something has gone wrong with the low-level sound code");

		// This code has been re-enabled for late joiners to play signals if they're not already (they will miss the emerging of the spears that triggers the sounds)
		// low level sound code has been fixed and this is no longer required for normal
		if (!pNodeInfo->m_havePlayedSignal)
		{
			CryLog("CGameRulesObjective_PowerStruggle::Client_UpdateSoundSignalForNode() spear %s has never played it's audio signal.. playing it now!", m_pGameRules->GetEntityName(pNodeInfo->m_id));

			pNodeInfo->m_signalPlayer.SetSignal(m_captureSignalId); 

			//pNodeInfo->m_signalPlayer.Play(pNodeInfo->m_id);
			pNodeInfo->m_havePlayedSignal=true;
		}
	}

	bool capturing=false;
	bool contended=(pNodeInfo->m_controllingTeamId == CONTESTED_TEAM_ID);
	float completion=0.f;
	bool completed=false;

	switch(pNodeInfo->m_state)
	{
		case ENS_captured:
			completed=true;
			// no break intentionally
		case ENS_capturing_from_neutral:
		case ENS_capturing_from_capture:
			capturing=true;
			completion = pNodeInfo->m_captureStatus;
			break;
	}

	// TODO - do we have to be more careful with not changing params on signals if we don't need to
	// or does fmod cache the var changes itself, otherwise as with flash, probably want to cache locally
	// and have it only change if its changed

	static const float capturingRate = 1.0f;
	static const float contendedRate = 1.0f;
	static const float completedRate = 1.0f;

	float desiredCapturing = (capturing ? 1.f : 0.f);
	float desiredContended = (contended ? 1.f : 0.f);
	float desiredCompleted = (completed ? 1.f : 0.f);
	float currentCapturing = pNodeInfo->m_currentCapturingAudioParam;
	float currentContended = pNodeInfo->m_currentContendedAudioParam;
	float currentCompleted = pNodeInfo->m_currentCompletedAudioParam;

	currentCapturing = SmoothlyUpdateValue(currentCapturing, desiredCapturing, capturingRate, frameTime);
	currentContended = SmoothlyUpdateValue(currentContended, desiredContended, contendedRate, frameTime);
	currentCompleted = SmoothlyUpdateValue(currentCompleted, desiredCompleted, completedRate, frameTime);

	pNodeInfo->m_signalPlayer.SetParam(pNodeInfo->m_id, "capturing", currentCapturing);
	pNodeInfo->m_signalPlayer.SetParam(pNodeInfo->m_id, "contended", currentContended); 
	pNodeInfo->m_signalPlayer.SetParam(pNodeInfo->m_id, "completed", currentCompleted);
	pNodeInfo->m_signalPlayer.SetParam(pNodeInfo->m_id, "completion", completion); 

	//DbgLog("CGameRulesObjective_PowerStruggle::Client_UpdateSoundSignalForNode() node=%s; capturing=%f; contended=%f; completed=%f; completion=%f", m_pGameRules->GetEntityName(pNodeInfo->m_id), currentCapturing, currentContended, currentCompleted, completion);

	pNodeInfo->m_currentCapturingAudioParam = currentCapturing;
	pNodeInfo->m_currentContendedAudioParam = currentContended;
	pNodeInfo->m_currentCompletedAudioParam = currentCompleted;
}

void CGameRulesObjective_PowerStruggle::Common_HandleSpearCaptureStarting(SNodeInfo *pNodeInfo)
{
#if 0
	// we've lost the sounds for this :-(

	EntityId localActorId = g_pGame->GetIGameFramework()->GetClientActorId();
	int localTeamId = m_pGameRules->GetTeam(localActorId);
	DbgLog("CGameRulesObjective_Powerstruggle::Common_HandleSpearCaptureStarting() for node=%s", m_pGameRules->GetEntityName(pNodeInfo->m_id));

	if (pNodeInfo->m_lastCapturedTeamId == localTeamId)
	{
		DbgLog("CGameRulesObjective_Powerstruggle::Common_HandleSpearCaptureStarting() our team used to own this spear, we need to announce this!");

		// it's one of our spears being newly captured
		switch (pNodeInfo->m_activeIdentityId)
		{
			case eAII_spearA_id:
				CAnnouncer::GetInstance()->Announce("SpearEnemyCapturingA", CAnnouncer::eAC_inGame);
				break;
			case eAII_spearB_id:
				CAnnouncer::GetInstance()->Announce("SpearEnemyCapturingB", CAnnouncer::eAC_inGame);
				break;
			case eAII_spearC_id:
				CAnnouncer::GetInstance()->Announce("SpearEnemyCapturingC", CAnnouncer::eAC_inGame);
				break;
			default:
				CRY_ASSERT_MESSAGE(0, string().Format("unhandled activeIdentityId=%d", pNodeInfo->m_activeIdentityId));
				break;
		}
	}
#endif
}

void CGameRulesObjective_PowerStruggle::Common_HandleCapturedSpear(SNodeInfo *pNodeInfo)
{
	if( pNodeInfo->m_soloCaptureState == ESolo_Activated )
	{
		//only add solo capturer to list if it isn't already there
		CryFixedArray<EntityId, MAX_PLAYER_LIMIT>::iterator entIt = std::find( m_soloCapturingPlayers.begin(), m_soloCapturingPlayers.end(), pNodeInfo->m_soloCapturerId );
		if( entIt == m_soloCapturingPlayers.end() )
		{
			m_soloCapturingPlayers.push_back( pNodeInfo->m_soloCapturerId );
		}
	}

	pNodeInfo->SetState(ENS_captured); 

	// optimistic basic scoring.. sadly its more complex than this 
	if (gEnv->bServer)
	{
		if (IGameRulesScoringModule* pScoringModule = m_pGameRules->GetScoringModule())
		{
			// reward the players who helped capture the node
			TPlayerIdArray &insideEntities = pNodeInfo->m_insideEntities[pNodeInfo->m_controllingTeamId-1];
			int numInside=pNodeInfo->m_insideCount[pNodeInfo->m_controllingTeamId-1];

			for (int i=0; i<numInside; i++)
			{
				pScoringModule->OnPlayerScoringEvent(insideEntities[i], EGRST_PowerStruggle_CaptureSpear);
			}
		}
	}

	pNodeInfo->m_lastCapturedTeamId = pNodeInfo->m_controllingTeamId;

	DbgLog("CGameRulesObjective_PowerStruggle::Common_HandleCapturedSpear() calling NodeCaptureStatusChanged()");
	NodeCaptureStatusChanged(pNodeInfo);

	LogSpearStateToTelemetry( eSSE_captured, pNodeInfo );
}

bool IsPointInsideCone(const Vec3 &inPoint, const Vec3 &inConePos, const float inConeRadius, const float inConeHeight)
{
	Vec3 p = inPoint - inConePos;

	// is p.z > 0 and p.z < inConeHeight
	if (p.z * (inConeHeight-p.z) < 0.f)
		return false;
	
	float distSqr = p.GetLengthSquared2D(); // dist from cone axis

	// The radius of the cone at p.z is
	float rz = (inConeHeight-p.z) * (inConeRadius / inConeHeight);
	float rzSqr = rz * rz;

	if (distSqr <= rzSqr)
	{
		return true;
	}

	return false;
}

/*static*/ bool CGameRulesObjective_PowerStruggle::CanActorCaptureNodes(IActor * pActor)
{
	return (pActor && pActor->IsPlayer() && !pActor->IsDead() && (pActor->GetSpectatorMode() == CPlayer::eASM_None));
}

void CGameRulesObjective_PowerStruggle::Common_UpdateActiveNode(SNodeInfo *pNodeInfo, float frameTime, bool &bAspectChanged)
{
	bool bClientActorInside = false;
	bool bCountsChanged = false;

	// Evaluate inside count
	for (int teamIndex = 0; teamIndex < NUM_TEAMS; ++ teamIndex)
	{
		TPlayerIdArray &insideEntities = pNodeInfo->m_insideEntities[teamIndex];
		insideEntities.clear();

		TPlayerIdArray &boxEntities = pNodeInfo->m_insideBoxEntities[teamIndex];

		Vec3 conePos = pNodeInfo->m_position;
		conePos.z += pNodeInfo->m_controlZOffset;

		for (int playerIndex = 0, numInsideBox = boxEntities.size(); playerIndex < numInsideBox; ++ playerIndex)
		{
			EntityId playerId = boxEntities[playerIndex];
			IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(playerId);
			if ( CGameRulesObjective_PowerStruggle::CanActorCaptureNodes(pActor) )
			{
				Vec3 playerPos = pActor->GetEntity()->GetPos();

				if (IsPointInsideCone(playerPos, conePos, pNodeInfo->m_controlRadius, pNodeInfo->m_controlHeight))
				{
					insideEntities.push_back(playerId);
					if (pActor->IsClient())
					{
						bClientActorInside = true;

						CRY_ASSERT(!m_haveDisplayedProgressBar); // only one node should cause the progress bar to be displayed

						// If We are inside.. we need to update and render our progress bar
						const bool bClientInvoledInCapture = Client_UpdateCaptureProgressBar(pNodeInfo);
						if(bClientInvoledInCapture && !pNodeInfo->m_clientPlayerPresentAtCaptureTime)
						{
							pNodeInfo->m_clientPlayerPresentAtCaptureTime = true;
							Client_CheckCapturedAllPoints();
						}

						m_haveDisplayedProgressBar=true;
					}
				}
			}
			else
			{
				CRY_ASSERT_MESSAGE(!pActor, "now that the kill listener is correctly setup this shouldn't be possible, aside from perhaps some untracked disconnect resulting in a null actor");
				boxEntities.removeAt(playerIndex);
				-- playerIndex;
				-- numInsideBox;
			}
		}
		if (gEnv->bServer && (pNodeInfo->m_insideCount[teamIndex] != insideEntities.size()))
		{
			pNodeInfo->m_insideCount[teamIndex] = insideEntities.size();
			bCountsChanged = true;
		}
	}
	
	if (gEnv->bServer && bCountsChanged)
	{
		int previousOwnerId = pNodeInfo->m_controllingTeamId;
		int team1Count = pNodeInfo->m_insideCount[0];
		int team2Count = pNodeInfo->m_insideCount[1];
		int newControllingTeamId;

		if (team1Count && team2Count)
		{
			newControllingTeamId = CONTESTED_TEAM_ID;
		}
		else if (team1Count && !team2Count)
		{
			newControllingTeamId = 1;
		}
		else if (!team1Count && team2Count)
		{
			newControllingTeamId = 2;
		}
		else
		{
			newControllingTeamId = 0;
		}

		// we have a new team controlling
		if (newControllingTeamId > 0)
		{
			if (pNodeInfo->m_state == ENS_neutral || pNodeInfo->m_state == ENS_emerging)
			{
				pNodeInfo->SetState(ENS_capturing_from_neutral);
				pNodeInfo->m_captureStatusOwningTeam = newControllingTeamId;	// set the new capture status owning team

				LogSpearStateToTelemetry( eSSE_capturing_from_neutral, pNodeInfo );
			}
			else if (pNodeInfo->m_state == ENS_captured)
			{
				if (newControllingTeamId != pNodeInfo->m_lastCapturedTeamId)
				{
					// we have a different team controlling than has captured this captured node
					pNodeInfo->SetState(ENS_capturing_from_capture);
					pNodeInfo->m_captureStatusOwningTeam = newControllingTeamId; // set the new capture status owning team
					pNodeInfo->m_captureStatus = 0.f;		// this wont be used again till another team tries to retake this captured node, so have it ready for use to grow up from 0.f

					Common_HandleSpearCaptureStarting(pNodeInfo);	

					LogSpearStateToTelemetry( eSSE_capturing_from_capture, pNodeInfo );
				}
			}
		}
		else if( newControllingTeamId == CONTESTED_TEAM_ID && pNodeInfo->m_controllingTeamId != CONTESTED_TEAM_ID )
		{
			LogSpearStateToTelemetry( eSSE_contested, pNodeInfo );
			Common_NewlyContestedSpear(pNodeInfo);
		}


		pNodeInfo->m_controllingTeamId = newControllingTeamId;
		bAspectChanged = true;

		//if status is capturing

		if( pNodeInfo->m_state == ENS_capturing_from_capture || pNodeInfo->m_state == ENS_capturing_from_neutral )
		{
			if( pNodeInfo->m_controllingTeamId == 1 || pNodeInfo->m_controllingTeamId == 2 )
			{
				// someone is capturing (not unclaimed or contested)
				TPlayerIdArray &capturingEntities = pNodeInfo->m_insideEntities[ pNodeInfo->m_controllingTeamId-1 ];

				if( capturingEntities.size() == 1 )
				{
					if( pNodeInfo->m_soloCaptureState == ESolo_Available )
					{
						//solo capturing, this guy is eligible for solo capture award

						pNodeInfo->m_soloCaptureState = ESolo_Activated;
						pNodeInfo->m_soloCapturerId = capturingEntities[0];
					}
					else
					{
						int oldTeam = (pNodeInfo->m_soloCapturerId!=-1)? m_pGameRules->GetTeam( pNodeInfo->m_soloCapturerId ) : 0;
						if( oldTeam == pNodeInfo->m_controllingTeamId )
						{
							if( capturingEntities[0] != pNodeInfo->m_soloCapturerId )
							{
								//if new capturer is not old capturer, but same team as old, denied now.
								pNodeInfo->m_soloCaptureState = ESolo_Denied;
								pNodeInfo->m_soloCapturerId = -1;
							}
						}
						else
						{
							if( pNodeInfo->m_state == ENS_capturing_from_neutral )
							{
								//special, if different and point neutral, then swap guy and re-activate award
								pNodeInfo->m_soloCaptureState = ESolo_Activated;
								pNodeInfo->m_soloCapturerId = capturingEntities[0];
							}
						}
					}
				}
				else
				{
					//more than one capturer, solo capture award denied
					pNodeInfo->m_soloCaptureState = ESolo_Denied;
					pNodeInfo->m_soloCapturerId = -1;
				}
			}
		}
	}

	// Update capture status
	float captureStatus = pNodeInfo->m_captureStatus;

	// no captureStatus change when in contention
	if (pNodeInfo->m_controllingTeamId != CONTESTED_TEAM_ID)
	{
		switch(pNodeInfo->m_state)
		{
			case ENS_emerging:
			case ENS_neutral:
			case ENS_captured:
				// no capturing
				break;
			case ENS_capturing_from_neutral:
			case ENS_capturing_from_capture:
				// bAspectChanged=true;	// all the time, captureStatus is ALWAYS changing - wasteful but here as a fallback

				if (pNodeInfo->m_controllingTeamId && pNodeInfo->m_controllingTeamId != pNodeInfo->m_captureStatusOwningTeam && pNodeInfo->m_controllingTeamId != pNodeInfo->m_lastCapturedTeamId)
				{
					// only reset the captureStatus if we've changed to a new controlling team and the new controlling team is NOT the team that has currently captured the site
					captureStatus = 0.f;
					pNodeInfo->m_captureStatusOwningTeam = pNodeInfo->m_controllingTeamId;	// we have a new owner of the captureStatus
					bAspectChanged=true;	
					DbgLog("CGameRulesObjective_PowerStruggle::Common_UpdateActiveNode() a giant spear is having one team's (%d) capture reset to the a team %d", pNodeInfo->m_captureStatusOwningTeam, pNodeInfo->m_controllingTeamId);
				}
				else
				{
					if (pNodeInfo->m_controllingTeamId && pNodeInfo->m_controllingTeamId != pNodeInfo->m_lastCapturedTeamId)
					{
						// if a team is capturing the node who doesn't already own the node
						float amount=CalculateCapturePointQuantity(pNodeInfo, frameTime);
						captureStatus+=amount;
						if (captureStatus >= 1.0f)
						{
							Common_HandleCapturedSpear(pNodeInfo);
						}
					}
					else
					{
						// decaying - due to no team capping or the current capture team is the current owner
						captureStatus -= (m_captureDecayMultiplier * frameTime);

						if (captureStatus <= 0.f)
						{
							captureStatus = 0.f;
							pNodeInfo->m_lastControllingTeamId = 0;	// fully decayed no longer need to keep the last controller around for any HUD elements

							if (pNodeInfo->m_state == ENS_capturing_from_neutral)
							{
								pNodeInfo->SetState(ENS_neutral);
								pNodeInfo->m_captureStatusOwningTeam = 0;	// no one owns capture status
							}
							else
							{
								CRY_ASSERT(pNodeInfo->m_state == ENS_capturing_from_capture);
								pNodeInfo->SetState(ENS_captured);
								pNodeInfo->m_captureStatusOwningTeam = pNodeInfo->m_lastCapturedTeamId;	// restore captureStatusOwningTeam to lastCapturedTeam now we're back in captured state
							}

							LogSpearStateToTelemetry( eSSE_failed_capture, pNodeInfo );
						}
					}
				}
				break;
		}	

		if (pNodeInfo->m_controllingTeamId > 0)
		{
			pNodeInfo->m_lastControllingTeamId = pNodeInfo->m_controllingTeamId;	// contention or no controlling team doesn't overwrite lastControllingTeam here
		}
	}

	pNodeInfo->m_captureStatus = captureStatus;
}

void CGameRulesObjective_PowerStruggle::Server_ActivateNode(int index)
{
	CRY_ASSERT(gEnv->bServer);
	SNodeInfo *pNodeInfo = &m_nodes[index];

	DbgLog("CGameRulesObjective_PowerStruggle::Server_ActivateNode() entity=%s", m_pGameRules->GetEntityName(pNodeInfo->m_id));
	
	CRY_ASSERT(pNodeInfo->m_state == ENS_hidden);

	pNodeInfo->SetState(ENS_emerging);

	EntityId nodeId = pNodeInfo->m_id;
	IEntity* pNodeEntity = gEnv->pEntitySystem->GetEntity(nodeId);
	CRY_ASSERT(pNodeEntity);

	pNodeEntity->Invisible(false);

	IScriptTable *pScript = pNodeEntity->GetScriptTable();
	if (pScript)
	{
		if (pScript->GetValueType("ActivateNode") == svtFunction)
		{
			IScriptSystem *pScriptSystem = gEnv->pScriptSystem;
			pScriptSystem->BeginCall(pScript, "ActivateNode");
			pScriptSystem->PushFuncParam(pScript);
			pScriptSystem->EndCall();
		}
	}

	if (IGameRulesSpawningModule *pSpawningModule = m_pGameRules->GetSpawningModule())
	{
		pSpawningModule->EnablePOI(nodeId);	// clients do this in the netserialize() going into state emerging
	}

	pNodeInfo->m_signalPlayer.SetSignal(m_captureSignalId); 
	DbgLog("CGameRulesObjective_PowerStruggle::Server_ActivateNode() setting normal spear loop signal for spear=%s", m_pGameRules->GetEntityName(pNodeInfo->m_id));
	CAudioSignalPlayer::JustPlay(m_spear_going_up, pNodeInfo->m_position);

	if (m_setupIsSpecifyingNodeLetters)
	{
		CRY_ASSERT_MESSAGE(pNodeInfo->m_activeIdentityId != eAII_unknown_id, "we expect all nodes to have assigned active identity id when its been specified within the setup");
	}
	else
	{
		CRY_ASSERT_MESSAGE(pNodeInfo->m_activeIdentityId == eAII_unknown_id, "we expect inactive nodes to have no assigned active identity id");
		pNodeInfo->m_activeIdentityId = CGameRulesObjective_PowerStruggle::s_nodeIdentityInfo[index+1].eIdentity;
	}


	DbgLog("CGameRulesObjective_PowerStruggle::Server_ActivateNode() playing signal for spear=%s", m_pGameRules->GetEntityName(pNodeInfo->m_id));
	//pNodeInfo->m_signalPlayer.Play(pNodeInfo->m_id);
	pNodeInfo->m_havePlayedSignal=true;

	if (gEnv->IsClient())
	{
		Client_ShakeCameraForSpear(pNodeInfo);
	}
}

void CGameRulesObjective_PowerStruggle::Server_InitNode(SNodeInfo *pNodeInfo)
{
	CRY_ASSERT(gEnv->bServer);

	EntityId nodeId = pNodeInfo->m_id;
	IEntity* pNodeEntity = gEnv->pEntitySystem->GetEntity(nodeId);

	DbgLog("CGameRulesObjective_PowerStruggle::Server_DeActivateNode() entity=%s", m_pGameRules->GetEntityName(pNodeInfo->m_id));

	pNodeInfo->SetState(ENS_hidden);

	IScriptTable *pScript = pNodeEntity->GetScriptTable();
	if (pScript)
	{
		if (pScript->GetValueType("DeActivateNode") == svtFunction)
		{
			IScriptSystem *pScriptSystem = gEnv->pScriptSystem;
			pScriptSystem->BeginCall(pScript, "DeActivateNode");
			pScriptSystem->PushFuncParam(pScript);
			pScriptSystem->EndCall();
		}
	}

	if (IGameRulesSpawningModule *pSpawningModule = m_pGameRules->GetSpawningModule())
	{
		pSpawningModule->DisablePOI(nodeId);	// clients do this in the netserialize() going into state emerging
	}

	//pNodeInfo->m_signalPlayer.Stop(pNodeInfo->m_id);
	pNodeEntity->Invisible(true);
}

void CGameRulesObjective_PowerStruggle::Server_ActivateAllNodes()
{
	for (int i = 0; i < MAX_NODES; ++ i)
	{
		Server_ActivateNode(i);
	}
}

void CGameRulesObjective_PowerStruggle::Common_UpdateNode( SNodeInfo *pNodeInfo, float frameTime, bool &bAspectChanged)
{
	if (pNodeInfo->m_state >= ENS_emerging && pNodeInfo->m_state <= ENS_capturing_from_capture)
	{
		Common_UpdateActiveNode(pNodeInfo, frameTime, bAspectChanged);
	}

	if (pNodeInfo->m_state >= ENS_emerging)	// no audio updating for hidden (silent) spears
	{
		if (gEnv->IsClient())
		{
			Client_UpdateSoundSignalForNode(pNodeInfo, frameTime);
		}
	}
}

void CGameRulesObjective_PowerStruggle::Server_UpdateScoring(float frameTime, bool &bAspectChanged)
{
	m_timeTillScore -= frameTime;
	if (m_timeTillScore < 0.f)
	{
		if (gEnv->bServer)
		{
			for(int i = 0; i < NUM_TEAMS; ++i)
			{
				SChargeeInfo& chargeeInfo = m_Chargees[i];				
				chargeeInfo.m_PrevChargeValue = chargeeInfo.m_ChargeValue;
			}

			for (int i = 0; i < MAX_NODES; ++ i)
			{
				SNodeInfo *pNodeInfo = &m_nodes[i];
				if (pNodeInfo->m_lastCapturedTeamId)
				{
					// team 1 to 2 -> charge index 0 to 1
					SChargeeInfo* pChargeeInfo = &m_Chargees[pNodeInfo->m_lastCapturedTeamId-1];

					if(pChargeeInfo->m_PrevChargeValue < 1.f)
					{
						pChargeeInfo->m_ChargeValue = min(pChargeeInfo->m_ChargeValue + m_chargeSpeed, 1.f);
						bAspectChanged = true;
					}
				}
			}

			IGameRulesScoringModule *pScoringModule = m_pGameRules->GetScoringModule();

			for(int i = 0; i < NUM_TEAMS; ++i)
			{
				SChargeeInfo& chargeeInfo = m_Chargees[i];
				if(chargeeInfo.m_PrevChargeValue != chargeeInfo.m_ChargeValue)
				{
					if( (int)(chargeeInfo.m_PrevChargeValue*100.f) != (int)(chargeeInfo.m_ChargeValue*100.f) ) //Crossed a 0.01f threshold then give score
					{
						if(pScoringModule)
						{
							pScoringModule->OnTeamScoringEvent(chargeeInfo.m_TeamId, EGRST_CaptureObjectiveHeld);
						}
					}
				}
			}
	
			SChargeeInfo *pWinningChargee = NULL;
			if (m_Chargees[0].m_ChargeValue > m_Chargees[1].m_ChargeValue)
			{
				pWinningChargee = &m_Chargees[0];
			}
			else
			{
				pWinningChargee = &m_Chargees[1];
			}

			// only do announcements for the currently winning team
			Common_CheckNotifyStringForChargee(pWinningChargee, pWinningChargee->m_PrevChargeValue, pWinningChargee->m_ChargeValue);

		}

		m_timeTillScore += m_scoreTimerLength;
	}
}

void CGameRulesObjective_PowerStruggle::Common_UpdateDebug(float frameTime)
{
#if CRY_WATCH_ENABLED

	if (g_pGameCVars->g_PS_debug == 0)
		return;

	for (int i = 0; i < MAX_NODES; ++ i)
	{
		SNodeInfo *pNodeInfo = &m_nodes[i];
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(pNodeInfo->m_id);
		CryWatch("Node: entity=%s; Invisible: %d; Hidden: %d", pEntity ? pEntity->GetName() : "NULL", pEntity ? pEntity->IsInvisible() : 0, pEntity ? pEntity->IsHidden() : 0);
	}

	int clientTeamId = m_pGameRules->GetTeam(g_pGame->GetIGameFramework()->GetClientActorId());

	for (int i = 0; i < MAX_NODES; ++ i)
	{
		SNodeInfo *pNodeInfo = &m_nodes[i];
		CryWatch("Node: entity=%s; state=%d; teamOwner=%d; lastTeamOwner=%d; lastCapTeam=%d; capOwner=%d; ", m_pGameRules->GetEntityName(pNodeInfo->m_id), pNodeInfo->m_state, pNodeInfo->m_controllingTeamId, pNodeInfo->m_lastControllingTeamId, pNodeInfo->m_lastCapturedTeamId, pNodeInfo->m_captureStatusOwningTeam);
		CryWatch("      capture=%.2f; activeId=%d; insideCount=%d,%d; audio: cap=%.1f; con=%.1f; com=%.1f", pNodeInfo->m_captureStatus, pNodeInfo->m_activeIdentityId, pNodeInfo->m_insideCount[0], pNodeInfo->m_insideCount[1], pNodeInfo->m_currentCapturingAudioParam, pNodeInfo->m_currentContendedAudioParam, pNodeInfo->m_currentCompletedAudioParam);
	}
		
	// Friendly status first
	for(int i = 0; i < NUM_TEAMS; ++i)
	{	
		SChargeeInfo& chargeeInfo = m_Chargees[i];
		if(chargeeInfo.m_TeamId == clientTeamId)
		{	
			int chargePercent = static_cast<int>(chargeeInfo.m_ChargeValue * 100.0f); 
			chargePercent = min(chargePercent, 100); 
			CryWatch("Friendly Charge %d %%", chargePercent);	
		}
	}

	// Enemy status second
	for(int i = 0; i < NUM_TEAMS; ++i)
	{
		SChargeeInfo& chargeeInfo = m_Chargees[i];
		if(chargeeInfo.m_TeamId != clientTeamId)
		{	
			int chargePercent = static_cast<int>(chargeeInfo.m_ChargeValue * 100.0f); 
			chargePercent = min(chargePercent, 100); 
			CryWatch("Enemy   Charge %d %%", chargePercent);	
		}
	}

#endif
}

//------------------------------------------------------------------------
void CGameRulesObjective_PowerStruggle::Update( float frameTime )
{
	// for PC pre-game handling do nothing till we're actually in game 
	if (!m_pGameRules->HasGameActuallyStarted() /*&& !m_pGameRules->IsPrematchCountDown()*/)
	{
		return;
	}

	if (IGameRulesStateModule* pStateModule = m_pGameRules->GetStateModule())
	{
		if (pStateModule->GetGameState() == IGameRulesStateModule::EGRS_InGame 
			/*|| (pStateModule->GetGameState() == IGameRulesStateModule::EGRS_PreGame && pStateModule->IsInCountdown())*/)
		{
			if (m_manageActiveSpearsTimer == k_manage_active_spears_initial_time)
			{
				CAnnouncer::GetInstance()->Announce("SpearInitial", CAnnouncer::eAC_inGame);
				SHUDEventWrapper::OnBigMessage("", m_stringCaptureTheSpears);
			}

			if (m_manageActiveSpearsTimer > 0.0f)
			{
				m_manageActiveSpearsTimer -= frameTime;

				if(gEnv->bServer)
				{
					if (m_manageActiveSpearsTimer <= 0.f)
					{
						// first time managing nodes - start with all spears active to try and reduce any imbalance
						Server_ActivateAllNodes();

						// Server only!!
						CAnnouncer::GetInstance()->Announce("SpearsEmerging", CAnnouncer::eAC_inGame);
						SHUDEventWrapper::GameStateNotify(m_stringNewSpearsEmerging.c_str(), SHUDEventWrapper::SMsgAudio(m_positiveSignalId));
					}
				}
			}
		}
	}
	
	m_haveDisplayedProgressBar=false;

	bool bAspectChanged = false;

	for (int i = 0; i < MAX_NODES; ++ i)
	{
		SNodeInfo *pNodeInfo = &m_nodes[i];
		Common_UpdateNode(pNodeInfo, frameTime, bAspectChanged);
	}

	if (!m_haveDisplayedProgressBar)
	{
		// hide the capture bar
		SHUDEventWrapper::PowerStruggleManageCaptureBar(eHPSCBT_None, 0.f, false, "");
	}

	if (gEnv->bServer)
	{
		Server_UpdateScoring(frameTime, bAspectChanged);
	}

	Common_UpdateDebug(frameTime);


	// ICONS
	if(gEnv->IsClient())
	{
		// Update icons
		Client_SetAllIcons(frameTime);
	}
	
	if (bAspectChanged)
	{
		CHANGED_NETWORK_STATE(m_pGameRules, POWER_STRUGGLE_ASPECT);
	}
}

//------------------------------------------------------------------------
void CGameRulesObjective_PowerStruggle::OnStartGame()
{
	CryLog("GameRulesObjective_PowerStruggle::OnStartGame()");

	if (g_pGame->GetHostMigrationState() != CGame::eHMS_NotMigrating)
	{
		CryLog("GameRulesObjective_PowerStruggle::OnStartGame() we're host migrating. Early returning. We dont want to do another initialisation, all our entities are still valid");
		return;
	}

	IEntityClassRegistry* pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
	const IEntityClass *pNodeClass = pClassRegistry->FindClass("PowerStruggleNode");
	FATAL_ASSERT(pNodeClass);

	IEntityItPtr pIt = gEnv->pEntitySystem->GetEntityIterator();
	pIt->MoveFirst();

	int idx = 0;
	while (IEntity *pEntity = pIt->Next())
	{
		IEntityClass* pEntityClass = pEntity->GetClass();
		if (pEntityClass == pNodeClass)
		{
			Common_AddNode(pEntity, idx);
			idx++;
		}
	}

	Common_SortNodes();

	// Need to manually setup team IDs on chargees and cores to prevent bugs. 
	for(int i = 0; i < NUM_TEAMS; ++i)
	{
		m_Chargees[i].m_TeamId = i+1;	// only hardcode teams if they've not already been set
	}

	SHUDEventWrapper::OnGameStatusUpdate(eGBNFLP_Neutral, m_stringCaptureTheSpears);
}

//------------------------------------------------------------------------
void CGameRulesObjective_PowerStruggle::PostInitClient( int channelId )
{
	CRY_ASSERT(gEnv->bServer);

	// Make sure the capture status is synched correctly
	CHANGED_NETWORK_STATE(m_pGameRules, POWER_STRUGGLE_ASPECT);
}

bool CGameRulesObjective_PowerStruggle::NetSerializeNode(SNodeInfo *pNodeInfo, TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	bool capStatusChanged=false;

	int oldControllingTeam = pNodeInfo->m_controllingTeamId;

	int controllingTeam = pNodeInfo->m_controllingTeamId + 1;
	ser.Value("controllingTeamId", controllingTeam, 'ui2');

	int newControllingTeam = controllingTeam -1;
	pNodeInfo->m_controllingTeamId = newControllingTeam;

	if (ser.IsReading())
	{
		if( newControllingTeam == CONTESTED_TEAM_ID && oldControllingTeam != CONTESTED_TEAM_ID )
		{
			Common_NewlyContestedSpear(pNodeInfo);
		}
	}

	int lastControllingTeam = pNodeInfo->m_lastControllingTeamId + 1;
	ser.Value("lastControllingTeamId", lastControllingTeam, 'ui2');
	pNodeInfo->m_lastControllingTeamId = lastControllingTeam - 1;

	ser.Value("lastCapturedTeamId", pNodeInfo->m_lastCapturedTeamId, 'ui2'); // has no negative value ever set
	ser.Value("captureStatusOwningTeamId", pNodeInfo->m_captureStatusOwningTeam, 'ui2'); // has no negative value ever set
	ser.EnumValue("activeIdentityId", pNodeInfo->m_activeIdentityId, eAII_unknown_id, eAII_num_ids);

	//TODO: Change state to game not per node.

	int state = pNodeInfo->m_state;
	ser.Value("state", state, 'ui3');
	if (ser.IsReading())
	{
		CRY_ASSERT(!gEnv->bServer);
		if (state != pNodeInfo->m_state)
		{
			//Catches late joiners jumping straight from hidden to a capturing state, bypassing 'emerging'
			if(pNodeInfo->m_state == ENS_hidden)
			{
				IGameRulesSpawningModule *pSpawningModule = g_pGame->GetGameRules()->GetSpawningModule();
				if (pSpawningModule)
				{
					pSpawningModule->EnablePOI(pNodeInfo->m_id);	
				}

				IEntity* pNodeEntity = gEnv->pEntitySystem->GetEntity(pNodeInfo->m_id);
				CRY_ASSERT(pNodeEntity);
				if (pNodeEntity)
				{
					CryLog("CGameRulesObjective_Powerstruggle::NetSerializeNode() showing node entity %s", pNodeEntity->GetName());
					pNodeEntity->Invisible(false);

					// ensure the node is visible before we try playing any signals on it. Hope to fix some SFX on client issues
					// lua calls for this state are server only
				}
				else
				{
					CryLog("CGameRulesObjective_Powerstruggle::NetSerializeNode() failed to find node entity");
				}

				DbgLog("CGameRulesObjective_PowerStruggle::NetSerializeNode() setting normal spear loop signal for spear=%s", m_pGameRules->GetEntityName(pNodeInfo->m_id));
				pNodeInfo->m_signalPlayer.SetSignal(m_captureSignalId); 

				DbgLog("CGameRulesObjective_PowerStruggle::NetSerializeNode() playing signal for spear=%s", m_pGameRules->GetEntityName(pNodeInfo->m_id));
				//pNodeInfo->m_signalPlayer.Play(pNodeInfo->m_id);
				pNodeInfo->m_havePlayedSignal=true;
			}

			// changing state
			switch(state)
			{
				case ENS_emerging:
				{
					// we'll get 3 nodes emerging but only want to display the message once
					if (!m_haveDisplayedInitialSpearsEmergingMsg)
					{
						CAnnouncer::GetInstance()->Announce("SpearsEmerging", CAnnouncer::eAC_inGame);
						SHUDEventWrapper::GameStateNotify(m_stringNewSpearsEmerging.c_str(), SHUDEventWrapper::SMsgAudio(m_positiveSignalId));
						m_haveDisplayedInitialSpearsEmergingMsg=true;
					}

					CAudioSignalPlayer::JustPlay(m_spear_going_up, pNodeInfo->m_position);

					if (gEnv->IsClient())
					{
						Client_ShakeCameraForSpear(pNodeInfo);
					}

					break;
				}
				case ENS_neutral:
				case ENS_capturing_from_neutral:
					break;
				case ENS_captured:		
					DbgLog("CGameRulesObjective_PowerStruggle::NetSerializeNode() a node has transitioned into captured state");
					capStatusChanged=true; // this node needs to message about the capture state change, but wait till all nodes processed
					break;
				case ENS_capturing_from_capture:
					DbgLog("CGameRulesObjective_PowerStruggle::NetSerializeNode() a node has transitioned into capturing from captured state");
					Common_HandleSpearCaptureStarting(pNodeInfo);				
					break;
			}
		}
		pNodeInfo->SetState(static_cast<ENodeState>(state));
	}

	ser.Value("insideCount0", pNodeInfo->m_insideCount[0], 'ui4');
	ser.Value("insideCount1", pNodeInfo->m_insideCount[1], 'ui4');
	ser.Value("captureStatus", pNodeInfo->m_captureStatus, 'sone');		 // sone policy is a float between -1 and 1

	return capStatusChanged;
}

//------------------------------------------------------------------------
bool CGameRulesObjective_PowerStruggle::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	if (aspect == POWER_STRUGGLE_ASPECT)
	{
		bool capStatusChanged[MAX_NODES];

		for (int i = 0; i < MAX_NODES; ++ i)
		{
			SNodeInfo *pNodeInfo = &m_nodes[i];
			capStatusChanged[i] = NetSerializeNode(pNodeInfo, ser, aspect, profile, flags);
		}
				
		// after all nodes have been updated, message for any capture status changes that have occurred now the states are correct for all nodes and the correct spear count can be guaranteed
		for (int i = 0; i < MAX_NODES; ++ i)
		{
			if (capStatusChanged[i])
			{
				SNodeInfo *pNodeInfo = &m_nodes[i];

				DbgLog("CGameRulesObjective_PowerStruggle::NetSerialize() calling NodeCaptureStatusChanged()");
				NodeCaptureStatusChanged(pNodeInfo); 
			}
		}

		for(int i = 0; i < NUM_TEAMS; ++i)
		{
			SChargeeInfo& chargee = m_Chargees[i];
				
			float prevChargeValueWas=chargee.m_PrevChargeValue;
			float chargeValueWas=chargee.m_ChargeValue;

			ser.Value("chargeValue", chargee.m_ChargeValue, 'sone');

			if (ser.IsReading())
			{
				//DbgLog("CGameRulesObjective_PowerStruggle::NetSerialize() chargee[%d]; teamId=%d; prevChargeValue: %f->%f; chargeValue: %f->%f", i, chargee.m_TeamId, prevChargeValueWas, chargee.m_PrevChargeValue, chargeValueWas, chargee.m_ChargeValue);
				chargee.m_PrevChargeValue = chargeValueWas;
			}
		}

		if (ser.IsReading())
		{
			SChargeeInfo *pWinningChargee = NULL;
			if (m_Chargees[0].m_ChargeValue > m_Chargees[1].m_ChargeValue)
			{
				pWinningChargee = &m_Chargees[0];
			}
			else
			{
				pWinningChargee = &m_Chargees[1];
			}

			// only do announcements for the currently winning team
			Common_CheckNotifyStringForChargee(pWinningChargee, pWinningChargee->m_PrevChargeValue, pWinningChargee->m_ChargeValue);
		}

		if(gEnv->IsClient())
		{
			Client_SetAllIcons(0.f);
		}
	}

	return true;
}


//------------------------------------------------------------------------
void CGameRulesObjective_PowerStruggle::OnEntitySignal(EntityId entityId, int signal)
{
	if (gEnv->IsEditing())
		return;

	if (gEnv->bServer)
	{
		switch(signal)
		{
			case eEntitySignal_node_buried:
			{
				break;
			}
			case eEntitySignal_node_emerged:
			{
				if (SNodeInfo *pNodeInfo = Common_FindNodeFromEntityId(entityId))
				{
					if (pNodeInfo->m_state == ENS_emerging)
					{
						DbgLog("CGameRulesObjective_PowerStruggle::OnEntitySignal() entity=%s has emerged. Setting state to neutral", m_pGameRules->GetEntityName(entityId));
						pNodeInfo->SetState(ENS_neutral);
					}
					else
					{
						DbgLog("CGameRulesObjective_PowerStruggle::OnEntitySignal() entity=%s has emerged. But not in state emerging in state %d. Doing nothing, presuming this spear has already been interacted with whilst emerging.", m_pGameRules->GetEntityName(entityId), pNodeInfo->m_state);
					}
				}
				break;
			}
			default:
				CRY_ASSERT_MESSAGE(0, string().Format("OnEntitySignal() unhandled signal %d", signal));
				break;
		}
	}

}

//------------------------------------------------------------------------
void CGameRulesObjective_PowerStruggle::OnEntityEvent( IEntity *pEntity, const SEntityEvent& event )
{
	EntityId insideId = (EntityId) event.nParam[0];
	EntityId entityId = pEntity->GetId();
	SNodeInfo *pNodeInfo=Common_FindNodeFromEntityId(entityId);
	CRY_ASSERT_MESSAGE(pNodeInfo, "unable to find a node from the entity who's event we've received. This shouldn't happen");

	if (event.event == ENTITY_EVENT_ENTERAREA)
	{
		IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(insideId);
		if ( CGameRulesObjective_PowerStruggle::CanActorCaptureNodes(pActor) )
		{
			int teamId = m_pGameRules->GetTeam(insideId);
			int teamIndex = teamId - 1;
			if ((teamIndex == 0) || (teamIndex == 1))
			{
				pNodeInfo->m_insideBoxEntities[teamIndex].push_back(insideId);
			}
		}
	}
	else // ENTITY_EVENT_LEAVEAREA
	{
		Common_RemovePlayerFromNode(insideId, pNodeInfo);
	}
}

//------------------------------------------------------------------------
void CGameRulesObjective_PowerStruggle::OnEntityKilled(const HitInfo &hitInfo)
{
	Common_OnPlayerKilledOrLeft(hitInfo.targetId, hitInfo.shooterId);
}

//------------------------------------------------------------------------
void CGameRulesObjective_PowerStruggle::OnClientDisconnect(int channelId, EntityId playerId)
{
	Common_OnPlayerKilledOrLeft(playerId, 0);

	//remove them from players who have solo captured, as they can't receive the award anymore
	for( uint iPlayer = 0; iPlayer < m_soloCapturingPlayers.size(); iPlayer++ )
	{
		if( m_soloCapturingPlayers[ iPlayer ] == playerId )
		{
			m_soloCapturingPlayers.removeAt( iPlayer );
			break;
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesObjective_PowerStruggle::OnOwnClientEnteredGame()
{
	Client_SetAllIcons(0.f);
}

//------------------------------------------------------------------------
void CGameRulesObjective_PowerStruggle::NodeCaptureStatusChanged(SNodeInfo *pNodeInfo)
{
	int teamId = pNodeInfo->m_lastCapturedTeamId;
	
	if (teamId > 0 && gEnv->IsClient())
	{
		bool announcedAll=false;

		int team1Count=0, team2Count=0;
		for (int i = 0; i < MAX_NODES; ++ i)
		{
			SNodeInfo *pNode = &m_nodes[i];
			if (pNode->m_lastCapturedTeamId == 1)
				team1Count++;
			else if (pNode->m_lastCapturedTeamId == 2)
				team2Count++;
		}
		
		if (team1Count == MAX_NODES)
		{
			CAnnouncer::GetInstance()->AnnounceFromTeamId(1, "AllSpears", CAnnouncer::eAC_inGame);
			announcedAll=true;
		}
		else if (team2Count == MAX_NODES)
		{
			CAnnouncer::GetInstance()->AnnounceFromTeamId(2, "AllSpears", CAnnouncer::eAC_inGame);
			announcedAll=true;
		}

		if (!announcedAll)	// only announce individual spear captures if we've not just announced all spears taken
		{
			switch (pNodeInfo->m_activeIdentityId)
			{
				case eAII_spearA_id:
					CAnnouncer::GetInstance()->AnnounceFromTeamId(teamId, "SpearCapturedA", CAnnouncer::eAC_inGame);
					break;
				case eAII_spearB_id:
					CAnnouncer::GetInstance()->AnnounceFromTeamId(teamId, "SpearCapturedB", CAnnouncer::eAC_inGame);
					break;
				case eAII_spearC_id:
					CAnnouncer::GetInstance()->AnnounceFromTeamId(teamId, "SpearCapturedC", CAnnouncer::eAC_inGame);
					break;
				default:
					CRY_ASSERT_MESSAGE(0, string().Format("unhandled activeIdentityId=%d", pNodeInfo->m_activeIdentityId));
					break;
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesObjective_PowerStruggle::Common_NewlyContestedSpear(SNodeInfo *pNodeInfo)
{
	switch (pNodeInfo->m_activeIdentityId)
	{
		case eAII_spearA_id:
			CAnnouncer::GetInstance()->Announce("SpearContendedA", CAnnouncer::eAC_inGame);
			break;
		case eAII_spearB_id:
			CAnnouncer::GetInstance()->Announce("SpearContendedB", CAnnouncer::eAC_inGame);
			break;
		case eAII_spearC_id:
			CAnnouncer::GetInstance()->Announce("SpearContendedC", CAnnouncer::eAC_inGame);
			break;
		default:
			CRY_ASSERT_MESSAGE(0, string().Format("unhandled activeIdentityId=%d", pNodeInfo->m_activeIdentityId));
			break;
	}
}

//------------------------------------------------------------------------
void CGameRulesObjective_PowerStruggle::Client_SetIconForNode( SNodeInfo *pNodeInfo )
{
	// If this icon isn't being hidden.. Set to new
	EGameRulesMissionObjectives iconType = EGRMO_Unknown;
	ENodeHUDState	hudState = eNHS_none;

	float progress = 0.f;
	float captureStatus = pNodeInfo->m_captureStatus;

	EGameRulesMissionObjectives team1Node;
	ENodeHUDState								team1NodeHudState;
	EGameRulesMissionObjectives team1CapturingNode;
	ENodeHUDState								team1CapturingNodeHudState;
	EGameRulesMissionObjectives team1CapturingNodeFromTeam2;
	ENodeHUDState								team1CapturingNodeFromTeam2HudState;
	EGameRulesMissionObjectives team2Node;
	ENodeHUDState								team2NodeHudState;
	EGameRulesMissionObjectives team2CapturingNode;
	ENodeHUDState								team2CapturingNodeHudState;
	EGameRulesMissionObjectives team2CapturingNodeFromTeam1;
	ENodeHUDState								team2CapturingNodeFromTeam1HudState;

	EntityId localActorId = g_pGame->GetIGameFramework()->GetClientActorId();
	int localTeamId = m_pGameRules->GetTeam(localActorId);
	if (localTeamId == 1)
	{
		team1Node = m_iconFriendlyNode;
		team1NodeHudState = eNHS_friendlyOwned;
		team1CapturingNode = m_iconFriendlyCapturingNode;
		team1CapturingNodeHudState = eNHS_friendlyCaptureFromNeutral;
		team1CapturingNodeFromTeam2 = m_iconFriendlyCapturingNodeFromHostile;
		team1CapturingNodeFromTeam2HudState = eNHS_friendlyCaptureFromEnemy;

		team2Node = m_iconHostileNode;
		team2NodeHudState = eNHS_enemyOwned;
		team2CapturingNode = m_iconHostileCapturingNode;
		team2CapturingNodeHudState = eNHS_enemyCaptureFromNeutral;
		team2CapturingNodeFromTeam1 = m_iconHostileCapturingNodeFromFriendly;
		team2CapturingNodeFromTeam1HudState = eNHS_enemyCaptureFromFriendly;
	}
	else
	{
		team1Node = m_iconHostileNode;
		team1NodeHudState = eNHS_enemyOwned;
		team1CapturingNode = m_iconHostileCapturingNode;
		team1CapturingNodeHudState = eNHS_enemyCaptureFromNeutral;
		team1CapturingNodeFromTeam2 = m_iconHostileCapturingNodeFromFriendly;
		team1CapturingNodeFromTeam2HudState = eNHS_enemyCaptureFromFriendly;

		team2Node = m_iconFriendlyNode;
		team2NodeHudState = eNHS_friendlyOwned;
		team2CapturingNode = m_iconFriendlyCapturingNode;
		team2CapturingNodeHudState = eNHS_friendlyCaptureFromNeutral;
		team2CapturingNodeFromTeam1 = m_iconFriendlyCapturingNodeFromHostile;
		team2CapturingNodeFromTeam1HudState = eNHS_enemyCaptureFromFriendly;
	}
	
	// animating from team colour to other team colour when capturing
	switch(pNodeInfo->m_state)
	{
		case ENS_emerging:
		case ENS_neutral:
			iconType = m_iconNeutralNode;
			hudState = eNHS_neutral;
			break;
		case ENS_capturing_from_neutral:
			if (pNodeInfo->m_controllingTeamId == 1)
			{
				iconType = team1CapturingNode;
				hudState = team1CapturingNodeHudState;
			}
			else if (pNodeInfo->m_controllingTeamId == 2)
			{
				iconType = team2CapturingNode;
				hudState = team2CapturingNodeHudState;
			}
			else
			{
				iconType = m_iconNeutralNode;
				hudState = eNHS_neutral;
			}

			progress = captureStatus;
			break;
		case ENS_captured:	
			if (pNodeInfo->m_lastCapturedTeamId == 1)
			{
				iconType = team1Node;
				hudState = team1NodeHudState;
			}
			else // if (pNodeInfo->m_lastCapturedTeamId == 2)
			{
				CRY_ASSERT(pNodeInfo->m_lastCapturedTeamId == 2);
				iconType = team2Node;
				hudState = team2NodeHudState;
			}
			break;
		case ENS_capturing_from_capture:
			// TODO - use colour background icons (background=m_lastCapturedTeamId; foreground=m_lastControllingTeamId)
			if (pNodeInfo->m_lastCapturedTeamId == 1 && pNodeInfo->m_controllingTeamId == 2)
			{
				iconType = team2CapturingNodeFromTeam1;
				hudState = team2CapturingNodeFromTeam1HudState;
			}
			else if (pNodeInfo->m_lastCapturedTeamId == 2 && pNodeInfo->m_controllingTeamId == 1)
			{
				iconType = team1CapturingNodeFromTeam2;
				hudState = team1CapturingNodeFromTeam2HudState;
			}
			else if (pNodeInfo->m_lastCapturedTeamId == 1)
			{
				iconType = team1Node;
				hudState = team1NodeHudState;
			}
			else // if (pNodeInfo->m_lastCapturedTeamId == 2)
			{
				CRY_ASSERT(pNodeInfo->m_lastCapturedTeamId == 2);
				iconType = team2Node;
				hudState = team2NodeHudState;
			}
			
			progress = captureStatus;

			break;
	}

	// any states in contention?
	if	(	pNodeInfo->m_state == ENS_capturing_from_neutral ||
				pNodeInfo->m_state == ENS_capturing_from_capture ||
				pNodeInfo->m_state == ENS_captured )
	{
		if ( pNodeInfo->m_controllingTeamId == -1 )	// contention 
		{
			iconType = m_iconContentionOverNode;
			hudState = eNHS_contention;
		}
	}

	if (iconType != pNodeInfo->m_currentObjectiveIconType && g_pGame->GetIGameFramework()->GetClientActor() != NULL)
	{
		// Remove any existing icon if its changed
		if (pNodeInfo->m_currentObjectiveIconType != EGRMO_Unknown)
		{
			SHUDEventWrapper::OnRemoveObjectiveWithRadarEntity(pNodeInfo->m_id, pNodeInfo->m_id, 0);
		}

		// Now actually show the new icon, and update our cached type
		if (iconType != EGRMO_Unknown )
		{			
			const char *pText = s_nodeIdentityInfo[pNodeInfo->m_activeIdentityId].pIconName;
			SHUDEventWrapper::OnNewObjectiveWithRadarEntity(pNodeInfo->m_id, pNodeInfo->m_id, iconType, progress, 0, pText, ICON_STRING_WHITE_COLOUR);
		}

		pNodeInfo->m_currentObjectiveIconType = iconType; 
	}

	// only nodes that have an active identity need to signal the score element to update its icons
	// TODO - we need to handle the hiding state as as soon as the node is deactivated and buried its lost its active identity id
	if (pNodeInfo->m_activeIdentityId && hudState != pNodeInfo->m_currentHUDState)
	{
		SHUDEventWrapper::PowerStruggleNodeStateChange(pNodeInfo->m_activeIdentityId, hudState);
		pNodeInfo->m_currentHUDState = hudState;
	}
}

// only called for the winning team
void CGameRulesObjective_PowerStruggle::Common_CheckNotifyStringForChargee( SChargeeInfo* pChargeeInfo, const float fPrevChargeValue, const float fChargeValue )
{
	if(fChargeValue != fPrevChargeValue)
	{
		const char *announcement=NULL;

#if 0
		// we think we've lost assets for this too :-(
		if (fChargeValue >= 0.25f && fPrevChargeValue < 0.25f) // charge is 25% complete
		{
			announcement="SpearEnergy25";
		}
		else 
#endif

		if (fChargeValue >= 0.5f && fPrevChargeValue < 0.5f) // charge is 50% complete
		{
			announcement="SpearEnergy50";
		}
		else if (fChargeValue >= 0.75f && fPrevChargeValue < 0.75f) // charge is 75% complete
		{
			announcement="SpearEnergy75";
		}
		else if (fChargeValue >= 0.9f && fPrevChargeValue < 0.9f) // charge is 90% complete
		{
			announcement="SpearEnergy90";
		}

		if(announcement)
		{
			CAnnouncer::GetInstance()->AnnounceFromTeamId(pChargeeInfo->m_TeamId, announcement, CAnnouncer::eAC_inGame);
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesObjective_PowerStruggle::Client_SetAllIcons(const float frameTime)
{
	for (int i = 0; i < MAX_NODES; ++ i)
	{
		SNodeInfo *pNodeInfo = &m_nodes[i];
		if(pNodeInfo->m_id)
			Client_SetIconForNode(pNodeInfo);
	}
}

//------------------------------------------------------------------------
void CGameRulesObjective_PowerStruggle::Common_AddNode(IEntity *pNodeEntity, int indexToUse)
{
	CryLog("CGameRulesObjective_PowerStruggle::Common_AddNode() adding %s", pNodeEntity->GetName());

	SNodeInfo *pNodeInfo = &m_nodes[indexToUse];
	EntityId nodeId = pNodeEntity->GetId();

	pNodeInfo->m_id = nodeId;
	Vec3 nodePos = pNodeEntity->GetPos();
	pNodeInfo->m_position  = nodePos;

	float radius = 5.f, height = 5.f;
	float zOffset = -1.f;
	const char *letter=NULL;

	IScriptTable *pScript = pNodeEntity->GetScriptTable();
	if (pScript)
	{
		SmartScriptTable propertiesTable;
		if (pScript->GetValue("Properties", propertiesTable))
		{
			propertiesTable->GetValue("ControlRadius", radius);
			propertiesTable->GetValue("ControlHeight", height);
			propertiesTable->GetValue("ControlOffsetZ", zOffset);
			propertiesTable->GetValue("Letter", letter);
		}
	}

	if (m_setupIsSpecifyingNodeLetters)
	{
		if (!gEnv->IsEditor())
		{
			DesignerWarning(letter && letter[0] != 0 && (letter[0]=='a'||letter[0]=='A'||letter[0]=='b'||letter[0]=='B'||letter[0]=='c'||letter[0]=='C'), "setup is specifying node letters directly, yet this node %s doesn't have a valid letter set!", pNodeEntity->GetName());
		}
	}

	if (letter && letter[0] != 0)
	{
		CryLog("CGameRulesObjective_PowerStruggle::Common_AddNode() pNodeEntity=%s has specified letter of %s", pNodeEntity->GetName(), letter);
		m_setupIsSpecifyingNodeLetters=true;

		if (!gEnv->IsEditor())
		{
			DesignerWarning(!m_setupHasHadNodeWithNoLetterSet, "setup is specifying node letters directly with this node %s however another node has been setup with NO letter set!", pNodeEntity->GetName());
		}

		if (letter[0]=='a' || letter[0]=='A')
		{
			CryLog("CGameRulesObjective_PowerStruggle::Common_AddNode() setting activeIdentityId to eAII_spearA_id");
			pNodeInfo->m_activeIdentityId = eAII_spearA_id;
		}
		else if (letter[0]=='b' || letter[0]=='B')
		{
			CryLog("CGameRulesObjective_PowerStruggle::Common_AddNode() setting activeIdentityId to eAII_spearB_id");
			pNodeInfo->m_activeIdentityId = eAII_spearB_id;
		}
		else if (letter[0]=='c' || letter[0]=='C')
		{
			CryLog("CGameRulesObjective_PowerStruggle::Common_AddNode() setting activeIdentityId to eAII_spearC_id");
			pNodeInfo->m_activeIdentityId = eAII_spearC_id;
		}
		else if (!gEnv->IsEditor())
		{
			CryLog("CGameRulesObjective_PowerStruggle::Common_AddNode() failed to set an activeIdentityId from letter string=%s", letter);
			DesignerWarning(0, "failed to set an activeIdentityId from letter string=%s", letter);
		}
	}
	else
	{
		m_setupHasHadNodeWithNoLetterSet=true;
	}

	pNodeInfo->m_controlRadius = radius;
	pNodeInfo->m_controlHeight = height;
	pNodeInfo->m_controlZOffset = zOffset;
	pNodeInfo->m_cameraShakeId = m_spearCameraShakeIdToUse++;

	gEnv->pEntitySystem->AddEntityEventListener(nodeId, ENTITY_EVENT_ENTERAREA, this);
	gEnv->pEntitySystem->AddEntityEventListener(nodeId, ENTITY_EVENT_LEAVEAREA, this);

	pNodeEntity->SetFlags(ENTITY_FLAG_UPDATE_HIDDEN);

	if (gEnv->bServer)
	{
		Server_InitNode(pNodeInfo);	// has to be called after this node has been added so the nodeInfo can be found from the entity when getting the signal back from lua
	}
	else //Client
	{
		//Instead of using additional RMIs to set state, just update the entity state according to the nodeinfo when the
		//	node entity is added
		if(pNodeInfo->m_state == ENS_hidden)
		{
			if(!pNodeEntity->IsHidden())
			{
				DbgLog("CGameRulesObjective_PowerStruggle::Common_AddNode() spear=%s, HIDE NODE", m_pGameRules->GetEntityName(pNodeInfo->m_id));
				pNodeEntity->Invisible(true);
			}
		}
		else if(pNodeEntity->IsHidden())
		{
			pNodeEntity->Invisible(false);
			DbgLog("CGameRulesObjective_PowerStruggle::Common_AddNode() spear=%s, SHOW NODE", m_pGameRules->GetEntityName(pNodeInfo->m_id));
		}
	}

	// TODO - manage these, they need to be disabled here and enabled/disabled as they are shown/hidden
	IGameRulesSpawningModule *pSpawningModule = g_pGame->GetGameRules()->GetSpawningModule();
	if (pSpawningModule)
	{
		pSpawningModule->AddAvoidPOI(nodeId, m_avoidSpawnPOIDistance, false, true);		// whilst the nodes aren't static they're disabled when they're out of position (if the anim even changes the pos). This may need updating if it caches their current position which is incorrect
	}
}

//------------------------------------------------------------------------
void CGameRulesObjective_PowerStruggle::Common_RemovePlayerFromNode(EntityId playerId, SNodeInfo *pNodeInfo)
{
	for (int teamIndex = 0; teamIndex < NUM_TEAMS; ++ teamIndex)
	{
		int numInside = pNodeInfo->m_insideBoxEntities[teamIndex].size();
		for (int i = 0; i < numInside; ++ i)
		{
			if (pNodeInfo->m_insideBoxEntities[teamIndex][i] == playerId)
			{
				pNodeInfo->m_insideBoxEntities[teamIndex].removeAt(i);
				return;
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesObjective_PowerStruggle::Common_OnPlayerKilledOrLeft(EntityId targetId, EntityId shooterId)
{
	for (int i = 0; i < MAX_NODES; ++ i)
	{
		Common_RemovePlayerFromNode(targetId, &m_nodes[i]);
	}
}

//-------------------------------------------------------------------------------------------------
// sort the nodes by ID so they will be the same between clients and servers
// - i'd expect this anyways from the same setup file?
// It's important that this is a sort on only the id, pos, and m_bUsed values
// Netserialise may have already occurred populating the nodes with information according to their already sorted 
// ordering on the server. This sorting here will bring the entityIds to match and will leave any existing sync-ed data 
// in the correct node. 
// Do NOT add any code here to copy other members across without considering that you may be copying netserialised data
// into the wrong node
void CGameRulesObjective_PowerStruggle::Common_SortNodes()
{
	for (int i = 0; i < MAX_NODES-1; i++)
	{
		if(m_nodes[i].m_id > m_nodes[i+1].m_id)
			CryFatalError("Node entity IDs are out of order, sorting code needs to be reinstated");
	}

#if 0
	for(int i = 0; i < MAX_NODES - 1; ++i)
	{
		for(int j = i+1; j < MAX_NODES; ++j)
		{
			if(m_nodes[i].m_id < m_nodes[j].m_id)
			{
				EntityId tempId = m_nodes[i].m_id;
				m_nodes[i].m_id = m_nodes[j].m_id;
				m_nodes[j].m_id = tempId;

				Vec3 tempPos = m_nodes[i].m_position;
				m_nodes[i].m_position = m_nodes[j].m_position;
				m_nodes[j].m_position = tempPos;

				float tempRadius = m_nodes[i].m_controlRadius;
				m_nodes[i].m_controlRadius = m_nodes[j].m_controlRadius;
				m_nodes[j].m_controlRadius = tempRadius;

				float tempHeight = m_nodes[i].m_controlHeight;
				m_nodes[i].m_controlHeight = m_nodes[j].m_controlHeight;
				m_nodes[j].m_controlHeight = tempHeight;

				float tempZOffset = m_nodes[i].m_controlZOffset;
				m_nodes[i].m_controlZOffset = m_nodes[j].m_controlZOffset;
				m_nodes[j].m_controlZOffset = tempZOffset;
			}
		}
	}
#endif
}


//-------------------------------------------------------------------------------------------------

int CGameRulesObjective_PowerStruggle::Server_CalculateWinningTeam()
{
	// Team with most charge wins...
	// If both teams have same charge + then draw. 

	// if team 1 charge > than team 2 charge
	// team 1 win
	// if team 2 charge > than team 1 charge
	// team 2 win
	// else  if T1 charge == T2 charge
	// draw

	// Test to see if one team wins by having more charge for their super-weapon
	if(m_Chargees[0].m_ChargeValue > m_Chargees[1].m_ChargeValue)
	{
		return m_Chargees[0].m_TeamId;
	}
	else if(m_Chargees[1].m_ChargeValue > m_Chargees[0].m_ChargeValue)
	{
		return m_Chargees[1].m_TeamId; 
	}

	// If none of the win conditions above detected.. then its a draw
	return 0; // DRAW
}

void CGameRulesObjective_PowerStruggle::Client_ShakeCameraForSpear(const SNodeInfo *pNodeInfo)
{
	IActor *clientActor = g_pGame->GetIGameFramework()->GetClientActor();
	CRY_ASSERT(clientActor);
	IView* pView = clientActor ? (g_pGame->GetIGameFramework()->GetIViewSystem()->GetViewByEntityId(clientActor->GetEntityId())) : NULL;
	if (pView)
	{
		const Vec3 delta = pView->GetCurrentParams()->position - pNodeInfo->m_position;
		const float dist = delta.GetLength();

		// TODO - change for giant spear
		float shakeMaxR = 50.f;
		float shakeMinR = 1.0f;
		float shakeAmount = 0.1f; // 0.03f; //0.01f; // 1.f; // degrees
		float shakeRnd = 0.04f; // 0.003f; // 0.001f; //0.05f;
		float halfDuration = 2.0f;

		const float deltaDistanceFromShake = shakeMaxR - dist;
		const float shakeRadiusRange = shakeMaxR - shakeMinR;

		const float shakeFactor = min(1.0f, deltaDistanceFromShake * __fres(shakeRadiusRange));
		const float angleShake = 2.0f * DEG2RAD(shakeAmount);
		const float shiftShake = shakeAmount * 0.2f;

		const Ang3 shakeAngle(angleShake, angleShake, angleShake);
		const Vec3 shakeShift(shiftShake, shiftShake, shiftShake);

		DbgLog("CGameRulesObjective_PowerStruggle::Client_ShakeCameraForSpear() spear=%s; angleShake=%f; shiftShake=%f; shakeFactor=%f; shakeRnd=%f; deltaDistFromShake=%f", m_pGameRules->GetEntityName(pNodeInfo->m_id), angleShake, shiftShake, shakeFactor, shakeRnd, deltaDistanceFromShake);
		pView->SetViewShake(shakeAngle, shakeShift, halfDuration + (halfDuration * shakeFactor), 0.05f, shakeRnd, pNodeInfo->m_cameraShakeId);
	}
}

bool CGameRulesObjective_PowerStruggle::Client_UpdateCaptureProgressBar( const SNodeInfo *pNodeInfo )
{
	bool bClientInvolvedInCapture = false;

	assert(pNodeInfo); 

	// Calculate which team controls
	int		clientTeamId = m_pGameRules->GetTeam(g_pGame->GetIGameFramework()->GetClientActorId());
	
	bool contention=false;
	bool friendlyCapturing=false;
	enum {
		ECaptureFromNeutral=0,
		ECaptureFromFriendly,
		ECaptureFromEnemy,
	} capturingFrom = ECaptureFromNeutral;
	const char *barString="";

	if (pNodeInfo->m_captureStatusOwningTeam == clientTeamId)
	{
		friendlyCapturing=true;
	}
	
	if (pNodeInfo->m_lastCapturedTeamId > 0)
	{
		if (clientTeamId == pNodeInfo->m_lastCapturedTeamId)
		{
			capturingFrom = ECaptureFromFriendly;
		}
		else
		{
			capturingFrom = ECaptureFromEnemy;
		}
	}

	float  zeroToOneProgress= fabsf(pNodeInfo->m_captureStatus); 
	if (pNodeInfo->m_state == ENS_captured)
	{
		// captureStatus will have been reset to 0.f waiting for any opponents to try retaking the node from 0.f but for the sake of this bar should show 1.0f for fully captured
		zeroToOneProgress = 1.0f;
	}

	if (pNodeInfo->m_controllingTeamId == CONTESTED_TEAM_ID)
	{
		contention=true;
	}

	if (pNodeInfo->m_state == ENS_captured)
	{
		barString = m_stringCaptured;
		bClientInvolvedInCapture = true;
	}
	else
	{
		barString = m_stringCapturing;
	}

	EHUDPowerStruggleCaptureBarType newType=eHPSCBT_None;
	switch(capturingFrom)
	{
	case ECaptureFromNeutral:
		if (friendlyCapturing)
		{
			newType = eHPSCBT_FriendlyCaptureFromNeutral;
		}
		else
		{
			newType = eHPSCBT_EnemyCaptureFromNeutral;
		}
		break;
	case ECaptureFromEnemy:
		if (friendlyCapturing)
		{
			newType = eHPSCBT_FriendlyCaptureFromEnemy;
		}
		else
		{
			// the node is enemy owned but the enemy is capturing too. We're either fully enemy charged or in contention with an enemy captured node. Need to show red with 100% capture flashing
			newType = eHPSCBT_EnemyCaptureFromNeutral;
		}
		break;
	case ECaptureFromFriendly:
		if (friendlyCapturing)
		{
			// the node is friendly owned but the friendlies are capturing too. We're either fully friendly charged or in contention with an enemy captured node. Need to show blue with 100% capture flashing
			newType = eHPSCBT_FriendlyCaptureFromNeutral;
		}
		else
		{
			newType = eHPSCBT_EnemyCaptureFromFriendly;
		}
		break;
	default:
		CRY_ASSERT_MESSAGE(0, string().Format("unhandled capturingFrom=%d", capturingFrom));
		break;
	}

	// TODO - need giant spear handling
	SHUDEventWrapper::PowerStruggleManageCaptureBar(newType, zeroToOneProgress, contention, barString);

	return bClientInvolvedInCapture;
}

void CGameRulesObjective_PowerStruggle::LogSpearStateToTelemetry( uint8 state, SNodeInfo* pSpearInfo )
{
	if( CStatsRecordingMgr* pStatsRecordingMgr = g_pGame->GetStatsRecorder() )
	{
		if( IStatsTracker* pRoundTracker = pStatsRecordingMgr->GetRoundTracker() )
		{
			CSpearStateEvent* pEvent = new CSpearStateEvent( pSpearInfo->m_activeIdentityId, state, pSpearInfo->m_insideCount[ 0 ], pSpearInfo->m_insideCount[ 1 ], pSpearInfo->m_lastCapturedTeamId, pSpearInfo->m_captureStatusOwningTeam	);
			pRoundTracker->Event( eGSE_Spears_SpearStateChanged, pEvent );
		}
	}
}

bool CGameRulesObjective_PowerStruggle::HasSoloCapturedSpear( EntityId playerId )
{
	CryFixedArray<EntityId, MAX_PLAYER_LIMIT>::iterator entIt = std::find( m_soloCapturingPlayers.begin(), m_soloCapturingPlayers.end(), playerId );
	if( entIt != m_soloCapturingPlayers.end() )
	{
		//found
		return true;
	}
	return false;
}

void CGameRulesObjective_PowerStruggle::Client_CheckCapturedAllPoints()
{
	bool bHasCapturesAllPoints = true;

	for (int i = 0; i < MAX_NODES; ++ i)
	{
		SNodeInfo *pNodeInfo = &m_nodes[i];
		if(!pNodeInfo->m_clientPlayerPresentAtCaptureTime)
		{
			bHasCapturesAllPoints = false;
			break;
		}
	}

	if(bHasCapturesAllPoints)
	{
		g_pGame->GetPersistantStats()->IncrementClientStats(EIPS_AllSpearsCaptured, 1);
	}
}

#undef POWER_STRUGGLE_ASPECT


// play nice with selotaped compiling
#undef DbgLog
#undef DbgLogAlways 