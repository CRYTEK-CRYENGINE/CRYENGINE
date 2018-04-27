// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IAgent.h>
#include "AI/AIManager.h"
#include "HyperGraph/FlowGraphManager.h"
#include "HyperGraph/FlowGraph.h"

#include "SmartObjectActionDialog.h"

// CSmartObjectActionDialog dialog

IMPLEMENT_DYNAMIC(CSmartObjectActionDialog, CDialog)
CSmartObjectActionDialog::CSmartObjectActionDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CSmartObjectActionDialog::IDD, pParent)
{
}

CSmartObjectActionDialog::~CSmartObjectActionDialog()
{
}

void CSmartObjectActionDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_ANCHORS, m_wndActionList);
}

BEGIN_MESSAGE_MAP(CSmartObjectActionDialog, CDialog)
ON_LBN_DBLCLK(IDC_ANCHORS, OnLbnDblClk)
ON_LBN_SELCHANGE(IDC_ANCHORS, OnLbnSelchangeAction)
ON_BN_CLICKED(IDC_NEW, OnNewBtn)
ON_BN_CLICKED(IDEDIT, OnEditBtn)
ON_BN_CLICKED(IDREFRESH, OnRefreshBtn)
END_MESSAGE_MAP()

// CSmartObjectStateDialog message handlers

void CSmartObjectActionDialog::OnNewBtn()
{
	string filename;
	if (GetIEditor()->GetAIManager()->NewAction(filename))
	{
		m_sSOAction = PathUtil::GetFileName((const char*)filename);
		IAIAction* pAction = gEnv->pAISystem->GetAIActionManager()->GetAIAction(m_sSOAction);
		if (pAction)
		{
			CFlowGraphManager* pManager = GetIEditor()->GetFlowGraphManager();
			CHyperFlowGraph* pFlowGraph = pManager->FindGraphForAction(pAction);
			assert(pFlowGraph);
			if (pFlowGraph)
				pManager->OpenView(pFlowGraph);

			EndDialog(IDOK);
		}
	}
}

void CSmartObjectActionDialog::OnEditBtn()
{
	//	EndDialog( IDCANCEL );

	IAIAction* pAction = gEnv->pAISystem->GetAIActionManager()->GetAIAction(m_sSOAction);
	if (pAction)
	{
		CFlowGraphManager* pManager = GetIEditor()->GetFlowGraphManager();
		CHyperFlowGraph* pFlowGraph = pManager->FindGraphForAction(pAction);
		assert(pFlowGraph);
		if (pFlowGraph)
			pManager->OpenView(pFlowGraph);
	}
}

void CSmartObjectActionDialog::OnRefreshBtn()
{
	// add empty string item
	m_wndActionList.ResetContent();
	m_wndActionList.AddString("");

	IAIManager* pAIMgr = GetIEditor()->GetAIManager();
	ASSERT(pAIMgr);
	typedef std::vector<string> TSOActionsVec;
	TSOActionsVec vecSOActions;
	pAIMgr->GetSmartObjectActions(vecSOActions);
	for (TSOActionsVec::iterator it = vecSOActions.begin(); it != vecSOActions.end(); ++it)
		m_wndActionList.AddString((*it).GetString());
	m_wndActionList.SelectString(-1, m_sSOAction);
}

void CSmartObjectActionDialog::OnLbnDblClk()
{
	if (m_wndActionList.GetCurSel() >= 0)
		EndDialog(IDOK);
}

void CSmartObjectActionDialog::OnLbnSelchangeAction()
{
	SetDlgItemText(IDCANCEL, "Cancel");
	GetDlgItem(IDOK)->EnableWindow(TRUE);

	int nSel = m_wndActionList.GetCurSel();
	if (nSel == LB_ERR)
		return;
	m_wndActionList.GetText(nSel, m_sSOAction);
}

BOOL CSmartObjectActionDialog::OnInitDialog()
{
	CDialog::OnInitDialog();
	SetWindowText("AI Actions");
	SetDlgItemText(IDC_LISTCAPTION, "&Choose AI Action:");

	GetDlgItem(IDC_NEW)->EnableWindow(TRUE);
	GetDlgItem(IDEDIT)->EnableWindow(TRUE);
	GetDlgItem(IDDELETE)->EnableWindow(FALSE);
	GetDlgItem(IDREFRESH)->EnableWindow(TRUE);

	OnRefreshBtn();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

