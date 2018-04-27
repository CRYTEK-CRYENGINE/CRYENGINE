// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   History:
   - 12:05:2010   Created by Steve Humphreys
*************************************************************************/

#include "StdAfx.h"
#include "Player.h"
#include "ILevelSystem.h"

#include <CryAISystem/IAIActor.h>
#include <CryAISystem/IAgent.h>
#include <CryFlowGraph/IFlowBaseNode.h>

//////////////////////////////////////////////////////////////////////////
class CFlowNode_FireSystemEvent : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		EIP_FireEvent = 0,
		EIP_EventType,
	};

	enum OUTPUTS
	{
		EOP_FiredEvent = 0,
	};

public:

	CFlowNode_FireSystemEvent(SActivationInfo* pActInfo) {}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("FireEvent", _HELP("Fire system event")),
			InputPortConfig<int>("EventType", 0,                          _HELP("Type of event"),0, _UICONFIG("enum_int:LEVELGAMEPLAYSTART=0")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_Void("FiredEvent", _HELP("Triggers when event is fired")),
			{ 0 }
		};

		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Fires system event");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActInfo)
	{
		switch (flowEvent)
		{
		case eFE_Activate:
			{

				if (IsPortActive(pActInfo, EIP_FireEvent))
				{
					const int iEventType = GetPortInt(pActInfo, EIP_EventType);

					// Currently only supporting ESYSTEM_EVENT_LEVEL_GAMEPLAY_START
					if (iEventType != 0)
						return;

					gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_GAMEPLAY_START, 0, 0);

					ActivateOutput(pActInfo, EOP_FiredEvent, true);
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

};

class CFlowNode_SetPostEffectParam : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		EIP_Set = 0,
		EIP_ParamName,
		EIP_ParamFloat,
	};

	enum OUTPUTS
	{
		EOP_Set = 0,
	};

public:
	CFlowNode_SetPostEffectParam(SActivationInfo* pActInfo)
	{
	}

	~CFlowNode_SetPostEffectParam()
	{
	}

	/*
	   IFlowNodePtr Clone( SActivationInfo * pActInfo )
	   {
	   return this;
	   }
	 */

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Set",          _HELP("Sets the param")),
			InputPortConfig<string>("ParamName", _HELP("Parameter name")),
			InputPortConfig<float>("ParamFloat", 0,                       _HELP("Parameter type in float")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_Void("Set", _HELP("Triggers when param set")),
			{ 0 }
		};

		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Sets post effect param by name");
		config.SetCategory(EFLN_DEBUG);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event != eFE_Activate)
			return;

		I3DEngine* pEngine = gEnv->p3DEngine;

		const string& message = GetPortString(pActInfo, EIP_ParamName);
		const float& fParamFloat = GetPortFloat(pActInfo, EIP_ParamFloat);

		pEngine->SetPostEffectParam(message, fParamFloat);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowIsDemo : public CFlowBaseNode<eNCT_Singleton>
{
	enum
	{
		OUT_DEMO1 = 0,
		OUT_DEMO2,
		OUT_DEMO3
	};

	enum
	{
		INP_CHECK = 0
	};

public:
	CFlowIsDemo(SActivationInfo* pActInfo)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("check", _HELP("ReTriggers the output")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<bool>("isDemo",  _HELP("True if the game is playing in demo mode")),
			OutputPortConfig<bool>("isDemo2", _HELP("True if the game is playing in demo mode v2")),
			OutputPortConfig<bool>("isDemo3", _HELP("True if the game is playing in demo mode v3")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Outputs whether the game is playing in demo mode or not.");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				switch (g_pGame->GetCVars()->g_devDemo)
				{
				case 1:
					ActivateOutput(pActInfo, OUT_DEMO1, true);
					break;
				case 2:
					ActivateOutput(pActInfo, OUT_DEMO2, true);
					break;
				case 3:
					ActivateOutput(pActInfo, OUT_DEMO3, true);
					break;
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Game:IsZoomToggling node.
// Checks if zoom toggling is enabled or disabled
//////////////////////////////////////////////////////////////////////////
class CFlowIsZoomToggling : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		EIP_Check = 0,
	};

	enum OUTPUTS
	{
		EOP_Enabled = 0,
		EOP_Disabled,
	};

public:

	CFlowIsZoomToggling(SActivationInfo* pActInfo) {}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Check", _HELP("Checks if zoom toggling is enabled.")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig<bool>("Enabled",  _HELP("True if zoom toggling enabled.")),
			OutputPortConfig<bool>("Disabled", _HELP("True if zoom toggling disabled.")),
			{ 0 }
		};

		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Checks whether zoom toggling is enabled or disabled.");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActInfo)
	{
		switch (flowEvent)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Check))
				{
					if (g_pGameCVars->cl_zoomToggle > 0)
					{
						ActivateOutput(pActInfo, EOP_Enabled, true);
					}
					else
					{
						ActivateOutput(pActInfo, EOP_Disabled, true);
					}
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

};

//////////////////////////////////////////////////////////////////////////
class CFlowSaveGameNode : public CFlowBaseNode<eNCT_Instanced>, public IGameFrameworkListener
{
public:

	enum
	{
		EIP_Save = 0,
		EIP_Load,
		EIP_Name,
		EIP_Desc,
		EIP_EnableSave,
		EIP_DisableSave,
		EIP_DelaySaveIfPlayerInAir,
	};

	enum
	{
		EOP_SaveOrLoadDone = 0,
	};

	enum EState
	{
		ES_Idle = 0,
		ES_WaitForSaveDone,
		ES_Notify,
		ES_WaitForPlayerNotInAir
	};

	CFlowSaveGameNode(SActivationInfo* pActInfo) : m_state(ES_Idle)
	{
	}

	~CFlowSaveGameNode()
	{
		gEnv->pGameFramework->UnregisterListener(this);
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowSaveGameNode(pActInfo); }

	virtual void         Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		uint32 val = m_state;
		ser.Value("m_state", val);
		m_state = EState(val);
		if (ser.IsReading() && m_state == ES_WaitForSaveDone)
		{
			m_state = ES_Notify;  // because we are not going to receive any event notification from CryAction, and we still want to notify the Done output.
		}
		if (m_state == ES_WaitForPlayerNotInAir) // even just in saving mode, we dont want this to keep running if there was already another save triggered.
		{
			m_state = ES_Idle;
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->Add(*this);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig_Void("Save",                    _HELP("Trigger to save game")),
			InputPortConfig_Void("Load",                    _HELP("Trigger to load game")),
			InputPortConfig<string>("Name",                 string("quicksave"),                                _HELP("Name of SaveGame to save/load. Use $LAST to load last savegame")),
			InputPortConfig<string>("Desc",                 string(),                                           _HELP("Description [Currently ignored]"),                                                                                                                                                                                           _HELP("Description")),
			InputPortConfig_Void("EnableSave",              _HELP("Trigger to globally allow quick-saving")),
			InputPortConfig_Void("DisableSave",             _HELP("Trigger to globally disallow quick-saving")),
			InputPortConfig<bool>("DelaySaveIfPlayerInAir", false,                                              _HELP("if true, the savegame will be delayed until the the player is no longuer in air (jumping, falling, etc). /nUse only in case there is a real danger of problem with the checkpoint (player dying after fall, for example)")),
			{ 0 }
		};

		static const SOutputPortConfig out_config[] =
		{
			OutputPortConfig_Void("SaveOrLoadDone", _HELP("Triggered after the savegame is created and also after the savegame is loaded./n"
			                                              "When saving, the trigger hapens after the 'snapshot' of the game state is taken. The actual physical writing of the data into the HD or memory card could take longer./n"
			                                              "this output will also be triggered if there was any critical error and the savegame could not be created"
			                                              )),
			{ 0 }
		};

		config.sDescription = _HELP("SaveGame for Autosave");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;

		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_state = ES_Idle;
			break;

		case eFE_Update:
			{
				switch (m_state)
				{
				case ES_Notify:
					{
						ActivateOutput(pActInfo, EOP_SaveOrLoadDone, true);

						gEnv->pGameFramework->UnregisterListener(this);

						pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
						m_state = ES_Idle;
						break;
					}

				case ES_WaitForPlayerNotInAir:
					{
						if (!PlayerIsInAir() && gEnv->pGameFramework->CanSave())
						{
							m_extraCheckDeadTimerCounter--;
							if (m_extraCheckDeadTimerCounter <= 0)
								Save(pActInfo);
						}
						break;
					}
				}

				break;
			}

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_DisableSave))
				{
					gEnv->pGameFramework->AllowSave(false);
				}

				if (IsPortActive(pActInfo, EIP_EnableSave))
				{
					gEnv->pGameFramework->AllowSave(true);
				}

				if (IsPortActive(pActInfo, EIP_Save))
				{
					if (GetPortBool(pActInfo, EIP_DelaySaveIfPlayerInAir) == true && PlayerIsInAir())
					{
						pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
						m_state = ES_WaitForPlayerNotInAir;
						m_extraCheckDeadTimerCounter = EXTRA_CHECK_DEAD_NUMFRAMES;
					}
					else
						Save(pActInfo);
				}

				if (IsPortActive(pActInfo, EIP_Load))
				{
					string name = GetPortString(pActInfo, EIP_Name);

					if (name == "$LAST")
					{
						gEnv->pGameFramework->ExecuteCommandNextFrame("loadLastSave");
					}
					else
					{
						PathUtil::RemoveExtension(name);

						name += CRY_SAVEGAME_FILE_EXT;

						gEnv->pGameFramework->LoadGame(name.c_str(), true);
					}
				}

				break;
			}
		}
	}

	bool PlayerIsInAir()
	{
		bool InAir = false;
		IActor* pClientActor = gEnv->pGameFramework->GetClientActor();
		if (pClientActor && pClientActor->IsPlayer())
		{
			CPlayer* pPlayer = static_cast<CPlayer*>(pClientActor);
			InAir = pPlayer->IsInAir() || pPlayer->IsInFreeFallDeath();
		}
		return InAir;
	}

	//////////////////////////////////////////////////////////////////////////
	void Save(SActivationInfo* pActInfo)
	{
		m_name = GetPortString(pActInfo, EIP_Name);
		PathUtil::RemoveExtension(m_name);

		if (gEnv->IsEditor())
		{
			m_state = ES_Idle;
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);

			IActor* pClientActor = g_pGame->GetIGameFramework()->GetClientActor();
			if (pClientActor && !pClientActor->IsDead())
				ActivateOutput(pActInfo, EOP_SaveOrLoadDone, true);
		}
		else
		{
			gEnv->pGameFramework->RegisterListener(this, "CFlowSaveGameNode", FRAMEWORKLISTENERPRIORITY_DEFAULT);

			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
			m_state = ES_WaitForSaveDone;
		}

		auto saveGameName = gEnv->pGameFramework->CreateSaveGameName();

		gEnv->pGameFramework->SaveGame(saveGameName, true, false, eSGR_FlowGraph, false, m_name.c_str());
	}

	// IGameFrameworkListener

	virtual void OnPostUpdate(float fDeltaTime)
	{
	}

	virtual void OnSaveGame(ISaveGame* pSaveGame)
	{
	}

	virtual void OnLoadGame(ILoadGame* pLoadGame)
	{
	}

	virtual void OnLevelEnd(const char* pNextLevel)
	{
	}

	virtual void OnActionEvent(const SActionEvent& event)
	{
		if (m_state == ES_WaitForSaveDone && event.m_event == eAE_postSaveGame && event.m_description && !strcmp(event.m_description, m_name.c_str()))
		{
			m_state = ES_Notify;
		}
	}

	// ~IGameFrameworkListener

private:

	EState m_state;
	string m_name;
	int32  m_extraCheckDeadTimerCounter;
	enum { EXTRA_CHECK_DEAD_NUMFRAMES = 5 }; // in case we had to wait for the player to not be inAir before saving, we add this time to the wait to avoid any possible edge case
	                                         // with the player dying right after or something similar. (which does not happens, is just an extra sanity precaution)
};


REGISTER_FLOW_NODE( "Game:FireSystemEvent", CFlowNode_FireSystemEvent);
REGISTER_FLOW_NODE( "Game:SetPostEffectParam", CFlowNode_SetPostEffectParam);
REGISTER_FLOW_NODE( "Game:IsDemo", CFlowIsDemo);
REGISTER_FLOW_NODE( "Game:IsZoomToggling", CFlowIsZoomToggling);
REGISTER_FLOW_NODE( "Game:SaveGame", CFlowSaveGameNode );
