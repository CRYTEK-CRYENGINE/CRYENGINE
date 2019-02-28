// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HyperGraphManager.h"

#include "GameEngine.h"

#include "HyperGraph.h"
#include "HyperGraphNode.h"
#include "Nodes/CommentNode.h"
#include "Nodes/CommentBoxNode.h"
#include "Nodes/QuickSearchNode.h"
#include "Nodes/BlackBoxNode.h"
#include "Nodes/MissingNode.h"
#include "Controls/HyperGraphEditorWnd.h"

//////////////////////////////////////////////////////////////////////////
void CHyperGraphManager::Init()
{
	m_notifyListenersDisabled = false;
}

//////////////////////////////////////////////////////////////////////////
CHyperGraphDialog* CHyperGraphManager::OpenView(IHyperGraph* pGraph)
{
	CWnd* pWnd = GetIEditorImpl()->FindView("Flow Graph");
	if (!pWnd)
	{
		pWnd = GetIEditorImpl()->OpenView("Flow Graph");
	}
	if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CHyperGraphDialog)))
	{
		CHyperGraphDialog* pDlg = (CHyperGraphDialog*)pWnd;
		pDlg->SetGraph((CHyperGraph*)pGraph);
		return pDlg;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CHyperNode* CHyperGraphManager::CreateNode(CHyperGraph* pGraph, const char* sNodeClass, HyperNodeID nodeId, const Gdiplus::PointF& /* pos */, CBaseObject* pObj, bool bAllowMissing)
{
	// AlexL: ignore pos, as a SetPos would screw up Undo. And it will be set afterwards anyway
	CHyperNode* pNode = NULL;
	CHyperNode* pPrototype = stl::find_in_map(m_prototypes, sNodeClass, NULL);
	if (pPrototype)
	{
		pNode = pPrototype->Clone();
		pNode->m_id = nodeId;
		pNode->SetGraph(pGraph);
		pNode->Init();
		return pNode;
	}

	//////////////////////////////////////////////////////////////////////////
	// Hack for naming changes
	//////////////////////////////////////////////////////////////////////////
	string newtype;
	// Hack for Math: to Vec3: change.
	if (strncmp(sNodeClass, "Math:", 5) == 0)
		newtype = string("Vec3:") + (sNodeClass + 5);

	if (newtype.size() > 0)
	{
		pPrototype = stl::find_in_map(m_prototypes, newtype, NULL);
		if (pPrototype)
		{
			pNode = pPrototype->Clone();
			pNode->m_id = nodeId;
			pNode->SetGraph(pGraph);
			pNode->Init();
			return pNode;
		}
	}
	//////////////////////////////////////////////////////////////////////////

	if (0 == strcmp(sNodeClass, CCommentNode::GetClassType()))
	{
		pNode = new CCommentNode();
		pNode->m_id = nodeId;
		pNode->SetGraph(pGraph);
		pNode->Init();
		return pNode;
	}
	else if (0 == strcmp(sNodeClass, CCommentBoxNode::GetClassType()))
	{
		pNode = new CCommentBoxNode();
		pNode->m_id = nodeId;
		pNode->SetGraph(pGraph);
		pNode->Init();
		return pNode;
	}
	else if (0 == strcmp(sNodeClass, CBlackBoxNode::GetClassType()))
	{
		pNode = new CBlackBoxNode();
		pNode->m_id = nodeId;
		pNode->SetGraph(pGraph);
		pNode->Init();
		return pNode;
	}
	else if (0 == strcmp(sNodeClass, CQuickSearchNode::GetClassType()))
	{
		pNode = new CQuickSearchNode();
		pNode->m_id = nodeId;
		pNode->SetGraph(pGraph);
		pNode->Init();
		return pNode;
	}
	else if (bAllowMissing)
	{
		gEnv->pLog->LogError("Missing Node: %s, Referenced in FlowGraph %s", sNodeClass, pGraph->GetName());
		pNode = new CMissingNode(sNodeClass);
		pNode->m_id = nodeId;
		pNode->SetGraph(pGraph);
		return pNode;
	}

	gEnv->pLog->LogError("Node of class '%s' doesn't exist -> Node creation failed", sNodeClass);
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphManager::GetPrototypes(std::vector<CString>& prototypes, bool bForUI, NodeFilterFunctor filterFunc)
{
	prototypes.clear();
	for (NodesPrototypesMap::iterator it = m_prototypes.begin(); it != m_prototypes.end(); ++it)
	{
		CHyperNode* pNode = it->second;
		if (filterFunc && filterFunc(pNode) == false)
			continue;
		if (bForUI && (pNode->CheckFlag(EHYPER_NODE_HIDE_UI)))
			continue;
		prototypes.push_back(pNode->GetClassName());
	}
#if 0
	if (bForUI)
	{
		prototypes.push_back(CCommentNode::GetClassType());
		prototypes.push_back(CCommentBoxNode::GetClassType());
		prototypes.push_back(CBlackBoxNode::GetClassType());
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphManager::GetPrototypesEx(std::vector<THyperNodePtr>& prototypes, bool bForUI, NodeFilterFunctor filterFunc /* = NodeFilterFunctor( */)
{
	prototypes.clear();
	for (NodesPrototypesMap::iterator it = m_prototypes.begin(); it != m_prototypes.end(); ++it)
	{
		CHyperNode* pNode = it->second;
		if (filterFunc && filterFunc(pNode) == false)
			continue;
		if (bForUI && (pNode->CheckFlag(EHYPER_NODE_HIDE_UI)))
			continue;
		prototypes.push_back(it->second);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphManager::SendNotifyEvent(EHyperGraphEvent event, IHyperGraph* pGraph, IHyperNode* pNode)
{
	if (m_notifyListenersDisabled)
		return;

	Listeners::iterator next;
	for (Listeners::iterator it = m_listeners.begin(); it != m_listeners.end(); it = next)
	{
		next = it;
		next++;
		(*it)->OnHyperGraphManagerEvent(event, pGraph, pNode);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphManager::SetGUIControlsProcessEvents(bool active, bool refreshTreeCtrList)
{
	CWnd* pWnd = GetIEditorImpl()->FindView("Flow Graph");
	CHyperGraphDialog* pHGDlg = nullptr;
	if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CHyperGraphDialog)))
	{
		pHGDlg = static_cast<CHyperGraphDialog*>(pWnd);
		pHGDlg->SetIgnoreEvents(!active, refreshTreeCtrList);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphManager::SetCurrentViewedGraph(CHyperGraph* pGraph)
{
	CWnd* pWnd = GetIEditorImpl()->FindView("Flow Graph");
	CHyperGraphDialog* pHGDlg = nullptr;
	if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CHyperGraphDialog)))
	{
		pHGDlg = static_cast<CHyperGraphDialog*>(pWnd);
		pHGDlg->GetGraphView()->SetGraph(pGraph);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphManager::AddListener(IHyperGraphManagerListener* pListener)
{
	stl::push_back_unique(m_listeners, pListener);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphManager::RemoveListener(IHyperGraphManagerListener* pListener)
{
	stl::find_and_erase(m_listeners, pListener);
}

