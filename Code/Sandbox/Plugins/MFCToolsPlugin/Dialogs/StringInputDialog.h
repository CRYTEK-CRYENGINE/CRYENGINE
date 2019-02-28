// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// This is the header file for the general utility dialog for
// string input. The purpose of this dialog, as one might imagine, is to get
// string input for any purpose necessary.
// The recomended way to call this dialog is through DoModal() method.

#ifndef StringInputDialog_h__
#define StringInputDialog_h__

#pragma once

class PLUGIN_API CStringInputDialog : public CDialog
{
	//////////////////////////////////////////////////////////////////////////
	// Types & Typedefs
public:
protected:
private:
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Methods
public:
	CStringInputDialog();
	CStringInputDialog(CString strText, CString strTitle);
	void    DoDataExchange(CDataExchange* pDX);
	BOOL    OnInitDialog();
	void    OnOK();

	void    SetText(CString strText);
	void    SetTitle(CString strTitle);

	CString GetResultingText();
protected:
private:
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Data fields
public:
protected:
	CString m_strTitle;
	CString m_strText;
	CEdit   m_nameEdit;
private:
	//////////////////////////////////////////////////////////////////////////
};

#endif // StringInputDialog_h__

