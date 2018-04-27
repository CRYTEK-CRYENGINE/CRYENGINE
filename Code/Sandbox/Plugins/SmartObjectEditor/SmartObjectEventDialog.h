// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// CSmartObjectEventDialog dialog

class CSmartObjectEventDialog : public CDialog
{
	DECLARE_DYNAMIC(CSmartObjectEventDialog)

private:
	CString  m_sSOEvent;
	CListBox m_wndEventList;

public:
	CSmartObjectEventDialog(CWnd* pParent = NULL);     // standard constructor
	virtual ~CSmartObjectEventDialog();

	void    SetSOEvent(const CString& sSOEvent) { m_sSOEvent = sSOEvent; }
	CString GetSOEvent()                        { return m_sSOEvent; };

	// Dialog Data
	enum { IDD = IDD_AIANCHORS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);      // DDX/DDV support

	DECLARE_MESSAGE_MAP()

	afx_msg void OnLbnDblClk();
	afx_msg void OnLbnSelchangeEvent();
	afx_msg void OnNewBtn();
	afx_msg void OnEditBtn();
	afx_msg void OnDeleteBtn();
	afx_msg void OnRefreshBtn();

	void         UpdateDescription();

public:
	virtual BOOL OnInitDialog();
};

