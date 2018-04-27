// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __mannerrorreportdialog_h__
#define __mannerrorreportdialog_h__
#pragma once

#include "MannequinBase.h"

// CMannErrorReportDialog dialog

class CMannErrorRecord
{
public:
	enum ESeverity
	{
		ESEVERITY_ERROR,
		ESEVERITY_WARNING,
		ESEVERITY_COMMENT
	};
	enum EFlags
	{
		FLAG_NOFILE   = 0x0001,   // Indicate that required file was not found.
		FLAG_SCRIPT   = 0x0002,   // Error with scripts.
		FLAG_TEXTURE  = 0x0004,   // Error with scripts.
		FLAG_OBJECTID = 0x0008,   // Error with object Ids, Unresolved/Duplicate etc...
		FLAG_AI       = 0x0010,   // Error with AI.
	};
	//! Severity of this error.
	ESeverity                        severity;
	//! Error Text.
	CString                          error;
	//! Error Type.
	SMannequinErrorReport::ErrorType type;
	//! File which is missing or causing problem.
	CString                          file;
	int                              count;
	FragmentID                       fragmentID;
	SFragTagState                    tagState;
	uint32                           fragOptionID;
	FragmentID                       fragmentIDTo;
	SFragTagState                    tagStateTo;
	const SScopeContextData*         contextDef;

	int                              flags;

	CMannErrorRecord(ESeverity _severity, const char* _error, int _flags = 0, int _count = 0)
		: severity(_severity)
		, flags(_flags)
		, count(_count)
		, error(_error)
		, fragmentID(FRAGMENT_ID_INVALID)
		, fragmentIDTo(FRAGMENT_ID_INVALID)
		, fragOptionID(0)
		, contextDef(NULL)
	{
	}

	CMannErrorRecord()
	{
		severity = ESEVERITY_WARNING;
		flags = 0;
		count = 0;
	}
	CString GetErrorText();
};

class CMannErrorReportDialog : public CXTResizeDialog
{
	//DECLARE_DYNAMIC(CMannErrorReportDialog)
	//DECLARE_DYNCREATE(CMannErrorReportDialog)

public:

	enum EReportColumn
	{
		COLUMN_CHECKBOX,
		COLUMN_SEVERITY,
		COLUMN_FRAGMENT,
		COLUMN_TAGS,
		COLUMN_FRAGMENTTO,
		COLUMN_TAGSTO,
		COLUMN_TYPE,
		COLUMN_TEXT,
		COLUMN_FILE
	};

	CMannErrorReportDialog(CWnd* pParent = NULL);
	virtual ~CMannErrorReportDialog();

	void Clear();
	void CopyToClipboard();
	void Refresh();

	void AddError(const CMannErrorRecord& error);
	bool HasErrors() const
	{
		return (m_errorRecords.size() > 0);
	}

	void SendInMail();
	void OpenInExcel();

	// Dialog Data
	enum { IDD = IDD_MANN_ERROR_REPORT };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	afx_msg void OnNMDblclkErrors(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSelectObjects();
	afx_msg void OnSelectAll();
	afx_msg void OnClearSelection();
	afx_msg void OnCheckErrors();
	afx_msg void OnDeleteFragments();
	afx_msg void OnSize(UINT nType, int cx, int cy);

	afx_msg void OnReportItemClick(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportItemRClick(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportColumnRClick(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportItemDblClick(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportHyperlink(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportKeyDown(NMHDR* pNotifyStruct, LRESULT* result);

	void         RepositionControls();

	void         UpdateErrors();
	void         ReloadErrors();

	DECLARE_MESSAGE_MAP()

	CImageList m_imageList;

	std::vector<CMannErrorRecord> m_errorRecords;

	CXTPReportControl             m_wndReport;
	CXTPReportSubListControl      m_wndSubList;
	CXTPReportFilterEditControl   m_wndFilterEdit;

	SMannequinContexts*           m_contexts;
};

#endif // __mannerrorreportdialog_h__

