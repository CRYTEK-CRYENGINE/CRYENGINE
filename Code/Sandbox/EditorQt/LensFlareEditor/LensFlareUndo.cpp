// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryRenderer/IFlares.h>
#include "LensFlareUndo.h"
#include "LensFlareEditor.h"
#include "LensFlareItem.h"
#include "LensFlareManager.h"
#include "LensFlareUtil.h"
#include "LensFlareElementTree.h"

void RestoreLensFlareItem(CLensFlareItem* pLensFlareItem, IOpticsElementBasePtr pOptics, const char* flarePathName)
{
	pLensFlareItem->ReplaceOptics(pOptics);

	CLensFlareEditor* pEditor = CLensFlareEditor::GetLensFlareEditor();
	if (pEditor)
	{
		if (pOptics->GetType() == eFT_Root)
		{
			pEditor->RenameLensFlareItem(pLensFlareItem, pLensFlareItem->GetGroupName(), LensFlareUtil::GetShortName(flarePathName));
			pEditor->UpdateLensFlareItem(pLensFlareItem);
		}
	}
}

void ActivateLensFlareItem(CLensFlareItem* pLensFlareItem, bool bRestoreSelectInfo, const char* selectedFlareItemName)
{
	CLensFlareEditor* pEditor = CLensFlareEditor::GetLensFlareEditor();
	if (pEditor == NULL)
		return;
	if (pLensFlareItem == pEditor->GetLensFlareElementTree()->GetLensFlareItem())
	{
		pEditor->UpdateLensFlareItem(pLensFlareItem);
		if (bRestoreSelectInfo)
			pEditor->SelectItemInLensFlareElementTreeByName(selectedFlareItemName);
	}
}

CUndoLensFlareItem::CUndoLensFlareItem(CLensFlareItem* pLensFlareItem, const char* undoDescription)
{
	if (pLensFlareItem)
	{
		m_Undo.m_pOptics = LensFlareUtil::CreateOptics(pLensFlareItem->GetOptics());
		m_flarePathName = pLensFlareItem->GetFullName();
		m_Undo.m_bRestoreSelectInfo = false;
		CLensFlareEditor* pEditor = CLensFlareEditor::GetLensFlareEditor();
		if (pEditor)
			m_Undo.m_bRestoreSelectInfo = pEditor->GetSelectedLensFlareName(m_Undo.m_selectedFlareItemName);
		m_undoDescription = undoDescription;
	}
}

CUndoLensFlareItem::~CUndoLensFlareItem()
{
}

void CUndoLensFlareItem::Undo(bool bUndo)
{
	if (bUndo)
	{
		CLensFlareItem* pLensFlareItem = (CLensFlareItem*)GetIEditorImpl()->GetLensFlareManager()->FindItemByName(m_flarePathName);
		CLensFlareEditor* pEditor = CLensFlareEditor::GetLensFlareEditor();
		if (pEditor && pLensFlareItem)
		{
			m_Redo.m_bRestoreSelectInfo = false;
			m_Redo.m_pOptics = LensFlareUtil::CreateOptics(pLensFlareItem->GetOptics());
			m_Redo.m_bRestoreSelectInfo = pEditor->GetSelectedLensFlareName(m_Redo.m_selectedFlareItemName);
		}
	}
	Restore(m_Undo);
}

void CUndoLensFlareItem::Redo()
{
	Restore(m_Redo);
}

void CUndoLensFlareItem::Restore(const SData& data)
{
	if (data.m_pOptics == NULL)
		return;

	CLensFlareItem* pLensFlareItem = (CLensFlareItem*)GetIEditorImpl()->GetLensFlareManager()->FindItemByName(m_flarePathName);
	if (pLensFlareItem == NULL)
		return;
	RestoreLensFlareItem(pLensFlareItem, data.m_pOptics, m_flarePathName);
	ActivateLensFlareItem(pLensFlareItem, data.m_bRestoreSelectInfo, data.m_selectedFlareItemName);
}

CUndoRenameLensFlareItem::CUndoRenameLensFlareItem(const char* oldFullName, const char* newFullName, bool bRefreshItemTreeWhenUndo, bool bRefreshItemTreeWhenRedo)
{
	m_undo.m_oldFullItemName = oldFullName;
	m_undo.m_newFullItemName = newFullName;
	m_undo.m_bRefreshItemTreeWhenUndo = bRefreshItemTreeWhenUndo;
	m_undo.m_bRefreshItemTreeWhenRedo = bRefreshItemTreeWhenRedo;
}

void CUndoRenameLensFlareItem::Undo(bool bUndo)
{
	if (bUndo)
	{
		m_redo.m_oldFullItemName = m_undo.m_newFullItemName;
		m_redo.m_newFullItemName = m_undo.m_oldFullItemName;
		m_redo.m_bRefreshItemTreeWhenUndo = m_undo.m_bRefreshItemTreeWhenUndo;
		m_redo.m_bRefreshItemTreeWhenRedo = m_undo.m_bRefreshItemTreeWhenRedo;
	}

	Rename(m_undo, m_undo.m_bRefreshItemTreeWhenUndo);
}

void CUndoRenameLensFlareItem::Redo()
{
	Rename(m_redo, m_redo.m_bRefreshItemTreeWhenRedo);
}

void CUndoRenameLensFlareItem::Rename(const SUndoDataStruct& data, bool bRefreshItemTree)
{
	CLensFlareEditor* pEditor = CLensFlareEditor::GetLensFlareEditor();
	if (pEditor == NULL)
		return;

	CLensFlareItem* pLensFlareItem = (CLensFlareItem*)GetIEditorImpl()->GetLensFlareManager()->FindItemByName(data.m_newFullItemName);
	if (pLensFlareItem == NULL)
		return;

	int nDotPos = data.m_oldFullItemName.Find(".");
	if (nDotPos == -1)
		return;

	string shortName(LensFlareUtil::GetShortName(data.m_oldFullItemName));
	string groupName(LensFlareUtil::GetGroupNameFromFullName(data.m_oldFullItemName));
	pEditor->RenameLensFlareItem(pLensFlareItem, groupName, shortName);
	if (pLensFlareItem->GetOptics())
	{
		pLensFlareItem->GetOptics()->SetName(shortName);
		LensFlareUtil::UpdateOpticsName(pLensFlareItem->GetOptics());
	}
	pEditor->UpdateLensFlareItem(pLensFlareItem);
	if (bRefreshItemTree)
		pEditor->ReloadItems();
}

CUndoLensFlareElementSelection::CUndoLensFlareElementSelection(CLensFlareItem* pLensFlareItem, const char* flareTreeItemFullName, const char* undoDescription)
{
	if (pLensFlareItem)
	{
		m_flarePathNameForUndo = pLensFlareItem->GetFullName();
		m_flareTreeItemFullNameForUndo = flareTreeItemFullName;
		m_undoDescription = undoDescription;
	}
}

void CUndoLensFlareElementSelection::Undo(bool bUndo)
{
	if (bUndo)
	{
		m_flarePathNameForRedo = m_flarePathNameForUndo;
		CLensFlareEditor* pEditor = CLensFlareEditor::GetLensFlareEditor();
		if (pEditor)
			pEditor->GetSelectedLensFlareName(m_flareTreeItemFullNameForRedo);
	}

	CLensFlareItem* pLensFlareItem = (CLensFlareItem*)GetIEditorImpl()->GetLensFlareManager()->FindItemByName(m_flarePathNameForUndo);
	if (pLensFlareItem == NULL)
		return;
	ActivateLensFlareItem(pLensFlareItem, true, m_flareTreeItemFullNameForUndo);
}

void CUndoLensFlareElementSelection::Redo()
{
	CLensFlareItem* pLensFlareItem = (CLensFlareItem*)GetIEditorImpl()->GetLensFlareManager()->FindItemByName(m_flarePathNameForRedo);
	if (pLensFlareItem == NULL)
		return;
	ActivateLensFlareItem(pLensFlareItem, true, m_flareTreeItemFullNameForRedo);
}

CUndoLensFlareItemSelectionChange::CUndoLensFlareItemSelectionChange(const char* fullLensFlareItemName, const char* undoDescription)
{
	m_FullLensFlareItemNameForUndo = fullLensFlareItemName;
	m_undoDescription = undoDescription;
}

void CUndoLensFlareItemSelectionChange::Undo(bool bUndo)
{
	CLensFlareEditor* pEditor = CLensFlareEditor::GetLensFlareEditor();
	if (pEditor == NULL)
		return;

	if (bUndo)
	{
		string currentFullName;
		pEditor->GetFullSelectedFlareItemName(currentFullName);
		m_FullLensFlareItemNameForRedo = currentFullName;
	}

	pEditor->SelectLensFlareItem(m_FullLensFlareItemNameForUndo);
}

void CUndoLensFlareItemSelectionChange::Redo()
{
	CLensFlareEditor* pEditor = CLensFlareEditor::GetLensFlareEditor();
	if (pEditor == NULL)
		return;
	pEditor->SelectLensFlareItem(m_FullLensFlareItemNameForRedo);
}

