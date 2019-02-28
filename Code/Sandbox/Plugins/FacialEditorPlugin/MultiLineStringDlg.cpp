// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "MultiLineStringDlg.h"

CMultiLineStringDlg::CMultiLineStringDlg(const char* title, CWnd* pParent)
	: CXTResizeDialog(CMultiLineStringDlg::IDD, pParent)
{
	m_strString = _T("");

	if (title)
		m_title = title;
}

//////////////////////////////////////////////////////////////////////////
void CMultiLineStringDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Text(pDX, IDC_STRING, m_strString);
}

BEGIN_MESSAGE_MAP(CMultiLineStringDlg, CXTResizeDialog)
	//{{AFX_MSG_MAP(CMultiLineStringDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMultiLineStringDlg message handlers
BOOL CMultiLineStringDlg::OnInitDialog()
{
	BOOL res = __super::OnInitDialog();

	if (res)
	{
		if (!m_title.IsEmpty())
			SetWindowText(m_title);

		SetResize(IDOK, SZ_REPOS(1));
		SetResize(IDCANCEL, SZ_REPOS(1));

		SetResize(IDC_STATIC, CXTResizeRect(0, 1, 1, 1));
		SetResize(IDC_STRING, SZ_RESIZE(1));
	}

	return res;  // return TRUE unless you set the focus to a control
}

