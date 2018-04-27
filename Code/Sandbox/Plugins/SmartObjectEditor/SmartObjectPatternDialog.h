// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// CSmartObjectPatternDialog dialog

class CSmartObjectPatternDialog : public CDialog
{
	DECLARE_DYNAMIC(CSmartObjectPatternDialog)

private:
	CString  m_sPattern;
	CListBox m_wndList;

public:
	CSmartObjectPatternDialog(CWnd* pParent = NULL);
	virtual ~CSmartObjectPatternDialog();

	void    SetPattern(const CString& sPattern) { m_sPattern = sPattern; }
	CString GetPattern()                        { return m_sPattern; };

	enum { IDD = IDD_AISTATEPATTERN };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

	afx_msg void OnLbnDblClk();
	afx_msg void OnLbnSelchange();
	afx_msg void OnNewBtn();
	afx_msg void OnEditBtn();
	afx_msg void OnDeleteBtn();
	afx_msg void OnOkBtn();

public:
	virtual BOOL OnInitDialog();
};

