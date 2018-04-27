// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 26:04:2007   17.47: Created by Steve Humphreys

*************************************************************************/

#include "StdAfx.h"

#include "GameRules.h"
#include "GameRulesModules/IGameRulesRoundsListener.h"
#include "GameRulesModules/IGameRulesStateModule.h"
#include "GameRulesModules/IGameRulesRoundsModule.h"
#include "GameRulesModules/IGameRulesObjectivesModule.h"
#include "GameRulesModules/IGameRulesRevivedListener.h"
#include "GameCVars.h"
#include "Player.h"
#include "MPTrackViewManager.h"
#include "MPPathFollowingManager.h"
#include "EnvironmentalWeapon.h"
#include "GameRulesModules/IGameRulesSpectatorModule.h"
#include "GameRulesModules/IGameRulesSpawningModule.h"
#include "Effects/GameEffects/ParameterGameEffect.h"
#include "RecordingSystem.h"
#include "Effects/GameEffects/KillCamGameEffect.h"
#include "LocalPlayerComponent.h"
#include "GameActions.h"

#if USE_PC_PREMATCH
	#include "GameRulesModules/IGameRulesPrematchListener.h"
#endif
#include "PlaylistManager.h"

#include <CryFlowGraph/IFlowBaseNode.h>
#include <IVehicleSystem.h>

class CFlowNode_MP : public CFlowBaseNode<eNCT_Instanced>, public SGameRulesListener
{
	enum INPUTS
	{
		EIP_GameEndTime = 0,
	};

	enum OUTPUTS
	{
		//EOP_GameStarted,
		EOP_EnteredGame,
		EOP_EndGameNear,
		EOP_EndGameInvalid,
		EOP_GameWon,
		EOP_GameLost,
		EOP_GameTied,
	};

public:
	CFlowNode_MP(SActivationInfo* pActInfo)
	{
		if (pActInfo && pActInfo->pGraph)
		{
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
		}

		m_localPlayerSpectatorMode = 0;
		m_endGameNear = false;
		m_timeRemainingTriggered = true;    // default to true so it won't be triggered again until player enters game.
	}

	~CFlowNode_MP()
	{
		CGameRules* pGameRules = g_pGame->GetGameRules();
		if (pGameRules)
			pGameRules->RemoveGameRulesListener(this);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_MP(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig<float>("GameEndTime", _HELP("Number of seconds remaining at which to trigger the EndGameNear output")),
			{ 0 }
		};
		static const SOutputPortConfig out_ports[] =
		{
			//OutputPortConfig_Void( "GameStarted", _HELP("Triggered when MP game begins")),
			OutputPortConfig_Void("EnteredGame",    _HELP("Triggered when local player enters the game")),
			OutputPortConfig_Void("EndGameNear",    _HELP("Triggered when game-ending condition is near")),
			OutputPortConfig_Void("EndGameInvalid", _HELP("Triggered when game-ending condition is invalidated")),
			OutputPortConfig_Void("GameWon",        _HELP("Triggered when local player's team wins the game")),
			OutputPortConfig_Void("GameLost",       _HELP("Triggered when local player's team loses the game")),
			OutputPortConfig_Void("GameTied",       _HELP("Triggered when neither team wins the game")),
			{ 0 }
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Checks various Multiplayer states and conditions");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				CGameRules* pGameRules = g_pGame->GetGameRules();
				if (pGameRules)
					pGameRules->AddGameRulesListener(this);

				CPlayer* pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetClientActor());
				if (pPlayer)
					m_localPlayerSpectatorMode = pPlayer->GetSpectatorMode();

				m_actInfo = *pActInfo;
			}
			break;
		case eFE_Activate:
			break;
		case eFE_Update:
			{
				CPlayer* pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetClientActor());
				if (!pPlayer)
					return;

				// first check: tac weapons trigger endgamenear / endgameinvalid
				if (m_MDList.empty() && m_endGameNear)
				{
					// if less than 3 min remaining don't return to normal
					bool cancel = true;
					if (g_pGame && g_pGame->GetGameRules() && g_pGame->GetGameRules()->IsTimeLimited() && m_timeRemainingTriggered)
					{
						float timeRemaining = g_pGame->GetGameRules()->GetRemainingGameTime();
						if (timeRemaining < GetPortFloat(pActInfo, EIP_GameEndTime))
							cancel = false;
					}

					if (cancel)
					{
						if (g_pGame->GetCVars()->i_debug_mp_flowgraph != 0)
						{
							CryLog("MP flowgraph: EndGameInvalid");
						}
						ActivateOutput(&m_actInfo, EOP_EndGameInvalid, true);
						m_endGameNear = false;
					}
				}
				else if (!m_MDList.empty())
				{
					// go through the list of tac/sing weapons and check if they are still present
					std::list<EntityId>::iterator next;
					std::list<EntityId>::iterator it = m_MDList.begin();
					for (; it != m_MDList.end(); it = next)
					{
						next = it;
						++next;
						if (gEnv->pEntitySystem->GetEntity(*it))
						{
							if (!m_endGameNear)
							{
								if (g_pGame->GetCVars()->i_debug_mp_flowgraph != 0)
								{
									CryLog("--MP flowgraph: EndGameNear");
								}
								ActivateOutput(&m_actInfo, EOP_EndGameNear, true);
								m_endGameNear = true;
							}

							// in the case of tanks, entity isn't removed for quite some time after destruction.
							//	Check vehicle health directly here...
							IVehicle* pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(*it);
							if (pVehicle != NULL && pVehicle->IsDestroyed())
							{
								m_MDList.erase(it);
							}
						}
						else
						{
							// entity no longer exists - remove from list.
							m_MDList.erase(it);
						}
					}
				}

				// get the remaining time from game rules
				if (!m_timeRemainingTriggered && g_pGame->GetGameRules() && g_pGame->GetGameRules()->IsTimeLimited() && pPlayer->GetSpectatorMode() == 0 && !m_endGameNear)
				{
					float timeRemaining = g_pGame->GetGameRules()->GetRemainingGameTime();
					if (timeRemaining < GetPortFloat(pActInfo, EIP_GameEndTime))
					{
						if (g_pGame->GetCVars()->i_debug_mp_flowgraph != 0)
						{
							CryLog("--MP flowgraph: EndGameNear");
						}
						ActivateOutput(&m_actInfo, EOP_EndGameNear, timeRemaining);
						m_timeRemainingTriggered = true;
						m_endGameNear = true;
					}
				}

				// also check whether the local player is in game yet
				if (pPlayer->GetSpectatorMode() == 0 && m_localPlayerSpectatorMode != 0)
				{
					if (g_pGame->GetCVars()->i_debug_mp_flowgraph != 0)
					{
						CryLog("--MP flowgraph: EnteredGame");
					}
					ActivateOutput(&m_actInfo, EOP_EnteredGame, true);
					m_localPlayerSpectatorMode = pPlayer->GetSpectatorMode();
				}
			}
			break;
		}
	}

protected:
	virtual void GameOver(EGameOverType localWinner, bool isClientSpectator)
	{
		switch (localWinner)
		{
		case EGOT_Win:
			if (g_pGame->GetCVars()->i_debug_mp_flowgraph != 0)
			{
				CryLog("--MP flowgraph: GameWon");
			}
			ActivateOutput(&m_actInfo, EOP_GameWon, true);
			break;

		case EGOT_Lose:
			if (g_pGame->GetCVars()->i_debug_mp_flowgraph != 0)
			{
				CryLog("--MP flowgraph: GameLost");
			}
			ActivateOutput(&m_actInfo, EOP_GameLost, true);
			break;

		default:
			if (g_pGame->GetCVars()->i_debug_mp_flowgraph != 0)
			{
				CryLog("--MP flowgraph: GameTied");
			}
			ActivateOutput(&m_actInfo, EOP_GameTied, true);
			break;
		}
	}
	virtual void EnteredGame()
	{
		//      CPlayer* pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetClientActor());
		//      if(pPlayer && pPlayer->GetSpectatorMode() == 0)
		//      {
		//        ActivateOutput(&m_actInfo, EOP_EnteredGame,true);
		//        m_timeRemainingTriggered = false;
		//      }
	}
	virtual void EndGameNear(EntityId id)
	{
		if (id != 0)
		{
			m_MDList.push_back(id);
		}
	}

	SActivationInfo     m_actInfo;
	int                 m_localPlayerSpectatorMode;
	bool                m_endGameNear;
	bool                m_timeRemainingTriggered;

	std::list<EntityId> m_MDList;
};

//----------------------------------------------------------------------------------------

class CFlowNode_MPGameType : public CFlowBaseNode<eNCT_Instanced>
{
	enum INPUTS
	{
		EIP_GetGameMode = 0,
	};

	enum OUTPUTS
	{
		EOP_GameRulesName,
		EOP_SpecificModeStart,
	};

public:
	CFlowNode_MPGameType(SActivationInfo* pActInfo)
	{
	}

	~CFlowNode_MPGameType()
	{
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_MPGameType(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("GetGameType", _HELP("Activate this to retrigger relevent outputs")),
			{ 0 }
		};
		static SOutputPortConfig out_ports[eGM_NUM_GAMEMODES + 2] = { { 0 } };
		static bool bInitialised = false;
		if (!bInitialised)
		{
			bInitialised = true;

			out_ports[EOP_GameRulesName] = OutputPortConfig<string>("GameRulesName", _HELP("Outputs the current game rules name"));

			for (int i = 0; i < eGM_NUM_GAMEMODES; ++i)
			{
				const char* pGameModeName = CGameRules::S_GetGameModeNamesArray()[i];
				if (strstr(pGameModeName, "eGM_") == pGameModeName)
				{
					pGameModeName += 4;
				}

				out_ports[EOP_SpecificModeStart + i] = OutputPortConfig_Void(pGameModeName, _HELP("Triggered on level load if game rules are named type"));
			}
		}
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Obtain Game Type information");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_actInfo = *pActInfo;
			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_GetGameMode))
				{
					TriggerGameTypeOutput();
				}
			}
			break;
		}
	}

	void TriggerGameTypeOutput()
	{
		CGameRules* pGameRules = g_pGame->GetGameRules();
		if (pGameRules)
		{
			ActivateOutput(&m_actInfo, (int)pGameRules->GetGameMode() + EOP_SpecificModeStart, true);

			// output the name as well (for supporting any additional modes that might be added)
			const char* gameRulesName = pGameRules->GetEntity()->GetClass()->GetName();
			ActivateOutput(&m_actInfo, EOP_GameRulesName, string(gameRulesName));
		}
	}

protected:
	SActivationInfo m_actInfo;
};

//////////////////////////////////////////////////////////////////////////
// Flow node for getting round start / end events.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_RoundTrigger : public CFlowBaseNode<eNCT_Instanced>, public IGameRulesRoundsListener, public IGameRulesStateListener
{
public:
	CFlowNode_RoundTrigger(SActivationInfo* pActInfo) : CFlowBaseNode<eNCT_Instanced>()
	{
		m_startTime = 0.0f;
		m_roundStart = false;
		m_suddenDeath = false;
		m_roundEnd = false;
	}

	virtual ~CFlowNode_RoundTrigger()
	{
		CGameRules* pGameRules = g_pGame->GetGameRules();
		if (pGameRules)
		{
			pGameRules->UnRegisterRoundsListener(this);
			IGameRulesStateModule* pState = pGameRules->GetStateModule();
			if (pState)
				pState->RemoveListener(this);
		}
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_RoundTrigger(pActInfo);
	}

	enum INPUTS
	{
		IN_DELAY = 0,
	};

	enum OUTPUTS
	{
		OUT_BEGIN = 0,
		OUT_SUDDENDEATH,
		OUT_END,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("DelayBegin", 0.0f, _HELP("Delay begin trigger in Seconds")),
			{ 0 }
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Begin",       _HELP("Triggered when a round begins")),
			OutputPortConfig_AnyType("SuddenDeath", _HELP("Triggered when a sudden death begins")),
			OutputPortConfig_AnyType("End",         _HELP("Triggered when a round ends")),
			{ 0 }
		};

		config.sDescription = _HELP("Game Round Triggers");
		config.pOutputPorts = out_config;
		config.pInputPorts = in_config;
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Initialize)
		{
			CGameRules* pGameRules = g_pGame->GetGameRules();
			if (pGameRules)
			{
				pGameRules->RegisterRoundsListener(this);
				IGameRulesStateModule* pState = pGameRules->GetStateModule();
				if (pState)
					pState->AddListener(this);
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
			}
		}
		if (event == eFE_Update)
		{
			if (m_roundStart)
			{
				float curTime = gEnv->pTimer->GetCurrTime();
				if ((curTime - m_startTime) > GetPortFloat(pActInfo, IN_DELAY))
				{
					ActivateOutput(pActInfo, OUT_BEGIN, true);
					m_roundStart = false;
				}
			}

			if (m_suddenDeath)
			{
				ActivateOutput(pActInfo, OUT_SUDDENDEATH, true);
				m_suddenDeath = false;
			}

			if (m_roundEnd)
			{
				ActivateOutput(pActInfo, OUT_END, true);
				m_roundEnd = false;
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	//round listener interfaces
	virtual void OnRoundStart()
	{
		m_startTime = gEnv->pTimer->GetCurrTime();
		m_roundStart = true;
	}

	virtual void OnRoundEnd()
	{
		m_roundEnd = true;
	}

	virtual void OnSuddenDeath()
	{
		m_suddenDeath = true;
	}

	//game listener interfaces
	virtual void OnGameStart()
	{
		m_startTime = gEnv->pTimer->GetCurrTime();
		m_roundStart = true;
	}

	virtual void OnGameEnd()
	{
		m_roundEnd = true;
	}

	virtual void OnStateEntered(IGameRulesStateModule::EGR_GameState newGameState) {};

	virtual void OnIntroStart()                                                    {}

	virtual void ClRoundsNetSerializeReadState(int newState, int curState)
	{

	}

	virtual void OnRoundAboutToStart() {}

private:

	bool  m_roundStart;
	bool  m_suddenDeath;
	bool  m_roundEnd;
	float m_startTime;
};

class CFlowNode_IsServer : public CFlowBaseNode<eNCT_Instanced>
{
	enum INPUTS
	{
		EIP_CheckIsServer = 0,
	};

	enum OUTPUTS
	{
		EOP_True = 0,
		EOP_False,
	};

public:
	CFlowNode_IsServer(SActivationInfo* pActInfo)
	{
	}

	~CFlowNode_IsServer()
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_IsServer(pActInfo);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("IsServerCheck", _HELP("Perform a test to see whether the current user is the server")),
			{ 0 }
		};
		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_Void("True",  _HELP("The user is the server")),
			OutputPortConfig_Void("False", _HELP("The user is not the server")),
			{ 0 }
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Multiplayer check to see if the current user is the server");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_actInfo = *pActInfo;
			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_CheckIsServer))
				{
					if (gEnv->bServer)
					{
						ActivateOutput(&m_actInfo, EOP_True, true);
					}
					else
					{
						ActivateOutput(&m_actInfo, EOP_False, true);
					}
				}
			}
			break;
		}
	}

protected:
	SActivationInfo m_actInfo;
};

//Flow graph node for multiplayer clients to request the server begins an animation sequence
//The track view manager will then ensure all clients start the animation
class CRequestPlaySequence_Node : public CFlowBaseNode<eNCT_Instanced>, public IMovieListener
{
	enum INPUTS
	{
		EIP_Request,
		EIP_Listen,
		EIP_SequenceName,
	};

	enum OUTPUTS
	{
		EOP_Requested,
		EOP_Started,
		EOP_Finished,
	};

public:
	CRequestPlaySequence_Node(SActivationInfo* pActInfo)
	{
		m_pSequence = NULL;
	}

	~CRequestPlaySequence_Node()
	{
		if (m_pSequence)
		{
			IMovieSystem* pMovieSystem = gEnv->pMovieSystem;
			if (pMovieSystem)
			{
				pMovieSystem->RemoveMovieListener(m_pSequence, this);
			}
			m_pSequence = NULL;
		}
	};

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CRequestPlaySequence_Node(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Request",              _HELP("Sends a request to the server to start the sequence (Or starts it immediately if called on the server)"), _HELP("Request")),
			InputPortConfig_Void("Listen",               _HELP("Listen for a sequence starting/finishing"),                                                               _HELP("Listen")),
			InputPortConfig<string>("seq_Sequence_File", _HELP("Name of the Sequence"),                                                                                   _HELP("Sequence")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Requested", _HELP("Triggered when the request is sent")),
			OutputPortConfig_Void("Started",   _HELP("Triggered when the requested animation starts playing")),
			OutputPortConfig_Void("Finished",  _HELP("Triggered when the request animation finishes playing")),
			{ 0 }
		};
		config.sDescription = _HELP("Requests a trackview sequence to be started by the server");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_ActInfo = *pActInfo;
			}
			break;

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Request))
				{
					const string& sequenceName = GetPortString(pActInfo, EIP_SequenceName);
					CryHashStringId id(sequenceName);
					CGameRules* pGameRules = NULL;
					if ((gEnv->bMultiplayer || gEnv->IsEditor()) && id.id && (pGameRules = g_pGame->GetGameRules()))
					{
						CMPTrackViewManager* pMPTrackViewManager = pGameRules->GetMPTrackViewManager();

						IAnimSequence* pSequence = pMPTrackViewManager->FindTrackviewSequence(id.id);
						if (pSequence)
						{
							IMovieSystem* pMovieSystem = gEnv->pMovieSystem;

							UpdateTrackedSequence(pSequence);

							CGameRules::STrackViewRequestParameters params;
							params.m_TrackViewID = id.id;
							if (gEnv->bServer)
							{
								pGameRules->GetMPTrackViewManager()->AnimationRequested(params);
							}
							else
							{
								pGameRules->GetGameObject()->InvokeRMI(CGameRules::SvTrackViewRequestAnimation(), params, eRMI_ToServer);
							}

							ActivateOutput(pActInfo, EOP_Requested, true);
						}
					}
				}
				else if (IsPortActive(pActInfo, EIP_Listen))
				{
					const string& sequenceName = GetPortString(pActInfo, EIP_SequenceName);
					CryHashStringId id(sequenceName);
					CGameRules* pGameRules = NULL;
					if ((gEnv->bMultiplayer || gEnv->IsEditor()) && id.id && (pGameRules = g_pGame->GetGameRules()))
					{
						CMPTrackViewManager* pMPTrackViewManager = pGameRules->GetMPTrackViewManager();

						IAnimSequence* pSequence = pMPTrackViewManager->FindTrackviewSequence(id.id);
						if (pSequence)
						{
							UpdateTrackedSequence(pSequence);
						}

						//Check to see if the sequence was played before we even joined
						if (!gEnv->bServer)
						{
							if (pMPTrackViewManager->HasTrackviewFinished(id))
							{
								ActivateOutput(&m_ActInfo, EOP_Started, true);
								ActivateOutput(&m_ActInfo, EOP_Finished, true);
							}
						}
					}
				}
			}
			break;
		}
	}

	void UpdateTrackedSequence(IAnimSequence* pSequence)
	{
		IMovieSystem* pMovieSystem = gEnv->pMovieSystem;

		if (m_pSequence)
		{
			if (pSequence == m_pSequence)
			{
				//Don't allow multiple requests for the same sequence (If it becomes a problem we can use a timer to stop spamming but its a reliable message so should be fine)
				return;
			}
			else
			{
				//We're now connecting a different sequence to the node so first remove the old listener
				pMovieSystem->RemoveMovieListener(m_pSequence, this);
			}
		}

		m_pSequence = pSequence;

		pMovieSystem->AddMovieListener(pSequence, this);
	}

	virtual void OnMovieEvent(IMovieListener::EMovieEvent event, IAnimSequence* pSequence)
	{
		IMovieSystem* pMovieSystem = gEnv->pMovieSystem;

		if (event == IMovieListener::eMovieEvent_Started)
		{
			ActivateOutput(&m_ActInfo, EOP_Started, true);
		}
		if (event == IMovieListener::eMovieEvent_Stopped || event == IMovieListener::eMovieEvent_Aborted)
		{
			ActivateOutput(&m_ActInfo, EOP_Finished, true);
			pMovieSystem->RemoveMovieListener(pSequence, this);
			m_pSequence = NULL;
		}
	}

private:
	SActivationInfo m_ActInfo;
	IAnimSequence*  m_pSequence;
};

class CMPAttachToPath_Node : public CFlowBaseNode<eNCT_Instanced>, public IMPPathFollowingListener
{
	enum INPUTS
	{
		EIP_Attach,
		EIP_StartAtInitialNode,
		EIP_Loop,
		EIP_PathId,
		EIP_Speed,
	};

	enum OUTPUTS
	{
		EOP_Started,
		EOP_Finished,
	};

public:
	CMPAttachToPath_Node(SActivationInfo* pActInfo) : m_EntityId(0), m_ListeningForFinishedCallback(false)
	{
	}

	~CMPAttachToPath_Node()
	{
		CGameRules* pGameRules = NULL;
		if (m_ListeningForFinishedCallback && m_EntityId && (pGameRules = g_pGame->GetGameRules()))
		{
			if (CMPPathFollowingManager* pMPPathFollowingManager = pGameRules->GetMPPathFollowingManager())
			{
				pMPPathFollowingManager->UnregisterListener(m_EntityId);
			}
		}
	};

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CMPAttachToPath_Node(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Attach",              _HELP("Request an entity be attached to a path"),                                                                                                                         _HELP("Attach")),
			InputPortConfig<bool>("StartAtInitialNode", _HELP("If true the entity should teleport to the initial node; otherwise it should travel from its current position to the first node"),                                  _HELP("StartAtInitialNode")),
			InputPortConfig<bool>("Loop",               _HELP("If true the entity should travel back to the initial node once reaching the final one; otherwise the Finished output will trigger once the last node is reached"), _HELP("Loop")),
			InputPortConfig<EntityId>("PathId",         _HELP("The path entity ID to attach to"),                                                                                                                                 _HELP("Path")),
			InputPortConfig<float>("Speed",             _HELP("The speed the entity shold travel at"),                                                                                                                            _HELP("Speed")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Started",  _HELP("Triggered immediately when the attach call is made")),
			OutputPortConfig_Void("Finished", _HELP("Triggered when a non-looped entity reaches the end of the path")),
			{ 0 }
		};
		config.sDescription = _HELP("Requests an entity be attached to a path. The entity target should be the follower entity");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_ActInfo = *pActInfo;
			}
			break;

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Attach))
				{
					const bool startAtInitialNode = GetPortBool(pActInfo, EIP_StartAtInitialNode);
					const bool loop = GetPortBool(pActInfo, EIP_Loop);
					const EntityId pathId = GetPortEntityId(pActInfo, EIP_PathId);
					const float speed = GetPortFloat(pActInfo, EIP_Speed);
					CGameRules* pGameRules = NULL;
					if (gEnv->bMultiplayer && m_EntityId && pathId && (pGameRules = g_pGame->GetGameRules()))
					{
						IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId);
						if (pEntity)
						{
							CMPPathFollowingManager* pMPPathFollowingManager = pGameRules->GetMPPathFollowingManager();
							uint16 classId;
							g_pGame->GetIGameFramework()->GetNetworkSafeClassId(classId, pEntity->GetClass()->GetName());

							IMPPathFollower::MPPathIndex index = 0;
							pMPPathFollowingManager->GetPath(pathId, index);
							SPathFollowingAttachToPathParameters params(classId, m_EntityId, index, startAtInitialNode, loop, speed, speed, -1, -1, 0.f, false);
							pMPPathFollowingManager->RequestAttachEntityToPath(params);
							ActivateOutput(&m_ActInfo, EOP_Started, true);

							if (!loop)
							{
								pMPPathFollowingManager->RegisterListener(m_EntityId, this);
								m_ListeningForFinishedCallback = true;
							}
						}
					}
				}
				else if (IsPortActive(pActInfo, EIP_Speed))
				{
					if (gEnv->bMultiplayer && m_EntityId)
					{
						CGameRules* pGameRules = NULL;
						IEntity* pEntity = gEnv->bMultiplayer ? gEnv->pEntitySystem->GetEntity(m_EntityId) : NULL;
						if (pEntity && (pGameRules = g_pGame->GetGameRules()))
						{
							CMPPathFollowingManager* pMPPathFollowingManager = pGameRules->GetMPPathFollowingManager();
							uint16 classId;
							g_pGame->GetIGameFramework()->GetNetworkSafeClassId(classId, pEntity->GetClass()->GetName());
							const float speed = GetPortFloat(pActInfo, EIP_Speed);

							pMPPathFollowingManager->RequestUpdateSpeed(classId, m_EntityId, speed);
						}
					}
				}
			}
			break;
		case eFE_SetEntityId:
			{
				if (pActInfo->pEntity)
				{
					EntityId newId = pActInfo->pEntity->GetId();

					//Handle the entity being changed while the listener is active
					if (m_ListeningForFinishedCallback && m_EntityId && newId != m_EntityId)
					{
						g_pGame->GetGameRules()->GetMPPathFollowingManager()->UnregisterListener(m_EntityId);
						m_ListeningForFinishedCallback = false;
					}

					m_EntityId = newId;
				}
			}
			break;
		}
	}

	virtual void OnPathCompleted(EntityId attachedEntityId)
	{
		CRY_ASSERT_MESSAGE(attachedEntityId == m_EntityId, "CMPAttachToPath_Node::OnPathCompleted - Path listener listening to unexpected entity");

		CMPPathFollowingManager* pMPPathFollowingManager = g_pGame->GetGameRules()->GetMPPathFollowingManager();
		pMPPathFollowingManager->UnregisterListener(attachedEntityId);
		m_ListeningForFinishedCallback = false;

		ActivateOutput(&m_ActInfo, EOP_Finished, true);
	}

private:
	SActivationInfo m_ActInfo;
	EntityId        m_EntityId;
	bool            m_ListeningForFinishedCallback;
};

class CMPEnvironmentalWeapon_Node : public CFlowBaseNode<eNCT_Instanced>, public IEnvironmentalWeaponEventListener
{
	enum OUTPUTS
	{
		EOP_OnRipStart,
		EOP_OnRipDetach,
		EOP_OnRipEnd,
	};

public:
	CMPEnvironmentalWeapon_Node(SActivationInfo* pActInfo) : m_EntityId(0)
	{
	}

	~CMPEnvironmentalWeapon_Node()
	{
		if (m_EntityId)
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId);
			if (pEntity)
			{
				CEnvironmentalWeapon* pEnvWeapon = static_cast<CEnvironmentalWeapon*>(g_pGame->GetIGameFramework()->QueryGameObjectExtension(m_EntityId, "EnvironmentalWeapon"));
				if (pEnvWeapon)
				{
					pEnvWeapon->UnregisterListener(this);
				}
			}
		}
	};

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CMPEnvironmentalWeapon_Node(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("OnRipStart",  _HELP("Triggered at the point the environmental weapon rip first begins")),
			OutputPortConfig_Void("OnRipDetach", _HELP("Triggered at the xml defined point (pickandthrowWeapon.xml) that we consider the weapon 'detached'")),
			OutputPortConfig_Void("OnRipEnd",    _HELP("Triggered at the point the environmental weapon ripping is completed")),
			{ 0 }
		};
		config.sDescription = _HELP("Allows listening to key rip events for environmental weapons");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_ActInfo = *pActInfo;
			}
			break;
		case eFE_SetEntityId:
			{
				if (pActInfo->pEntity)
				{
					EntityId newId = pActInfo->pEntity->GetId();

					//Handle the entity being changed while the listener is active
					if (newId != m_EntityId)
					{
						if (m_EntityId)
						{
							CEnvironmentalWeapon* pEnvWeapon = static_cast<CEnvironmentalWeapon*>(g_pGame->GetIGameFramework()->QueryGameObjectExtension(m_EntityId, "EnvironmentalWeapon"));
							if (pEnvWeapon)
							{
								pEnvWeapon->UnregisterListener(this);
							}
						}

						m_EntityId = newId;

						// Register for new
						if (m_EntityId)
						{
							CEnvironmentalWeapon* pEnvWeapon = static_cast<CEnvironmentalWeapon*>(g_pGame->GetIGameFramework()->QueryGameObjectExtension(m_EntityId, "EnvironmentalWeapon"));
							if (pEnvWeapon)
							{
								pEnvWeapon->RegisterListener(this);
							}
						}
					}
				}
			}
			break;
		}
	}

	// Called when Rip anim is started
	virtual void OnRipStart()
	{
		ActivateOutput(&m_ActInfo, EOP_OnRipStart, true);
	}

	// Called at the point within the rip action that the actual 'detach/Unroot' occurs
	virtual void OnRipDetach()
	{
		ActivateOutput(&m_ActInfo, EOP_OnRipDetach, true);
	}

	// Called at the end of the rip anim
	virtual void OnRipEnd()
	{
		ActivateOutput(&m_ActInfo, EOP_OnRipEnd, true);
	}

private:
	SActivationInfo m_ActInfo;
	EntityId        m_EntityId;
};

class CRequestCinematicIntroSequence_Node : public CFlowBaseNode<eNCT_Instanced>, public IMovieListener, public IGameRulesStateListener, public IGameRulesClientConnectionListener, public ISystemEventListener
{
	enum INPUTS
	{
		EIP_Team0SequenceName,
		EIP_Team1SequenceName,
		EIP_Team2SequenceName,

		EIP_Team0CameraPointEntityId,
		EIP_Team1CameraPointEntityId,
		EIP_Team2CameraPointEntityId,

		EIP_Team0AudioSignalName,
		EIP_Team1AudioSignalName,
		EIP_Team2AudioSignalName,

		EIP_InitialSpawnGroup,

		// For easier testing in editor
		EIP_ForceTeam0Sequence,
		EIP_ForceTeam1Sequence,
		EIP_ForceTeam2Sequence,
	};

	enum OUTPUTS
	{
		EOP_Started,
		EOP_Finished,
	};

public:
	CRequestCinematicIntroSequence_Node(SActivationInfo* pActInfo) :
		m_pSequence(NULL),
		m_startFadeToBlackTimeStamp(0.0f)
	{
		if (g_pGameCVars->g_IntroSequencesEnabled)
		{
			CGameRules* pGameRules = g_pGame->GetGameRules();
			if (pGameRules)
			{
				IGameRulesStateModule* pStateModule = pGameRules->GetStateModule();
				if (pStateModule)
				{
					pStateModule->AddListener(this);

					// For now don't support separate code path to position / lock player in editor (trackview can still be played manually to edit/test)
					if (!gEnv->IsEditor())
					{
						IGameRulesObjectivesModule* pObjectives = pGameRules->GetObjectivesModule();
						if ((g_pGameCVars->g_forceIntroSequence) || (pObjectives && pObjectives->CanPlayIntroSequence()))
						{
							// Let the world know there is a valid intro sequence coming.
							pGameRules->SetIntroSequenceRegistered(true);
						}
					}
				}
			}
		}
	}

	~CRequestCinematicIntroSequence_Node()
	{
		if (m_pSequence)
		{
			IMovieSystem* pMovieSystem = gEnv->pMovieSystem;
			if (pMovieSystem)
			{
				pMovieSystem->RemoveMovieListener(m_pSequence, this);
			}
			m_pSequence = NULL;
		}

		CGameRules* pGameRules = g_pGame->GetGameRules();
		if (pGameRules)
		{
			IGameRulesStateModule* pStateModule = pGameRules->GetStateModule();
			if (pStateModule)
			{
				pStateModule->RemoveListener(this);
			}
			pGameRules->UnRegisterClientConnectionListener(this);
		}
		GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);
	};
	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CRequestCinematicIntroSequence_Node(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<string>("team0_sequence_name",            _HELP("The name of the team 0 intro sequence (Non team games)"),               _HELP("team0 Sequence Name")),
			InputPortConfig<string>("team1_sequence_name",            _HELP("The name of the team 1 intro sequence"),                                _HELP("team1 Sequence Name")),
			InputPortConfig<string>("team2_sequence_name",            _HELP("The name of the team 2 intro sequence"),                                _HELP("team2 Sequence Name")),

			InputPortConfig<EntityId>("team0_camera_point_entity_id", _HELP("The entity id of the <CameraPoint> entity to use for the team0 intro"), _HELP("team0 Cam Point")),
			InputPortConfig<EntityId>("team1_camera_point_entity_id", _HELP("The entity id of the <CameraPoint> entity to use for the team1 intro"), _HELP("team1 Cam Point")),
			InputPortConfig<EntityId>("team2_camera_point_entity_id", _HELP("The entity id of the <CameraPoint> entity to use for the team2 intro"), _HELP("team2 Cam Point")),

			InputPortConfig<string>("team0_audio_name",               _HELP("The name of the team 0 audio signal (Non team games)"),                 _HELP("team0 audio signal Name")),
			InputPortConfig<string>("team1_audio_name",               _HELP("The name of the team 1 audio signal "),                                 _HELP("team1 audio signal Name")),
			InputPortConfig<string>("team2_audio_name",               _HELP("The name of the team 2 audio signal "),                                 _HELP("team2 audio signal Name")),

			InputPortConfig<string>("InitialSpawnGroup",              _HELP("The initial spawn group to be used"),                                   _HELP("InitialSpawnGroup")),

			InputPortConfig<bool>("ForceTeam0Sequence",               _HELP("Forces the team0 sequence to play regardless of client player's team"), _HELP("ForceTeam0Sequence")),
			InputPortConfig<bool>("ForceTeam1Sequence",               _HELP("Forces the team1 sequence to play regardless of client player's team"), _HELP("ForceTeam1Sequence")),
			InputPortConfig<bool>("ForceTeam2Sequence",               _HELP("Forces the team2 sequence to play regardless of client player's team"), _HELP("ForceTeam2Sequence")),

			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Started",  _HELP("Triggered when the requested animation starts playing")),
			OutputPortConfig_Void("Finished", _HELP("Triggered when the request animation finishes playing")),
			{ 0 }
		};
		config.sDescription = _HELP("Requests a trackview sequence to be started by all based on the team of the client player in predator game mode");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	float GetIntroSequenceDuration(int teamId)
	{
		const string& sequenceName = GetPortString(&m_ActInfo, (teamId == 2) ? EIP_Team2SequenceName : ((teamId == 1) ? EIP_Team1SequenceName : EIP_Team0SequenceName));
		CryHashStringId id(sequenceName);
		if (id.id)
		{
			IAnimSequence* pSequence = FindTrackviewSequence(id.id);
			if (pSequence)
			{
				return pSequence->GetTimeRange().Length().ToFloat();
			}
		}

		return -1.0f;
	}

	void OnDedicatedServerIntroStart()
	{
		// Get the longest of any specified sequence (should be same length ultimately)
		float longestSequenceDuration = -1.0f;
		for (int i = 0; i < 3; ++i)
		{
			longestSequenceDuration = std::max(longestSequenceDuration, GetIntroSequenceDuration(i));
		}
		// CryLog("OnDedicatedServerIntroStart() - Setting intro duration [%.3f]", longestSequenceDuration);
		// -1.0f indicates no intros
		if (longestSequenceDuration > -1.0f)
		{
			// Then we do have an intro. Register so we know when the first client connects
			CGameRules* pGameRules = g_pGame->GetGameRules();
			pGameRules->RegisterClientConnectionListener(this);

			m_introEndTimeStamp = longestSequenceDuration; // Sneakily store this here
		}
		else
		{
			OnDedicatedServerIntroEnd();
		}
	}

	void OnDedicatedServerIntroEnd()
	{
		CGameRules* pGameRules = g_pGame->GetGameRules();
		pGameRules->UnRegisterClientConnectionListener(this);

		// Lets progress to the next stage (e.g. Ingame)
		IGameRulesStateModule* pStateModule = pGameRules->GetStateModule();
		if (pStateModule)
		{
			pStateModule->OnIntroCompleted();
		}

		// Tell any players that exist that the intro  is over, and they can get on with life, registering on the HUD and other such things.
		CGameRules::TPlayers players;
		pGameRules->GetPlayers(players);
		IActorSystem* pActorSystem = g_pGame->GetIGameFramework()->GetIActorSystem();
		CGameRules::TPlayers::const_iterator iter = players.begin();
		CGameRules::TPlayers::const_iterator end = players.end();
		while (iter != end)
		{
			CPlayer* pPlayer = static_cast<CPlayer*>(pActorSystem->GetActor(*iter));
			if (pPlayer)
			{
				pPlayer->OnIntroSequenceFinished();
			}
			++iter;
		}
	}

	void SetupIntroStart(const EntityId cameraPoint, const float introDuration, const char* pIntroSound)
	{
		// For now don't support separate code path to position / lock player in editor (trackview can still be played manually to edit/test)
		if (gEnv->IsEditor())
		{
			return;
		}

		CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
		if (pActor)
		{
			IEntity* pActorEntity = pActor->GetEntity();
			if (pActorEntity)
			{
				// Move to camera position if available
				IEntity* pCameraPointEntity = gEnv->pEntitySystem->GetEntity(cameraPoint);
				if (pCameraPointEntity)
				{
					pActorEntity->SetWorldTM(pCameraPointEntity->GetWorldTM(), ENTITY_XFORM_TRACKVIEW);

					// Set view limits
					float horizontalLimit = 0.0f;
					float verticalLimit = 0.0f;

					if (EntityScripts::GetEntityProperty(pCameraPointEntity, "PitchLimits", verticalLimit) &&
					    EntityScripts::GetEntityProperty(pCameraPointEntity, "YawLimits", horizontalLimit))
					{

						horizontalLimit = DEG2RAD(horizontalLimit);
						verticalLimit = DEG2RAD(verticalLimit);

						CPlayer* pPlayer = static_cast<CPlayer*>(pActor);
						m_postIntroViewLimitParams = pPlayer->GetActorParams().viewLimits;
						pPlayer->GetActorParams().viewLimits.SetViewLimit(pCameraPointEntity->GetWorldTM().GetColumn1(), horizontalLimit, verticalLimit, m_postIntroViewLimitParams.GetViewLimitRangeVDown(), m_postIntroViewLimitParams.GetViewLimitRangeVUp(), m_postIntroViewLimitParams.GetViewLimitState());
						pPlayer->GetActorStats()->forceSTAP = SPlayerStats::eFS_Off;
					}

				}
				pActorEntity->Hide(true);

				// Make all other local players currently present invisible (joiners after this event will be locally hidden by revive code).
				// Once they spawn properly, they will be visible again
				g_pGame->GetGameRules()->SetAllPlayerVisibility(false, false);

				// Kick off any audio
				if (pIntroSound && pIntroSound[0])
				{
					CAudioSignalPlayer::JustPlay(pIntroSound, pActorEntity->GetWorldPos());
				}
			}
		}
		CRecordingSystem* pCrs = g_pGame->GetRecordingSystem();
		CRY_ASSERT(pCrs);
		if (pCrs)
		{
			pCrs->GetKillCamGameEffect().SetCurrentMode(CKillCamGameEffect::eKCEM_IntroCam);
			pCrs->GetKillCamGameEffect().SetActiveIfCurrentMode(CKillCamGameEffect::eKCEM_IntroCam, true);
		}

		// Record our start time + initialise timer for fade to black
		const float fOffset = 0.5f;
		m_startFadeToBlackTimeStamp = (gEnv->pTimer->GetCurrTime() + introDuration) - fOffset; // Always start the fade to black *just* before the end of the intro
		m_ActInfo.pGraph->SetRegularlyUpdated(m_ActInfo.myID, true);
	}

	void SetupIntroEnd()
	{
		// For now don't support separate code path to position / lock player in editor (trackview can still be played manually to edit/test)
		if (gEnv->IsEditor())
		{
			return;
		}

		CGameRules* pGameRules = g_pGame->GetGameRules();
		pGameRules->SetIntroSequenceHasCompletedPlaying();

		// Allow any other systems to mess with black and white screen effects again
		CParameterGameEffect* pParameterGameEffect = g_pGame->GetParameterGameEffect();
		FX_ASSERT_MESSAGE(pParameterGameEffect, "Pointer to ParameterGameEffect is NULL");
		if (pParameterGameEffect)
		{
			pParameterGameEffect->SetSaturationAmount(-1.0f, CParameterGameEffect::eSEU_Intro);
		}

		// Reset any player state we have messed with.
		CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
		if (pActor)
		{
			// Unhide
			IEntity* pActorEntity = pActor->GetEntity();
			pActorEntity->Hide(false);

			// Restore View limits
			CPlayer* pPlayer = static_cast<CPlayer*>(pActor);
			pPlayer->GetActorParams().viewLimits.SetViewLimit(m_postIntroViewLimitParams.GetViewLimitDir(), m_postIntroViewLimitParams.GetViewLimitRangeH(), m_postIntroViewLimitParams.GetViewLimitRangeV(), m_postIntroViewLimitParams.GetViewLimitRangeVUp(), m_postIntroViewLimitParams.GetViewLimitRangeVDown(), m_postIntroViewLimitParams.GetViewLimitState());
			pPlayer->GetActorStats()->forceSTAP = SPlayerStats::eFS_None;

			pPlayer->SetPlayIntro(false);

			// Head back to spectate state we would have been in, if not for intro (FORCED - Server cannot refuse this)
			IGameRulesSpectatorModule* pSpecmod = pGameRules->GetSpectatorModule();
			if (pSpecmod)
			{
				EntityId spectatorPointId = pSpecmod->GetInterestingSpectatorLocation();
				if (spectatorPointId)
				{
					pSpecmod->ChangeSpectatorMode(pActor, CActor::eASM_Fixed, spectatorPointId, false, true);
				}
				else
				{
					pSpecmod->ChangeSpectatorMode(pActor, CActor::eASM_Free, 0, false, true);
				}
			}
		}

		// End letterboxing
		g_pGame->GetRecordingSystem()->GetKillCamGameEffect().SetActiveIfCurrentMode(CKillCamGameEffect::eKCEM_IntroCam, false);

		// Lets progress to the next stage (e.g. Ingame)
		IGameRulesStateModule* pStateModule = pGameRules->GetStateModule();
		if (pStateModule)
		{
			pStateModule->OnIntroCompleted();
		}

		// Tell any players that exist that the intro  is over, and they can get on with life, registering on the HUD and other such things.
		CGameRules::TPlayers players;
		pGameRules->GetPlayers(players);
		IActorSystem* pActorSystem = g_pGame->GetIGameFramework()->GetIActorSystem();
		CGameRules::TPlayers::const_iterator iter = players.begin();
		CGameRules::TPlayers::const_iterator end = players.end();
		while (iter != end)
		{
			CPlayer* pPlayer = static_cast<CPlayer*>(pActorSystem->GetActor(*iter));
			if (pPlayer)
			{
				pPlayer->OnIntroSequenceFinished();
			}
			++iter;
		}
	}

	// IGameRulesStateListener
	virtual void OnIntroStart()
	{
		if (g_pGameCVars->g_IntroSequencesEnabled)
		{
			if (gEnv->IsDedicated())
			{
				OnDedicatedServerIntroStart();
			}
			else
			{
				// We can now start as sooon as all entities we need are available to us
				GetISystem()->GetISystemEventDispatcher()->RegisterListener(this, "CRequestCinematicIntroSequence_Node");
			}
		}

		g_pGameActions->FilterNotYetSpawned()->Enable(true);
	}

	virtual void OnGameStart()                                                     {}
	virtual void OnGameEnd()                                                       {}
	virtual void OnStateEntered(IGameRulesStateModule::EGR_GameState newGameState) {};
	// ~IGameRulesStateListener

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
	{
		// If we are *still* in the intro state at this point (the server hasn't told us the game state has moved on e.g. to prematch/in game) then kick off the intro
		CGameRules* pGameRules = g_pGame->GetGameRules();
		if (pGameRules)
		{
			IGameRulesStateModule* pStateModule = pGameRules->GetStateModule();
			if (event == ESYSTEM_EVENT_LEVEL_GAMEPLAY_START && pStateModule && pStateModule->GetGameState() == IGameRulesStateModule::EGRS_Intro)
			{
				CParameterGameEffect* pParameterGameEffect = g_pGame->GetParameterGameEffect();
				FX_ASSERT_MESSAGE(pParameterGameEffect, "Pointer to ParameterGameEffect is NULL");
				if (pParameterGameEffect)
				{
					pParameterGameEffect->SetSaturationAmount(1.0f, CParameterGameEffect::eSEU_Intro);
				}

				// Kick off sequence
				IActor* pClientActor = g_pGame->GetIGameFramework()->GetClientActor();
				if (pClientActor)
				{
					int clientTeamId = pGameRules->GetTeam(pClientActor->GetEntityId());
					bool bIsTeam0 = (clientTeamId == 0);
					bool bIsTeam1 = (clientTeamId == 1);
					bool bIsTeam2 = (clientTeamId == 2);

					// Allow overrides when testing
					if (gEnv->IsEditor())
					{
						bIsTeam0 |= GetPortBool(&m_ActInfo, EIP_ForceTeam0Sequence);
						bIsTeam1 |= GetPortBool(&m_ActInfo, EIP_ForceTeam1Sequence);
						bIsTeam2 |= GetPortBool(&m_ActInfo, EIP_ForceTeam2Sequence);
					}

					const string& sequenceName = GetPortString(&m_ActInfo, bIsTeam2 ? EIP_Team2SequenceName : (bIsTeam1 ? EIP_Team1SequenceName : EIP_Team0SequenceName));
					CryHashStringId id(sequenceName);
					if ((gEnv->bMultiplayer || gEnv->IsEditor()) && id.id)
					{
						IAnimSequence* pSequence = FindTrackviewSequence(id.id);
						if (pSequence)
						{
							IMovieSystem* pMovieSystem = gEnv->pMovieSystem;
							if (!pMovieSystem->IsPlaying(pSequence))
							{
								m_pSequence = pSequence;
								pMovieSystem->AddMovieListener(m_pSequence, this);

								// DON'T SYNC THIS ! (all intros are local for now - can this be set from in the editor on a sequence? I couldn't find it :( )
								m_pSequence->SetFlags(IAnimSequence::eSeqFlags_NoMPSyncingNeeded);

								pMovieSystem->PlaySequence(pSequence, NULL, true, false);

								// Get any audio we want to play
								const string& signalName = GetPortString(&m_ActInfo, bIsTeam2 ? EIP_Team2AudioSignalName : (bIsTeam1 ? EIP_Team1AudioSignalName : EIP_Team0AudioSignalName));
								SetupIntroStart(GetPortEntityId(&m_ActInfo, bIsTeam0 ? EIP_Team0CameraPointEntityId : (bIsTeam1 ? EIP_Team1CameraPointEntityId : EIP_Team2CameraPointEntityId)), pSequence->GetTimeRange().Length().ToFloat(), signalName.c_str());
							}
						}
					}
				}
			}
		}
	}
	// ~ISystemEventListener

	IAnimSequence* FindTrackviewSequence(int trackviewId)
	{
		IMovieSystem* pMovieSystem = gEnv->pMovieSystem;

		for (int i = 0; i < pMovieSystem->GetNumSequences(); ++i)
		{
			IAnimSequence* pSequence = pMovieSystem->GetSequence(i);
			CryHashStringId id(pSequence->GetName());
			if (id == trackviewId)
			{
				return pSequence;
			}
		}

		return NULL;
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_ActInfo = *pActInfo;

				// Kick off initial spawn group override
				CGameRules* pGameRules = g_pGame->GetGameRules();
				IGameRulesSpawningModule* pSpawningModule = pGameRules ? pGameRules->GetSpawningModule() : NULL;
				if (pSpawningModule)
				{
					pSpawningModule->SetInitialSpawnGroup(GetPortString(&m_ActInfo, EIP_InitialSpawnGroup).c_str());
				}
			}
			break;

		case eFE_Activate:
			{
				// Enable editor to test
				if (gEnv->IsEditor() && gEnv->IsEditorGameMode())
				{
					if (IsPortActive(pActInfo, EIP_ForceTeam0Sequence) ||
					    IsPortActive(pActInfo, EIP_ForceTeam1Sequence) ||
					    IsPortActive(pActInfo, EIP_ForceTeam2Sequence))
					{
						OnIntroStart();
					}
				}
			}
			break;

		case eFE_Update:
			{
				Update();
			}
			break;
		}
	}

	void Update()
	{
		if (!gEnv->IsDedicated())
		{
			if (m_startFadeToBlackTimeStamp > -1.0f)
			{
				if (gEnv->pTimer->GetCurrTime() > m_startFadeToBlackTimeStamp)
				{
					// Trigger a fade to black. This will stay onscreen until the player has selected equipment + revived.
					CPlayer* pClientPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetClientActor());
					if (pClientPlayer != NULL)
					{
						CLocalPlayerComponent* pLocalPlayerComponent = pClientPlayer->GetLocalPlayerComponent();
						pLocalPlayerComponent->TriggerFadeToBlack();
						pClientPlayer->HoldScreenEffectsUntilNextSpawnRevive();
					}

					m_ActInfo.pGraph->SetRegularlyUpdated(m_ActInfo.myID, false);
					m_startFadeToBlackTimeStamp = -1.0f;
				}
			}
		}
		else
		{
			// Done
			if (gEnv->pTimer->GetCurrTime() > m_introEndTimeStamp)
			{
				m_ActInfo.pGraph->SetRegularlyUpdated(m_ActInfo.myID, false);
				OnDedicatedServerIntroEnd();
			}
		}
	}

	virtual void OnMovieEvent(IMovieListener::EMovieEvent event, IAnimSequence* pSequence)
	{
		if (m_pSequence && m_pSequence->GetId() == pSequence->GetId())
		{
			IMovieSystem* pMovieSystem = gEnv->pMovieSystem;

			if (event == IMovieListener::eMovieEvent_Started)
			{
				g_pGame->GetGameRules()->SetIntroSequenceCurrentlyPlaying(true);
				ActivateOutput(&m_ActInfo, EOP_Started, true);
			}
			if (event == IMovieListener::eMovieEvent_Stopped || event == IMovieListener::eMovieEvent_Aborted)
			{
				g_pGame->GetGameRules()->SetIntroSequenceCurrentlyPlaying(false);
				ActivateOutput(&m_ActInfo, EOP_Finished, true);
				pMovieSystem->RemoveMovieListener(pSequence, this);
				m_pSequence = NULL;

				// Setup various player configuration + game state
				SetupIntroEnd();
			}
		}
	}

	// IGameRulesClientConnectionListener()
	virtual void OnClientConnect(int channelId, bool isReset, EntityId playerId) {};
	virtual void OnClientDisconnect(int channelId, EntityId playerId)            {};
	virtual void OnOwnClientEnteredGame()                                        {};

	virtual void OnClientEnteredGame(int channelId, bool isReset, EntityId playerId)
	{
		CRY_ASSERT(gEnv->IsDedicated());

		// We stored the intro duration in the end timestamp earlier, calculate actual 'end' timestamp.
		m_introEndTimeStamp = gEnv->pTimer->GetCurrTime() + m_introEndTimeStamp;
		m_ActInfo.pGraph->SetRegularlyUpdated(m_ActInfo.myID, true);

		g_pGame->GetGameRules()->UnRegisterClientConnectionListener(this);
	}
	// ~IGameRulesClientConnectionListener()

private:
	SActivationInfo  m_ActInfo;
	IAnimSequence*   m_pSequence;
	SViewLimitParams m_postIntroViewLimitParams;

	// Timer
	float m_startFadeToBlackTimeStamp;
	float m_introEndTimeStamp;
};

class CMPHighlightInteractiveEntity : public CFlowBaseNode<eNCT_Instanced>
	                                    , public IEntityEventListener
	                                    , public IGameRulesRevivedListener
{
	enum INPUTS
	{
		EIP_Highlight,
		EIP_Unhighlight,
		EIP_Shoot,
		EIP_RequiresNanosuit,
	};

public:
	CMPHighlightInteractiveEntity(SActivationInfo* pActInfo) : m_entityId(0), m_bHighlighted(false), m_bRequiresNanosuit(false)
	{
	}

	~CMPHighlightInteractiveEntity()
	{
	};

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CMPHighlightInteractiveEntity(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Highlight",         _HELP("Start highlighting this entity"), _HELP("Highlight")),
			InputPortConfig_Void("Unhighlight",       _HELP("Stop highlighting this entity"),  _HELP("Unhighlight")),
			InputPortConfig<bool>("Shoot",            false,                                   _HELP("The entity is shot to activate"),                             _HELP("Shoot")),
			InputPortConfig<bool>("RequiresNanosuit", false,                                   _HELP("The entity is only highlighted if the player has a nanosuit"),_HELP("RequiresNanosuit")),
			{ 0 }
		};
		config.sDescription = _HELP("Highlights the given entity through the interactive entity monitor player plugin");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Highlight))
				{
					const bool shootToInteract = GetPortBool(pActInfo, EIP_Shoot);
					m_bRequiresNanosuit = GetPortBool(pActInfo, EIP_RequiresNanosuit);

					HighlightEntity(true, shootToInteract);

					if (CGameRules* pGameRules = g_pGame->GetGameRules())
					{
						pGameRules->RegisterRevivedListener(this);
					}
				}
				else if (IsPortActive(pActInfo, EIP_Unhighlight))
				{
					HighlightEntity(false);

					if (CGameRules* pGameRules = g_pGame->GetGameRules())
					{
						pGameRules->UnRegisterRevivedListener(this);
					}
				}
			}
			break;
		case eFE_SetEntityId:
			{
				if (pActInfo->pEntity)
				{
					EntityId newId = pActInfo->pEntity->GetId();

					//Handle the entity being changed while the current entity is highlighted
					if (m_bHighlighted)
					{
						HighlightEntity(false);
					}

					m_entityId = newId;
				}
			}
			break;
		}
	}

	virtual void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event)
	{
		if (event.event == ENTITY_EVENT_DONE)
		{
			HighlightEntity(false);
			m_entityId = 0;
		}
	}

	virtual void HighlightEntity(bool highlight, bool shootToInteract = false)
	{
		if (highlight != m_bHighlighted && m_entityId)
		{
			m_bHighlighted = highlight;

			if (IActor* pClientActor = g_pGame->GetIGameFramework()->GetClientActor())
			{
				CPlayer* pPlayer = static_cast<CPlayer*>(pClientActor);
				if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_entityId))
				{
					if (highlight)
					{
						gEnv->pEntitySystem->AddEntityEventListener(m_entityId, ENTITY_EVENT_DONE, this);
					}
					else
					{
						gEnv->pEntitySystem->RemoveEntityEventListener(m_entityId, ENTITY_EVENT_DONE, this);
					}
				}
			}
		}
	}

	virtual void EntityRevived(EntityId entityId)
	{
		const IGameFramework* pGameFramework = g_pGame->GetIGameFramework();
		if (m_bRequiresNanosuit && gEnv->IsClient() && pGameFramework->GetClientActorId() == entityId)
		{
			const CActor* pClientActor = static_cast<CActor*>(pGameFramework->GetClientActor());

			if (m_bHighlighted)
			{
				HighlightEntity(m_bHighlighted, false);
			}
		}
	}

private:
	EntityId m_entityId;
	bool     m_bHighlighted;
	bool     m_bRequiresNanosuit;
};

class CMPGetClientTeamId_Node : public CFlowBaseNode<eNCT_Instanced>
{
	enum INPUTS
	{
		EIP_Request,
	};

	enum OUTPUTS
	{
		EOP_Team0,
		EOP_Team1,
		EOP_Team2,
	};

public:
	CMPGetClientTeamId_Node(SActivationInfo* pActInfo)
	{
	}

	~CMPGetClientTeamId_Node()
	{
	};

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CMPGetClientTeamId_Node(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Request", _HELP("Request team id of client player"), _HELP("Request")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Team0", _HELP("Triggered when the local client is on team 0")),
			OutputPortConfig_Void("Team1", _HELP("Triggered when the local client is on team 1")),
			OutputPortConfig_Void("Team2", _HELP("Triggered when the local client is on team 2")),
			{ 0 }
		};
		config.sDescription = _HELP("Obtain client player team id");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_ActInfo = *pActInfo;
			}
			break;

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Request))
				{

					CGameRules* pGameRules = g_pGame->GetGameRules();
					if (pGameRules)
					{
						EntityId localClientId = gEnv->pGameFramework->GetClientActorId();
						int localClientTeamId = pGameRules->GetTeam(localClientId);

						ActivateOutput(&m_ActInfo, localClientTeamId == 2 ? EOP_Team2 : (localClientTeamId == 1 ? EOP_Team1 : EOP_Team0), true);
					}
				}
			}
			break;
		}
	}

private:
	SActivationInfo m_ActInfo;
};

class CMPGameState_Node : public CFlowBaseNode<eNCT_Instanced>, public IGameRulesStateListener, public IGameRulesRoundsListener
#if USE_PC_PREMATCH
	                        , public IGameRulesPrematchListener
#endif
{
	enum INPUTS
	{
	};

	enum OUTPUTS
	{
		EOP_OnPrematchStateStart, // Intro state ending, Prematch state starting
		EOP_OnInGameStateStart,   // Prematch state ending, in game state triggered
		EOP_OnPostGameStateStart, // Ingame state ending, Post game state triggered

		EOP_OnAnyNewRoundStart,
		EOP_OnFirstRoundStart,
		EOP_OnFinalRoundStart,
		EOP_OnRoundEnd,
	};

public:
	CMPGameState_Node(SActivationInfo* pActInfo)
	{
	}

	~CMPGameState_Node()
	{
		// Require info about game state and (if applicable) rounds.
		if (g_pGame)
		{
			CGameRules* pGameRules = g_pGame->GetGameRules();
			if (pGameRules)
			{
				IGameRulesStateModule* pStateModule = pGameRules->GetStateModule();
				if (pStateModule)
				{
					pStateModule->RemoveListener(this);
				}

				pGameRules->UnRegisterRoundsListener(this);

#if USE_PC_PREMATCH
				pGameRules->UnRegisterPrematchListener(this);
#endif   // USE_PC_PREMATCH
			}
		}
	};

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CMPGameState_Node(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {

			OutputPortConfig_Void("OnPrematchStateStart", _HELP("Triggered when the PREMATCH state is starting")),
			OutputPortConfig_Void("OnInGameStateStart",   _HELP("Triggered when the PREMATCH state has ended and the IN GAME state begins")),
			OutputPortConfig_Void("OnPostGameStateStart", _HELP("Triggered when the IN GAME state has ended and the POST GAME state is entered")),

			OutputPortConfig_Void("OnAnyNewRoundStart",   _HELP("Triggered when ANY new round starts")),
			OutputPortConfig_Void("OnFirstRoundStart",    _HELP("Triggered when the FIRST round starts")),
			OutputPortConfig_Void("OnFinalRoundStart",    _HELP("Triggered when the FINAL round starts")),
			OutputPortConfig_Void("OnRoundEnd",           _HELP("Triggered when any round ends")),

			{ 0 }
		};
		config.sDescription = _HELP("React to various game state events");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Update:
			{
				CGameRules* pGameRules = g_pGame->GetGameRules();
				if (pGameRules)
				{
					RegisterAsListener(pGameRules);
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				}
			}
			break;
		case eFE_Initialize:
			{
				m_ActInfo = *pActInfo;

				// Require info about game state and (if applicable) rounds.
				CGameRules* pGameRules = g_pGame->GetGameRules();
				if (pGameRules)
				{
					RegisterAsListener(pGameRules);
				}
				else
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
				}
			}
			break;
		}
	}

	void RegisterAsListener(CGameRules* pGameRules)
	{
		if (pGameRules)
		{
			IGameRulesStateModule* pStateModule = pGameRules->GetStateModule();
			if (pStateModule)
			{
				pStateModule->AddListener(this);
			}

			pGameRules->RegisterRoundsListener(this);

#if USE_PC_PREMATCH
			pGameRules->RegisterPrematchListener(this);
#endif // USE_PC_PREMATCH
		}
	}

	// IGameRulesRoundsListener
	virtual void OnRoundStart()
	{
		ActivateOutput(&m_ActInfo, EOP_OnAnyNewRoundStart, true);

		IGameRulesRoundsModule* pRoundsModule = g_pGame->GetGameRules()->GetRoundsModule();
		if (pRoundsModule)
		{
			// Final?
			if (pRoundsModule->GetRoundsRemaining() == 0)
			{
				ActivateOutput(&m_ActInfo, EOP_OnFinalRoundStart, true);
			}
		}
	}

	virtual void OnRoundEnd()
	{
		ActivateOutput(&m_ActInfo, EOP_OnRoundEnd, true);
	}

	virtual void OnSuddenDeath()                                           {};
	virtual void ClRoundsNetSerializeReadState(int newState, int curState) {};
	virtual void OnRoundAboutToStart()                                     {};
	//~IGameRulesRoundsListener

	// IGameRulesStateListener
	virtual void OnIntroStart() {};
	virtual void OnGameStart()
	{
		CGameRules* pGameRules = g_pGame->GetGameRules();
		if (pGameRules)
		{
			if (pGameRules->HasGameActuallyStarted()) // If in prematch state still this == false
			{
				ActivateOutput(&m_ActInfo, EOP_OnInGameStateStart, true);

				IGameRulesRoundsModule* pRoundsModule = g_pGame->GetGameRules()->GetRoundsModule();
				if (pRoundsModule)
				{
					if (pRoundsModule->GetRoundNumber() == 0)
					{
						ActivateOutput(&m_ActInfo, EOP_OnFirstRoundStart, true);
					}
				}
			}
		}
	}

	virtual void OnGameEnd() {};
	virtual void OnStateEntered(IGameRulesStateModule::EGR_GameState newGameState)
	{
		switch (newGameState)
		{
		case IGameRulesStateModule::EGRS_PreGame:
			{
				ActivateOutput(&m_ActInfo, EOP_OnPrematchStateStart, true);
			}
			break;
		case IGameRulesStateModule::EGRS_PostGame:
			{
				ActivateOutput(&m_ActInfo, EOP_OnPostGameStateStart, true);
			}
			break;
		}
	}
	// ~IGameRulesStateListener

#if USE_PC_PREMATCH
	// IGameRulesPrematchListener
	void OnPrematchEnd()
	{
		// This is when design will expect 'inGame' state change to occur (it actually occurs much earlier in code, before we exit prematch confusingly)
		ActivateOutput(&m_ActInfo, EOP_OnInGameStateStart, true);

		IGameRulesRoundsModule* pRoundsModule = g_pGame->GetGameRules()->GetRoundsModule();
		if (pRoundsModule)
		{
			ActivateOutput(&m_ActInfo, EOP_OnFirstRoundStart, true);
		}
	}
	//~IGameRulesPrematchListener
#endif

private:
	SActivationInfo m_ActInfo;
};

class CMPCheckCVar_Node : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CMPCheckCVar_Node(SActivationInfo* pActInfo) {}

	enum EInPorts
	{
		GET = 0,
		NAME,
	};
	enum EOutPorts
	{
		BOOLVALUE = 0,
		INTVALUE,
		FLTVALUE,
		STRVALUE,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<SFlowSystemVoid>("Get", _HELP("Trigger to Get CVar's value")),
			InputPortConfig<string>("CVar",         _HELP("Name of the CVar to get [case-INsensitive]")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<bool>("BoolValue",     _HELP("Value of the CVar is non-zero [Int or Float]")),
			OutputPortConfig<int>("IntValue",       _HELP("Current Int Value of the CVar [Int or Float]")),
			OutputPortConfig<float>("FloatValue",   _HELP("Current Float Value of the CVar [Int or Float]")),
			OutputPortConfig<string>("StringValue", _HELP("Current String Value of the CVar [String]")),
			{ 0 }
		};
		config.sDescription = _HELP("Gets the value of a console variable (CVar).");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate)
		{
			if (!IsPortActive(pActInfo, GET))
				return;

			const string& cvar = GetPortString(pActInfo, NAME);
			ICVar* pICVar = gEnv->pConsole->GetCVar(cvar.c_str());
			if (pICVar != 0)
			{
				if (pICVar->GetType() == CVAR_INT)
				{
					const int ival = pICVar->GetIVal();
					ActivateOutput(pActInfo, INTVALUE, ival);
					ActivateOutput(pActInfo, BOOLVALUE, ival != 0);
					ActivateOutput(pActInfo, FLTVALUE, (float)ival);
				}
				if (pICVar->GetType() == CVAR_FLOAT)
				{
					const float fval = pICVar->GetFVal();
					ActivateOutput(pActInfo, FLTVALUE, fval);
					ActivateOutput(pActInfo, BOOLVALUE, fabs_tpl(fval) > FLT_EPSILON);
					ActivateOutput(pActInfo, INTVALUE, (int)fval);
				}
				if (pICVar->GetType() == CVAR_STRING)
				{
					const string sVal = pICVar->GetString();
					ActivateOutput(pActInfo, STRVALUE, sVal);
				}
			}
			else
			{
				CryLogAlways("[flow] Cannot find console variable '%s'", cvar.c_str());
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//----------------------------------------------------------------------------------------

class CFlowNode_IsMultiplayer : public CFlowBaseNode<eNCT_Instanced>
{
	enum INPUTS
	{
		EIP_Get = 0,
	};

	enum OUTPUTS
	{
		EOP_True = 0,
		EOP_False,
	};

public:
	CFlowNode_IsMultiplayer(SActivationInfo* pActInfo)
	{
	}

	~CFlowNode_IsMultiplayer()
	{
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_IsMultiplayer(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Get", _HELP("Activate this to retrigger relevent outputs")),
			{ 0 }
		};
		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_Void("True",  _HELP("Triggered if Multiplayer game")),
			OutputPortConfig_Void("False", _HELP("Triggered if it is a Singleplayer game")),
			{ 0 }
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Checks if Game Mode is Multiplayer or not");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_actInfo = *pActInfo;
			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Get))
				{
					if (gEnv->bMultiplayer)
					{
						ActivateOutput(&m_actInfo, EOP_True, true);
					}
					else
					{
						ActivateOutput(&m_actInfo, EOP_False, true);
					}
				}
			}
			break;
		}
	}

protected:
	SActivationInfo m_actInfo;
};

//----------------------------------------------------------------------------------------

class CFlowNode_GameOption : public CFlowBaseNode<eNCT_Instanced>
{
	enum INPUTS
	{
		EIP_Set = 0,
		EIP_Get,
		EIP_Option,
		EIP_Value,
	};

	enum OUTPUTS
	{
		EOP_Done = 0,
		EOP_Val,
	};

public:
	CFlowNode_GameOption(SActivationInfo* pActInfo)
	{
	}

	~CFlowNode_GameOption()
	{
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_GameOption(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Set",       _HELP("Set game option to variant")),
			InputPortConfig_Void("Get",       _HELP("Get game option")),
			InputPortConfig<string>("Option", _HELP("options name")),
			InputPortConfig<string>("Value",  _HELP("option values (used with set)")),
			{ 0 }
		};
		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_Void("Done",     _HELP("Outputs when set or get is done")),
			OutputPortConfig<string>("Value", _HELP("Option value outputted by get")),
			{ 0 }
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Set gamemode option");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Set))
				{
					if (gEnv->bMultiplayer)
					{
						string option = GetPortString(pActInfo, EIP_Option);
						string val = GetPortString(pActInfo, EIP_Value);

						CPlaylistManager* pPlaylistManager = g_pGame->GetPlaylistManager();
						if (pPlaylistManager)
						{
							pPlaylistManager->SetGameModeOption(option.c_str(), val.c_str());
						}
					}

					ActivateOutput(pActInfo, EOP_Done, 1);
				}

				if (IsPortActive(pActInfo, EIP_Get))
				{
					if (gEnv->bMultiplayer)
					{
						string option = GetPortString(pActInfo, EIP_Option);

						CPlaylistManager* pPlaylistManager = g_pGame->GetPlaylistManager();
						CryFixedStringT<32> out_val;
						if (pPlaylistManager)
						{
							pPlaylistManager->GetGameModeOption(option.c_str(), out_val);
							string stringVal(out_val);
							ActivateOutput(pActInfo, EOP_Val, stringVal);
						}
					}

					ActivateOutput(pActInfo, EOP_Done, 1);
				}
			}
			break;
		}
	}
};

REGISTER_FLOW_NODE("Multiplayer:IsMultiplayer", CFlowNode_IsMultiplayer);
REGISTER_FLOW_NODE("Game:RoundTrigger", CFlowNode_RoundTrigger);
REGISTER_FLOW_NODE("Multiplayer:MP", CFlowNode_MP);
REGISTER_FLOW_NODE("Multiplayer:GameType", CFlowNode_MPGameType);
REGISTER_FLOW_NODE("Multiplayer:IsServer", CFlowNode_IsServer);
REGISTER_FLOW_NODE("Multiplayer:RequestPlaySequence", CRequestPlaySequence_Node);
REGISTER_FLOW_NODE("Multiplayer:AttachToPath", CMPAttachToPath_Node);
REGISTER_FLOW_NODE("Multiplayer:EnvironmentalWeaponListener", CMPEnvironmentalWeapon_Node);
REGISTER_FLOW_NODE("Multiplayer:GetClientTeamId", CMPGetClientTeamId_Node);
REGISTER_FLOW_NODE("Multiplayer:HighlightEntity", CMPHighlightInteractiveEntity);
REGISTER_FLOW_NODE("Multiplayer:GameState", CMPGameState_Node);
REGISTER_FLOW_NODE("Multiplayer:CheckCVar", CMPCheckCVar_Node);
REGISTER_FLOW_NODE("Multiplayer:GameOption", CFlowNode_GameOption);
