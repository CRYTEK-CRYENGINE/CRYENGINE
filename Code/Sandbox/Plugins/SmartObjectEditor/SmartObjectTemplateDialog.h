// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// CSmartObjectTemplateDialog dialog

class CSmartObjectTemplateDialog : public CDialog
{
	DECLARE_DYNAMIC(CSmartObjectTemplateDialog)

private:
	int      m_idSOTemplate;
	CListBox m_wndTemplateList;

public:
	CSmartObjectTemplateDialog(CWnd* pParent = NULL);     // standard constructor
	virtual ~CSmartObjectTemplateDialog();

	void        SetSOTemplate(const char* sSOTemplate);
	const char* GetSOTemplate() const;
	int         GetSOTemplateId() const { return m_idSOTemplate; }

	// Dialog Data
	enum { IDD = IDD_AIANCHORS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);      // DDX/DDV support

	DECLARE_MESSAGE_MAP()

	afx_msg void OnLbnDblClk();
	afx_msg void OnLbnSelchangeTemplate();
	afx_msg void OnNewBtn();
	afx_msg void OnEditBtn();
	afx_msg void OnDeleteBtn();
	afx_msg void OnRefreshBtn();

	void         UpdateDescription();

public:
	virtual BOOL OnInitDialog();
};

