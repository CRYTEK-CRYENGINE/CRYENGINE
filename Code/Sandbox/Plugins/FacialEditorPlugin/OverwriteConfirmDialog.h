// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __OVERWRITECONFIRMDIALOG_H__
#define __OVERWRITECONFIRMDIALOG_H__

class COverwriteConfirmDialog : public CDialog
{
public:

	COverwriteConfirmDialog(CWnd* pParentWindow, const char* szMessage, const char* szCaption);

private:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	afx_msg BOOL OnCommand(UINT uID);

	DECLARE_MESSAGE_MAP()

	string message;
	string caption;
	CEdit  messageEdit;
};

#endif //__OVERWRITECONFIRMDIALOG_H__

