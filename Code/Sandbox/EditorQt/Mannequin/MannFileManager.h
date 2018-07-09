// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MannFileManager_H__
#define __MannFileManager_H__
#pragma once

#include "MannequinBase.h"
#include "Controls/InPlaceButton.h"

class CMannequinFileChangeWriter;

class CMannFileManager
	: public CXTResizeDialog
{
public:
	CMannFileManager(CMannequinFileChangeWriter& fileChangeWriter, bool bChangedFileMode, CWnd* pParent = NULL);

	enum { IDD = IDD_MANN_FILE_MANAGER };
	enum { IDD_MONITOR = IDD_MANN_FILE_MONITOR };

	afx_msg void OnRefresh();

protected:
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
	afx_msg void OnAddToSourceControl();
	afx_msg void OnCheckOutSelection();
	afx_msg void OnUndoSelection();
	afx_msg void OnReloadAllFiles();
	afx_msg void OnSaveFiles();

	afx_msg void OnReportItemClick(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportItemRClick(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportColumnRClick(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportItemDblClick(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportSelChanged(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportHyperlink(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportKeyDown(NMHDR* pNotifyStruct, LRESULT* result);

	void         RepositionControls();

	void         ReloadFileRecords();
	void         UpdateFileRecords();

	void         SetButtonStates();

	void         OnDisplayOnlyCurrentPreviewClicked();
private:
	CImageList                    m_imageList;

	CXTPReportControl             m_wndReport;
	CXTPReportSubListControl      m_wndSubList;
	CXTPReportFilterEditControl   m_wndFilterEdit;

	std::auto_ptr<CInPlaceButton> m_pShowCurrentPreviewFilesOnlyCheckbox;
	CMannequinFileChangeWriter&   m_fileChangeWriter;
	bool                          m_bSourceControlAvailable;
	bool                          m_bInChangedFileMode;
};

#endif // __MannFileManager_H__

