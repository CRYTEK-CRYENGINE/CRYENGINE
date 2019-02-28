// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CheckOutDialog.h"

// CCheckOutDialog dialog

IMPLEMENT_DYNAMIC(CCheckOutDialog, CDialog)

CCheckOutDialog::CCheckOutDialog(const CString& file, CWnd* pParent)
	: CDialog(IDD_CHECKOUT, pParent)
	, m_text(_T(""))
{
	m_file = file;
	m_result = CANCEL;
}

CCheckOutDialog::~CCheckOutDialog()
{
}

void CCheckOutDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_TEXT, m_text);
}

BEGIN_MESSAGE_MAP(CCheckOutDialog, CDialog)
ON_BN_CLICKED(IDC_CHECKOUT, OnBnClickedCheckout)
ON_BN_CLICKED(IDC_OVERWRITEALL, OnBnClickedOverwriteAll)
ON_BN_CLICKED(IDOK, OnBnClickedOk)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
// CCheckOutDialog message handlers
void CCheckOutDialog::OnBnClickedCheckout()
{
	// Check out this file.
	m_result = CHECKOUT;
	OnOK();
}

//////////////////////////////////////////////////////////////////////////
void CCheckOutDialog::OnBnClickedOk()
{
	// Overwrite this file.
	m_result = OVERWRITE;
	OnOK();
}

//////////////////////////////////////////////////////////////////////////
void CCheckOutDialog::OnBnClickedOverwriteAll()
{
	m_result = OVERWRITE_ALL;
	InstanceIsForAll() = true;
	OnOK();
}

//////////////////////////////////////////////////////////////////////////
BOOL CCheckOutDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetWindowText(PathUtil::GetFile(string(m_file)) + " is read-only");

	m_text.Format(_T("%s is read-only file, can be under Source Control."), (const char*)m_file);

	if (InstanceEnableForAll() && GetDlgItem(IDC_OVERWRITEALL))
		GetDlgItem(IDC_OVERWRITEALL)->EnableWindow(TRUE);

	GetDlgItem(IDC_CHECKOUT)->EnableWindow(GetIEditor()->IsSourceControlAvailable() ? TRUE : FALSE);

	UpdateData(FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

//static ////////////////////////////////////////////////////////////////
bool& CCheckOutDialog::InstanceEnableForAll()
{
	static bool isEnableForAll = false;
	return isEnableForAll;
}

//static ////////////////////////////////////////////////////////////////
bool& CCheckOutDialog::InstanceIsForAll()
{
	static bool isForAll = false;
	return isForAll;
}

//static ////////////////////////////////////////////////////////////////
bool CCheckOutDialog::EnableForAll(bool isEnable)
{
	bool bPrevEnable = InstanceEnableForAll();
	InstanceEnableForAll() = isEnable;
	if (!bPrevEnable || !isEnable)
		InstanceIsForAll() = false;
	return bPrevEnable;
}

