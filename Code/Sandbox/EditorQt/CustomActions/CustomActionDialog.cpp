// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "CustomActionDialog.h"

#include "HyperGraph/FlowGraphManager.h"
#include "HyperGraph/FlowGraph.h"

#include "CustomActions/CustomActionsEditorManager.h"
#include <CryAction/ICustomActions.h>
#include <CryGame/IGameFramework.h>

//////////////////////////////////////////////////////////////////////////
// Dialog shown for custom action property
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CCustomActionDialog, CDialog)
CCustomActionDialog::CCustomActionDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CCustomActionDialog::IDD, pParent)
{
}

CCustomActionDialog::~CCustomActionDialog()
{
}

//////////////////////////////////////////////////////////////////////////
void CCustomActionDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_ANCHORS, m_wndActionList);
}

//////////////////////////////////////////////////////////////////////////
bool CCustomActionDialog::OpenViewForCustomAction()
{
	if (!gEnv->pGameFramework)
		return false;

	ICustomActionManager* pCustomActionManager = gEnv->pGameFramework->GetICustomActionManager();
	CRY_ASSERT(pCustomActionManager != NULL);
	if (pCustomActionManager)
	{
		ICustomAction* pCustomAction = pCustomActionManager->GetCustomActionFromLibrary(m_customAction);
		if (pCustomAction)
		{
			CFlowGraphManager* pManager = GetIEditorImpl()->GetFlowGraphManager();
			CHyperFlowGraph* pFlowGraph = pManager->FindGraphForCustomAction(pCustomAction);
			assert(pFlowGraph);
			if (pFlowGraph)
			{
				pManager->OpenView(pFlowGraph);
				return true;
			}
		}
	}

	return false;
}

BEGIN_MESSAGE_MAP(CCustomActionDialog, CDialog)
ON_LBN_DBLCLK(IDC_ANCHORS, OnLbnDblClk)
ON_LBN_SELCHANGE(IDC_ANCHORS, OnLbnSelchangeAction)
ON_BN_CLICKED(IDC_NEW, OnNewBtn)
ON_BN_CLICKED(IDEDIT, OnEditBtn)
ON_BN_CLICKED(IDREFRESH, OnRefreshBtn)
END_MESSAGE_MAP()

// CCustomActionDialog message handlers

//////////////////////////////////////////////////////////////////////////
void CCustomActionDialog::OnNewBtn()
{
	CString filename;

	if (GetIEditorImpl()->GetCustomActionManager()->NewCustomAction(filename))
	{
		m_customAction = PathUtil::GetFileName((const char*)filename);

		bool bResult = OpenViewForCustomAction();
		if (bResult)
		{
			EndDialog(IDOK);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCustomActionDialog::OnEditBtn()
{
	//	EndDialog( IDCANCEL );

	OpenViewForCustomAction();
}

//////////////////////////////////////////////////////////////////////////
void CCustomActionDialog::OnRefreshBtn()
{
	// add empty string item
	m_wndActionList.ResetContent();
	m_wndActionList.AddString("");

	typedef std::vector<CString> TCustomActionsVec;
	TCustomActionsVec vecCustomActions;

	CCustomActionsEditorManager* pCustomActionsManager = GetIEditorImpl()->GetCustomActionManager();
	CRY_ASSERT(pCustomActionsManager != NULL);
	if (pCustomActionsManager)
	{
		pCustomActionsManager->GetCustomActions(vecCustomActions);
	}

	for (TCustomActionsVec::iterator it = vecCustomActions.begin(); it != vecCustomActions.end(); ++it)
		m_wndActionList.AddString(*it);
	m_wndActionList.SelectString(-1, m_customAction);
}

//////////////////////////////////////////////////////////////////////////
void CCustomActionDialog::OnLbnDblClk()
{
	if (m_wndActionList.GetCurSel() >= 0)
		EndDialog(IDOK);
}

//////////////////////////////////////////////////////////////////////////
void CCustomActionDialog::OnLbnSelchangeAction()
{
	SetDlgItemText(IDCANCEL, "Cancel");
	GetDlgItem(IDOK)->EnableWindow(TRUE);

	int nSel = m_wndActionList.GetCurSel();
	if (nSel == LB_ERR)
		return;
	m_wndActionList.GetText(nSel, m_customAction);
}

//////////////////////////////////////////////////////////////////////////
BOOL CCustomActionDialog::OnInitDialog()
{
	CDialog::OnInitDialog();
	SetWindowText("Custom Actions");
	SetDlgItemText(IDC_LISTCAPTION, "&Choose Custom Action:");

	GetDlgItem(IDC_NEW)->EnableWindow(TRUE);
	GetDlgItem(IDEDIT)->EnableWindow(TRUE);
	GetDlgItem(IDDELETE)->EnableWindow(FALSE);
	GetDlgItem(IDREFRESH)->EnableWindow(TRUE);

	OnRefreshBtn();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

