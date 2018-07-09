// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// CItemDescriptionDlg dialog

class CItemDescriptionDlg : public CDialog
{
	DECLARE_DYNAMIC(CItemDescriptionDlg)

public:
	CItemDescriptionDlg(CWnd* pParent = NULL, bool bEditName = true,
	                    bool bAllowAnyName = false, bool bLocation = false, bool bTemplate = false);
	virtual ~CItemDescriptionDlg();

	static bool ValidateItem(const CString& item);
	static bool ValidateLocation(CString& location);

	// Dialog Data
	enum { IDD = IDD_ITEMDESCRIPTION };
	enum { IDD_LOCATION = IDD_ITEMLOCDESC };
	enum { IDD_TEMPLATE = IDD_ITEMTEMPLATELOCDESC };

protected:
	bool m_bEditName;
	bool m_bAllowAnyName;
	bool m_bLocation;
	bool m_bTemplate;

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CString m_sCaption;
	CString m_sItemCaption;
	CString m_sItemEdit;
	CString m_sDescription;
	CString m_sLocation;
	CString m_sTemplate;
	CButton m_btnOk;
	afx_msg void OnEnChangeItemedit();
	virtual BOOL OnInitDialog();
};

