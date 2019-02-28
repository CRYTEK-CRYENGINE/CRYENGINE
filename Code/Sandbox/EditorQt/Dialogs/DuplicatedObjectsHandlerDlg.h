// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CDuplicatedObjectsHandlerDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CDuplicatedObjectsHandlerDlg)

public:
	CDuplicatedObjectsHandlerDlg(const char* msg, CWnd* pParent = NULL);
	virtual ~CDuplicatedObjectsHandlerDlg();

	enum EResult
	{
		eResult_None,
		eResult_Override,
		eResult_CreateCopies
	};

	EResult GetResult() const
	{
		return m_result;
	}

	// Dialog Data
	enum { IDD = IDD_DUPLICATED_OBJECTS_HANDLER };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
protected:

	CString m_msg;
	EResult m_result;

	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedOverrideBtn();
	afx_msg void OnBnClickedCreateCopiesBtn();
};

