// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryAISystem/IAIAction.h>
#include <CrySystem/Scaleform/IFlashUI.h>

#include "GameEngine.h"

#include "FlowGraph.h"
#include "FlowGraphNode.h"
#include "FlowGraphManager.h"
#include "FlowGraphMigrationHelper.h"
#include "Nodes/CommentNode.h"
#include "Nodes/CommentBoxNode.h"
#include "Nodes/TrackEventNode.h"
#include "Nodes/MissingNode.h"
#include "Controls/HyperGraphEditorWnd.h"

#include "Objects/EntityObject.h"
#include "Objects/PrefabObject.h"
#include "Objects/ObjectLoader.h"
#include "Prefabs/PrefabManager.h"

#define FG_INTENSIVE_DEBUG
#undef  FG_INTENSIVE_DEBUG

//////////////////////////////////////////////////////////////////////////
// CUndoFlowGraph
//
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Undo object for CHyperFlowGraph.
// Remarks [AlexL]: The CUndoFlowGraph serializes the graph to XML to store
//                  the current state. If it is an entity FlowGraph we use
//                  the entity's GUID to lookup the FG for the entity, because
//                  when an entity is deleted and then re-created from undo,
//                  the FG is also recreated and hence the FG pointer in the Undo
//                  object is garbage! AIAction's may not suffer from this, but
//                  maybe we should reference these by names then or something else...
//                  Currently for simple node changes [Select, PosChange] we create a
//                  full graph XML as well. The Undo hypernode suffers from the same
//                  problem as Graphs, because nodes are deleted and recreated, so pointer
//                  storing is BAD. Maybe use the nodeID [are these surely be the same on recreation?]
//////////////////////////////////////////////////////////////////////////
class CUndoFlowGraph : public IUndoObject
{
public:
	CUndoFlowGraph(CHyperFlowGraph* pGraph)
	{
		// Stores the current state of given graph.
		assert(pGraph != 0);
		assert(pGraph->IsFlowGraph());

		m_pGraph = 0;
		m_entityGUID = CryGUID::Null();

		CEntityObject* pEntity = pGraph->GetEntity();
		if (pEntity)
		{
			m_entityGUID = pEntity->GetId();
			pEntity->SetLayerModified();
		}
		else
			m_pGraph = pGraph;

		m_redo = 0;
		m_undo = XmlHelpers::CreateXmlNode("HyperGraph");
		pGraph->Serialize(m_undo, false);
#ifdef FG_INTENSIVE_DEBUG
		CryLogAlways("CUndoFlowGraph 0x%p:: Serializing from graph 0x%p", this, pGraph);
#endif
	}

protected:
	CHyperFlowGraph* GetGraph()
	{
		if (m_entityGUID == CryGUID::Null())
		{
			assert(m_pGraph != 0);
			return m_pGraph;
		}
		CBaseObject* pObj = GetIEditorImpl()->GetObjectManager()->FindObject(m_entityGUID);
		assert(pObj != 0);
		if (pObj && pObj->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* pEntity = static_cast<CEntityObject*>(pObj);
			CHyperFlowGraph* pFG = pEntity->GetFlowGraph();
			if (pFG == 0)
			{
				pFG = GetIEditorImpl()->GetFlowGraphManager()->CreateGraphForEntity(pEntity);
				pEntity->SetFlowGraph(pFG);
			}
			assert(pFG != 0);
			return pFG;
		}
		return 0;
	}

	virtual const char* GetDescription() { return "FlowGraph Undo"; };

	virtual void        Undo(bool bUndo)
	{
		CHyperFlowGraph* pGraph = GetGraph();
		assert(pGraph != 0);

		if (bUndo)
		{
#ifdef FG_INTENSIVE_DEBUG
			CryLogAlways("CUndoFlowGraph 0x%p::Undo 1: Serializing to graph 0x%p", this, pGraph);
#endif

			m_redo = XmlHelpers::CreateXmlNode("HyperGraph");
			if (pGraph)
			{
				pGraph->Serialize(m_redo, false);
				GetIEditorImpl()->GetFlowGraphManager()->SendNotifyEvent(EHG_GRAPH_OWNER_CHANGE, pGraph);
			}
		}

#ifdef FG_INTENSIVE_DEBUG
		CryLogAlways("CUndoFlowGraph 0x%p::Undo 2: Serializing to graph 0x%p", this, pGraph);
#endif

		if (pGraph)
		{
			GetIEditorImpl()->GetPrefabManager()->SetSkipPrefabUpdate(true);
			CObjectArchive remappingInfo(GetIEditorImpl()->GetObjectManager(), m_undo, true);
			pGraph->ExtractObjectsPrefabIdToGlobalIdMappingFromPrefab(remappingInfo);

			pGraph->Serialize(m_undo, true, &remappingInfo);
			GetIEditorImpl()->GetFlowGraphManager()->SendNotifyEvent(EHG_GRAPH_INVALIDATE, pGraph);
			CEntityObject* pEntity = pGraph->GetEntity();
			if (pEntity)
				pEntity->SetLayerModified();

			GetIEditorImpl()->GetPrefabManager()->SetSkipPrefabUpdate(false);
			pGraph->SendNotifyEvent(nullptr, EHG_GRAPH_UNDO_REDO);
			// If we are part of the prefab set this graph to the current viewed one.
			// You can potentially be looking at the same graph but in a different prefab.
			// This undo action could be on the not currently viewed one so enforce the refresh of the view
			if (pEntity && pEntity->IsPartOfPrefab())
			{
				GetIEditorImpl()->GetFlowGraphManager()->SetCurrentViewedGraph(pGraph);
			}
		}
	}
	virtual void Redo()
	{
		if (m_redo)
		{
			CHyperFlowGraph* pGraph = GetGraph();
			assert(pGraph != 0);
			if (pGraph)
			{
#ifdef FG_INTENSIVE_DEBUG
				CryLogAlways("CUndoFlowGraph 0x%p::Redo: Serializing to graph 0x%p", this, pGraph);
#endif
				GetIEditorImpl()->GetPrefabManager()->SetSkipPrefabUpdate(true);
				CObjectArchive remappingInfo(GetIEditorImpl()->GetObjectManager(), m_redo, true);
				;
				pGraph->ExtractObjectsPrefabIdToGlobalIdMappingFromPrefab(remappingInfo);

				pGraph->Serialize(m_redo, true, &remappingInfo);
				GetIEditorImpl()->GetFlowGraphManager()->SendNotifyEvent(EHG_GRAPH_INVALIDATE, pGraph);
				CEntityObject* pEntity = pGraph->GetEntity();
				if (pEntity)
					pEntity->SetLayerModified();

				GetIEditorImpl()->GetPrefabManager()->SetSkipPrefabUpdate(false);
				pGraph->SendNotifyEvent(nullptr, EHG_GRAPH_UNDO_REDO);

				// If we are part of the prefab set this graph to the current viewed one.
				// You can potentially be looking at the same graph but in a different prefab.
				// This undo action could be on the not currently viewed one so enforce the refresh of the view
				if (pEntity && pEntity->IsPartOfPrefab())
				{
					GetIEditorImpl()->GetFlowGraphManager()->SetCurrentViewedGraph(pGraph);
				}
			}
			else
			{
				CryLogAlways("CUndoFlowGraph 0x%p: Got 0 graph", this);
			}
		}
	}
private:
	CryGUID          m_entityGUID;
	CHyperFlowGraph* m_pGraph;
	XmlNodeRef       m_undo;
	XmlNodeRef       m_redo;
};



//////////////////////////////////////////////////////////////////////////
// CHyperFlowGraph
//////////////////////////////////////////////////////////////////////////

CHyperFlowGraph::CHyperFlowGraph(CHyperGraphManager* pManager, const char* sGroupName/* = "" */, CEntityObject* pEntity /*= nullptr*/)
	: CHyperGraph(pManager)
	, m_pGameFlowGraph(nullptr)
	, m_pEntity(nullptr)
	, m_pAIAction(nullptr)
	, m_pUIAction(nullptr)
	, m_pCustomAction(nullptr)
	, m_mpType(eMPT_ClientServer)
{
#ifdef FG_INTENSIVE_DEBUG
	CryLogAlways("CHyperFlowGraph: Creating 0x%p", this);
#endif

	// set group name and entity directly, without sending changed events */
	m_groupName = sGroupName;
	m_pEntity = pEntity;

	IFlowSystem* pFlowSystem = GetIEditorImpl()->GetGameEngine()->GetIFlowSystem();
	if (pFlowSystem)
	{
		m_pGameFlowGraph = pFlowSystem->CreateFlowGraph();
		m_pGameFlowGraph->SetEnabled(true);
		m_pGameFlowGraph->RegisterFlowNodeActivationListener(this);

		if (m_pEntity && m_pEntity->GetIEntity())
		{
			m_pGameFlowGraph->SetGraphEntity(m_pEntity->GetEntityId(), 0);
		}
	}
	GetIEditorImpl()->GetFlowGraphManager()->RegisterGraph(this);
}

CHyperFlowGraph::CHyperFlowGraph(CHyperGraphManager* pManager, IFlowGraph* pIFlowGraph)
	: CHyperGraph(pManager)
	, m_pGameFlowGraph(pIFlowGraph)
	, m_pEntity(nullptr)
	, m_pAIAction(nullptr)
	, m_pUIAction(nullptr)
	, m_pCustomAction(nullptr)
	, m_mpType(eMPT_ClientServer)
{
#ifdef FG_INTENSIVE_DEBUG
	CryLogAlways("CHyperFlowGraph: Creating 0x%p", this);
#endif

	if (m_pGameFlowGraph)
		m_pGameFlowGraph->RegisterFlowNodeActivationListener(this);

	GetIEditorImpl()->GetFlowGraphManager()->RegisterGraph(this);
}


CHyperFlowGraph::~CHyperFlowGraph()
{
#ifdef FG_INTENSIVE_DEBUG
	CryLogAlways("CHyperFlowGraph: About to delete 1 0x%p", this);
#endif

	if (m_pGameFlowGraph)
		m_pGameFlowGraph->RemoveFlowNodeActivationListener(this);

	// first check is the manager still there!
	// could be deleted in which case we don't
	// care about unregistering.
	CFlowGraphManager* pManager = GetIEditorImpl()->GetFlowGraphManager();
	if (pManager)
		pManager->UnregisterGraph(this);

#ifdef FG_INTENSIVE_DEBUG
	CryLogAlways("CHyperFlowGraph: About to delete 2 0x%p", this);
#endif
}


IFlowGraph* CHyperFlowGraph::GetIFlowGraph()
{
	if (!m_pGameFlowGraph)
	{
		IFlowSystem* pFlowSystem = GetIEditorImpl()->GetGameEngine()->GetIFlowSystem();
		if (pFlowSystem)
		{
			m_pGameFlowGraph = pFlowSystem->CreateFlowGraph();
			m_pGameFlowGraph->RegisterFlowNodeActivationListener(this);
		}
	}

	return m_pGameFlowGraph;
}


void CHyperFlowGraph::SetIFlowGraph(IFlowGraph* pFlowGraph)
{
	if (m_pGameFlowGraph)
	{
		m_pGameFlowGraph->RemoveFlowNodeActivationListener(this);
	}

	m_pGameFlowGraph = pFlowGraph;

	if (m_pGameFlowGraph)
	{
		m_pGameFlowGraph->RegisterFlowNodeActivationListener(this);
	}
}


bool CHyperFlowGraph::OnFlowNodeActivation(IFlowGraphPtr pFlowGraph, TFlowNodeId srcNode, TFlowPortId srcPort, TFlowNodeId toNode, TFlowPortId toPort, const char* value)
{
	NodesMap::iterator it = m_nodesMap.begin();
	NodesMap::iterator end = m_nodesMap.end();
	for (; it != end; ++it)
	{
		CFlowNode* pNode = static_cast<CFlowNode*>(it->second.get());
		TFlowNodeId id = pNode->GetFlowNodeId();
		if (id == toNode)
		{
			pNode->DebugPortActivation(toPort, value, GetIFlowGraph()->IsInInitializationPhase());
			return false;
		}
		//else if(id == srcNode)
		//	pNode->DebugPortActivation(srcPort, "out");
	}

	return false;
}


void CHyperFlowGraph::OnEnteringGameMode()
{
	IHyperGraphEnumerator* pEnum = GetNodesEnumerator();
	for (IHyperNode* pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
	{
		CHyperNode* pNode = (CHyperNode*)pINode;
		pNode->OnEnteringGameMode();
	}
	pEnum->Release();
}


void CHyperFlowGraph::ClearAll()
{
	GetIFlowGraph()->Clear();
	__super::ClearAll();
}


void CHyperFlowGraph::RemoveEdge(CHyperEdge* pEdge)
{
	// Remove link in flow system.
	CFlowEdge* pFlowEdge = (CFlowEdge*)pEdge;
	GetIFlowGraph()->UnlinkNodes(pFlowEdge->addr_out, pFlowEdge->addr_in);

	CHyperGraph::RemoveEdge(pEdge);
}


void CHyperFlowGraph::EnableEdge(CHyperEdge* pEdge, bool bEnable)
{
	CHyperGraph::EnableEdge(pEdge, bEnable);

	SetModified();

	CFlowEdge* pFlowEdge = (CFlowEdge*)pEdge;

	if (!bEnable)
	{
		GetIFlowGraph()->UnlinkNodes(pFlowEdge->addr_out, pFlowEdge->addr_in);
	}
	else
	{
		GetIFlowGraph()->LinkNodes(pFlowEdge->addr_out, pFlowEdge->addr_in);
	}
}


CHyperEdge* CHyperFlowGraph::MakeEdge(CHyperNode* pNodeOut, CHyperNodePort* pPortOut, CHyperNode* pNodeIn, CHyperNodePort* pPortIn, bool bEnabled)
{
	assert(pNodeIn);
	assert(pNodeOut);
	assert(pPortIn);
	assert(pPortOut);
	CFlowEdge* pEdge = new CFlowEdge;

	pEdge->nodeIn = pNodeIn->GetId();
	pEdge->nodeOut = pNodeOut->GetId();
	pEdge->portIn = pPortIn->GetName();
	pEdge->portOut = pPortOut->GetName();
	pEdge->enabled = bEnabled;

	pEdge->nPortIn = pPortIn->nPortIndex;
	pEdge->nPortOut = pPortOut->nPortIndex;

	++pPortOut->nConnected;
	++pPortIn->nConnected;

	RegisterEdge(pEdge);

	SetModified();

	// Create link in flow system.
	pEdge->addr_in = SFlowAddress(((CFlowNode*)pNodeIn)->GetFlowNodeId(), pPortIn->nPortIndex, false);
	pEdge->addr_out = SFlowAddress(((CFlowNode*)pNodeOut)->GetFlowNodeId(), pPortOut->nPortIndex, true);
	if (bEnabled)
	{
		m_pGameFlowGraph->LinkNodes(pEdge->addr_out, pEdge->addr_in);
		if (!m_bLoadingNow)
			m_pGameFlowGraph->InitializeValues();
	}
	return pEdge;
}


void CHyperFlowGraph::SetEntity(CEntityObject* pEntity, bool bAdjustGraphNodes)
{
	assert(pEntity);
	m_pEntity = pEntity;

	if (m_pGameFlowGraph != NULL && m_pEntity != NULL && m_pEntity->GetIEntity())
	{
		m_pGameFlowGraph->SetGraphEntity(m_pEntity->GetEntityId(), 0);
	}
	if (bAdjustGraphNodes)
	{
		IHyperGraphEnumerator* pEnum = GetNodesEnumerator();
		for (IHyperNode* pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
		{
			CHyperNode* pNode = (CHyperNode*)pINode;
			if (pNode->CheckFlag(EHYPER_NODE_GRAPH_ENTITY))
				((CFlowNode*)pNode)->SetEntity(pEntity);
		}
		pEnum->Release();
	}
	GetIEditorImpl()->GetFlowGraphManager()->SendNotifyEvent(EHG_GRAPH_OWNER_CHANGE, this);
	GetIEditorImpl()->GetFlowGraphManager()->SendNotifyEvent(EHG_GRAPH_UPDATE_ENTITY, this);
}


void CHyperFlowGraph::SetAIAction(IAIAction* pAIAction)
{
	m_pAIAction = pAIAction;

	GetIFlowGraph()->SetType(IFlowGraph::eFGT_AIAction);

	if (0 != m_pGameFlowGraph)
	{
		m_pGameFlowGraph->SetAIAction(pAIAction); // KLUDE to make game flowgraph aware that this is an AI action
		m_pGameFlowGraph->SetAIAction(0);         // KLUDE cont'd: for safety we re-set afterwards (game flowgraph remembers)
	}
	GetIEditorImpl()->GetFlowGraphManager()->SendNotifyEvent(EHG_GRAPH_OWNER_CHANGE, this);
}


void CHyperFlowGraph::SetUIAction(IUIAction* pUIAction)
{
	assert(!m_pUIAction);
	m_pUIAction = pUIAction;

	if (m_pGameFlowGraph)
		m_pGameFlowGraph->RemoveFlowNodeActivationListener(this);

	m_pGameFlowGraph = pUIAction->GetFlowGraph();
	m_pGameFlowGraph->SetType(IFlowGraph::eFGT_UIAction);
	stack_string debugName = "[UI Action] ";
	debugName.append(pUIAction->GetName());
	m_pGameFlowGraph->SetDebugName(debugName);

	if (m_pGameFlowGraph)
		m_pGameFlowGraph->RegisterFlowNodeActivationListener(this);

	((CFlowGraphManager*)GetManager())->SendNotifyEvent(EHG_GRAPH_OWNER_CHANGE, this);
}


void CHyperFlowGraph::SetCustomAction(ICustomAction* pCustomAction)
{
	m_pCustomAction = pCustomAction;

	GetIFlowGraph()->SetType(IFlowGraph::eFGT_CustomAction);

	if (0 != m_pGameFlowGraph)
	{
		m_pGameFlowGraph->SetCustomAction(pCustomAction); // KLUDE to make game flowgraph aware that this is an Custom action
		m_pGameFlowGraph->SetCustomAction(0);             // KLUDE cont'd: for safety we re-set afterwards (game flowgraph remembers)
	}
	GetIEditorImpl()->GetFlowGraphManager()->SendNotifyEvent(EHG_GRAPH_OWNER_CHANGE, this);
}


IHyperGraph* CHyperFlowGraph::Clone()
{
	CHyperFlowGraph* pGraph = new CHyperFlowGraph(GetManager());

	pGraph->m_bLoadingNow = true;

	IHyperGraphEnumerator* pEnum = GetNodesEnumerator();
	for (IHyperNode* pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
	{
		CHyperNode* pSrcNode = (CHyperNode*)pINode;

		// create a clone of the node
		CHyperNode* pNewNode = pSrcNode->Clone();

		// comment is the only node which is no CFlowNode ... not entirely true anymore
		if (!pNewNode->IsFlowNode())
		{
			pNewNode->SetGraph(pGraph);
			pNewNode->Init();
		}
		else
		{
			// clone the node, this creates only our Editor shallow node
			// also assigns the m_pEntity (which we might have to correct!)
			CFlowNode* pNode = static_cast<CFlowNode*>(pNewNode);

			// set the graph of the newly created node
			pNode->SetGraph(pGraph);

			// create a real flowgraph node (note: inputs are not yet set!)
			pNode->Init();

			// set the inputs of the base FG node (shallow already has correct values
			// because pVars are copied by pSrcNode->Clone()
			pNode->SetInputs(false, true);  // Make sure entities in normal ports are set!

			// WE CAN'T DO IT HERE, BECAUSE THE pNode->GetDefaultEntity returns always 0
			// THE ENTITY IS ADJUSTED IN THE SetEntity call to the CHyperFlowGraph
			// finally we have to set the correct entity for this node
			// e.g. GraphEntity has to be updated set to new graph entity
			if (pSrcNode->CheckFlag(EHYPER_NODE_GRAPH_ENTITY))
			{
				pNode->SetEntity(0);
				pNode->SetFlag(EHYPER_NODE_GRAPH_ENTITY, true);
			}
			else if (pSrcNode->CheckFlag(EHYPER_NODE_ENTITY))
			{
				CFlowNode* pSrcFlowNode = static_cast<CFlowNode*>(pSrcNode);
				pNode->SetEntity(pSrcFlowNode->GetEntity());
			}
		}

		// and add the node
		pGraph->AddNode(pNewNode);
	}
	pEnum->Release();

	std::vector<CHyperEdge*> edges;
	GetAllEdges(edges);
	for (int i = 0; i < edges.size(); i++)
	{
		CHyperEdge& edge = *(edges[i]);

		CHyperNode* pNodeIn = (CHyperNode*)pGraph->FindNode(edge.nodeIn);
		CHyperNode* pNodeOut = (CHyperNode*)pGraph->FindNode(edge.nodeOut);
		if (!pNodeIn || !pNodeOut)
			continue;

		CHyperNodePort* pPortIn = pNodeIn->FindPort(edge.portIn, true);
		CHyperNodePort* pPortOut = pNodeOut->FindPort(edge.portOut, false);
		if (!pPortIn || !pPortOut)
			continue;

		pGraph->MakeEdge(pNodeOut, pPortOut, pNodeIn, pPortIn, edge.enabled);
	}

	pGraph->m_bLoadingNow = false;

	// clone graph tokens
	for (size_t i = 0; i < m_pGameFlowGraph->GetGraphTokenCount(); ++i)
	{
		const IFlowGraph::SGraphToken* pToken = m_pGameFlowGraph->GetGraphToken(i);
		pGraph->m_pGameFlowGraph->AddGraphToken(*pToken);
	}

	OnPostLoad();

	pGraph->SetEnabled(IsEnabled());
	pGraph->SetModified();

	return pGraph;
}


void CHyperFlowGraph::Serialize(XmlNodeRef& node, bool bLoading, CObjectArchive* ar)
{
	{
		// when loading an entity flowgraph assign a name in advance, so
		// error messages are bit more descriptive
		if (bLoading && m_pEntity)
			SetName(m_pEntity->GetName());

		__super::Serialize(node, bLoading, ar);
		if (bLoading)
		{
			bool enabled;
			if (node->getAttr("enabled", enabled) == false)
				enabled = true;
			SetEnabled(enabled);

			EMultiPlayerType mpType = eMPT_ServerOnly;
			const char* mpTypeAttr = node->getAttr("MultiPlayer");
			if (mpTypeAttr)
			{
				if (strcmp("ClientOnly", mpTypeAttr) == 0)
					mpType = eMPT_ClientOnly;
				else if (strcmp("ClientServer", mpTypeAttr) == 0)
					mpType = eMPT_ClientServer;
			}
			SetMPType(mpType);

			m_pGameFlowGraph->RemoveGraphTokens();
			XmlNodeRef graphTokens = node->findChild("GraphTokens");
			if (graphTokens)
			{
				int nTokens = graphTokens->getChildCount();
				for (int i = 0; i < nTokens; ++i)
				{
					XmlString tokenName;
					int tokenType;

					XmlNodeRef tokenXml = graphTokens->getChild(i);
					tokenName = tokenXml->getAttr("Name");
					tokenXml->getAttr("Type", tokenType);

					IFlowGraph::SGraphToken token;
					token.name = tokenName.c_str();
					token.type = (EFlowDataTypes)tokenType;

					m_pGameFlowGraph->AddGraphToken(token);
				}
			}
		}
		else
		{
			node->setAttr("enabled", IsEnabled());
			EMultiPlayerType mpType = GetMPType();
			if (mpType == eMPT_ClientOnly)
				node->setAttr("MultiPlayer", "ClientOnly");
			else if (mpType == eMPT_ClientServer)
				node->setAttr("MultiPlayer", "ClientServer");
			else
				node->setAttr("MultiPlayer", "ServerOnly");

			XmlNodeRef tokens = node->newChild("GraphTokens");
			size_t gtCount = m_pGameFlowGraph->GetGraphTokenCount();
			for (size_t i = 0; i < gtCount; ++i)
			{
				const IFlowGraph::SGraphToken* pToken = m_pGameFlowGraph->GetGraphToken(i);
				XmlNodeRef tokenXml = tokens->newChild("Token");

				tokenXml->setAttr("Name", pToken->name);
				tokenXml->setAttr("Type", pToken->type);
			}
		}
	}
}


bool CHyperFlowGraph::Migrate(XmlNodeRef& node)
{
	CFlowGraphMigrationHelper& migHelper = GetIEditorImpl()->GetFlowGraphManager()->GetMigrationHelper();

	bool bChanged = migHelper.Substitute(node);

	const std::vector<CFlowGraphMigrationHelper::ReportEntry>& report = migHelper.GetReport();
	if (report.size() > 0)
	{
		for (int i = 0; i < report.size(); ++i)
		{
			const CFlowGraphMigrationHelper::ReportEntry& entry = report[i];
			CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, entry.description);
			CryLogAlways("CHyperFlowGraph::Migrate: FG '%s': %s", GetName(), entry.description.GetString());
		}
	}
	return bChanged;
}


void CHyperFlowGraph::OnPostLoad()
{
	if (m_pGameFlowGraph)
	{
		m_pGameFlowGraph->InitializeValues();
		m_pGameFlowGraph->PrecacheResources();
	}
}

void CHyperFlowGraph::PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx)
{
	// all flownodes need to have a post clone
	__super::PostClone(pFromObject, ctx);
}


void CHyperFlowGraph::SetGroupName(const CString& sName)
{
	// we have to override so everybody knows that we changed the group. maybe do this in CHyperGraph instead
	__super::SetGroupName(sName);
	GetIEditorImpl()->GetFlowGraphManager()->SendNotifyEvent(EHG_GRAPH_OWNER_CHANGE, this);
}


void CHyperFlowGraph::SetEnabled(bool bEnable)
{
	__super::SetEnabled(bEnable);
	// Enable/Disable game flow graph.
	if (m_pGameFlowGraph)
		m_pGameFlowGraph->SetEnabled(bEnable);
}


void CHyperFlowGraph::InitializeFromGame()
{
}


IUndoObject* CHyperFlowGraph::CreateUndo()
{
	// create undo object
	return new CUndoFlowGraph(this);
}


bool CHyperFlowGraph::CanConnectPorts(CHyperNode* pSrcNode, CHyperNodePort* pSrcPort, CHyperNode* pTrgNode, CHyperNodePort* pTrgPort, CHyperEdge* pExistingEdge)
{
	// Target node cannot be a track event node
	if (strcmp(pTrgNode->GetClassName(), TRACKEVENTNODE_CLASS) == 0)
		return false;

	if (strcmp(pTrgNode->GetClassName(), MISSING_NODE_CLASS) == 0 || strcmp(pSrcNode->GetClassName(), MISSING_NODE_CLASS) == 0)
	{
		Warning("Trying to link a missing node!");
		return false;
	}

	bool bCanConnect = __super::CanConnectPorts(pSrcNode, pSrcPort, pTrgNode, pTrgPort, pExistingEdge);
	if (bCanConnect && m_pGameFlowGraph)
	{
		CFlowNode* pSrcFlowNode = static_cast<CFlowNode*>(pSrcNode);
		TFlowNodeId srcId = pSrcFlowNode->GetFlowNodeId();
		SFlowNodeConfig srcConfig;
		m_pGameFlowGraph->GetNodeConfiguration(srcId, srcConfig);

		CFlowNode* pTrgFlowNode = static_cast<CFlowNode*>(pTrgNode);
		TFlowNodeId trgId = pTrgFlowNode->GetFlowNodeId();
		SFlowNodeConfig trgConfig;
		m_pGameFlowGraph->GetNodeConfiguration(trgId, trgConfig);

		if (srcConfig.pOutputPorts && trgConfig.pInputPorts)
		{
			const SOutputPortConfig* pSrcPortConfig = srcConfig.pOutputPorts + pSrcPort->nPortIndex;
			const SInputPortConfig* pTrgPortConfig = trgConfig.pInputPorts + pTrgPort->nPortIndex;
			int srcType = pSrcPortConfig->type;
			int trgType = pTrgPortConfig->defaultData.GetType();
			if (srcType == eFDT_EntityId)
			{
				bCanConnect = true;
			}
			else if (trgType == eFDT_EntityId)
			{
				bCanConnect = (srcType == eFDT_EntityId || srcType == eFDT_Int || srcType == eFDT_Any);
			}
			if (!bCanConnect)
			{
				Warning("An Entity port can only be connected from an Entity or an Int port!");
			}
		}
	}
	return bCanConnect;
}


void CHyperFlowGraph::ValidateEdges(CHyperNode* pNode)
{
	std::vector<CHyperEdge*> edges;
	if (FindEdges(pNode, edges))
	{
		for (int i = 0; i < edges.size(); i++)
		{
			CHyperNode* nodeIn = (CHyperNode*)FindNode(edges[i]->nodeIn);
			if (nodeIn)
			{
				CHyperNodePort* portIn = nodeIn->FindPort(edges[i]->portIn, true);
				if (!portIn)
					RemoveEdge(edges[i]);
			}
			CHyperNode* nodeOut = (CHyperNode*)FindNode(edges[i]->nodeOut);
			if (nodeOut)
			{
				CHyperNodePort* portOut = nodeOut->FindPort(edges[i]->portOut, false);
				if (!portOut)
					RemoveEdge(edges[i]);
			}
		}
	}
}

CFlowNode* CHyperFlowGraph::FindFlowNode(TFlowNodeId id) const
{
	NodesMap::const_iterator it = m_nodesMap.begin();
	NodesMap::const_iterator end = m_nodesMap.end();
	for (; it != end; ++it)
	{
		CFlowNode* pNode = static_cast<CFlowNode*>(it->second.get());
		TFlowNodeId nodeID = pNode->GetFlowNodeId();
		if (id == nodeID)
		{
			return pNode;
		}
	}

	return NULL;
}

void CHyperFlowGraph::ExtractObjectsPrefabIdToGlobalIdMappingFromPrefab(CObjectArchive& archive)
{
	if (CEntityObject* pEntity = GetEntity())
	{
		if (CPrefabObject* pPrefabObject = (CPrefabObject*)pEntity->GetPrefab())
		{
			std::vector<CBaseObject*> childs;
			pPrefabObject->GetAllPrefabFlagedChildren(childs);

			for (int i = 0, count = childs.size(); i < count; ++i)
				archive.RemapID(childs[i]->GetIdInPrefab(), childs[i]->GetId());
		}
	}
}

