// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

/////////////////////////////////////////////////////////////////////////////
// CSourceControlDescDlg dialog

class PLUGIN_API CSourceControlDescDlg : public CDialog
{
	// Construction
public:
	CSourceControlDescDlg(CWnd* pParent = NULL);   // standard constructor

	// Dialog Data
	//{{AFX_DATA(CSourceControlDescDlg)
	
	// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSourceControlDescDlg)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CSourceControlDescDlg)
	// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	afx_msg void OnBnClickedOk();

public:

	CString m_sDesc;
};


