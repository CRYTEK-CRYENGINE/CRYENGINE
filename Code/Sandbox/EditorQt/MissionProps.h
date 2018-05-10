// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// CMissionProps dialog

class CMissionProps : public CPropertyPage
{
	DECLARE_DYNAMIC(CMissionProps)

private:
	CButton  m_btnScript;
	CButton  m_btnEquipPack;
	CListBox m_lstMethods;
	CListBox m_lstEvents;

public:
	CMissionProps();
	virtual ~CMissionProps();

	// Dialog Data
	enum { IDD = IDD_MISSIONMAINPROPS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
private:
	void         LoadScript(CString sFilename);
public:
	afx_msg void OnBnClickedEquippack();
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedBrowse();
	afx_msg void OnBnClickedRemove();
	afx_msg void OnBnClickedCreate();
	afx_msg void OnBnClickedReload();
	afx_msg void OnBnClickedEdit();
};

