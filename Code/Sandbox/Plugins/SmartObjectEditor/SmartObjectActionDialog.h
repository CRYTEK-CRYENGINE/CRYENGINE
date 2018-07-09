// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// CSmartObjectActionDialog dialog

class CSmartObjectActionDialog : public CDialog
{
	DECLARE_DYNAMIC(CSmartObjectActionDialog)

private:
	CString  m_sSOAction;
	CListBox m_wndActionList;

public:
	CSmartObjectActionDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSmartObjectActionDialog();

	void    SetSOAction(const CString& sSOAction) { m_sSOAction = sSOAction; }
	CString GetSOAction()                         { return m_sSOAction; };

	// Dialog Data
	enum { IDD = IDD_AIANCHORS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

	afx_msg void OnLbnDblClk();
	afx_msg void OnLbnSelchangeAction();
	afx_msg void OnNewBtn();
	afx_msg void OnEditBtn();
	afx_msg void OnRefreshBtn();
public:
	virtual BOOL OnInitDialog();
};

