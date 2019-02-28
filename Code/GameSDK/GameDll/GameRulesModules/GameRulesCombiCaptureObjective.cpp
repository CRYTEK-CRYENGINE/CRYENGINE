// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Implementation of a "combination capture" objective.
		One team's goal is to be in proximity of one or more of multiple "capture"
		points for a specified combined duration of time.
		The other team must prevent them.

	-------------------------------------------------------------------------
	History:
	- 16:12:2009  : Created by Thomas Houghton
	- 03:03:2009  : Refactored to use HoldObjectiveBase by Colin Gulliver

*************************************************************************/

/*************************************************************************
	TODO
	----
	+ cache round number and attackingteam at round start, to get around messiness inbetween rounds when that information changes before everything else resets (can also get rid of the IsRestarting() clause in a couple of the asserts too)

*************************************************************************/

#include "StdAfx.h"
#include "GameRulesCombiCaptureObjective.h"
#include <CrySystem/XML/IXml.h>
#include "GameRules.h"
#include "Player.h"
#include "Utility/CryWatch.h"
#include <CryString/StringUtils.h>
#include "Utility/CryDebugLog.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "UI/HUD/HUDUtils.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "GameRulesModules/IGameRulesScoringModule.h"
#include "GameRulesModules/IGameRulesSpawningModule.h"
#include "GameRulesModules/IGameRulesRoundsModule.h"
#include "GameRulesModules/IGameRulesPlayerStatsModule.h"
#include "GameRulesModules/IGameRulesStateModule.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "Battlechatter.h"
#include "PersistantStats.h"
#include "GameRulesModules/GameRulesStandardVictoryConditionsTeam.h"
#include "EquipmentLoadout.h"
#include "Audio/Announcer.h"
#if NUM_ASPECTS > 8
	#define COMBICAPTURE_OBJECTIVE_STATE_ASPECT		eEA_GameServerB
#else
	#define COMBICAPTURE_OBJECTIVE_STATE_ASPECT		eEA_GameServerStatic
#endif

/*(static)*/ const int  CGameRulesCombiCaptureObjective::AMOUNT_OF_DESIRED_CAP_ENTS_MORE_THAN_PLAYERS	= 100;
/*(static)*/ const int  CGameRulesCombiCaptureObjective::MAX_DESIRED_CAP_ENTS													= HOLD_OBJECTIVE_MAX_ENTITIES;  // ie. and there's no point in the desired num of Capture Entities being greater than HOLD_OBJECTIVE_MAX_ENTITIES
/*(static)*/ const int  CGameRulesCombiCaptureObjective::MIN_DESIRED_CAP_ENTS													= CLAMP((1 + AMOUNT_OF_DESIRED_CAP_ENTS_MORE_THAN_PLAYERS), 1, MAX_DESIRED_CAP_ENTS);  // ie. there should always be at least 1 person on a team (hence "1 + ..."), but we also don't want this value to drop below 1 (eg. if AMOUNT_OF_DESIRED_CAP_ENTS_MORE_THAN_PLAYERS above is negative for some crazy reason)

#define GET_COMBI_CAPTURE_ENTITY	\
	SCaptureEntity *pCaptureEntity = static_cast<SCaptureEntity *>(pDetails->m_pAdditionalData);	\
	if (!pCaptureEntity)	\
	{	\
		CRY_ASSERT(pCaptureEntity);	\
		CryLog("CGameRulesCombiCaptureObjective failed to find capture entity from hold entity"); \
		return;	\
	}

#define GET_COMBI_CAPTURE_ENTITY_RET(returnValue)	\
	SCaptureEntity *pCaptureEntity = static_cast<SCaptureEntity *>(pDetails->m_pAdditionalData);	\
	if (!pCaptureEntity)	\
	{	\
		CRY_ASSERT(pCaptureEntity);	\
		CryLog("CGameRulesCombiCaptureObjective failed to find capture entity from hold entity"); \
		return returnValue;	\
	}

#define LOADOUT_PACKAGE_GROUP_ATTACKERS CEquipmentLoadout::SDK
#define LOADOUT_PACKAGE_GROUP_DEFENDERS CEquipmentLoadout::SDK

//========================================================================

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::SCaptureEntity::Associate(const SHoldEntityDetails* pDetails, CGameRulesCombiCaptureObjective* pCombiCapObj)
{
	if (gEnv->IsClient())
	{
		CRY_ASSERT(!m_alarmSignalPlayer.IsPlaying(pDetails->m_id));
		m_alarmSignalPlayer.SetSignal(pCombiCapObj->m_alarmSignalId);
	}
}

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::SCaptureEntity::SetEnabled(const bool enable, const bool updateIcon, const SHoldEntityDetails* pDetails)
{
	CRY_ASSERT(pDetails && (pDetails->m_pAdditionalData == this));
	if (pDetails && (pDetails->m_pAdditionalData == this))
	{
		if (enable != m_enabled)
		{
			const EntityId  eid = pDetails->m_id;

			if (IEntity* pEntity=gEnv->pEntitySystem->GetEntity(eid))
			{
				if (IEntityRender* renderProxy=pEntity->GetRenderInterface())
				{
					if (IRenderNode* renderNode=renderProxy->GetRenderNode())
					{
						renderNode->SetRndFlags(ERF_HIDDEN, !enable);
					}
				}

				pEntity->EnablePhysics(enable);

				/*if (enable == false)
				{
					if (gEnv->IsClient() && m_alarmSignalPlayer.IsPlaying(eid))
					{
						m_alarmSignalPlayer.Stop(eid); 
					}
				}*/
			}

			m_needIconUpdate = updateIcon;	

			m_enabled = enable;
		}
	}
}

//========================================================================

//------------------------------------------------------------------------
CGameRulesCombiCaptureObjective::SSvCaptureScorer* CGameRulesCombiCaptureObjective::CSvCaptureScorersList::FindByEntityId(const EntityId eid)
{
	SSvCaptureScorer*  pScorer = NULL;

	iterator  beginIt = begin();
	iterator  endIt = end();
	for (iterator it = beginIt; it != endIt; ++it)
	{
		SSvCaptureScorer*  pIterScorer = &(*it);
		if (pIterScorer->m_eid == eid)
		{
			pScorer = pIterScorer;
			break;
		}
	}

	return pScorer;
}

//========================================================================

//------------------------------------------------------------------------
CGameRulesCombiCaptureObjective::CGameRulesCombiCaptureObjective()
	: m_bBetweenRounds(false)
{
	for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; ++ i)
	{
		m_additionalInfo[i].Reset(NULL);
	}

	m_ourCapturePoint = EGRMO_Unknown;
	m_theirCapturePoint = EGRMO_Unknown;
	m_usCapturingPoint = EGRMO_Unknown;
	m_themCapturingPoint = EGRMO_Unknown;

	m_iconPriority = 0;
	m_numActiveEntities = 0;

	m_attackingTeamId = 1;
	m_prevAttackingTeamId = 0;
	m_clientTeamId = 0;

	m_highestNumDesiredCapEntsThisRound = MIN_DESIRED_CAP_ENTS;
	CryLog("CGameRulesCombiCaptureObjective::CGameRulesCombiCaptureObjective: RESET m_highestNumDesiredCapEntsThisRound to %d", m_highestNumDesiredCapEntsThisRound);

	m_combiProgress = 0.f;
	m_combiProgressBanked = 0.f;

	m_goalCombiCaptureTime = 0.01f;

	m_progressBankingThreshold = 0.f;

	m_defWin_timeRemainBonus_minTime = 0.f;
	m_defWin_timeRemainBonus_minPoints = 0.f;

	m_captureScoringThreshold = 0.f;
	m_captureScoringAssistThreshold = 0.f;
	m_captureScoringAssistFrac = 0.f;

	m_lastMinuteSkillAssessmentThreshold = 0.f;

	m_doMidThresholdPartialCaptureScoring = false;

	m_contestable = false;

	m_useIcons = false;
	m_allowMultiPlayerCaptures = false;

	m_updatedCombiProgressThisFrame = false;
	m_bUpdatedBankedProgressThisFrame = false;

	m_captureSignalId = INVALID_AUDIOSIGNAL_ID;
	m_interruptSignalId = INVALID_AUDIOSIGNAL_ID;
	m_inactiveSignalId = INVALID_AUDIOSIGNAL_ID;
	m_alarmSignalId = INVALID_AUDIOSIGNAL_ID;

	m_currentVO = eCCVO_Initial;

	m_combiVOData[eCCVO_25].Init("Hacking25", 0.25f);
	m_combiVOData[eCCVO_50].Init("Hacking50", 0.5f);
	m_combiVOData[eCCVO_75].Init("Hacking75", 0.75f);
	m_combiVOData[eCCVO_90].Init("Hacking90", 0.9f);
	// note that the Hacking100 sound gets triggered in ClTeamScoreFeedback(), otherwise the game ends before it would've got processed

	if (CGameRules* pGameRules=g_pGame->GetGameRules())
	{
		pGameRules->RegisterRoundsListener(this);
		pGameRules->RegisterPlayerStatsListener(this);
		pGameRules->RegisterRevivedListener(this);
		pGameRules->AddGameRulesListener(this);
	}
}

//------------------------------------------------------------------------
CGameRulesCombiCaptureObjective::~CGameRulesCombiCaptureObjective()
{
	CGameRules *pGameRules = g_pGame->GetGameRules();

	if (pGameRules)
	{
		pGameRules->UnRegisterRoundsListener(this);
		pGameRules->UnRegisterPlayerStatsListener(this);
		pGameRules->UnRegisterRevivedListener(this);
		pGameRules->RemoveGameRulesListener(this);
	}
}

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::Init( XmlNodeRef xml )
{
	BaseType::Init(xml);

	float  fscratch = 0.f;

	if (xml->getAttr("goalCombiCaptureTime", m_goalCombiCaptureTime))
	{
		CryLog("CGameRulesCombiCaptureObjective::Init, goal combi capture time set to %f", m_goalCombiCaptureTime);
		m_goalCombiCaptureTime = std::max(0.01f, m_goalCombiCaptureTime);
	}

	if (xml->getAttr("progressBankingIntervalSecs", fscratch))
	{
		m_progressBankingThreshold = (((fscratch < m_goalCombiCaptureTime) && (m_goalCombiCaptureTime > 0.f)) ? (fscratch / m_goalCombiCaptureTime) : 0.f);
		CryLog("CGameRulesCombiCaptureObjective::Init, read \"progressBankingIntervalSecs\" as %f, set m_progressBankingThreshold to %f", fscratch, m_progressBankingThreshold);
	}

	if (xml->getAttr("contestable", m_contestable))
	{
		CryLog("CGameRulesCombiCaptureObjective::Init, contestable set to %d", m_contestable);
	}

	if (xml->getAttr("allowMultiPlayerCaptures", m_allowMultiPlayerCaptures))
	{
		CryLog("CGameRulesCombiCaptureObjective::Init, allowMultiPlayerCaptures set to %d", m_allowMultiPlayerCaptures);
	}

	if (xml->getAttr("defWin_timeRemainBonus_minTime", m_defWin_timeRemainBonus_minTime))
	{
		CryLog("CGameRulesCombiCaptureObjective::Init, defWin_timeRemainBonus_minTime set to %f", m_defWin_timeRemainBonus_minTime);
	}

	if (xml->getAttr("defWin_timeRemainBonus_minPoints", m_defWin_timeRemainBonus_minPoints))
	{
		CryLog("CGameRulesCombiCaptureObjective::Init, defWin_timeRemainBonus_minPoints set to %f", m_defWin_timeRemainBonus_minPoints);
	}

	if (xml->getAttr("captureScoringThreshold", m_captureScoringThreshold))
	{
		CryLog("CGameRulesCombiCaptureObjective::Init, captureScoringThreshold set to %f", m_captureScoringThreshold);
	}

	bool  hasCaptureAssistScoring = false;

	if (xml->getAttr("captureScoringAssistThreshold", m_captureScoringAssistThreshold))
	{
		CryLog("CGameRulesCombiCaptureObjective::Init, captureScoringAssistThreshold set to %f", m_captureScoringAssistThreshold);
		hasCaptureAssistScoring = true;
	}

	if (hasCaptureAssistScoring)
	{
		if (xml->getAttr("captureScoringAssistFrac", m_captureScoringAssistFrac))
		{
			CryLog("CGameRulesCombiCaptureObjective::Init, captureScoringAssistFrac set to %f", m_captureScoringAssistFrac);
		}
		else
		{
			CryLog("CGameRulesCombiCaptureObjective::Init, no captureScoringAssistFrac setting present, setting to default of 1.f");
			m_captureScoringAssistFrac = 1.f;
		}
	}
	else
	{
		CryLog("CGameRulesCombiCaptureObjective::Init, no captureScoringAssistThreshold setting present, so setting it equal to captureScoringThreshold, and setting captureScoringAssistFrac to default of 1.f");
		m_captureScoringAssistThreshold = m_captureScoringThreshold;
		m_captureScoringAssistFrac = 1.f;
	}

	if (xml->getAttr("doMidThresholdPartialCaptureScoring", m_doMidThresholdPartialCaptureScoring))
	{
		CryLog("CGameRulesCombiCaptureObjective::Init, doMidThresholdPartialCaptureScoring set to %d", m_doMidThresholdPartialCaptureScoring);
	}	

	if (xml->getAttr("lastMinuteSkillAssessmentThreshold", m_lastMinuteSkillAssessmentThreshold))
	{
		CryLog("CGameRulesCombiCaptureObjective::Init, m_lastMinuteSkillAssessmentThreshold set to %f", m_lastMinuteSkillAssessmentThreshold);
	}

	int numChildren = xml->getChildCount();
	for (int childIdx = 0; childIdx < numChildren; ++ childIdx)
	{
		const XmlNodeRef&  xmlChild = xml->getChild(childIdx);
		const char*  tagName = xmlChild->getTag(); 
		if (!stricmp(tagName, "Icons"))
		{
			CRY_ASSERT_MESSAGE(!m_useIcons, "CombiCaptureObjective xml contains more than one 'Icons' node, we only support one");
			m_useIcons = true;

			xmlChild->getAttr("priority", m_iconPriority);

			m_ourCapturePoint    = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("ourCapturePoint"));
			m_theirCapturePoint  = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("theirCapturePoint"));
			m_usCapturingPoint   = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("usCapturingPoint"));
			m_themCapturingPoint = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("themCapturingPoint"));

			m_shouldShowIconFunc.Format("%s", xmlChild->getAttr("checkFunc"));

			CryLog("CGameRulesCombiCaptureObjective::Init, using on-screen icons [%i %i %i %i]", m_ourCapturePoint, m_theirCapturePoint, m_usCapturingPoint, m_themCapturingPoint);

			g_pGame->GetGameRules()->RegisterTeamChangedListener(this);
		}
		else if(strcmp(tagName,"Audio") == 0)
		{
			if(xmlChild->haveAttr("capturedLoop"))
			{
				m_captureSignalId = g_pGame->GetGameAudio()->GetSignalID(xmlChild->getAttr("capturedLoop"));
			}

			if(xmlChild->haveAttr("interrupt"))
			{
				m_interruptSignalId = g_pGame->GetGameAudio()->GetSignalID(xmlChild->getAttr("interrupt"));
			}

			if(xmlChild->haveAttr("inactiveLoop"))
			{
				m_inactiveSignalId = g_pGame->GetGameAudio()->GetSignalID(xmlChild->getAttr("inactiveLoop"));
			}

			if( xmlChild->haveAttr("alarmLoop"))
			{
				m_alarmSignalId = g_pGame->GetGameAudio()->GetSignalID(xmlChild->getAttr("alarmLoop"));
			}
		}
		else
		{
			GameWarning("Unrecognised/unsupported node '%s' in XML.", tagName);
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::UpdateIcon(SHoldEntityDetails * pDetails, bool force)
{
	GET_COMBI_CAPTURE_ENTITY;

	EGameRulesMissionObjectives requestedIcon = GetIcon(pDetails);

	if (requestedIcon != pCaptureEntity->m_currentIcon || pCaptureEntity->m_needIconUpdate || force)
	{
		if (requestedIcon != -1)
		{
			CCCPOINT(CombiCaptureObj_SetNewIcon);

			SHUDEventWrapper::OnNewObjective(pDetails->m_id, requestedIcon, 0.0f, m_iconPriority);

			CRY_TODO(09, 10, 2009, "Move to happen only once when CaptureEntities are created because the hud doesn't exist at that point currently");
			SHUDEvent hudevent(eHUDEvent_AddEntity);
			hudevent.AddData(SHUDEventData((int)pDetails->m_id));
			CHUDEventDispatcher::CallEvent(hudevent);
		}
		else
		{
			SHUDEventWrapper::OnRemoveObjective(pDetails->m_id, m_iconPriority);
		}
		pCaptureEntity->m_currentIcon = requestedIcon;
		pCaptureEntity->m_needIconUpdate = false;
	}
}

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::Update( float frameTime )
{
	BaseType::Update(frameTime);

	const bool  updatedCombiProgressLastFrame = m_updatedCombiProgressThisFrame;
	m_updatedCombiProgressThisFrame = false;

	EntityId  localClientId = g_pGame->GetIGameFramework()->GetClientActorId();
	CGameRules*  pGameRules = g_pGame->GetGameRules();

	m_clientTeamId = pGameRules->GetTeam(localClientId);

	IGameRulesRoundsModule*  pRoundsModule = pGameRules->GetRoundsModule();
	CRY_ASSERT(pRoundsModule);
	if (!pRoundsModule)
		return;

	m_attackingTeamId = pRoundsModule->GetPrimaryTeam();
	CRY_ASSERT(m_attackingTeamId > 0);
	if (gEnv->IsClient())
	{
		if (m_prevAttackingTeamId != m_attackingTeamId)
		{
			SHUDEvent event (eHUDEvent_SetAttackingTeam);
			event.AddData(m_attackingTeamId);
			CHUDEventDispatcher::CallEvent(event);

			m_prevAttackingTeamId = m_attackingTeamId;
		}
	}

#if CRY_WATCH_ENABLED
	if (g_pGameCVars->g_CombiCaptureObjective_watchLvl > 0)
	{
		CryWatch("[CGameRulesCombiCaptureObjective::Update()]");
		CryWatch(" Round num: %d (of %d)", pRoundsModule->GetRoundNumber(), (pRoundsModule->GetRoundNumber() + pRoundsModule->GetRoundsRemaining()));

		if(g_pGameCVars->g_CombiCaptureObjective_watchLvl > 2)
		{
			CryWatch("m_attackingTeamId %d", m_attackingTeamId);
			CryWatch("m_clientTeamId %d", m_clientTeamId);
		}

		CryWatch("combiProgressBanked = %f (not banked = %f)", m_combiProgressBanked, m_combiProgress);

		if (g_pGameCVars->g_CombiCaptureObjective_watchLvl > 1)
		{
			CryWatch(" Capture ents:");
		}
	}

	if (g_pGameCVars->g_CombiCaptureObjective_watchTerminalSignalPlayers > 0)
	{
		CryWatch("Assault Terminal signal players:");
		for (int i=0; i<HOLD_OBJECTIVE_MAX_ENTITIES; i++)
		{
			SHoldEntityDetails *pDetails = &m_entities[i];
			if (pDetails->m_id)
			{
				GET_COMBI_CAPTURE_ENTITY;
				IEntity*  pEnt = gEnv->pEntitySystem->GetEntity(pDetails->m_id);

				const bool  isPlaying = pDetails->m_signalPlayer.IsPlaying(pDetails->m_id);
				const TAudioSignalID  signalId = pDetails->m_signalPlayer.GetSignalID();
#ifndef _RELEASE
				const char*  signalName = pDetails->m_signalPlayer.GetSignalName();
#else
				const char*  signalName = "[release_build]";
#endif

				CryWatch("  %d '%s' eid %d] isPlaying=%s signal='%s'(%d)", i, (pEnt?pEnt->GetName():"!"), pDetails->m_id, (isPlaying?"YES":"NO"), signalName, signalId);
			}
		}
	}
#endif

	UpdateCaptureVO();

	bool bGameHasStarted = pGameRules->HasGameActuallyStarted();

	int currActiveIndex = -1;
	for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; i++)
	{
		SHoldEntityDetails *pDetails = &m_entities[i];
		if (pDetails->m_id)
		{
			GET_COMBI_CAPTURE_ENTITY;
			++ currActiveIndex;

#if CRY_WATCH_ENABLED
			if (g_pGameCVars->g_CombiCaptureObjective_watchLvl > 1)
			{
				IEntity *pEnt = gEnv->pEntitySystem->GetEntity(pDetails->m_id);
				CryWatch("  %d: '%s' eid %d enabled %d controlledBy %d", i, (pEnt?pEnt->GetName():"!"), pDetails->m_id, pCaptureEntity->m_enabled, pDetails->m_controllingTeamId);
			}
#endif

			if (bGameHasStarted)
			{
				if (pCaptureEntity->m_enabled && ((pDetails->m_controllingTeamId == m_attackingTeamId) || (!m_contestable && (pDetails->m_controllingTeamId == CONTESTED_TEAM_ID))))
				{
					UpdateCaptureProgress(pDetails, frameTime);

					if (!pCaptureEntity->m_capturing)
					{
						ClSiteStartCapturing(pDetails);
						pCaptureEntity->m_capturing = true;
					}
				}
				else
				{
					pCaptureEntity->m_capturing = false;
				}
			}

			// Do HUD stuff
			if (gEnv->IsClient())
			{
				ClUpdateSiteHUD(pDetails, currActiveIndex);
			}

			if (gEnv->bServer)
			{
				SvUpdateCaptureScorers();
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::UpdateCaptureVO()
{
	CRY_ASSERT(m_currentVO >= eCCVO_Initial && m_currentVO <= eCCVO_Size);

	if(m_currentVO < eCCVO_Size)
	{
		if(m_combiVOData[m_currentVO].m_progress < m_combiProgressBanked)
		{
			CRY_ASSERT(m_combiVOData[m_currentVO].m_announcement);

			CAnnouncer::GetInstance()->AnnounceFromTeamId(m_attackingTeamId, m_combiVOData[m_currentVO].m_announcement, CAnnouncer::eAC_inGame);
			m_currentVO++;

			CRY_ASSERT(m_currentVO >= eCCVO_Initial && m_currentVO <= eCCVO_Size);
		}
	}
}

//------------------------------------------------------------------------
// NOTE this now overrides the Base function completely
void CGameRulesCombiCaptureObjective::OnControllingTeamChanged(SHoldEntityDetails *pDetails, const int oldControllingTeam)
{
	GET_COMBI_CAPTURE_ENTITY;

	if (pCaptureEntity && pCaptureEntity->m_enabled)
	{
		if ((oldControllingTeam == m_attackingTeamId) && (pDetails->m_controllingTeamId != m_attackingTeamId))  // attackers were capturing, but not any more
		{
			m_combiProgress = m_combiProgressBanked;  // wipe out any progress not banked during the interrupted capturing interval

			if (gEnv->IsClient())
			{
				CAudioSignalPlayer::JustPlay(m_interruptSignalId, pDetails->m_id);

				if (!pDetails->m_signalPlayer.IsPlaying(pDetails->m_id) || (pDetails->m_signalPlayer.GetSignalID() != m_inactiveSignalId))
				{
					/*if (pDetails->m_signalPlayer.IsPlaying(pDetails->m_id))
					{
						pDetails->m_signalPlayer.Stop(pDetails->m_id);
					}*/

					pDetails->m_signalPlayer.SetSignal(m_inactiveSignalId);
					//pDetails->m_signalPlayer.Play(pDetails->m_id);
				}

				/*if( pCaptureEntity->m_alarmSignalPlayer.IsPlaying( pDetails->m_id ) )
				{
					pCaptureEntity->m_alarmSignalPlayer.Stop( pDetails->m_id );
				}*/
			}
		}

		if (gEnv->IsClient())
		{
			if (pDetails->m_controllingTeamId == m_attackingTeamId)  // started capturing
			{
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(pDetails->m_id);
				if(pEntity)
				{
					const char* pName = pEntity->GetName();
					string announcementName("Terminal");
					announcementName.append(pName);
					CAnnouncer::GetInstance()->AnnounceFromTeamId(m_attackingTeamId, announcementName.c_str(), CAnnouncer::eAC_inGame);
				}

				/*if( !pCaptureEntity->m_alarmSignalPlayer.IsPlaying( pDetails->m_id ) )
				{
					pCaptureEntity->m_alarmSignalPlayer.Play( pDetails->m_id );
				}*/
			}
		}

		if (pDetails->m_localPlayerIsWithinRange)
		{
			if (pDetails->m_controllingTeamId == CONTESTED_TEAM_ID)
			{
				CHUDEventDispatcher::CallEvent(SHUDEvent(eHUDEvent_OnClientInContestedSite));
			}
			else
			{
				CHUDEventDispatcher::CallEvent(SHUDEvent(eHUDEvent_OnClientInOwnedSite));
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::ClUpdateSiteHUD(SHoldEntityDetails *pDetails, const int currActiveIndex)
{
	GET_COMBI_CAPTURE_ENTITY;

	UpdateIcon(pDetails, false);

	if (pCaptureEntity->m_enabled && (pDetails->m_controllingTeamId == m_attackingTeamId))
	{
		SHUDEvent siteIsBeingCaptured;
		siteIsBeingCaptured.eventType = eHUDEvent_OnSiteBeingCaptured;
		siteIsBeingCaptured.eventIntData = currActiveIndex;
		siteIsBeingCaptured.eventIntData2 = (int) pDetails->m_id;
		siteIsBeingCaptured.eventIntData3 = (pDetails->m_localPlayerIsWithinRange ? 1 : 0);
		siteIsBeingCaptured.eventFloatData = m_combiProgressBanked;
		siteIsBeingCaptured.eventFloat2Data = m_combiProgressBanked;
		CHUDEventDispatcher::CallEvent(siteIsBeingCaptured);

		assert(m_attackingTeamId > 0 && m_attackingTeamId <= NUM_TEAMS);

		CGameRules* pGameRules = g_pGame->GetGameRules();
		CBattlechatter *pBattleChatter = pGameRules->GetBattlechatter();
		const int insideEntityCount = pDetails->m_insideEntities[m_attackingTeamId - 1].size();
		for(int i = 0; i < insideEntityCount; i++)
		{
			pBattleChatter->PlayerIsDownloadingDataFromTerminal(pDetails->m_insideEntities[m_attackingTeamId - 1].at(i));
		}

		if(pDetails->m_localPlayerIsWithinRange && m_combiProgressBanked >= 1.0f)
		{
			if(pGameRules->IsTimeLimited())
			{
				const float timeRemaining = pGameRules->GetRemainingGameTime();
				if(timeRemaining < 5.0f)
				{
					g_pGame->GetPersistantStats()->IncrementClientStats(EIPS_FinalIntel5SecRemaining);
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::SvUpdateCaptureScorers()
{
	CRY_ASSERT(gEnv->bServer);

	CGameRules*  pGameRules = g_pGame->GetGameRules();

	IGameRulesStateModule*  pStateModule = pGameRules->GetStateModule();
	if (pStateModule != NULL && (pStateModule->GetGameState() != IGameRulesStateModule::EGRS_InGame))
		return;

	IGameRulesRoundsModule*  pRoundsModule = pGameRules->GetRoundsModule();
	if (pRoundsModule != NULL && !pRoundsModule->IsInProgress())
		return;

	if (IGameRulesScoringModule* pScoringModule=pGameRules->GetScoringModule())
	{
		const EGRST  type = EGRST_CombiCapObj_Capturing_PerSec;
		const float  pointsPerSec = (float) pScoringModule->GetPlayerPointsByType(type);

		if (pointsPerSec >= 1.f)
		{
			const int  curFrame = gEnv->pRenderer->GetFrameID();
			const float  curTime = pGameRules->GetCurrentGameTime();

			for (int i=0; i<HOLD_OBJECTIVE_MAX_ENTITIES; i++)
			{
				SHoldEntityDetails*  pDetails = &m_entities[i];
				if (pDetails->m_id)
				{
					GET_COMBI_CAPTURE_ENTITY;

					if (pCaptureEntity && pCaptureEntity->m_enabled)
					{
						CRY_ASSERT_MESSAGE(m_attackingTeamId-1 >= 0 && m_attackingTeamId-1 < NUM_TEAMS, "SvUpdateCaptureScorers() attackingTeamId out of range");

						const int  count = pDetails->m_insideEntities[m_attackingTeamId - 1].size();
						for(int k=0; k<count; k++)
						{
							const EntityId  eid = pDetails->m_insideEntities[m_attackingTeamId - 1].at(k);
							if (IEntity* pEnt=gEnv->pEntitySystem->GetEntity(eid))
							{
								if (SSvCaptureScorer* pScorer=m_svCaptureScorers.FindByEntityId(eid))
								{
									pScorer->m_frame = curFrame;
								}
								else
								{
#ifndef _RELEASE
									const int  sizeBefore = m_svCaptureScorers.size();
#endif
									SSvCaptureScorer  newScorer;
									newScorer.Set(eid, curFrame, curTime, (k == 0));
									m_svCaptureScorers.push_back(newScorer);
#ifndef _RELEASE
									const int  sizeAfter = m_svCaptureScorers.size();
									CRY_ASSERT(sizeAfter == (sizeBefore + 1));
#endif
								}
							}
						}
					}
				}
			}

			int  n = m_svCaptureScorers.size();
			for (int i=0; i<n; i++)
			{
				SSvCaptureScorer*  pScorer = &m_svCaptureScorers[i];

				if (pScorer->m_frame == curFrame)
				{
					const float  sinceLastAdd = (curTime - pScorer->m_lastScoreBucketAddTime);
					if (sinceLastAdd >= 1.f)
					{
						const float  pointsFrac = (pScorer->m_primary ? 1.f : m_captureScoringAssistFrac);
						pScorer->m_scoreBucket += (pointsPerSec * pointsFrac);
						pScorer->m_lastScoreBucketAddTime = curTime;

						if (pScorer->m_scoreBucket >= 1.f)
						{
							const float  scoringThreshold = (pScorer->m_primary ? m_captureScoringThreshold : m_captureScoringAssistThreshold);
							if (pScorer->m_scoreBucket >= scoringThreshold)
							{
								const TGameRulesScoreInt  score = (TGameRulesScoreInt) floorf( pScorer->m_scoreBucket );
								SGameRulesScoreInfo  scoreInfo (type, score);
								pScoringModule->OnPlayerScoringEventWithInfo(pScorer->m_eid, &scoreInfo);

								pScorer->m_scoreBucket = 0.f;
							}
						}
					}
				}
				else
				{
					if (m_doMidThresholdPartialCaptureScoring && (pScorer->m_scoreBucket >= 1.f))
					{
						const TGameRulesScoreInt  score = (TGameRulesScoreInt) floorf( pScorer->m_scoreBucket );
						SGameRulesScoreInfo  scoreInfo (type, score);
						pScoringModule->OnPlayerScoringEventWithInfo(pScorer->m_eid, &scoreInfo);
					}
					m_svCaptureScorers.removeAt(i);
					i--;
					n--;
					continue;
				}
			}
		}
	}
}

//------------------------------------------------------------------------
// note: this gets called before every round (including the first) for the server, but only before the first for the client
void CGameRulesCombiCaptureObjective::OnStartGame()
{
	BaseType::OnStartGame();

	for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; ++ i)
	{
		SHoldEntityDetails *pDetails = &m_entities[i];
		if (pDetails->m_id)
		{
			GET_COMBI_CAPTURE_ENTITY;
			pCaptureEntity->m_needIconUpdate = true;
		}
	}

	m_svCaptureScorers.clear();
}

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::OnInsideStateChanged(SHoldEntityDetails *pDetails)
{
	GET_COMBI_CAPTURE_ENTITY;
	pCaptureEntity->m_needIconUpdate = true;
}

//------------------------------------------------------------------------
bool CGameRulesCombiCaptureObjective::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	BaseType::NetSerialize(ser, aspect, profile, flags);
	if (aspect == COMBICAPTURE_OBJECTIVE_STATE_ASPECT)
	{
		float  progressBanked = m_combiProgressBanked;

		ser.Value("combiProgressBanked", progressBanked, 'unit');

		if (ser.IsReading())
		{
			if (m_combiProgressBanked != progressBanked)
			{
				m_combiProgressBanked = progressBanked;
				m_bUpdatedBankedProgressThisFrame = true;

				SHUDEvent progressUpdate(eHUDEvent_OnOverallCaptureProgressUpdate);
				progressUpdate.AddData(m_combiProgressBanked);
				CHUDEventDispatcher::CallEvent(progressUpdate);
			}
		}
	}
	
	return true;
}

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::OnNewHoldEntity(SHoldEntityDetails *pDetails, int index)
{
	CRY_ASSERT(index < HOLD_OBJECTIVE_MAX_ENTITIES);

	SCaptureEntity *pCaptureEntity = &m_additionalInfo[index];

	pDetails->m_pAdditionalData = pCaptureEntity;

	m_numActiveEntities ++;
	if (gEnv->IsClient())
	{
		SHUDEvent activeChanged;
		activeChanged.eventType = eHUDEvent_OnCaptureObjectiveNumChanged;
		activeChanged.AddData(SHUDEventData(m_numActiveEntities));
		activeChanged.AddData(SHUDEventData(m_numActiveEntities - 1));
		activeChanged.AddData(SHUDEventData(index));
		activeChanged.AddData(SHUDEventData((int)pDetails->m_id));
		CHUDEventDispatcher::CallEvent(activeChanged);

		CRY_ASSERT(pDetails->m_id);

		CRY_ASSERT(!pDetails->m_signalPlayer.IsPlaying(pDetails->m_id));
		pDetails->m_signalPlayer.SetSignal(m_inactiveSignalId);
		//pDetails->m_signalPlayer.Play(pDetails->m_id);
	}

	pCaptureEntity->Associate(pDetails, this);

	RefreshCaptureEntEnabledState(pDetails, GetNumDesiredEnabledCaptureEnts());

	OnInsideStateChanged(pDetails);
}

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::OnRemoveHoldEntity(SHoldEntityDetails *pDetails)
{
	GET_COMBI_CAPTURE_ENTITY;

	pCaptureEntity->Reset(pDetails);

	m_numActiveEntities --;

	if (gEnv->IsClient())
	{
		SHUDEvent activeChanged;
		activeChanged.eventType = eHUDEvent_OnCaptureObjectiveNumChanged;
		activeChanged.AddData(SHUDEventData(m_numActiveEntities));
		activeChanged.AddData(SHUDEventData(m_numActiveEntities + 1));
		CHUDEventDispatcher::CallEvent(activeChanged);

		SHUDEventWrapper::OnRemoveObjective(pDetails->m_id, m_iconPriority);

		CRY_ASSERT(pDetails->m_id);
		// stop any remaining loop sound from playing before cleaning up the entity (usually at end of round)
		/*if (pDetails->m_signalPlayer.IsPlaying(pDetails->m_id))
		{
			pDetails->m_signalPlayer.Stop(pDetails->m_id); 
		}*/
		REINST("needs verification!");
	}
}

//------------------------------------------------------------------------
bool CGameRulesCombiCaptureObjective::IsComplete( int teamId )
{
	CRY_ASSERT(gEnv->bServer);
	CRY_ASSERT(teamId > 0 && teamId <= 2);

	IGameRulesRoundsModule* pRoundsModule = g_pGame->GetGameRules()->GetRoundsModule();
	if (pRoundsModule != NULL && !pRoundsModule->IsInProgress())
		return false;

	CGameRules*  pGameRules = g_pGame->GetGameRules();

	bool  isComplete = false;

	if (teamId != m_attackingTeamId)
	{
		isComplete = true;
		if (!AllTeamPlayersDead(m_attackingTeamId))  // TODO? this could perhaps be improved by hooking into ClPlayerStatsNetSerializeReadDeath and keeping track of how many attacking team-members are alive there instead
		{
			isComplete = false;
		}
		if (isComplete)
		{
			CCCPOINT(CombiCaptureObj_SvDefendersWonRound_KilledAll);
			if (IGameRulesScoringModule* pScoringModule=pGameRules->GetScoringModule())
			{
				pScoringModule->OnTeamScoringEvent(teamId, EGRST_CaptureObjectivesDefended);
				pScoringModule->SetAttackingTeamLost();
			}
		}
	}
	else
	{
		if (m_combiProgressBanked >= 1.f)
		{
			CCCPOINT(CombiCaptureObj_SvAttackersWonRound);
			if (IGameRulesScoringModule* pScoringModule=pGameRules->GetScoringModule())
			{
				pScoringModule->OnTeamScoringEvent(m_attackingTeamId, EGRST_CaptureObjectiveTaken);
			}
			isComplete = true;
		}
	}
	if (isComplete)
	{
		SvDoEndOfRoundPlayerScoring(teamId);
	}
	return isComplete;
}

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::SvDoEndOfRoundPlayerScoring(const int winningTeam)
{
	CryLog("CGameRulesCombiCaptureObjective::SvDoEndOfRoundPlayerScoring()");

	CRY_ASSERT(gEnv->bServer);

	CRY_ASSERT((winningTeam == 1) || (winningTeam == 2));

	CGameRules *pGameRules = g_pGame->GetGameRules();

	IGameRulesStateModule *pStateModule = pGameRules->GetStateModule();
	CRY_ASSERT(pStateModule && (pStateModule->GetGameState() != IGameRulesStateModule::EGRS_PostGame));

	if (IGameRulesScoringModule* pScoringModule=pGameRules->GetScoringModule())
	{
		const int  defendingTeamId = (3 - m_attackingTeamId);

		// EGRST_CombiCapObj_Win
		{
			EGRST  type = EGRST_CombiCapObj_Win;
			const TGameRulesScoreInt  score = pScoringModule->GetPlayerPointsByType(type);
			if (score > 0)
			{
				SGameRulesScoreInfo  scoreInfo (type, score);
				pScoringModule->OnPlayerScoringEventToAllTeamWithInfo(winningTeam, &scoreInfo);
			}
		}

		if (winningTeam == m_attackingTeamId)
		{
			// EGRST_CombiCapObj_Def_Lost_TimeRemainBonus_Max
			{
				EGRST  type = EGRST_CombiCapObj_Def_Lost_TimeRemainBonus_Max;
				const float  maxScore = pScoringModule->GetPlayerPointsByType(type);
				if (maxScore >= 1.f)
				{
					const float  maxTime = (g_pGameCVars->g_timelimit * 60.f);  // [tlh] this could also be set in xml if designers want it
					const float  curTime = pGameRules->GetCurrentGameTime();

					if ((curTime >= m_defWin_timeRemainBonus_minTime) && (curTime <= maxTime))
					{
						const float  timeRange = (maxTime - m_defWin_timeRemainBonus_minTime);

						if (timeRange > 0.f)
						{
							const float  ratio = std::max(0.f, std::min(1.f, (1.f - ((curTime - m_defWin_timeRemainBonus_minTime) / timeRange)) ));

							if (ratio > 0.f)
							{
								const float  pointsRange = (maxScore - m_defWin_timeRemainBonus_minPoints);

								const TGameRulesScoreInt  score = (TGameRulesScoreInt) floorf( m_defWin_timeRemainBonus_minPoints + (pointsRange * ratio) );

								SGameRulesScoreInfo  scoreInfo (type, score);
								pScoringModule->OnPlayerScoringEventToAllTeamWithInfo(defendingTeamId, &scoreInfo);
							}
						}
					}
				}
			}
		}
		else
		{
			CRY_ASSERT(winningTeam == defendingTeamId);

			// EGRST_CombiCapObj_Def_Win_IntelRemainBonus_Max
			{
				EGRST  type = EGRST_CombiCapObj_Def_Win_IntelRemainBonus_Max;
				const float  maxScore = pScoringModule->GetPlayerPointsByType(type);
				if (maxScore >= 1.f)
				{
					const float  ratio = std::max(0.f, std::min(1.f, (1.f - m_combiProgressBanked) ));
					const TGameRulesScoreInt  score = (TGameRulesScoreInt) floorf( maxScore * ratio );
					SGameRulesScoreInfo  scoreInfo (type, score);
					pScoringModule->OnPlayerScoringEventToAllTeamWithInfo(defendingTeamId, &scoreInfo);
				}
			}

			// EGRST_CombiCapObj_Att_Lost_IntelDownloadedBonus_Max
			{
				EGRST  type = EGRST_CombiCapObj_Att_Lost_IntelDownloadedBonus_Max;
				const float  maxScore = pScoringModule->GetPlayerPointsByType(type);
				if (maxScore >= 1.f)
				{
					const float  ratio = std::max(0.f, std::min(1.f, (m_combiProgressBanked) ));
					const TGameRulesScoreInt  score = (TGameRulesScoreInt) floorf( maxScore * ratio );
					SGameRulesScoreInfo  scoreInfo (type, score);
					pScoringModule->OnPlayerScoringEventToAllTeamWithInfo(m_attackingTeamId, &scoreInfo);
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::UpdateCaptureProgress(SHoldEntityDetails *pDetails, float frameTime)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	IGameRulesRoundsModule* pRoundsModule = pGameRules->GetRoundsModule();
	if (pRoundsModule != NULL && !pRoundsModule->IsInProgress())
		return;

	GET_COMBI_CAPTURE_ENTITY;
	CRY_ASSERT(pCaptureEntity->m_enabled);

	const float  timeToCapture = m_goalCombiCaptureTime;
	// [tlh] if we ever wanted per-entity capture multipliers, this is where they'd go (basically fudge timeToCapture)

	float multiplier = (1.f / timeToCapture);

	if (m_allowMultiPlayerCaptures)
	{
		int insideCount = pDetails->m_insideCount[m_attackingTeamId - 1];
		CRY_ASSERT(insideCount > 0);
		multiplier *= insideCount;
	}

	float progress = (multiplier * frameTime);

	m_combiProgress += progress;
	m_updatedCombiProgressThisFrame = true;

	if (gEnv->bServer && ((m_combiProgress - m_combiProgressBanked) >= m_progressBankingThreshold || m_combiProgress > 1.f))
	{
		if (pGameRules->GetGameMode() == eGM_Assault)
		{
			IGameRulesVictoryConditionsModule *pVictory = pGameRules->GetVictoryConditionsModule();
			if(pVictory)
			{
				//I know this is slow.
				int nNewScore = 10 * (int)((m_combiProgress * 10.f) + 0.5f);
				int nOldScore = 10 * (int)((m_combiProgressBanked * 10.f) + 0.5f);

				if(m_attackingTeamId == 1)
				{
					pVictory->AddIntsToDrawResolutionData(ESVC_DrawResolution_level_1, nNewScore - nOldScore, 0);
				}
				else
				{
					pVictory->AddIntsToDrawResolutionData(ESVC_DrawResolution_level_1, 0, nNewScore - nOldScore);
				}				
			}
		}

		m_bUpdatedBankedProgressThisFrame = true;
		const float fRemainder = fmodf(m_combiProgress, m_progressBankingThreshold);
		m_combiProgressBanked = (m_combiProgress - fRemainder);
		CHANGED_NETWORK_STATE(g_pGame->GetGameRules(), COMBICAPTURE_OBJECTIVE_STATE_ASPECT);
	}

	if (pGameRules->GetGameMode() == eGM_Assault)
	{
		if (gEnv->IsClient())
		{
			UpdateCaptureAudio(pDetails);
		}

		if (m_clientTeamId == m_attackingTeamId)
		{
			for (int i=0; i<HOLD_OBJECTIVE_MAX_ENTITIES; i++)
			{
				SHoldEntityDetails* pEntDetails = &m_entities[i];
				if (pEntDetails->m_id && pEntDetails->m_localPlayerIsWithinRange)
				{
					CPersistantStats::GetInstance()->IncrementClientStats(EFPS_IntelCollectedTime, frameTime);
					break;
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::ClSiteStartCapturing(SHoldEntityDetails *pDetails)
{
	CCCPOINT(CombiCaptureObj_ClSiteCaptureStarted);

#ifndef _RELEASE
	GET_COMBI_CAPTURE_ENTITY;
	CRY_ASSERT(pCaptureEntity && pCaptureEntity->m_enabled);
#endif

	//Feedback
	CGameRules *pGameRules = g_pGame->GetGameRules();
	CRY_ASSERT(m_attackingTeamId > 0);
	int localTeam = pGameRules->GetTeam(g_pGame->GetIGameFramework()->GetClientActorId());
	if (localTeam == m_attackingTeamId)
	{
		CCCPOINT(CombiCaptureObj_SiteStartCapturingByFriendlyTeam);
		
		const int insideEntityCount = pDetails->m_insideEntities[m_attackingTeamId - 1].size();
		for(int i = 0; i < insideEntityCount; i++)
		{
			BATTLECHATTER(BC_AssaultDownload, pDetails->m_insideEntities[m_attackingTeamId - 1].at(i));
		}
	}
	else
	{
		CCCPOINT(CombiCaptureObj_SiteStartCapturingByEnemyTeam);
	}
}

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::OnChangedTeam( EntityId entityId, int oldTeamId, int newTeamId )
{
	BaseType::OnChangedTeam(entityId, oldTeamId, newTeamId);
	if ((g_pGame->GetIGameFramework()->GetClientActorId() == entityId) && newTeamId)
	{
		// Local player has changed teams, reset icons
		int currActiveIndex = -1;
		for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; ++ i)
		{
			SHoldEntityDetails *pDetails = &m_entities[i];
			if (pDetails->m_id)
			{
				GET_COMBI_CAPTURE_ENTITY;
				pCaptureEntity->m_needIconUpdate = true;
			}
		}
		SetLoadoutPackageGroup(newTeamId, false);
	}
}

//------------------------------------------------------------------------
// IGameRulesRoundsListener
// NOTE this doesn't get called before first round, but should be called before any subsequent rounds start.  should be the same behaviour on clients and server
void CGameRulesCombiCaptureObjective::OnRoundStart()
{
	CryLog("CGameRulesCombiCaptureObjective::OnRoundStart()");

	m_combiProgress = 0.f;
	m_combiProgressBanked = 0.f;

	if (gEnv->bServer)
	{
		CHANGED_NETWORK_STATE(g_pGame->GetGameRules(), COMBICAPTURE_OBJECTIVE_STATE_ASPECT);
	}

	CGameRules *pGameRules = g_pGame->GetGameRules();
	IActorIteratorPtr pIter = gEnv->pGameFramework->GetIActorSystem()->CreateActorIterator();
	while (CActor* pActor = (CActor*)pIter->Next())
	{
		EntityId actorId = pActor->GetEntityId();
		int teamId = pGameRules->GetTeam(actorId);
		if(teamId == m_attackingTeamId)
		{
			ADD_VISUAL_BATTLECHATTER(actorId, BC_VisualAssaultAttacker);
		}
		else
		{
			REMOVE_VISUAL_BATTLECHATTER(actorId);
		}
	}

	m_currentVO = eCCVO_Initial;

	m_bBetweenRounds = false;
}

//------------------------------------------------------------------------
// IGameRulesRoundsListener
void CGameRulesCombiCaptureObjective::OnRoundEnd()
{
	if (gEnv->IsClient())
	{
		for (int i=0; i<HOLD_OBJECTIVE_MAX_ENTITIES; i++)
		{
			SHoldEntityDetails*  pDetails = &m_entities[i];
			if (pDetails->m_id)
			{
				GET_COMBI_CAPTURE_ENTITY;
				CRY_ASSERT(gEnv->pEntitySystem->GetEntity(pDetails->m_id));
				/*if (pCaptureEntity->m_alarmSignalPlayer.IsPlaying(pDetails->m_id))
				{
					CryLog("CGameRulesCombiCaptureObjective::OnRoundEnd: calling m_alarmSignalPlayer.Stop(%d) for cap ent 0x%p", pDetails->m_id, pCaptureEntity);
					pCaptureEntity->m_alarmSignalPlayer.Stop(pDetails->m_id);
				}*/
			}
		}

		CGameRules *pGameRules = g_pGame->GetGameRules();
		int teamId = pGameRules->GetTeam(g_pGame->GetIGameFramework()->GetClientActorId());
		SetLoadoutPackageGroup(teamId, true);
	}

	m_highestNumDesiredCapEntsThisRound = MIN_DESIRED_CAP_ENTS;
	CryLog("CGameRulesCombiCaptureObjective::OnRoundEnd: RESET m_highestNumDesiredCapEntsThisRound to %d", m_highestNumDesiredCapEntsThisRound);

	m_bBetweenRounds = true;
}

//------------------------------------------------------------------------
EGameRulesMissionObjectives CGameRulesCombiCaptureObjective::GetIcon(SHoldEntityDetails *pDetails)
{
	GET_COMBI_CAPTURE_ENTITY_RET(EGRMO_Unknown);

	EGameRulesMissionObjectives requestedIcon = pCaptureEntity->m_currentIcon;	// Default to current icon

	if (pCaptureEntity->m_enabled)
	{
		bool iconAllowed = true;

		if (!m_shouldShowIconFunc.empty())
		{
			IEntity *pEntity = gEnv->pEntitySystem->GetEntity(pDetails->m_id);
			if (pEntity)
			{
				IScriptTable *pEntityScript = pEntity->GetScriptTable();
				HSCRIPTFUNCTION iconCheckFunc;
				if (pEntityScript != NULL && pEntityScript->GetValue(m_shouldShowIconFunc.c_str(), iconCheckFunc))
				{
					IScriptSystem *pScriptSystem = gEnv->pScriptSystem;
					bool result = false;
					if (Script::CallReturn(pScriptSystem, iconCheckFunc, pEntityScript, result))
					{
						if (!result)
						{
							requestedIcon = EGRMO_Unknown;
							iconAllowed = false;
						}
					}
				}
			}
		}

		if (iconAllowed)
		{
			CGameRules *pGameRules = g_pGame->GetGameRules();
			int localTeamId = pGameRules->GetTeam(g_pGame->GetIGameFramework()->GetClientActorId());
			float serverTime = pGameRules->GetServerTime();

			if (pCaptureEntity->m_capturing)
			{
				requestedIcon = (localTeamId == m_attackingTeamId) ? m_usCapturingPoint : m_themCapturingPoint;
			}
			else
			{
				requestedIcon = (localTeamId == m_attackingTeamId) ? m_theirCapturePoint : m_ourCapturePoint;
			}
		}
	}
	else
	{
		requestedIcon = EGRMO_Unknown;
	}

	return requestedIcon;
}

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::OnOwnClientEnteredGame()
{
	BaseType::OnOwnClientEnteredGame();
	for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; ++ i)
	{
		SHoldEntityDetails *pDetails = &m_entities[i];
		if (pDetails->m_id)
		{
			GET_COMBI_CAPTURE_ENTITY;
			pCaptureEntity->m_needIconUpdate = true;
		}
	}

	CGameRules *pGameRules = g_pGame->GetGameRules();
	int teamId = pGameRules->GetTeam(g_pGame->GetIGameFramework()->GetClientActorId());
	SetLoadoutPackageGroup(teamId, m_bBetweenRounds);
}

//------------------------------------------------------------------------
// SGameRulesListener (interface)
void CGameRulesCombiCaptureObjective::SvOnTimeLimitExpired()
{
	CryLog("[tlh] @ CGameRulesCombiCaptureObjective::SvOnTimeLimitExpired()");
	CRY_ASSERT(gEnv->bServer);

	CGameRules*  pGameRules = g_pGame->GetGameRules();

	CRY_ASSERT(m_attackingTeamId > 0);
	int  defendingTeamId = (3 - m_attackingTeamId);

	if (IGameRulesScoringModule* pScoringModule=pGameRules->GetScoringModule())
	{
		pScoringModule->OnTeamScoringEvent(defendingTeamId, EGRST_CaptureObjectivesDefended);
		pScoringModule->SetAttackingTeamLost();
		SvDoEndOfRoundPlayerScoring(defendingTeamId);
	}
}

//------------------------------------------------------------------------
// SGameRulesListener (interface)
void CGameRulesCombiCaptureObjective::ClTeamScoreFeedback(int teamId, int prevScore, int newScore)
{
	CRY_ASSERT(gEnv->IsClient());

	if ((teamId == m_attackingTeamId) && (m_combiProgressBanked >= (1.f - (m_progressBankingThreshold * 0.9f))))
	{
		// play the "Hacking 100%" announcement here, which /should/ override the generic "we've won/lost" announcement triggered in the standard Scoring Event processing after this listener is called (because it shouldn't play multiple announcements at same time)
		CAnnouncer::GetInstance()->AnnounceFromTeamId(m_attackingTeamId, "Hacking100", CAnnouncer::eAC_inGame);  // for server's client (non-dedicated)
	}
}

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::OnClientConnect( int channelId, bool isReset, EntityId playerId )
{
	BaseType::OnClientConnect(channelId, isReset, playerId);
	// Need to mark aspect as changes so that the network collects the latest capture progress amounts
	CHANGED_NETWORK_STATE(g_pGame->GetGameRules(), COMBICAPTURE_OBJECTIVE_STATE_ASPECT);
}

//------------------------------------------------------------------------
// IGameRulesKillListener
void CGameRulesCombiCaptureObjective::OnEntityKilled( const HitInfo &hitInfo )
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	IGameRulesScoringModule* pScoringModule=pGameRules->GetScoringModule();
	EntityId localClientId = g_pGame->GetIGameFramework()->GetClientActorId();

	// Test done before clearing the killed player from inside entities
	if (pGameRules->GetGameMode() == eGM_Assault)
	{
		if (hitInfo.shooterId == localClientId)
		{
			int shooterTeam = pGameRules->GetTeam(hitInfo.shooterId);

			if (shooterTeam != m_attackingTeamId)
			{
				if (m_combiProgress >= m_lastMinuteSkillAssessmentThreshold && m_combiProgress < 1.0f)
				{
					bool isComplete=true;
					IGameRulesPlayerStatsModule*  pPlayStatsMo = pGameRules->GetPlayerStatsModule();
					CRY_ASSERT(pPlayStatsMo);
					IGameRulesSpawningModule*  pSpawnMo = pGameRules->GetSpawningModule();
					CRY_ASSERT(pSpawnMo);
					const int  spawnNumLives = pSpawnMo->GetNumLives();
					CRY_ASSERT_MESSAGE(g_pGameCVars->g_timelimitextratime <= 0.f, "Sudden Death / Extra Time not supported by this module yet. (The for-loop below will have to change for that.)");
					int  numStats = pPlayStatsMo->GetNumPlayerStats();
					for (int i=0; i<numStats; i++)
					{
						const SGameRulesPlayerStat*  s = pPlayStatsMo->GetNthPlayerStats(i);
						if (s->flags & SGameRulesPlayerStat::PLYSTATFL_HASSPAWNEDTHISROUND)
						{
							if (pGameRules->GetTeam(s->playerId) == m_attackingTeamId)
							{
								// requires the deaths to have already been updated at this point
								// TODO - if fails we can be out by one in this check
								if (s->deathsThisRound < spawnNumLives && s->playerId != hitInfo.targetId) 
								{
									isComplete = false;
									break;
								}
							}
						}
					}

					if (isComplete)
					{
						int targetTeamId = g_pGame->GetGameRules()->GetTeam(hitInfo.targetId);
						if (targetTeamId == m_attackingTeamId)
						{
							g_pGame->GetPersistantStats()->IncrementClientStats(EIPS_AssaultKillLastAttack5pc);
						}
					}
				}
			}
		}
	}

	// will remove target from any insideEntities 
	BaseType::OnEntityKilled(hitInfo);

	if (gEnv->bServer)
	{
		if (pScoringModule)
		{
			const int  defKillPoints = pScoringModule->GetPlayerPointsByType(EGRST_CombiCapObj_Def_PlayerKill);
			const int  attKillPoints = pScoringModule->GetPlayerPointsByType(EGRST_CombiCapObj_Att_PlayerKill);
			const bool  hasDefKillScoring = (defKillPoints != 0);
			const bool  hasAttKillScoring = (attKillPoints != 0);

			if (hasDefKillScoring || hasAttKillScoring)
			{
				IActor*  pIShooter = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(hitInfo.shooterId);
				IActor*  pITarget = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(hitInfo.targetId);

				if ((pIShooter != NULL && pIShooter->IsPlayer()) && (pITarget != NULL && pITarget->IsPlayer()))
				{
					CPlayer*  pShooter = (CPlayer*) pIShooter;
					CPlayer*  pTarget = (CPlayer*) pITarget;

					const int  shooterTeam = g_pGame->GetGameRules()->GetTeam(hitInfo.shooterId);
					const int  targetTeam = g_pGame->GetGameRules()->GetTeam(hitInfo.targetId);

					if ((shooterTeam == 1 || shooterTeam == 2) && (targetTeam == 1 || targetTeam == 2))
					{
						if (!pShooter->IsFriendlyEntity(hitInfo.targetId))
						{
							if (shooterTeam == m_attackingTeamId)
							{
								if (hasAttKillScoring)
								{
									SGameRulesScoreInfo  scoreInfo (EGRST_CombiCapObj_Att_PlayerKill, attKillPoints);
									scoreInfo.AttachVictim(hitInfo.targetId);
									pScoringModule->OnPlayerScoringEventWithInfo(hitInfo.shooterId, &scoreInfo);
								}
							}
							else
							{
								if (hasDefKillScoring)
								{
									SGameRulesScoreInfo  scoreInfo (EGRST_CombiCapObj_Def_PlayerKill, defKillPoints);
									scoreInfo.AttachVictim(hitInfo.targetId);
									pScoringModule->OnPlayerScoringEventWithInfo(hitInfo.shooterId, &scoreInfo);
								}
							}
						}
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::DetermineControllingTeamId(SHoldEntityDetails *pDetails, const int team1Count, const int team2Count)
{
	if (!m_contestable && ((team1Count > 0) && (team2Count > 0)))
	{
		CRY_ASSERT((m_attackingTeamId == 1) || (m_attackingTeamId == 2)) ;

		CryLog("CGameRulesCombiCaptureObjective::DetermineControllingTeamId: both teams inside entity but it's not contestable, so setting controller to be attacking team (%d)", m_attackingTeamId);
		pDetails->m_controllingTeamId = m_attackingTeamId;

		CCCPOINT(HoldObjective_BothTeamsNowInProximity);
	}
	else
	{
		BaseType::DetermineControllingTeamId(pDetails, team1Count, team2Count);
	}
}

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::UpdateCaptureAudio(SHoldEntityDetails *pDetails)
{
	CRY_ASSERT(gEnv->IsClient());

	if (!pDetails->m_signalPlayer.IsPlaying(pDetails->m_id) || (pDetails->m_signalPlayer.GetSignalID() != m_captureSignalId))
	{
		/*if (pDetails->m_signalPlayer.IsPlaying(pDetails->m_id))
		{
			pDetails->m_signalPlayer.Stop(pDetails->m_id);
		}*/

		pDetails->m_signalPlayer.SetSignal(m_captureSignalId);
		//pDetails->m_signalPlayer.Play(pDetails->m_id);
	}

	const float  prog = ((pDetails->m_controllingTeamId == m_attackingTeamId) ? m_combiProgressBanked : 0.f);
	//CryLogAlways("[tlh] UpdateCaptureAudio: contr = %d, attck = %d, prog = %f", pDetails->m_controllingTeamId, m_attackingTeamId, prog);

	pDetails->m_signalPlayer.SetParam(pDetails->m_id, "hacking", prog);
}

//------------------------------------------------------------------------
// NOTE this now overrides the Base function completely
bool CGameRulesCombiCaptureObjective::IsPlayerEntityUsingObjective(EntityId playerId)
{
	const int  playerTeamId = g_pGame->GetGameRules()->GetTeam(playerId);

	if (playerTeamId == m_attackingTeamId)
	{
		const SHoldEntityDetails*  pDetails = NULL;
		int  i = 0;

		for (i=0; i<HOLD_OBJECTIVE_MAX_ENTITIES; i++)
		{
			pDetails = &m_entities[i];

			if (pDetails->m_id)
			{
				if (stl::find(pDetails->m_insideEntities[0], playerId))
				{
					break;
				}
				else if (stl::find(pDetails->m_insideEntities[1], playerId))
				{
					break;
				}
			}
		}

		if ((i < HOLD_OBJECTIVE_MAX_ENTITIES) && pDetails)
		{
			GET_COMBI_CAPTURE_ENTITY_RET(false);
			return (pCaptureEntity->m_enabled);
		}
	}

	return false;
}

//------------------------------------------------------------------------
bool CGameRulesCombiCaptureObjective::AllTeamPlayersDead(const int teamId)
{
	bool  allDead = true;

	CGameRules*  pGameRules = g_pGame->GetGameRules();
	IGameRulesPlayerStatsModule*  pPlayStatsMo = pGameRules->GetPlayerStatsModule();
	CRY_ASSERT(pPlayStatsMo);
	IGameRulesSpawningModule*  pSpawnMo = pGameRules->GetSpawningModule();
	CRY_ASSERT(pSpawnMo);
	const int  spawnNumLives = pSpawnMo->GetNumLives();
	CRY_ASSERT_MESSAGE(g_pGameCVars->g_timelimitextratime <= 0.f, "Sudden Death / Extra Time not supported by this module yet. (The for-loop below will have to change for that.)");

	int  numAttackersSpawned = 0;

	int  numStats = pPlayStatsMo->GetNumPlayerStats();
	for (int i=0; i<numStats; i++)
	{
		const SGameRulesPlayerStat*  s = pPlayStatsMo->GetNthPlayerStats(i);
		if (s->flags & SGameRulesPlayerStat::PLYSTATFL_HASSPAWNEDTHISROUND)
		{
			if (pGameRules->GetTeam(s->playerId) == teamId)
			{
				numAttackersSpawned++;
				if (s->deathsThisRound < spawnNumLives)
				{
					allDead = false;
					break;
				}
			}
		}
	}

	if (numAttackersSpawned == 0)
	{
		allDead = false;
	}

	return allDead;
}

//------------------------------------------------------------------------
// IGameRulesPlayerStatsListener
void CGameRulesCombiCaptureObjective::ClPlayerStatsNetSerializeReadDeath(const SGameRulesPlayerStat* s, uint16 prevDeathsThisRound, uint8 prevFlags)
{
	CryLog("[tlh] @ CGameRulesCombiCaptureObjective::ClPlayerStatsNetSerializeReadDeath(s=%p, prevDeathsThisRound=%u, prevFlags=%u)", s, prevDeathsThisRound, prevFlags);
	CRY_ASSERT(gEnv->IsClient());
	if ((m_clientTeamId == m_attackingTeamId) && (m_clientTeamId > 0))
	{
		if (CGameRules* pGameRules=g_pGame->GetGameRules())
		{
			const int  targetTeamId = pGameRules->GetTeam(s->playerId);
			if (targetTeamId != m_clientTeamId)
			{
				if (AllTeamPlayersDead(targetTeamId))
				{
					SHUDEventWrapper::RoundMessageNotify("@ui_msg_as_all_defenders_eliminated", SHUDEventWrapper::SMsgAudio(1, CAnnouncer::GetInstance()->NameToID("DefendersAllDead")));
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
// IGameRulesRevivedListener
void CGameRulesCombiCaptureObjective::EntityRevived(EntityId entityId)
{
#ifndef _RELEASE
	IEntity*  pEntity = gEnv->pEntitySystem->GetEntity(entityId);
	CryLog("CGameRulesCombiCaptureObjective::EntityRevived(entityId=[\"%s\"])", (pEntity?pEntity->GetName():"NULL"));
#endif

	RefreshAllCaptureEntsEnabledStates();
}

//------------------------------------------------------------------------
int CGameRulesCombiCaptureObjective::GetNumDesiredEnabledCaptureEnts()
{
	if (CGameRules* pGameRules=g_pGame->GetGameRules())
	{
		const int  flagsNeeded = (SGameRulesPlayerStat::PLYSTATFL_HASSPAWNEDTHISROUND | SGameRulesPlayerStat::PLYSTATFL_HASHADROUNDRESTART);  // will test for /either/ of these - ie. either a player has already spawned and is in the game, or hasn't spawned yet but either had spanwed last round or was spectating last round so we know they /will/ spawn eventually
		const int  t1count = pGameRules->GetTeamPlayerCountWithStatFlags(1, flagsNeeded, false);
		const int  t2count = pGameRules->GetTeamPlayerCountWithStatFlags(2, flagsNeeded, false);

		const int  biggestTeam = std::max(t1count, t2count);

		const int  idealNumDesired = (biggestTeam + AMOUNT_OF_DESIRED_CAP_ENTS_MORE_THAN_PLAYERS);

		m_highestNumDesiredCapEntsThisRound = CLAMP(idealNumDesired, m_highestNumDesiredCapEntsThisRound, MAX_DESIRED_CAP_ENTS);  // ie. the num desired is not allowed to decrease during a round
		CryLog("CGameRulesCombiCaptureObjective::GetNumDesiredEnabledCaptureEnts: UPDATED m_highestNumDesiredCapEntsThisRound to %d", m_highestNumDesiredCapEntsThisRound);
	}

	return m_highestNumDesiredCapEntsThisRound;
}

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::RefreshCaptureEntEnabledState(SHoldEntityDetails* pDetails, const int numDesired)
{
	CryLog("CGameRulesCombiCaptureObjective::RefreshCaptureEntEnabledState(pDetails=%p, numDesired=%d)", pDetails, numDesired);

	if (pDetails->m_id)
	{
		if (IEntity* pEnt=gEnv->pEntitySystem->GetEntity(pDetails->m_id))
		{
			bool  enable = true;

			const char*  pEntName = pEnt->GetName();

			if (pEntName != NULL && (strlen(pEntName) == 1))
			{
				const int  alphaIdx = (int) (pEntName[0] - 'A');

				enable = (alphaIdx < numDesired);

				SHUDEvent  refreshEnabledState;
				refreshEnabledState.eventType = eHUDEvent_OnCaptureObjectiveRefreshEnabledState;
				refreshEnabledState.AddData(SHUDEventData(alphaIdx));
				refreshEnabledState.AddData(SHUDEventData(enable));
				CHUDEventDispatcher::CallEvent(refreshEnabledState);
			}

			GET_COMBI_CAPTURE_ENTITY;
			CryLog("  ... setting CaptureEntity '%s' enabled to %s", pEntName, (enable?"TRUE":"FALSE"));
			pCaptureEntity->SetEnabled(enable, true, pDetails);
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::RefreshAllCaptureEntsEnabledStates()
{
	const int numDesired = GetNumDesiredEnabledCaptureEnts();

	for (int i=0; i<HOLD_OBJECTIVE_MAX_ENTITIES; i++)
	{
		SHoldEntityDetails*  pDetails = &m_entities[i];
		RefreshCaptureEntEnabledState(pDetails, numDesired);
	}
}

//------------------------------------------------------------------------
void CGameRulesCombiCaptureObjective::SetLoadoutPackageGroup( int teamId, bool bOnRoundEnd )
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	IGameRulesRoundsModule *pRoundsModule = pGameRules->GetRoundsModule();
	CEquipmentLoadout *pEquipmentLoadout = g_pGame->GetEquipmentLoadout();
	if (pRoundsModule != NULL && pEquipmentLoadout != NULL)
	{
		int primaryTeam = pRoundsModule->GetPrimaryTeam();
		if ((!bOnRoundEnd && (teamId == primaryTeam)) || (bOnRoundEnd && (teamId != primaryTeam)))
		{
			pEquipmentLoadout->SetPackageGroup(LOADOUT_PACKAGE_GROUP_ATTACKERS);
		}
		else
		{
			pEquipmentLoadout->SetPackageGroup(LOADOUT_PACKAGE_GROUP_DEFENDERS);
		}
	}
}

#undef COMBICAPTURE_OBJECTIVE_STATE_ASPECT
#undef GET_COMBI_CAPTURE_ENTITY
#undef GET_COMBI_CAPTURE_ENTITY_RET
#undef LOADOUT_PACKAGE_GROUP_ATTACKERS
#undef LOADOUT_PACKAGE_GROUP_DEFENDERS
