// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// This is the source file for the general utility dialog for
// string input. The purpose of this dialog, as one might imagine, is to get
// string input for any purpose necessary.
// The recomended way to call this dialog is through DoModal() method.

#include "stdafx.h"
#include "StringInputDialog.h"

//////////////////////////////////////////////////////////////////////////
CStringInputDialog::CStringInputDialog() :
	CDialog(IDD_DIALOG_RENAME),
	m_strText(""),
	m_strTitle("Please type your text in the text box bellow")
{

}
//////////////////////////////////////////////////////////////////////////
CStringInputDialog::CStringInputDialog(CString strName, CString strCaption)
	: CDialog(IDD_DIALOG_RENAME),
	m_strText(strName),
	m_strTitle(strCaption)
{

}
//////////////////////////////////////////////////////////////////////////
void CStringInputDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_NAME, m_nameEdit);
	DDX_Text(pDX, IDC_EDIT_NAME, m_strText);
}
//////////////////////////////////////////////////////////////////////////
BOOL CStringInputDialog::OnInitDialog()
{
	if (__super::OnInitDialog() == FALSE)
		return FALSE;

	SetWindowText(m_strTitle);

	UpdateData(FALSE);

	// set the focus on the edit control
	m_nameEdit.SetFocus();
	// move the cursor at the end of the text; ( 0, -1 ) would select the entire text
	m_nameEdit.SetSel(m_strText.GetLength(), -1);

	return FALSE;
}
//////////////////////////////////////////////////////////////////////////
void CStringInputDialog::OnOK()
{
	UpdateData(TRUE);
	__super::OnOK();
}
//////////////////////////////////////////////////////////////////////////
void CStringInputDialog::SetText(CString strName)
{
	m_strText = strName;
}
//////////////////////////////////////////////////////////////////////////
void CStringInputDialog::SetTitle(CString strCaption)
{
	m_strTitle = strCaption;
}
//////////////////////////////////////////////////////////////////////////
CString CStringInputDialog::GetResultingText()
{
	return m_strText;
}
//////////////////////////////////////////////////////////////////////////

