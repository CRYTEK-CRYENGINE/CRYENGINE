// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AINodes.h"

#include <CryAISystem/IAIObject.h>
#include <CryAISystem/IAIActor.h>
#include "AI/GameAISystem.h"
#include "AI/GameAIEnv.h"
#include "AI/AIBattleFront.h"
#include "AI/AICorpse.h"
#include "GunTurret.h"
#include <GameObjects/GameObject.h>
#include "Turret/Turret/Turret.h"

void CFlowNode_BattleFrontControl::GetConfiguration( SFlowNodeConfig& config )
{
	static const SInputPortConfig in_ports[] = 
	{
		InputPortConfig<int>("Group", 0, _HELP("Target battle front group id.")),
		InputPortConfig<Vec3>("Position", _HELP("Desired battle front position")),
		InputPortConfig_Void("Activate", _HELP("Activate battle front position control." )),
		InputPortConfig_Void("Deactivate", _HELP("Deactivate battle front position control." )),
		{0}
	};
	config.pInputPorts = in_ports;
	config.pOutputPorts = 0;
	config.sDescription = _HELP("BattleFront control. Used for overriding the default position of a AI group's BattleFront.");
	config.SetCategory(EFLN_ADVANCED);
}

void CFlowNode_BattleFrontControl::ProcessEvent( EFlowEvent event, SActivationInfo *activationInformation )
{
	if (event == eFE_Initialize)
	{
		m_enabled = false;
	}

	if(event == eFE_Activate)
	{
		int	groupID = GetPortInt(activationInformation, EIP_Group);	
		CAIBattleFrontGroup* battleFrontGroup = gGameAIEnv.battleFrontModule->GetGroupByID(groupID);
		if (!battleFrontGroup)
		{
			GameWarning("[flow] CFlowNode_BattleFrontControl: Couldn't get group with id '%d' from battle front module.", groupID );
			return;
		}

		if(m_enabled)
		{
			if (IsPortActive(activationInformation, EIP_Deactivate))
			{
				m_enabled = false;
				battleFrontGroup->DisableDesignerControlledBattleFront();
			}
			else if (IsPortActive(activationInformation, EIP_Position))
			{
				Vec3 position = GetPortVec3(activationInformation, EIP_Position);	
				battleFrontGroup->SetDesignerControlledBattleFrontAt(position);
			}
		}
		else
		{
			if (IsPortActive(activationInformation, EIP_Activate))
			{
				m_enabled = true;
				Vec3 position = GetPortVec3(activationInformation, EIP_Position);	
				battleFrontGroup->SetDesignerControlledBattleFrontAt(position);
			}
		}

	}
}

void CFlowNode_BattleFrontControl::GetMemoryUsage( ICrySizer * sizer ) const
{
	sizer->Add(*this);
}

// *****************************************************************************

void CFlowNode_SetTurretFaction::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputPortConfig[] = 
	{
		InputPortConfig<string>("Faction", "", _HELP("Faction to set."), 0, "enum_global:Faction"),
		InputPortConfig_Void("Set", _HELP("Set the faction as specified.")),
		{0}
	};

	config.pInputPorts = inputPortConfig;
	config.pOutputPorts = NULL;
	config.sDescription = _HELP("Sets the faction for a turret.");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

void CFlowNode_SetTurretFaction::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Activate:
		{
			if (IsPortActive(pActInfo, InputPort_Set))
				SetFaction(pActInfo);

			break;
		}
	}
}

static CTurret* GetCTurret(IEntity& entity)
{
	const char* entityClassName = entity.GetClass()->GetName();

	if (strcmp(entityClassName, "Turret") == 0)
	{
		if (IEntityComponent* pUserProxy = entity.GetProxy(ENTITY_PROXY_USER))
		{
			CGameObject* pGameObject = static_cast<CGameObject*>(pUserProxy);
			CTurret* pTurret = static_cast<CTurret*>(pGameObject->QueryExtension("Turret"));
			return pTurret;
		}
	}

	return NULL;
}

void CFlowNode_SetTurretFaction::SetFaction(SActivationInfo* pActInfo)
{
	if (IEntity* entity = pActInfo->pEntity)
	{
		if (CGunTurret* turret = GetTurret(*entity))
		{
			uint8 factionID = gEnv->pAISystem->GetFactionMap().GetFactionID(GetPortString(pActInfo, InputPort_Faction));
			turret->SetFaction(factionID);
		}
		else if(CTurret* pCTurret = GetCTurret(*entity))
		{
			uint8 factionID = gEnv->pAISystem->GetFactionMap().GetFactionID(GetPortString(pActInfo, InputPort_Faction));
			pCTurret->SetFactionId(factionID);
		}
		else
		{
			gEnv->pLog->LogError("Entity used for 'SetTurretFaction' is not a Turret");
		}
	}
	else
	{
		gEnv->pLog->LogError("No entity connected to the flow node 'SetTurretFaction'");
	}
}

CGunTurret* CFlowNode_SetTurretFaction::GetTurret(IEntity& entity) const
{
	const char* entityClassName = entity.GetClass()->GetName();

	if ((strcmp(entityClassName, "AutoTurret") == 0) ||
			(strcmp(entityClassName, "HumanTurret") == 0))
	{
		if (IItem* item = gEnv->pGameFramework->GetIItemSystem()->GetItem(entity.GetId()))
		{
			return static_cast<CGunTurret*>(item);
		}
	}

	return NULL;
}




//////////////////////////////////////////////////////////////////////////
// differences to apparently similar nodes:  
// AI:AIAlertness  -> global AI alertness state, not necesarily towards the player. it could increase just by fight between different AI factions, for example.
// AI:Awareness -> essentially same node than this one, but deprecated since Crysis1 and very inefficient. Kept only for backwards compatibility (and probably not needed even for that..well see)
class CFlowNode_AIAwarenessToPlayer : public CFlowBaseNode<eNCT_Instanced>, public CAIAwarenessToPlayerHelper::IListener
{
	enum EOut
	{
		OUT_AWARENESS = 0,
		OUT_GREEN,
		OUT_YELLOW,
		OUT_RED,

		NOPORT // need to be after all others.
	};

	enum EInp
	{
		INP_ENABLED = 0,
	};

public:
	CFlowNode_AIAwarenessToPlayer( SActivationInfo * pActInfo ) : m_lastColourPortTriggered( NOPORT )
	{
		m_actInfo = *pActInfo;
	}

	~CFlowNode_AIAwarenessToPlayer()
	{
		if (g_pGame)
			g_pGame->GetGameAISystem()->GetAIAwarenessToPlayerHelper().RemoveListener( this );
	}

	void Serialize( SActivationInfo * pActInfo, TSerialize ser )
	{
		int val = m_lastColourPortTriggered;
		ser.Value( "m_lastColourPortTriggered", val );
		m_lastColourPortTriggered = (EOut)val;
		m_actInfo = *pActInfo;
		UpdateListenerStatus( pActInfo );
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = 
		{
			InputPortConfig<bool>("Enabled", true, _HELP("To enable/disable the node.")),
			{0}
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<int>("Awareness", _HELP("0-100")),
			OutputPortConfig_Void("green", _HELP("Awareness=0")),
			OutputPortConfig_Void("yellow", _HELP("Awareness between 0-50")),
			OutputPortConfig_Void("red",  _HELP("Awareness >50")),
			{0}
		};    

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Outputs a value that represents the AI enemy awareness of the player. This value closely matches the HUD alertness indicator. \nOutputs are triggered only when they change.");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
			case eFE_Initialize:
			{
				UpdateListenerStatus( pActInfo );
				m_actInfo = *pActInfo;
				m_lastColourPortTriggered = NOPORT;
				break;
			}

			case eFE_Activate:
			{
				if (IsPortActive(pActInfo, INP_ENABLED))
				{
					UpdateListenerStatus( pActInfo );
					bool enabled = GetPortBool( pActInfo, INP_ENABLED );
					if (enabled)
						UpdateOutputs( g_pGame->GetGameAISystem()->GetAIAwarenessToPlayerHelper().GetIntAwareness() );
				}
				break;
			}
		}
	}

	// CAIAwarenessToPlayerHelper::IListener
	void AwarenessChanged( int value )
	{
		UpdateOutputs( value );
	}


	void UpdateListenerStatus( SActivationInfo* pActInfo )
	{
		if (g_pGame)
		{
			bool enabled = GetPortBool( pActInfo, INP_ENABLED );
			if (enabled)
				g_pGame->GetGameAISystem()->GetAIAwarenessToPlayerHelper().AddListener( this );
			else
				g_pGame->GetGameAISystem()->GetAIAwarenessToPlayerHelper().RemoveListener( this );
		}
	}


	void UpdateOutputs( int value )
	{
		ActivateOutput( &m_actInfo, OUT_AWARENESS, value );
		EOut port = OUT_GREEN;
		if (value>0 && value<=50)
			port = OUT_YELLOW;
		else
			if (value>50)
				port = OUT_RED;

		if (port!=m_lastColourPortTriggered)
		{
			ActivateOutput( &m_actInfo, port, true );
			m_lastColourPortTriggered = port;
		}
	}


	virtual void GetMemoryUsage(ICrySizer * s) const
	{
		s->Add(*this);
	}

	IFlowNodePtr Clone(SActivationInfo *pActInfo) { return new CFlowNode_AIAwarenessToPlayer(pActInfo); }

	SActivationInfo m_actInfo;
	EOut m_lastColourPortTriggered;
};


//////////////////////////////////////////////////////////////////////////
class CFlowNode_AIAlertness : public CFlowBaseNode<eNCT_Instanced>, public CAICounter_Alertness::IListener
{
public:
	CFlowNode_AIAlertness( SActivationInfo * pActInfo ) : m_faction(IFactionMap::InvalidFactionID)
	{
		m_actInfo = *pActInfo;
	}
	~CFlowNode_AIAlertness()
	{
		if (g_pGame)
			g_pGame->GetGameAISystem()->GetAICounters().GetAlertnessCounter().RemoveListener( this );
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CFlowNode_AIAlertness(pActInfo);
	}

	void Serialize( SActivationInfo * pActInfo, TSerialize ser )
	{
		if (ser.IsReading())
		{
			m_actInfo = *pActInfo;
			ReadFactionFromInp( pActInfo );
		}
		UpdateListenerStatus( pActInfo );
	}


	enum EFilterAI
	{
		FAI_ALL = 0,
		FAI_ENEMIES,
		FAI_FRIENDS,
		FAI_FACTION
	};

	enum EInp
	{
		IN_FILTERAI = 0,
		IN_FACTION,
		IN_ENABLED,
		IN_CHECK,
	};

	enum EOut
	{
		OUT_ALERTNESS = 0,
		OUT_GREEN,
		OUT_ORANGE,
		OUT_RED
	};

	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig inp_config[] = 
		{
			InputPortConfig<int>("FilterAI", 0, _HELP("Filter which AIs are used for the alertness value."), _HELP("FilterAI"),_UICONFIG("enum_int:All=0,Enemies=1,Friends=2,Faction=3")),
			InputPortConfig<string>("Faction", "", _HELP("Only used when 'FilterAI' input is set to 'Faction'. )."), 0, "enum_global:Faction"),
			InputPortConfig<bool>("Enabled", true, _HELP("To enable/disable the node.")),
			InputPortConfig_Void("Check", _HELP("instant check.")),
			{0}
		};

		static const SOutputPortConfig out_config[] = 
		{
			OutputPortConfig<int>("alertness", "0 - green\n1 - orange\n2 - red"),
			OutputPortConfig_Void("green"),
			OutputPortConfig_Void("orange"),
			OutputPortConfig_Void("red"),
			{0}
		};

		config.sDescription = _HELP( "The highest level of alertness of the specified AIs" );
		config.pInputPorts = inp_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
			case eFE_Initialize:
			{
				UpdateListenerStatus( pActInfo );
				m_actInfo = *pActInfo;
				ReadFactionFromInp( pActInfo );
				break;
			}

			case eFE_Activate:
			{
				if (IsPortActive( pActInfo, IN_FACTION ))
					ReadFactionFromInp( pActInfo );

				if (IsPortActive(pActInfo, IN_ENABLED))
				{
					UpdateListenerStatus( pActInfo ); // this will automatically cause an AlertnessChanged() call from the AiCounters, so we dont need to manually update in this case
				}

				if (IsPortActive(pActInfo, IN_CHECK))
				{
					UpdateOutputs( g_pGame->GetGameAISystem()->GetAICounters().GetAlertnessCounter().GetAlertnessGlobal() );
				}
				break;
			}
		}
	}

	// CAICounter_Alertness::IListener
	void AlertnessChanged( int alertnessGlobal )
	{
		UpdateOutputs( alertnessGlobal );
	}

	void UpdateOutputs( int alertnessGlobal )
	{
		EFilterAI filterAI = (EFilterAI)(GetPortInt( &m_actInfo, IN_FILTERAI ));
		int alertness = 0;

		switch (filterAI)
		{
			case FAI_ALL:
				alertness = alertnessGlobal;
				break;

			case FAI_ENEMIES:
				alertness = g_pGame->GetGameAISystem()->GetAICounters().GetAlertnessCounter().GetAlertnessEnemies();
				break;

			case FAI_FRIENDS:
				alertness = g_pGame->GetGameAISystem()->GetAICounters().GetAlertnessCounter().GetAlertnessFriends();
				break;

			case FAI_FACTION:
			{
				if (m_faction!=IFactionMap::InvalidFactionID)
				{
					alertness = g_pGame->GetGameAISystem()->GetAICounters().GetAlertnessCounter().GetAlertnessFaction( m_faction );
				}
				break;
			}
		}

		ActivateOutput( &m_actInfo, OUT_ALERTNESS, alertness );
		uint32 colorPort = clamp_tpl( alertness+OUT_GREEN, (int)OUT_GREEN, (int)OUT_RED );
		ActivateOutput( &m_actInfo, colorPort, true );
	}

	void ReadFactionFromInp( SActivationInfo *pActInfo )
	{
		m_faction = gEnv->pAISystem->GetFactionMap().GetFactionID( GetPortString( pActInfo, IN_FACTION ).c_str() );
	}


	void UpdateListenerStatus( SActivationInfo* pActInfo )
	{
		if (g_pGame)
		{
			bool enabled = GetPortBool( pActInfo, IN_ENABLED );
			if (enabled)
				g_pGame->GetGameAISystem()->GetAICounters().GetAlertnessCounter().AddListener( this );
			else
				g_pGame->GetGameAISystem()->GetAICounters().GetAlertnessCounter().RemoveListener( this );
		}
	}

	virtual void GetMemoryUsage(ICrySizer * s) const
	{
		s->Add(*this);
	}

	SActivationInfo m_actInfo;
	uint32 m_faction;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CFlowNode_AICorpses::GetConfiguration( SFlowNodeConfig& config )
{
	static const SInputPortConfig inp_config[] = 
	{
		InputPortConfig_Void("CleanCorpses", _HELP("Removes all corpses in the level")),
		{0}
	};

	config.sDescription = _HELP( "AI corpse management" );
	config.pInputPorts = inp_config;
	config.pOutputPorts = NULL;
	config.SetCategory(EFLN_APPROVED);
}

void CFlowNode_AICorpses::ProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	if(event == eFE_Activate)
	{
		if(IsPortActive(pActInfo, InputPort_CleanCorpses))
		{
			CAICorpseManager* pAICorpseManager = CAICorpseManager::GetInstance();
			if(pAICorpseManager != NULL)
			{
				pAICorpseManager->RemoveAllCorpses( "FlowNode_AICorpses" );
			}
		}
	}
}

void CFlowNode_AICorpses::GetMemoryUsage( ICrySizer * sizer ) const
{
	sizer->Add(*this);
}

REGISTER_FLOW_NODE("AI:BattleFrontControl", CFlowNode_BattleFrontControl)
REGISTER_FLOW_NODE("AI:SetTurretFaction", CFlowNode_SetTurretFaction);
REGISTER_FLOW_NODE("AI:AIAwarenessToPlayer", CFlowNode_AIAwarenessToPlayer);
REGISTER_FLOW_NODE("AI:AlertnessState",CFlowNode_AIAlertness )
REGISTER_FLOW_NODE("AI:Corpses", CFlowNode_AICorpses );
