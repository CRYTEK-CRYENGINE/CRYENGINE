// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   History:
   - 23:04:2010   Created by Ian Masters
*************************************************************************/

#include "StdAfx.h"
#include "UI/HUD/HUDEventDispatcher.h"

#include <CrySystem/ISystem.h>
#include <CryAnimation/ICryAnimation.h>
#include <IViewSystem.h>
#include <CryFlowGraph/IFlowBaseNode.h>

class CTacticalScanNode : public CFlowBaseNode<eNCT_Instanced>, IHUDEventListener
{
	enum INPUTS
	{
		EIP_Enable = 0,
		EIP_Disable,
	};

	enum OUTPUTS
	{
		EOP_EntityID = 0,
	};

public:
	CTacticalScanNode(SActivationInfo* pActInfo)
		: m_entityId(0)
		, m_enabled(false)
	{
		CHUDEventDispatcher::AddHUDEventListener(this, "OnEntityScanned");
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	~CTacticalScanNode()
	{
		CHUDEventDispatcher::RemoveHUDEventListener(this);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CTacticalScanNode(pActInfo);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.BeginGroup("Local");

		ser.Value("m_entityId", m_entityId);
		ser.Value("m_enabled", m_enabled);

		if (ser.IsReading())
		{
			CHUDEventDispatcher::AddHUDEventListener(this, "OnEntityScanned"); // can't hurt to do this on load as the listener uses stl::push_back_unique
			if (m_enabled)
			{
				m_actInfo = *pActInfo;
				ActivateOutput(&m_actInfo, EOP_EntityID, m_entityId);
			}
		}

		ser.EndGroup();
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Enable",  _HELP("Trigger to enable the node")),
			InputPortConfig_Void("Disable", _HELP("Trigger to disable the node")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<EntityId>("EntityId", _HELP("ID of the entity being scanned")),
			{ 0 }
		};
		config.sDescription = _HELP("Gets the Id of the entity currently being tactical scanned");
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
				m_actInfo = *pActInfo;
				break;
			}

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Enable)) m_enabled = true;
				else if (IsPortActive(pActInfo, EIP_Disable)) m_enabled = false;
				break;
			}
		}
	}

protected:

	void OnHUDEvent(const SHUDEvent& event)
	{
		switch (event.eventType)
		{
		case eHUDEvent_OnEntityScanned:
			{
				EntityId id = static_cast<EntityId>(event.GetData(0).m_int);
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
				if (!pEntity)
					break;

				m_entityId = id;

				if (m_enabled)
					ActivateOutput(&m_actInfo, EOP_EntityID, m_entityId);
				break;
			}
		}
	}

private:

	SActivationInfo m_actInfo;
	EntityId        m_entityId;
	bool            m_enabled;
};

class CTacticalScanStartNode : public CFlowBaseNode<eNCT_Instanced>, IHUDEventListener
{
	enum INPUTS
	{
		EIP_Enable = 0,
		EIP_DelayResult,
		EIP_Disable,
	};

	enum OUTPUTS
	{
		EOP_OnEvent = 0,
		EOP_EntityID,
	};

public:
	CTacticalScanStartNode(SActivationInfo* pActInfo)
		: m_entityId(0)
		, m_enabled(false)
		, m_delayResult(false)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	~CTacticalScanStartNode()
	{
		SetEnabled(false);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CTacticalScanStartNode(pActInfo);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.BeginGroup("Local");

		ser.Value("m_entityId", m_entityId);
		ser.Value("m_delayResult", m_delayResult);
		bool bEnabled = m_enabled;
		ser.Value("m_enabled", bEnabled);

		if (ser.IsReading())
		{
			if (bEnabled)
			{
				m_actInfo = *pActInfo;
			}
			SetEnabled(bEnabled); // can't hurt to do this on load as the listener uses stl::push_back_unique
		}

		ser.EndGroup();
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Enable",       _HELP("Trigger to enable the node")),
			InputPortConfig<bool>("DelayResult", false,                                _HELP("Prevents scan from completing normally (Must use Entity:TacticalScanCurrentControl)")),
			InputPortConfig_Void("Disable",      _HELP("Trigger to disable the node")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("OnEvent",       _HELP("Triggered when event with specified entity occurs")),
			OutputPortConfig<EntityId>("EntityId", _HELP("ID of the entity that started being scanned")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Gets the Id of the entity that just started being tactical scanned");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				IEntity* pEntity = pActInfo->pEntity;
				if (!pEntity)
					return;

				m_delayResult = GetPortBool(pActInfo, EIP_DelayResult);

				if (IsPortActive(pActInfo, EIP_Enable))
				{
					m_actInfo = *pActInfo;
					m_entityId = pEntity->GetId();
					SetEnabled(true);
				}
				else if (IsPortActive(pActInfo, EIP_Disable))
				{
					SetEnabled(false);
				}
				break;
			}
		}
	}

protected:
	void SetEnabled(bool bEnable)
	{
		if (m_enabled == bEnable)
			return;

		m_enabled = bEnable;
		if (bEnable)
		{
			CHUDEventDispatcher::AddHUDEventListener(this, "OnScanningStart");
		}
		else
		{
			CHUDEventDispatcher::RemoveHUDEventListener(this);
		}
	}

	void OnHUDEvent(const SHUDEvent& event)
	{
		switch (event.eventType)
		{
		case eHUDEvent_OnScanningStart:
			{
				if (m_enabled)
				{
					EntityId scannedEntityId = static_cast<EntityId>(event.GetData(0).m_int);

					if (scannedEntityId != m_entityId) // Only selected entity
						break;

					IEntity* pScannedEntity = gEnv->pEntitySystem->GetEntity(scannedEntityId);
					if (!pScannedEntity)
					{
						SetEnabled(false);
						break;
					}

					if (m_delayResult)
					{
						SHUDEvent currentEvent(eHUDEvent_OnControlCurrentTacticalScan);
						currentEvent.AddData(SHUDEventData(true)); // Delay result
						CHUDEventDispatcher::CallEvent(currentEvent);
					}

					ActivateOutput(&m_actInfo, EOP_OnEvent, true);
					ActivateOutput(&m_actInfo, EOP_EntityID, m_entityId);
				}
				break;
			}
		}
	}

private:

	SActivationInfo m_actInfo;
	EntityId        m_entityId;
	bool            m_enabled;
	bool            m_delayResult;
};

class CTacticalScanCompleteNode : public CFlowBaseNode<eNCT_Instanced>, IHUDEventListener
{
	enum INPUTS
	{
		EIP_Enable = 0,
		EIP_DelayResult,
		EIP_Disable,
	};

	enum OUTPUTS
	{
		EOP_OnEvent = 0,
		EOP_EntityID,
	};

public:
	CTacticalScanCompleteNode(SActivationInfo* pActInfo)
		: m_entityId(0)
		, m_enabled(false)
		, m_delayResult(false)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	~CTacticalScanCompleteNode()
	{
		SetEnabled(false);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CTacticalScanCompleteNode(pActInfo);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.BeginGroup("Local");

		ser.Value("m_entityId", m_entityId);
		ser.Value("m_delayResult", m_delayResult);

		if (ser.IsReading())
		{
			bool bEnabled = false;
			ser.Value("m_enabled", bEnabled);

			if (bEnabled)
			{
				m_actInfo = *pActInfo;
			}

			SetEnabled(bEnabled); // can't hurt to do this on load as the listener uses stl::push_back_unique
		}
		else
		{
			ser.Value("m_enabled", m_enabled);
		}

		ser.EndGroup();
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Enable",       _HELP("Trigger to enable the node")),
			InputPortConfig<bool>("DelayResult", false,                                _HELP("Prevents scan from completing normally (Must use Entity:TacticalScanCurrentControl)")),
			InputPortConfig_Void("Disable",      _HELP("Trigger to disable the node")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("OnEvent",       _HELP("Triggered when event with specified entity occurs")),
			OutputPortConfig<EntityId>("EntityId", _HELP("ID of the entity that just completed being scanned")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Gets the Id of the entity that just completed (But before success or failure) being tactical scanned");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				IEntity* pEntity = pActInfo->pEntity;
				if (!pEntity)
					return;

				m_delayResult = GetPortBool(pActInfo, EIP_DelayResult);

				if (IsPortActive(pActInfo, EIP_Enable))
				{
					m_actInfo = *pActInfo;
					m_entityId = pEntity->GetId();
					SetEnabled(true);
				}
				else if (IsPortActive(pActInfo, EIP_Disable))
				{
					SetEnabled(false);
				}
				break;
			}
		}
	}

protected:
	void SetEnabled(bool bEnable)
	{
		if (m_enabled == bEnable)
			return;

		m_enabled = bEnable;
		if (bEnable)
		{
			CHUDEventDispatcher::AddHUDEventListener(this, "OnScanningComplete");
		}
		else
		{
			CHUDEventDispatcher::RemoveHUDEventListener(this);
		}
	}

	void OnHUDEvent(const SHUDEvent& event)
	{
		switch (event.eventType)
		{
		case eHUDEvent_OnScanningComplete:
			{
				if (m_enabled)
				{
					EntityId scannedEntityId = static_cast<EntityId>(event.GetData(0).m_int);

					if (scannedEntityId != m_entityId) // Only selected entity
						break;

					IEntity* pScannedEntity = gEnv->pEntitySystem->GetEntity(scannedEntityId);
					if (!pScannedEntity)
					{
						SetEnabled(false);
						break;
					}

					if (m_delayResult)
					{
						SHUDEvent _event(eHUDEvent_OnControlCurrentTacticalScan);
						_event.AddData(SHUDEventData(true)); // Delay result
						CHUDEventDispatcher::CallEvent(_event);
					}

					ActivateOutput(&m_actInfo, EOP_OnEvent, true);
					ActivateOutput(&m_actInfo, EOP_EntityID, m_entityId);
				}
				break;
			}
		}
	}

private:

	SActivationInfo m_actInfo;
	EntityId        m_entityId;
	bool            m_enabled;
	bool            m_delayResult;
};

class CTacticalScanCurrentControlNode : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		EIP_FailCurrentScan = 0,
		EIP_SucceedCurrentScan,
	};

	enum OUTPUTS
	{
		EOP_Failed = 0,
		EOP_Succeeded,
	};

public:
	CTacticalScanCurrentControlNode(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	~CTacticalScanCurrentControlNode()
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("FailCurrentScan",    _HELP("Trigger to fail current scan")),
			InputPortConfig_Void("SucceedCurrentScan", _HELP("Trigger to succeed current scan")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Failed",    _HELP("Triggered when fail input is triggered")),
			OutputPortConfig_Void("Succeeded", _HELP("Triggered when succeed input is triggered")),
			{ 0 }
		};
		config.sDescription = _HELP("Can control the tactical scan that is currently in progress");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_FailCurrentScan))
				{
					SHUDEvent _event(eHUDEvent_OnControlCurrentTacticalScan);
					_event.AddData(SHUDEventData(false)); // Delay result
					_event.AddData(SHUDEventData(false));
					CHUDEventDispatcher::CallEvent(_event);

					ActivateOutput(pActInfo, EOP_Failed, true);
				}
				else if (IsPortActive(pActInfo, EIP_SucceedCurrentScan))
				{
					SHUDEvent _event(eHUDEvent_OnControlCurrentTacticalScan);
					_event.AddData(SHUDEventData(false)); // Delay result
					_event.AddData(SHUDEventData(true));
					CHUDEventDispatcher::CallEvent(_event);

					ActivateOutput(pActInfo, EOP_Succeeded, true);
				}
				break;
			}
		}
	}
};

REGISTER_FLOW_NODE("Entity:TacticalScan", CTacticalScanNode);
REGISTER_FLOW_NODE("Entity:TacticalScanStart", CTacticalScanStartNode);
REGISTER_FLOW_NODE("Entity:TacticalScanComplete", CTacticalScanCompleteNode);
REGISTER_FLOW_NODE("Entity:TacticalScanCurrentControl", CTacticalScanCurrentControlNode);
