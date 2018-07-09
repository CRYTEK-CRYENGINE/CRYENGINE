// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Module.h"

#include "FlowSystem/FlowSystem.h"
#include "ModuleManager.h"
#include "FlowModuleNodes.h"

CFlowGraphModule::CFlowGraphModule(TModuleId moduleId)
	: m_Id(moduleId)
	, m_type(IFlowGraphModule::eT_Global)
	, m_pRootGraph(nullptr)
	, m_nextInstanceId(0)
	, m_pInstanceBeingCreated(nullptr)
	, m_startNodeTypeId(InvalidFlowNodeTypeId)
	, m_returnNodeTypeId(InvalidFlowNodeTypeId)
	, m_callNodeTypeId(InvalidFlowNodeTypeId)
{ }

CFlowGraphModule::~CFlowGraphModule()
{
	Destroy();
}

void CFlowGraphModule::Destroy()
{
	LOADING_TIME_PROFILE_SECTION_ARGS(m_name)

	ClearCallNodesForInstances();
	ClearGlobalControlNodes();

	for (TEntityInstanceMap::iterator entityIt = m_runningInstances.begin(), entityItE = m_runningInstances.end(); entityIt != entityItE; ++entityIt)
	{
		for (TInstanceMap::iterator instanceIt = entityIt->second.begin(), instanceItE = entityIt->second.end(); instanceIt != instanceItE; ++instanceIt)
		{
			instanceIt->second.m_pGraph = nullptr;
		}
		entityIt->second.clear();
	}
	m_runningInstances.clear();

	m_pRootGraph = nullptr;

	UnregisterNodes();
}

/* Serialization */

bool CFlowGraphModule::PreLoadModule(const char* fileName)
{
	LOADING_TIME_PROFILE_SECTION_ARGS(fileName);

	m_fileName = fileName;

	XmlNodeRef moduleRef = gEnv->pSystem->LoadXmlFromFile(fileName);

	if (!moduleRef)
	{
		CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, "Unable to preload Flowgraph Module: %s", PathUtil::GetFileName(fileName).c_str());
		return false;
	}

	assert(!stricmp(moduleRef->getTag(), "Graph"));

	bool bIsModule = false;
	moduleRef->getAttr("isModule", bIsModule);
	assert(bIsModule);
	if (!bIsModule)
		return false;

	assert(m_pRootGraph == nullptr);

	XmlString tempName;
	if (moduleRef->getAttr("moduleName", tempName))
		m_name = tempName;

	// module ports
	XmlNodeRef modulePorts = moduleRef->findChild("ModuleInputsOutputs");
	RemoveModulePorts();
	if (modulePorts)
	{
		int nPorts = modulePorts->getChildCount();
		for (int i = 0; i < nPorts; ++i)
		{
			XmlString portName;
			int portType;
			bool isInput;

			XmlNodeRef port = modulePorts->getChild(i);
			port->getAttr("Name", portName);
			port->getAttr("Type", portType);
			port->getAttr("Input", isInput);

			IFlowGraphModule::SModulePortConfig portConfig;
			portConfig.name = portName.c_str();
			portConfig.type = (EFlowDataTypes)portType;
			portConfig.input = isInput;

			AddModulePort(portConfig);
		}
	}

	// register call/start/end nodes for this module in the flowsystem (needs to be done before actual graph load)
	RegisterNodes();

	return true;
}

bool CFlowGraphModule::LoadModuleGraph(const char* moduleName, const char* fileName)
{
	LOADING_TIME_PROFILE_SECTION_ARGS(moduleName)

	assert(m_name == moduleName);
	assert(m_fileName == fileName);

	XmlNodeRef moduleRef = gEnv->pSystem->LoadXmlFromFile(fileName);

	if (!moduleRef)
	{
		CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, "Unable to load Flowgraph Module Graph: %s", moduleName);
		return false;
	}

	assert(!stricmp(moduleRef->getTag(), "Graph"));

	bool bIsModule = false;
	moduleRef->getAttr("isModule", bIsModule);
	assert(bIsModule);
	if (!bIsModule)
		return false;
	assert(m_pRootGraph == nullptr);

	bool bResult = false;
	if (!m_pRootGraph)
	{
		IFlowSystem* pSystem = gEnv->pFlowSystem;
		assert(pSystem);

		// Create the runtime graph, to be cloned
		m_pRootGraph = pSystem->CreateFlowGraph();
		if (m_pRootGraph)
		{
			m_pRootGraph->SerializeXML(moduleRef, true);

			stack_string sTemp = "[Module] ";
			sTemp.append(moduleName);
			m_pRootGraph->SetDebugName(sTemp);

			// Root graph is for cloning, so not active!
			m_pRootGraph->UnregisterFromFlowSystem();
			m_pRootGraph->SetEnabled(false);
			m_pRootGraph->SetActive(false);

			bResult = true;
		}
	}

	return bResult;
}

bool CFlowGraphModule::SaveModuleXml(XmlNodeRef saveTo)
{
	if (!m_pRootGraph || !saveTo)
		return false;

	saveTo->setAttr("isModule", true);
	saveTo->setAttr("moduleName", m_name);

	// note: save only module specific data. the actual graph is serialized in CHyperFlowGraph::Serialize as usual

	if (m_moduleInpPorts.size() > 0 || m_moduleOutPorts.size() > 0)
	{
		XmlNodeRef inputs = saveTo->newChild("ModuleInputsOutputs");
		for (size_t i = 0; i < m_moduleInpPorts.size(); ++i)
		{
			XmlNodeRef ioChild = inputs->newChild("Port");
			ioChild->setAttr("Name", m_moduleInpPorts[i].name);
			ioChild->setAttr("Type", m_moduleInpPorts[i].type);
			ioChild->setAttr("Input", true);
		}
		for (size_t i = 0; i < m_moduleOutPorts.size(); ++i)
		{
			XmlNodeRef ioChild = inputs->newChild("Port");
			ioChild->setAttr("Name", m_moduleOutPorts[i].name);
			ioChild->setAttr("Type", m_moduleOutPorts[i].type);
			ioChild->setAttr("Input", false);
		}
	}

	return true;
}

/* (Un)Register generated nodes */

void CFlowGraphModule::RegisterNodes()
{
	IFlowGraphModuleManager* pMgr = gEnv->pFlowSystem ? gEnv->pFlowSystem->GetIModuleManager() : nullptr;
	if (pMgr)
	{
		m_startNodeTypeId = gEnv->pFlowSystem->RegisterType(pMgr->GetStartNodeName(m_name), new CFlowModuleStartNodeFactory(this));
		m_returnNodeTypeId = gEnv->pFlowSystem->RegisterType(pMgr->GetReturnNodeName(m_name), new CFlowModuleReturnNodeFactory(this));
		m_callNodeTypeId = gEnv->pFlowSystem->RegisterType(pMgr->GetCallerNodeName(m_name), new CFlowModuleCallNodeFactory(this));
	}
}

void CFlowGraphModule::UnregisterNodes()
{
	IFlowGraphModuleManager* pMgr = gEnv->pFlowSystem ? gEnv->pFlowSystem->GetIModuleManager() : nullptr;
	if (pMgr)
	{
		gEnv->pFlowSystem->UnregisterType(pMgr->GetStartNodeName(m_name));
		gEnv->pFlowSystem->UnregisterType(pMgr->GetReturnNodeName(m_name));
		gEnv->pFlowSystem->UnregisterType(pMgr->GetCallerNodeName(m_name));
	}
}

/* Module Ports */

size_t CFlowGraphModule::GetModuleInputPortCount() const
{
	return m_moduleInpPorts.size();
}

size_t CFlowGraphModule::GetModuleOutputPortCount() const
{
	return m_moduleOutPorts.size();
}

const IFlowGraphModule::SModulePortConfig* CFlowGraphModule::GetModuleInputPort(size_t index) const
{
	if (index < m_moduleInpPorts.size())
		return &m_moduleInpPorts[index];

	return nullptr;
}

const IFlowGraphModule::SModulePortConfig* CFlowGraphModule::GetModuleOutputPort(size_t index) const
{
	if (index < m_moduleOutPorts.size())
		return &m_moduleOutPorts[index];

	return nullptr;
}

bool CFlowGraphModule::AddModulePort(const IFlowGraphModule::SModulePortConfig& port)
{
	if (port.input)
	{
		m_moduleInpPorts.push_back(port);
	}
	else
	{
		m_moduleOutPorts.push_back(port);
	}
	return true;
}

void CFlowGraphModule::RemoveModulePorts()
{
	stl::free_container(m_moduleInpPorts);
	stl::free_container(m_moduleOutPorts);
}

/* Instance Handling */

inline SModuleInstance* CFlowGraphModule::GetInstance(EntityId entityId, TModuleInstanceId instanceId)
{
	TEntityInstanceMap::iterator entityIte = m_runningInstances.find(entityId);
	if (entityIte != m_runningInstances.end())
	{
		TInstanceMap::iterator instanceIte = entityIte->second.find(instanceId);
		if (instanceIte != entityIte->second.end())
		{
			return &instanceIte->second;
		}
	}

	return nullptr;
}

bool CFlowGraphModule::HasInstanceGraph(IFlowGraphPtr pGraph)
{
	for (TEntityInstanceMap::iterator entityIt = m_runningInstances.begin(), entityItE = m_runningInstances.end(); entityIt != entityItE; ++entityIt)
	{
		for (TInstanceMap::iterator instanceIt = entityIt->second.begin(), instanceItE = entityIt->second.end(); instanceIt != instanceItE; ++instanceIt)
		{
			if (instanceIt->second.m_pGraph == pGraph)
			{
				return true;
			}
		}
	}

	return false;
}

bool CFlowGraphModule::HasRunningInstances() const
{
	for (TEntityInstanceMap::const_iterator entityIt = m_runningInstances.begin(), entityItE = m_runningInstances.end(); entityIt != entityItE; ++entityIt)
	{
		for (TInstanceMap::const_iterator instanceIt = entityIt->second.begin(), instanceItE = entityIt->second.end(); instanceIt != instanceItE; ++instanceIt)
		{
			if (instanceIt->second.m_bUsed)
			{
				return true;
			}
		}
	}
	return false;
}

size_t CFlowGraphModule::GetRunningInstancesCount() const
{
	size_t count = 0;
	for (TEntityInstanceMap::const_iterator entityIt = m_runningInstances.begin(), entityItE = m_runningInstances.end(); entityIt != entityItE; ++entityIt)
	{
		for (TInstanceMap::const_iterator instanceIt = entityIt->second.begin(), instanceItE = entityIt->second.end(); instanceIt != instanceItE; ++instanceIt)
		{
			if (instanceIt->second.m_bUsed)
			{
				count += 1;
			}
		}
	}
	return count;
}

void CFlowGraphModule::RegisterStartNodeForInstanceBeingCreated(CFlowModuleStartNode* pStartNode)
{
	assert(m_pInstanceBeingCreated);
	if (m_pInstanceBeingCreated)
	{
		m_pInstanceBeingCreated->SetStartNode(pStartNode);
	}
}

void CFlowGraphModule::CreateInstance(EntityId entityId, TModuleInstanceId runningInstanceId)
{
	LOADING_TIME_PROFILE_SECTION_ARGS(m_name.c_str());

	assert(m_pRootGraph); // can not clone an instance if the root graph is missing to clone from
	if (m_pRootGraph)
	{
		// create instance
		SModuleInstance instance(this, runningInstanceId);
		instance.m_entityId = entityId;
		instance.m_bUsed = true;

		// insert in registry
		TEntityInstanceMap::iterator entityIte = m_runningInstances.find(entityId);
		if (entityIte == m_runningInstances.end())
		{
			entityIte = m_runningInstances.insert(std::make_pair(entityId, TInstanceMap())).first;
		}
		TInstanceMap::iterator it = entityIte->second.insert(std::make_pair(runningInstanceId, instance)).first;

		// Clone and instantiate the module's nodes
		m_pInstanceBeingCreated = &it->second; // expose instance, so start and return nodes grab it when cloned.
		IFlowGraphPtr pGraphClone = m_pRootGraph->Clone();
		assert(pGraphClone);

		// Enable the instance's inner nodes
		pGraphClone->SetType(IFlowGraph::eFGT_Module);
		stack_string cloneGraphName;
		cloneGraphName.Format("%s (EntityID %u, InstanceID %u)", m_pRootGraph->GetDebugName(), entityId, runningInstanceId);
		pGraphClone->SetDebugName(cloneGraphName);
		pGraphClone->SetEnabled(true);
		pGraphClone->SetActive(true);
		pGraphClone->InitializeValues();
		m_pInstanceBeingCreated->m_pGraph = pGraphClone;

		// Activate 'OnCalled' port for all call nodes
		const TCallNodesMap* pCallNodes = &m_requestedInstanceIds[entityId][runningInstanceId];
		for (TCallNodesMap::const_iterator it = pCallNodes->begin(), end = pCallNodes->end(); it != end; ++it)
		{
			it->second->OnInstanceStarted(runningInstanceId);
		}

		// Forward the instance id to the 'OnCalled' port of all global control nodes
		for (TCallNodesMap::const_iterator it = m_globalControlNodes.begin(), end = m_globalControlNodes.end(); it != end; ++it)
		{
			it->second->OnInstanceStarted(runningInstanceId);
		}

		//notify manager that an instance was created so that the event can be forwarded to listeners
		CFlowGraphModuleManager* pModuleManager = static_cast<CFlowSystem*>(gEnv->pFlowSystem)->GetModuleManager();
		pModuleManager->BroadcastModuleInstanceStarted(this, runningInstanceId);
	}
}

void CFlowGraphModule::CallDefaultInstanceForEntity(IEntity* pEntity)
{
	if (pEntity)
	{
		TModuleInstanceId runningInstanceId = MODULEINSTANCE_INVALID;
		EntityId entityId = pEntity->GetId();

		CreateInstance(entityId, runningInstanceId);
		m_pInstanceBeingCreated->GetStartNode()->OnUpdateAllInputs(nullptr);
	}
}

TModuleInstanceId CFlowGraphModule::CallModuleInstance(EntityId entityId, TModuleInstanceId instanceId, const TModuleParams& params, CFlowModuleCallNode* pCallingNode)
{
	TModuleInstanceId runningInstanceId = instanceId;
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);

	// is instance already running?
	bool bIsInstanceRunning = false;

	TEntityInstanceMap::iterator entityIte = m_runningInstances.find(entityId);
	if (entityIte != m_runningInstances.end())
	{
		TInstanceMap::iterator instanceIte = entityIte->second.find(instanceId);
		if (instanceIte != entityIte->second.end())
		{
			// yes: update inputs
			instanceIte->second.GetStartNode()->OnUpdateAllInputs(&params);
			bIsInstanceRunning = true;
		}
	}

	if (!bIsInstanceRunning)
	{
		// no: create instance and generate ID if needed
		if (!pEntity && instanceId == MODULEINSTANCE_INVALID)
		{
			runningInstanceId = m_nextInstanceId++;
			RegisterCallNodeForInstance(entityId, runningInstanceId, pCallingNode->GetId(), pCallingNode);
		}

		CreateInstance(entityId, runningInstanceId);

		// Send inputs to the instance
		m_pInstanceBeingCreated->GetStartNode()->OnUpdateAllInputs(&params);
	}

	return runningInstanceId;
}

void CFlowGraphModule::CallAllModuleInstances(const TModuleParams& params, CFlowModuleCallNode* pCallingNode)
{
	bool bHasRunningInstances = false;

	// Send inputs to all running instances
	for (TEntityInstanceMap::iterator entityIt = m_runningInstances.begin(), entityItE = m_runningInstances.end(); entityIt != entityItE; ++entityIt)
	{
		for (TInstanceMap::iterator instanceIt = entityIt->second.begin(), instanceItE = entityIt->second.end(); instanceIt != instanceItE; ++instanceIt)
		{
			if (instanceIt->second.m_bUsed)
			{
				bHasRunningInstances = true;
				instanceIt->second.GetStartNode()->OnUpdateAllInputs(&params);
			}
		}
	}

	// there were no running instances, create one and send inputs
	if (!bHasRunningInstances)
	{
		CallModuleInstance(INVALID_ENTITYID, MODULEINSTANCE_INVALID, params, pCallingNode);
	}
}

void CFlowGraphModule::UpdateInstanceInputPort(EntityId entityId, TModuleInstanceId instanceId, size_t paramIdx, const TFlowInputData& value)
{
	SModuleInstance* pInst = GetInstance(entityId, instanceId);
	if (pInst)
	{
		pInst->GetStartNode()->OnUpdateSingleInput(paramIdx, value);
	}
}

void CFlowGraphModule::UpdateAllInstancesInputPort(size_t paramIdx, const TFlowInputData& value)
{
	for (TEntityInstanceMap::iterator entityIt = m_runningInstances.begin(), entityItE = m_runningInstances.end(); entityIt != entityItE; ++entityIt)
	{
		for (TInstanceMap::iterator instanceIt = entityIt->second.begin(), instanceItE = entityIt->second.end(); instanceIt != instanceItE; ++instanceIt)
		{
			if (instanceIt->second.m_bUsed)
			{
				instanceIt->second.GetStartNode()->OnUpdateSingleInput(paramIdx, value);
			}
		}
	}
}

void CFlowGraphModule::UpdateInstanceOutputPort(EntityId entityId, TModuleInstanceId instanceId, size_t paramIdx, const TFlowInputData& value)
{
	// pass output to the call node, who will output it if in Continuous Output mode
	const TCallNodesMap* pCallNodes = &m_requestedInstanceIds[entityId][instanceId];
	for (TCallNodesMap::const_iterator it = pCallNodes->begin(), end = pCallNodes->end(); it != end; ++it)
	{
		it->second->OnInstanceOutput(paramIdx, value);
	}

	// pass output to the global control nodes, who will output it if in Continuous Output mode
	for (TCallNodesMap::const_iterator it = m_globalControlNodes.begin(), end = m_globalControlNodes.end(); it != end; ++it)
	{
		it->second->OnInstanceOutput(paramIdx, value);
	}
}

void CFlowGraphModule::OnInstanceFinished(EntityId entityId, TModuleInstanceId instanceId, bool bSuccess, const TModuleParams& params)
{
	DestroyInstance(entityId, instanceId);

	// output result for all call nodes of this instance
	const TCallNodesMap* pCallNodes = &m_requestedInstanceIds[entityId][instanceId];
	for (TCallNodesMap::const_iterator it = pCallNodes->begin(), end = pCallNodes->end(); it != end; ++it)
	{
		it->second->OnInstanceFinished(bSuccess, params);
	}

	// output result for all global control nodes
	for (TCallNodesMap::const_iterator it = m_globalControlNodes.begin(), end = m_globalControlNodes.end(); it != end; ++it)
	{
		it->second->OnInstanceFinished(bSuccess, params);
	}

	// notify manager that an instance has finished so that the event can be forwarded to listeners
	CFlowGraphModuleManager* pModuleManager = static_cast<CFlowSystem*>(gEnv->pFlowSystem)->GetModuleManager();
	pModuleManager->BroadcastModuleInstanceFinished(this, instanceId);
}

void CFlowGraphModule::CancelInstance(EntityId entityId, TModuleInstanceId instanceId)
{
	SModuleInstance* pInst = GetInstance(entityId, instanceId);
	if (pInst)
	{
		pInst->GetStartNode()->OnCancel();
	}
}

void CFlowGraphModule::CancelAllInstances()
{
	for (TEntityInstanceMap::iterator entityIt = m_runningInstances.begin(), entityItE = m_runningInstances.end(); entityIt != entityItE; ++entityIt)
	{
		for (TInstanceMap::iterator instanceIt = entityIt->second.begin(), instanceItE = entityIt->second.end(); instanceIt != instanceItE; ++instanceIt)
		{
			if (instanceIt->second.m_bUsed)
			{
				instanceIt->second.GetStartNode()->OnCancel();
			}
		}
	}
}

void CFlowGraphModule::DeactivateInstanceGraph(SModuleInstance* pInstance) const
{
	IFlowGraph* pModuleGraph = pInstance->m_pGraph;
	if (pModuleGraph)
	{
		pModuleGraph->SetEnabled(false);
		pModuleGraph->SetActive(false);

		// Deactivate all nodes
		IFlowNodeIteratorPtr pNodeIter = pModuleGraph->CreateNodeIterator();
		if (pNodeIter)
		{
			TFlowNodeId id = InvalidFlowNodeId;
			while (pNodeIter->Next(id))
			{
				pModuleGraph->SetRegularlyUpdated(id, false);
			}
		}
	}
}

bool CFlowGraphModule::DestroyInstance(EntityId entityId, TModuleInstanceId instanceId)
{
	bool bResult = false;

	// destroy and remove the instance from the running instances
	SModuleInstance* pInstance = GetInstance(entityId, instanceId);
	if (pInstance)
	{
		DeactivateInstanceGraph(pInstance);

		// mark as unused. Can't delete the graph at this point as it is still
		//	in the middle of an update. It will be deleted at end of flow system update.
		pInstance->m_bUsed = false;
		bResult = true;
	}

	return bResult;
}

void CFlowGraphModule::RemoveCompletedInstances()
{
	for (TEntityInstanceMap::iterator entityIt = m_runningInstances.begin(), entityItE = m_runningInstances.end(); entityIt != entityItE; )
	{
		for (TInstanceMap::iterator instanceIt = entityIt->second.begin(), instanceItE = entityIt->second.end(); instanceIt != instanceItE; )
		{
			if (!instanceIt->second.m_bUsed)
			{
				instanceIt = entityIt->second.erase(instanceIt);
			}
			else
			{
				++instanceIt;
			}
		}
		if (entityIt->second.empty())
		{
			entityIt = m_runningInstances.erase(entityIt);
		}
		else
		{
			++entityIt;
		}
	}
}

void CFlowGraphModule::RemoveAllInstances()
{
	for (TEntityInstanceMap::iterator entityIt = m_runningInstances.begin(), entityItE = m_runningInstances.end(); entityIt != entityItE; ++entityIt)
	{
		for (TInstanceMap::iterator instanceIt = entityIt->second.begin(), instanceItE = entityIt->second.end(); instanceIt != instanceItE; ++instanceIt)
		{
			if (instanceIt->second.m_bUsed)
			{
				DeactivateInstanceGraph(&instanceIt->second);
				instanceIt->second.m_bUsed = false;
			}
		}
	}
}

/* Keeping track of call nodes for instances */

void CFlowGraphModule::RegisterCallNodeForInstance(EntityId entityId, TModuleInstanceId instanceId, uint callNodeId, CFlowModuleCallNode* pCallNode)
{
	// see notes on Instance IDs in Module.h

	if (entityId != INVALID_ENTITYID)
	{
		// valid entity registers any instance ID including -1. there are no generated IDs to keep track of
		m_requestedInstanceIds[entityId][instanceId][callNodeId] = pCallNode;
	}
	else
	{
		// no entity. -1 is not a valid instance id to register. keep track of next instance id to be generated
		if (instanceId != MODULEINSTANCE_INVALID)
		{
			m_requestedInstanceIds[entityId][instanceId][callNodeId] = pCallNode;

			if ((instanceId + 1) > m_nextInstanceId)
			{
				m_nextInstanceId = instanceId + 1;
			}
		}
	}
}

void CFlowGraphModule::UnregisterCallNodeForInstance(EntityId entityId, TModuleInstanceId instanceId, uint callNodeId)
{
	TEntityInstancesToCallNodes::iterator entityIte = m_requestedInstanceIds.find(entityId);
	if (entityIte != m_requestedInstanceIds.end())
	{
		TInstanceCallNodesMap::iterator instanceIte = entityIte->second.find(instanceId);
		if (instanceIte != entityIte->second.end())
		{
			instanceIte->second.erase(callNodeId);
		}
	}
}

void CFlowGraphModule::ChangeRegisterCallNodeForInstance(
  EntityId oldEntityId, TModuleInstanceId oldInstanceId,
  EntityId newEntityId, TModuleInstanceId newInstanceId,
  uint callNodeId, CFlowModuleCallNode* pCallNode)
{
	if (oldEntityId == newEntityId && oldInstanceId == newInstanceId)
		return;

	UnregisterCallNodeForInstance(oldEntityId, oldInstanceId, callNodeId);

	RegisterCallNodeForInstance(newEntityId, newInstanceId, callNodeId, pCallNode);
}

void CFlowGraphModule::ClearCallNodesForInstances()
{
	m_requestedInstanceIds.clear();
	m_nextInstanceId = 0;
}

/* Keeping track of global control nodes */

void CFlowGraphModule::RegisterGlobalControlNode(uint callNodeId, CFlowModuleCallNode* pCallNode)
{
	m_globalControlNodes[callNodeId] = pCallNode;
}

void CFlowGraphModule::UnregisterGlobalControlNode(uint callNodeId)
{
	m_globalControlNodes.erase(callNodeId);
}

void CFlowGraphModule::ClearGlobalControlNodes()
{
	m_globalControlNodes.clear();
}

/* Iterator */

IModuleInstanceIteratorPtr CFlowGraphModule::CreateInstanceIterator()
{
	if (m_iteratorPool.empty())
	{
		return new CInstanceIterator(this);
	}
	else
	{
		CInstanceIterator* pIter = m_iteratorPool.back();
		m_iteratorPool.pop_back();
		new(pIter) CInstanceIterator(this);
		return pIter;
	}
}
