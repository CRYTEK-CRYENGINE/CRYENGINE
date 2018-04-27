// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "MissionSelectDialog.h"
#include "Mission.h"
#include "cryeditdoc.h"

/////////////////////////////////////////////////////////////////////////////
// CMissionSelectDialog dialog

CMissionSelectDialog::CMissionSelectDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CMissionSelectDialog::IDD, pParent)
{
	//{{AFX_DATA_INIT(CMissionSelectDialog)
	m_description = _T("");
	m_selected = _T("");
	//}}AFX_DATA_INIT
}

void CMissionSelectDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMissionSelectDialog)
	DDX_Control(pDX, IDC_MISSIONS, m_missions);
	DDX_Text(pDX, IDC_DESCRIPTION, m_description);
	DDX_LBString(pDX, IDC_MISSIONS, m_selected);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CMissionSelectDialog, CDialog)
//{{AFX_MSG_MAP(CMissionSelectDialog)
ON_LBN_SELCHANGE(IDC_MISSIONS, OnSelectMission)
ON_LBN_DBLCLK(IDC_MISSIONS, OnDblclkMissions)
ON_EN_UPDATE(IDC_DESCRIPTION, OnUpdateDescription)
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMissionSelectDialog message handlers

void CMissionSelectDialog::OnOK()
{
	CCryEditDoc* doc = GetIEditorImpl()->GetDocument();
	for (int i = 0; i < doc->GetMissionCount(); i++)
	{
		CMission* m = doc->GetMission(i);
		m->SetDescription(m_descriptions[i]);
	}

	CDialog::OnOK();
}

void CMissionSelectDialog::OnCancel()
{
	CDialog::OnCancel();
}

BOOL CMissionSelectDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Init missions.
	m_missions.ResetContent();
	CCryEditDoc* doc = GetIEditorImpl()->GetDocument();

	m_descriptions.resize(doc->GetMissionCount());
	for (int i = 0; i < doc->GetMissionCount(); i++)
	{
		CMission* m = doc->GetMission(i);
		m_missions.AddString(m->GetName());
		m_descriptions[i] = m->GetDescription();
	}

	if (doc->GetMissionCount() > 0)
	{
		// Select first mission.
		m_missions.SetCurSel(0);
		OnSelectMission();
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CMissionSelectDialog::OnSelectMission()
{
	// TODO: Add your control notification handler code here
	int sel = m_missions.GetCurSel();
	if (sel != LB_ERR)
	{
		UpdateData(TRUE);
		m_description = m_descriptions[sel];
		UpdateData(FALSE);
	}
}

void CMissionSelectDialog::OnDblclkMissions()
{
	OnOK();
}

void CMissionSelectDialog::OnUpdateDescription()
{
	int sel = m_missions.GetCurSel();
	if (sel != LB_ERR)
	{
		UpdateData(TRUE);
		m_descriptions[sel] = m_description;
	}
}

