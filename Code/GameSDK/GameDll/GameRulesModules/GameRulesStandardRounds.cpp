// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 

	-------------------------------------------------------------------------
	History:
	- 26:10:2009  : Created by Colin Gulliver

*************************************************************************/

#include "StdAfx.h"
#include "GameRulesStandardRounds.h"
#include <CrySystem/XML/IXml.h>
#include "GameRules.h"
#include "GameCVars.h"
#include "GameRulesModules/IGameRulesObjectivesModule.h"
#include "GameRulesModules/IGameRulesSpawningModule.h"
#include "GameRulesModules/IGameRulesPlayerStatsModule.h"
#include "GameRulesModules/IGameRulesPlayerSetupModule.h"
#include "GameRulesModules/IGameRulesScoringModule.h"
#include "GameRulesModules/IGameRulesVictoryConditionsModule.h"
#include "GameRulesModules/IGameRulesStateModule.h"
#include "GameRulesModules/IGameRulesSpectatorModule.h"
#include "GameRulesModules/GameRulesObjective_Predator.h"
#include "UI/UIManager.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "UI/HUD/HUDUtils.h"
#include "Audio/Announcer.h"
#include "EquipmentLoadout.h"
#include "Player.h"
#include "Utility/DesignerWarning.h"
#include "RecordingSystem.h"

#include "Utility/CryWatch.h"

static TKeyValuePair<EGameOverReason,const char*>
g_gameOverReasons[] = {
	{EGOR_Unknown,							"Unknown"},
	{EGOR_TimeLimitReached,			"TimeLimitReached"},
	{EGOR_ObjectivesCompleted,	"ObjectivesCompleted"},
	{EGOR_NoLivesRemaining,			"NoLivesRemaining"},
};

static TKeyValuePair<CGameRulesStandardRounds::ERoundType,const char*>
g_roundTypeNames[] = {
	{CGameRulesStandardRounds::ERT_Any,		"Any"},
	{CGameRulesStandardRounds::ERT_Odd,		"Odd"},
	{CGameRulesStandardRounds::ERT_Even,	"Even"},
};


#if NUM_ASPECTS > 8
	#define STANDARD_ROUNDS_ASPECT				eEA_GameServerC
#else
	#define STANDARD_ROUNDS_ASPECT				eEA_GameServerStatic
#endif

//-------------------------------------------------------------------------
CGameRulesStandardRounds::CGameRulesStandardRounds()
{
	CryLog("CGameRulesStandardRounds::CGameRulesStandardRounds()");

	m_roundNumber = 0;
	m_treatCurrentRoundAsFinalRound = false;
	m_resetScores = false;
	m_allowBestOfVictory = false;
	m_previousRoundWinnerTeamId = 0;
	m_numLocalSpawnsThisRound = 0;
	m_previousRoundWinnerEntityId = 0;
	m_previousRoundWinReason = EGOR_Unknown;
	m_previousRoundTeamScores[0] = 0;
	m_previousRoundTeamScores[1] = 0;
	m_prevServerTime = 0.f;
	m_newRoundStartTime = -1.f;
	m_newRoundShowLoadoutTime = -1.f;
	m_victoryMessageTime = 0.0f;
	m_suddenDeathTime = 0.0f;
	SetRoundState(eGRRS_InProgress);
	m_bShownLoadout = false;
	m_bShowPrimaryTeamBanner = false;
	m_bShowSecondaryTeamBanner = false;

	m_primaryStartRoundStringIsTeamMessage=false;
	m_secondaryStartRoundStringIsTeamMessage=false;

	m_endOfRoundStringCount = 0;
	m_endOfRoundVictoryStringCount = 0;

	m_roundEndHUDState = eREHS_Unknown;
	m_timeSinceRoundEndStateChange = 0.f;
	m_missedLoadout = 0;
	m_primaryTeamOverride = -1; 

	m_bShowKillcamAtEndOfRound = false;

	m_bCustomHeaderPrimary = false;
	m_bCustomHeaderSecondary = false;
	m_bShowRoundStartingMessageEverySpawn = false;
}

//-------------------------------------------------------------------------
CGameRulesStandardRounds::~CGameRulesStandardRounds()
{
	CryLog("CGameRulesStandardRounds::~CGameRulesStandardRounds()");

	if (gEnv->pConsole)
	{
		gEnv->pConsole->RemoveCommand("g_nextRound");
	}

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		pGameRules->UnRegisterTeamChangedListener(this);
	
		IGameRulesStateModule *pStateModule = pGameRules->GetStateModule();
		if (pStateModule)
		{
			pStateModule->RemoveListener(this);
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::Init( XmlNodeRef xml )
{
	CryLog("CGameRulesStandardRounds::Init()");

	int iScratch;

	iScratch = 0;
	if (xml->getAttr("resetScores", iScratch))
	{
		m_resetScores = (iScratch != 0);
	}

	iScratch = 0;
	if (xml->getAttr("allowBestOfVictory", iScratch))
	{
		m_allowBestOfVictory = (iScratch != 0);
	}

	iScratch = 0;
	if (xml->getAttr("suddenDeathTimeSecs", iScratch))
	{
		m_suddenDeathTime = (iScratch * __fres(60.0f));
	}

	// Prevent primary team swapping on round changes
	if (xml->getAttr("primaryTeamOverride", iScratch))
	{
		m_primaryTeamOverride = iScratch;
	}

	if (xml->getAttr("showKillcamAtEndOfRound", iScratch))
	{
		m_bShowKillcamAtEndOfRound = (iScratch != 0);
	}

	if (xml->getAttr("showRoundStartingMessageEverySpawn", iScratch))
	{
		m_bShowRoundStartingMessageEverySpawn = (iScratch != 0);
	}

	int numChildren = xml->getChildCount();
	for (int i = 0; i < numChildren; ++ i)
	{
		XmlNodeRef xmlChild = xml->getChild(i);
		if (!stricmp(xmlChild->getTag(), "PrimaryTeam"))
		{
			ReadEntityClasses(m_primaryTeamEntities, xmlChild, m_primaryStartRoundString, m_primaryStartRoundStringIsTeamMessage, m_primaryStartRoundStringExtra, m_bShowPrimaryTeamBanner, m_primaryCustomHeader, m_bCustomHeaderPrimary);
		}
		else if (!stricmp(xmlChild->getTag(), "SecondaryTeam"))
		{
			ReadEntityClasses(m_secondaryTeamEntities, xmlChild, m_secondaryStartRoundString, m_secondaryStartRoundStringIsTeamMessage, m_secondaryStartRoundStringExtra, m_bShowSecondaryTeamBanner, m_secondaryCustomHeader, m_bCustomHeaderSecondary);
		}
		else if (!stricmp(xmlChild->getTag(), "EndStrings"))
		{
			const char *pString = 0;

			// Defaults
			if (xmlChild->getAttr("winMessage", &pString))
			{
				m_endOfRoundStringsDefault.m_roundWinMessage.Format("@%s", pString);
			}
			if (xmlChild->getAttr("loseMessage", &pString))
			{
				m_endOfRoundStringsDefault.m_roundLoseMessage.Format("@%s", pString);
			}
			if (xmlChild->getAttr("drawMessage", &pString))
			{
				m_endOfRoundStringsDefault.m_roundDrawMessage.Format("@%s", pString);
			}

			// Custom end of round strings
			int numEORChildren = xmlChild->getChildCount();
			for (int j = 0; j < numEORChildren; ++ j)
			{
				XmlNodeRef xmlEORChild = xmlChild->getChild(j);
				if (!stricmp(xmlEORChild->getTag(), "Strings"))
				{
					if (xmlEORChild->getAttr("reason", &pString))
					{
						EGameOverReason reason = KEY_BY_VALUE(pString, g_gameOverReasons);

						if (m_endOfRoundStringCount < MAX_END_OF_ROUND_STRINGS)
						{
							SOnEndRoundStrings &strings = m_endOfRoundStrings[m_endOfRoundStringCount];
							bool found = false;
							for (int k = 0; (k < MAX_END_OF_ROUND_STRINGS) && (k < m_endOfRoundStringCount); ++k )
							{
								if (reason == m_endOfRoundStrings[k].m_reason)
								{
									CryLog("CGameRulesStandardRounds::Init() EndOfRound strings already exist for reason:%s", pString);
									strings = m_endOfRoundStrings[k];
									found = true;
									break;
								}
							}

							if (!found)
							{
								CryLog("CGameRulesStandardRounds::Init() Added EndOfRound strings for reason:%s", pString);
								strings.m_reason = reason;
								m_endOfRoundStringCount++;
							}

							if (xmlEORChild->getAttr("winMessage", &pString))
							{
								if (pString[0] == 0)
								{
									strings.m_roundWinMessage.Format(" ");
								}
								else
								{
									strings.m_roundWinMessage.Format("@%s", pString);
								}
							}
							if (xmlEORChild->getAttr("loseMessage", &pString))
							{
								if (pString[0] == 0)
								{
									strings.m_roundLoseMessage.Format(" ");
								}
								else
								{
									strings.m_roundLoseMessage.Format("@%s", pString);
								}
							}
							if (xmlEORChild->getAttr("drawMessage", &pString))
							{
								if (pString[0] == 0)
								{
									strings.m_roundDrawMessage.Format(" ");
								}
								else
								{
									strings.m_roundDrawMessage.Format("@%s", pString);
								}
							}
						}
						else
						{
							CRY_ASSERT_MESSAGE(0, "CGameRulesStandardRounds::Init() Reached max number of EndOfRound strings. Increase MAX_END_OF_ROUND_STRINGS if more are required.");
						}
					}
				}
				else if(!stricmp(xmlEORChild->getTag(), "VictoryStrings"))
				{
					if (xmlEORChild->getAttr("reason", &pString))
					{
						EGameOverReason reason = KEY_BY_VALUE(pString, g_gameOverReasons);

						if (m_endOfRoundVictoryStringCount < k_MaxEndOfRoundVictoryStrings)
						{
							SOnEndRoundVictoryStrings &strings = m_endOfRoundVictoryStrings[m_endOfRoundVictoryStringCount];

							CryLog("CGameRulesStandardRounds::Init() Added EndOfRound Victory strings for reason:%s", pString);
							strings.m_reason = reason;
							m_endOfRoundVictoryStringCount++;

							if(xmlEORChild->getAttr("round", &pString))
							{
								strings.m_round = KEY_BY_VALUE(pString, g_roundTypeNames);
							}

							if (xmlEORChild->getAttr("winMessage", &pString))
							{
								strings.m_roundWinVictoryMessage.Format("@%s", pString);
							}
							if (xmlEORChild->getAttr("loseMessage", &pString))
							{
								strings.m_roundLoseVictoryMessage.Format("@%s", pString);
							}
						}
						else
						{
							CRY_ASSERT_MESSAGE(0, "CGameRulesStandardRounds::Init() Reached max number of EndOfRound Victory strings. Increase k_MaxEndOfRoundVictoryStrings if more are required.");
						}
					}
				}
			}
		}
		else if (!stricmp(xmlChild->getTag(), "StartStrings"))
		{
			const char *pString = 0;
			if (xmlChild->getAttr("startNewRound", &pString))
			{
				m_primaryStartRoundString.Format("@%s", pString);
				m_secondaryStartRoundString.Format("@%s", pString);
			}
		}
	}
	
	REGISTER_COMMAND("g_nextRound", CmdNextRound, VF_RESTRICTEDMODE, "Starts the next game round.");

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		pGameRules->RegisterTeamChangedListener(this);

		if (pGameRules->GetGameMode() == eGM_CaptureTheFlag)
		{
			DesignerWarning(pGameRules->GetRoundLimit() == 2, "The Game/HUD only supports CaptureTheRelay being a 2 round limit game!!!!");
		}

		IGameRulesStateModule *pStateModule = pGameRules->GetStateModule();
		pStateModule->AddListener(this);
	}

}

//-------------------------------------------------------------------------

void CGameRulesStandardRounds::ReadEntityClasses( TEntityDetailsVec &classesVec, XmlNodeRef xml, TFixedString &startRoundString, bool &startRoundStringIsTeamMessage, TFixedString &startRoundStringExtra, bool &bShowTeamBanner, TFixedString &startRoundHeaderString, bool &bCustomHeader)
{
	int numChildren = xml->getChildCount();
	for (int i = 0; i < numChildren; ++ i)
	{
		XmlNodeRef xmlChild = xml->getChild(i);
		if (!stricmp(xmlChild->getTag(), "Entity"))
		{
			SEntityDetails entityDetails;
			bool valid = false;

			const char *pClassName = 0;
			if (xmlChild->getAttr("class", &pClassName))
			{
				const IEntityClass *pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(pClassName);
				if (pClass)
				{
					entityDetails.m_pEntityClass = pClass;
					valid = true;
				}
			}
			const char *pFuncName = 0;
			if (xmlChild->getAttr("luaActivateFunc", &pFuncName))
			{
				entityDetails.m_activateFunc.Format("%s", pFuncName);
			}
			if (xmlChild->getAttr("luaDeactivateFunc", &pFuncName))
			{
				entityDetails.m_deactivateFunc.Format("%s", pFuncName);
			}

			entityDetails.m_doRandomSelection = false;
			entityDetails.m_selectCount = 1;

			const char *pSelectType = 0;
			if (xmlChild->getAttr("select", &pSelectType))
			{
				if (!stricmp(pSelectType, "Random"))
				{
					entityDetails.m_doRandomSelection = true;
					xmlChild->getAttr("count", entityDetails.m_selectCount);
				}
			}

			if (valid)
			{
				classesVec.push_back(entityDetails);
			}
		}
		else if (!stricmp(xmlChild->getTag(), "StartStrings"))
		{
			const char *pString = 0;
			if (xmlChild->getAttr("startNewRound", &pString))
			{
				startRoundString.Format("@%s", pString);
				startRoundStringIsTeamMessage = false;
			}
			else if (xmlChild->getAttr("startNewRoundTeam", &pString))
			{
				startRoundString.Format("@%s", pString);
				startRoundStringIsTeamMessage = true;
			}
			
			if (xmlChild->getAttr("startNewRoundSmallBanner", &pString))
			{
				startRoundStringExtra.Format("@%s", pString);
			}

			if (xmlChild->getAttr("startNewRoundHeader", &pString))
			{
				bCustomHeader = true;
				startRoundHeaderString.Format("@%s", pString);
			}
			else
			{
				bCustomHeader = false;
			}

			int showTeamBanner;
			if (xmlChild->getAttr("showTeamBanner", showTeamBanner))
			{
				bShowTeamBanner = (showTeamBanner != 0);
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::PostInit()
{
	CryLog("CGameRulesStandardRounds::PostInit()");
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::OnStartGame()
{
	if (gEnv->bServer)
	{
		StartNewRound(true);
	}
}

//-------------------------------------------------------------------------
bool CGameRulesStandardRounds::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	if (aspect == STANDARD_ROUNDS_ASPECT)
	{
		CGameRules *pGameRules = g_pGame->GetGameRules();

#ifndef _RELEASE
		const int  oldRoundNum = m_roundNumber;
#endif

		ser.Value("roundNum", m_roundNumber, 'ui8');
		ser.Value("treatAsFinalRound", m_treatCurrentRoundAsFinalRound, 'bool');
		ser.Value("previousTeamId", m_previousRoundWinnerTeamId, 'team');
		ser.Value("previousEntityId", m_previousRoundWinnerEntityId, 'eid');
		ser.Value("previousTeamScore1", m_previousRoundTeamScores[0], 'ui16');
		ser.Value("previousTeamScore2", m_previousRoundTeamScores[1], 'ui16');

		int winReason = (int)m_previousRoundWinReason;
		ser.Value("previousWinReason", winReason, 'ui8');
		if (ser.IsReading())
		{
			m_previousRoundWinReason = EGameOverReason(winReason);
		}

		if(ser.IsWriting())
		{
			m_victoryMessageTime = GetVictoryMessageTime();
		}
		ser.Value("victoryMessageTime", m_victoryMessageTime, 'fsec');

		int state = int(m_roundState);
		ser.Value("state", state, 'ui3');

		if (ser.IsReading())
		{
#ifndef _RELEASE
		 if (m_roundNumber != oldRoundNum)
		 {
			LOG_PRIMARY_ROUND("LOG_PRIMARY_ROUND: CGameRulesStandardRounds::NetSerialize: serialize read round from %d to %d", oldRoundNum, m_roundNumber);
		 }
#endif

			ERoundState oldState = m_roundState;
			ERoundState newState = ERoundState(state);

			if (m_roundState != newState)
			{
				CRY_ASSERT(!gEnv->bServer);
				CRY_ASSERT(gEnv->IsClient());

				if (newState == eGRRS_Restarting)
				{
					pGameRules->OnRoundEnd_NotifyListeners();  // for clients
					pGameRules->FreezeInput(true);
					ClDisplayEndOfRoundMessage();
					const float serverTime = pGameRules->GetServerTime();
					CalculateNewRoundTimes(serverTime);
					m_bShownLoadout = false;
					OnRoundEnd();
				}
				else if (newState == eGRRS_SuddenDeath)
				{
					pGameRules->OnSuddenDeath_NotifyListeners(); // for clients
					DisplaySuddenDeathMessage();
				}
				else if(newState == eGRRS_Finished)
				{
					if (GetRoundsRemaining() == 0)
					{
						const float serverTime = pGameRules->GetServerTime();
						CalculateEndGameTime(serverTime);
					}

					pGameRules->OnRoundEnd_NotifyListeners();  // for clients
					pGameRules->FreezeInput(true);
					ClDisplayEndOfRoundMessage();
					OnRoundEnd();
				}

				CRY_FIXME(25,2,2010, "This code doesn't happen when the 1st round starts (ie. first going from pregame to ingame) - this should be deprecated and most (if not all) usages should be changed over to the OnRoundStart_NotifyListeners func (called below) instead");
				pGameRules->ClRoundsNetSerializeReadState_NotifyListeners(newState, m_roundState);

				CryLog("CGameRulesStandardRounds::NetSerialize() setting new roundState=%d", newState);
				SetRoundState(newState);

				if (m_roundState == eGRRS_Restarting || m_roundState == eGRRS_Finished)
				{
					g_pGame->GetUI()->ActivateDefaultState();	// must be after setting new state to correctly pick endround
				}
			}

			IGameRulesStateModule*  pStateMo = pGameRules->GetStateModule();

			if (((oldState == eGRRS_Restarting) && (m_roundState == eGRRS_InProgress)) ||
				((m_roundState == eGRRS_InProgress) && (pStateMo && (pStateMo->GetGameState() == IGameRulesStateModule::EGRS_Reset))))
			{
				// note: the second line in the if() above is to catch the case of starting the 1st round (ie. from pregame to ingame)

				if( (oldState == eGRRS_Restarting) && (m_bShownLoadout == false) )
				{
					const float  serverTime = pGameRules->GetServerTime();
					m_missedLoadout ++;

					CryLog("Rounds: Set state to In Progress but shown Loadout was false, current server time %.2f, happened %d times", serverTime, m_missedLoadout );
					
					NewRoundTransition();
				}

				pGameRules->OnRoundStart_NotifyListeners();  // for clients

				if (pGameRules->GetObjectivesModule())
				{
					pGameRules->GetObjectivesModule()->OnGameReset();
				}

				CHUDEventDispatcher::CallEvent(SHUDEvent(eHUDEvent_OnRoundStart));  // for clients
			}

			// RoundStarting Message has to be called after sending hud event on roundstart so that we hear the sounds properly
			if (oldState != newState)
			{
				if (newState == eGRRS_InProgress)
				{
					pGameRules->FreezeInput(false);
					if (m_roundNumber != 0) // game started or prematch ended will take care of the first spawn of the match
					{
						CryLog("CGameRulesStandardRounds::NetSerialize() calling ShowRoundStartingMessage()");
						ShowRoundStartingMessage(false);			// Clients only - of course. Called for every round start apart from the first one, unless we're showing round starts every spawn - this is equivelant to StartNewRound() for the server
					}
				}
				else if (newState == eGRRS_Restarting)
				{
					CryLog("CGameRulesStandardRounds::NetSerialize() going into restarting state, resetting numLocalSpawns to 0");
					m_numLocalSpawnsThisRound=0;	// clear up spawns for this round on clients
				}
			}
		}
	}
	return true;
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::ClDisplayEndOfRoundMessage()
{
	EntityId localActorId = g_pGame->GetIGameFramework()->GetClientActorId();
	CGameRules *pGameRules = g_pGame->GetGameRules();
	int localTeam = pGameRules->GetTeam(localActorId);
	int primaryTeam = GetPrimaryTeam();

	LOG_PRIMARY_ROUND("LOG_PRIMARY_ROUND: ClDisplayEndOfRoundMessage: primary team = %d, local team = %d", primaryTeam, localTeam);

	SOnEndRoundStrings &strings = m_endOfRoundStringsDefault;

	for (int i = 0; i < m_endOfRoundStringCount; ++ i)
	{
		if (m_previousRoundWinReason == m_endOfRoundStrings[i].m_reason)
		{
			strings = m_endOfRoundStrings[i];
			break;
		}
	}

	const char* victoryDescMessage = "";
	int winner = 0;
	EAnnouncementID announcement = INVALID_ANNOUNCEMENT_ID;
	CGameAudio* pGameAudio = g_pGame->GetGameAudio();
	CAnnouncer* pAnnouncer = CAnnouncer::GetInstance();
	bool clientScoreIsTop = false;
	const bool bIndividualScore = pGameRules->IndividualScore();
	const bool bShowRoundsAsDraw = pGameRules->ShowRoundsAsDraw();

	if (!bShowRoundsAsDraw && m_previousRoundWinnerTeamId)
	{
		winner = m_previousRoundWinnerTeamId;

		static CryFixedStringT<16>  strWinTeamName;
		strWinTeamName.Format("@ui_hud_team_%d", m_previousRoundWinnerTeamId);

		if (localTeam == m_previousRoundWinnerTeamId)
		{
			// Local team won
			CryLog("CGameRulesStandardRounds::ClDisplayEndOfRoundMessage(), local team won");

			clientScoreIsTop = true;

			if (!strings.m_roundWinMessage.empty())
			{
				victoryDescMessage = CHUDUtils::LocalizeString(strings.m_roundWinMessage.c_str(), strWinTeamName.c_str(), NULL);
			}
			announcement = pAnnouncer->NameToID("RoundWin");
		}
		else
		{
			// Local team lost
			CryLog("CGameRulesStandardRounds::ClDisplayEndOfRoundMessage(), local team lost");
			
			if (!strings.m_roundLoseMessage.empty())
			{
				victoryDescMessage = CHUDUtils::LocalizeString(strings.m_roundLoseMessage.c_str(), strWinTeamName.c_str(), NULL);
			}
			announcement = pAnnouncer->NameToID("RoundLose");
		}
	}
	else if (!bShowRoundsAsDraw && m_previousRoundWinnerEntityId)
	{
		winner = (int)m_previousRoundWinnerEntityId;

		IEntity *pPlayer = gEnv->pEntitySystem->GetEntity(m_previousRoundWinnerEntityId);
		if (m_previousRoundWinnerEntityId == localActorId)
		{
			// Local player won
			CryLog("CGameRulesStandardRounds::ClDisplayEndOfRoundMessage(), local player won");

			clientScoreIsTop = true;
			
			if (!strings.m_roundWinMessage.empty())
			{
				victoryDescMessage = CHUDUtils::LocalizeString( strings.m_roundWinMessage.c_str(), pPlayer ? pPlayer->GetName() : "" );
			}
			announcement = pAnnouncer->NameToID("RoundWon");
		}
		else
		{
			// Local player lost
			CryLog("CGameRulesStandardRounds::ClDisplayEndOfRoundMessage(), local player lost");
			
			if (!strings.m_roundLoseMessage.empty())
			{
				victoryDescMessage = CHUDUtils::LocalizeString( strings.m_roundLoseMessage.c_str(), pPlayer ? pPlayer->GetName() : "" );
			}
			announcement = pAnnouncer->NameToID("RoundLose");
		}
	}
	else
	{
		// Draw
		CryLog("CGameRulesStandardRounds::ClDisplayEndOfRoundMessage(), draw");

		if (!bIndividualScore && (localTeam > 0))
		{
			clientScoreIsTop = true;
		}
		else
		{
			if (IGameRulesPlayerStatsModule* pPlayStatsMo=g_pGame->GetGameRules()->GetPlayerStatsModule())
			{
				int  clientScore = 0;
				if (const SGameRulesPlayerStat* s=pPlayStatsMo->GetPlayerStats(localActorId))
				{
					clientScore = s->points;
				}

				int  highestOpponentScore = 0;
				const int  numStats = pPlayStatsMo->GetNumPlayerStats();
				for (int i=0; i<numStats; i++)
				{
					const SGameRulesPlayerStat*  s = pPlayStatsMo->GetNthPlayerStats(i);
					if (s != NULL && (s->flags & SGameRulesPlayerStat::PLYSTATFL_HASSPAWNEDTHISROUND))
					{
						if ((s->points >= highestOpponentScore) && (s->playerId != localActorId))
						{
							highestOpponentScore = s->points;
						}
					}
				}

				clientScoreIsTop = (clientScore == highestOpponentScore);
			}
		}
		
		if (!strings.m_roundDrawMessage.empty())
		{
			victoryDescMessage = strings.m_roundDrawMessage.c_str();
		}

		if (pGameRules->GetGameMode() != eGM_Gladiator)
		{
			announcement = pAnnouncer->NameToID("RoundDraw");
		}
		else
		{
			if (localTeam == m_previousRoundWinnerTeamId)
			{
				announcement = pAnnouncer->NameToID((localTeam == 1) ? "Marine_RoundWin" : "Hunter_WinRound");
			}
			else
			{
				announcement = pAnnouncer->NameToID((localTeam == 1) ? "Marine_RoundLose" : "Hunter_LoseRound");
			}
		}
	}
	m_victoryDescMessage.Format("%s", victoryDescMessage);

	const char* victoryMessage = ClGetVictoryMessage(localTeam);
	m_victoryMessage.Format("%s", victoryMessage);

	CAudioSignalPlayer::JustPlay("RoundEndOneShot"); 
	SHUDEventWrapper::OnRoundEnd(winner, clientScoreIsTop, m_victoryDescMessage.c_str(), m_victoryMessage.c_str(), announcement);

	EnterRoundEndHUDState(eREHS_HUDMessage);
}

//-------------------------------------------------------------------------
const char* CGameRulesStandardRounds::ClGetVictoryMessage(const int localTeam) const
{
	const char* victoryMessage = NULL;
	const int victoryStringCount = m_endOfRoundVictoryStringCount;
	for (int i = 0; i < victoryStringCount; ++ i)
	{
		if (m_previousRoundWinReason == m_endOfRoundVictoryStrings[i].m_reason && IsCurrentRoundType(m_endOfRoundVictoryStrings[i].m_round))
		{
			if (m_previousRoundWinnerTeamId)
			{
				CryLog("CGameRulesStandardRounds::ClGetVictoryMessage() has Victory message");

				if (localTeam == m_previousRoundWinnerTeamId)
				{
					victoryMessage = m_endOfRoundVictoryStrings[i].m_roundWinVictoryMessage;
				}
				else
				{
					victoryMessage = m_endOfRoundVictoryStrings[i].m_roundLoseVictoryMessage;
				}
			}
			break;
		}
	}

	EGameMode mode = g_pGame->GetGameRules()->GetGameMode();
	if (mode == eGM_CaptureTheFlag || mode == eGM_Extraction)
	{
		if (m_roundNumber == 0)
		{
			victoryMessage="@mp_CTF_EndofRound1";
		}
		else
		{
			victoryMessage="@mp_CTF_EndofRound2";
		}
	}
	else if (mode == eGM_Gladiator)
	{
		if (m_previousRoundWinnerTeamId == CGameRulesObjective_Predator::TEAM_SOLDIER)
		{
			victoryMessage="@mp_Pred_marines_survived";
		}
		else
		{
			victoryMessage="@mp_Pred_marines_eliminated";
		}
	}


	const char* timeString = GetTimeString(GetVictoryMessageTime(), true);

	victoryMessage = CHUDUtils::LocalizeString( victoryMessage, timeString );

	return victoryMessage;
}

//-------------------------------------------------------------------------
const float CGameRulesStandardRounds::GetVictoryMessageTime() const
{
	if(gEnv->bServer)
	{
		IGameRulesVictoryConditionsModule *pVictoryModule = g_pGame->GetGameRules()->GetVictoryConditionsModule();
		CRY_ASSERT(pVictoryModule);
		if (pVictoryModule)
		{
			const SDrawResolutionData* drawRes = pVictoryModule->GetDrawResolutionData(ESVC_DrawResolution_level_1);
			CRY_ASSERT(drawRes);
			if(drawRes && drawRes->m_dataType == SDrawResolutionData::eDataType_float)
			{
				return drawRes->m_floatDataForTeams[m_roundNumber % 2];
			}
		}
	}

	return m_victoryMessageTime;
}

//-------------------------------------------------------------------------
const bool CGameRulesStandardRounds::IsCurrentRoundType(const ERoundType roundType) const
{
	switch(roundType)
	{
	case ERT_Any:
		return true;
	case ERT_Even:
		return !(m_roundNumber & 0x01);
	case ERT_Odd:
		return (m_roundNumber & 0x01);
	}

	CRY_ASSERT_MESSAGE(0, "Unable to find round type");
	return false;
}

//-------------------------------------------------------------------------
// only play audio for the first round of each match
void CGameRulesStandardRounds::ShowRoundStartingMessage(bool bPlayAudio)
{
	int primaryTeam = GetPrimaryTeam();
	EntityId localActorId = g_pGame->GetIGameFramework()->GetClientActorId();
	CGameRules *pGameRules = g_pGame->GetGameRules();
	int localTeam = pGameRules->GetTeam(localActorId);

	CRY_ASSERT_MESSAGE(localActorId, "ShowRoundStartingMessage() is trying to setup messages without a valid local actor");

	CryLog("CGameRulesStandardRounds::ShowRoundStartingMessage() primaryTeam=%d; localTeam=%d", primaryTeam, localTeam);

	const char* gamemodeName = pGameRules->GetEntity()->GetClass()->GetName();

	CryFixedStringT<32> strSignalName;
	strSignalName.Format("StartGame%s", gamemodeName);
	TAudioSignalID signalId = INVALID_AUDIOSIGNAL_ID; 
	
	if (bPlayAudio)
	{
		signalId = g_pGame->GetGameAudio()->GetSignalID(strSignalName);
	}

	float messageHoldTime=0.f;

	if (pGameRules->GetGameMode() == eGM_Gladiator)
	{
		messageHoldTime=g_pGameCVars->mp_predatorParams.hintMessagePauseTime;
	}

	if (!localTeam || (primaryTeam == localTeam))
	{
		if (!m_primaryStartRoundString.empty())
		{
			const char* msg = m_primaryStartRoundString.c_str();
		
			CryLog("CGameRulesStandardRounds::ShowRoundStartingMessage() saying primary start round string %s", msg);

			if (m_primaryStartRoundStringIsTeamMessage)
			{
				SHUDEventWrapper::TeamMessage(msg, localTeam, SHUDEventWrapper::SMsgAudio(signalId), true, true, (m_bCustomHeaderPrimary)? m_primaryCustomHeader.c_str(): NULL, messageHoldTime);
			}
			else
			{
				SHUDEventWrapper::RoundMessageNotify(msg);
			}
		}

		if (!m_primaryStartRoundStringExtra.empty())
		{
			const char *msg = m_primaryStartRoundStringExtra.c_str();

			CryLog("CGameRulesStandardRounds::ShowRoundStartingMessage() saying primary start round extra string %s", msg);

			SHUDEventWrapper::SimpleBannerMessage(msg, SHUDEventWrapper::kMsgAudioNULL);
		}

		if (m_bShowPrimaryTeamBanner)
		{
			CryFixedStringT<16> strTeamName;
			strTeamName.Format("@ui_hud_team_%d", localTeam);
			SHUDEventWrapper::TeamMessage(strTeamName.c_str(), localTeam, SHUDEventWrapper::SMsgAudio(signalId), false, true, NULL, messageHoldTime);
		}
	}
	else
	{
		if (!m_secondaryStartRoundString.empty())
		{
			const char* msg = m_secondaryStartRoundString.c_str();

			CryLog("CGameRulesStandardRounds::ShowRoundStartingMessage() saying secondary start round string %s", msg);

			if (m_secondaryStartRoundStringIsTeamMessage)
			{
				SHUDEventWrapper::TeamMessage(msg, localTeam, SHUDEventWrapper::SMsgAudio(signalId), true, true, (m_bCustomHeaderSecondary)? m_secondaryCustomHeader.c_str(): NULL, messageHoldTime);
			}
			else
			{
				SHUDEventWrapper::RoundMessageNotify(msg);
			}
		}

		if (!m_secondaryStartRoundStringExtra.empty())
		{
			const char *msg = m_secondaryStartRoundStringExtra.c_str();

			CryLog("CGameRulesStandardRounds::ShowRoundStartingMessage() saying secondary start round extra string %s", msg);

			SHUDEventWrapper::SimpleBannerMessage(msg, SHUDEventWrapper::kMsgAudioNULL, messageHoldTime);
		}

		if (m_bShowSecondaryTeamBanner)
		{
			CryFixedStringT<16> strTeamName;
			strTeamName.Format("@ui_hud_team_%d", localTeam);
			SHUDEventWrapper::TeamMessage(strTeamName.c_str(), localTeam, SHUDEventWrapper::SMsgAudio(signalId), false, true, NULL, messageHoldTime);
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::DisplaySuddenDeathMessage()
{
	CryLog("ClDisplaySuddenDeathMessage()");

	SHUDEventWrapper::OnSuddenDeath();
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::OnLocalPlayerSpawned()
{
	CryLog("CGameRulesStandardRounds::OnLocalPlayerSpawned()");
	m_numLocalSpawnsThisRound++;

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		IGameRulesStateModule *pStateModule = pGameRules->GetStateModule();
		if (pStateModule->GetGameState() != IGameRulesStateModule::EGRS_InGame)
		{
			CryLog("CGameRulesStandardRounds::OnLocalPlayerSpawned() early returning as we're not ingame");
			return;
		}
	}

#if USE_PC_PREMATCH
	if (pGameRules && !pGameRules->HasGameActuallyStarted())
	{
		CryLog("CGameRulesStandardRounds::OnLocalPlayerSpawned() early returning as the game hasn't actually started");
		return;
	}

#endif // USE_PC_PREMATCH
	
	// if we're starting a round with spawns. We may spawn before we've started the round which will stop our hud messages playing sounds. 
	if (m_bShowRoundStartingMessageEverySpawn && m_roundState == eGRRS_InProgress)
	{
		if (gEnv->IsClient())
		{
			if (m_numLocalSpawnsThisRound != 1)
			{
				// first spawns of the round need to only do their message handling when the round starts (and the sounds will play)
				CryLog("CGameRulesStandardRounds::OnLocalPlayerSpawned() calling ShowRoundStartingMessage() numLocalSpawnsThisRound=%d", m_numLocalSpawnsThisRound);
				ShowRoundStartingMessage(false);		// Clients and Servers - any spawn apart from the starting the round spawn
			}
		}
	}

}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::OnEndGame(int teamId, EntityId playerId, EGameOverReason reason)
{
	if (gEnv->bServer && (IsInProgress() || m_roundState == eGRRS_Restarting))
	{
		m_previousRoundWinnerTeamId = teamId;
		m_previousRoundWinnerEntityId = playerId;
		

		CGameRules *pGameRules = g_pGame->GetGameRules();
		if(pGameRules->GetGameMode() != eGM_Extraction)
		{
			int newTeamRoundScore = pGameRules->GetTeamRoundScore(teamId);
			pGameRules->SetTeamRoundScore(teamId, ++newTeamRoundScore);
		}
		
		m_previousRoundWinReason = reason;
		if (pGameRules->GetGameMode() == eGM_Gladiator)
		{
			if (teamId == CGameRulesObjective_Predator::TEAM_SOLDIER)
			{
				m_previousRoundWinReason = EGOR_TimeLimitReached;
			}
		}

		const int  scoreTeam1 = pGameRules->GetTeamsScore(1);
		const int  scoreTeam2 = pGameRules->GetTeamsScore(2);

		if (m_allowBestOfVictory && !m_treatCurrentRoundAsFinalRound)
		{
			const int  roundsRem = GetRoundsRemaining();
			if (roundsRem > 0)
			{
				const int  scoreDiff = abs(scoreTeam1 - scoreTeam2);
				CryLog("CGameRulesStandardRounds::OnEndGame: m_allowBestOfVictory is set, m_treatCurrentRoundAsFinalRound isn't yet, at least 1 round remains, scoreDiff=%d (one=%d, two=%d), roundsRem=%d", scoreDiff, scoreTeam1, scoreTeam2, roundsRem);
				if (scoreDiff > roundsRem)
				{
					CryLog("                                   : scoreDiff is greater than roundsRem, so treating the round that's just ended as the final round");
					SetTreatCurrentRoundAsFinalRound(true);
				}
			}
		}

		m_previousRoundTeamScores[0] = scoreTeam1;
		m_previousRoundTeamScores[1] = scoreTeam2;

		EndRound();

		if (gEnv->IsClient())
		{
			pGameRules->FreezeInput(true);
			g_pGame->GetUI()->ActivateDefaultState();	// must be after settings new state to pick endround
			ClDisplayEndOfRoundMessage();
		}
		OnRoundEnd();
	}
}

//-------------------------------------------------------------------------
int CGameRulesStandardRounds::GetRoundNumber() // Starts at 0
{
	return m_roundNumber;
}

//-------------------------------------------------------------------------
int CGameRulesStandardRounds::GetRoundsRemaining() const // Returns 0 if currently on last round
{
	return (m_treatCurrentRoundAsFinalRound ? 0 : std::max(0, (g_pGame->GetGameRules()->GetRoundLimit() - m_roundNumber - 1)));
}

//-------------------------------------------------------------------------
int CGameRulesStandardRounds::GetPrimaryTeam() const
{
	if(m_primaryTeamOverride == -1)
	{
		return (m_roundNumber % 2) + 1;		// Primary team is 1 for even round numbers, 2 for odd round numbers
	}
	else
	{
		return m_primaryTeamOverride; 
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::StartNewRound( bool isReset )
{
	CGameRules *pGameRules = g_pGame->GetGameRules();

	CRY_ASSERT(gEnv->bServer);

	pGameRules->FreezeInput(false);

	CryLog("Rounds: Start New Round Called, isReset: '%s'", isReset ? "true" : "false");

	if (isReset)
	{
		LOG_PRIMARY_ROUND("LOG_PRIMARY_ROUND: CGameRulesStandardRounds::StartNewRound: resetting round from %d to 0", m_roundNumber);
		m_roundNumber = 0;
	}

	m_treatCurrentRoundAsFinalRound = false;

	pGameRules->SvCacheRoundStartTeamScores();

	if (m_resetScores)
	{
		int startTeamScore = 0;
		if (pGameRules->GetScoringModule())
		{
			startTeamScore = pGameRules->GetScoringModule()->GetStartTeamScore();
		}

		pGameRules->SetTeamsScore(1, startTeamScore);
		pGameRules->SetTeamsScore(2, startTeamScore);
	}

	SetupEntities();

	if (!isReset)
	{
		pGameRules->ResetGameTime();

		if (pGameRules->GetObjectivesModule())
		{
			pGameRules->GetObjectivesModule()->OnGameReset();
		}

		if (IGameRulesPlayerStatsModule* pPlayStatsMod=pGameRules->GetPlayerStatsModule())
		{
			pPlayStatsMod->OnStartNewRound();
		}

		// Reset entities
		IEntityItPtr pIt = gEnv->pEntitySystem->GetEntityIterator();
		IEntity *pEntity = 0;

		pIt->MoveFirst();
		while(pEntity = pIt->Next())
		{
			EntityId entityId = pEntity->GetId();
			const CGameRules::SEntityRespawnData *pRespawnData = pGameRules->GetEntityRespawnData(entityId);
			if (pRespawnData)
			{
				// Check if the entity has been modified (currently only check position and rotation)
				if (!(pEntity->GetWorldPos().IsEquivalent(pRespawnData->position) && Quat::IsEquivalent(pEntity->GetWorldRotation(),pRespawnData->rotation)))
				{
					IItem *pItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(entityId);
					if (pItem != NULL && pItem->GetOwnerId())
					{
						if (pItem->IsUsed())
						{
							pItem->StopUse(pItem->GetOwnerId());
						}
						else
						{
							pItem->Drop();
						}
					}
					pGameRules->ScheduleEntityRemoval(entityId, 0.f, false);
					pGameRules->ScheduleEntityRespawn(entityId, true, 0.f);
				}
			}
		}

		pGameRules->FlushEntitySchedules();
	}

	if (m_roundEndHUDState == eREHS_Top3)
	{
		CRY_ASSERT_MESSAGE(0, "CGameRulesStandardRounds::StartNewRound() about to set round in progress but we are still in Top3 roundstate!");
		CryLog("CGameRulesStandardRounds::StartNewRound() about to set round in progress but we are still in Top3 roundstate!");

		// TODO - perhaps reset roundEndHUDState to unknown now
	}

	SetRoundState(eGRRS_InProgress);		// Have to set state before reviving players or the spawning will refuse

	if (IGameRulesSpawningModule* pSpawningModule=pGameRules->GetSpawningModule())
	{
		pSpawningModule->ReviveAllPlayers(isReset, false);
	}

	if (IGameRulesPlayerSetupModule* pPlaySetupMod=pGameRules->GetPlayerSetupModule())
	{
		pPlaySetupMod->SvOnStartNewRound(isReset);
	}
		
	IGameRulesStateModule*  pStateMo = pGameRules->GetStateModule();
	bool  onRoundStart = (!pStateMo || pStateMo->IsOkToStartRound());
	if (onRoundStart)
	{
		pGameRules->OnRoundStart_NotifyListeners();  // for server (including dedicated)
	}

	if (gEnv->IsClient())
	{
		if (onRoundStart)
		{
			CHUDEventDispatcher::CallEvent(SHUDEvent(eHUDEvent_OnRoundStart));  // for server (but not dedicated)
		}

		// Round Start message has to happen AFTER the hud event so its sounds are played properly
		if (!isReset)  
		{
			// only want to try sending these messages on rounds other than the first round
			// the first round will be handled by OnGameStart() or OnPrematchStateEnded()
			CryLog("CGameRulesStandardRounds::StartNewRound() calling ShowRoundStartingMessage()");
			ShowRoundStartingMessage(false); // server only - clients equivelant in NetSerialize()
		}
	}

	CHANGED_NETWORK_STATE(pGameRules, STANDARD_ROUNDS_ASPECT);
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::EndRound()
{
	CGameRules *pGameRules = g_pGame->GetGameRules();

	CRY_ASSERT(gEnv->bServer);

	if(gEnv->bServer)
	{
		// if anyone was selecting equipment (eASS_ForcedEquipmentChange), set them back to spectator state, so that they can trigger the equipment select correctly in the next round
		IGameRulesSpectatorModule *pSpectatorModule = pGameRules->GetSpectatorModule();
		if (pSpectatorModule)
		{
			CGameRules::TPlayers players;
			pGameRules->GetPlayers(players);
			const int numPlayers = players.size();
			for (int i = 0; i < numPlayers; ++ i)
			{
				CPlayer *pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(players[i]));
				if (pPlayer)
				{
					if(pPlayer->GetSpectatorState() == CActor::eASS_ForcedEquipmentChange)
					{
						pPlayer->SetSpectatorState(CActor::eASS_Ingame);
						if (EntityId spectatorPointId = pSpectatorModule->GetInterestingSpectatorLocation())
						{
							pSpectatorModule->ChangeSpectatorMode(pPlayer, CActor::eASM_Fixed, spectatorPointId, true);
						}
						else
						{
							pSpectatorModule->ChangeSpectatorMode(pPlayer, CActor::eASM_Free, 0, true);
						}
					}
				}
			}
		}
	}

	if (m_roundState == eGRRS_SuddenDeath)
	{
		CryLog("CGameRulesStandardRounds::EndRound() was in sudden death, resetting timelimit");
		g_pGameCVars->g_timelimit -= m_suddenDeathTime;
		pGameRules->UpdateGameRulesCvars();
	}

	if (GetRoundsRemaining())
	{
		if (m_roundState != eGRRS_Restarting)
		{
			const float serverTime = pGameRules->GetServerTime();
			CalculateNewRoundTimes(serverTime);
			m_bShownLoadout = false;			
			SetRoundState(eGRRS_Restarting);
		}
	}
	else
	{
		const float serverTime = pGameRules->GetServerTime();
		CalculateEndGameTime(serverTime);
		SetRoundState(eGRRS_Finished);
	}

	CHANGED_NETWORK_STATE(pGameRules, STANDARD_ROUNDS_ASPECT);

	if(gEnv->bServer)
	{
		pGameRules->OnRoundEnd_NotifyListeners();  // for server
	}

	// Reset loadouts so the spawning waits for a loadout to be set before respawning players
	CEquipmentLoadout *pEquipmentLoadout = g_pGame->GetEquipmentLoadout();
	if (pEquipmentLoadout)
	{
		pEquipmentLoadout->OnRoundEnded();
	}

	m_numLocalSpawnsThisRound=0;
}

void CGameRulesStandardRounds::OnEnterSuddenDeath()
{
	CGameRules *pGameRules = g_pGame->GetGameRules();

	g_pGameCVars->g_timelimit += m_suddenDeathTime;
	pGameRules->UpdateGameRulesCvars();

	SetRoundState(eGRRS_SuddenDeath);
	CHANGED_NETWORK_STATE(pGameRules, STANDARD_ROUNDS_ASPECT);

	if(gEnv->bServer)
	{
		pGameRules->OnSuddenDeath_NotifyListeners();  // for server
		DisplaySuddenDeathMessage();
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::SetupEntities()
{
	int primaryTeamId = GetPrimaryTeam();
	int secondaryTeamId = 3 - primaryTeamId;
	CryLog("CGameRulesStandardRounds::SetupEntities(), primaryTeamId=%i", primaryTeamId);

	SetTeamEntities(m_primaryTeamEntities, primaryTeamId);
	SetTeamEntities(m_secondaryTeamEntities, secondaryTeamId);
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::SetTeamEntities( TEntityDetailsVec &classesVec, int teamId)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();

	IEntityItPtr pIt = gEnv->pEntitySystem->GetEntityIterator();
	IEntity *pEntity = 0;

	const size_t numClasses = classesVec.size();
	for (size_t i = 0; i < numClasses; ++ i)
	{
		SEntityDetails *pEntityDetails = &classesVec[i];

		TEntityIdVec newRoundEntities;

		if ((m_roundNumber % 2) == 0)
		{
			// If this is an even numbered round then we need to select new entities, otherwise we use the same as last time but with different teams

			pIt->MoveFirst();
			while(pEntity = pIt->Next())
			{
				if (pEntityDetails->m_pEntityClass == pEntity->GetClass())
				{
					newRoundEntities.push_back(pEntity->GetId());
				}
			}

			if (pEntityDetails->m_doRandomSelection)
			{
				// If we're doing random selection then we probably don't need all the entities found in the loop above, remove excess ones
				int totalEntities = newRoundEntities.size();
				while (totalEntities > pEntityDetails->m_selectCount)
				{
					int index = g_pGame->GetRandomNumber() % totalEntities;
					TEntityIdVec::iterator it = newRoundEntities.begin() + index;
					newRoundEntities.erase(it);
					-- totalEntities;
				}
			}
		}
		else
		{
			newRoundEntities = pEntityDetails->m_currentEntities;
		}

		// Deactivate all the entities used in previous rounds (even if we're going to reactivate it in a second)
		if (pEntityDetails->m_currentEntities.size())
		{
			DeactivateEntities(pEntityDetails->m_deactivateFunc.c_str(), pEntityDetails->m_currentEntities);
		}

		const size_t totalEntities = newRoundEntities.size();
		for (size_t j = 0; j < totalEntities; ++j)
		{
			EntityId entityId = newRoundEntities[j];
			ActivateEntity(entityId, teamId, pEntityDetails->m_activateFunc.c_str(), pEntityDetails->m_currentEntities);
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::OnPromoteToServer()
{
	int primaryTeamId = GetPrimaryTeam();
	int secondaryTeamId = 3 - primaryTeamId;
	GetTeamEntities(m_primaryTeamEntities, primaryTeamId);
	GetTeamEntities(m_secondaryTeamEntities, secondaryTeamId);
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::GetTeamEntities( TEntityDetailsVec &classesVec, int teamId)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();

	IEntityItPtr pIt = gEnv->pEntitySystem->GetEntityIterator();
	IEntity *pEntity = 0;

	const size_t numClasses = classesVec.size();

	for (size_t i = 0; i < numClasses; ++ i)
	{
		SEntityDetails *pEntityDetails = &classesVec[i];

		pIt->MoveFirst();
		while(pEntity = pIt->Next())
		{
			if (pEntityDetails->m_pEntityClass == pEntity->GetClass())
			{
				const EntityId entityId = pEntity->GetId();
				if (pGameRules->GetTeam(entityId) == teamId)
				{
					pEntityDetails->m_currentEntities.push_back(entityId);
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::ActivateEntity( EntityId entityId, int teamId, const char *pActivateFunc, TEntityIdVec &pEntitiesVec )
{
	g_pGame->GetGameRules()->SetTeam(teamId, entityId);
	if (pActivateFunc && *pActivateFunc)
	{
		CallScript(entityId, pActivateFunc);
	}
	pEntitiesVec.push_back(entityId);
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::DeactivateEntities( const char *pDeactivateFunc, TEntityIdVec &entitiesVec)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();

	int numEntities = entitiesVec.size();
	for (int i = 0; i < numEntities; ++ i)
	{
		EntityId entityId = entitiesVec[i];
		pGameRules->SetTeam(0, entityId);
		if (pDeactivateFunc && *pDeactivateFunc)
		{
			CallScript(entityId, pDeactivateFunc);
		}
	}
	entitiesVec.clear();
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::CallScript( EntityId entityId, const char *pFunction )
{
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(entityId);
	if (pEntity)
	{
		IScriptTable *pScriptTable = pEntity->GetScriptTable();
		if (pScriptTable)
		{
			if (pScriptTable->GetValueType(pFunction) == svtFunction)
			{
				IScriptSystem *pScriptSystem = gEnv->pScriptSystem;
				pScriptSystem->BeginCall(pScriptTable, pFunction);
				pScriptSystem->PushFuncParam(pScriptTable);
				pScriptSystem->EndCall();
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::CmdNextRound( IConsoleCmdArgs *pArgs )
{
	CGameRulesStandardRounds *pRoundsModule = static_cast<CGameRulesStandardRounds*>(g_pGame->GetGameRules()->GetRoundsModule());
	if (pRoundsModule)
	{
		pRoundsModule->EndRound();
		++ pRoundsModule->m_roundNumber;
		LOG_PRIMARY_ROUND("LOG_PRIMARY_ROUND: CmdNextRound: incremented round num to %d", pRoundsModule->m_roundNumber);
		pRoundsModule->StartNewRound(false);
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::OnChangedTeam( EntityId entityId, int oldTeamId, int newTeamId )
{
	if (!gEnv->bServer)		// This stuff has already been done on the server
	{
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(entityId);

		CheckForTeamEntity(pEntity, newTeamId, m_primaryTeamEntities);
		CheckForTeamEntity(pEntity, newTeamId, m_secondaryTeamEntities);
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::CheckForTeamEntity( IEntity *pEntity, int newTeamId, TEntityDetailsVec &entitiesVec )
{
	CryLog("CGameRulesStandardRounds::CheckForTeamEntity, entity '%s' has joined team '%i'", pEntity->GetName(), newTeamId);
	const IEntityClass *pEntityClass = pEntity->GetClass();

	int numEntityTypes = entitiesVec.size();
	for (int i = 0; i < numEntityTypes; ++ i)
	{
		SEntityDetails *pEntityDetails = &entitiesVec[i];
		if (pEntityDetails->m_pEntityClass == pEntityClass)
		{
			if (newTeamId)
			{
				// Activate the entity
				if (!pEntityDetails->m_activateFunc.empty())
				{
					CallScript(pEntity->GetId(), pEntityDetails->m_activateFunc.c_str());
					CryLog("Activating");
				}
			}
			else
			{
				// Deactivate the entity
				if (!pEntityDetails->m_deactivateFunc.empty())
				{
					CallScript(pEntity->GetId(), pEntityDetails->m_deactivateFunc.c_str());
					CryLog("Deactivating");
				}
			}
			break;
		}
	}
}

// IGameRulesStateListener
void CGameRulesStandardRounds::OnGameStart()
{
	CryLog("CGameRulesStandardRounds::OnGameStart()");
	
#if USE_PC_PREMATCH
	// when prematch is enabled we actually do start game before the prematch. 
	// In this case when we come out of prematch we'll handle it then in OnPrematchStateEnded()
	CGameRules*  pGameRules = g_pGame->GetGameRules();
	if (pGameRules && !pGameRules->HasGameActuallyStarted())
	{
		return;
	}
#endif // USE_PC_PREMATCH

	if (gEnv->IsClient())
	{
		CryLog("CGameRulesStandardRounds::OnGameStart() calling ShowRoundStartingMessage()");
		ShowRoundStartingMessage(true);  // clients and servers. First round starting, when no prematch on PC
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::Update( float frameTime )
{
	CGameRules*  pGameRules = g_pGame->GetGameRules();
	const float  serverTime = pGameRules->GetServerTime();

#if !defined(_RELEASE)
	if( m_missedLoadout > 0 )
	{
		CryWatch( "Rounds: Missed Loadout Display %d times", m_missedLoadout );
	}
	//CryWatch("numLocalSpawnsThisRound=%d", m_numLocalSpawnsThisRound);
#endif

	if (m_roundState == eGRRS_Restarting)
	{
		if (gEnv->bServer)
		{
			if (serverTime > m_newRoundStartTime)
			{
				++ m_roundNumber;
				LOG_PRIMARY_ROUND("LOG_PRIMARY_ROUND: Update: incremented round num to %d", m_roundNumber);
				StartNewRound(false);
			}
		}

		if (m_bShownLoadout == false)
		{
			if (gEnv->IsClient())
			{
				UpdateRoundEndHUD(frameTime);
			}

			if (serverTime > m_newRoundShowLoadoutTime)
			{
				CryLog("Rounds: Show Loadout time hit, current server time %.2f", serverTime);
				NewRoundTransition();
			}
		}
	}
	else if(m_roundState == eGRRS_Finished)
	{
		CGameRules *pNewGameRules = g_pGame->GetGameRules();
		const float newServerTime = pNewGameRules->GetServerTime();

		if (gEnv->IsClient())
		{
			UpdateRoundEndHUD(frameTime);
		}

		if (gEnv->bServer)
		{
			if (newServerTime > m_newRoundStartTime)
			{
				SetRoundState(eGRRS_GameOver);
				CHANGED_NETWORK_STATE(pNewGameRules, STANDARD_ROUNDS_ASPECT);

				if (IGameRulesVictoryConditionsModule *pVictoryConditionsModule = pNewGameRules->GetVictoryConditionsModule())
				{
					pVictoryConditionsModule->SvOnEndGame();
				}
			}
		}
	}
	else if (m_roundState == eGRRS_SuddenDeath)
	{
		const IGameRulesObjectivesModule* pObjectivesModule = g_pGame->GetGameRules()->GetObjectivesModule();
		if (pObjectivesModule && !pObjectivesModule->AreSuddenDeathConditionsValid())
		{
			g_pGameCVars->g_timelimit -= m_suddenDeathTime;
			pGameRules->UpdateGameRulesCvars();
		}
	}
	else if (m_roundState == eGRRS_GameOver)
	{
		if (gEnv->IsClient())
		{
			UpdateRoundEndHUD(frameTime);
		}
	}

	m_prevServerTime = serverTime;
}

//-------------------------------------------------------------------------
float CGameRulesStandardRounds::GetTimeTillRoundStart() const
{
	float milliseconds = m_newRoundStartTime - g_pGame->GetGameRules()->GetServerTime();
	return (milliseconds / 1000.f);
}

//-------------------------------------------------------------------------
bool CGameRulesStandardRounds::CanEnterSuddenDeath() const
{
	bool ret=false;

	const IGameRulesObjectivesModule* pObjectivesModule = g_pGame->GetGameRules()->GetObjectivesModule();
	if (	m_roundState == eGRRS_InProgress && 
			m_suddenDeathTime > 0.0f )
	{
		if (pObjectivesModule)
		{
			ret=pObjectivesModule->AreSuddenDeathConditionsValid();
		}
		else
		{
			ret=true;
		}
	}

	return ret;
}


void CGameRulesStandardRounds::SetRoundState(ERoundState inRoundState)
{
	CryLog("CGameRulesStandardRounds::SetRoundState() %d -> %d (roundEndHudState=%d)", m_roundState, inRoundState, m_roundEndHUDState);
	m_roundState = inRoundState;
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::EnterRoundEndHUDState( ERoundEndHUDState state )
{
	CryLog("CGameRulesStandardRounds::EnterRoundEndHUDState() %d -> %d", m_roundEndHUDState, state);
	m_roundEndHUDState = state;
	m_timeSinceRoundEndStateChange = 0.f;
#if 0 // old frontend
	// Close menu if equipment select, to show round end. 
	TFlashFrontEndPtr pFlashMenu = g_pGame->GetFlashFrontEnd();
	if (pFlashMenu.get() && pFlashMenu->GetCurrentMenuScreen() == eFFES_equipment_select)
	{
		pFlashMenu->Schedule(eSchedule_Clear);
	}
#endif
	
	switch (state)
	{
		case eREHS_HUDMessage:
		{
			break;
		}
		case eREHS_Top3:
		{
			CRecordingSystem *pRecordingSystem = g_pGame->GetRecordingSystem();
			if (pRecordingSystem->IsPlayingBack())
			{
				pRecordingSystem->StopPlayback();
			}
			
			g_pGame->GetUI()->ActivateDefaultState();

			SHUDEvent  scoreboardVisibleEvent( eHUDEvent_MakeMatchEndScoreboardVisible );
			scoreboardVisibleEvent.AddData(true);
			CHUDEventDispatcher::CallEvent(scoreboardVisibleEvent);

			break;
		}
		case eREHS_WinningKill:
		{
			bool bHideTop3 = true;

			if (m_bShowKillcamAtEndOfRound)
			{
				CRecordingSystem *pRecordingSystem = g_pGame->GetRecordingSystem();
				if(!pRecordingSystem->PlayWinningKillcam())
				{
					bHideTop3 = false;
				}
			}

			if (bHideTop3)
			{
				SHUDEvent  scoreboardVisibleEvent( eHUDEvent_MakeMatchEndScoreboardVisible );
				scoreboardVisibleEvent.AddData(false);
				CHUDEventDispatcher::CallEvent(scoreboardVisibleEvent);
			}
			break;
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::UpdateRoundEndHUD(float frameTime)
{
	m_timeSinceRoundEndStateChange += frameTime;

	switch (m_roundEndHUDState)
	{
		case eREHS_HUDMessage:
		{
			if (m_timeSinceRoundEndStateChange > g_pGameCVars->g_gameRules_postGame_HUDMessageTime)
			{
				EnterRoundEndHUDState(eREHS_Top3);
			}
			break;
		}
		case eREHS_Top3:
		{
			if (m_timeSinceRoundEndStateChange > g_pGameCVars->g_roundScoreboardTime)
			{
				EnterRoundEndHUDState(eREHS_WinningKill);
			}
			break;
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::CalculateNewRoundTimes( float serverTime )
{
	float killcamTime = (m_bShowKillcamAtEndOfRound ? g_pGameCVars->kc_length : 0.f);
	m_newRoundShowLoadoutTime = serverTime + ((g_pGameCVars->g_gameRules_postGame_HUDMessageTime + g_pGameCVars->g_roundScoreboardTime + killcamTime) * 1000.f);
	m_newRoundStartTime = m_newRoundShowLoadoutTime + (g_pGameCVars->g_roundStartTime * 1000.f);
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::CalculateEndGameTime( float serverTime )
{
	m_newRoundStartTime = serverTime + ((g_pGameCVars->g_gameRules_postGame_HUDMessageTime + g_pGameCVars->g_roundScoreboardTime) * 1000.f);
}

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::OnRoundEnd()
{
}


void CGameRulesStandardRounds::NewRoundTransition()
{
	CGameRules*  pGameRules = g_pGame->GetGameRules();
	const float  serverTime = pGameRules->GetServerTime();

	if (gEnv->bServer)
	{
		IGameRulesSpectatorModule *pSpectatorModule = pGameRules->GetSpectatorModule();
		if (pSpectatorModule)
		{
			CGameRules::TPlayers players;
			pGameRules->GetPlayers(players);
			const int numPlayers = players.size();
			for (int i = 0; i < numPlayers; ++ i)
			{
				CPlayer *pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(players[i]));
				if (pPlayer)
				{
					pPlayer->SetSpectatorState(CActor::eASS_ForcedEquipmentChange);

					if (EntityId spectatorPointId = pSpectatorModule->GetInterestingSpectatorLocation())
					{
						pSpectatorModule->ChangeSpectatorMode(pPlayer, CActor::eASM_Fixed, spectatorPointId, false);
					}
					else
					{
						pSpectatorModule->ChangeSpectatorMode(pPlayer, CActor::eASM_Free, 0, false);
					}
				}
			}
		}
	}
	m_bShownLoadout = true;

	CryLog("CGameRulesStandardRounds::Update() just shown loadout, setting roundEndHUDState to unknown!");
	EnterRoundEndHUDState(eREHS_Unknown);
	CHUDEventDispatcher::CallEvent(SHUDEvent(eHUDEvent_FlushMpMessages)); // Flush/clear any messages, in particular the "you can't spawn because the round was already started" message

	if( !gEnv->IsDedicated() )
	{
		const float timeTillStartInMilliseconds = (m_newRoundStartTime - serverTime);
		const float timeTillStartInSeconds = floorf((timeTillStartInMilliseconds * 0.001f) + 0.5f);
		SHUDEventWrapper::UpdateGameStartCountdown( ePreGameCountdown_RoundStarting, timeTillStartInSeconds );
	}

	CHUDEventDispatcher::CallEvent(SHUDEvent(eHUDEvent_OnRoundAboutToStart));

	pGameRules->OnRoundAboutToStart_NotifyListeners();
}

#if USE_PC_PREMATCH
void CGameRulesStandardRounds::OnPrematchStateEnded(bool isSkipped)
{
	// show messages and disarm flags so dont show on next spawn
	// skipped prematches need to show their round messages whenever the game starts, in OnGameStart()
	if (!isSkipped)
	{
		if (gEnv->IsClient())
		{
			CryLog("CGameRulesStandardRounds::OnPrematchStateEnded() calling ShowRoundStartingMessage()");
			ShowRoundStartingMessage(true);		// clients and servers.. prematch ended
		}
	}
}
#endif // #use USE_PC_PREMATCH

//-------------------------------------------------------------------------
void CGameRulesStandardRounds::AdjustTimers( CTimeValue adjustment )
{
	const float milliseconds = adjustment.GetMilliSeconds();
	m_newRoundStartTime += milliseconds;
	m_newRoundShowLoadoutTime += milliseconds;
	
	if(m_roundState==eGRRS_Restarting)
	{
		m_bShownLoadout = false;
	}
}

