// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// This is the implementation file for the general utility dialog for
// overwrite confirmation. The purpose of this dialog, as one might imagine, is to
// get check if the user really wants to overwrite some item, allowing them to
// apply the same choices to all the remaining items.
// The recomended way to call this dialog is through DoModal() method.

#include "stdafx.h"
#include "GenericOverwriteDialog.h"

BEGIN_MESSAGE_MAP(CGenericOverwriteDialog, CDialog)
ON_COMMAND(IDC_GENERIC_OVERWRITE_YES_BUTTON, &CGenericOverwriteDialog::OnYes)
ON_COMMAND(IDC_GENERIC_OVERWRITE_NO_BUTTON, &CGenericOverwriteDialog::OnNo)
ON_COMMAND(IDC_GENERIC_OVERWRITE_CANCEL_BUTTON, &CGenericOverwriteDialog::OnCancel)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CGenericOverwriteDialog::CGenericOverwriteDialog(const CString& strTitle, const CString& strText, bool boMultipleFiles) :
	CDialog(IDD_GENERIC_OVERWRITE_DIALOG),
	m_strText(strText),
	m_strTitle(strTitle),
	m_boToAll(false),
	m_boMultipleFiles(boMultipleFiles)
{

}
//////////////////////////////////////////////////////////////////////////
void CGenericOverwriteDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_GENERIC_OVERWRITE_TEXT, m_strTextMessage);
	DDX_Control(pDX, IDC_GENERIC_OVERWRITE_ALL_ITEMS_CHECK, m_oToAll);
}
//////////////////////////////////////////////////////////////////////////
BOOL CGenericOverwriteDialog::OnInitDialog()
{
	if (__super::OnInitDialog() == FALSE)
		return FALSE;

	SetWindowText(m_strTitle);
	m_strTextMessage.SetWindowText(m_strText);

	if (!m_boMultipleFiles)
	{
		m_oToAll.ShowWindow(SW_HIDE);
	}

	UpdateData(FALSE);

	return TRUE;
}
//////////////////////////////////////////////////////////////////////////
void CGenericOverwriteDialog::OnYes()
{
	m_boToAll = (m_oToAll.GetCheck() == BST_CHECKED);

	if (m_boIsModal)
	{
		EndDialog(IDYES);
		m_boIsModal = false;
	}
}
//////////////////////////////////////////////////////////////////////////
void CGenericOverwriteDialog::OnNo()
{
	if (m_boIsModal)
	{
		EndDialog(IDNO);
		m_boIsModal = false;
	}
}
//////////////////////////////////////////////////////////////////////////
void CGenericOverwriteDialog::OnCancel()
{
	if (m_boIsModal)
	{
		EndDialog(IDCANCEL);
		m_boIsModal = false;
	}
}
//////////////////////////////////////////////////////////////////////////
INT_PTR CGenericOverwriteDialog::DoModal()
{
	m_boIsModal = true;
	return __super::DoModal();
}
//////////////////////////////////////////////////////////////////////////
bool CGenericOverwriteDialog::IsToAllToggled()
{
	return m_boToAll;
}
//////////////////////////////////////////////////////////////////////////

