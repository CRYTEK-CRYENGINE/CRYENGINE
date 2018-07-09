// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryEntitySystem/IEntity.h>
#include <CryAISystem/IAISystem.h>
#include "ItemDescriptionDlg.h"
//#include "AI/AIManager.h"
#include "SmartObjectsEditorDialog.h"

#include "SmartObjectEventDialog.h"
#include "Controls/QuestionDialog.h"

// CSmartObjectEventDialog dialog

IMPLEMENT_DYNAMIC(CSmartObjectEventDialog, CDialog)
CSmartObjectEventDialog::CSmartObjectEventDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CSmartObjectEventDialog::IDD, pParent)
{
}

CSmartObjectEventDialog::~CSmartObjectEventDialog()
{
}

void CSmartObjectEventDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_ANCHORS, m_wndEventList);
}

BEGIN_MESSAGE_MAP(CSmartObjectEventDialog, CDialog)
ON_LBN_DBLCLK(IDC_ANCHORS, OnLbnDblClk)
ON_LBN_SELCHANGE(IDC_ANCHORS, OnLbnSelchangeEvent)
ON_BN_CLICKED(IDC_NEW, OnNewBtn)
ON_BN_CLICKED(IDEDIT, OnEditBtn)
ON_BN_CLICKED(IDDELETE, OnDeleteBtn)
ON_BN_CLICKED(IDREFRESH, OnRefreshBtn)
END_MESSAGE_MAP()

// CSmartObjectEventDialog message handlers

void CSmartObjectEventDialog::OnDeleteBtn()
{
}

void CSmartObjectEventDialog::OnNewBtn()
{
	CItemDescriptionDlg dlg;
	dlg.m_sCaption = "New Event";
	dlg.m_sItemCaption = "Event &name:";
	dlg.m_sDescription = "";
	if (dlg.DoModal() == IDOK)
	{
		int index = m_wndEventList.FindString(-1, dlg.m_sItemEdit);
		if (index >= 0)
		{
			m_sSOEvent = dlg.m_sItemEdit;
			m_wndEventList.SelectString(-1, m_sSOEvent);

			CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("Entered event name already exists and might be used for something else...\n\nThe event will be not created!"));
		}
		else if (CSOLibrary::StartEditing())
		{
			m_sSOEvent = dlg.m_sItemEdit;
			m_wndEventList.AddString(m_sSOEvent);
			m_wndEventList.SelectString(-1, m_sSOEvent);
			SetDlgItemText(IDC_DESCRIPTION, dlg.m_sDescription);
			CSOLibrary::AddEvent(m_sSOEvent, dlg.m_sDescription);
		}

		SetDlgItemText(IDCANCEL, "Cancel");
		GetDlgItem(IDOK)->EnableWindow(TRUE);
	}
}

void CSmartObjectEventDialog::OnEditBtn()
{
	if (m_sSOEvent.IsEmpty())
		return;

	CSOLibrary::VectorEventData::iterator it = CSOLibrary::FindEvent(m_sSOEvent);
	if (it == CSOLibrary::GetEvents().end())
		return;

	CItemDescriptionDlg dlg(this, false);
	dlg.m_sCaption = "Edit Event";
	dlg.m_sItemCaption = "Event &name:";
	dlg.m_sItemEdit = m_sSOEvent;
	dlg.m_sDescription = it->description;
	if (dlg.DoModal() == IDOK)
	{
		if (CSOLibrary::StartEditing())
		{
			it->description = dlg.m_sDescription;
			SetDlgItemText(IDC_DESCRIPTION, dlg.m_sDescription);
		}
	}
}

void CSmartObjectEventDialog::OnRefreshBtn()
{
	// add empty string item
	m_wndEventList.ResetContent();
	m_wndEventList.AddString("");

	//CAIManager* pAIMgr = GetIEditor()->GetAI();
	//ASSERT( pAIMgr );

	CSOLibrary::VectorEventData::iterator it, itEnd = CSOLibrary::GetEvents().end();
	for (it = CSOLibrary::GetEvents().begin(); it != itEnd; ++it)
		m_wndEventList.AddString(it->name);

	m_wndEventList.SelectString(-1, m_sSOEvent);
	UpdateDescription();
}

void CSmartObjectEventDialog::OnLbnDblClk()
{
	if (m_wndEventList.GetCurSel() >= 0)
		EndDialog(IDOK);
}

void CSmartObjectEventDialog::OnLbnSelchangeEvent()
{
	SetDlgItemText(IDCANCEL, "Cancel");
	GetDlgItem(IDOK)->EnableWindow(TRUE);

	int nSel = m_wndEventList.GetCurSel();
	if (nSel == LB_ERR)
	{
		m_sSOEvent.Empty();
		return;
	}
	m_wndEventList.GetText(nSel, m_sSOEvent);
	UpdateDescription();
}

void CSmartObjectEventDialog::UpdateDescription()
{
	if (m_sSOEvent.IsEmpty())
		SetDlgItemText(IDC_DESCRIPTION, "");
	else
	{
		CSOLibrary::VectorEventData::iterator it = CSOLibrary::FindEvent(m_sSOEvent);
		if (it != CSOLibrary::GetEvents().end())
			SetDlgItemText(IDC_DESCRIPTION, it->description);
		else
			SetDlgItemText(IDC_DESCRIPTION, "");
	}
}

BOOL CSmartObjectEventDialog::OnInitDialog()
{
	CDialog::OnInitDialog();
	SetWindowText("Smart Object Events");
	SetDlgItemText(IDC_LISTCAPTION, "&Choose Smart Object Event:");

	GetDlgItem(IDC_NEW)->EnableWindow(TRUE);
	GetDlgItem(IDEDIT)->EnableWindow(TRUE);
	GetDlgItem(IDDELETE)->EnableWindow(FALSE);
	GetDlgItem(IDREFRESH)->EnableWindow(FALSE);

	OnRefreshBtn();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

