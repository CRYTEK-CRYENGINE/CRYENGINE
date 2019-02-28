// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CustomActionsDialog_h__
#define __CustomActionsDialog_h__

#if _MSC_VER > 1000
	#pragma once
#endif

//////////////////////////////////////////////////////////////////////////
// Dialog shown for custom action property
//////////////////////////////////////////////////////////////////////////
class CCustomActionDialog : public CDialog
{
	DECLARE_DYNAMIC(CCustomActionDialog)

private:
	CString  m_customAction;
	CListBox m_wndActionList;

public:
	CCustomActionDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CCustomActionDialog();

	void    SetCustomAction(const CString& customAction) { m_customAction = customAction; }
	CString GetCustomAction()                            { return m_customAction; };

	// Dialog Data
	enum { IDD = IDD_AIANCHORS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	bool         OpenViewForCustomAction();

	DECLARE_MESSAGE_MAP()

	afx_msg void OnLbnDblClk();
	afx_msg void OnLbnSelchangeAction();
	afx_msg void OnNewBtn();
	afx_msg void OnEditBtn();
	afx_msg void OnRefreshBtn();
public:
	virtual BOOL OnInitDialog();
};

#endif // __CustomActionsDialog_h__

