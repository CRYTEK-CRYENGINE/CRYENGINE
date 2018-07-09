// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MANN_CONTEXT_EDITOR_DIALOG_H__
#define __MANN_CONTEXT_EDITOR_DIALOG_H__
#pragma once

#include "MannequinBase.h"

class CXTPMannContextRecord;

class CMannContextEditorDialog : public CXTResizeDialog
{
public:
	CMannContextEditorDialog(CWnd* pParent = NULL);
	virtual ~CMannContextEditorDialog();

	afx_msg void OnReportItemDblClick(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportSelChanged(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportKeyDown(NMHDR* pNotifyStruct, LRESULT* result);

	afx_msg void OnNewContext();
	afx_msg void OnEditContext();
	afx_msg void OnCloneAndEditContext();
	afx_msg void OnDeleteContext();
	afx_msg void OnMoveUp();
	afx_msg void OnMoveDown();

	afx_msg void OnImportBackground();

protected:
	DECLARE_MESSAGE_MAP()

	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();

private:
	void                   PopulateReport();
	void                   EnableControls();
	CXTPMannContextRecord* GetFocusedRecord();
	void                   SetFocusedRecord(const uint32 dataID);

	CXTPReportControl m_wndReport;
	CButton           m_newContext;
	CButton           m_editContext;
	CButton           m_cloneContext;
	CButton           m_deleteContext;
	CButton           m_moveUp;
	CButton           m_moveDown;
};

#endif

