// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SourceControlDescDlg.h"

/////////////////////////////////////////////////////////////////////////////
// CSourceControlDescDlg dialog

CSourceControlDescDlg::CSourceControlDescDlg(CWnd* pParent /*=NULL*/)
	: CDialog(IDD_SOURCECONTROL_DESC, pParent)
{
	//{{AFX_DATA_INIT(CSourceControlDescDlg)
	// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

void CSourceControlDescDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSourceControlDescDlg)
	// NOTE: the ClassWizard will add DDX and DDV calls here
	//DDX_Check(pDX, IDC_CHECK1, m_bQuality);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CSourceControlDescDlg, CDialog)
ON_BN_CLICKED(IDOK, OnBnClickedOk)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSourceControlDescDlg message handlers

void CSourceControlDescDlg::OnBnClickedOk()
{
	CWnd* pWnd = GetDlgItem(IDC_EDIT1);
	if (pWnd)
	{
		pWnd->GetWindowText(m_sDesc);
		if (m_sDesc.GetLength() > 0)
			OnOK();
	}
}

