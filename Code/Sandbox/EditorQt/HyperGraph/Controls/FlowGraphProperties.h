// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Controls/PropertyCtrl.h"
#include "Controls/ColorCtrl.h"
#include "HyperGraph/HyperGraph.h"

class CHyperGraphDialog;
class CXTPTaskPanel;
class CHyperGraph;
class CHyperFlowGraph;

class CHGGraphPropsPanel : public CWnd
{
public:
	DECLARE_DYNAMIC(CHGGraphPropsPanel)

	CHGGraphPropsPanel(CHyperGraphDialog* pParent, CXTPTaskPanel* pPanel);

	BOOL Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID);

	void SetGraph(CHyperGraph* pGraph);
	void UpdateGraphProperties(CHyperGraph* pGraph);

protected:
	void Init(int nID);
	void FillProps();
	void ResizeProps(bool bHide = false);
	void OnVarChange(IVariable* pVar);

protected:
	CHyperFlowGraph*        m_pGraph;
	CHyperGraphDialog*      m_pParent;
	CXTPTaskPanel*          m_pTaskPanel;
	CXTPTaskPanelGroup*     m_pGroup;
	CXTPTaskPanelGroupItem* m_pPropItem;
	CPropertyCtrl           m_graphProps;

	CSmartVariable<bool>    m_varEnabled;
	CSmartVariableEnum<int> m_varMultiPlayer;

	bool                    m_bUpdate;
};

////////////////////////////////////////////////////////////////////////////////

class CHGSelNodeInfoPanel : public CWnd
{
public:
	DECLARE_DYNAMIC(CHGSelNodeInfoPanel)

	CHGSelNodeInfoPanel(CHyperGraphDialog* pParent, CXTPTaskPanel* pPanel);
	virtual ~CHGSelNodeInfoPanel();

	BOOL Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID);

	void SetNodeInfo(std::vector<CHyperNode*>& nodes);

protected:
	void Init(int nID);

protected:
	CHyperFlowGraph*        m_pGraph;
	CHyperGraphDialog*      m_pParent;
	CXTPTaskPanel*          m_pTaskPanel;
	CXTPTaskPanelGroup*     m_pGroup;
	CXTPTaskPanelItem*      m_pInfoItem;
	CXTPTaskPanelGroupItem* m_pDescTitleHeaderItem;
	CXTPTaskPanelGroupItem* m_pDescItem;
	CColorCtrl<CEdit>       m_nodeDescription;
};

