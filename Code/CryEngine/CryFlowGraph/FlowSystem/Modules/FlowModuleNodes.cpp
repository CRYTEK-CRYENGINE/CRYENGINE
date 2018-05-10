// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowModuleNodes.h"

#include "FlowSystem/FlowSystem.h"
#include "ModuleManager.h"
#include "Module.h"

//////////////////////////////////////////////////////////////////////////
//
// Start Node factory
//
//////////////////////////////////////////////////////////////////////////

CFlowModuleStartNodeFactory::CFlowModuleStartNodeFactory(CFlowGraphModule* pModule)
	: m_pModule(pModule)
{}

CFlowModuleStartNodeFactory::~CFlowModuleStartNodeFactory()
{
	Reset();
}

void CFlowModuleStartNodeFactory::Reset()
{
	stl::free_container(m_outputs);
}

void CFlowModuleStartNodeFactory::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		{ 0 }
	};
	config.pInputPorts = in_config;
	config.nFlags |= EFLN_HIDE_UI | EFLN_UNREMOVEABLE;
	config.SetCategory(EFLN_APPROVED);
	config.SetDescription(_HELP("Starts the execution flow of a module and passes in inputs from the Call node"));

	// if we are in runtime, the ports are no longer changing, so we only need to build once (when empty), no need to rebuild
	if (m_pModule && (gEnv->IsEditing() || m_outputs.empty()))
	{
		size_t customPortCount = m_pModule->GetModuleInputPortCount(); // inputs to the module should be outputs for the Start node
		size_t idx = 0;
		m_outputs.resize(5 + customPortCount); // 4 fixed ports + custom + sentinel

		m_outputs[idx].type = eFDT_EntityId;
		m_outputs[idx].name = "EntityID";
		m_outputs[idx].description = _HELP("The ID of the Entity set in the CallNode (may be invalid if not set).");
		m_outputs[idx].humanName = _HELP("Entity ID");
		++idx;

		m_outputs[idx].type = eFDT_Any;
		m_outputs[idx].name = "Start";
		m_outputs[idx].description = _HELP("Activates when the module instance starts");
		++idx;

		m_outputs[idx].type = eFDT_Any;
		m_outputs[idx].name = "Update";
		m_outputs[idx].description = _HELP("Activates when the module instance is updated");
		++idx;

		m_outputs[idx].type = eFDT_Any;
		m_outputs[idx].name = "Cancel";
		m_outputs[idx].description = _HELP("Activates when the module instance is cancelled");
		++idx;

		for (size_t i = 0; i < customPortCount; ++i)
		{
			// inputs to the module should be outputs for the Start node
			const IFlowGraphModule::SModulePortConfig* pPort = m_pModule->GetModuleInputPort(i);
			if (pPort)
			{
				m_outputs[idx + i].type = pPort->type;
				m_outputs[idx + i].name = pPort->name;
			}
		}

		m_outputs[idx + customPortCount].name = 0; //sentinel
	}

	config.pOutputPorts = &m_outputs[0];
}

IFlowNodePtr CFlowModuleStartNodeFactory::Create(IFlowNode::SActivationInfo* pActInfo)
{
	return new CFlowModuleStartNode(this, pActInfo);
}

//////////////////////////////////////////////////////////////////////////
//
// Start Node
//
//////////////////////////////////////////////////////////////////////////

CFlowModuleStartNode::CFlowModuleStartNode(CFlowModuleStartNodeFactory* pFactory, SActivationInfo* pActInfo)
	: m_pFactory(pFactory)
	, m_pModule(pFactory->m_pModule)
	, m_entityId(INVALID_ENTITYID)
	, m_instanceId(MODULEINSTANCE_INVALID)
	, m_bStarted(false)
{}

IFlowNodePtr CFlowModuleStartNode::Clone(SActivationInfo* pActInfo)
{
	CFlowModuleStartNode* pNode = new CFlowModuleStartNode(m_pFactory, pActInfo);
	pNode->m_entityId = m_pModule->GetEntityOfInstanceBeingCreated();
	pNode->m_instanceId = m_pModule->GetInstanceIdOfInstanceBeingCreated();
	m_pModule->RegisterStartNodeForInstanceBeingCreated(pNode);
	return pNode;
};

void CFlowModuleStartNode::GetConfiguration(SFlowNodeConfig& config)
{
	m_pFactory->GetConfiguration(config);
}

void CFlowModuleStartNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (eFE_Initialize == event)
	{
		m_actInfo = *pActInfo;
		m_bStarted = false;
	}
}

void CFlowModuleStartNode::OnUpdateAllInputs(TModuleParams const* params)
{
	ActivateOutput(&m_actInfo, m_bStarted ? eOP_Update : eOP_Start, true);
	ActivateOutput(&m_actInfo, eOP_EntityId, m_entityId);
	if (params)
	{
		// number of params passed in should match the number of outputs-eOP_Param1-1 for the sentinel
		assert(params->size() == m_pFactory->m_outputs.size() - eOP_Param1 - 1);
		for (size_t i = 0; i < params->size(); ++i)
		{
			ActivateOutput(&m_actInfo, eOP_Param1 + i, (*params)[i]);
		}
	}
	m_bStarted = true;
}

void CFlowModuleStartNode::OnUpdateSingleInput(size_t idx, const TFlowInputData& value)
{
	assert(idx < m_pFactory->m_outputs.size() - eOP_Param1 - 1); // -1 for the sentinel

	ActivateOutput(&m_actInfo, eOP_Update, true);
	ActivateOutput(&m_actInfo, eOP_Param1 + idx, value);
}

void CFlowModuleStartNode::OnCancel()
{
	ActivateOutput(&m_actInfo, eOP_Cancel, true);
}

//////////////////////////////////////////////////////////////////////////
//
// Return node factory
//
//////////////////////////////////////////////////////////////////////////

CFlowModuleReturnNodeFactory::CFlowModuleReturnNodeFactory(CFlowGraphModule* pModule)
	: m_pModule(pModule)
{}

CFlowModuleReturnNodeFactory::~CFlowModuleReturnNodeFactory()
{
	Reset();
}

void CFlowModuleReturnNodeFactory::Reset()
{
	stl::free_container(m_inputs);
}

void CFlowModuleReturnNodeFactory::GetConfiguration(SFlowNodeConfig& config)
{
	static const SOutputPortConfig out_config[] = {
		{ 0 }
	};
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_HIDE_UI | EFLN_UNREMOVEABLE;
	config.SetCategory(EFLN_APPROVED);
	config.SetDescription(_HELP("Ends the module instance execution and passes the result and outputs to the Call node"));

	// if we are in runtime, the ports are no longer changing, so we only need to build once (when empty), no need to rebuild
	if (m_pModule && (gEnv->IsEditing() || m_inputs.empty()))
	{
		size_t customPortCount = m_pModule->GetModuleOutputPortCount(); // outputs from the module should be inputs for the Return node
		size_t idx = 0;
		m_inputs.resize(3 + customPortCount); // 2 fixed ports + custom + sentinel

		m_inputs[idx].defaultData = TFlowInputData(0, false); // eFDT_Any
		m_inputs[idx].name = "Success";
		m_inputs[idx].description = _HELP("Activate to end this module instance (success)");
		++idx;

		m_inputs[idx].defaultData = TFlowInputData(0, false); // eFDT_Any
		m_inputs[idx].name = "Cancel";
		m_inputs[idx].description = _HELP("Activate to end this module instance (canceled)");
		++idx;

		for (size_t i = 0; i < customPortCount; ++i)
		{
			// outputs from the module should be inputs for the Return node
			const IFlowGraphModule::SModulePortConfig* pPort = m_pModule->GetModuleOutputPort(i);
			if (pPort)
			{
				m_inputs[idx + i].defaultData = TFlowInputData::CreateDefaultInitializedForTag((int)pPort->type, (pPort->type == eFDT_Any ? false : true)); //lock ports of specific type
				m_inputs[idx + i].name = pPort->name;
			}
		}

		m_inputs[idx + customPortCount].name = 0; //sentinel
	}

	config.pInputPorts = &m_inputs[0];
}

IFlowNodePtr CFlowModuleReturnNodeFactory::Create(IFlowNode::SActivationInfo* pActInfo)
{
	return new CFlowModuleReturnNode(this, pActInfo);
}

//////////////////////////////////////////////////////////////////////////
//
// Return node
//
//////////////////////////////////////////////////////////////////////////

CFlowModuleReturnNode::CFlowModuleReturnNode(CFlowModuleReturnNodeFactory* pFactory, SActivationInfo* pActInfo)
	: m_pFactory(pFactory)
	, m_pModule(pFactory->m_pModule)
	, m_entityId(INVALID_ENTITYID)
	, m_instanceId(MODULEINSTANCE_INVALID)
	, m_bFinished(false)
{}

IFlowNodePtr CFlowModuleReturnNode::Clone(SActivationInfo* pActInfo)
{
	CFlowModuleReturnNode* pNode = new CFlowModuleReturnNode(m_pFactory, pActInfo);
	pNode->m_entityId = m_pModule->GetEntityOfInstanceBeingCreated();
	pNode->m_instanceId = m_pModule->GetInstanceIdOfInstanceBeingCreated();
	return pNode;
};

void CFlowModuleReturnNode::GetConfiguration(SFlowNodeConfig& config)
{
	m_pFactory->GetConfiguration(config);
}

void CFlowModuleReturnNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (eFE_Initialize == event)
	{
		m_actInfo = *pActInfo;
	}
	else if (eFE_Activate == event)
	{
		// pass outputs, they will be forwarded to every call node of this instance and then effectively activated according to the output mode
		for (size_t i = eIP_Param1, numInputs = m_pFactory->m_inputs.size() - 1; i < numInputs; ++i)
		{
			if (IsPortActive(pActInfo, i))
			{
				m_pModule->UpdateInstanceOutputPort(m_entityId, m_instanceId, i - eIP_Param1, GetPortAny(&m_actInfo, i));
			}
		}

		if (IsPortActive(pActInfo, eIP_Success))
			OnFinished(true);
		else if (IsPortActive(pActInfo, eIP_Cancel))
			OnFinished(false);
	}
}

void CFlowModuleReturnNode::OnFinished(bool success)
{
	if (m_bFinished)
	{
		return;
	}

	TModuleParams params;
	for (size_t i = eIP_Param1, numInputs = m_pFactory->m_inputs.size() - 1; i < numInputs; ++i)
	{
		params.push_back(GetPortAny(&m_actInfo, i));
	}

	m_pModule->OnInstanceFinished(m_entityId, m_instanceId, success, params);
	m_bFinished = true; // the node can get multiple finish activations on the same tick, but it should only 'finish' once. ignore the following ones.
}

//////////////////////////////////////////////////////////////////////////
//
// Call node factory
//
//////////////////////////////////////////////////////////////////////////

CFlowModuleCallNodeFactory::CFlowModuleCallNodeFactory(CFlowGraphModule* pModule)
	: m_pModule(pModule)
{}

CFlowModuleCallNodeFactory::~CFlowModuleCallNodeFactory()
{
	Reset();
}

void CFlowModuleCallNodeFactory::Reset()
{
	stl::free_container(m_inputs);
	stl::free_container(m_outputs);
}

void CFlowModuleCallNodeFactory::GetConfiguration(SFlowNodeConfig& config)
{
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
	config.sDescription = _HELP("Node for calling instances of a FG Module");

	// if we are in runtime, the ports are no longer changing, so we only need to build once (when empty), no need to rebuild
	if (m_pModule && (gEnv->IsEditing() || m_outputs.empty() || m_inputs.empty()))
	{
		// inputs
		{
			size_t customPortCount = m_pModule->GetModuleInputPortCount();
			size_t idx = 0;
			m_inputs.resize(7 + customPortCount); // 6 fixed ports + custom + sentinel

			m_inputs[idx].defaultData = TFlowInputData(0, false); //eFDT_Any
			m_inputs[idx].name = "Call";
			m_inputs[idx].description = _HELP("Activate to call an instance of this module, if it is already running, it will update the instance with the current inputs (regardless of the input mode)");
			++idx;

			m_inputs[idx].defaultData = TFlowInputData(0, false); //eFDT_Any
			m_inputs[idx].name = "Cancel";
			m_inputs[idx].description = _HELP("Send a Cancel input to the module instance");
			++idx;

			m_inputs[idx].defaultData = TFlowInputData(false, true);
			m_inputs[idx].name = "GlobalController";
			m_inputs[idx].description = _HELP("Set to true to send inputs and listen to outputs of all the active instances of the module");
			++idx;

			m_inputs[idx].defaultData = TFlowInputData(-1, true);
			m_inputs[idx].name = "InstanceID";
			m_inputs[idx].description = _HELP("ID to identify instance of the module run by this call node. -1 will automatically generate an ID");
			++idx;

			m_inputs[idx].defaultData = TFlowInputData(false, true);
			m_inputs[idx].name = "ContinuousInput";
			m_inputs[idx].description = _HELP("If set to true, it will pass in inputs individually to the instance when they are triggered without need for a 'Call' trigger. Otherwise, all inputs are passed in to the instance only when 'Call' is triggered.");
			++idx;

			m_inputs[idx].defaultData = TFlowInputData(false, true);
			m_inputs[idx].name = "ContinuousOutput";
			m_inputs[idx].description = _HELP("If set to true, this node's output will activate immediately when the instance sends them. Otherwise this node only activates the outputs when the instance is 'Done' sucessfuly.");
			++idx;

			for (size_t i = 0; i < customPortCount; ++i)
			{
				const IFlowGraphModule::SModulePortConfig* pPort = m_pModule->GetModuleInputPort(i);
				if (pPort && pPort->input)
				{
					m_inputs[idx + i].defaultData = TFlowInputData::CreateDefaultInitializedForTag((int)pPort->type, (pPort->type == eFDT_Any ? false : true)); //lock ports of specific type
					m_inputs[idx + i].name = pPort->name;
				}
			}

			m_inputs[idx + customPortCount].name = 0; //sentinel
		}

		// outputs
		{
			size_t customPortCount = m_pModule->GetModuleOutputPortCount();
			size_t idx = 0;
			m_outputs.resize(4 + customPortCount); // 3 fixed ports + custom + sentinel

			m_outputs[idx].type = eFDT_Int;
			m_outputs[idx].name = "OnCalled";
			m_outputs[idx].description = _HELP("Activated when the module instance is created. Outputs the ID of the instance run by this call node");
			++idx;

			m_outputs[idx].type = eFDT_Any;
			m_outputs[idx].name = "Done";
			m_outputs[idx].description = _HELP("Called when the instance finishes with a successful status");
			++idx;

			m_outputs[idx].type = eFDT_Any;
			m_outputs[idx].name = "Canceled";
			m_outputs[idx].description = _HELP("Called when the module was cancelled internally");
			++idx;

			for (size_t i = 0; i < customPortCount; ++i)
			{
				const IFlowGraphModule::SModulePortConfig* pPort = m_pModule->GetModuleOutputPort(i);
				if (pPort && !pPort->input)
				{
					m_outputs[idx + i].type = pPort->type;
					m_outputs[idx + i].name = pPort->name;
				}
			}

			m_outputs[idx + customPortCount].name = 0; //sentinel
		}
	}

	config.pInputPorts = &m_inputs[0];
	config.pOutputPorts = &m_outputs[0];
}

IFlowNodePtr CFlowModuleCallNodeFactory::Create(IFlowNode::SActivationInfo* pActInfo)
{
	return new CFlowModuleCallNode(this, pActInfo);
}

//////////////////////////////////////////////////////////////////////////
//
// Call node
//
//////////////////////////////////////////////////////////////////////////

uint CFlowModuleCallNode::s_nextCallNodeId = 0;

CFlowModuleCallNode::CFlowModuleCallNode(CFlowModuleCallNodeFactory* pFactory, SActivationInfo* pActInfo)
	: m_callNodeId(++s_nextCallNodeId)
	, m_pFactory(pFactory)
	, m_pModule(pFactory->m_pModule)
	, m_moduleId(pFactory->m_pModule->GetId())
	, m_entityId(INVALID_ENTITYID)
	, m_instanceId(MODULEINSTANCE_INVALID)
	, m_bIsGlobal(false)
	, m_bEntityChanged(false)
{}

CFlowModuleCallNode::~CFlowModuleCallNode()
{
	CFlowGraphModuleManager* pModuleManager = static_cast<CFlowSystem*>(gEnv->pFlowSystem)->GetModuleManager();
	if (!pModuleManager->GetModule(m_moduleId))
	{
		// it is possible that the module that a call node is associated with was already deleted
		// if there is a circular dependency of module calls when running from the launcher. check just in case.
		// no need to unregister from the module if it was already deleted
		return;
	}

	// remove all references that could possibly be registered
	m_pModule->UnregisterCallNodeForInstance(m_entityId, m_instanceId, m_callNodeId);
	m_pModule->UnregisterGlobalControlNode(m_callNodeId);
}

IFlowNodePtr CFlowModuleCallNode::Clone(SActivationInfo* pActInfo)
{
	IFlowNode* pNode = new CFlowModuleCallNode(m_pFactory, pActInfo);
	return pNode;
};

void CFlowModuleCallNode::GetConfiguration(SFlowNodeConfig& config)
{
	m_pFactory->GetConfiguration(config);
}

void CFlowModuleCallNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (eFE_Initialize == event)
	{
		m_actInfo = *pActInfo;

		m_instanceId = static_cast<TModuleInstanceId>(GetPortInt(pActInfo, eIP_InstanceId));
		m_entityId = pActInfo->pEntity ? pActInfo->pEntity->GetId() : INVALID_ENTITYID;

		m_bIsGlobal = GetPortBool(pActInfo, eIP_IsGlobalController);
		if (m_bIsGlobal)
		{
			// global nodes ignore the entityID. This is a design decision.
			// The alternative would be to update instances only of that entity, which sounds consistent, but in practice is not a common case
			// (common case would be to accidentally set the entity and only have 1 instance updated instead of all)
			m_pModule->RegisterGlobalControlNode(m_callNodeId, this);
		}
		else
		{
			m_pModule->RegisterCallNodeForInstance(m_entityId, m_instanceId, m_callNodeId, this);
		}

		m_bEntityChanged = false;
	}
	else if (event == eFE_SetEntityId)
	{
		m_bEntityChanged = true;
	}
	else if (eFE_Activate == event)
	{
		// Abandon all hope, ye who enter here
		// do not add 'else' to the 'ifs' - all ports can be triggered in combination at the same time

		if (IsPortActive(pActInfo, eIP_InstanceId) || m_bEntityChanged)
		{
			if (!GetPortBool(pActInfo, eIP_IsGlobalController))
			{
				// change registry of this call node to the new Entity + Instance ID if it changed
				EntityId newEntityId = pActInfo->pEntity ? pActInfo->pEntity->GetId() : INVALID_ENTITYID;
				TModuleInstanceId newInstanceId = static_cast<TModuleInstanceId>(GetPortInt(pActInfo, eIP_InstanceId));
				if (newEntityId != m_entityId || newInstanceId != m_instanceId)
				{
					m_pModule->ChangeRegisterCallNodeForInstance(m_entityId, m_instanceId, newEntityId, newInstanceId, m_callNodeId, this);
					m_entityId = newEntityId;
					m_instanceId = newInstanceId;
				}
			}
		}

		if (IsPortActive(pActInfo, eIP_IsGlobalController))
		{
			bool bNewIsGlobal = GetPortBool(pActInfo, eIP_IsGlobalController);
			if (m_bIsGlobal != bNewIsGlobal)
			{
				if (bNewIsGlobal)
				{
					// node changed to global
					m_pModule->UnregisterCallNodeForInstance(m_entityId, m_instanceId, m_callNodeId);
					m_pModule->RegisterGlobalControlNode(m_callNodeId, this);
				}
				else
				{
					// node changed to not global
					m_pModule->UnregisterGlobalControlNode(m_callNodeId);
					if (m_entityId || m_instanceId != MODULEINSTANCE_INVALID)
					{
						m_pModule->RegisterCallNodeForInstance(m_entityId, m_instanceId, m_callNodeId, this);
					}
				}
				m_bIsGlobal = bNewIsGlobal;
			}
		}

		if (IsPortActive(pActInfo, eIP_Call))
		{
			// extract and load parameters
			TModuleParams params;
			for (size_t i = eIP_Param1, numInputs = m_pFactory->m_inputs.size() - 1; i < numInputs; ++i) // size-1 for the sentinel
			{
				params.push_back(GetPortAny(pActInfo, i));
			}

			if (GetPortBool(pActInfo, eIP_IsGlobalController))
			{
				m_pModule->CallAllModuleInstances(params, this);
			}
			else
			{
				// call the instance will start it or update it with new inputs
				// the instance is always called with the actual id on the port (not the saved member variable), specially in case it is '-1'
				TModuleInstanceId actualInstanceId = static_cast<TModuleInstanceId>(GetPortInt(pActInfo, eIP_InstanceId));
				if (actualInstanceId != m_instanceId)
				{
					// CallModuleInstance will change the instance this node is associated with.
					m_pModule->UnregisterCallNodeForInstance(m_entityId, m_instanceId, m_callNodeId);
				}
				m_instanceId = m_pModule->CallModuleInstance(m_entityId, actualInstanceId, params, this);
			}
		}

		if (IsPortActive(pActInfo, eIP_Cancel))
		{
			if (GetPortBool(pActInfo, eIP_IsGlobalController))
			{
				m_pModule->CancelAllInstances();
			}
			else
			{
				m_pModule->CancelInstance(m_entityId, m_instanceId);
			}
		}

		if (IsPortActive(pActInfo, eIP_ContInput) || IsPortActive(pActInfo, eIP_ContOutput))
		{
			// no need to do anything
		}

		// data inputs for the module
		// without continuous input, a trigger on the call port is required to update inputs
		if (GetPortBool(pActInfo, eIP_ContInput))
		{
			if (GetPortBool(pActInfo, eIP_IsGlobalController))
			{
				// pass active inputs to all existing instances. do not create new ones.
				for (size_t i = eIP_Param1, numInputs = m_pFactory->m_inputs.size() - 1; i < numInputs; ++i) // size-1 for the sentinel
				{
					if (IsPortActive(pActInfo, i))
					{
						m_pModule->UpdateAllInstancesInputPort(i - eIP_Param1, GetPortAny(pActInfo, i));
					}
				}
			}
			else
			{
				// pass active inputs in to existing instance. do not create a new one.
				if (m_entityId != INVALID_ENTITYID || m_instanceId != MODULEINSTANCE_INVALID)
				{
					for (size_t i = eIP_Param1, numInputs = m_pFactory->m_inputs.size() - 1; i < numInputs; ++i) // size-1 for the sentinel
					{
						if (IsPortActive(pActInfo, i))
						{
							m_pModule->UpdateInstanceInputPort(m_entityId, m_instanceId, i - eIP_Param1, GetPortAny(pActInfo, i));
						}
					}
				}
			}
		}

		m_bEntityChanged = false;
	}
}

void CFlowModuleCallNode::OnInstanceStarted(TModuleInstanceId instanceId)
{
	if (instanceId == MODULEINSTANCE_INVALID && (m_bIsGlobal || m_entityId != INVALID_ENTITYID))
	{
		ActivateOutput(&m_actInfo, eOP_OnCall, -1);
	}
	else
	{
		ActivateOutput(&m_actInfo, eOP_OnCall, instanceId);
	}
}

void CFlowModuleCallNode::OnInstanceOutput(size_t paramIdx, const TFlowInputData& value)
{
	assert(paramIdx < m_pFactory->m_outputs.size());

	if (GetPortBool(&m_actInfo, eIP_ContOutput))
	{
		ActivateOutput(&m_actInfo, eOP_Param1 + paramIdx, value);
	}
}

void CFlowModuleCallNode::OnInstanceFinished(bool bSuccess, const TModuleParams& params)
{
	assert(params.size() == m_pFactory->m_outputs.size() - eOP_Param1 - 1);

	if (bSuccess)
	{
		ActivateOutput(&m_actInfo, eOP_Success, true);

		if (!GetPortBool(&m_actInfo, eIP_ContOutput))
		{
			for (size_t i = 0, numOutputs = m_pFactory->m_outputs.size() - eOP_Param1 - 1; i < numOutputs; ++i)
			{
				ActivateOutput(&m_actInfo, eOP_Param1 + i, params[i]);
			}
		}
	}
	else
	{
		ActivateOutput(&m_actInfo, eOP_Canceled, true);
	}

	m_instanceId = MODULEINSTANCE_INVALID;
}

//////////////////////////////////////////////////////////////////////////
//
// Module ID map node
//
//////////////////////////////////////////////////////////////////////////
CFlowModuleUserIdNode::TInstanceUserIDs CFlowModuleUserIdNode::s_ids;

void CFlowModuleUserIdNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("Get",           _HELP("Get Module instance id for given user id")),
		InputPortConfig_Void("Set",           _HELP("Sets Module instance id for given user id")),
		InputPortConfig<int>("UserID",     0, _HELP("Custom user id (e.g. an entity id)")),
		InputPortConfig<string>("ModuleName", _HELP("Name of the FG Module"), _HELP("Module Name"), _UICONFIG("dt=fgmodules")),
		InputPortConfig<int>("InstanceID",-1, _HELP("Module instance id")),
		{ 0 }
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<int>("ModuleId", _HELP("Module instance id for given user id (returns -1 if not id was found)"), _HELP("InstanceID")),
		{ 0 }
	};

	config.sDescription = _HELP("Node to create and access a map of 'user IDs' to module instance IDs. 'Users' are anything that relate with instances (eg: associate an Entity ID with an Instance ID).");
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

void CFlowModuleUserIdNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Initialize:
		s_ids.clear();
		break;
	case eFE_Activate:
		if (IsPortActive(pActInfo, eIP_Get))
		{
			const string& moduleName = GetPortString(pActInfo, eIP_ModuleName);
			const int userid = GetPortInt(pActInfo, eIP_UserId);

			TInstanceUserIDs::const_iterator it = s_ids.find(moduleName);
			if (it != s_ids.end())
			{
				std::map<int, TModuleInstanceId>::const_iterator it2 = it->second.find(userid);
				if (it2 != it->second.end())
				{
					ActivateOutput(pActInfo, eOP_ModuleInstanceId, it2->second);
				}
				else
				{
					ActivateOutput(pActInfo, eOP_ModuleInstanceId, MODULEINSTANCE_INVALID);
				}
			}
			else
			{
				ActivateOutput(pActInfo, eOP_ModuleInstanceId, MODULEINSTANCE_INVALID);
			}
		}
		else if (IsPortActive(pActInfo, eIP_Set))
		{
			const string& moduleName = GetPortString(pActInfo, eIP_ModuleName);
			const int instanceId = GetPortInt(pActInfo, eIP_ModuleInstanceId);
			const int userid = GetPortInt(pActInfo, eIP_UserId);
			s_ids[moduleName][userid] = instanceId;
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
//
// Running Instances Count Listener Node
//
//////////////////////////////////////////////////////////////////////////
CFlowModuleCountListener::CFlowModuleCountListener(SActivationInfo* pActInfo)
	: m_sModuleName(nullptr)
{
	CFlowGraphModuleManager* pModuleManager = static_cast<CFlowSystem*>(gEnv->pFlowSystem)->GetModuleManager();
	pModuleManager->RegisterListener(this, "CFlowModuleCountListener");
}

CFlowModuleCountListener::~CFlowModuleCountListener()
{
	CFlowGraphModuleManager* pModuleManager = static_cast<CFlowSystem*>(gEnv->pFlowSystem)->GetModuleManager();
	pModuleManager->UnregisterListener(this);
}

IFlowNodePtr CFlowModuleCountListener::Clone(SActivationInfo* pActInfo)
{
	return new CFlowModuleCountListener(pActInfo);
}

void CFlowModuleCountListener::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig<string>("ModuleName", _HELP("Name of the FG Module"), _HELP("Module Name"), _UICONFIG("dt=fgmodules")),
		{ 0 }
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<int>("Count", _HELP("Number of running instances of this module. -1 if the module name is not found")),
		{ 0 }
	};

	config.sDescription = _HELP("Outputs in real-time the number of running instances of a given module");
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

void CFlowModuleCountListener::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Initialize:
		m_actInfo = *pActInfo;
		m_sModuleName = GetPortString(pActInfo, eIP_ModuleName);
		break;
	case eFE_Activate:
		if (IsPortActive(pActInfo, eIP_ModuleName))
		{
			m_sModuleName = GetPortString(pActInfo, eIP_ModuleName);
			CFlowGraphModuleManager* pModuleManager = static_cast<CFlowSystem*>(gEnv->pFlowSystem)->GetModuleManager();
			CFlowGraphModule* pModule = static_cast<CFlowGraphModule*>(pModuleManager->GetModule(m_sModuleName.c_str()));
			ActivateOutput(pActInfo, eOP_InstanceCount, pModule ? static_cast<int>(pModule->GetRunningInstancesCount()) : -1);
		}
		break;
	}
}

void CFlowModuleCountListener::OnModuleInstanceCreated(IFlowGraphModule* pModule, TModuleInstanceId instanceID)
{
	if (m_sModuleName.compareNoCase(pModule->GetName()) == 0)
	{
		ActivateOutput(&m_actInfo, eOP_InstanceCount, static_cast<int>(pModule->GetRunningInstancesCount()));
	}
}

void CFlowModuleCountListener::OnModuleInstanceDestroyed(IFlowGraphModule* pModule, TModuleInstanceId instanceID)
{
	if (m_sModuleName.compareNoCase(pModule->GetName()) == 0)
	{
		ActivateOutput(&m_actInfo, eOP_InstanceCount, static_cast<int>(pModule->GetRunningInstancesCount()));
	}
}

REGISTER_FLOW_NODE("Module:Utils:UserIDToModuleID", CFlowModuleUserIdNode);
REGISTER_FLOW_NODE("Module:Utils:InstanceCountListener", CFlowModuleCountListener);
