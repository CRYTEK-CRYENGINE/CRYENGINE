// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CMultiLineStringDlg : public CXTResizeDialog
{
	// Construction
public:
	CMultiLineStringDlg(const char* title = NULL, CWnd* pParent = NULL);   // standard constructor

																		   // Dialog Data
																		   //{{AFX_DATA(CStringDlg)
	enum { IDD = IDD_STRING_MULTI };
	CString m_strString;
	//}}AFX_DATA

	void		SetString(const char* str) { m_strString = str; };
	const char*	GetString() { return m_strString; };

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CStringDlg)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
														//}}AFX_VIRTUAL

														// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CStringDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	CString m_title;
};

