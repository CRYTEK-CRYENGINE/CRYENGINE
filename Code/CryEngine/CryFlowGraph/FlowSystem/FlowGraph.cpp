// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowGraph.h"

#include <CryString/StringUtils.h>
#include <CryAISystem/IAIAction.h>
#include <CryAction/ICustomActions.h>
#include <CryGame/IGameTokens.h>

#include "FlowSystem.h"
#include "Modules/ModuleManager.h"

// Debug disabled edges in FlowGraphs
#define FLOW_DEBUG_DISABLED_EDGES
#undef FLOW_DEBUG_DISABLED_EDGES

// Debug mapping from NodeIDs (==char* Name) to internal IDs (index in std::vector)
#define FLOW_DEBUG_ID_MAPPING
#undef FLOW_DEBUG_ID_MAPPING

// #define FLOW_DEBUG_PENDING_UPDATES is defined in header file FlowGraph.h

#define NODE_NAME_ATTR "ID"
#define NODE_TYPE_ATTR "Class"

class CFlowGraphBase::CNodeIterator : public IFlowNodeIterator
{
public:
	CNodeIterator(CFlowGraphBase* pParent) :
		m_refs(0),
		m_pParent(pParent)
	{
		m_begin = pParent->m_nodeNameToId.begin();
		m_iter = m_begin;
		m_pParent->AddRef();
	}
	~CNodeIterator()
	{
		m_pParent->Release();
	}

	void AddRef()
	{
		++m_refs;
	}

	void Release()
	{
		if (0 == --m_refs)
			delete this;
	}

	IFlowNodeData* Next(TFlowNodeId& id)
	{
		IFlowNodeData* out = NULL;
		if (m_iter != m_pParent->m_nodeNameToId.end())
		{
			id = m_iter->second;
			out = &m_pParent->m_flowData[id];
			++m_iter;
		}
		return out;
	}

private:
	int                                     m_refs;
	CFlowGraphBase*                         m_pParent;
	std::map<string, TFlowNodeId>::iterator m_begin;
	std::map<string, TFlowNodeId>::iterator m_iter;
};

class CFlowGraphBase::CEdgeIterator : public IFlowEdgeIterator
{
public:
	CEdgeIterator(CFlowGraphBase* pParent) :
		m_refs(0),
		m_pParent(pParent),
		m_iter(pParent->m_edges.begin())
	{
		m_pParent->AddRef();
	}
	~CEdgeIterator()
	{
		m_pParent->Release();
	}

	void AddRef()
	{
		++m_refs;
	}

	void Release()
	{
		if (0 == --m_refs)
			delete this;
	}

	bool Next(Edge& edge)
	{
		if (m_iter == m_pParent->m_edges.end())
			return false;
		edge.fromNodeId = m_iter->fromNode;
		edge.toNodeId = m_iter->toNode;
		edge.fromPortId = m_iter->fromPort;
		edge.toPortId = m_iter->toPort;
		++m_iter;
		return true;
	}

private:
	int                                m_refs;
	CFlowGraphBase*                    m_pParent;
	std::vector<SEdge>::const_iterator m_iter;
};

class CFlowGraphBase::SEdgeHasNode
{
public:
	ILINE SEdgeHasNode(TFlowNodeId id) : m_id(id) {}
	ILINE bool operator()(const SEdge& edge) const
	{
		return edge.fromNode == m_id || edge.toNode == m_id;
	}

private:
	TFlowNodeId m_id;
};


// CFlowGraphBase

const TFlowNodeId CFlowGraphBase::NOT_MODIFIED = ~TFlowNodeId(0);
const TFlowNodeId CFlowGraphBase::END_OF_MODIFIED_LIST = CFlowGraphBase::NOT_MODIFIED - 1;


CFlowGraphBase::CFlowGraphBase(CFlowSystem* pSys)
	: m_firstModifiedNode(END_OF_MODIFIED_LIST)
	, m_firstActivatingNode(END_OF_MODIFIED_LIST)
	, m_bInUpdate(false)
	, m_firstFinalActivatingNode(END_OF_MODIFIED_LIST)
	, m_bEnabled(false)
	, m_bActive(true)
	, m_bEdgesSorted(true)
	, m_bNeedsInitialize(true)
	, m_bNeedsUpdating(false)
	, m_pSys(pSys)
	, m_nRefs(0)
	, m_pEntitySystem(nullptr)
	, m_bSuspended(false)
	, m_bIsAIAction(false)
	, m_pAIAction(nullptr)
	, m_bIsCustomAction(false)
	, m_pCustomAction(nullptr)
	, m_pClonedFlowGraph(nullptr)
	, m_bRegistered(false)
	, m_graphId(InvalidFlowGraphId)
	, m_Type(IFlowGraph::eFGT_Default)
	, m_graphTokens()
{
	m_pEntitySystem = gEnv->pEntitySystem;

	for (int i = 0; i < MAX_GRAPH_ENTITIES; i++)
		m_graphEntityId[i] = 0;

	CreateDebugName();

	m_graphId = m_pSys->RegisterGraph(this, GetDebugName());
	m_bRegistered = true;
	m_pFlowGraphDebugger = GetIFlowGraphDebuggerPtr();
}

void CFlowGraphBase::SetEnabled(bool bEnable)
{
	if (m_bEnabled != bEnable)
	{
		m_bEnabled = bEnable;
		if (bEnable) NeedUpdate();
	}
}

void CFlowGraphBase::SetActive(bool bActive)
{
	if (m_bActive != bActive)
	{
		m_bActive = bActive;
		if (bActive) NeedUpdate();
	}
}

void CFlowGraphBase::RegisterFlowNodeActivationListener(SFlowNodeActivationListener* listener)
{
	if (!stl::find(m_flowNodeActivationListeners, listener))
		m_flowNodeActivationListeners.push_back(listener);
}

void CFlowGraphBase::RemoveFlowNodeActivationListener(SFlowNodeActivationListener* listener)
{
	stl::find_and_erase(m_flowNodeActivationListeners, listener);
}

bool CFlowGraphBase::NotifyFlowNodeActivationListeners(TFlowNodeId srcNode, TFlowPortId srcPort, TFlowNodeId toNode, TFlowPortId toPort, const char* value)
{
	if (m_pClonedFlowGraph)
	{
		// [3/24/2011 evgeny] Allow AI Action FG to be debugged as well
		m_pClonedFlowGraph->NotifyFlowNodeActivationListeners(srcNode, srcPort, toNode, toPort, value);
		return false;
	}
	else
	{
		// Modules should notify via their root graph, so that all module activations are debugged on the
		//	graph visible in the editor.
		if (GetType() == IFlowGraph::eFGT_Module)
		{
			IFlowGraphModule* pModule = gEnv->pFlowSystem->GetIModuleManager()->GetModule(this);

			if (pModule)
			{
				IFlowGraphPtr pGraph = pModule->GetRootGraph();
				return pGraph->NotifyFlowNodeActivationListeners(srcNode, srcPort, toNode, toPort, value);
			}
		}

		std::vector<SFlowNodeActivationListener*>::iterator it = m_flowNodeActivationListeners.begin();
		std::vector<SFlowNodeActivationListener*>::iterator end = m_flowNodeActivationListeners.end();
		bool stopUpdate = false;
		for (; it != end; ++it)
		{
			bool stop = (*it)->OnFlowNodeActivation(this, srcNode, srcPort, toNode, toPort, value);

			if (stop)
				stopUpdate = true;
		}

		return stopUpdate;
	}
}

void CFlowGraphBase::SetSuspended(bool suspend)
{
	if (m_bSuspended != suspend)
	{
		m_bSuspended = false;

		IFlowNode::SActivationInfo activationInfo(this, 0);
		for (std::vector<CFlowData>::iterator it = m_flowData.begin(); it != m_flowData.end(); ++it)
		{
			if (!it->IsValid())
				continue;

			activationInfo.myID = (TFlowNodeId)(it - m_flowData.begin());
			activationInfo.pEntity = GetIEntityForNode(activationInfo.myID);
			it->SetSuspended(&activationInfo, suspend);
		}

		if (suspend)
			m_bSuspended = true;
		else
			NeedUpdate();
	}
}

bool CFlowGraphBase::IsSuspended() const
{
	return m_bSuspended;
}

void CFlowGraphBase::SetAIAction(IAIAction* pAIAction)
{
	m_pAIAction = pAIAction;
	if (pAIAction)
	{
		m_bIsAIAction = true; // once an AIAction, always an AIAction
		m_Type = eFGT_AIAction;
	}
	else
	{
		// m_bIsAIAction = false?? clearing some things but not others does not look correct, but let's not ask existential questions now
		m_Type = eFGT_Default;
	}

	CreateDebugName();
}

IAIAction* CFlowGraphBase::GetAIAction() const
{
	return m_pAIAction;
}

void CFlowGraphBase::SetCustomAction(ICustomAction* pCustomAction)
{
	m_pCustomAction = pCustomAction;

	if (pCustomAction)
	{
		m_bIsCustomAction = true; // once an CustomAction, always an CustomAction
		m_pCustomAction = pCustomAction;
		CreateDebugName();
	}
	// else? same as above?
}

ICustomAction* CFlowGraphBase::GetCustomAction() const
{
	return m_pCustomAction;
}

void CFlowGraphBase::UnregisterFromFlowSystem()
{
	assert(m_bRegistered);
	if (m_pSys && m_bRegistered)
	{
		UnregisterGraphTokens();
		m_pSys->UnregisterGraph(this);
	}

	m_graphId = InvalidFlowGraphId;
	m_bRegistered = false;
}

void CFlowGraphBase::Cleanup()
{
	m_firstModifiedNode = END_OF_MODIFIED_LIST;
	m_firstActivatingNode = END_OF_MODIFIED_LIST;
	m_firstFinalActivatingNode = END_OF_MODIFIED_LIST;
	m_bEdgesSorted = true; // no edges => sorted :)
	stl::free_container(m_modifiedNodes);
	stl::free_container(m_activatingNodes);
	stl::free_container(m_finalActivatingNodes);
	stl::free_container(m_flowData);
	stl::free_container(m_deallocatedIds);
	stl::free_container(m_edges);
	m_nodeNameToId.clear();
	stl::free_container(m_regularUpdates);
	stl::free_container(m_activatingUpdates);
	stl::free_container(m_inspectors);
}

CFlowGraphBase::~CFlowGraphBase()
{
	LOADING_TIME_PROFILE_SECTION

	RemoveGraphTokens();

	if (m_pSys && m_graphId != InvalidFlowGraphId && m_bRegistered)
		m_pSys->UnregisterGraph(this);

	Cleanup();
}


const char* CFlowGraphBase::GetDebugName() const
{
#if !defined(_RELEASE)
	return m_debugName.c_str();
#else
	return "";
#endif
}

void CFlowGraphBase::SetDebugName(const char* sName)
{
#if !defined(_RELEASE)
	m_debugName = sName;
#endif
}

void CFlowGraphBase::CreateDebugName()
{
#if !defined(_RELEASE)
	char* sType;
	stack_string sExtra = "";

	switch (m_Type)
	{
	case eFGT_Default:
		{
			sType = "Files";
			IEntity *pEntity = gEnv->pEntitySystem->GetEntity(GetGraphEntity(0));
			if (pEntity)
			{
				sType = "Entity";
				sExtra.Format(" '%s'", pEntity->GetName());
			}
			break;
		}
	case eFGT_AIAction:
		{
			sType = "AI Action";
			sExtra.Format(" '%s'", m_pAIAction->GetName());
			break;
		}
	case eFGT_UIAction: sType = "UI Action"; break;
	case eFGT_Module: sType = "Module"; break;
	case eFGT_CustomAction:
		{
			sType = "Custom Action";
			sExtra.Format(" '%s'", m_pCustomAction->GetCustomActionGraphName());
			break;
		}
	case eFGT_MaterialFx: sType = "Material FX"; break;
	}

	m_debugName.Format("[%s] %s", sType, sExtra.c_str());
#endif
}

#if defined (FLOW_DEBUG_PENDING_UPDATES)

namespace {
string GetPrettyNodeName(IFlowGraph* pGraph, TFlowNodeId id)
{
	const char* typeName = pGraph->GetNodeTypeName(id);
	const char* nodeName = pGraph->GetNodeName(id);
	string human;
	human += typeName;
	human += " Node:";

	if (nodeName == 0)
	{
		IEntity* pEntity = ((CFlowGraph*)pGraph)->GetIEntityForNode(id);
		if (pEntity)
			human += pEntity->GetName();
	}
	else
		human += nodeName;
	return human;
}
};


void CFlowGraphBase::DebugPendingActivations()
{
	if (m_bIsAIAction == true)
		return;

	GameWarning("[flow] %s was destroyed with pending updates/activations. Report follows...", m_debugName.c_str());

	if (m_pSys == 0)
	{
		GameWarning("[flow] FlowSystem already shutdown.");
		return;
	}

	GameWarning("[flow] Pending nodes:");

	TFlowNodeId id = m_firstModifiedNode;
	// traverse the list of modified nodes
	while (id != END_OF_MODIFIED_LIST)
	{
		assert(ValidateNode(id) == true);
		bool bIsReg(std::find(m_regularUpdates.begin(), m_regularUpdates.end(), id) != m_regularUpdates.end());
		GameWarning("[flow] %s regular=%d", GetPrettyNodeName(this, id).c_str(), bIsReg);
		TFlowNodeId nextId = m_modifiedNodes[id];
		m_modifiedNodes[id] = NOT_MODIFIED;
		id = nextId;
	}

	id = m_firstFinalActivatingNode;

	if (id != m_firstFinalActivatingNode)
		GameWarning("[flow] Pending nodes [final activations]:");

	while (id != END_OF_MODIFIED_LIST)
	{
		assert(ValidateNode(id) == true);
		bool bIsReg(std::find(m_regularUpdates.begin(), m_regularUpdates.end(), id) != m_regularUpdates.end());
		GameWarning("[flow] %s regular=%d", GetPrettyNodeName(this, id).c_str(), bIsReg);
		TFlowNodeId nextId = m_finalActivatingNodes[id];
		m_finalActivatingNodes[id] = NOT_MODIFIED;
		id = nextId;
	}
}

#endif

void CFlowGraphBase::AddRef()
{
	m_nRefs++;
}

void CFlowGraphBase::Release()
{
	CRY_ASSERT(m_nRefs > 0);
	if (0 == --m_nRefs)
		delete this;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::RegisterHook(IFlowGraphHookPtr p)
{
	stl::push_back_unique(m_hooks, p);
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::UnregisterHook(IFlowGraphHookPtr p)
{
	stl::find_and_erase(m_hooks, p);
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::SetGraphEntity(EntityId id, int nIndex)
{
	if (nIndex >= 0 && nIndex < MAX_GRAPH_ENTITIES)
	{
		m_graphEntityId[nIndex] = id;
		CreateDebugName();
	}
}

//////////////////////////////////////////////////////////////////////////
EntityId CFlowGraphBase::GetGraphEntity(int nIndex) const
{
	if (nIndex >= 0 && nIndex < MAX_GRAPH_ENTITIES)
		return m_graphEntityId[nIndex];
	return 0;
}

//////////////////////////////////////////////////////////////////////////
IFlowGraphPtr CFlowGraphBase::Clone()
{
	CFlowGraphBase* pClone = new CFlowGraph(m_pSys);
	CloneInner(pClone);

	return pClone;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::CloneInner(CFlowGraphBase* pClone)
{
	pClone->m_activatingNodes = m_activatingNodes;
	pClone->m_activatingUpdates = m_activatingUpdates;
	pClone->m_bEdgesSorted = m_bEdgesSorted;
	pClone->m_deallocatedIds = m_deallocatedIds;
	pClone->m_edges = m_edges;
	pClone->m_firstActivatingNode = m_firstActivatingNode;
	pClone->m_firstModifiedNode = m_firstModifiedNode;
	pClone->m_firstFinalActivatingNode = m_firstFinalActivatingNode;
	pClone->m_flowData = m_flowData;
	pClone->m_modifiedNodes = m_modifiedNodes;
	pClone->m_finalActivatingNodes = m_finalActivatingNodes;
	pClone->m_nodeNameToId = m_nodeNameToId;
	pClone->m_regularUpdates = m_regularUpdates;
	pClone->m_hooks = m_hooks;
	pClone->m_bEnabled = m_bEnabled;
	pClone->m_pClonedFlowGraph = this;
	pClone->m_Type = m_Type;
	// pClone->m_bActive = m_bActive;
	// pClone->m_bSuspended = m_bSuspended;
#if !defined (_RELEASE)
	// copy the name as is. something else should overwrite it (such as the module manager or changing the graph's entity)
	pClone->m_debugName = m_debugName;
#endif

	// clone graph tokens
	const size_t numTokens = GetGraphTokenCount();
	pClone->m_graphTokens.reserve(numTokens);
	for (size_t i = 0; i < numTokens; ++i)
	{
		const IFlowGraph::SGraphToken* pToken = GetGraphToken(i);
		// the clone will copy SGraphToken and register a separate CGameToken in the GTS
		pClone->AddGraphToken(*pToken);
	}

	// clone the individual nodes
	IFlowNode::SActivationInfo info;
	info.pGraph = pClone;
	for (std::vector<CFlowData>::iterator iter = pClone->m_flowData.begin(); iter != pClone->m_flowData.end(); ++iter)
	{
		info.myID = (TFlowNodeId)(iter - pClone->m_flowData.begin());
		if (iter->IsValid())
			iter->CloneImpl(&info);
	}

	//
	/*
	   std::vector<SEdge>::const_iterator itEdges, itEdgesEnd = m_edges.end();
	   for ( itEdges = m_edges.begin(); itEdges != itEdgesEnd; ++itEdges )
	   {
	   IFlowNode::SActivationInfo info(this);
	   info.myID = itEdges->fromNode;
	   info.connectPort = itEdges->fromPort;
	   m_flowData[itEdges->fromNode].GetNode()->ProcessEvent( IFlowNode::eFE_ConnectOutputPort, &info );
	   info.myID = itEdges->toNode;
	   info.connectPort = itEdges->toPort;
	   m_flowData[itEdges->toNode].GetNode()->ProcessEvent( IFlowNode::eFE_ConnectInputPort, &info );
	   }
	 */
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::Clear()
{
	Cleanup();
}

//////////////////////////////////////////////////////////////////////////
SFlowAddress CFlowGraphBase::ResolveAddress(const char* addr, bool isOutput)
{
	SFlowAddress flowAddr;
	const char* colonPos = strchr(addr, ':');
	if (colonPos)
	{
		PREFAST_SUPPRESS_WARNING(6255)
		char* prefix = (char*)alloca(colonPos - addr + 1);
		memcpy(prefix, addr, colonPos - addr);
		prefix[colonPos - addr] = 0;
		flowAddr = ResolveAddress(prefix, colonPos + 1, isOutput);
	}
	return flowAddr;
}

SFlowAddress CFlowGraphBase::ResolveAddress(const char* node, const char* port, bool isOutput)
{
	SFlowAddress flowAddr;
	flowAddr.node = stl::find_in_map(m_nodeNameToId, node, InvalidFlowNodeId);
	if (flowAddr.node == InvalidFlowNodeId)
		return SFlowAddress();
	if (!m_flowData[flowAddr.node].ResolvePort(port, flowAddr.port, isOutput))
		return SFlowAddress();
	flowAddr.isOutput = isOutput;
	return flowAddr;
}

TFlowNodeId CFlowGraphBase::ResolveNode(const char* name)
{
	return stl::find_in_map(m_nodeNameToId, name, InvalidFlowNodeId);
}

void CFlowGraphBase::GetNodeConfiguration(TFlowNodeId id, SFlowNodeConfig& config)
{
	CRY_ASSERT(ValidateNode(id));
	m_flowData[id].GetConfiguration(config);
}

EntityId CFlowGraphBase::GetEntityId(TFlowNodeId id)
{
	CRY_ASSERT(ValidateNode(id));
	return m_flowData[id].GetEntityId();
}

void CFlowGraphBase::SetEntityId(TFlowNodeId nodeId, EntityId entityId)
{
	CRY_ASSERT(ValidateNode(nodeId));
	if (m_flowData[nodeId].SetEntityId(entityId))
		ActivateNodeInt(nodeId);
}

string CFlowGraphBase::PrettyAddress(SFlowAddress addr)
{
	CFlowData& data = m_flowData[addr.node];
	return data.GetName() + ':' + data.GetPortName(addr.port, addr.isOutput);
}

bool CFlowGraphBase::ValidateNode(TFlowNodeId id) const
{
	if (id == InvalidFlowNodeId)
		return false;
	if (id >= m_flowData.size())
		return false;
	return m_flowData[id].IsValid();
}

bool CFlowGraphBase::ValidateAddress(const SFlowAddress from)
{
	if (!ValidateNode(from.node))
		return false;
	if (!m_flowData[from.node].ValidatePort(from.port, from.isOutput))
		return false;
	return true;
}

bool CFlowGraphBase::ValidateLink(SFlowAddress& from, SFlowAddress& to)
{
	// can't link output->output, or input->input
	if (from.isOutput == to.isOutput)
	{
		const char* type = from.isOutput ? "output" : "input";
		GameWarning("[flow] Attempt to link an %s node to an %s node", type, type);
		return false;
	}
	// check order is correct, and fix it up if not
	if (!from.isOutput)
	{
		GameWarning("[flow] Attempt to link from an input node to an output node: reversing");
		std::swap(from, to);
	}
	// validate that the addresses are correct
	if (!ValidateAddress(from) || !ValidateAddress(to))
	{
		GameWarning("[flow] Trying to link an invalid node");
		return false;
	}

	return true;
}

bool CFlowGraphBase::LinkNodes(SFlowAddress from, SFlowAddress to)
{
	if (!ValidateLink(from, to))
		return false;

	// add this link to the edges collection
	m_edges.push_back(SEdge(from, to));
	// and make sure that we re-sort soon
	m_bEdgesSorted = false;

	IFlowNode::SActivationInfo info(this);
	info.myID = from.node;
	info.connectPort = from.port;
	m_flowData[from.node].GetNode()->ProcessEvent(IFlowNode::eFE_ConnectOutputPort, &info);
	info.myID = to.node;
	info.connectPort = to.port;
	m_flowData[to.node].GetNode()->ProcessEvent(IFlowNode::eFE_ConnectInputPort, &info);

	NeedInitialize();

	return true;
}

void CFlowGraphBase::UnlinkNodes(SFlowAddress from, SFlowAddress to)
{
	if (!ValidateLink(from, to))
		return;

	EnsureSortedEdges();

	SEdge findEdge(from, to);
	std::vector<SEdge>::iterator iter = std::lower_bound(m_edges.begin(), m_edges.end(), findEdge);
	if (iter != m_edges.end() && *iter == findEdge)
	{
		IFlowNode::SActivationInfo info(this);
		info.myID = from.node;
		info.connectPort = from.port;
		m_flowData[from.node].GetNode()->ProcessEvent(IFlowNode::eFE_DisconnectOutputPort, &info);
		info.myID = to.node;
		info.connectPort = to.port;
		m_flowData[to.node].GetNode()->ProcessEvent(IFlowNode::eFE_DisconnectInputPort, &info);

		m_edges.erase(iter);
		m_bEdgesSorted = false;
	}
	else
	{
		GameWarning("[flow] Link not found");
	}
}

//////////////////////////////////////////////////////////////////////////
TFlowNodeId CFlowGraphBase::CreateNode(TFlowNodeTypeId typeId, const char* name, void* pUserData)
{
	std::pair<CFlowData*, TFlowNodeId> nd = CreateNodeInt(typeId, name, pUserData);
	return nd.second;
}

//////////////////////////////////////////////////////////////////////////
TFlowNodeId CFlowGraphBase::CreateNode(const char* typeName, const char* name, void* pUserData)
{
	std::pair<CFlowData*, TFlowNodeId> nd = CreateNodeInt(m_pSys->GetTypeId(typeName), name, pUserData);
	return nd.second;
}

//////////////////////////////////////////////////////////////////////////
IFlowNodeData* CFlowGraphBase::GetNodeData(TFlowNodeId id)
{
	CRY_ASSERT(ValidateNode(id));
	if (id < m_flowData.size())
	{
		return &m_flowData[id];
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
std::pair<CFlowData*, TFlowNodeId> CFlowGraphBase::CreateNodeInt(TFlowNodeTypeId typeId, const char* name, void* pUserData)
{
	typedef std::pair<CFlowData*, TFlowNodeId> R;

	// make sure we only allocate for the name and type once...
	string sName = name;

	if (m_nodeNameToId.find(sName) != m_nodeNameToId.end())
	{
		GameWarning("[flow] Trying to create a node with the same name as an existing name: %s", sName.c_str());
		return R((CFlowData*)NULL, InvalidFlowNodeId);
	}

	// allocate a node id
	TFlowNodeId id = AllocateId();
	if (id == InvalidFlowNodeId)
	{
		GameWarning("[flow] Unable to create node id for node named %s", sName.c_str());
		return R((CFlowData*)NULL, InvalidFlowNodeId);
	}

	// create the actual node
	IFlowNode::SActivationInfo activationInfo(this, id, pUserData);
	IFlowNodePtr pNode = CreateNodeOfType(&activationInfo, typeId);
	if (!pNode)
	{
		const char* typeName = m_pSys->GetTypeName(typeId);
		GameWarning("[flow] Couldn't create node of type: %s (node was to be %s)", typeName, sName.c_str());
		DeallocateId(id);
		return R((CFlowData*)NULL, InvalidFlowNodeId);
	}

	for (size_t i = 0; i != m_hooks.size(); ++i)
	{
		if (!m_hooks[i]->CreatedNode(id, sName.c_str(), typeId, pNode))
		{
			for (size_t j = 0; j < i; j++)
				m_hooks[j]->CancelCreatedNode(id, sName.c_str(), typeId, pNode);

			DeallocateId(id);
			return R((CFlowData*)NULL, InvalidFlowNodeId);
		}
	}

	m_nodeNameToId.insert(std::make_pair(sName, id));
	m_flowData[id] = CFlowData(pNode, sName, typeId);
	m_bEdgesSorted = false; // need to regenerate link data

	return R(&m_flowData[id], id);
}

void CFlowGraphBase::RemoveNode(const char* name)
{
	std::map<string, TFlowNodeId>::iterator iter = m_nodeNameToId.find(name);
	if (iter == m_nodeNameToId.end())
	{
		GameWarning("[flow] No node named %s", name);
		return;
	}
	RemoveNode(iter->second);
}

void CFlowGraphBase::RemoveNode(TFlowNodeId id)
{
	if (!ValidateNode(id))
	{
		GameWarning("[flow] Trying to remove non-existant flow node %d", id);
		return;
	}

	// remove any referring edges
	EnsureSortedEdges();
	std::vector<SEdge>::iterator firstRemoveIter = std::remove_if(m_edges.begin(), m_edges.end(), SEdgeHasNode(id));
	for (std::vector<SEdge>::iterator iter = firstRemoveIter; iter != m_edges.end(); ++iter)
	{
		IFlowNode::SActivationInfo info(this);
		info.myID = iter->fromNode;
		info.connectPort = iter->fromPort;
		m_flowData[iter->fromNode].GetNode()->ProcessEvent(IFlowNode::eFE_DisconnectOutputPort, &info);
		info.myID = iter->toNode;
		info.connectPort = iter->toPort;
		m_flowData[iter->toNode].GetNode()->ProcessEvent(IFlowNode::eFE_DisconnectInputPort, &info);
	}
	m_edges.erase(firstRemoveIter, m_edges.end());

	DeallocateId(id);

	RemoveNodeFromActivationArray(id, m_firstModifiedNode, m_modifiedNodes);
	RemoveNodeFromActivationArray(id, m_firstActivatingNode, m_activatingNodes);
	RemoveNodeFromActivationArray(id, m_firstFinalActivatingNode, m_finalActivatingNodes);
	SetRegularlyUpdated(id, false);
	m_bEdgesSorted = false;
}

//////////////////////////////////////////////////////////////////////////
bool CFlowGraphBase::SetNodeName(TFlowNodeId id, const char* sName)
{
	if (id < m_flowData.size())
	{
		// avoid duplicate names
		for (size_t i = 0; i < m_flowData.size(); ++i)
		{
			if (m_flowData[i].GetName() == sName)
			{
				GameWarning("Can't rename flownode: name %s already in use", sName);
				return false;
			}
		}

		m_flowData[id].SetName(sName);
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
const char* CFlowGraphBase::GetNodeName(TFlowNodeId id)
{
	if (id < m_flowData.size())
	{
		return m_flowData[id].GetName().c_str();
	}
	return "";
}

//////////////////////////////////////////////////////////////////////////
TFlowNodeTypeId CFlowGraphBase::GetNodeTypeId(TFlowNodeId id)
{
	if (id < m_flowData.size())
	{
		return m_flowData[id].GetTypeId();
	}
	return InvalidFlowNodeTypeId;
}

//////////////////////////////////////////////////////////////////////////
const char* CFlowGraphBase::GetNodeTypeName(TFlowNodeId id)
{
	if (id < m_flowData.size())
	{
		TFlowNodeTypeId typeId = m_flowData[id].GetTypeId();
		return m_pSys->GetTypeInfo(typeId).name.c_str();
	}
	return "";
}

//////////////////////////////////////////////////////////////////////////
TFlowNodeId CFlowGraphBase::AllocateId()
{
	TFlowNodeId id = InvalidFlowNodeId;

	if (m_deallocatedIds.empty())
	{
		if (m_flowData.size() < InvalidFlowNodeId)
		{
			id = TFlowNodeId(m_flowData.size());
			m_flowData.resize(m_flowData.size() + 1);
			m_activatingNodes.resize(m_flowData.size(), NOT_MODIFIED);
			m_modifiedNodes.resize(m_flowData.size(), NOT_MODIFIED);
			m_finalActivatingNodes.resize(m_flowData.size(), NOT_MODIFIED);
		}
	}
	else
	{
		id = m_deallocatedIds.back();
		m_deallocatedIds.pop_back();
	}

	return id;
}

void CFlowGraphBase::DeallocateId(TFlowNodeId id)
{
	if (!ValidateNode(id))
		return;

	m_nodeNameToId.erase(m_flowData[id].GetName());

	m_flowData[id] = CFlowData();
	m_userData.erase(id);
	m_deallocatedIds.push_back(id);
}

void CFlowGraphBase::SetUserData(TFlowNodeId id, const XmlNodeRef& data)
{
	m_userData[id] = data;
}

XmlNodeRef CFlowGraphBase::GetUserData(TFlowNodeId id)
{
	return m_userData[id];
}

void CFlowGraphBase::Update()
{
	CRY_PROFILE_FUNCTION_ARG(PROFILE_ACTION, m_debugName);

	// Disabled or Deactivated or Suspended flow graphs shouldn't be updated!
	if (m_bEnabled == false || m_bActive == false || m_bSuspended || m_bNeedsUpdating == false)
		return;

	if (m_bNeedsInitialize)
		InitializeValues();

#ifndef _RELEASE
	CFlowSystem::FGProfile.graphsUpdated++;
#endif //_RELEASE

	m_bInUpdate = true;

	IFlowNode::SActivationInfo activationInfo(this, 0);

	// initially, we need to send an "Update yourself" event to anyone that
	// has asked for it
	// we explicitly check if empty to save STL the hassle of deallocating
	// memory that we'll use again quite soon ;)
	if (!m_regularUpdates.empty())
	{
		m_activatingUpdates = m_regularUpdates;
		for (RegularUpdates::const_iterator iter = m_activatingUpdates.begin();
		     iter != m_activatingUpdates.end(); ++iter)
		{
#ifndef _RELEASE
			CFlowSystem::FGProfile.nodeUpdates++;
#endif //_RELEASE

			CRY_ASSERT(ValidateNode(*iter));
			activationInfo.myID = *iter;
			activationInfo.pEntity = GetIEntityForNode(activationInfo.myID);
			m_flowData[*iter].Update(&activationInfo);
		}
	}

	DoUpdate(IFlowNode::eFE_Activate);

	m_bNeedsUpdating = false;
	if (!m_regularUpdates.empty())
		NeedUpdate();

	m_bInUpdate = false;
}

void CFlowGraphBase::InitializeValues()
{
	CRY_PROFILE_FUNCTION_ARG(PROFILE_ACTION, m_debugName);

	// flush activation list
	DoUpdate(IFlowNode::eFE_DontDoAnythingWithThisPlease);

	// reset graph tokens
	for (TGraphTokens::const_iterator it = m_graphTokens.begin(), end = m_graphTokens.end(); it != end; ++it)
	{
		ResetGraphToken(*it);
	}

	// Initially suspended flow graphs should never be initialized nor updated!
	if (m_bSuspended || m_bActive == false || m_bEnabled == false)
		return;

	for (std::vector<CFlowData>::iterator iter = m_flowData.begin(); iter != m_flowData.end(); ++iter)
	{
		if (!iter->IsValid())
			continue;

		iter->FlagInputPorts();
		ActivateNodeInt((TFlowNodeId)(iter - m_flowData.begin()));
	}

	m_bNeedsInitialize = true;

	DoUpdate(IFlowNode::eFE_Initialize);

	m_bNeedsInitialize = false;
}

void CFlowGraphBase::DoUpdate(IFlowNode::EFlowEvent event)
{
	IFlowNode::SActivationInfo activationInfo(this, 0);

	CRY_ASSERT(m_firstFinalActivatingNode == END_OF_MODIFIED_LIST);

	// we repeat updating until there have been too many iterations (in which
	// case we emit a warning) or until all possible activations have
	// completed
	int numLoops = 0;
	static const int MAX_LOOPS = 256;
	while (m_firstModifiedNode != END_OF_MODIFIED_LIST && numLoops++ < MAX_LOOPS)
	{
		// swap the two sets of modified nodes -- ensures that we are reentrant
		// wrt being able to call PerformActivation in response to being
		// activated
		m_activatingNodes.swap(m_modifiedNodes);
		CRY_ASSERT(m_firstActivatingNode == END_OF_MODIFIED_LIST);
		m_firstActivatingNode = m_firstModifiedNode;
		m_firstModifiedNode = END_OF_MODIFIED_LIST;

		// traverse the list of modified nodes
		while (m_firstActivatingNode != END_OF_MODIFIED_LIST)
		{
			activationInfo.myID = m_firstActivatingNode;
			activationInfo.pEntity = GetIEntityForNode(activationInfo.myID);
			m_flowData[m_firstActivatingNode].Activated(&activationInfo, event);
#if defined(ALLOW_MULTIPLE_PORT_ACTIVATIONS_PER_UPDATE)
			TCachedActivations::iterator iter = m_cachedActivations.find(m_firstActivatingNode);

			if (iter != m_cachedActivations.end())
			{
				TCachedPortActivations::const_iterator iterPortActivation = iter->second.begin();

				for (; iterPortActivation != iter->second.end(); ++iterPortActivation)
				{
					if (iterPortActivation->portID != InvalidFlowPortId)
					{
						m_flowData[m_firstActivatingNode].ActivateInputPort(iterPortActivation->portID, iterPortActivation->value);
						ActivateNodeInt(m_firstActivatingNode);
					}
					else
					{
						activationInfo.pEntity = GetIEntityForNode(activationInfo.myID);
						m_flowData[m_firstActivatingNode].Activated(&activationInfo, event);
					}
				}

				m_cachedActivations.erase(iter);
			}
#endif
			TFlowNodeId nextId = m_activatingNodes[m_firstActivatingNode];
			m_activatingNodes[m_firstActivatingNode] = NOT_MODIFIED;
			m_firstActivatingNode = nextId;

#ifndef _RELEASE
			CFlowSystem::FGProfile.nodeActivations++;
#endif //_RELEASE
		}
	}
	if (numLoops >= MAX_LOOPS)
	{
#if defined (FLOW_DEBUG_PENDING_UPDATES)
		CryLogAlways("[flow] CFlowGraphBase::DoUpdate: %s -> Reached maxLoops %d during event %d", m_debugName.c_str(), MAX_LOOPS, event);
#endif
		if (event == IFlowNode::eFE_Initialize)
		{
			// flush pending activations when we exceed the number of maximum loops
			// because then there are still nodes which have pending activations due to
			// the Initialize event. So they would wrongly be activated during the next update
			// round on a different event. That's why we flush
			DoUpdate(IFlowNode::eFE_DontDoAnythingWithThisPlease);
		}
	}

	while (m_firstFinalActivatingNode != END_OF_MODIFIED_LIST)
	{
		activationInfo.myID = m_firstFinalActivatingNode;
		activationInfo.pEntity = GetIEntityForNode(activationInfo.myID);
		m_flowData[m_firstFinalActivatingNode].Activated(&activationInfo, (IFlowNode::EFlowEvent)(event + 1));

		TFlowNodeId nextId = m_finalActivatingNodes[m_firstFinalActivatingNode];
		m_finalActivatingNodes[m_firstFinalActivatingNode] = NOT_MODIFIED;
		m_firstFinalActivatingNode = nextId;

#ifndef _RELEASE
		CFlowSystem::FGProfile.nodeActivations++;
#endif //_RELEASE
	}

	// AlexL 02/06/06: When there are activations in a eFE_FinalActivate/eFE_FinalInitialize update
	// these activations are stored in the list, but the FlowGraph is not scheduled for a next update round
	// the normal update loop continues until all activations are handled or MAX_LOOPS is reached
	// the final activation update only runs once. this means, that activations from the final activation round
	// are delayed to the next frame!
	if (m_firstModifiedNode != END_OF_MODIFIED_LIST)
	{
		NeedUpdate();
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::PrecacheResources()
{
	IFlowNode::SActivationInfo actInfo(this, 0);
	for (std::vector<CFlowData>::iterator it = m_flowData.begin(); it != m_flowData.end(); ++it)
	{
		if (it->IsValid())
		{
			CFlowData& flowData = *it;

			// Notify the Flowgraph to set its EntityId again so it can act on the entity now that it exists
			actInfo.myID = (TFlowNodeId)(it - m_flowData.begin());
			//actInfo.pEntity = GetIEntityForNode(actInfo.myID);
			flowData.CompleteActivationInfo(&actInfo);
			if (flowData.GetNode())
				flowData.GetNode()->ProcessEvent(IFlowNode::eFE_PrecacheResources, &actInfo);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// Graph Tokens

size_t CFlowGraphBase::GetGraphTokenCount() const
{
	return m_graphTokens.size();
}

const IFlowGraph::SGraphToken* CFlowGraphBase::GetGraphToken(size_t index) const
{
	if (index < m_graphTokens.size())
	{
		return &(m_graphTokens[index]);
	}

	return nullptr;
}

const char* CFlowGraphBase::GetGlobalNameForGraphToken(const char* tokenName) const
{
	assert(m_graphId != InvalidFlowGraphId);  // a graph may have an invalid id if it is not registered in the flowSystem.
	// An unregistered graph should not be registering game tokens. Using this ID as unique identifier will surely lead to problems later.

	// graph tokens are registered in the game token system using the format:
	//	'GraphToken.Graph<id>.<tokenName>'
	static stack_string globalName;
	globalName.Format("GraphToken.Graph%u.%s", m_graphId, tokenName);
	return globalName.c_str();
}

void CFlowGraphBase::UnregisterGraphTokens()
{
	LOADING_TIME_PROFILE_SECTION

	IGameTokenSystem* pGTS = gEnv->pGameFramework->GetIGameTokenSystem();
	IF_UNLIKELY(!pGTS) return;

	for (TGraphTokens::iterator it = m_graphTokens.begin(), end = m_graphTokens.end(); it != end; ++it)
	{
		const char* globalName = GetGlobalNameForGraphToken(it->name.c_str());

		IGameToken* pToken = pGTS->FindToken(globalName);
		assert(pToken && ((pToken->GetFlags() & EGAME_TOKEN_GRAPHVARIABLE) != 0));
		if (pToken)
		{
			pGTS->DeleteToken(pToken);
		}
		else
		{
			GameWarning("Attempted to remove nonexistent GraphToken: %s", globalName);
		}
	}
}

void CFlowGraphBase::RemoveGraphTokens()
{
	LOADING_TIME_PROFILE_SECTION
	if (m_graphTokens.empty()) return; //nothing to do

	if (m_bRegistered)
	{
		UnregisterGraphTokens();
	}

	stl::free_container(m_graphTokens);
}

bool CFlowGraphBase::AddGraphToken(const IFlowGraph::SGraphToken& token)
{
	m_graphTokens.push_back(token);

	ResetGraphToken(token);

	return true;
}

void CFlowGraphBase::ResetGraphToken(const IFlowGraph::SGraphToken& token)
{
	// if this graph is not registered in the flow system, the graph tokens should not get registered either
	if(!m_bRegistered) return;

	IGameTokenSystem* pGTS = gEnv->pGameFramework->GetIGameTokenSystem();
	IF_UNLIKELY(!pGTS) return;

	const char* globalName = GetGlobalNameForGraphToken(token.name);

	TFlowInputData temp = TFlowInputData::CreateDefaultInitializedForTag((int)token.type, true);
	IGameToken* pToken = pGTS->SetOrCreateToken(globalName, temp);
	if (pToken)
		pToken->SetFlags(pToken->GetFlags() | EGAME_TOKEN_GRAPHVARIABLE);
}


//////////////////////////////////////////////////////////////////////////

void CFlowGraphBase::RequestFinalActivation(TFlowNodeId id)
{
	CRY_ASSERT(m_bInUpdate);
	if (m_finalActivatingNodes[id] == NOT_MODIFIED)
	{
		m_finalActivatingNodes[id] = m_firstFinalActivatingNode;
		m_firstFinalActivatingNode = id;
	}
}

void CFlowGraphBase::SortEdges()
{
	std::sort(m_edges.begin(), m_edges.end());
	m_edges.erase(std::unique(m_edges.begin(), m_edges.end()), m_edges.end());

	for (std::vector<CFlowData>::iterator iter = m_flowData.begin(); iter != m_flowData.end(); ++iter)
	{
		if (!iter->IsValid())
			continue;
		for (int i = 0; i < iter->GetNumOutputs(); i++)
		{
			SEdge firstEdge((TFlowNodeId)(iter - m_flowData.begin()), i, 0, 0);
			std::vector<SEdge>::const_iterator iterEdge = std::lower_bound(m_edges.begin(), m_edges.end(), firstEdge);
			iter->SetOutputFirstEdge(i, (TFlowNodeId)(iterEdge - m_edges.begin()));
		}
	}

	m_bEdgesSorted = true;
}

void CFlowGraphBase::RemoveNodeFromActivationArray(TFlowNodeId id, TFlowNodeId& front, std::vector<TFlowNodeId>& array)
{
	if (front == id)
	{
		front = array[id];
		array[id] = NOT_MODIFIED;
	}
	else if (array[id] == NOT_MODIFIED)
	{
		// nothing to do
	}
	else
	{
		// the really tricky case... the node that was removed is midway through
		// the activation list... so we'll need to remove it the hard way
		TFlowNodeId current = front;
		TFlowNodeId previous = NOT_MODIFIED;
		while (current != id)
		{
			CRY_ASSERT(current != END_OF_MODIFIED_LIST);
			CRY_ASSERT(current != NOT_MODIFIED);
			previous = current;
			current = array[current];
		}
		CRY_ASSERT(previous != NOT_MODIFIED);
		CRY_ASSERT(current == id);
		array[previous] = array[current];
		array[current] = NOT_MODIFIED;
	}

	CRY_ASSERT(array[id] == NOT_MODIFIED);
}

void CFlowGraphBase::SetRegularlyUpdated(TFlowNodeId id, bool bUpdated)
{
	if (bUpdated)
	{
		stl::push_back_unique(m_regularUpdates, id);
		NeedUpdate();
	}
	else
		stl::find_and_erase(m_regularUpdates, id);
}

void CFlowGraphBase::ActivatePortAny(SFlowAddress addr, const TFlowInputData& value)
{
	PerformActivation(addr, value);
}

void CFlowGraphBase::ActivatePortCString(SFlowAddress addr, const char* cstr)
{
	TFlowInputData value;
	value.Set(string(cstr));
	PerformActivation(addr, value);
}

//////////////////////////////////////////////////////////////////////////
bool CFlowGraphBase::SetInputValue(TFlowNodeId node, TFlowPortId port, const TFlowInputData& value)
{
	if (!ValidateNode(node))
		return false;
	CFlowData& data = m_flowData[node];
	if (!data.ValidatePort(port, false))
		return false;
	const bool bOk = data.SetInputPort(port, value);
	if(bOk)
	{
		int entityOffset = (data.HasEntity() ? 1 : 0);
		IFlowNode::SActivationInfo info(this, node);
		info.connectPort = port - entityOffset;
		info.pInputPorts = data.GetInputPort(entityOffset);

		data.GetNode()->ProcessEvent(IFlowNode::eFE_EditorInputPortDataSet, &info);
	}
	return bOk;
}

//////////////////////////////////////////////////////////////////////////
const TFlowInputData* CFlowGraphBase::GetInputValue(TFlowNodeId node, TFlowPortId port)
{
	if (!ValidateNode(node))
		return 0;
	CFlowData& data = m_flowData[node];
	if (!data.ValidatePort(port, false))
		return 0;
	return data.GetInputPort(port);
}

bool CFlowGraphBase::GetActivationInfo(const char* nodeName, IFlowNode::SActivationInfo& actInfo)
{
	const TFlowNodeId nodeID = stl::find_in_map(m_nodeNameToId, nodeName, InvalidFlowNodeId);

	if (nodeID == InvalidFlowNodeId)
		return false;

	actInfo.pGraph = this;
	actInfo.myID = nodeID;
	actInfo.pEntity = GetIEntityForNode(nodeID);
	m_flowData[nodeID].CompleteActivationInfo(&actInfo);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CFlowGraphBase::SerializeXML(const XmlNodeRef& root, bool reading)
{
	if (reading)
	{
		bool ok = ReadXML(root);
		if (!ok)
			m_pSys->NotifyCriticalLoadingError();
		return ok;
	}
	else
		return WriteXML(root);
}

void CFlowGraphBase::FlowLoadError(const char* format, ...)
{
	if (!format)
		return;
	char buffer[MAX_WARNING_LENGTH];
	va_list args;
	va_start(args, format);
	cry_vsprintf(buffer, format, args);
	va_end(args);
	IEntity* pEnt = gEnv->pEntitySystem->GetEntity(GetGraphEntity(0));

	// fg_abortOnLoadError:
	// 2 --> abort
	// 1 [!=0 && !=2] --> dialog
	// 0 --> log only
	if (CFlowSystemCVars::Get().m_abortOnLoadError == 2 && gEnv->IsEditor() == false)
	{
		if (m_pAIAction != 0)
			CryFatalError("[flow] %s : %s", m_pAIAction->GetName(), buffer);
		else
			CryFatalError("[flow] %s : %s", pEnt ? pEnt->GetName() : "<noname>", buffer);
	}
	else
	{
		if (m_pAIAction != 0)
			CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_ERROR, "[flow] %s : %s", m_pAIAction->GetName(), buffer);
		else
			CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_ERROR, "[flow] %s : %s", pEnt ? pEnt->GetName() : "<noname>", buffer);
	}
}

bool CFlowGraphBase::ReadXML(const XmlNodeRef& root)
{
	Cleanup();

	if (!root)
		return false;

	bool ok = false;

	while (true)
	{
		int i;

		XmlNodeRef nodes = root->findChild("Nodes");
		if (!nodes)
		{
			FlowLoadError("No [Nodes] XML child");
			break;
		}

#ifdef FLOW_DEBUG_ID_MAPPING
		{
			IEntity* pEnt = gEnv->pEntitySystem->GetEntity(GetGraphEntity(0));
			if (m_pAIAction != 0)
				CryLogAlways("[flow] %s 0x%p: Loading FG", m_pAIAction->GetName(), this);
			else
				CryLogAlways("[flow] %s 0x%p: Loading FG", pEnt ? pEnt->GetName() : "<noname>", this);
		}
#endif

		int nNodes = nodes->getChildCount();

		int nConcreteNodes = 0;

		for (i = 0; i < nNodes; i++)
		{
			XmlNodeRef node = nodes->getChild(i);
			const char* type = node->getAttr(NODE_TYPE_ATTR);
			const char* name = node->getAttr(NODE_NAME_ATTR);
			if (0 == strcmp(type, "_comment"))
				continue;
			if (0 == strcmp(type, "_commentbox"))
				continue;
			if (0 == strcmp(type, "_blackbox"))
				continue;
			++nConcreteNodes;
		}

		m_flowData.reserve(nConcreteNodes);
		m_activatingNodes.reserve(nConcreteNodes);
		m_modifiedNodes.reserve(nConcreteNodes);
		m_finalActivatingNodes.reserve(nConcreteNodes);

		IFlowNode::SActivationInfo actInfo(this, 0);
		for (i = 0; i < nNodes; i++)
		{
			XmlNodeRef node = nodes->getChild(i);
			const char* type = node->getAttr(NODE_TYPE_ATTR);
			const char* name = node->getAttr(NODE_NAME_ATTR);
			if (0 == strcmp(type, "_comment"))
				continue;
			if (0 == strcmp(type, "_commentbox"))
				continue;
			if (0 == strcmp(type, "_blackbox"))
				continue;
			TFlowNodeTypeId typeId = m_pSys->GetTypeId(type);
			std::pair<CFlowData*, TFlowNodeId> info = CreateNodeInt(typeId, name);
			if (!info.first || info.second == InvalidFlowNodeId)
			{
				FlowLoadError("Failed to create node '%s' of type '%s' - FLOWGRAPH DISCARDED", name, type);
				break;
			}
			actInfo.myID = info.second;

#ifdef FLOW_DEBUG_ID_MAPPING
			{
				IEntity* pEnt = gEnv->pEntitySystem->GetEntity(GetGraphEntity(0));
				if (m_pAIAction != 0)
					CryLogAlways("[flow] %s : Mapping ID=%s to internal ID=%d", m_pAIAction->GetName(), name, actInfo.myID);
				else
					CryLogAlways("[flow] %s : Mapping ID=%s to internal ID=%d", pEnt ? pEnt->GetName() : "<noname>", name, actInfo.myID);
			}
#endif

			if (!info.first->SerializeXML(&actInfo, node, true))
			{
				FlowLoadError("Failed to load node %s of type %s - FLOWGRAPH DISCARDED", name, type);
				DeallocateId(info.second);
				break;
			}
		}
		if (i != nNodes) // didn't load all of the nodes
		{
			FlowLoadError("Did not load all nodes (%d/%d nodes) - FLOWGRAPH DISCARDED", i, nNodes);
			break;
		}

		XmlNodeRef edges = root->findChild("Edges");
		if (!edges)
		{
			FlowLoadError("No [Edges] XML child");
			break;
		}
		int nEdges = edges->getChildCount();

		int nConcreteEdges = 0;

		for (i = 0; i < nEdges; i++)
		{
			XmlNodeRef edge = edges->getChild(i);
			if (strcmp(edge->getTag(), "Edge"))
				break;

			SFlowAddress from = ResolveAddress(edge->getAttr("nodeOut"), edge->getAttr("portOut"), true);
			SFlowAddress to = ResolveAddress(edge->getAttr("nodeIn"), edge->getAttr("portIn"), false);

			bool edgeEnabled;
			if (((edge->getAttr("enabled", edgeEnabled) == false) || edgeEnabled) && ValidateLink(from, to))
				++nConcreteEdges;
		}

		m_edges.reserve(nConcreteEdges);

		for (i = 0; i < nEdges; i++)
		{
			XmlNodeRef edge = edges->getChild(i);
			if (strcmp(edge->getTag(), "Edge"))
				break;
			SFlowAddress from = ResolveAddress(edge->getAttr("nodeOut"), edge->getAttr("portOut"), true);
			SFlowAddress to = ResolveAddress(edge->getAttr("nodeIn"), edge->getAttr("portIn"), false);
			bool edgeEnabled;
			if (edge->getAttr("enabled", edgeEnabled) == false)
				edgeEnabled = true;

			if (edgeEnabled)
			{
				if (!LinkNodes(from, to))
				{
					FlowLoadError("Can't link edge <%s,%s> to <%s,%s> - FLOWGRAPH DISCARDED",
					              edge->getAttr("nodeOut"), edge->getAttr("portOut"),
					              edge->getAttr("nodeIn"), edge->getAttr("portIn"));
					break;
				}
			}
			else
			{
#if defined (FLOW_DEBUG_DISABLED_EDGES)
				IEntity* pEnt = gEnv->pEntitySystem->GetEntity(GetGraphEntity(0));
				if (m_pAIAction != 0)
				{
					CryLogAlways("[flow] Disabled edge %d for AI action graph '%s' <%s,%s> to <%s,%s>", i, m_pAIAction->GetName(),
					             edge->getAttr("nodeOut"), edge->getAttr("portOut"),
					             edge->getAttr("nodeIn"), edge->getAttr("portIn"));
				}
				else
				{
					GameWarning("[flow] Disabled edge %d for entity %d '%s' <%s,%s> to <%s,%s>", i, GetGraphEntity(0),
					            pEnt ? pEnt->GetName() : "<NULL>",
					            edge->getAttr("nodeOut"), edge->getAttr("portOut"),
					            edge->getAttr("nodeIn"), edge->getAttr("portIn"));
				}
#endif
			}
		}
		if (i != nEdges) // didn't load all edges
		{
			FlowLoadError("Did not load all edges (%d/%d edges) - FLOWGRAPH DISCARDED", i, nEdges);
			break;
		}

		// Load Graph Tokens

		XmlNodeRef graphTokens = root->findChild("GraphTokens");
		if (graphTokens)
		{
			int nTokens = graphTokens->getChildCount();
			RemoveGraphTokens();
			m_graphTokens.reserve(nTokens);
			for (int j = 0; j < nTokens; ++j)
			{
				XmlString tokenName;
				int tokenType;

				XmlNodeRef tokenXml = graphTokens->getChild(j);
				tokenXml->getAttr("Name", tokenName);
				tokenXml->getAttr("Type", tokenType);

				IFlowGraph::SGraphToken token;
				token.name = tokenName.c_str();
				token.type = (EFlowDataTypes)tokenType;

				AddGraphToken(token);
			}
		}

		ok = true;
		break;
	}

	if (root->getAttr("enabled", m_bEnabled) == false)
		m_bEnabled = true;

	if (!ok)
		Cleanup();
	else
		NeedInitialize();

	PrecacheResources();

	return ok;
}

bool CFlowGraphBase::WriteXML(const XmlNodeRef& root)
{
	XmlNodeRef nodes = root->createNode("Nodes");
	for (std::vector<CFlowData>::iterator iter = m_flowData.begin(); iter != m_flowData.end(); ++iter)
	{
		if (iter->IsValid())
		{
			XmlNodeRef node = nodes->createNode(m_pSys->GetTypeName(iter->GetNodeTypeId()));
			nodes->addChild(node);

			node->setAttr(NODE_NAME_ATTR, iter->GetName().c_str());
			IFlowNode::SActivationInfo info(this, (TFlowNodeId)(iter - m_flowData.begin()));
			if (!iter->SerializeXML(&info, node, false))
				return false;
		}
	}

	XmlNodeRef edges = root->createNode("Edges");
	for (std::vector<SEdge>::const_iterator iter = m_edges.begin(); iter != m_edges.end(); ++iter)
	{
		string from = PrettyAddress(SFlowAddress(iter->fromNode, iter->fromPort, true));
		string to = PrettyAddress(SFlowAddress(iter->toNode, iter->toPort, false));

		XmlNodeRef edge = edges->createNode("Edge");
		edges->addChild(edge);
		edge->setAttr("from", from.c_str());
		edge->setAttr("to", to.c_str());
	}

	XmlNodeRef tokens = root->createNode("GraphTokens");
	for (int i = 0; i < m_graphTokens.size(); ++i)
	{
		XmlNodeRef tokenXml = tokens->createNode("Token");
		tokens->addChild(tokenXml);
		tokenXml->setAttr("Name", m_graphTokens[i].name);
		tokenXml->setAttr("Type", m_graphTokens[i].type);
	}

	root->addChild(nodes);
	root->addChild(edges);
	root->addChild(tokens);
	root->setAttr("enabled", m_bEnabled);
	return true;
}

IFlowNodePtr CFlowGraphBase::CreateNodeOfType(IFlowNode::SActivationInfo* pActInfo, TFlowNodeTypeId typeId)
{
	IFlowNodePtr pPtr;
	for (size_t i = 0; i < m_hooks.size() && !pPtr; ++i)
		pPtr = m_hooks[i]->CreateNode(pActInfo, typeId);
	if (!pPtr)
		pPtr = m_pSys->CreateNodeOfType(pActInfo, typeId);
	return pPtr;
}

IFlowNodeIteratorPtr CFlowGraphBase::CreateNodeIterator()
{
	return new CNodeIterator(this);
}

IFlowEdgeIteratorPtr CFlowGraphBase::CreateEdgeIterator()
{
	return new CEdgeIterator(this);
}

bool CFlowGraphBase::IsOutputConnected(SFlowAddress addr)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	CRY_ASSERT(ValidateAddress(addr));

	bool connected = false;
	if (addr.isOutput)
	{
		EnsureSortedEdges();

		/*
		   SEdge firstEdge(addr.node, addr.port, 0, 0);
		   std::vector<SEdge>::const_iterator iter = std::lower_bound( m_edges.begin(), m_edges.end(), firstEdge );
		 */
		std::vector<SEdge>::const_iterator iter = m_edges.begin() + m_flowData[addr.node].GetOutputFirstEdge(addr.port);
		connected = iter != m_edges.end() && iter->fromNode == addr.node && iter->fromPort == addr.port;
	}

	return connected;
}

void CFlowGraphBase::Serialize(TSerialize ser)
{
	// When reading, we clear the regular updates before nodes are serialized
	// because their serialization could request to be scheduled as regularUpdate
	if (ser.IsReading())
	{
		// clear regulars
		m_regularUpdates.resize(0);
	}

	ser.Value("needsInitialize", m_bNeedsInitialize);
	ser.Value("enabled", m_bEnabled);
	ser.Value("suspended", m_bSuspended);
	ser.Value("active", m_bActive);

	std::vector<string> activatedNodes;
	if (ser.IsWriting())
	{
		// get activations
		for (TFlowNodeId id = m_firstModifiedNode; id != END_OF_MODIFIED_LIST; id = m_modifiedNodes[id])
			activatedNodes.push_back(m_flowData[id].GetName());
	}
	ser.Value("activatedNodes", activatedNodes);
	if (ser.IsReading())
	{
		// flush modified nodes
		m_firstModifiedNode = END_OF_MODIFIED_LIST;
		m_modifiedNodes.resize(0);
		m_modifiedNodes.resize(m_flowData.size(), NOT_MODIFIED);
		// reactivate
		for (std::vector<string>::const_iterator iter = activatedNodes.begin(); iter != activatedNodes.end(); ++iter)
		{
			TFlowNodeId id = stl::find_in_map(m_nodeNameToId, *iter, InvalidFlowNodeId);
			if (id != InvalidFlowNodeId)
				ActivateNodeInt(id);
			else
				GameWarning("[flow] Flow graph has changed between save-games");
		}
	}

	IFlowNode::SActivationInfo activationInfo(this, 0);
	if (ser.IsWriting())
	{
		uint32 nodeCount = 0;
		for (std::vector<CFlowData>::iterator iter = m_flowData.begin(), end = m_flowData.end(); iter != end; ++iter)
		{
			if (iter->IsValid())
			{
				activationInfo.myID = (TFlowNodeId)(iter - m_flowData.begin());
				activationInfo.pEntity = GetIEntityForNode(activationInfo.myID);
				ser.BeginGroup("Node");
				ser.Value("id", iter->GetName());
				iter->Serialize(&activationInfo, ser);
				ser.EndGroup();

				++nodeCount;
			}
		}
		ser.Value("nodeCount", nodeCount);
	}
	else
	{
		uint32 nodeCount;
		ser.Value("nodeCount", nodeCount);
		if (nodeCount != m_flowData.size())
		{
			// there are nodes in the MAP which are not in the SaveGame
			// or there are nodes in the SaveGame which are not in the map
			// can happen if level.pak got changed after savegame creation!
			// this comment is just to remind, that this case exists.
			// GameSerialize.cpp will send an eFE_Initialize after level load, but before serialize,
			// so these nodes get at least initialized
			// AlexL: 18/04/2007
		}

		while (nodeCount--)
		{
			ser.BeginGroup("Node");
			string name;
			ser.Value("id", name);
			TFlowNodeId id = stl::find_in_map(m_nodeNameToId, name, InvalidFlowNodeId);
			/*
			   if (id == InvalidFlowNodeId) {
			   std::map<string, TFlowNodeId>::const_iterator iter = m_nodeNameToId.begin();
			   CryLogAlways("Map Contents:");
			   while (iter != m_nodeNameToId.end())
			   {
			    CryLogAlways("Map: %s", iter->first.c_str());
			   ++iter;
			   }
			   }
			 */
			if (id != InvalidFlowNodeId)
			{
				activationInfo.myID = id;
				activationInfo.pEntity = GetIEntityForNode(activationInfo.myID);
				m_flowData[id].Serialize(&activationInfo, ser);
			}
			else
				GameWarning("[flow] Flowgraph '%s' has changed between save-games. Can't resolve node named '%s'", GetDebugName(), name.c_str());
			ser.EndGroup();
		}
	}

	// regular updates
	std::vector<CryFixedStringT<32>> regularUpdates;
	regularUpdates.reserve(m_regularUpdates.size());
	if (ser.IsWriting())
	{
		RegularUpdates::const_iterator iter(m_regularUpdates.begin());
		RegularUpdates::const_iterator end(m_regularUpdates.end());
		while (iter != end)
		{
			regularUpdates.push_back(m_flowData[*iter].GetName().c_str());
			++iter;
		}
	}

	ser.Value("regularUpdates", regularUpdates);
	if (ser.IsReading())
	{
		// reserve some space in the m_regularUpdates vector
		// as there might have already been added some this is somewhat conservative
		// regularUpdates.size()+m_regularUpdates.size() would be maximum, but most of the time
		// too much
		m_regularUpdates.reserve(regularUpdates.size());

		// re-fill regular updates
		for (std::vector<CryFixedStringT<32>>::const_iterator iter = regularUpdates.begin(); iter != regularUpdates.end(); ++iter)
		{
			TFlowNodeId id = stl::find_in_map(m_nodeNameToId, CONST_TEMP_STRING(iter->c_str()), InvalidFlowNodeId);
			if (id != InvalidFlowNodeId)
				SetRegularlyUpdated(id, true);
			else
				GameWarning("[flow] Flow graph has changed between save-games");
		}
	}

	// when we load a flowgraph and it didn't have activations so far
	// it is still in the initialized state
	// so we need to mark this graph to be updated, so it gets initialized
	if (ser.IsReading() && m_bNeedsInitialize)
	{
		NeedUpdate();
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::PostSerialize()
{
	IFlowNode::SActivationInfo activationInfo(this, 0);
	for (std::vector<CFlowData>::iterator iter = m_flowData.begin(), end = m_flowData.end(); iter != end; ++iter)
	{
		if (iter->IsValid())
		{
			activationInfo.myID = (TFlowNodeId)(iter - m_flowData.begin());
			activationInfo.pEntity = GetIEntityForNode(activationInfo.myID);
			iter->PostSerialize(&activationInfo);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IEntity* CFlowGraphBase::GetIEntityForNode(TFlowNodeId flowNodeId)
{
	CRY_ASSERT(ValidateNode(flowNodeId));
	EntityId id = m_flowData[flowNodeId].GetEntityId();
	if (id == 0)
		return 0;

	if (id == (EntityId)EFLOWNODE_ENTITY_ID_GRAPH1)
		id = m_graphEntityId[0];
	else if (id == (EntityId)EFLOWNODE_ENTITY_ID_GRAPH2)
		id = m_graphEntityId[1];

	IEntity* pEntity = m_pEntitySystem->GetEntity(id);
	return pEntity;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::NotifyFlowSystemDestroyed()
{
	m_pSys = NULL;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::RegisterInspector(IFlowGraphInspectorPtr pInspector)
{
	stl::push_back_unique(m_inspectors, pInspector);
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::UnregisterInspector(IFlowGraphInspectorPtr pInspector)
{
	stl::find_and_erase(m_inspectors, pInspector);
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::GetGraphStats(int& nodeCount, int& edgeCount)
{
	nodeCount = m_flowData.size();
	edgeCount = m_edges.size();
	size_t cool = m_nodeNameToId.size();
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::OnEntityReused(IEntity* pEntity, SEntitySpawnParams& params)
{
	for (std::vector<CFlowData>::iterator it = m_flowData.begin(); it != m_flowData.end(); ++it)
	{
		if (it->IsValid())
		{
			CFlowData& flowData = *it;

			if (params.id == flowData.GetEntityId() && flowData.GetCurrentForwardingEntity() == 0) // if is forwarding to an spawned entity, we dont want to do all this
			{
				// Notify the Flowgraph to set its EntityId again so it can act on the entity now that it exists
				IFlowNode::SActivationInfo actInfo(this, 0);
				actInfo.myID = (TFlowNodeId)(it - m_flowData.begin());
				actInfo.pEntity = GetIEntityForNode(actInfo.myID);
				flowData.CompleteActivationInfo(&actInfo);
				flowData.GetNode()->ProcessEvent(IFlowNode::eFE_SetEntityId, &actInfo);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::OnEntityIdChanged(EntityId oldId, EntityId newId)
{
	for (std::vector<CFlowData>::iterator it = m_flowData.begin(); it != m_flowData.end(); ++it)
	{
		if (it->IsValid())
		{
			CFlowData& flowData = *it;

			if (oldId == flowData.GetEntityId()) // if this node was holding the old EntityID, update it
			{
				IFlowNode::SActivationInfo actInfo(this, 0);
				actInfo.myID = (TFlowNodeId)(it - m_flowData.begin());
				m_flowData[actInfo.myID].SetEntityId(newId);
				actInfo.pEntity = GetIEntityForNode(actInfo.myID);
				flowData.CompleteActivationInfo(&actInfo);
				flowData.GetNode()->ProcessEvent(IFlowNode::eFE_SetEntityId, &actInfo);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// executes any pending forwarding.
// this needs to be done explicitly, because otherwise some nodes may stay in "toforward" state but never actually be forwarded (this happens for example if they dont receive any input)
void CFlowGraphBase::UpdateForwardings()
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	std::vector<CFlowData>::iterator endIt = m_flowData.end();
	for (std::vector<CFlowData>::iterator it = m_flowData.begin(); it != endIt; ++it)
	{
		if (it->IsValid())
		{
			CFlowData& flowData = *it;

			IFlowNode::SActivationInfo actInfo(this, 0);
			actInfo.myID = (TFlowNodeId)(it - m_flowData.begin());
			actInfo.pEntity = GetIEntityForNode(actInfo.myID);
			flowData.DoForwardingIfNeed(&actInfo);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::GetMemoryUsage(ICrySizer* s) const
{
	{
		SIZER_SUBCOMPONENT_NAME(s, "FlowGraphLocal");
		s->AddObject(m_modifiedNodes);
		s->AddObject(m_activatingNodes);
		s->AddObject(m_finalActivatingNodes);
		s->AddObject(m_deallocatedIds);
		s->AddObject(m_edges);
		s->AddObject(m_regularUpdates);
		s->AddObject(m_activatingNodes);
		s->AddObject(m_nodeNameToId);
		s->AddObject(m_hooks);
		s->AddObject(m_userData);
		s->AddObject(m_inspectors);
	}

	{
		SIZER_SUBCOMPONENT_NAME(s, "FlowData-FlowData-Struct");
		s->AddObject(m_flowData);
	}

	{
		SIZER_SUBCOMPONENT_NAME(s, "NodeNameToIdMap");
		s->AddObject(m_nodeNameToId);
	}
}

void CFlowGraph::GetMemoryUsage(ICrySizer* s) const
{
	s->Add(*this);
	CFlowGraphBase::GetMemoryUsage(s);
}
