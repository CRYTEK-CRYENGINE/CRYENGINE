// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __REPORTDIALOG_H__
#define __REPORTDIALOG_H__

#include "Report.h"

class CReportDialog : public CXTResizeDialog
{
	DECLARE_DYNAMIC(CReportDialog)

public:
	CReportDialog(const char* title, CWnd* pParent = NULL);
	virtual ~CReportDialog();

	void Open(CReport* report);
	void Close();

protected:
	void         Load(CReport* report);

	virtual void OnOK();
	virtual void OnCancel();
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void PostNcDestroy();
	afx_msg void OnSelectObjects();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);

	afx_msg void OnReportItemClick(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportItemRClick(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportColumnRClick(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportItemDblClick(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportHyperlink(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportKeyDown(NMHDR* pNotifyStruct, LRESULT* result);

	DECLARE_MESSAGE_MAP()

	CXTPReportControl m_wndReport;
	CXTPReportSubListControl    m_wndSubList;
	CXTPReportFilterEditControl m_wndFilterEdit;
	string                      m_title;
	string                      m_profileTitle;
};

#endif //__REPORTDIALOG_H__

