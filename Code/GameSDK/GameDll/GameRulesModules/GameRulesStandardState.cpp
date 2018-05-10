// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 

	-------------------------------------------------------------------------
	History:
	- 03:09:2009  : Created by Colin Gulliver

*************************************************************************/

#include "StdAfx.h"
#include "GameRulesStandardState.h"
#include <CrySystem/XML/IXml.h>
#include "GameRules.h"
#include "GameCVars.h"
#include "IGameObject.h"
#include <CryNetwork/ISerialize.h>
#include "GameRulesModules/IGameRulesStatsRecording.h"
#include "GameRulesModules/IGameRulesPlayerStatsModule.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "UI/HUD/HUDUtils.h"
#include "UI/UIManager.h"
#include "PlayerProgression.h"
#include "PlaylistManager.h"
#include "Actor.h"
#include "Network/Lobby/GameLobby.h"
#include "GameRulesModules/IGameRulesSpawningModule.h"
#include "EquipmentLoadout.h"
#include "Audio/Announcer.h"
#include "GameCodeCoverage/GameCodeCoverageManager.h"
#include "RecordingSystem.h"
#include "UI/Menu3dModels/MenuRender3DModelMgr.h"
#include "GameActions.h"
#include "GameRulesModules/IGameRulesObjectivesModule.h"
#include "GameRulesModules/IGameRulesSpectatorModule.h"
#include "Utility/CryWatch.h"
#include "Player.h"

#if NUM_ASPECTS > 8
	#define STANDARD_STATE_ASPECT			eEA_GameServerC
#else
	#define STANDARD_STATE_ASPECT			eEA_GameServerStatic
#endif

#define KILLCAM_EXTRA_TIME 1.3f //The additional time a single killcam video might need to finish (Including initialization/transition/slow-motion)

//-------------------------------------------------------------------------
CGameRulesStandardState::CGameRulesStandardState()
	: m_timeInPostGame(0.0f)
	, m_startTimerOverrideWait(0.0f)
	, m_timeInCurrentPostGameState(0.0f)
	, m_isStarting(false)
	, m_introMessageShown(false)
	, m_isWaitingForOverrideTimer(false)
	, m_bHaveNotifiedIntroListeners(false)
	, m_bHasShownHighlightReel(false)
{
	CryLog("CGameRulesStandardState::CGameRulesStandardState()");

	m_pGameRules = NULL;
}

//-------------------------------------------------------------------------
CGameRulesStandardState::~CGameRulesStandardState()
{
	IConsole *pConsole = gEnv->pConsole;
	pConsole->RemoveCommand("g_setgamestate");
	CMenuRender3DModelMgr::Release(true);
}

//-------------------------------------------------------------------------
void CGameRulesStandardState::Init( XmlNodeRef xml )
{
	m_pGameRules = g_pGame->GetGameRules();

	m_state = EGRS_Intro; 
	m_lastReceivedServerState = m_state; 
	m_timeInPostGame = 0.f;
	m_introMessageShown = false;
	m_isStarting = false;
	m_isWaitingForOverrideTimer = false;
	m_startTimerOverrideWait = 0.0f;
	m_timeInCurrentPostGameState = 0.f;
	m_postGameState = ePGS_Unknown;
	m_bHaveNotifiedIntroListeners = false; 
	m_bHasShownHighlightReel = false;

	ChangeState(EGRS_Intro);

	int numChildren = xml->getChildCount();
	for (int i = 0; i < numChildren; ++ i)
	{
		XmlNodeRef xmlChild = xml->getChild(i);
		if (!stricmp(xmlChild->getTag(), "StartStrings"))
		{
			const char *pString = 0;
			if (xmlChild->getAttr("startMatch", &pString))
			{
				m_startMatchString.Format("@%s", pString);
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardState::PostInit()
{
	REGISTER_COMMAND("g_setgamestate", CmdSetState, VF_NULL, "Force game state (0=Reset, 1=PreGame, 2=InGame, 3=PostGame");

	// Gather list of initially connecting clients to wait for before starting		
	if (CGameLobby* pGameLobby = g_pGame->GetGameLobby())
	{
		const SSessionNames &sessionNames = pGameLobby->GetSessionNames();

		const int numPlayers = sessionNames.Size();
		for (int i = 0; i < numPlayers; ++ i)
		{
			const int channelId = (int)sessionNames.m_sessionNames[i].m_conId.m_uid;
			m_initialChannelIds.push_back(channelId);
			CryLog("CGameRulesStandardState::PostInit(), Initial player channels, adding %d)", channelId);
		}
	}
}

//-------------------------------------------------------------------------
CGameRulesStandardState::EGR_GameState CGameRulesStandardState::GetGameState() const
{
	return m_state;
}

//-------------------------------------------------------------------------
void CGameRulesStandardState::OwnClientEnteredGame(const CPlayer& rPlayer)
{
	CryLog("OwnClientEnteredGame() for '%s', ShouldPlayIntro: %s, state: %i", rPlayer.GetEntity()->GetName(), rPlayer.ShouldPlayIntro() ? "true" : "false", m_state);

	if(!rPlayer.ShouldPlayIntro() && m_state == IGameRulesStateModule::EGRS_Intro)
	{
		// If the player spawn indicates we shouldn't play the intro, move on to the next server state
		CryLog("CGameRulesStandardState::OwnClientEnteredGame(), state changed from %i to %i", (int)m_state, (int)m_lastReceivedServerState);
		ChangeState(m_lastReceivedServerState);
		if(m_pGameRules->IsIntroSequenceRegistered()) // If registered, then players will have been hidden + tag names disabled. Clean up. 
		{
			CleanUpAbortedIntro(); 
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardState::Add3DWinningTeamMember()
{
	IActor * localActor = g_pGame->GetIGameFramework()->GetClientActor();

	if (localActor != NULL && static_cast<CActor*>(localActor)->GetSpectatorState() != CActor::eASS_SpectatorMode && !g_pGame->GetUI()->IsInMenu())
	{
		IEntity * renderModelFromEntity = NULL;

		if (m_pGameRules->GetTeamCount() > 1)
		{
			// Only way to get in here is for it to be a team game (as opposed to every-man-for-himself Instant Action)
			const int teamId = m_pGameRules->GetTeam(localActor->GetEntityId());
			assert(teamId == 1 || teamId == 2);
			const int enemyTeamId = 3 - teamId;

			const int clientTeamScore = m_pGameRules->GetTeamsScore(teamId);
			const int nonClientTeamScore = m_pGameRules->GetTeamsScore(enemyTeamId);
			bool winning = (clientTeamScore >= nonClientTeamScore);

			CryLog ("Want to add 3D model of winning team member... local player '%s' is %s", localActor->GetEntity()->GetName(), winning ? " on the winning team" : "not on the winning team");
			if (! winning)
			{
				if (m_pGameRules->GetTeamPlayerCount(enemyTeamId, false))
				{
					renderModelFromEntity = gEnv->pEntitySystem->GetEntity(m_pGameRules->GetTeamPlayer(enemyTeamId, 0));
				}
			}
		}

		if (renderModelFromEntity == NULL)
		{
			// Local entity must exist (checked at top of function) so we'll _definitely_ have a renderModelFromEntity after this
			renderModelFromEntity = localActor->GetEntity();
		}

		// Additional paranoia :)
		if (renderModelFromEntity)
		{
			CryLog ("Adding 3D model of '%s' to accompany scoreboard", renderModelFromEntity->GetName());
			INDENT_LOG_DURING_SCOPE();

			ICharacterInstance * characterInst = renderModelFromEntity->GetCharacter(0);
			if (characterInst != NULL)
			{
				CMenuRender3DModelMgr *pModelManager = new CMenuRender3DModelMgr();

				CMenuRender3DModelMgr::SSceneSettings sceneSettings;
				sceneSettings.fovScale = 0.5f;
				sceneSettings.fadeInSpeed = 0.01f;
				sceneSettings.flashEdgeFadeScale = 0.2f;
				sceneSettings.ambientLight = Vec4(0.f, 0.f, 0.f, 0.8f);
				sceneSettings.lights.resize(3);
				sceneSettings.lights[0].pos.Set(-25.f, -10.f, 30.f);
				sceneSettings.lights[0].color.Set(5.f, 6.f, 5.5f);
				sceneSettings.lights[0].specular = 4.f;
				sceneSettings.lights[0].radius = 400.f;
				sceneSettings.lights[1].pos.Set(25.f, -4.f, 30.f);
				sceneSettings.lights[1].color.Set(0.7f, 0.7f, 0.7f);
				sceneSettings.lights[1].specular = 10.f;
				sceneSettings.lights[1].radius = 400.f;
				sceneSettings.lights[2].pos.Set(60.f, 40.f, 10.f);
				sceneSettings.lights[2].color.Set(0.5f, 1.0f, 0.7f);
				sceneSettings.lights[2].specular = 10.f;
				sceneSettings.lights[2].radius = 400.f;
				pModelManager->SetSceneSettings(sceneSettings);

				const char * usePlayerModelName = characterInst->GetFilePath();

				CMenuRender3DModelMgr::SModelParams params;
				params.pFilename = usePlayerModelName;
				params.posOffset.Set(0.1f, 0.f, -0.2f);
				params.rot.Set(0.f, 0.f, 3.5f);
				params.scale = 1.35f;
				params.pName = "char";
				params.screenRect[0] = 0.f;
				params.screenRect[1] = 0.f;
				params.screenRect[2] = 0.3f;
				params.screenRect[3] = 0.8f;

				const float ANIM_SPEED_MULTIPLIER = 0.5f;
				if (m_pGameRules->GetGameMode() == eGM_Gladiator)
				{
					// In Hunter mode, force the model to be a hunter
					params.pFilename = "Objects/Characters/Human/sdk_player/sdk_player.cdf";
				}
				const CMenuRender3DModelMgr::TAddedModelIndex characterModelIndex = pModelManager->AddModel(params);
				pModelManager->UpdateAnim(characterModelIndex, "stand_tac_idle_nw_3p_01", ANIM_SPEED_MULTIPLIER);
			}
		}
	}
}

//-------------------------------------------------------------------------
bool CGameRulesStandardState::CheckInitialChannelPlayers()
{
	bool bOk = false;

	if (CGameLobby* pGameLobby = g_pGame->GetGameLobby())
	{
		const SSessionNames &sessionNames = pGameLobby->GetSessionNames();

		int inGameCount = 0;
		int initialNumPlayers = m_initialChannelIds.size();
		for (int i = 0; i < initialNumPlayers; ++ i)
		{
			const int channelId = m_initialChannelIds[i];

			SCryMatchMakingConnectionUID connectionId;
			connectionId.m_uid = channelId;
			if (sessionNames.FindIgnoringSID(connectionId) == SSessionNames::k_unableToFind)
			{
				m_initialChannelIds.removeAt(i);
				--i;
				--initialNumPlayers;

				CryLog("CGameRulesStandardState::CheckInitialChannelPlayers(), Client channel %d is in the initial channel list but no longer in the lobby, removing)", channelId);

				continue;
			}

			if (m_pGameRules->IsChannelInGame(channelId))
			{
				++inGameCount;
			}
		}

		const float startTimerPlayerRatio = g_pGameCVars->g_gameRules_startTimerPlayersRatio;
		const float currentRatio = (initialNumPlayers > 0) ? ((float)inGameCount / initialNumPlayers) : 1.f;

		//CryLog("CGameRulesStandardState::CheckInitialChannelPlayers(), Waiting for channels to join %d of %d clients have joined (%.2f / %.2f required, %f sec til starting anyway)", inGameCount, initialNumPlayers, currentRatio, startTimerPlayerRatio, m_startTimerOverrideWait);

		bOk = (currentRatio >= startTimerPlayerRatio);
	}

	return bOk;
}

//-------------------------------------------------------------------------
void CGameRulesStandardState::Update( float frameTime )
{
	if (gEnv->bServer)
	{
		if(m_state == EGRS_Intro)
		{
			// We assume there is an intro, if we reach this point and an intro hasnt been registered, we know there isn't one. Onwards and upwards. 
			if(!m_pGameRules->IsIntroSequenceRegistered())
			{
				ChangeState(EGRS_PreGame); 
			}
		}

		if (m_state == EGRS_PreGame)
		{
			if (m_isStarting)
			{
				const float remainingTime = m_pGameRules->GetRemainingStartTimer();
				if (remainingTime <= 0.f)
				{
					CryLog("CGameRulesStandardState::Update(), starting game");

					ChangeState(EGRS_InGame);
				}
			}
			else
			{
				bool bOk = true;
				CGameLobby* pGameLobby = g_pGame->GetGameLobby();
				const int numPlayers = m_pGameRules->GetPlayerCount(true);

				if (pGameLobby)
				{
					if (m_isWaitingForOverrideTimer)
					{
						//-- test override timer
						m_startTimerOverrideWait -= frameTime;
						bOk = (m_startTimerOverrideWait <= 0.0f);

						if (!bOk)
						{
							bOk = true;

							//-- testing min player count doesn't apply to private games
							const bool publicGame = !pGameLobby->IsPrivateGame();
							const bool onlineGame = pGameLobby->IsOnlineGame();
							if (publicGame && onlineGame)
							{
								// Start only when we have enough players
								if (m_pGameRules->GetTeamCount() > 1)
								{
									//-- team game, insist at least 1 player per team
									const int numPlayersPerTeamMin = g_pGameCVars->g_gameRules_startTimerMinPlayersPerTeam;
									const int numPlayersTeam1 = m_pGameRules->GetTeamPlayerCount(1, true);
									const int numPlayersTeam2 = m_pGameRules->GetTeamPlayerCount(2, true);

									bOk = ((numPlayersTeam1 >= numPlayersPerTeamMin) && (numPlayersTeam2 >= numPlayersPerTeamMin));
								}
								else
								{
									//-- not a team game, so just insist on minimum 2 players
									const int numPlayersMin = g_pGameCVars->g_gameRules_startTimerMinPlayers;
									bOk = (numPlayers >= numPlayersMin);
								}
								const int numPlayersInLobby = pGameLobby->GetSessionNames().Size();
								bOk |= (numPlayersInLobby == numPlayers);
							}

							if (bOk)
							{
								//-- Enforce a percentage of lobby players in game before starting countdown
								bOk = (!gEnv->IsClient() || (gEnv->pGameFramework->GetClientActorId() != 0)) && CheckInitialChannelPlayers();
							}
						}
					}
					else
					{
						bOk = false;

						if (numPlayers)
						{
							//-- Start the override timer. 
							m_startTimerOverrideWait = g_pGameCVars->g_gameRules_startTimerOverrideWait;
							m_isWaitingForOverrideTimer = true;
						}
					}
				}

				if (bOk)
				{
					CryLog("CGameRulesStandardState::Update(), we have %i players, starting the game", numPlayers);
					float startTimeLength = 
#if !defined(_RELEASE)
						g_pGameCVars->g_gameRules_skipStartTimer ? 0.0f : 
#endif
						g_pGameCVars->g_gameRules_startTimerLength;

#if USE_PC_PREMATCH
					bool bDoPCPrematch = false;

					CGameRules::EPrematchState prematchState = m_pGameRules->GetPrematchState();
					if (prematchState==CGameRules::ePS_Prematch)
					{
						int numRequiredPlayers = g_pGameCVars->g_minPlayersForRankedGame - m_pGameRules->GetPlayerCount(true);
						if ((numRequiredPlayers > 0) || (pGameLobby && pGameLobby->UseLobbyTeamBalancing() && !pGameLobby->IsGameBalanced()))
						{
							bDoPCPrematch = true;

							CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
							if (pGameLobby && pPlaylistManager)
							{
								if (!pGameLobby->IsRankedGame() || pPlaylistManager->IsUsingCustomVariant())
								{
									// Private games don't do prematch
									bDoPCPrematch = false;
								}
							}

							if (bDoPCPrematch)
							{
								// If we are waiting for players on pc, spawn ingame and set a prematch state which waits for players.
								m_pGameRules->StartPrematch();
								startTimeLength = 0.f;
							}
						}

						if (!bDoPCPrematch)
						{
							m_pGameRules->SkipPrematch();
						}
					}
#endif

					m_pGameRules->ResetGameStartTimer(startTimeLength);
					StartCountdown(true);
				}
			}
		}
		else if (m_state == EGRS_PostGame)
		{
			const float prevUpdateStamp = m_timeInPostGame;
			const float timeInPost = (prevUpdateStamp + frameTime);

			const float timeToShowHUDMessage = g_pGameCVars->g_gameRules_postGame_HUDMessageTime;
			const float timeToShowTop3 = g_pGameCVars->g_gameRules_postGame_Top3Time;
			const float timeToShowScoreboard = g_pGameCVars->g_gameRules_postGame_ScoreboardTime;
			float killcamLength = m_pGameRules->GameEndedByWinningKill() ? g_pGameCVars->kc_length : 0.f;
			if (g_pGameCVars->kc_showHighlightsAtEndOfGame)
			{
				CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
				if(pRecordingSystem)
				{
					killcamLength += pRecordingSystem->GetHighlightsReelLength();
					killcamLength = min(killcamLength, 20.f);
				}
			}

			const float totalPostGameTime = timeToShowHUDMessage + timeToShowTop3 + timeToShowScoreboard + killcamLength;

			if (timeInPost > totalPostGameTime)
			{
				CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
				if (!pRecordingSystem || (!pRecordingSystem->IsPlaybackQueued() && !pRecordingSystem->IsPlayingBack()))
				{
					CGameLobby *pGameLobby = g_pGame->GetGameLobby();
					if (pGameLobby)
					{
						CryLog("[GameRules] Server trying to return to lobby");
						pGameLobby->SvFinishedGame(frameTime);
					}
				}
				else if(pRecordingSystem)
				{
					pRecordingSystem->StopHighlightReel();
				}
			}

			m_timeInPostGame = timeInPost;
		}
	}

	CPlayer * pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetClientActor());
	if((pPlayer && pPlayer->ShouldPlayIntro() || gEnv->bServer) && m_pGameRules->IsIntroSequenceRegistered() && !m_bHaveNotifiedIntroListeners)
	{
		// All flowgraph nodes that want to listen, should be created at this point
		OnIntroStart_NotifyListeners();
	}
	

#ifndef _RELEASE
	if(g_pGameCVars->g_hud_postgame_debug)
	{
		const char* stateName = "";
		switch(m_state)
		{
		case EGRS_Reset: { stateName = "EGRS_Reset"; break;}
		case EGRS_Intro: { stateName = "EGRS_Intro"; break;}
		case EGRS_PreGame: { stateName = "EGRS_PreGame"; break;}
		case EGRS_InGame: { stateName = "EGRS_InGame"; break;}
		case EGRS_PostGame: { stateName = "EGRS_PostGame"; break;}
		case EGRS_MAX: { stateName = "EGRS_MAX"; break;}
		}
		CryWatch("GameRulesStandardState - State = %s", stateName);

		if(m_state == EGRS_PostGame)
		{
			const char* postGameStateName = "";
			switch(m_postGameState)
			{
			case ePGS_Unknown: { postGameStateName = "ePGS_Unknown"; break; }
			case ePGS_Starting: { postGameStateName = "ePGS_Starting"; break; }
			case ePGS_HudMessage: { postGameStateName = "ePGS_HudMessage"; break; }
			case ePGS_FinalKillcam: { postGameStateName = "ePGS_FinalKillcam"; break; }
			case ePGS_HighlightReel: { postGameStateName = "ePGS_HighlightReel"; break; }
			case ePGS_Top3: { postGameStateName = "ePGS_Top3"; break; }
			case ePGS_Scoreboard: { postGameStateName = "ePGS_Scoreboard"; break; }
			}
			CryWatch("GameRulesStandardState -PostGameState = %s", postGameStateName);

		}
	}
#endif

	if (gEnv->IsClient())
	{
		if (m_state == EGRS_PreGame)
		{
			if( !gEnv->IsDedicated() )
			{
				if (m_isStarting)
				{
					const float timeTillStartInSeconds = m_pGameRules->GetRemainingStartTimer();
					SHUDEventWrapper::UpdateGameStartCountdown( ePreGameCountdown_MatchStarting, timeTillStartInSeconds );
				}
				else
				{
					SHUDEventWrapper::UpdateGameStartCountdown( ePreGameCountdown_WaitingForPlayers, 0.0f );
				}
			}
		}
		else if (m_state == EGRS_InGame && !gEnv->IsDedicated() )
		{
			if (m_introMessageShown == false)	// Show only once
			{
				CGameRules *pGameRules = g_pGame->GetGameRules();
				if (pGameRules && pGameRules->HasGameActuallyStarted())
				{
					if (EntityId localPlayerId = g_pGame->GetIGameFramework()->GetClientActorId())
					{
						int teamId = g_pGame->GetGameRules()->GetTeam(localPlayerId);
						bool bTeamGame = (pGameRules->GetTeamCount() > 1);

						IActor *pActor = g_pGame->GetIGameFramework()->GetClientActor();
						if (pActor->GetSpectatorMode()==CActor::eASM_None && !pActor->IsDead() && (!bTeamGame || teamId!=0))
						{
							if (IGameRulesPlayerStatsModule *statsModule = pGameRules->GetPlayerStatsModule())
							{
								const SGameRulesPlayerStat *stats = statsModule->GetPlayerStats(localPlayerId);
								if (stats->deaths <= 0) // Not died ever
								{
									if (m_startMatchString.empty() == false)
									{
										const char* gamemodeName = g_pGame->GetGameRules()->GetEntity()->GetClass()->GetName();

										CryFixedStringT<32> strSignalName;
										strSignalName.Format("StartGame%s", gamemodeName);
										TAudioSignalID signalId = g_pGame->GetGameAudio()->GetSignalID(strSignalName);

										CryFixedStringT<64> localisedStartString = CHUDUtils::LocalizeString( m_startMatchString.c_str() );

										if (bTeamGame)
										{
											CryFixedStringT<16> strTeamName;
											strTeamName.Format("@ui_hud_team_%d", teamId);
											
											SHUDEventWrapper::TeamMessage(strTeamName.c_str(), teamId, SHUDEventWrapper::SMsgAudio(signalId), false, true);
											SHUDEventWrapper::SimpleBannerMessage(localisedStartString.c_str(), SHUDEventWrapper::kMsgAudioNULL);
										}
										else
										{
											SHUDEventWrapper::RoundMessageNotify(localisedStartString.c_str(), SHUDEventWrapper::SMsgAudio(signalId));
										}
									}
								}
							}

							m_introMessageShown = true; // Or not if has already died, but don't check again anyway.
						}
					}
				}
			}
		}
		else if(m_state == EGRS_PostGame && !gEnv->IsDedicated())
		{
			if (!gEnv->bServer)
			{
				m_timeInPostGame += frameTime;
			}

			m_timeInCurrentPostGameState += frameTime;

			switch (m_postGameState)
			{
				case ePGS_Starting:
				{
					CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
					if (pRecordingSystem != NULL && (pRecordingSystem->IsPlayingBack() || pRecordingSystem->IsPlaybackQueued()))
					{
						// Currently showing a killcam, skip to the killcam stage of the postgame flow
						EnterPostGameState(ePGS_FinalKillcam);
					}
					else
					{
						if (m_pGameRules->GetRoundsModule())
						{
							EnterPostGameState(ePGS_Top3);
						}
						else
						{
							EnterPostGameState(ePGS_HudMessage);
						}
					}
					break;
				}
				case ePGS_HudMessage:
				{
					if (m_timeInCurrentPostGameState > g_pGameCVars->g_gameRules_postGame_HUDMessageTime)
					{
						EnterPostGameState(ePGS_FinalKillcam);
					}
					break;
				}
				case ePGS_FinalKillcam:
				{
					CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
					if (!pRecordingSystem || !(pRecordingSystem->IsPlayingBack() || (pRecordingSystem->IsPlaybackQueued() && pRecordingSystem->HasWinningKillcam())))
					{
						EnterPostGameState(ePGS_Top3);
					}
#ifndef _RELEASE
					else if(g_pGameCVars->g_hud_postgame_debug && pRecordingSystem)
					{
						CryWatch("PostGameState - Waiting for final killcam to end:"); 
						CryWatch("IsPlaybackQueued: %s, IsPlayingBack: %s", 
							pRecordingSystem->IsPlaybackQueued() ? "True" : "False", pRecordingSystem->IsPlayingBack() ? "True" : "False");
					}
#endif
					break;
				}
				case ePGS_HighlightReel:
				{
					CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
					if (!pRecordingSystem || (!pRecordingSystem->IsPlaybackQueued() && !pRecordingSystem->IsPlayingBack()))
					{
						CGameLobby *pGameLobby = g_pGame->GetGameLobby();
						if (pGameLobby)
						{
							CryLog("[GameRules] Client trying to return to lobby");
							pGameLobby->SvFinishedGame(frameTime);
							EnterPostGameState(ePGS_LeavingGame);
						}
					}
#ifndef _RELEASE
					else if(g_pGameCVars->g_hud_postgame_debug && pRecordingSystem)
					{
						CryWatch("PostGameState - Waiting for highlight reel to end:");
						CryWatch("IsPlaybackQueued: %s, IsPlayingBack: %s, IsPlayingHighlightsReel: %s", 
							pRecordingSystem->IsPlaybackQueued() ? "True" : "False", pRecordingSystem->IsPlayingBack() ? "True" : "False", pRecordingSystem->IsPlayingHighlightsReel() ? "True" : "False");
					}
#endif
					break;
				}
				case ePGS_Top3:
				{
					if (m_timeInCurrentPostGameState > g_pGameCVars->g_gameRules_postGame_Top3Time)
					{
						EnterPostGameState(ePGS_Scoreboard);
					}
					break;
				}
				case ePGS_Scoreboard:
				{
					if(m_timeInCurrentPostGameState > g_pGameCVars->g_gameRules_postGame_ScoreboardTime)
					{
						if(!m_bHasShownHighlightReel && g_pGameCVars->kc_showHighlightsAtEndOfGame)
						{
							EnterPostGameState(ePGS_HighlightReel);
						}
						else
						{
							CGameLobby *pGameLobby = g_pGame->GetGameLobby();
							if (pGameLobby)
							{
								CryLog("[GameRules] Client trying to return to lobby [No highlight reel]");
								pGameLobby->SvFinishedGame(frameTime);
							}
						}
					}
					break;
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardState::EnterPostGameState( EPostGameState state )
{
#ifndef _RELEASE
	if(g_pGameCVars->g_hud_postgame_debug)
	{
		CryLog("HUD PostGame - Entering new state %i from %i after %.2f seconds", (int)state, (int)m_postGameState, m_timeInCurrentPostGameState);
	}
#endif

	m_timeInCurrentPostGameState = 0.f;
	m_postGameState = state;

	switch (m_postGameState)
	{
		case ePGS_FinalKillcam:
		{
			if (g_pGameCVars->kc_enableWinningKill)
			{
				CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
				if (pRecordingSystem)
				{
					if(pRecordingSystem->IsPlaybackQueued())
					{
						if(!pRecordingSystem->PlayWinningKillcam())
						{
							CRY_ASSERT_MESSAGE(false, "There should be a Winning Kill queued up when we get here...");
						}
					}
					else
					{
						CRY_ASSERT_MESSAGE(false, "There should be a Winning Kill queued up when we get here...");
					}
				}
			}
			break;
		}
		case ePGS_HighlightReel:
		{
			bool canPlayHighlightReel = false;

			//Reset to standard style
			SHUDEvent postEvent(eHUDEvent_ResetHUDStyle);
			CHUDEventDispatcher::CallEvent(postEvent);

			CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
			if (pRecordingSystem)
			{
				if (!(pRecordingSystem->IsPlayingBack() || pRecordingSystem->IsPlaybackQueued()))
				{
					canPlayHighlightReel = pRecordingSystem->PlayAllHighlights(false);
#ifndef _RELEASE
					if(g_pGameCVars->g_hud_postgame_debug)
					{
						CryLog("HUD PostGame - Could play highlight reel = %s", canPlayHighlightReel ? "True" : "False");
					}
#endif
				}
			}

			//Change to fixed spectator mode
			IActor* pLocalActor = g_pGame->GetIGameFramework()->GetClientActor();
			IGameRulesSpectatorModule* pSpecMod = g_pGame->GetGameRules()->GetSpectatorModule();
			EntityId specEntity = 0;
			if(pLocalActor && pSpecMod && pSpecMod->ModeIsAvailable(pLocalActor->GetEntityId(), CActor::eASM_Fixed, &specEntity))
			{
				pSpecMod->ChangeSpectatorMode(pLocalActor, CActor::eASM_Fixed, specEntity, true, true);
			}
#ifndef _RELEASE
			else if(g_pGameCVars->g_hud_postgame_debug)
			{
				CryLog("HUD PostGame - Failed to change spectator mode when entering highlight reel");
			}
#endif

			if(canPlayHighlightReel)
			{
				m_bHasShownHighlightReel = true;
			}
			break;
		}
		case ePGS_Top3:
		{
			g_pGame->GetUI()->ActivateDefaultState();

			SHUDEvent scoreboardVisibleEvent( eHUDEvent_MakeMatchEndScoreboardVisible );
			scoreboardVisibleEvent.AddData(true);
			CHUDEventDispatcher::CallEvent(scoreboardVisibleEvent);

			break;
		}
		case ePGS_Scoreboard:
		{
			CryLog("HUD PostGame - Activating default state for scoreboard");
			// Activating the default state while in this postgame state will cause the scoreboard to show
			g_pGame->GetUI()->ActivateDefaultState();
			break;
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardState::CleanUpAbortedIntro()
{
	CGameRules::TPlayers players; 
	m_pGameRules->GetPlayers(players);
	IActorSystem* pActorSystem = g_pGame->GetIGameFramework()->GetIActorSystem();
	CGameRules::TPlayers::const_iterator iter = players.begin();
	CGameRules::TPlayers::const_iterator end = players.end();
	while(iter != end)
	{
		CPlayer* pPlayer = static_cast<CPlayer*>( pActorSystem->GetActor( *iter ) );
		if(pPlayer)
		{
			pPlayer->OnIntroSequenceFinished(); 
		}
		++iter; 
	}
}

//-------------------------------------------------------------------------
bool CGameRulesStandardState::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	if (aspect == STANDARD_STATE_ASPECT)
	{
		EGR_GameState state = m_state;
		ser.EnumValue("state", state, EGRS_Reset, EGRS_MAX);

		if (ser.IsReading())
		{
			m_lastReceivedServerState = state;

			if(!m_pGameRules->IsIntroSequenceCurrentlyPlaying())
			{
				bool bApplyChange = false;

				if(CPlayer * pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetClientActor()))
				{
					bApplyChange = !pPlayer->ShouldPlayIntro();
				}
				 
				if(bApplyChange)
				{
					if (m_state != m_lastReceivedServerState)
					{
						bool bNeedsCleanUpIntro = false;
						// If we are no longer going to play the intro, cleanup!. Players may not have registered with the HUD etc. Make sure they get a chance to. 
						if(m_state == EGRS_Intro && m_pGameRules->IsIntroSequenceRegistered())
						{
							bNeedsCleanUpIntro = true; 
						}

						CryLog("CGameRulesStandardState::NetSerialize(), state changed from %i to %i", (int)m_state, (int)m_lastReceivedServerState);
						ChangeState(m_lastReceivedServerState);

						if (bNeedsCleanUpIntro)
						{
							CleanUpAbortedIntro();
						}
					}
				}
			}
		}
	}

	return true;
}

//-------------------------------------------------------------------------
// called when the CGameRules is Restart()-ing
void CGameRulesStandardState::OnGameRestart()
{
	ChangeState(EGRS_Reset);
}

//-------------------------------------------------------------------------
void CGameRulesStandardState::OnGameReset()
{
	if (gEnv->bServer)
	{

		CryLog("CGameRulesStandardState::OnGameReset(), setting state to InGame");
		ChangeState(EGRS_InGame);
	}
	else
	{
		// Predict that we're going to end up in InGame
		if (m_state != EGRS_InGame)
		{
			ChangeState(EGRS_InGame);
			m_pGameRules->ResetGameTime();
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardState::OnGameEnd()
{
	if (gEnv->bServer)
	{
		ChangeState(EGRS_PostGame);
	}
}

//------------------------------------------------------------------------
void CGameRulesStandardState::OnGameStart_NotifyListeners()
{
  if (m_listeners.empty() == false)
  {
    TGameRulesStateListenersVec::iterator  iter = m_listeners.begin();
    while (iter != m_listeners.end())
    {
      (*iter)->OnGameStart();
      ++iter;
    }
  }
}

//------------------------------------------------------------------------
void CGameRulesStandardState::OnStateEntered_NotifyListeners()
{
	if (m_listeners.empty() == false)
	{
		TGameRulesStateListenersVec::iterator  iter = m_listeners.begin();
		while (iter != m_listeners.end())
		{
			(*iter)->OnStateEntered(m_state);
			++iter;
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesStandardState::OnGameEnd_NotifyListeners()
{
  if (m_listeners.empty() == false)
  {
    TGameRulesStateListenersVec::iterator  iter = m_listeners.begin();
    while (iter != m_listeners.end())
    {
      (*iter)->OnGameEnd();
      ++iter;
    }
  }
}

//------------------------------------------------------------------------
void CGameRulesStandardState::OnIntroStart_NotifyListeners()
{
	if (m_listeners.empty() == false)
	{
		TGameRulesStateListenersVec::iterator  iter = m_listeners.begin();
		while (iter != m_listeners.end())
		{
			(*iter)->OnIntroStart();
			++iter;
		}
	}
	
	m_bHaveNotifiedIntroListeners = true;
}

//-------------------------------------------------------------------------
void CGameRulesStandardState::AddListener( IGameRulesStateListener *pListener )
{
  if (!stl::find(m_listeners, pListener))
  {
    m_listeners.push_back(pListener);
  }
}

//-------------------------------------------------------------------------
void CGameRulesStandardState::RemoveListener( IGameRulesStateListener *pListener )
{
  stl::find_and_erase(m_listeners, pListener);
}

//-------------------------------------------------------------------------
void CGameRulesStandardState::ChangeState( EGR_GameState newState )
{
	if (gEnv->bServer)
	{
		if (newState == EGRS_InGame)
		{
			CCCPOINT(GameRulesStandardState_EnterInGame);

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
			if (m_state != EGRS_InGame)
			{
				if (g_pGameCVars->g_gameRules_startCmd[0] != '\0')
				{
					gEnv->pConsole->ExecuteString(g_pGameCVars->g_gameRules_startCmd, false, true);
				}
			}
#endif // #if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)

			m_pGameRules->ResetGameTime();
			IGameRulesStatsRecording	*st=m_pGameRules->GetStatsRecordingModule();
			if (st)
			{
				st->OnInGameBegin();
			}
		}
		else if (newState == EGRS_PostGame)
		{
			m_timeInPostGame = 0.0f;
			IGameRulesStatsRecording	*st=m_pGameRules->GetStatsRecordingModule();
			if (st)
			{
				st->OnPostGameBegin();
			}

			CGameLobby *pGameLobby = g_pGame->GetGameLobby();
			if (pGameLobby)
			{
				CryLog("[GameRules] GameFinished, telling clients");
				pGameLobby->SvFinishedGame(0.0f);
			}
		}
	}

	const bool bIsClient = gEnv->IsClient();
	if (bIsClient)
	{
		if (newState == EGRS_PostGame)
		{
			m_timeInPostGame = 0.0f;

			EnterPostGameState(ePGS_Starting);
		}
		else if (newState == EGRS_InGame)
		{
			CHUDEventDispatcher::CallEvent( SHUDEvent(eHUDEvent_OnGameStart) );
		}
		else if (newState == EGRS_Reset)
		{
			m_introMessageShown = false;
		}
	}

	m_state = newState;

	OnStateEntered_NotifyListeners(); 

	if (newState == EGRS_PostGame)
	{
		m_pGameRules->FreezeInput(true);
		g_pGame->GetUI()->ActivateDefaultState();	// must be after settings newState to pick endgame
	
		OnGameEnd_NotifyListeners();
	}
	else if (newState == EGRS_InGame)
	{
		OnGameStart_NotifyListeners();

		if (bIsClient && g_pGameCVars->g_gameRules_preGame_StartSpawnedFrozen)
		{
			g_pGameActions->FilterMPPreGameFreeze()->Enable(false);
			g_pGame->GetUI()->ActivateDefaultState();	// must be after settings newState
		}

		// Have to explicitly call the spawning module, can't use the listener since that would make the modules
		// dependent on the initialisation order - which comes from xml
		IGameRulesSpawningModule *pSpawningModule = m_pGameRules->GetSpawningModule();
		if (pSpawningModule)
		{
			pSpawningModule->OnInGameBegin();
		}
	}

	if (gEnv->bServer)
	{
		CHANGED_NETWORK_STATE(m_pGameRules, STANDARD_STATE_ASPECT);

		if (m_state == EGRS_InGame)
		{
			// Revive players - has to be done after setting m_state since we don't revive players in PreGame
			IGameRulesSpawningModule *pSpawningModule = m_pGameRules->GetSpawningModule();
			if (pSpawningModule)
			{
				const bool bOnlyIfDead = (g_pGameCVars->g_gameRules_preGame_StartSpawnedFrozen!=0);
				pSpawningModule->ReviveAllPlayers(false, bOnlyIfDead);
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardState::CmdSetState( IConsoleCmdArgs *pArgs )
{
	if (gEnv->bServer)
	{
		const char *pState = pArgs->GetArg(1);
		if (pState)
		{
			int state = atoi(pState);
			if (state >= 0 && state < (int) EGRS_MAX)
			{
				EGR_GameState requestedState = (EGR_GameState) state;

				CGameRulesStandardState *pStateModule = static_cast<CGameRulesStandardState *>(g_pGame->GetGameRules()->GetStateModule());
				if (pStateModule)
				{
					pStateModule->ChangeState(requestedState);
				}
			}
		}
	}
}

bool CGameRulesStandardState::IsInitialChannelId( int channelId ) const
{
	unsigned int initialChannelsSize = m_initialChannelIds.size();
	for(unsigned int i = 0; i < initialChannelsSize; ++i)
	{
		if(m_initialChannelIds[i] == channelId)
		{
			return true;
		}
	}

	return false;
}

bool CGameRulesStandardState::IsOkToStartRound() const
{
	if (m_state != EGRS_PreGame && 
		  m_state != EGRS_Intro)
	{
		return true;
	}
	return false;
}

void CGameRulesStandardState::OnIntroCompleted()
{
	if(gEnv->bServer)
	{
		ChangeState(EGRS_PreGame);
	}
	else
	{
		// If we are a client look at the last cached game state from net serialise and switch to match that.
		CryLog("CGameRulesStandardState::OnIntroCompleted(), state changed from %i to %i", (int)m_state, (int)m_lastReceivedServerState);
		ChangeState(m_lastReceivedServerState);
	}
}

