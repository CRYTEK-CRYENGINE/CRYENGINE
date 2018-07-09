// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowGraphProperties.h"

#include "HyperGraph/FlowGraph.h"
#include "HyperGraphEditorWnd.h"

#include "Controls/SharedFonts.h"

#define IDC_NODE_INFO        4
#define IDC_NODE_DESCRIPTION 5
#define IDC_GRAPH_PROPERTIES 6

IMPLEMENT_DYNAMIC(CHGGraphPropsPanel, CWnd)

CHGGraphPropsPanel::CHGGraphPropsPanel(CHyperGraphDialog* pParent, CXTPTaskPanel* pPanel)
	: m_pParent(pParent),
	m_pTaskPanel(pPanel),
	m_pGraph(nullptr),
	m_bUpdate(false)
{
}

BOOL CHGGraphPropsPanel::Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID)
{
	__super::Create(NULL, NULL, dwStyle, rc, pParentWnd, nID);
	Init(nID);
	return TRUE;
}

void CHGGraphPropsPanel::Init(int nID)
{
	m_pGroup = m_pTaskPanel->AddGroup(nID);
	m_pGroup->SetCaption(_T("Graph Properties"));
	m_pGroup->SetItemLayout(xtpTaskItemLayoutDefault);

	m_graphProps.Create(WS_CHILD | WS_VISIBLE, CRect(0, 0, 100, 100), this, IDC_GRAPH_PROPERTIES);
	m_graphProps.ModifyStyleEx(0, WS_EX_CLIENTEDGE);
	m_graphProps.SetParent(m_pTaskPanel);
	m_graphProps.SetItemHeight(16);
	m_pPropItem = m_pGroup->AddControlItem(m_graphProps.GetSafeHwnd());

	m_varEnabled->SetDescription(_T("Enable/Disable the FlowGraph"));

	m_varMultiPlayer->SetDescription(_T("MultiPlayer Option of the FlowGraph (Default: ServerOnly)"));
	m_varMultiPlayer->AddEnumItem("ServerOnly", (int) CHyperFlowGraph::eMPT_ServerOnly);
	m_varMultiPlayer->AddEnumItem("ClientOnly", (int) CHyperFlowGraph::eMPT_ClientOnly);
	m_varMultiPlayer->AddEnumItem("ClientServer", (int) CHyperFlowGraph::eMPT_ClientServer);

	ResizeProps(true);
	m_pTaskPanel->ExpandGroup(m_pGroup, TRUE);
}

void CHGGraphPropsPanel::SetGraph(CHyperGraph* pGraph)
{
	if (pGraph && pGraph->IsFlowGraph())
	{
		m_pGraph = static_cast<CHyperFlowGraph*>(pGraph);
	}
	else
	{
		m_pGraph = nullptr;
	}

	if (m_pGraph)
	{
		FillProps();
	}
	else
	{
		m_graphProps.RemoveAllItems();
		ResizeProps(true);
	}
}

void CHGGraphPropsPanel::FillProps()
{
	if (!m_pGraph)
		return;

	m_graphProps.EnableUpdateCallback(false);
	m_varEnabled = m_pGraph->IsEnabled();
	m_varMultiPlayer = m_pGraph->GetMPType();
	m_graphProps.EnableUpdateCallback(true);

	CVarBlockPtr pVB = new CVarBlock;
	if (m_pGraph->GetAIAction() == 0)
	{
		pVB->AddVariable(m_varEnabled, "Enabled");
		pVB->AddVariable(m_varMultiPlayer, "MultiPlayer Options");
	}

	m_graphProps.RemoveAllItems();
	m_graphProps.AddVarBlock(pVB);
	m_graphProps.SetUpdateCallback(functor(*this, &CHGGraphPropsPanel::OnVarChange));

	ResizeProps(pVB->IsEmpty());
}

void CHGGraphPropsPanel::ResizeProps(bool bHide)
{
	bool bWasExpanded = m_pTaskPanel->IsGroupExpanded(m_pGroup);
	m_pTaskPanel->ExpandGroup(m_pGroup, FALSE);

	CRect rc;
	m_pTaskPanel->GetClientRect(rc);
	// Resize to fit properties.
	int h = m_graphProps.GetVisibleHeight();
	m_graphProps.SetWindowPos(NULL, 0, 0, rc.right, h + 1 + 4, SWP_NOMOVE);
	m_pPropItem->SetControlHandle(m_graphProps.GetSafeHwnd());
	m_pPropItem->SetVisible(!bHide);
	m_pTaskPanel->Reposition(false);

	m_pTaskPanel->ExpandGroup(m_pGroup, bWasExpanded);
}

void CHGGraphPropsPanel::OnVarChange(IVariable* pVar)
{
	assert(m_pGraph != 0);
#if 0
	CString val;
	pVar->Get(val);
	CryLogAlways("Var %s changed to %s", pVar->GetName(), val.GetString());
#endif
	m_pGraph->SetEnabled(m_varEnabled);
	m_pGraph->SetMPType((CHyperFlowGraph::EMultiPlayerType) (int) m_varMultiPlayer);

	m_bUpdate = true;
	m_pParent->UpdateGraphProperties(m_pGraph);
	m_bUpdate = false;
}

void CHGGraphPropsPanel::UpdateGraphProperties(CHyperGraph* pGraph)
{
	if (m_bUpdate)
		return;

	if (pGraph && pGraph->IsFlowGraph())
	{
		CHyperFlowGraph* pFG = static_cast<CHyperFlowGraph*>(pGraph);
		if (pFG == m_pGraph)
		{
			FillProps();
		}
	}
}


////////////////////////////////////////////////////////////////////////////////


IMPLEMENT_DYNAMIC(CHGSelNodeInfoPanel, CWnd)

CHGSelNodeInfoPanel::CHGSelNodeInfoPanel(CHyperGraphDialog* pParent, CXTPTaskPanel* pPanel)
	: m_pParent(pParent)
	, m_pTaskPanel(pPanel)
{
}

CHGSelNodeInfoPanel::~CHGSelNodeInfoPanel()
{
}

BOOL CHGSelNodeInfoPanel::Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID)
{
	__super::Create(NULL, NULL, dwStyle, rc, pParentWnd, nID);
	Init(nID);
	return TRUE;
}

void CHGSelNodeInfoPanel::Init(int nID)
{
	m_pGroup = m_pTaskPanel->AddGroup(nID);
	m_pGroup->SetCaption(_T("Selected Node Info"));
	m_pGroup->SetItemLayout(xtpTaskItemLayoutDefault);

	m_pInfoItem = m_pGroup->AddLinkItem(IDC_NODE_INFO);
	m_pInfoItem->SetCaption("No Node selected.");
	m_pInfoItem->SetType(xtpTaskItemTypeText);

	m_pDescTitleHeaderItem = m_pGroup->AddTextItem(_T("Description:"));
	m_nodeDescription.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE, CRect(0, 0, 100, 50), this, IDC_NODE_DESCRIPTION);
	m_nodeDescription.SetFont(CFont::FromHandle((HFONT)SMFCFonts::GetInstance().hSystemFont));
	m_nodeDescription.SetParent(m_pTaskPanel);
	m_nodeDescription.SetReadOnly(true);
	m_pDescItem = m_pGroup->AddControlItem(m_nodeDescription.GetSafeHwnd());
	m_pDescItem->SetVisible(false);
	m_pDescTitleHeaderItem->SetVisible(false);

	m_pTaskPanel->ExpandGroup(m_pGroup, true);
}

void CHGSelNodeInfoPanel::SetNodeInfo(std::vector<CHyperNode*>& nodes)
{
	m_pDescItem->SetVisible(false);
	m_pDescTitleHeaderItem->SetVisible(false);

	if (nodes.size() < 1)
	{
		m_pInfoItem->SetCaption("No Node selected.");
	}
	else if (nodes.size() > 1)
	{
		m_pInfoItem->SetCaption("Multiple Nodes selected.");
	}
	else // (nodes.size() == 1)
	{
		CHyperNode* pNode = nodes[0];
		if (pNode)
		{
			m_pInfoItem->SetCaption(pNode->GetInfoAsString());

			m_nodeDescription.SetWindowText(pNode->GetDescription());
			m_pDescItem->SetVisible(true);
			m_pDescTitleHeaderItem->SetVisible(true);
		}
		else
		{
			m_pInfoItem->SetCaption("No info available.");
		}
	}
}

