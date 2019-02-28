// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DuplicatedObjectsHandlerDlg.h"

IMPLEMENT_DYNAMIC(CDuplicatedObjectsHandlerDlg, CDialogEx)
CDuplicatedObjectsHandlerDlg::CDuplicatedObjectsHandlerDlg(const char* msg, CWnd* pParent)
	: CDialogEx(CDuplicatedObjectsHandlerDlg::IDD, pParent)
{
	m_msg = msg;
}

CDuplicatedObjectsHandlerDlg::~CDuplicatedObjectsHandlerDlg()
{
}

void CDuplicatedObjectsHandlerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CDuplicatedObjectsHandlerDlg, CDialogEx)
ON_BN_CLICKED(ID_OVERRIDE, OnBnClickedOverrideBtn)
ON_BN_CLICKED(ID_CREATE_COPIES, OnBnClickedCreateCopiesBtn)
END_MESSAGE_MAP()

BOOL CDuplicatedObjectsHandlerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	CEdit* pEdit = (CEdit*)GetDlgItem(IDC_EDIT_INFORMATION);
	pEdit->SetWindowText(m_msg);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CDuplicatedObjectsHandlerDlg::OnBnClickedOverrideBtn()
{
	m_result = eResult_Override;
	OnOK();
}

void CDuplicatedObjectsHandlerDlg::OnBnClickedCreateCopiesBtn()
{
	m_result = eResult_CreateCopies;
	OnOK();
}

