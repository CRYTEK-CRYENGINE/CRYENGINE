// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryAISystem/IAISystem.h>
#include "ItemDescriptionDlg.h"
#include "AI/AIManager.h"

#include "SmartObjectsEditorDialog.h"
#include "SmartObjectHelperDialog.h"

// CSmartObjectHelperDialog dialog

IMPLEMENT_DYNAMIC(CSmartObjectHelperDialog, CDialog)
CSmartObjectHelperDialog::CSmartObjectHelperDialog(CWnd* pParent /*=NULL*/, bool bAllowEmpty /*=true*/, bool bFromTemplate /*=false*/, bool bShowAuto /*=false*/)
	: CDialog(CSmartObjectHelperDialog::IDD, pParent)
	, m_bAllowEmpty(bAllowEmpty)
	, m_bFromTemplate(bFromTemplate)
	, m_bShowAuto(bShowAuto)
{
}

CSmartObjectHelperDialog::~CSmartObjectHelperDialog()
{
}

void CSmartObjectHelperDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_ANCHORS, m_wndHelperList);
}

BEGIN_MESSAGE_MAP(CSmartObjectHelperDialog, CDialog)
ON_LBN_DBLCLK(IDC_ANCHORS, OnLbnDblClk)
ON_LBN_SELCHANGE(IDC_ANCHORS, OnLbnSelchangeHelper)
ON_BN_CLICKED(IDC_NEW, OnNewBtn)
ON_BN_CLICKED(IDEDIT, OnEditBtn)
ON_BN_CLICKED(IDDELETE, OnDeleteBtn)
ON_BN_CLICKED(IDREFRESH, OnRefreshBtn)
END_MESSAGE_MAP()

// CSmartObjectHelperDialog message handlers

void CSmartObjectHelperDialog::OnDeleteBtn()
{
}

void CSmartObjectHelperDialog::OnNewBtn()
{
	/*
	   CString filename;
	   if ( GetIEditor()->GetAI()->NewAction(filename) )
	   {
	    m_sSOAction = PathUtil::GetFileName((const char*)filename);
	    IAIAction* pAction = gEnv->pAISystem->GetAIAction( m_sSOAction );
	    if ( pAction )
	    {
	      CFlowGraphManager* pManager = GetIEditor()->GetFlowGraphManager();
	      CHyperFlowGraph* pFlowGraph = pManager->FindGraphForAction( pAction );
	      assert( pFlowGraph );
	      if ( pFlowGraph )
	        pManager->OpenView( pFlowGraph );

	      EndDialog( IDOK );
	    }
	   }
	 */
}

void CSmartObjectHelperDialog::OnEditBtn()
{
	if (m_sSOHelper.IsEmpty())
		return;

	CSOLibrary::VectorHelperData::iterator itHelper = CSOLibrary::FindHelper(m_sClassName, m_sSOHelper);
	if (itHelper != CSOLibrary::GetHelpers().end())
	{
		CItemDescriptionDlg dlg(this, false);
		dlg.m_sCaption = "Edit Helper";
		dlg.m_sItemCaption = "Helper &name:";
		dlg.m_sItemEdit = itHelper->name;
		dlg.m_sDescription = itHelper->description;
		if (dlg.DoModal() == IDOK)
		{
			if (CSOLibrary::StartEditing())
			{
				itHelper->description = dlg.m_sDescription;
				SetDlgItemText(IDC_DESCRIPTION, itHelper->description);
			}
		}
	}
}

void CSmartObjectHelperDialog::OnRefreshBtn()
{
	// add empty string item
	m_wndHelperList.ResetContent();
	if (m_bAllowEmpty)
		m_wndHelperList.AddString("");

	if (m_bShowAuto)
	{
		m_wndHelperList.AddString("<Auto>");
		m_wndHelperList.AddString("<AutoInverse>");
	}

	//CAIManager* pAIMgr = GetIEditor()->GetAI();
	//ASSERT( pAIMgr );

	CSOLibrary::VectorHelperData::iterator it, itEnd = CSOLibrary::HelpersUpperBound(m_sClassName);
	for (it = CSOLibrary::HelpersLowerBound(m_sClassName); it != itEnd; ++it)
		m_wndHelperList.AddString(it->name);

	if (m_bFromTemplate)
	{
		CSOLibrary::VectorClassData::iterator itClass = CSOLibrary::FindClass(m_sClassName);
		if (itClass != CSOLibrary::GetClasses().end() && itClass->pClassTemplateData)
		{
			const CSOLibrary::CClassTemplateData::TTemplateHelpers& templateHelpers = itClass->pClassTemplateData->helpers;
			CSOLibrary::CClassTemplateData::TTemplateHelpers::const_iterator itHelpers, itHelpersEnd = templateHelpers.end();
			for (itHelpers = templateHelpers.begin(); itHelpers != itHelpersEnd; ++itHelpers)
				m_wndHelperList.AddString(itHelpers->name);
		}
	}

	m_wndHelperList.SelectString(-1, m_sSOHelper);
	GetDlgItem(IDEDIT)->EnableWindow(CSOLibrary::FindHelper(m_sClassName, m_sSOHelper) != CSOLibrary::GetHelpers().end());
	UpdateDescription();
}

void CSmartObjectHelperDialog::OnLbnDblClk()
{
	if (m_wndHelperList.GetCurSel() >= 0)
		EndDialog(IDOK);
}

void CSmartObjectHelperDialog::OnLbnSelchangeHelper()
{
	SetDlgItemText(IDCANCEL, "Cancel");
	GetDlgItem(IDOK)->EnableWindow(TRUE);

	int nSel = m_wndHelperList.GetCurSel();
	if (nSel == LB_ERR)
	{
		m_sSOHelper.Empty();
		return;
	}
	m_wndHelperList.GetText(nSel, m_sSOHelper);
	GetDlgItem(IDEDIT)->EnableWindow(CSOLibrary::FindHelper(m_sClassName, m_sSOHelper) != CSOLibrary::GetHelpers().end());
	UpdateDescription();
}

void CSmartObjectHelperDialog::UpdateDescription()
{
	if (m_sSOHelper.IsEmpty())
		SetDlgItemText(IDC_DESCRIPTION, "");
	else if (m_sSOHelper == "<Auto>")
		SetDlgItemText(IDC_DESCRIPTION, "The helper gets extracted automatically from the chosen animation.");
	else if (m_sSOHelper == "<AutoInverse>")
		SetDlgItemText(IDC_DESCRIPTION, "The helper gets extracted automatically from the chosen animation, then rotated by 180 degrees.");
	else
	{
		CSOLibrary::VectorHelperData::iterator itHelper = CSOLibrary::FindHelper(m_sClassName, m_sSOHelper);
		if (itHelper != CSOLibrary::GetHelpers().end())
			SetDlgItemText(IDC_DESCRIPTION, itHelper->description);
		else
			SetDlgItemText(IDC_DESCRIPTION, "");
	}
}

BOOL CSmartObjectHelperDialog::OnInitDialog()
{
	CDialog::OnInitDialog();
	SetWindowText("Smart Object Helpers");
	SetDlgItemText(IDC_LISTCAPTION, "&Choose Smart Object Helper:");

	GetDlgItem(IDC_NEW)->EnableWindow(FALSE);
	GetDlgItem(IDEDIT)->EnableWindow(TRUE);
	GetDlgItem(IDDELETE)->EnableWindow(FALSE);
	GetDlgItem(IDREFRESH)->EnableWindow(FALSE);

	OnRefreshBtn();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

