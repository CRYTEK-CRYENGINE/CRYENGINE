// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>
#include <CryCore/Containers/VectorMap.h>

//////////////////////////////////////////////////////////////////////
class CFlowNode_SetDefaultActionEntity : public CFlowBaseNode<eNCT_Singleton>
{
	enum Inputs
	{
		EIP_SET,
		EIP_UPDATE
	};

	enum Outputs
	{
		EOP_ONSET
	};

public:
	CFlowNode_SetDefaultActionEntity(SActivationInfo* pActInfo)
	{
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_AnyType("Set", _HELP("Set assigned Entity as default action entity")),
			InputPortConfig<bool>("UpdateExisting", true, _HELP("If set to true, Set will update all existing action map assignments")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig_AnyType("OnSet", _HELP("Gets triggered when default action entity gets set/changed")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_SET))
				{
					if (pActInfo->pEntity)
					{
						const bool bUpdate = GetPortBool(pActInfo, EIP_UPDATE);
						IActionMapManager* pActionMapMgr = gEnv->pGameFramework->GetIActionMapManager();
						if (pActionMapMgr && bUpdate)
						{
							pActionMapMgr->SetDefaultActionEntity(pActInfo->pEntity->GetId(), bUpdate);
						}
						ActivateOutput(pActInfo, EOP_ONSET, 1);
					}
				}
				break;
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////
class CFlowNode_GetDefaultActionEntity : public CFlowBaseNode<eNCT_Instanced>, IActionMapEventListener
{
	enum EInputs
	{
		EIP_GET,
		EIP_AUTOUPDATE
	};

	enum EOutputs
	{
		EOP_ENTITYID
	};

public:
	CFlowNode_GetDefaultActionEntity(SActivationInfo* pActInfo)
		: m_bAutoUpdate(false), m_currentEntityId(INVALID_ENTITYID)
	{
	}

	~CFlowNode_GetDefaultActionEntity()
	{
		Register(false);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_AnyType("Get", _HELP("Get current default action entity")),
			InputPortConfig<bool>("AutoUpdate", true, _HELP("outputs the new default entity whenever it changes")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig<EntityId>("EntityId", _HELP("")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("");
		config.SetCategory(EFLN_APPROVED);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_GetDefaultActionEntity(pActInfo);
	}

	void Serialize(SActivationInfo* pActivationInfo, TSerialize ser)
	{
		if (ser.IsReading())
		{
			Register(false);
		}

		ser.Value("m_currentEntityId", m_currentEntityId, 'eid');
		ser.Value("m_bAutoUpdate", m_bAutoUpdate, 'bool');

		if (ser.IsReading())
		{
			if (m_currentEntityId != 0 && m_bAutoUpdate)
			{
				Register(true);
			}
		}
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		m_pActInfo = *pActInfo;

		switch (event)
		{
		case eFE_Activate:
			{
				const bool bAutoUpdate = GetPortBool(pActInfo, EIP_AUTOUPDATE);

				if (m_bAutoUpdate != bAutoUpdate)
				{
					Register(bAutoUpdate);
				}

				if (IsPortActive(pActInfo, EIP_GET))
				{
					IActionMapManager* pActionMapMgr = gEnv->pGameFramework->GetIActionMapManager();
					ActivateOutput(pActInfo, EOP_ENTITYID, pActionMapMgr->GetDefaultActionEntity());
				}
				break;
			}
		}
	}

	void Register(bool bEnable)
	{
		IActionMapManager* pActionMapMgr = gEnv->pGameFramework->GetIActionMapManager();

		if (pActionMapMgr)
		{
			if (bEnable)
			{
				pActionMapMgr->RegisterActionMapEventListener(this);
			}
			else
			{
				pActionMapMgr->UnregisterActionMapEventListener(this);
			}

			m_bAutoUpdate = bEnable;
		}
	}

	void OnActionMapEvent(const SActionMapEvent& event)
	{
		if (event.event == SActionMapEvent::eActionMapManagerEvent_DefaultActionEntityChanged)
		{
			ActivateOutput(&m_pActInfo, EOP_ENTITYID, (EntityId)event.wparam);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	bool            m_bAutoUpdate;
	EntityId        m_currentEntityId;
	SActivationInfo m_pActInfo;
};

//////////////////////////////////////////////////////////////////////
class CFlowNode_InputActionListener : public CFlowBaseNode<eNCT_Instanced>, public IInputEventListener, public IActionListener
{
	typedef VectorMap<uint32, bool> TAnalogInputsPressed;

	enum EInputs
	{
		EIP_ENABLE = 0,
		EIP_DISABLE,
		EIP_ACTION
	};

	enum EOutputs
	{
		EOP_ENABLED,
		EOP_DISABLED,
		EOP_PRESSED,
		EOP_RELEASED,
		EOP_HOLD
	};

public:
	CFlowNode_InputActionListener(SActivationInfo* pActInfo)
		: m_bEnabled(false)
		, m_pAction(nullptr)
		, m_pActionMap(nullptr)
		, m_filterEntityId(INVALID_ENTITYID)
	{
	}

	~CFlowNode_InputActionListener()
	{
		Unregister();
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_Void("Enable",                     _HELP("Trigger to enable")),
			InputPortConfig_Void("Disable",                    _HELP("Trigger to disable")),
			InputPortConfig<string>("actionMapActions_Action", _HELP("Action input to trigger"),_HELP("Action")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig_Void("Enabled",     _HELP("Action Listener set")),
			OutputPortConfig_Void("Disabled",    _HELP("Action Listener removed")),
			OutputPortConfig<string>("Pressed",  _HELP("Action trigger pressed.")),
			OutputPortConfig<string>("Released", _HELP("Action trigger released.")),
			OutputPortConfig<string>("Hold",     _HELP("Action trigger hold. Make sure the action map is enabled, since this port goes properly through the action manager.")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("FlowNode to catch input events via their action name. Entity Input will listen for specified entity only");
		config.SetCategory(EFLN_APPROVED);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_InputActionListener(pActInfo);
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("m_bActive", m_bEnabled);
		ser.Value("m_actionInputStr", m_actionInputStr);
		if (ser.IsReading())
		{
			Register();
		}
	}

	void Register()
	{
		IInput* pInput = gEnv->pInput;
		if (!pInput)
			return;

		if (m_bEnabled)
			return;

		pInput->AddEventListener(this);

		const size_t colonIndex = m_actionInputStr.find_first_of(":");
		const string& actionMapName = m_actionInputStr.substr(0, colonIndex);
		IActionMapManager* pActionMapMgr = gEnv->pGameFramework->GetIActionMapManager();
		if (pActionMapMgr)
		{
			pActionMapMgr->AddFlowgraphNodeActionListener(this, actionMapName.c_str());

			const string& actionName = &m_actionInputStr.c_str()[colonIndex + 1];
			m_pActionMap = pActionMapMgr->GetActionMap(actionMapName);
			if (m_pActionMap)
			{
				m_pAction = m_pActionMap->GetAction(ActionId(actionName));
			}
		}

		m_bEnabled = true;
	}

	void Unregister()
	{
		IInput* pInput = gEnv->pInput;
		if (!pInput)
			return;

		if (!m_bEnabled)
			return;

		pInput->RemoveEventListener(this);

		const string& actionMapName = m_actionInputStr.substr(0, m_actionInputStr.find_first_of(":"));
		IActionMapManager* pActionMapMgr = gEnv->pGameFramework->GetIActionMapManager();
		if (pActionMapMgr)
		{
			pActionMapMgr->RemoveFlowgraphNodeActionListener(this, actionMapName);
		}
		m_pActionMap = nullptr;
		m_pAction = nullptr;

		m_bEnabled = false;
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_pActInfo = *pActInfo;
				m_actionInputStr = GetPortString(pActInfo, EIP_ACTION);
				Unregister();
				break;
			}

		case eFE_SetEntityId:
			{
				m_filterEntityId = pActInfo && pActInfo->pEntity ? pActInfo->pEntity->GetId() : INVALID_ENTITYID;
				break;
			}

		case eFE_Activate:
			{
				m_pActInfo = *pActInfo;

				if (IsPortActive(pActInfo, EIP_ACTION))
				{
					if (m_bEnabled)
					{
						Unregister();
						m_actionInputStr = GetPortString(pActInfo, EIP_ACTION);
						Register();
					}
					else
					{
						m_actionInputStr = GetPortString(pActInfo, EIP_ACTION);
					}
				}

				if (IsPortActive(pActInfo, EIP_DISABLE))
				{
					Unregister();
				}

				if (IsPortActive(pActInfo, EIP_ENABLE))
				{
					Register();
				}

				break;
			}
		}
	}

	virtual bool OnInputEvent(const SInputEvent& event)
	{
		if (gEnv->pConsole->IsOpened() || gEnv->pGameFramework->IsGamePaused())
			return false;

		if (m_pActionMap && m_pActionMap->Enabled() && (m_filterEntityId == INVALID_ENTITYID || m_filterEntityId == m_pActionMap->GetActionListener()))
		{
			if (m_pAction)
			{
				const char* eventKeyNameCStr = event.keyName.c_str();

				const int iNumInputData = m_pAction->GetNumActionInputs();
				for (int i = 0; i < iNumInputData; ++i)
				{
					const SActionInput* pActionInput = m_pAction->GetActionInput(i);

					if (!stricmp(pActionInput->input, eventKeyNameCStr))
					{
						// Handle analog inputs
						if (pActionInput->analogCompareOp != eAACO_None)
						{
							if (pActionInput->bAnalogConditionFulfilled && !GetAnalogInputPressed(pActionInput->inputCRC))
							{
								ActivateOutput(&m_pActInfo, EOP_PRESSED, string(m_pAction->GetActionId().c_str()));
								SetAnalogInputPressed(pActionInput->inputCRC, true);
							}
							else if (!pActionInput->bAnalogConditionFulfilled && GetAnalogInputPressed(pActionInput->inputCRC))
							{
								ActivateOutput(&m_pActInfo, EOP_RELEASED, string(m_pAction->GetActionId().c_str()));
								SetAnalogInputPressed(pActionInput->inputCRC, false);
							}
							break;
						}

						const bool nullInputValue = (fabs_tpl(event.value) < 0.02f);
						if ((event.state == eIS_Pressed) || ((event.state == eIS_Changed) && !nullInputValue))
						{
							const string eventKeyName(eventKeyNameCStr);
							ActivateOutput(&m_pActInfo, EOP_PRESSED, eventKeyName);
						}
						else if ((event.state == eIS_Released) || ((event.state == eIS_Changed) && nullInputValue))
						{
							const string eventKeyName(eventKeyNameCStr);
							ActivateOutput(&m_pActInfo, EOP_RELEASED, eventKeyName);
						}

						break;

					}
				}
			}
		}

		// return false, so other listeners get notification as well
		return false;
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void OnAction(const ActionId& action, int activationMode, float value)
	{
		const string& actionInput = GetPortString(&m_pActInfo, EIP_ACTION);
		const size_t colonIndex = actionInput.find_first_of(":");
		const string& actionMapName = actionInput.substr(0, colonIndex);
		const string& actionName = actionInput.substr(colonIndex + 1, (actionInput.length() - colonIndex));

		const EntityId filterEntityId = m_pActInfo.pEntity ? m_pActInfo.pEntity->GetId() : INVALID_ENTITYID;

		if (!actionMapName.empty() && !actionName.empty())
		{
			IActionMapManager* pActionMapMgr = gEnv->pGameFramework->GetIActionMapManager();
			if (pActionMapMgr)
			{
				const IActionMap* pActionMap = pActionMapMgr->GetActionMap(actionMapName);
				if (pActionMap && (filterEntityId == INVALID_ENTITYID || filterEntityId == pActionMap->GetActionListener()))
				{
					const IActionMapAction* pAction = pActionMap->GetAction(ActionId(actionName));
					if (pAction && pAction->GetActionId() == action && (activationMode & (eAAM_Always | eAAM_OnHold)))
					{
						ActivateOutput(&m_pActInfo, EOP_HOLD, actionName);
					}
				}
			}
		}
	}

	bool GetAnalogInputPressed(uint32 actionCrc) const
	{
		TAnalogInputsPressed::const_iterator it = m_analogInputPressed.find(actionCrc);
		if (it == m_analogInputPressed.end())
		{
			return false;
		}

		return it->second;
	}

	void SetAnalogInputPressed(uint32 actionCrc, bool state)
	{
		m_analogInputPressed[actionCrc] = state;
	}

	TAnalogInputsPressed m_analogInputPressed;
	SActivationInfo      m_pActInfo;
	bool                 m_bEnabled;
	string               m_actionInputStr;
	IActionMap*          m_pActionMap;
	IActionMapAction*    m_pAction;
	EntityId             m_filterEntityId;
};

//////////////////////////////////////////////////////////////////////
class CFlowNode_ActionMapManager : public CFlowBaseNode<eNCT_Instanced>
{
	enum EInputs
	{
		EIP_ENABLE = 0,
		EIP_DISABLE,
		EIP_RESET
	};

	enum EOutputs
	{
		EOP_ENABLED = 0,
		EOP_DISABLED,
	};

public:
	CFlowNode_ActionMapManager(SActivationInfo* pActInfo)
		: m_pActInfo(pActInfo)
	{
	}

	~CFlowNode_ActionMapManager()
	{
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_Void("Enable",      _HELP("Trigger to enable action map manager")),
			InputPortConfig_Void("Disable",     _HELP("Trigger to disable action map manager")),
			InputPortConfig<bool>("ResetState", false,                                          _HELP("Reset state on disable")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig_Void("Enabled",  _HELP("Triggered when action map(s) got enabled")),
			OutputPortConfig_Void("Disabled", _HELP("Triggered when action map(s) got disabled")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("FlowNode to enable/disable action map manager");
		config.SetCategory(EFLN_APPROVED);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_ActionMapManager(pActInfo);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		m_pActInfo = pActInfo;
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_ENABLE) || IsPortActive(pActInfo, EIP_DISABLE))
			{
				const bool bReset = GetPortBool(pActInfo, EIP_RESET);
				IActionMapManager* pActionMapMgr = gEnv->pGameFramework->GetIActionMapManager();

				if (pActionMapMgr)
				{
					pActionMapMgr->Enable(IsPortActive(pActInfo, EIP_ENABLE), bReset);
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

protected:
	SActivationInfo* m_pActInfo;
};

//////////////////////////////////////////////////////////////////////
class CFlowNode_InputActionFilter : public CFlowBaseNode<eNCT_Instanced>
{
	enum EInputs
	{
		EIP_ENABLE = 0,
		EIP_DISABLE,
		EIP_FILTER
	};

	enum EOutputs
	{
		EOP_ENABLED,
		EOP_DISABLED,
	};

public:
	CFlowNode_InputActionFilter(SActivationInfo* pActInfo)
		: m_bEnabled(false), m_pActInfo(pActInfo)
	{
	}

	~CFlowNode_InputActionFilter()
	{
		EnableFilter(false, false);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_Void("Enable",                 _HELP("Trigger to enable ActionFilter")),
			InputPortConfig_Void("Disable",                _HELP("Trigger to disable ActionFilter")),
			InputPortConfig<string>("actionFilter_Filter", _HELP("Select Action Filter"),            0),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig_Void("Enabled",  _HELP("Triggered when enabled.")),
			OutputPortConfig_Void("Disabled", _HELP("Triggered when disabled.")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("FlowNode to enable/disable ActionFilters");
		config.SetCategory(EFLN_ADVANCED);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_InputActionFilter(pActInfo);
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		m_pActInfo = pActInfo;
		// disable on reading
		if (ser.IsReading())
			EnableFilter(false);
		ser.Value("m_filterName", m_filterName);
		// on saving we ask the ActionMapManager if the filter is enabled
		// so, all basically all nodes referring to the same filter will write the correct (current) value
		// on quickload, all of them or none of them will enable/disable the filter again
		// maybe use a more central location for this
		if (ser.IsWriting())
		{
			bool bWroteIt = false;
			IActionMapManager* pAMMgr = gEnv->pGameFramework->GetIActionMapManager();
			if (pAMMgr && m_filterName.empty() == false)
			{
				IActionFilter* pFilter = pAMMgr->GetActionFilter(m_filterName.c_str());
				if (pFilter)
				{
					bool bIsEnabled = pAMMgr->IsFilterEnabled(m_filterName.c_str());
					ser.Value("m_bEnabled", bIsEnabled);
					bWroteIt = true;
				}
			}
			if (!bWroteIt)
				ser.Value("m_bEnabled", m_bEnabled);
		}
		else
		{
			ser.Value("m_bEnabled", m_bEnabled);
		}
		// re-enable
		if (ser.IsReading())
			EnableFilter(m_bEnabled);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		m_pActInfo = pActInfo;

		switch (event)
		{
		case eFE_Initialize:
			{
				EnableFilter(false);
				m_filterName = GetPortString(pActInfo, EIP_FILTER);
			}
			break;

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_FILTER))
				{
					m_filterName = GetPortString(pActInfo, EIP_FILTER);
				}
				else
				{
					EnableFilter(IsPortActive(pActInfo, EIP_ENABLE));
				}
			}
			break;
		}
	}

	void EnableFilter(bool bEnable, bool bActivateOutput = true)
	{
		if (m_filterName.empty())
			return;

		m_bEnabled = bEnable;

		IActionMapManager* pAMMgr = gEnv->pGameFramework->GetIActionMapManager();
		if (pAMMgr)
		{
			pAMMgr->EnableFilter(m_filterName, bEnable);
			if (bActivateOutput && m_pActInfo && m_pActInfo->pGraph)
			{
				ActivateOutput(m_pActInfo, bEnable ? EOP_ENABLED : EOP_DISABLED, 1);
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

protected:
	bool             m_bEnabled;
	string           m_filterName; // we need to store this name, as it the d'tor we need to disable the filter, but don't have access to port names
	SActivationInfo* m_pActInfo;
};

//////////////////////////////////////////////////////////////////////
class CFlowNode_ActionMap : public CFlowBaseNode<eNCT_Instanced>
{
	enum INPUTS
	{
		EIP_ENABLE = 0,
		EIP_DISABLE,
		EIP_EXCEPT,
		EIP_ACTIONMAP
	};

	enum OUTPUTS
	{
		EOP_ENABLED = 0,
		EOP_DISABLED,
	};

public:
	CFlowNode_ActionMap(SActivationInfo* pActInfo)
		: m_pActInfo(pActInfo)
	{
	}

	~CFlowNode_ActionMap()
	{
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_Void("Enable",                  _HELP("Enable selected action map")),
			InputPortConfig_Void("Disable",                 _HELP("Disable selected action map")),
			InputPortConfig<bool>("Except",                 true,                                 _HELP("disables all other existing action maps if set to true")),
			InputPortConfig<string>("actionMaps_ActionMap", _HELP("Action Map"),                  _HELP("ActionMap")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig_Void("Enabled",  _HELP("Triggered when action map got enabled")),
			OutputPortConfig_Void("Disabled", _HELP("Triggered when action map got disabled")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("FlowNode to enable/disable actionmaps");
		config.SetCategory(EFLN_APPROVED);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_ActionMap(pActInfo);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		m_pActInfo = pActInfo;
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_ENABLE) || IsPortActive(pActInfo, EIP_DISABLE))
			{
				const bool bExcept = GetPortBool(pActInfo, EIP_EXCEPT);
				const bool bEnable = IsPortActive(pActInfo, EIP_ENABLE);
				const string actionMapName = GetPortString(pActInfo, EIP_ACTIONMAP);
				IActionMapManager* pActionMapMgr = gEnv->pGameFramework->GetIActionMapManager();

				if (bExcept)
				{
					IActionMapIteratorPtr pActionMapIter = pActionMapMgr->CreateActionMapIterator();
					while (IActionMap* pActionMap = pActionMapIter->Next())
					{
						if (!strcmp(pActionMap->GetName(), actionMapName))
						{
							pActionMap->Enable(bEnable);
						}
						else
						{
							pActionMap->Enable(false);
						}
					}
				}

				if (pActionMapMgr && !actionMapName.empty())
				{
					if (IActionMap* pActionMap = pActionMapMgr->GetActionMap(actionMapName))
					{
						pActionMap->Enable(bEnable);

						if (pActInfo->pEntity)
						{
							pActionMap->SetActionListener(pActInfo->pEntity->GetId());
						}

						if (bEnable)
						{
							ActivateOutput(pActInfo, EOP_ENABLED, 1);
						}
						else
						{
							ActivateOutput(pActInfo, EOP_DISABLED, 1);
						}
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

protected:
	SActivationInfo* m_pActInfo;
};

REGISTER_FLOW_NODE("Input:ActionMaps:SetDefaultActionEntity", CFlowNode_SetDefaultActionEntity);
REGISTER_FLOW_NODE("Input:ActionMaps:GetDefaultActionEntity", CFlowNode_GetDefaultActionEntity);
REGISTER_FLOW_NODE("Input:ActionMaps:ActionListener", CFlowNode_InputActionListener);
REGISTER_FLOW_NODE("Input:ActionMaps:ActionMapManager", CFlowNode_ActionMapManager);
REGISTER_FLOW_NODE("Input:ActionMaps:ActionFilter", CFlowNode_InputActionFilter);
REGISTER_FLOW_NODE("Input:ActionMaps:ActionMap", CFlowNode_ActionMap);
