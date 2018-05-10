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
	CHyperFlowGraph*        m_pGraph{ nullptr };
	CHyperGraphDialog*      m_pParent{ nullptr };
	CXTPTaskPanel*          m_pTaskPanel{ nullptr };
	CXTPTaskPanelGroup*     m_pGroup{ nullptr };
	CXTPTaskPanelGroupItem* m_pPropItem{ nullptr };
	CPropertyCtrl           m_graphProps;

	CSmartVariable<bool>    m_varEnabled;
	CSmartVariableEnum<int> m_varMultiPlayer;

	bool                    m_bUpdate{ false };
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
	CHyperFlowGraph*        m_pGraph{ nullptr };
	CHyperGraphDialog*      m_pParent{ nullptr };
	CXTPTaskPanel*          m_pTaskPanel{ nullptr };
	CXTPTaskPanelGroup*     m_pGroup{ nullptr };
	CXTPTaskPanelItem*      m_pInfoItem{ nullptr };
	CXTPTaskPanelGroupItem* m_pDescTitleHeaderItem{ nullptr };
	CXTPTaskPanelGroupItem* m_pDescItem{ nullptr };
	CColorCtrl<CEdit>       m_nodeDescription;
};

