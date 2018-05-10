// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlowHudEventsNodes.cpp
//  Version:     v1.00
//  Created:     August 8th 2013 by Michiel Meesters.
//  Compilers:   Visual Studio.NET 2012
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "UI/HUD/HUDMissionObjectiveSystem.h"
#include "Game.h"
#include "UI/UIManager.h"
#include "UI/HUD/HUDSilhouettes.h"

#include <CryFlowGraph/IFlowBaseNode.h>

#define HUDEVENT_UICONFIG_ENTRY_FIRST(x)      string("enum_int:" # x "=").append(ToString((int)x))
#define HUDEVENT_UICONFIG_ENTRY_ADDITIONAL(x) .append(string("," # x "=").append(ToString((int)x)))

class CFlowNode_MissionStateListener : public CFlowBaseNode<eNCT_Instanced>, IHUDEventListener
{
private:
	SActivationInfo m_pActInfo;
public:
	//////////////////////////////////////////////////////////////////////////
	CFlowNode_MissionStateListener(SActivationInfo* pActInfo) { m_pActInfo = *pActInfo; }

	//////////////////////////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_MissionStateListener(pActInfo);
	}

	//////////////////////////////////////////////////////////////////////////
	virtual ~CFlowNode_MissionStateListener()
	{
		CHUDEventDispatcher::RemoveHUDEventListener(this);
	}

	//////////////////////////////////////////////////////////////////////////
	enum OutputPorts
	{
		eOP_StateChanged = 0,
		eOP_NamePrimary,
		eOP_DescPrimary,
		eOP_NameSecondary,
		eOP_DescSecondary,
		eOP_State,
		eOP_IsSecondary
	};

	//////////////////////////////////////////////////////////////////////////
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		// declare input ports
		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_Void("StateChanged",            "Fires when a mission's state is changed"),
			OutputPortConfig<string>("PrimaryName",          "Mission name specified in Objectives.xml"),
			OutputPortConfig<string>("PrimaryDescription",   "Mission description specified in Objectives.xml"),
			OutputPortConfig<string>("SecondaryName",        "Mission name specified in Objectives.xml"),
			OutputPortConfig<string>("SecondaryDescription", "Mission description specified in Objectives.xml"),
			OutputPortConfig<int>("Status",                  "0=deactivated, 1=completed, 2=failed, 3=activated"),
			OutputPortConfig<bool>("IsSecondary",            "0=primary, 1=secondary"),
			{ 0 }
		};

		// we set pointers in "config" here to specify which input and output ports the node contains
		config.sDescription = _HELP("Fires whenever a mission's state has been changed");
		config.pInputPorts = 0;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	//////////////////////////////////////////////////////////////////////////
	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void OnHUDEvent(const SHUDEvent& event)
	{
		// Since I only handle OnObjectiveChanged I only expect a void pointer and a boolean, fail in other cases.
		CHUDMissionObjective* pMissionObjective = NULL;
		bool bIsSilent = false;

		if (event.eventType == eHUDEvent_OnObjectiveChanged)
		{
			for (unsigned int i = 0; i < event.GetDataSize(); i++)
			{
				switch (event.GetData(i).m_type)
				{
				case SHUDEventData::eSEDT_voidptr:
					pMissionObjective = (CHUDMissionObjective*)event.GetData(i).GetPtr();
					break;
				case SHUDEventData::eSEDT_bool:
					bIsSilent = event.GetData(i).GetBool();
					break;
				case SHUDEventData::eSEDT_int:
					// not necessary now
					break;
				case SHUDEventData::eSEDT_float:
					// not necessary now
					break;
				case SHUDEventData::eSEDT_undef:
				default:
					CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, "[CFlowNode_MissionStateListener] HudEvent data unknown.");
					break;
				}
			}
			if (pMissionObjective && !bIsSilent)
			{
				// activate outputport
				ActivateOutput(&m_pActInfo, eOP_StateChanged, 1);
				ActivateOutput<string>(&m_pActInfo, !pMissionObjective->IsSecondary() ? eOP_NamePrimary : eOP_NameSecondary, string(pMissionObjective->GetShortDescription()));
				ActivateOutput<string>(&m_pActInfo, !pMissionObjective->IsSecondary() ? eOP_DescPrimary : eOP_DescSecondary, string(pMissionObjective->GetMessage()));
				ActivateOutput<int>(&m_pActInfo, eOP_State, pMissionObjective->GetStatus());
				ActivateOutput<bool>(&m_pActInfo, eOP_IsSecondary, pMissionObjective->IsSecondary());
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Initialize)
		{
			m_pActInfo = *pActInfo;
			CHUDEventDispatcher::AddHUDEventListener(this, "OnObjectiveChanged");
		}
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowNode_EntityTrackedListener : public CFlowBaseNode<eNCT_Instanced>, IHUDEventListener
{
private:
	SActivationInfo m_pActInfo;
	std::list<std::tuple<EHUDEventType, EntityId, bool>> m_queuedEvents;
public:
	//////////////////////////////////////////////////////////////////////////
	CFlowNode_EntityTrackedListener(SActivationInfo* pActInfo) { m_pActInfo = *pActInfo; }

	//////////////////////////////////////////////////////////////////////////
	IFlowNodePtr Clone(SActivationInfo* pActInfo) override
	{
		return new CFlowNode_EntityTrackedListener(pActInfo);
	}

	//////////////////////////////////////////////////////////////////////////
	virtual ~CFlowNode_EntityTrackedListener()
	{
		CHUDEventDispatcher::RemoveHUDEventListener(this);
	}

	//////////////////////////////////////////////////////////////////////////
	enum InputPorts
	{
		eIP_MissionOnly = 0,
		eIP_Class,
		eIP_CustomClass
	};

	enum OutputPorts
	{
		eOP_EntityAdded = 0,
		eOP_EntityRemoved,
		eOP_EntityId
	};

	//////////////////////////////////////////////////////////////////////////
	void GetConfiguration(SFlowNodeConfig& config) override
	{
		// declare input ports
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig<bool>("MissionOnly",     "Only trigger on events involving mission tracked entities"),
			InputPortConfig<string>("Class",         "AllClasses",                                                                 "Class you want to filter on. For custom classes, use CustomClasses input",0, _UICONFIG("enum_global:entity_classes")),
			InputPortConfig<string>("CustomClasses", "Optional: Add classes you want to filter the entities on (comma separated)"),
			{ 0 }
		};

		// declare output ports
		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_Void("EntityAdded",   "Fires when a entity is added to the radar from the Mission"),
			OutputPortConfig_Void("EntityRemoved", "Fires when a entity is removed from the radar by a Mission"),
			OutputPortConfig<EntityId>("Entity",   "The entity that needs adding"),
			{ 0 }
		};

		// we set pointers in "config" here to specify which input and output ports the node contains
		config.sDescription = _HELP("Fires when entities are added or removed to the radar from a mission");
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	//////////////////////////////////////////////////////////////////////////
	inline void splitStringList(std::vector<string>* result, const char* str, char delimeter)
	{
		result->clear();

		const char* ptr = str;
		for (; *ptr; ++ptr)
		{
			if (*ptr == delimeter)
			{
				result->push_back(string(str, ptr));
				str = ptr + 1;
			}
		}
		result->push_back(string(str, ptr));
	}

	//////////////////////////////////////////////////////////////////////////
	void Serialize(SActivationInfo* pActInfo, TSerialize ser) override
	{
	}

	//////////////////////////////////////////////////////////////////////////
	bool IsClassAllowed(const char* entClass)
	{
		const string classFilter = GetPortString(&m_pActInfo, eIP_Class);
		if ((classFilter == "AllClasses") || (classFilter == entClass))
		{
			return true;
		}
		else if (classFilter == "CustomClasses")
		{
			string customClasses = GetPortString(&m_pActInfo, eIP_CustomClass);
			std::vector<string> CustomClassList;
			splitStringList(&CustomClassList, customClasses.c_str(), ',');
			for (unsigned int i = 0; i < CustomClassList.size(); i++)
			{
				if (entClass == CustomClassList[i])
					return true;
			}

		}

		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	void OnHUDEvent(const SHUDEvent& event) override
	{
		if (event.eventType == eHUDEvent_AddEntity || event.eventType == eHUDEvent_RemoveEntity)
		{
			EntityId entityId = INVALID_ENTITYID;
			for (unsigned int i = 0; i < event.GetDataSize(); ++i)
			{
				switch (event.GetData(i).m_type)
				{
				case SHUDEventData::eSEDT_voidptr:
					break;
				case SHUDEventData::eSEDT_bool:
					break;
				case SHUDEventData::eSEDT_int:
					entityId = event.GetData(i).GetInt();
					break;
				case SHUDEventData::eSEDT_float:
					break;
				case SHUDEventData::eSEDT_undef:
				default:
					CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, "[CFlowNode_EntityTrackedListener] HudEvent data unknown.");
					break;
				}
			}
			const bool bMissionOnly = GetPortBool(&m_pActInfo, eIP_MissionOnly);

			if (m_queuedEvents.empty())
			{
				m_pActInfo.pGraph->SetRegularlyUpdated(m_pActInfo.myID, true);
			}

			m_queuedEvents.push_back(std::make_tuple(event.eventType, entityId, bMissionOnly));
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void HandleEvent(const EHUDEventType eventType, const EntityId entityId, const bool bMissionOnly)
	{
		if (entityId != INVALID_ENTITYID)
		{
			if (IEntity* const pEntity = gEnv->pEntitySystem->GetEntity(entityId))
			{
				const string entityClassName = pEntity->GetClass()->GetName();
				bool bTrigger = IsClassAllowed(entityClassName);
				if (bMissionOnly)
				{
					const CHUDMissionObjective* const pMO = g_pGame->GetMOSystem()->GetMissionObjectiveByEntityId(entityId);
					bTrigger = bTrigger && pMO != nullptr;
				}

				if (bTrigger)
				{
					ActivateOutput(&m_pActInfo, eventType == eHUDEvent_AddEntity ? eOP_EntityAdded : eOP_EntityRemoved, 1);
					ActivateOutput<EntityId>(&m_pActInfo, eOP_EntityId, entityId);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		if (event == eFE_Initialize)
		{
			m_pActInfo = *pActInfo;
			CHUDEventDispatcher::AddHUDEventListener(this, "AddEntity");
			CHUDEventDispatcher::AddHUDEventListener(this, "RemoveEntity");
			m_queuedEvents.clear();
		}
		else if (event == eFE_Update)
		{
			if (!m_queuedEvents.empty())
			{
				EHUDEventType eventType;
				EntityId entityId;
				bool bMissionOnly;
				std::tie(eventType, entityId, bMissionOnly) = m_queuedEvents.front();

				HandleEvent(eventType, entityId, bMissionOnly);
				m_queuedEvents.pop_front();
			}
			
			if (m_queuedEvents.empty())
			{
				m_pActInfo.pGraph->SetRegularlyUpdated(m_pActInfo.myID, false);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
	}
};

class CFlowNode_BattleAreaListener : public CFlowBaseNode<eNCT_Instanced>, IHUDEventListener
{
private:
	SActivationInfo m_pActInfo;
public:
	//////////////////////////////////////////////////////////////////////////
	CFlowNode_BattleAreaListener(SActivationInfo* pActInfo) { m_pActInfo = *pActInfo; }

	//////////////////////////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_BattleAreaListener(pActInfo);
	}

	//////////////////////////////////////////////////////////////////////////
	virtual ~CFlowNode_BattleAreaListener()
	{
		CHUDEventDispatcher::RemoveHUDEventListener(this);
	}

	//////////////////////////////////////////////////////////////////////////
	enum OutputPorts
	{
		eOP_EventFired = 0,
		eOP_DeathTimer,
	};

	//////////////////////////////////////////////////////////////////////////
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		// declare input ports
		static const string uiconfig = HUDEVENT_UICONFIG_ENTRY_FIRST(eHUDEvent_LeavingBattleArea) HUDEVENT_UICONFIG_ENTRY_ADDITIONAL(eHUDEvent_ReturningToBattleArea);

		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig<int>("Event", 2, _HELP(""), "Event", uiconfig.c_str()),
			{ 0 }
		};

		// declare output ports
		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_Void("EventFired",   "Fires when the event occurs"),
			OutputPortConfig<float>("DeathTimer", "Outputs current game time plus specified Delay time in ForbiddenArea entity"),
			{ 0 }
		};

		// we set pointers in "config" here to specify which input and output ports the node contains
		config.sDescription = _HELP("Used to catch HUD events about leaving and entering battle area");
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	//////////////////////////////////////////////////////////////////////////
	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void OnHUDEvent(const SHUDEvent& event)
	{
		int eventID = GetPortInt(&m_pActInfo, 0);
		if (event.eventType == eventID)
		{
			ActivateOutput(&m_pActInfo, eOP_EventFired, 1);
			for (unsigned int i = 0; i < event.GetDataSize(); i++)
			{
				switch (event.GetData(i).m_type)
				{
				case SHUDEventData::eSEDT_voidptr:
					break;
				case SHUDEventData::eSEDT_bool:
					break;
				case SHUDEventData::eSEDT_int:
					break;
				case SHUDEventData::eSEDT_float:
					{
						if (eventID == eHUDEvent_LeavingBattleArea)
						{
							float fDeathTimer = event.GetData(i).GetFloat();
							ActivateOutput(&m_pActInfo, eOP_DeathTimer, fDeathTimer);
						}
					}
					break;
				case SHUDEventData::eSEDT_undef:
				default:
					CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, "[CFlowNode_BattleAreaListener] HudEvent data unknown.");
					break;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Initialize)
		{
			m_pActInfo = *pActInfo;
			CHUDEventDispatcher::AddHUDEventListener(this, eHUDEvent_LeavingBattleArea);
			CHUDEventDispatcher::AddHUDEventListener(this, eHUDEvent_ReturningToBattleArea);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowNode_HUDSilhouettes : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		EIP_Activate = 0,
		EIP_Deactivate,
		EIP_Color,
	};

public:
	CFlowNode_HUDSilhouettes(SActivationInfo* pActInfo) {}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Activate",   _HELP("Trigger to activate. This sets a permanent silhouette (until removed), overwriting automated ones.")),
			InputPortConfig_Void("Deactivate", _HELP("Trigger to deactivate")),
			InputPortConfig<Vec3>("Color",     Vec3(1.0f,                                                                                                   0.0f,0.0f), _HELP("Color"), 0, _UICONFIG("dt=clr")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.sDescription = _HELP("HUD Silhouette Shader");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (eFE_Activate == event)
		{
			if (!pActInfo->pEntity)
				return;

			const bool activate = IsPortActive(pActInfo, EIP_Activate);
			const bool deactivate = IsPortActive(pActInfo, EIP_Deactivate);
			if (!activate && !deactivate)
				return;

			if (deactivate)
				g_pGame->GetUI()->GetSilhouettes()->ResetFlowGraphSilhouette(pActInfo->pEntity->GetId());

			if (activate)
			{
				const Vec3& color = GetPortVec3(pActInfo, EIP_Color);
				g_pGame->GetUI()->GetSilhouettes()->SetFlowGraphSilhouette(pActInfo->pEntity, color.x, color.y, color.z, 1.0f, -1.0f);
			}
		}
	}
};

class CFlowNode_SendHUDEvent : public CFlowBaseNode<eNCT_Instanced>
{
	enum eInputs
	{
		EIP_Trigger = 0,
		EIP_Event
	};

	enum eOutputs
	{
		EOP_Sent = 0,
		EOP_Event
	};

public:
	CFlowNode_SendHUDEvent(SActivationInfo* pActInfo) {}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) override
	{
		return new CFlowNode_SendHUDEvent(pActInfo);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config) override
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Trigger",  _HELP("")),
			InputPortConfig<string>("Event", "",        "Event to send",0, _UICONFIG("enum_global:hud_events")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig<EntityId>("Sent",    _HELP("Outputs the entityId of the sender, if successful")),
			OutputPortConfig<string>("EventName", "Name of the event, that just got sent"),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Send a HUD Event");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		if (eFE_Activate == event)
		{
			if (IsPortActive(pActInfo, EIP_Trigger))
			{
				string inputEvent = GetPortString(pActInfo, EIP_Event);
				if (inputEvent.length() > 0)
				{
					EHUDEventType hudEventType = CHUDEventDispatcher::GetEvent(inputEvent);
					if (hudEventType > eHUDEvent_None && hudEventType < eHUDEvent_LAST)
					{
						SHUDEvent hudEvent(hudEventType);
						EntityId entityId = INVALID_ENTITYID;

						if (pActInfo->pEntity)
						{
							entityId = pActInfo->pEntity->GetId();
							hudEvent.AddData(SHUDEventData((int)entityId));
						}

						CHUDEventDispatcher::CallEvent(hudEvent);
						ActivateOutput(pActInfo, EOP_Sent, entityId);
						ActivateOutput(pActInfo, EOP_Event, inputEvent);
					}
				}
			}
		}
	}
};

class CFlowNode_HUDEventListener : public CFlowBaseNode<eNCT_Instanced>, IHUDEventListener
{
private:
	SActivationInfo m_pActInfo;

	enum eInputs
	{
		EIP_Enable = 0,
		EIP_Disable,
		EIP_Event,
		EIP_ClassName,
		EIP_CustomClass
	};

	enum eOutputs
	{
		EOP_EventFired = 0,
		EOP_EventName,
		EOP_EventParameter
	};

public:
	CFlowNode_HUDEventListener(SActivationInfo* pActInfo)
	{
		m_pActInfo = *pActInfo;
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) override
	{
		return new CFlowNode_HUDEventListener(pActInfo);
	}

	virtual ~CFlowNode_HUDEventListener()
	{
		CHUDEventDispatcher::RemoveHUDEventListener(this);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config) override
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Enable",           _HELP("")),
			InputPortConfig_Void("Disable",          _HELP("")),
			InputPortConfig<string>("Event",         "",                                                                           "Event to listen for",                                                       0, _UICONFIG("enum_global:hud_events")),
			InputPortConfig<string>("Class",         "AllClasses",                                                                 "Class you want to filter on. For custom classes, use CustomClasses input",  0, _UICONFIG("enum_global:entity_classes")),
			InputPortConfig<string>("CustomClasses", "Optional: Add classes you want to filter the entities on (comma separated)"),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig<EntityId>("Fired",   _HELP("Outputs the entityId of the sender")),
			OutputPortConfig<string>("EventName", "Name of the event"),
			OutputPortConfig<string>("Parameter", "Comma separated string"),
			{ 0 }
		};

		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Listen to a specific HUD Event");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		if (event == eFE_Initialize)
		{
			m_pActInfo = *pActInfo;
		}

		if (eFE_Activate == event)
		{
			if (IsPortActive(pActInfo, EIP_Enable))
			{
				string outputEvent = GetPortString(pActInfo, EIP_Event);
				if (outputEvent.length() > 0)
				{
					CHUDEventDispatcher::AddHUDEventListener(this, outputEvent);
				}
			}
			else if (IsPortActive(pActInfo, EIP_Disable))
			{
				CHUDEventDispatcher::RemoveHUDEventListener(this);
			}
		}
	}

	virtual void OnHUDEvent(const SHUDEvent& event) override
	{
		const EntityId entityId = event.GetDataSize() > 0 ? event.GetData(0).GetInt() : INVALID_ENTITYID;
		const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
		const char* szEntityClassName = pEntity ? pEntity->GetClass()->GetName() : "";

		if (IsClassAllowed(szEntityClassName))
		{
			string outputParameter = "";
			for (unsigned int i = 0; i < event.GetDataSize(); i++)
			{
				if (i > 0)
				{
					outputParameter.append(",");
				}

				string addParam = "";

				switch (event.GetData(i).m_type)
				{
				case SHUDEventData::eSEDT_bool:
					event.GetData(i).GetBool() ? addParam.append("1") : addParam.append("0");
					break;
				case SHUDEventData::eSEDT_int:
					addParam.Format("%i", event.GetData(i).GetInt());
					break;
				case SHUDEventData::eSEDT_float:
					addParam.Format("%f", event.GetData(i).GetFloat());
					break;
				case SHUDEventData::eSEDT_undef:
				default:
					CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, "[CFlowNode_HUDEventListener] HudEvent data unknown.");
					break;
				}
				outputParameter.append(addParam);
			}

			ActivateOutput(&m_pActInfo, EOP_EventFired, entityId);
			ActivateOutput(&m_pActInfo, EOP_EventName, CHUDEventDispatcher::GetEventName(event.eventType));
			ActivateOutput(&m_pActInfo, EOP_EventParameter, outputParameter);
		}
	}

	bool IsClassAllowed(const char* szClassName)
	{
		const string classFilter = GetPortString(&m_pActInfo, EIP_ClassName);
		if ((classFilter != "AllClasses") || (classFilter != szClassName))
		{
			return true;
		}
		else if (classFilter == "CustomClasses")
		{
			std::vector<string> customClassList;
			string customClasses = GetPortString(&m_pActInfo, EIP_CustomClass);
			SplitStringList(customClassList, customClasses.c_str(), ",");

			for (unsigned int i = 0; i < customClassList.size(); i++)
			{
				if (!strcmp(szClassName, customClassList[i]))
					return true;
			}
		}
		return false;
	}

	void SplitStringList(std::vector<string>& result, const char* szFullString, const char* szDelimiter)
	{
		result.clear();

		const char* ptr = szFullString;
		for (; *ptr; ++ptr)
		{
			if (*ptr == szDelimiter[0])
			{
				result.push_back(string(szFullString, ptr));
				szFullString = ptr + 1;
			}
		}
		result.push_back(string(szFullString, ptr));
	}
};

REGISTER_FLOW_NODE("HUD:SendHUDEvent", CFlowNode_SendHUDEvent);
REGISTER_FLOW_NODE("HUD:HUDEventListener", CFlowNode_HUDEventListener);
REGISTER_FLOW_NODE("HUD:MissionStateListener", CFlowNode_MissionStateListener);
REGISTER_FLOW_NODE("HUD:EntityTrackedListener", CFlowNode_EntityTrackedListener);
REGISTER_FLOW_NODE("HUD:BattleAreaListener", CFlowNode_BattleAreaListener);
REGISTER_FLOW_NODE("HUD:SilhouetteOutline", CFlowNode_HUDSilhouettes);
