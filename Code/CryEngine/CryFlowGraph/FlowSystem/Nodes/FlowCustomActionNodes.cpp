// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   FlowCustomActionNodes.h
   $Id$
   Description: All nodes related to the Custom Action flow graphs

   -------------------------------------------------------------------------
   History:
   - 30:8:2011   15:24 : Created by Dean Claassen

 *********************************************************************/

#include "StdAfx.h"

#include "FlowCustomActionNodes.h"

#include "CryAction/ICustomActions.h"

//////////////////////////////////////////////////////////////////////////
// CustomAction:Start node.
// This node is the start of the Custom Action
//////////////////////////////////////////////////////////////////////////
class CFlowNode_CustomActionStart : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_CustomActionStart(SActivationInfo* pActInfo)
	{
	}
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			{ 0 }
		};
		static const SOutputPortConfig out_config[] =
		{
			OutputPortConfig<EntityId>("ObjectId", "Entity ID of the object on which the action is executing"),
			{ 0 }
		};
		config.pInputPorts = 0; // in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("This node is the start of the Custom Action");
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		ICustomAction* pCustomAction = pActInfo->pGraph->GetCustomAction();

		switch (event)
		{
		case eFE_Update:
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			if (pCustomAction)
			{
				IEntity* pTarget = pCustomAction->GetObjectEntity();
				ActivateOutput(pActInfo, 0, pTarget ? pTarget->GetId() : 0);
			}
			break;
		case eFE_Initialize:
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			break;
		case eFE_Activate:
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// CustomAction:Succeed node.
// This node is the succeed path of the Custom Action
//////////////////////////////////////////////////////////////////////////
class CFlowNode_CustomActionSucceed : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_CustomActionSucceed(SActivationInfo* pActInfo)
	{
	}
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			{ 0 }
		};
		static const SOutputPortConfig out_config[] =
		{
			OutputPortConfig<EntityId>("ObjectId", "Entity ID of the object on which the action is executing"),
			{ 0 }
		};
		config.pInputPorts = 0; // in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("This node is the succeed path of the Custom Action");
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		ICustomAction* pCustomAction = pActInfo->pGraph->GetCustomAction();

		switch (event)
		{
		case eFE_Update:
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			if (pCustomAction)
			{
				IEntity* pTarget = pCustomAction->GetObjectEntity();
				ActivateOutput(pActInfo, 0, pTarget ? pTarget->GetId() : 0);
			}
			break;
		case eFE_Initialize:
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			break;
		case eFE_Activate:
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// CustomAction:SucceedWait node.
// This node is the succeed wait path of the Custom Action
//////////////////////////////////////////////////////////////////////////
class CFlowNode_CustomActionSucceedWait : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_CustomActionSucceedWait(SActivationInfo* pActInfo)
	{
	}
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			{ 0 }
		};
		static const SOutputPortConfig out_config[] =
		{
			OutputPortConfig<EntityId>("ObjectId", "Entity ID of the object on which the action is executing"),
			{ 0 }
		};
		config.pInputPorts = 0; // in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("This node is the succeed wait path of the Custom Action");
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		ICustomAction* pCustomAction = pActInfo->pGraph->GetCustomAction();

		switch (event)
		{
		case eFE_Update:
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			if (pCustomAction)
			{
				IEntity* pTarget = pCustomAction->GetObjectEntity();
				ActivateOutput(pActInfo, 0, pTarget ? pTarget->GetId() : 0);
			}
			break;
		case eFE_Initialize:
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			break;
		case eFE_Activate:
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// CustomAction:SucceedWaitComplete node.
// This node is the succeed wait complete path of the Custom Action
//////////////////////////////////////////////////////////////////////////
class CFlowNode_CustomActionSucceedWaitComplete : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_CustomActionSucceedWaitComplete(SActivationInfo* pActInfo)
	{
	}
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			{ 0 }
		};
		static const SOutputPortConfig out_config[] =
		{
			OutputPortConfig<EntityId>("ObjectId", "Entity ID of the object on which the action is executing"),
			{ 0 }
		};
		config.pInputPorts = 0; // in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("This node is the succeed wait complete path of the Custom Action");
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		ICustomAction* pCustomAction = pActInfo->pGraph->GetCustomAction();

		switch (event)
		{
		case eFE_Update:
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			if (pCustomAction)
			{
				IEntity* pTarget = pCustomAction->GetObjectEntity();
				ActivateOutput(pActInfo, 0, pTarget ? pTarget->GetId() : 0);
			}
			break;
		case eFE_Initialize:
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			break;
		case eFE_Activate:
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// CustomAction:End node.
// This node is the end point for all the paths in the Custom Action
//////////////////////////////////////////////////////////////////////////
class CFlowNode_CustomActionEnd : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_CustomActionEnd(SActivationInfo* pActInfo)
	{
	}
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig_Void("Succeed",             _HELP("Start path ends here and succeed path begins")),
			InputPortConfig_Void("SucceedWait",         _HELP("Optional path that can be triggered after Succeed")),
			InputPortConfig_Void("SucceedWaitComplete", _HELP("Triggered when SucceedWait state is complete")),
			InputPortConfig_Void("Abort",               _HELP("Abort path begins")),
			InputPortConfig_Void("EndSuccess",          _HELP("Action ends in success")),
			InputPortConfig_Void("EndFailure",          _HELP("Action ends in failure")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = NULL;
		config.sDescription = _HELP("This node is the end point for all the paths in the Custom Action");
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event != eFE_Activate)
			return;

		ICustomAction* pCustomAction = pActInfo->pGraph->GetCustomAction();
		if (!pCustomAction) // Not inside a custom graph, must be an instance hack, get associated entity
		{
			IEntity* pEntity = pActInfo->pEntity;
			if (!pEntity)
			{
				CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, "CFlowNode_CustomActionEnd::ProcessEvent: Instance hack must have assigned entity");
				return;
			}

			ICustomActionManager* pCustomActionManager = gEnv->pGameFramework->GetICustomActionManager();
			if (pCustomActionManager)
			{
				pCustomAction = pCustomActionManager->GetActiveCustomAction(pEntity);
				if (!pCustomAction)
				{
					CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, "CFlowNode_CustomActionEnd::ProcessEvent: Can't find custom action for entity");
					return;
				}
			}
		}

		PREFAST_ASSUME(pCustomAction); // is validated above, if it fails it should already have returned

		if (IsPortActive(pActInfo, 0))
			pCustomAction->SucceedAction();
		if (IsPortActive(pActInfo, 1))
			pCustomAction->SucceedWaitAction();
		if (IsPortActive(pActInfo, 2))
			pCustomAction->SucceedWaitCompleteAction();
		else if (IsPortActive(pActInfo, 3))
			pCustomAction->AbortAction();
		else if (IsPortActive(pActInfo, 4))
			pCustomAction->EndActionSuccess();
		else if (IsPortActive(pActInfo, 5))
			pCustomAction->EndActionFailure();
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// CustomAction:Abort node.
// This node starts the abort path for the Custom Action
//////////////////////////////////////////////////////////////////////////
class CFlowNode_CustomActionAbort : public CFlowBaseNode<eNCT_Instanced>
{
protected:
	bool bAborted;

public:
	CFlowNode_CustomActionAbort(SActivationInfo* pActInfo)
	{
		bAborted = false;
	}
	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_CustomActionAbort(pActInfo);
	}
	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("Aborted", bAborted);
	}
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			{ 0 }
		};
		static const SOutputPortConfig out_config[] =
		{
			OutputPortConfig<EntityId>("ObjectId", "Entity ID of the object on which the action is executing"),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("This node starts the abort path for the Custom Action");
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		ICustomAction* pCustomAction = pActInfo->pGraph->GetCustomAction();
		switch (event)
		{
		case eFE_Update:
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			if (pCustomAction && !bAborted)
			{
				// Abort only once
				bAborted = true;

				IEntity* pTarget = pCustomAction->GetObjectEntity();
				ActivateOutput(pActInfo, 0, pTarget ? pTarget->GetId() : 0);
			}
			break;
		case eFE_Initialize:
			bAborted = false;
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			break;
		case eFE_Activate:
			if (!bAborted && pCustomAction && IsPortActive(pActInfo, 0))
			{
				pCustomAction->AbortAction();
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
// CustomAction:Control node.
// This node is used to control a custom action from a specific instance
//////////////////////////////////////////////////////////////////////////
CFlowNode_CustomActionControl::~CFlowNode_CustomActionControl()
{
	ICustomActionManager* pCustomActionManager = gEnv->pGameFramework->GetICustomActionManager();
	if (pCustomActionManager)
	{
		pCustomActionManager->UnregisterListener(this);
	}
}

void CFlowNode_CustomActionControl::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_ports[] =
	{
		InputPortConfig<string>("Start",               _HELP("Enters the start path")),
		InputPortConfig<string>("Succeed",             _HELP("Enters the succeed path")),
		InputPortConfig<string>("SucceedWait",         _HELP("Enters the succeed wait path (Optional from Succeed instead of EndSuccess)")),
		InputPortConfig<string>("SucceedWaitComplete", _HELP("Enters the succeed wait complete path (After SucceedWait which connects to EndSuccess)")),
		InputPortConfig_Void("Abort",                  _HELP("Enters the abort path")),
		InputPortConfig_Void("EndSuccess",             _HELP("Ends in success")),
		InputPortConfig_Void("EndFailure",             _HELP("Ends in failure")),
		{ 0 }
	};

	static const SOutputPortConfig out_ports[] =
	{
		OutputPortConfig_Void("Started",             _HELP("Entered the start path")),
		OutputPortConfig_Void("Succeeded",           _HELP("Entered the succeed path")),
		OutputPortConfig_Void("SucceedWait",         _HELP("Enters the succeed wait path (Optional from Succeed instead of EndSuccess)")),
		OutputPortConfig_Void("SucceedWaitComplete", _HELP("Enters the succeed wait complete path (After SucceedWait which connects to EndSuccess)")),
		OutputPortConfig_Void("Aborted",             _HELP("Entered the abort path")),
		OutputPortConfig_Void("EndedSuccess",        _HELP("Ends in success")),
		OutputPortConfig_Void("EndedFailure",        _HELP("Ends in failure")),
		{ 0 }
	};

	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_ports;
	config.pOutputPorts = out_ports;
	config.sDescription = _HELP("Node is used to control a specific custom action instance");
	config.SetCategory(EFLN_APPROVED);
}

void CFlowNode_CustomActionControl::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_Activate)
	{
		IEntity* pEntity = pActInfo->pEntity;
		if (!pEntity)
			return;

		ICustomActionManager* pCustomActionManager = gEnv->pGameFramework->GetICustomActionManager();
		CRY_ASSERT(pCustomActionManager != NULL);
		if (!pCustomActionManager)
			return;

		if (IsPortActive(pActInfo, EIP_Start))
		{
			const string& customGraphName = GetPortString(pActInfo, EIP_Start);

			pCustomActionManager->StartAction(pEntity, customGraphName.c_str(), this);

			// Output port is activated from event
		}
		else if (IsPortActive(pActInfo, EIP_Succeed))
		{
			const string& customGraphName = GetPortString(pActInfo, EIP_Succeed);

			pCustomActionManager->SucceedAction(pEntity, customGraphName.c_str(), this);

			// Output port is activated from event
		}
		else if (IsPortActive(pActInfo, EIP_SucceedWait))
		{
			pCustomActionManager->SucceedWaitAction(pEntity);

			// Output port is activated from event
		}
		else if (IsPortActive(pActInfo, EIP_SucceedWaitComplete))
		{
			pCustomActionManager->SucceedWaitCompleteAction(pEntity);
			// Output port is activated from event
		}
		else if (IsPortActive(pActInfo, EIP_Abort))
		{
			pCustomActionManager->AbortAction(pEntity);

			// Output port is activated from event
		}
		else if (IsPortActive(pActInfo, EIP_EndSuccess))
		{
			ICustomAction* pCustomAction = pCustomActionManager->GetActiveCustomAction(pEntity);
			if (!pCustomAction)
			{
				return;
			}

			pCustomAction->EndActionSuccess();

			// Output port is activated from event
		}
		else if (IsPortActive(pActInfo, EIP_EndFailure))
		{
			ICustomAction* pCustomAction = pCustomActionManager->GetActiveCustomAction(pEntity);
			if (!pCustomAction)
			{
				return;
			}

			pCustomAction->EndActionFailure();

			// Output port is activated from event
		}
	}
	else if (event == eFE_SetEntityId)
	{
		m_actInfo = *pActInfo;
	}
}

void CFlowNode_CustomActionControl::OnCustomActionEvent(ECustomActionEvent event, ICustomAction& customAction)
{
	IEntity* pEntity = customAction.GetObjectEntity();
	if (!pEntity)
		return;

	if (pEntity != m_actInfo.pEntity) // Events are received for all entities
		return;

	switch (event)
	{
	case CAE_Started:
		{
			ActivateOutput(&m_actInfo, EOP_Started, true);
		}
		break;
	case CAE_Succeeded:
		{
			ActivateOutput(&m_actInfo, EOP_Succeeded, true);
		}
		break;
	case CAE_SucceededWait:
		{
			ActivateOutput(&m_actInfo, EOP_SucceededWait, true);
		}
		break;
	case CAE_SucceededWaitComplete:
		{
			ActivateOutput(&m_actInfo, EOP_SucceededWaitComplete, true);
		}
		break;
	case CAE_Aborted:
		{
			ActivateOutput(&m_actInfo, EOP_Aborted, true);
		}
		break;
	case CAE_EndedSuccess:
		{
			ActivateOutput(&m_actInfo, EOP_EndedSuccess, true);
		}
		break;
	case CAE_EndedFailure:
		{
			ActivateOutput(&m_actInfo, EOP_EndedFailure, true);
		}
		break;
	}
}

REGISTER_FLOW_NODE("CustomAction:Start", CFlowNode_CustomActionStart)
REGISTER_FLOW_NODE("CustomAction:Succeed", CFlowNode_CustomActionSucceed)
REGISTER_FLOW_NODE("CustomAction:SucceedWait", CFlowNode_CustomActionSucceedWait)
REGISTER_FLOW_NODE("CustomAction:SucceedWaitComplete", CFlowNode_CustomActionSucceedWaitComplete)
REGISTER_FLOW_NODE("CustomAction:End", CFlowNode_CustomActionEnd)
REGISTER_FLOW_NODE("CustomAction:Abort", CFlowNode_CustomActionAbort)
REGISTER_FLOW_NODE("CustomAction:Control", CFlowNode_CustomActionControl)
