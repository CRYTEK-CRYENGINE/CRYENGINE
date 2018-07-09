// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// CSmartObjectHelperDialog dialog

class CSmartObjectHelperDialog : public CDialog
{
	DECLARE_DYNAMIC(CSmartObjectHelperDialog)

private:
	CString  m_sClassName;
	CString  m_sSOHelper;
	CListBox m_wndHelperList;
	bool     m_bAllowEmpty;
	bool     m_bFromTemplate;
	bool     m_bShowAuto;

public:
	CSmartObjectHelperDialog(CWnd* pParent = NULL, bool bAllowEmpty = true, bool bFromTemplate = false, bool bShowAuto = false);
	virtual ~CSmartObjectHelperDialog();

	void    SetSOHelper(const CString& sClassName, const CString& sSOHelper) { m_sClassName = sClassName; m_sSOHelper = sSOHelper; }
	CString GetSOHelper()                                                    { return m_sSOHelper; };

	// Dialog Data
	enum { IDD = IDD_AIANCHORS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);      // DDX/DDV support

	DECLARE_MESSAGE_MAP()

	afx_msg void OnLbnDblClk();
	afx_msg void OnLbnSelchangeHelper();
	afx_msg void OnNewBtn();
	afx_msg void OnEditBtn();
	afx_msg void OnDeleteBtn();
	afx_msg void OnRefreshBtn();

	void         UpdateDescription();

public:
	virtual BOOL OnInitDialog();
};

