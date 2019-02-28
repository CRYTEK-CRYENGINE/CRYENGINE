// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowGraphNode.h"
#include "FlowGraphVariables.h"
#include "GameEngine.h"
#include "FlowGraph.h"
#include "Objects\EntityObject.h"
#include "Objects\Group.h"
#include "Objects\ObjectLoader.h"
#include "Prefabs\PrefabManager.h"
#include "Prefabs\PrefabEvents.h"

#define FG_ALLOW_STRIPPED_PORTNAMES
//#undef  FG_ALLOW_STRIPPED_PORTNAMES

#define FG_WARN_ABOUT_STRIPPED_PORTNAMES
#undef  FG_WARN_ABOUT_STRIPPED_PORTNAMES

#define TITLE_COLOR_APPROVED Gdiplus::Color(20, 200, 20)
#define TITLE_COLOR_ADVANCED Gdiplus::Color(40, 40, 220)
#define TITLE_COLOR_DEBUG    Gdiplus::Color(220, 180, 20)
#define TITLE_COLOR_OBSOLETE Gdiplus::Color(255, 0, 0)

//////////////////////////////////////////////////////////////////////////
CFlowNode::CFlowNode()
	: CHyperNode()
{
	m_flowNodeId = InvalidFlowNodeId;
	m_szDescription = "";
	m_szUIClassName = 0; // 0 is ok -> means use GetClassName()
	m_entityGuid = CryGUID::Null();
	m_pEntity = NULL;
}

//////////////////////////////////////////////////////////////////////////
CFlowNode::~CFlowNode()
{
	if (strncmp(GetClassName(), "Prefab:", 7) == 0)
	{
		// Flow node deletion happens from multiple different paths (Sometimes notified from hypergraph listeners, from entity deletion, from prefab deletion
		// Notify prefab events manager about flownode removal here to cover all cases
		// The PrefabEvents only needs to prevent dangling pointers to prefab instance and source nodes. skip all other nodes.

		CPrefabManager* pPrefabManager = GetIEditor()->GetPrefabManager();
		if (pPrefabManager)
		{
			CPrefabEvents* pPrefabEvents = pPrefabManager->GetPrefabEvents();
			CRY_ASSERT(pPrefabEvents != NULL);
			pPrefabEvents->OnFlowNodeRemoval(this);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IFlowGraph* CFlowNode::GetIFlowGraph() const
{
	CHyperFlowGraph* pGraph = static_cast<CHyperFlowGraph*>(GetGraph());
	return pGraph ? pGraph->GetIFlowGraph() : nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::Init()
{
	CString sNodeName = "<unknown>";

	if (m_name.IsEmpty())
		sNodeName.Format("%d", GetId());
	else
		sNodeName = m_name;
	m_flowNodeId = GetIFlowGraph()->CreateNode(m_classname, sNodeName);
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::Done()
{
	if (m_flowNodeId != InvalidFlowNodeId)
		GetIFlowGraph()->RemoveNode(m_flowNodeId);
	m_flowNodeId = InvalidFlowNodeId;
	m_pEntity = NULL;
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::SetName(const char* sName)
{
	if (GetIFlowGraph()->SetNodeName(m_flowNodeId, sName))
		__super::SetName(sName);
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::SetFromNodeId(TFlowNodeId flowNodeId)
{
	m_flowNodeId = flowNodeId;

	IFlowNodeData* pNode = GetIFlowGraph()->GetNodeData(m_flowNodeId);
	if (pNode)
	{
		//m_entityId = pNode->GetEntityId();
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::DebugPortActivation(TFlowPortId port, const char* value,  bool bIsInitializationStep)
{
	m_portActivationMap[port] = string(value);
	m_portActivationAdditionalDebugInformationMap[port] = bIsInitializationStep;
	m_debugPortActivations.push_back(port);

	CHyperGraph* pGraph = static_cast<CHyperGraph*>(GetGraph());
	if (pGraph)
	{
		IncrementDebugCount();
		pGraph->SendNotifyEvent(this, EHG_NODE_CHANGE_DEBUG_PORT);
	}
}

void CFlowNode::ResetDebugPortActivation(CHyperNodePort* port)
{
	std::vector<TFlowPortId>::iterator result = std::find(m_debugPortActivations.begin(), m_debugPortActivations.end(), port->nPortIndex);
	if (result != m_debugPortActivations.end())
	{
		m_debugPortActivations.erase(result);
	}
}

bool CFlowNode::IsDebugPortActivated(CHyperNodePort* port) const
{
	std::vector<TFlowPortId>::const_iterator result = std::find(m_debugPortActivations.begin(), m_debugPortActivations.end(), port->nPortIndex);
	return (result != m_debugPortActivations.end());
}

//////////////////////////////////////////////////////////////////////////
bool CFlowNode::IsPortActivationModified(const CHyperNodePort* port) const
{
	if (port)
	{
		return m_portActivationMap.count(port->nPortIndex);
	}
	return m_portActivationMap.size() > 0;
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::ClearDebugPortActivation()
{
	ResetDebugCount();
	m_portActivationMap.clear();
	m_portActivationAdditionalDebugInformationMap.clear();
	m_debugPortActivations.clear();
}

//////////////////////////////////////////////////////////////////////////
CString CFlowNode::GetDebugPortValue(const CHyperNodePort& pp)
{
	const char* value = stl::find_in_map(m_portActivationMap, pp.nPortIndex, NULL);
	if (value && strlen(value) > 0 && stricmp(value, "out") && stricmp(value, "unknown"))
	{
		string portName = GetPortName(pp);
		portName += "=";
		portName += value;
		return portName.c_str();
	}
	return GetPortName(pp);
}

bool CFlowNode::GetAdditionalDebugPortInformation(const CHyperNodePort& pp, bool& bOutIsInitialization)
{
	std::vector<TFlowPortId>::const_iterator result = std::find(m_debugPortActivations.begin(), m_debugPortActivations.end(), pp.nPortIndex);
	if (result != m_debugPortActivations.end())
	{
		bOutIsInitialization = m_portActivationAdditionalDebugInformationMap[pp.nPortIndex];
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
CHyperNode* CFlowNode::Clone()
{
	CFlowNode* pNode = new CFlowNode;
	pNode->CopyFrom(*this);

	pNode->m_entityGuid = m_entityGuid;
	pNode->m_pEntity = m_pEntity;
	pNode->m_flowSystemNodeFlags = m_flowSystemNodeFlags;
	pNode->m_szDescription = m_szDescription;
	pNode->m_szUIClassName = m_szUIClassName;

#if 1 // AlexL: currently there is a per-variable-instance GetCustomItem interface
	// as there is a problem with undo
	// tell all input-port variables which node they belong to
	// output-port variables are not told (currently no need)
	Ports::iterator iter = pNode->m_inputs.begin();
	while (iter != pNode->m_inputs.end())
	{
		IVariable* pVar = (*iter).pVar;
		CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*>(pVar->GetUserData());
		if (pGetCustomItems != 0)
		{
			pGetCustomItems->SetFlowNode(pNode);
		}
		++iter;
	}
#endif

	return pNode;
}

//////////////////////////////////////////////////////////////////////////
IUndoObject* CFlowNode::CreateUndo()
{
	// we create a full copy of the flowgraph. See comment in CHyperFlowGraph::CUndoFlowGraph
	CHyperGraph* pGraph = static_cast<CHyperGraph*>(GetGraph());
	assert(pGraph != 0);
	return pGraph->CreateUndo();
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::SetSelectedEntity()
{
	CBaseObject* pSelObj = GetIEditorImpl()->GetSelectedObject();
	if (pSelObj && pSelObj->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		SetEntity((CEntityObject*)pSelObj);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::SetDefaultEntity()
{
	CEntityObject* pEntity = ((CHyperFlowGraph*)GetGraph())->GetEntity();
	SetEntity(pEntity);
	SetFlag(EHYPER_NODE_GRAPH_ENTITY, true);
	SetFlag(EHYPER_NODE_GRAPH_ENTITY2, false);
}

//////////////////////////////////////////////////////////////////////////
CEntityObject* CFlowNode::GetDefaultEntity() const
{
	CEntityObject* pEntity = ((CHyperFlowGraph*)GetGraph())->GetEntity();
	return pEntity;
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::SetEntity(CEntityObject* pEntity)
{
	SetFlag(EHYPER_NODE_ENTITY | EHYPER_NODE_ENTITY_VALID, true);
	SetFlag(EHYPER_NODE_GRAPH_ENTITY, false);
	SetFlag(EHYPER_NODE_GRAPH_ENTITY2, false);
	m_pEntity = pEntity;
	if (pEntity)
	{
		if (pEntity != GetDefaultEntity())
			m_entityGuid = pEntity->GetId();
		else
		{
			m_entityGuid = CryGUID::Null();
			SetFlag(EHYPER_NODE_GRAPH_ENTITY, true);
		}
	}
	else
	{
		m_entityGuid = CryGUID::Null();
		SetFlag(EHYPER_NODE_ENTITY_VALID, false);
	}
	GetIFlowGraph()->SetEntityId(m_flowNodeId, m_pEntity ? m_pEntity->GetEntityId() : 0);
}

//////////////////////////////////////////////////////////////////////////
CEntityObject* CFlowNode::GetEntity() const
{
	if (m_pEntity && m_pEntity->CheckFlags(OBJFLAG_DELETED))
		return 0;
	return m_pEntity;
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::Unlinked(bool bInput)
{
	if (bInput)
		SetInputs(false, true);
	Invalidate(true, true);
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::SetInputs(bool bActivate, bool bForceResetEntities)
{
	IFlowGraph* pGraph = GetIFlowGraph();

	// small hack to make sure that entity id on target-entity nodes doesnt
	// get reset here
	int startPort = 0;
	if (CheckFlag(EHYPER_NODE_ENTITY))
		startPort = 1;

	for (int i = startPort; i < m_inputs.size(); i++)
	{
		IVariable* pVar = m_inputs[i].pVar;
		int type = pVar->GetType();
		if (type == IVariable::UNKNOWN)
			continue;

		// all variables which are NOT editable are not set!
		// e.g. EntityIDs
		if (bForceResetEntities == false && pVar->GetFlags() & IVariable::UI_DISABLED)
			continue;

		TFlowInputData value;
		bool bSet = false;

		switch (type)
		{
		case IVariable::INT:
			{
				int v;
				pVar->Get(v);
				bSet = value.Set(v);
			}
			break;
		case IVariable::BOOL:
			{
				bool v;
				pVar->Get(v);
				bSet = value.Set(v);
			}
			break;
		case IVariable::FLOAT:
			{
				float v;
				pVar->Get(v);
				bSet = value.Set(v);
			}
			break;
		case IVariable::VECTOR:
			{
				Vec3 v;
				pVar->Get(v);
				bSet = value.Set(v);
			}
			break;
		case IVariable::STRING:
			{
				CString v;
				pVar->Get(v);
				bSet = value.Set(string((const char*)v));
				//string *str = value.GetPtr<string>();
			}
			break;
		}
		if (bSet)
		{
			if (bActivate)
			{
				// we explicitly set the value here, because the flowgraph might not be enabled
				// in which case the ActivatePort would not set the value
				pGraph->SetInputValue(m_flowNodeId, i, value);
				pGraph->ActivatePort(SFlowAddress(m_flowNodeId, i, false), value);
			}
			else
				pGraph->SetInputValue(m_flowNodeId, i, value);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::OnInputsChanged()
{
	SetInputs(true);
	SetModified();
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::OnEnteringGameMode()
{
	SetInputs(false);
}

//////////////////////////////////////////////////////////////////////////
const char* CFlowNode::GetDescription() const
{
	if (m_szDescription && *m_szDescription)
		return m_szDescription;
	return "";
}

//////////////////////////////////////////////////////////////////////////
const char* CFlowNode::GetUIClassName() const
{
	if (m_szUIClassName && *m_szUIClassName)
		return m_szUIClassName;
	return GetClassName();
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::Serialize(XmlNodeRef& node, bool bLoading, CObjectArchive* ar)
{
	__super::Serialize(node, bLoading, ar);

	if (bLoading)
	{
#ifdef  FG_ALLOW_STRIPPED_PORTNAMES
		// backwards compatibility
		// variables (port values) are now saved with its real name

		XmlNodeRef portsNode = node->findChild("Inputs");
		if (portsNode)
		{
			for (int i = 0; i < m_inputs.size(); i++)
			{
				IVariable* pVar = m_inputs[i].pVar;
				if (pVar->GetType() != IVariable::UNKNOWN)
				{
					if (portsNode->haveAttr(pVar->GetName()) == false)
					{
						// if we did not find a value for the variable we try to use the old name
						// with the stripped version
						const char* sVarName = pVar->GetName();
						const char* sSpecial = strchr(sVarName, '_');
						if (sSpecial)
							sVarName = sSpecial + 1;
						const char* val = portsNode->getAttr(sVarName);
						if (val[0])
						{
							pVar->Set(val);
	#ifdef FG_WARN_ABOUT_STRIPPED_PORTNAMES
							CryLogAlways("CFlowNode::Serialize: FG '%s': Value of Deprecated Variable %s -> %s successfully resolved.",
							             GetGraph()->GetName(), sVarName, pVar->GetName());
							CryLogAlways(" --> To get rid of this warning re-save flowgraph!", sVarName);
	#endif
						}
	#ifdef FG_WARN_ABOUT_STRIPPED_PORTNAMES
						else
						{
							CryLogAlways("CFlowNode::Serialize: FG '%s': Can't resolve value for '%s' <may not be severe>", GetGraph()->GetName(), sVarName);
						}
	#endif
					}
				}
			}
		}
#endif

		SetInputs(false);

		m_nFlags &= ~(EHYPER_NODE_GRAPH_ENTITY | EHYPER_NODE_GRAPH_ENTITY2);

		m_pEntity = 0;
		if (CheckFlag(EHYPER_NODE_ENTITY))
		{
			const bool bIsAIOrCustomAction = (m_pGraph != 0 && ((m_pGraph->GetAIAction() != 0) || (m_pGraph->GetCustomAction() != 0)));
			if (bIsAIOrCustomAction)
			{
				SetEntity(0);
			}
			else
			{
				if (node->getAttr("EntityGUID", m_entityGuid))
				{
					// If empty GUId then it is a default graph entity.
					if (m_entityGuid == CryGUID::Null())
					{
						SetFlag(EHYPER_NODE_GRAPH_ENTITY, true);
						SetEntity(GetDefaultEntity());
					}
					else
					{
						CryGUID origGuid = m_entityGuid;
						if (ar)
						{
							// use ar to remap ids
							m_entityGuid = ar->ResolveID(m_entityGuid);
						}
						CBaseObject* pObj = GetIEditorImpl()->GetObjectManager()->FindObject(m_entityGuid);
						if (!pObj)
						{
							SetEntity(NULL);
							CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, "FlowGraph %s Node %d (Class %s) targets unknown entity %s",
								m_pGraph->GetName(), GetId(), GetClassName(), origGuid.ToString());
						}
						else if (pObj->IsKindOf(RUNTIME_CLASS(CEntityObject)))
						{
							SetEntity((CEntityObject*)pObj);
						}
					}
				}
				else
					SetEntity(NULL);
			}
			if (node->haveAttr("GraphEntity"))
			{
				const char* sEntity = node->getAttr("GraphEntity");
				int index = atoi(node->getAttr("GraphEntity"));
				if (index == 0)
				{
					SetFlag(EHYPER_NODE_GRAPH_ENTITY, true);
					if (bIsAIOrCustomAction == false)
						SetEntity(GetDefaultEntity());
				}
				else
					SetFlag(EHYPER_NODE_GRAPH_ENTITY2, true);
			}
		}
	}
	else // saving
	{
		if (CheckFlag(EHYPER_NODE_GRAPH_ENTITY) || CheckFlag(EHYPER_NODE_GRAPH_ENTITY2))
		{
			int index = 0;
			if (CheckFlag(EHYPER_NODE_GRAPH_ENTITY2))
				index = 1;
			node->setAttr("GraphEntity", index);
		}
		else
		{
			// If we are exporting the level we should pick the unique entity guid no matter wether we are part of a prefab or not
			// During level export they are getting expanded as normal objects. Runtime prefabs on the other hand are not.
			const bool isLevelExportingInProgress = GetIEditorImpl()->GetObjectManager() ? GetIEditorImpl()->GetObjectManager()->IsExportingLevelInprogress() : false;
			if (m_entityGuid != CryGUID::Null())
			{
				if (m_pEntity && m_pEntity->IsPartOfPrefab() && !isLevelExportingInProgress)
				{
					CryGUID guidInPrefab = m_pEntity->GetIdInPrefab();

					node->setAttr("EntityGUID", guidInPrefab);
				}
				else
				{
					node->setAttr("EntityGUID", m_entityGuid);
				}
			}
		}
	}

}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx)
{
	// fix up all references
	CBaseObject* pOld = GetIEditorImpl()->GetObjectManager()->FindObject(m_entityGuid);
	CBaseObject* pNew = ctx.FindClone(pOld);
	if (pNew && pNew->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		SetEntity((CEntityObject*)pNew);
	}
}

//////////////////////////////////////////////////////////////////////////
Gdiplus::Color CFlowNode::GetCategoryColor() const
{
	// This color coding is used to identify critical and obsolete nodes in the flowgraph editor. [Jan Mueller]
	int32 category = GetCategory();
	switch (category)
	{
	case EFLN_APPROVED:
		return TITLE_COLOR_APPROVED;
	//return CHyperNode::GetCategoryColor();
	case EFLN_ADVANCED:
		return TITLE_COLOR_ADVANCED;
	case EFLN_DEBUG:
		return TITLE_COLOR_DEBUG;
	case EFLN_OBSOLETE:
		return TITLE_COLOR_OBSOLETE;
	default:
		return CHyperNode::GetCategoryColor();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CFlowNode::IsEntityValid() const
{
	return CheckFlag(EHYPER_NODE_ENTITY_VALID) && m_pEntity && !m_pEntity->CheckFlags(OBJFLAG_DELETED);
}

//////////////////////////////////////////////////////////////////////////
CString CFlowNode::GetEntityTitle() const
{
	if (CheckFlag(EHYPER_NODE_ENTITY))
	{
		// Check if entity port is connected.
		if (m_inputs.size() > 0 && m_inputs[0].nConnected != 0)
			return "<Input Entity>";
	}
	if (CheckFlag(EHYPER_NODE_GRAPH_ENTITY))
	{
		if (m_pGraph->GetAIAction() != 0)
			return "<User Entity>";
		else
			return "<Graph Entity>";
	}
	else if (CheckFlag(EHYPER_NODE_GRAPH_ENTITY2))
	{
		if (m_pGraph->GetAIAction() != 0)
			return "<Object Entity>";
		else
			return "<Graph Entity 2>";
	}

	CEntityObject* pEntity = GetEntity();
	if (pEntity)
	{
		if (pEntity->GetGroup())
		{
			CString name = "[";
			name += pEntity->GetGroup()->GetName();
			name += "] ";
			name += pEntity->GetName();
			return name;
		}
		else
			return pEntity->GetName().GetString();
	}
	return "Choose Entity";
}

//////////////////////////////////////////////////////////////////////////
CString CFlowNode::GetPortName(const CHyperNodePort& port)
{
	//IFlowGraph *pGraph = GetIFlowGraph();

	//const TFlowInputData *pInputValue = pGraph->GetInputValue( m_flowNodeId,port.nPortIndex );

	if (port.bInput)
	{
		CString text = port.pVar->GetHumanName();
		if (port.pVar->GetType() != IVariable::UNKNOWN)
		{
			if (!port.nConnected)
			{
				text = text + "=" + VarToValue(port.pVar);
			}
			else if (!IsPortActivationModified(&port))
			{
				text = text + "(" + VarToValue(port.pVar) + ")";
			}
		}
		return text;
	}
	return port.pVar->GetHumanName();
}

const char* CFlowNode::GetCategoryName() const
{
	const uint32 cat = GetCategory();
	switch (cat)
	{
	case EFLN_APPROVED:
		return _T("Release");
	case EFLN_ADVANCED:
		return _T("Advanced");
	case EFLN_DEBUG:
		return _T("Debug");
	//case EFLN_WIP:
	//	return _T("WIP");
	//case EFLN_LEGACY:
	//	return _T("Legacy");
	//case EFLN_NOCATEGORY:
	//	return _T("No Category");
	case EFLN_OBSOLETE:
		return _T("Obsolete");
	default:
		return _T("UNDEFINED!");
	}
}

//////////////////////////////////////////////////////////////////////////
CVarBlock* CFlowNode::GetInputsVarBlock()
{
	CVarBlock* pVarBlock = __super::GetInputsVarBlock();

#if 0 // commented out, because currently there is per-variable IGetCustomItems
	// which is set on node creation (in CFlowNode::Clone)
	for (int i = 0; i < pVarBlock->GetVarsCount(); ++i)
	{
		IVariable* pVar = pVarBlock->GetVariable(i);
		CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*>(pVar->GetUserData());
		if (pGetCustomItems != 0)
			pGetCustomItems->SetFlowNode(this);
	}
#endif

	return pVarBlock;
}

//////////////////////////////////////////////////////////////////////////
CString CFlowNode::GetTitle() const
{
	CString title;
	if (m_name.IsEmpty())
	{
		title = m_classname;

#if 0 // show full group:class
		{
			int p = title.Find(':');
			if (p >= 0)
				title = title.Mid(p + 1);
		}
#endif

#if 0
		// Hack: drop AI: from AI: nodes (does not look nice)
		{
			int p = title.Find("AI:");
			if (p >= 0)
				title = title.Mid(p + 3);
		}
#endif

	}
	else
		title = m_name + " (" + m_classname + ")";

	return title;
}

//////////////////////////////////////////////////////////////////////////
TFlowNodeId CFlowNode::GetTypeId() const
{
	if (GetFlowNodeId() == InvalidFlowNodeId || !GetIFlowGraph())
		return InvalidFlowNodeTypeId;

	IFlowNodeData* pData = GetIFlowGraph()->GetNodeData(GetFlowNodeId());
	if (pData)
		return pData->GetNodeTypeId();

	return InvalidFlowNodeTypeId;
}

