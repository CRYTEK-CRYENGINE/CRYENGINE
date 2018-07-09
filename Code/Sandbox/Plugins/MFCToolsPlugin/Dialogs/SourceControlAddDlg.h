// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#define IDD_SOURCECONTROL_ADD                                                                74
#define ID_ADD_DEFAULT                                                                       33909        
#define IDC_FILENAME_SCADD                                                                   2226
#define IDC_EDIT1                                                                            1738

/////////////////////////////////////////////////////////////////////////////
// CSourceControlAddDlg dialog

class PLUGIN_API CSourceControlAddDlg : public CDialog
{
public:

	enum ESCDialogResult
	{
		ADD_AND_SUBMIT,
		ADD_ONLY,
		NO_OP,
	};

	// Construction
	CSourceControlAddDlg(const string& sFilename, CWnd* pParent = NULL);   // standard constructor
	// Return the state of the dialog
	ESCDialogResult GetResult() const { return m_result; };

	// Dialog Data
	//{{AFX_DATA(CSourceControlAddDlg)
	enum { IDD = IDD_SOURCECONTROL_ADD };
	// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSourceControlAddDlg)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CSourceControlAddDlg)
	// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedAddDefault();
	virtual BOOL OnInitDialog();

public:

	CString m_sDesc;
	CString m_sFilename;

private:
	ESCDialogResult m_result;
};
