// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002-2012.
// -------------------------------------------------------------------------
//  File name:   TrackViewFBXImportPreviewDialog.cpp
//  Version:     v1.00
//  Created:     03/12/2012 by Konrad.
//  Compilers:   Visual Studio 2010
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "TrackViewFBXImportPreviewDialog.h"

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CTrackViewFBXImportPreviewDialog, CDialog)
CTrackViewFBXImportPreviewDialog::CTrackViewFBXImportPreviewDialog()
	: CDialog(IDD_TRACKVIEW_FBX_IMPORT_DLG, NULL)
{
}

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CTrackViewFBXImportPreviewDialog, CDialog)
ON_NOTIFY(NM_CLICK, IDC_TRACKVIEW_FBX_IMPORT_SEL_TREE, OnClickTree)
END_MESSAGE_MAP()

void CTrackViewFBXImportPreviewDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TRACKVIEW_FBX_IMPORT_SEL_TREE, m_tree);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewFBXImportPreviewDialog::OnClickTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	UINT uFlags = 0;
	CPoint pt(0, 0);
	GetCursorPos(&pt);
	m_tree.ScreenToClient(&pt);
	HTREEITEM hItem = m_tree.HitTest(pt, &uFlags);
	if (NULL != hItem && (TVHT_ONITEMSTATEICON & uFlags))
	{
		m_tree.SelectItem(hItem);
		m_fBXItemNames[m_tree.GetItemText(hItem)] = !m_tree.GetCheck(hItem);
	}

	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
BOOL CTrackViewFBXImportPreviewDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_tree.SetIndent(0);
	m_tree.ModifyStyle(TVS_CHECKBOXES, 0);
	m_tree.ModifyStyle(0, TVS_CHECKBOXES);

	for (TItemsMap::const_iterator itemIter = m_fBXItemNames.begin(); itemIter != m_fBXItemNames.end(); ++itemIter)
	{
		HTREEITEM newItem = m_tree.InsertItem(itemIter->first, 0, 0, TVI_ROOT);
		m_tree.SetCheck(newItem, true);
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewFBXImportPreviewDialog::AddTreeItem(const CString& objectName)
{
	m_fBXItemNames[objectName] = true;
}

