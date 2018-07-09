// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(AFX_MISSIONSELECTDIALOG_H__5F648DA5_8C4C_4F21_833A_16B5A276C507__INCLUDED_)
#define AFX_MISSIONSELECTDIALOG_H__5F648DA5_8C4C_4F21_833A_16B5A276C507__INCLUDED_

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000
// MissionSelectDialog.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CMissionSelectDialog dialog

class CMissionSelectDialog : public CDialog
{
	// Construction
public:
	CMissionSelectDialog(CWnd* pParent = NULL);   // standard constructor

	// Dialog Data
	//{{AFX_DATA(CMissionSelectDialog)
	enum { IDD = IDD_MISSIONS };
	CListBox m_missions;
	CString  m_description;
	CString  m_selected;
	//}}AFX_DATA

	CString GetSelected() { return m_selected; }

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMissionSelectDialog)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CMissionSelectDialog)
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();
	afx_msg void OnSelectMission();
	afx_msg void OnDblclkMissions();
	afx_msg void OnUpdateDescription();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	std::vector<CString> m_descriptions;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MISSIONSELECTDIALOG_H__5F648DA5_8C4C_4F21_833A_16B5A276C507__INCLUDED_)

