// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002-2012.
// -------------------------------------------------------------------------
//  File name:   TrackViewFBXImportPreviewDialog.h
//  Version:     v1.00
//  Created:     3/12/2012 by Konrad.
//  Compilers:   Visual Studio 2010
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __TRACKVIEW_FBX_IMPORT_PREVIEW_DIALOG_H__
#define __TRACKVIEW_FBX_IMPORT_PREVIEW_DIALOG_H__
#pragma once

class CTrackViewFBXImportPreviewDialog : public CDialog
{
	DECLARE_DYNAMIC(CTrackViewFBXImportPreviewDialog)

public:
	CTrackViewFBXImportPreviewDialog();
	virtual ~CTrackViewFBXImportPreviewDialog(){}

	void AddTreeItem(const CString& objectName);
	bool IsObjectSelected(const CString& objectName) { return m_fBXItemNames[objectName]; };

private:

	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	void         OnClickTree(NMHDR* pNMHDR, LRESULT* pResult);

	DECLARE_MESSAGE_MAP()

private:
	typedef std::map<CString, bool> TItemsMap;
	CTreeCtrl m_tree;
	TItemsMap m_fBXItemNames;
};

#endif

