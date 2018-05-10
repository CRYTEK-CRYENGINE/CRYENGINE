// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SourceControlAddDlg.h"

/////////////////////////////////////////////////////////////////////////////
// CSourceControlAddDlg dialog

CSourceControlAddDlg::CSourceControlAddDlg(const string& sFilename, CWnd* pParent /*=NULL*/)
	: CDialog(CSourceControlAddDlg::IDD, pParent), m_result(NO_OP), m_sFilename(sFilename)
{
	//{{AFX_DATA_INIT(CSourceControlAddDlg)
	// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

void CSourceControlAddDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSourceControlAddDlg)
	// NOTE: the ClassWizard will add DDX and DDV calls here
	//DDX_Check(pDX, IDC_CHECK1, m_bQuality);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CSourceControlAddDlg, CDialog)
ON_BN_CLICKED(IDOK, OnBnClickedOk)
ON_BN_CLICKED(ID_ADD_DEFAULT, OnBnClickedAddDefault)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSourceControlAddDlg message handlers

BOOL CSourceControlAddDlg::OnInitDialog()
{
	CWnd* pEditFilename = GetDlgItem(IDC_FILENAME_SCADD);
	if (pEditFilename)
		pEditFilename->SetWindowText(m_sFilename);
	return TRUE;
}

void CSourceControlAddDlg::OnBnClickedOk()
{
	CWnd* pWnd = GetDlgItem(IDC_EDIT1);
	if (pWnd)
	{
		pWnd->GetWindowText(m_sDesc);
		if (m_sDesc.GetLength() > 0)
		{
			m_result = ADD_AND_SUBMIT;
			OnOK();
		}
	}
}

void CSourceControlAddDlg::OnBnClickedAddDefault()
{
	m_result = ADD_ONLY;
	OnOK();
}

