// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Implementation of a king of the hill objective (take and hold)

	-------------------------------------------------------------------------
	History:
	- 15:02:2010  : Created by Colin Gulliver

*************************************************************************/

#include "StdAfx.h"
#include "GameRulesKingOfTheHillObjective.h"
#include <CrySystem/XML/IXml.h>
#include "GameRules.h"
#include "Player.h"
#include "GameRulesModules/IGameRulesScoringModule.h"
#include "Utility/CryWatch.h"
#include <CryString/StringUtils.h>
#include "UI/HUD/HUDEventDispatcher.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "UI/HUD/HUDUtils.h"
#include "GameRulesModules/IGameRulesSpawningModule.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "Utility/CryWatch.h"
#include "Battlechatter.h"
#include "PersistantStats.h"
#include "Audio/Announcer.h"

#if NUM_ASPECTS > 8
	#define KING_OF_THE_HILL_OBJECTIVE_STATE_ASPECT		eEA_GameServerA
#else
	#define KING_OF_THE_HILL_OBJECTIVE_STATE_ASPECT		eEA_GameServerStatic
#endif

//------------------------------------------------------------------------
CGameRulesKingOfTheHillObjective::CGameRulesKingOfTheHillObjective()
{
	m_scoreTimerMaxLength = 0.f;
	m_scoreTimerAdditionalPlayerMultiplier = 0.f;
	m_pulseTimerLength = 1.f;
	m_useIcons = false;
	m_neutralIcon = EGRMO_Unknown;
	m_friendlyIcon = EGRMO_Unknown;
	m_hostileIcon = EGRMO_Unknown;
	m_contestedIcon = EGRMO_Unknown;

	m_captureSignalId = INVALID_AUDIOSIGNAL_ID;

	for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; ++ i)
	{
		m_additionalInfo[i].Reset();
	}
}

//------------------------------------------------------------------------
CGameRulesKingOfTheHillObjective::~CGameRulesKingOfTheHillObjective()
{
}

//------------------------------------------------------------------------
void CGameRulesKingOfTheHillObjective::Init( XmlNodeRef xml )
{
	BaseType::Init(xml);

	if (xml->getAttr("scoreTime", m_scoreTimerMaxLength))
	{
		CryLog("CGameRulesKingOfTheHillObjective::Init, using score timer, length=%f", m_scoreTimerMaxLength);
	}
	xml->getAttr("additionalPlayerTimerMultiplier", m_scoreTimerAdditionalPlayerMultiplier);
	if (m_scoreTimerAdditionalPlayerMultiplier == 0.f)
	{
		m_scoreTimerAdditionalPlayerMultiplier = 1.f;
	}
	CryLog("CGameRulesKingOfTheHillObjective::Init, multiplier for additional players=%f", m_scoreTimerAdditionalPlayerMultiplier);

	int numChildren = xml->getChildCount();
	for (int childIdx = 0; childIdx < numChildren; ++ childIdx)
	{
		XmlNodeRef xmlChild = xml->getChild(childIdx);
		const char *pTag = xmlChild->getTag();
		if (!stricmp(pTag, "Icons"))
		{
			CRY_ASSERT_MESSAGE(!m_useIcons, "KingOfTheHillObjective xml contains more than one 'Icons' node, we only support one");
			m_useIcons = true;

			m_neutralIcon   = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("neutral"));
			m_friendlyIcon  = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("friendly"));
			m_hostileIcon   = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("hostile"));
			m_contestedIcon = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("contested"));

			m_shouldShowIconFunc.Format("%s", xmlChild->getAttr("checkFunc"));

			CryLog("CGameRulesKingOfTheHillObjective::Init, using on-screen icons [%i %i %i %i]", m_neutralIcon, m_friendlyIcon, m_hostileIcon, m_contestedIcon);
		}
		else if(strcmp(pTag,"Audio") == 0)
		{
			if(xmlChild->haveAttr("capturedLoop"))
			{
				m_captureSignalId = g_pGame->GetGameAudio()->GetSignalID(xmlChild->getAttr("capturedLoop"));
			}
		}
		else if (!stricmp(pTag, "Strings"))
		{
			const char *pString = 0;
			if (xmlChild->getAttr("friendlyCapture", &pString))
			{
				m_friendlyCaptureString.Format("@%s", pString);
			}
			if (xmlChild->getAttr("enemyCapture", &pString))
			{
				m_enemyCaptureString.Format("@%s", pString);
			}
			if (xmlChild->getAttr("friendlyLost", &pString))
			{
				m_friendlyLostString.Format("@%s", pString);
			}
			if (xmlChild->getAttr("enemyLost", &pString))
			{
				m_enemyLostString.Format("@%s", pString);
			}
			if (xmlChild->getAttr("newEntity", &pString))
			{
				m_newEntityString.Format("@%s", pString);
			}
			if (xmlChild->getAttr("gameStateNeutral", &pString))
			{
				m_gameStateNeutralString.Format("@%s", pString);
			}
			if (xmlChild->getAttr("gameStateFriendly", &pString))
			{
				m_gameStateFriendlyString.Format("@%s", pString);
			}
			if (xmlChild->getAttr("gameStateEnemy", &pString))
			{
				m_gameStateEnemyString.Format("@%s", pString);
			}
			if (xmlChild->getAttr("gameStateDestructing", &pString))
			{
				m_gameStateDestructingString.Format("@%s", pString);
			}
			if (xmlChild->getAttr("gameStateIncoming", &pString))
			{
				m_gameStateIncomingString.Format("@%s", pString);
			}
			if (xmlChild->getAttr("iconTextDefend", &pString))
			{
				m_iconTextDefend.Format("@%s", pString);
				m_iconTextDefend = CHUDUtils::LocalizeString(m_iconTextDefend.c_str());
			}
			if (xmlChild->getAttr("iconTextClear", &pString))
			{
				m_iconTextClear.Format("@%s", pString);
				m_iconTextClear = CHUDUtils::LocalizeString(m_iconTextClear.c_str());
			}
			if (xmlChild->getAttr("iconTextCapture", &pString))
			{
				m_iconTextCapture.Format("@%s", pString);
				m_iconTextCapture = CHUDUtils::LocalizeString(m_iconTextCapture.c_str());
			}
		}
		else if (!stricmp(pTag, "RadiusPulse"))
		{
			xmlChild->getAttr("time", m_pulseTimerLength);
			m_shouldDoPulseEffectFunc.Format("%s", xmlChild->getAttr("checkFunc"));
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesKingOfTheHillObjective::UpdateIcon(SHoldEntityDetails * pDetails)
{
	SKotHEntity *pKotHEntity = static_cast<SKotHEntity *>(pDetails->m_pAdditionalData);
	CRY_ASSERT(pKotHEntity);

	const char *pName = NULL;
	const char *pColour = NULL;
	EGameRulesMissionObjectives requestedIcon = GetIcon(pDetails, &pName, &pColour);

	if (requestedIcon != EGRMO_Unknown)
	{
		CCCPOINT(KingOfTheHillObjective_SetNewIcon);

		SHUDEventWrapper::OnNewObjective(pDetails->m_id, requestedIcon, 0.f, 0, pName, pColour);

		pKotHEntity->m_needsIconUpdate = false;

		if(!pKotHEntity->m_isOnRadar)
		{
			SHUDEvent hudevent(eHUDEvent_AddEntity);
			hudevent.AddData(SHUDEventData((int)pDetails->m_id));
			CHUDEventDispatcher::CallEvent(hudevent);
			pKotHEntity->m_isOnRadar = true;
		}
	}
	else if (pKotHEntity->m_currentIcon != EGRMO_Unknown)
	{
		CCCPOINT(KingOfTheHillObjective_RemoveIcon);

		SHUDEventWrapper::OnRemoveObjective(pDetails->m_id, 0);
	}

	pKotHEntity->m_currentIcon = requestedIcon;
}

//------------------------------------------------------------------------
void CGameRulesKingOfTheHillObjective::Update( float frameTime )
{
	BaseType::Update(frameTime);

	CGameRules *pGameRules = g_pGame->GetGameRules();
	IGameRulesScoringModule *pScoringModule = pGameRules->GetScoringModule();

	const int localTeamId = pGameRules->GetTeam(g_pGame->GetIGameFramework()->GetClientActorId());

	for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; ++ i)
	{
		SHoldEntityDetails *pDetails = &m_entities[i];
		if (!pDetails->m_id)
		{
			continue;
		}

		SKotHEntity *pKotHEntity = static_cast<SKotHEntity *>(pDetails->m_pAdditionalData);
		CRY_ASSERT(pKotHEntity);

		if (gEnv->bServer && pScoringModule)
		{
#ifndef _RELEASE
			if (g_pGameCVars->g_KingOfTheHillObjective_watchLvl)
			{
				IEntity *pEntity = gEnv->pEntitySystem->GetEntity(pDetails->m_id);
				const char *pEntName = pEntity ? pEntity->GetName() : "<NULL>";
				if (pDetails->m_controllingTeamId == CONTESTED_TEAM_ID)
				{
					CryWatch("KotH entity '%s' is contested", pEntName);
				}
				else if (pDetails->m_controllingTeamId == 0)
				{
					CryWatch("KotH entity '%s' has no players nearby", pEntName);
				}
				else
				{
					CryWatch("KotH entity '%s' controlled by team %i, scoreTimerLength='%.2f', timeSinceLastScore='%.2f'", 
						pEntName, pDetails->m_controllingTeamId, pKotHEntity->m_scoreTimerLength, pKotHEntity->m_timeSinceLastScore);
				}
			}
#endif

			if (pKotHEntity->m_scoringTeamId)
			{
				const int teamIndex = pKotHEntity->m_scoringTeamId - 1;
				CRY_ASSERT_MESSAGE(teamIndex >= 0 && teamIndex < NUM_TEAMS, "Update() scoringTeamId is out of range");

				pKotHEntity->m_timeSinceLastScore += frameTime;

				if (pKotHEntity->m_timeSinceLastScore >= pKotHEntity->m_scoreTimerLength)
				{
					pScoringModule->OnTeamScoringEvent(teamIndex + 1, EGRST_KingOfTheHillObjectiveHeld);
					pKotHEntity->m_timeSinceLastScore = 0.f;
					AwardPlayerPoints(&pDetails->m_insideEntities[teamIndex], EGRST_KingOfTheHillObjectiveHeld);
					CCCPOINT_IF((pKotHEntity->m_scoringTeamId == 1), KingOfTheHillObjective_TeamMarinesScored);
					CCCPOINT_IF((pKotHEntity->m_scoringTeamId == 2), KingOfTheHillObjective_TeamCellScored);
				}
			}
		}
		if (gEnv->IsClient())
		{
			if (m_useIcons && pKotHEntity->m_needsIconUpdate)
			{
				UpdateIcon(pDetails);
			}

			if (pGameRules->GetGameMode() == eGM_CrashSite)
			{
				if (pDetails->m_localPlayerIsWithinRange && pDetails->m_controllingTeamId != CONTESTED_TEAM_ID)
				{
					CPersistantStats::GetInstance()->IncrementClientStats(EFPS_CrashSiteHeldTime, frameTime);
				}
			}

			if (pKotHEntity->m_bPulseEnabled)
			{
				pKotHEntity->m_pulseTime -= frameTime;
				if (pKotHEntity->m_pulseTime < 0.f)
				{
					eRadiusPulseType pulseType = GetPulseType(pDetails);
					const float radiusEffectScale = pKotHEntity->m_radiusEffectScale * pDetails->m_controlRadius;
					RadiusEffectPulse(pDetails->m_id, pulseType, radiusEffectScale);
					pKotHEntity->m_pulseTime = m_pulseTimerLength;
				}
			}
			else
			{
				if (!m_shouldDoPulseEffectFunc.empty())
				{
					IEntity *pEntity = gEnv->pEntitySystem->GetEntity(pDetails->m_id);
					if (pEntity)
					{
						IScriptTable *pEntityScript = pEntity->GetScriptTable();
						HSCRIPTFUNCTION pulseCheckFunc;
						if (pEntityScript != NULL && pEntityScript->GetValue(m_shouldDoPulseEffectFunc.c_str(), pulseCheckFunc))
						{
							IScriptSystem *pScriptSystem = gEnv->pScriptSystem;
							bool result = false;
							if (Script::CallReturn(pScriptSystem, pulseCheckFunc, pEntityScript, result))
							{
								pKotHEntity->m_bPulseEnabled = result;
							}
						}
					}
				}
			}

			const float fOldScoringSFX = pKotHEntity->m_fScoringSFX;

			if(pKotHEntity->m_scoringTeamId)
			{
				pKotHEntity->m_fScoringSFX = min(pKotHEntity->m_fScoringSFX + (frameTime * 2.0f), 1.0f);
			}
			else
			{
				pKotHEntity->m_fScoringSFX = max(pKotHEntity->m_fScoringSFX - (frameTime * 1.0f), 0.0f);
			}

			if(pKotHEntity->m_fScoringSFX != fOldScoringSFX)
				UpdateEntityAudio(pDetails);
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesKingOfTheHillObjective::SvSiteChangedOwner( SHoldEntityDetails *pDetails )
{
	SKotHEntity *pKotHEntity = static_cast<SKotHEntity*>(pDetails->m_pAdditionalData);
	CRY_ASSERT(pKotHEntity);

	// Set the team
	g_pGame->GetGameRules()->SetTeam(std::max(0, pDetails->m_controllingTeamId), pDetails->m_id);
}

//------------------------------------------------------------------------
void CGameRulesKingOfTheHillObjective::ClUpdateHUD( SHoldEntityDetails *pDetails )
{
	SKotHEntity *pKotHEntity = static_cast<SKotHEntity *>(pDetails->m_pAdditionalData);
	CRY_ASSERT(pKotHEntity);

	pKotHEntity->m_needsIconUpdate = true;

	if(pDetails->m_localPlayerIsWithinRange)
	{
		CPlayer* pClientPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetClientActor());
		NET_BATTLECHATTER(BC_EnterCrashsite, pClientPlayer);
	}

	// TODO: Local player inside area
}

//------------------------------------------------------------------------
void CGameRulesKingOfTheHillObjective::OnStartGame()
{
	for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; ++ i)
	{
		if (m_entities[i].m_id)
		{
			SKotHEntity *pKotHEntity = static_cast<SKotHEntity*>(m_entities[i].m_pAdditionalData);
			CRY_ASSERT(pKotHEntity);
			pKotHEntity->m_needsIconUpdate = true;
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesKingOfTheHillObjective::OnInsideStateChanged( SHoldEntityDetails *pDetails )
{
	SKotHEntity *pKotHEntity = static_cast<SKotHEntity*>(pDetails->m_pAdditionalData);
	CRY_ASSERT(pKotHEntity);

	if (pKotHEntity->m_scoringTeamId != pDetails->m_controllingTeamId)
	{
		int oldTeamId = pKotHEntity->m_scoringTeamId;

		pKotHEntity->m_scoringTeamId = std::max(pDetails->m_controllingTeamId, 0);
		pKotHEntity->m_timeSinceLastScore = 0.f;

		if (gEnv->bServer)
		{
			SvSiteChangedOwner(pDetails);
		}
		if (gEnv->IsClient())
		{
			ClSiteChangedOwner(pDetails, oldTeamId);
			UpdateEntityAudio(pDetails);
		}
	}

	if (pKotHEntity->m_scoringTeamId)
	{
		int teamIndex = pKotHEntity->m_scoringTeamId - 1;

		assert(teamIndex >= 0 && teamIndex < NUM_TEAMS);
		pKotHEntity->m_scoreTimerLength = CalculateScoreTimer(pDetails->m_insideCount[teamIndex]);
	}

	if (gEnv->IsClient())
	{
		ClUpdateHUD( pDetails );
	}
	BaseType::OnInsideStateChanged(pDetails);
}

//------------------------------------------------------------------------
void CGameRulesKingOfTheHillObjective::OnNewHoldEntity(SHoldEntityDetails *pDetails, int index)
{
	CRY_ASSERT(index < HOLD_OBJECTIVE_MAX_ENTITIES);

	SKotHEntity *pKotHEntity = &m_additionalInfo[index];
	pKotHEntity->Reset();

	pDetails->m_pAdditionalData = pKotHEntity;

	if (gEnv->bServer)
	{
		g_pGame->GetGameRules()->SetTeam(0, pDetails->m_id);
	}

	if (gEnv->IsClient())
	{
		pKotHEntity->m_needsIconUpdate = true;		// Can't set icon straight away since the lua 'checkFunc' will give an incorrect result
		InitEntityAudio(pDetails);
	}

	OnInsideStateChanged(pDetails);

	// Cache radius pulse effect scale
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(pDetails->m_id);
	if (pEntity)
	{
		SmartScriptTable pScriptTable = pEntity->GetScriptTable();
		if (pScriptTable)
		{
			SmartScriptTable pPropertiesTable;
			if (pScriptTable->GetValue("Properties", pPropertiesTable))
			{
				float fRadiusEffectScale = 1.f;
				if (pPropertiesTable->GetValue("radiusEffectScale", fRadiusEffectScale))
				{
					pKotHEntity->m_radiusEffectScale = fRadiusEffectScale;
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesKingOfTheHillObjective::ClSiteChangedOwner( SHoldEntityDetails *pDetails, int oldTeamId )
{
	// Site has been captured
	CGameRules *pGameRules = g_pGame->GetGameRules();
	EntityId clientActorId = g_pGame->GetIGameFramework()->GetClientActorId();
	int localTeam = pGameRules->GetTeam(clientActorId);

	SKotHEntity *pKotHEntity = static_cast<SKotHEntity*>(pDetails->m_pAdditionalData);
	CRY_ASSERT(pKotHEntity);

	const int ownerTeamId = pDetails->m_controllingTeamId;
	if (ownerTeamId > 0)
	{
		ClSiteChangedOwnerAnnouncement(pDetails, clientActorId, ownerTeamId, localTeam);

		if (localTeam == ownerTeamId)
		{
			CCCPOINT(KingOfTheHillObjective_SiteCapturedByFriendlyTeam);
			if (!m_friendlyCaptureString.empty())
			{
				SHUDEventWrapper::GameStateNotify(m_friendlyCaptureString.c_str());
			}
			if (!m_gameStateFriendlyString.empty())
			{
				const char *localisedMessage = CHUDUtils::LocalizeString(m_gameStateFriendlyString.c_str());
				SHUDEventWrapper::OnGameStatusUpdate(eGBNFLP_Good, localisedMessage);
			}
		}
		else
		{
			CCCPOINT(KingOfTheHillObjective_SiteCapturedByEnemyTeam);
			if (!m_enemyCaptureString.empty())
			{
				SHUDEventWrapper::GameStateNotify(m_enemyCaptureString.c_str());
			}
			if (!m_gameStateEnemyString.empty())
			{
				const char *localisedMessage = CHUDUtils::LocalizeString(m_gameStateEnemyString.c_str());
				SHUDEventWrapper::OnGameStatusUpdate(eGBNFLP_Bad, localisedMessage);
			}
		}
	}
	else if (oldTeamId > 0)
	{
		if (localTeam == oldTeamId)
		{
			CCCPOINT(KingOfTheHillObjective_SiteLostByFriendlyTeam);
			if (!m_friendlyLostString.empty())
			{
				SHUDEventWrapper::GameStateNotify(m_friendlyLostString.c_str());
			}
		}
		else
		{
			CCCPOINT(KingOfTheHillObjective_SiteLostByEnemyTeam);
			if (!m_enemyLostString.empty())
			{
				SHUDEventWrapper::GameStateNotify(m_enemyLostString.c_str());
			}
		}

		if (!m_gameStateNeutralString.empty())
		{
			const char *localisedMessage = CHUDUtils::LocalizeString(m_gameStateNeutralString.c_str());
			SHUDEventWrapper::OnGameStatusUpdate(eGBNFLP_Neutral, localisedMessage);
		}
	}

	int currActiveIndex = -1;
	for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; i ++)
	{
		if( !m_entities[i].m_id)
		{
			continue;
		}
		++currActiveIndex;

		if( &m_entities[i] != pDetails )
		{
			continue;
		}

		CRY_TODO( 23,03,2010, "HUD: OnSiteCaptured events are being sent multiple times from multiple places. /FH");
		SHUDEvent siteIsCaptured(eHUDEvent_OnSiteCaptured);
		siteIsCaptured.eventIntData = currActiveIndex;
		siteIsCaptured.eventIntData2 = ownerTeamId;
		CHUDEventDispatcher::CallEvent(siteIsCaptured);
	}
}

//------------------------------------------------------------------------
void CGameRulesKingOfTheHillObjective::ClSiteChangedOwnerAnnouncement(SHoldEntityDetails *pDetails, EntityId clientActorId, int ownerTeamId, int localTeam)
{
	if(pDetails->m_localPlayerIsWithinRange && ownerTeamId == localTeam)
	{
		CAnnouncer::GetInstance()->Announce(clientActorId, "Captured", CAnnouncer::eAC_inGame);
	}
	else
	{
		CAnnouncer::GetInstance()->AnnounceFromTeamId(ownerTeamId, "Captured", CAnnouncer::eAC_inGame);
	}
}


//------------------------------------------------------------------------
void CGameRulesKingOfTheHillObjective::OnChangedTeam( EntityId entityId, int oldTeamId, int newTeamId )
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
				SKotHEntity *pKotHEntity = static_cast<SKotHEntity *>(pDetails->m_pAdditionalData);
				CRY_ASSERT(pKotHEntity);

				pKotHEntity->m_needsIconUpdate = true;

				++currActiveIndex;
				if (pDetails->m_controllingTeamId == 1 || pDetails->m_controllingTeamId == 2)
				{
					CRY_TODO( 23,03,2010, "HUD: OnSiteCaptured events are being sent multiple times from multiple places. /FH");
					SHUDEvent siteIsCaptured(eHUDEvent_OnSiteCaptured);
					siteIsCaptured.eventIntData = currActiveIndex;
					siteIsCaptured.eventIntData2 = pDetails->m_controllingTeamId;
					CHUDEventDispatcher::CallEvent(siteIsCaptured);
				}

				IEntity *pEntity = gEnv->pEntitySystem->GetEntity(pDetails->m_id);
				if (pEntity)
				{
					IScriptTable *pScript = pEntity->GetScriptTable();
					if (pScript && pScript->GetValueType("LocalPlayerChangedTeam") == svtFunction)
					{
						IScriptSystem *pScriptSystem = gEnv->pScriptSystem;
						pScriptSystem->BeginCall(pScript, "LocalPlayerChangedTeam");
						pScriptSystem->PushFuncParam(pScript);
						pScriptSystem->EndCall();
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
EGameRulesMissionObjectives CGameRulesKingOfTheHillObjective::GetIcon( SHoldEntityDetails *pDetails, const char **ppOutName, const char **ppOutColour )
{
	SKotHEntity *pKotHEntity = static_cast<SKotHEntity*>(pDetails->m_pAdditionalData);
	CRY_ASSERT(pKotHEntity);

	EGameRulesMissionObjectives requestedIcon = EGRMO_Unknown;

	if (!pDetails->m_id)
	{
		requestedIcon = EGRMO_Unknown;
	}
	else
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
			const char *pFriendlyColour = "#9AD5B7";
			const char *pHostileColour = "#AC0000";
			const char *pNeutralColour = "#666666";

			CGameRules *pGameRules = g_pGame->GetGameRules();
			int localTeamId = pGameRules->GetTeam(g_pGame->GetIGameFramework()->GetClientActorId());

			if ( (pDetails->m_controllingTeamId == 0) )
			{
				requestedIcon = m_neutralIcon;
				*ppOutName = m_iconTextCapture.c_str();
				*ppOutColour = pNeutralColour;
			}
			else if( (pDetails->m_controllingTeamId == CONTESTED_TEAM_ID) )
			{
				requestedIcon = m_contestedIcon;
				*ppOutName = m_iconTextClear.c_str();
				*ppOutColour = pHostileColour;
			}
			else if (pDetails->m_controllingTeamId == localTeamId)
			{
				requestedIcon = m_friendlyIcon;
				*ppOutName = m_iconTextDefend.c_str();
				*ppOutColour = pFriendlyColour;
			}
			else
			{
				requestedIcon = m_hostileIcon;
				*ppOutName = m_iconTextClear.c_str();
				*ppOutColour = pHostileColour;
			}
		}
	}

	return requestedIcon;
}

//------------------------------------------------------------------------
void CGameRulesKingOfTheHillObjective::InitEntityAudio( SHoldEntityDetails *pDetails )
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(pDetails->m_id);
	if(pEntity)
	{
		pEntity->GetOrCreateComponent<IEntityAudioComponent>();
		UpdateEntityAudio(pDetails);
	}
}

//------------------------------------------------------------------------
void CGameRulesKingOfTheHillObjective::UpdateEntityAudio( SHoldEntityDetails *pDetails )
{
	SKotHEntity *pKotHEntity = static_cast<SKotHEntity*>(pDetails->m_pAdditionalData);
	CRY_ASSERT(pKotHEntity);

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(pDetails->m_id);
	if(pEntity)
	{
		if (!pDetails->m_signalPlayer.IsPlaying())
		{
			pDetails->m_signalPlayer.SetSignal(m_captureSignalId);
			//pDetails->m_signalPlayer.Play(pDetails->m_id);
		}

		pDetails->m_signalPlayer.SetParam(pDetails->m_id, "mpcapture", pKotHEntity->m_fScoringSFX);
		pDetails->m_signalPlayer.SetParam(pDetails->m_id, "contended", pDetails->m_controllingTeamId == CONTESTED_TEAM_ID ? 1.0f : 0.0f);
	}
}

//------------------------------------------------------------------------
void CGameRulesKingOfTheHillObjective::ClearEntityAudio( SHoldEntityDetails *pDetails )
{
	REINST("needs verification!");
	/*IEntity *pEntity = gEnv->pEntitySystem->GetEntity(pDetails->m_id);
	if(pEntity)
	{
		IEntityAudioComponent *pIEntityAudioComponent = pEntity->GetComponent<IEntityAudioComponent>();
		if(pIEntityAudioComponent)
		{
			pIEntityAudioComponent->StopAllSounds();
		}
	}*/
}

void CGameRulesKingOfTheHillObjective::Announce(const char* announcement, TAnnounceType inType, const bool shouldPlayAudio) const
{
	BaseType::Announce(announcement, inType, shouldPlayAudio);

	switch(inType)
	{
		case k_announceType_CS_Incoming:
			if (!m_gameStateIncomingString.empty())
			{
				const char *localisedMessage = CHUDUtils::LocalizeString(m_gameStateIncomingString.c_str());
				SHUDEventWrapper::OnGameStatusUpdate(eGBNFLP_Neutral, localisedMessage);
			}
			break;
		case k_announceType_CS_Destruct:
			if (!m_gameStateDestructingString.empty())
			{
				const char *localisedMessage = CHUDUtils::LocalizeString(m_gameStateDestructingString.c_str());
				SHUDEventWrapper::OnGameStatusUpdate(eGBNFLP_Neutral, localisedMessage);
			}
			break;
	}
}

//------------------------------------------------------------------------
void CGameRulesKingOfTheHillObjective::OnRemoveHoldEntity( SHoldEntityDetails *pDetails )
{
	ClearEntityAudio(pDetails);

	SHUDEventWrapper::OnRemoveObjective(pDetails->m_id, 0);

	SHUDEvent hudevent(eHUDEvent_RemoveEntity);
	hudevent.AddData(SHUDEventData((int)pDetails->m_id));
	CHUDEventDispatcher::CallEvent(hudevent);
}

//------------------------------------------------------------------------
void CGameRulesKingOfTheHillObjective::OnOwnClientEnteredGame()
{
	BaseType::OnOwnClientEnteredGame();

	for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; ++ i)
	{
		SHoldEntityDetails *pDetails = &m_entities[i];
		if (pDetails->m_id)
		{
			SKotHEntity *pKotHEntity = static_cast<SKotHEntity*>(pDetails->m_pAdditionalData);
			CRY_ASSERT(pKotHEntity);

			pKotHEntity->m_needsIconUpdate = true;
		}
	}
}

//------------------------------------------------------------------------
float CGameRulesKingOfTheHillObjective::CalculateScoreTimer( int playerCount )
{
	float timerLength = m_scoreTimerMaxLength;
	timerLength *= pow(m_scoreTimerAdditionalPlayerMultiplier, playerCount - 1);
	return timerLength;
}

//------------------------------------------------------------------------
void CGameRulesKingOfTheHillObjective::OnTimeTillRandomChangeUpdated( int type, float fPercLiveSpan )
{
	for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; ++ i)
	{
		SHoldEntityDetails *pDetails = &m_entities[i];
		if (pDetails->m_id)
		{
			pDetails->m_signalPlayer.SetParam(pDetails->m_id, "mplifespan", fPercLiveSpan);
		}
	}
}
