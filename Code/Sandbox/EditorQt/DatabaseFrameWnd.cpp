// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DatabaseFrameWnd.h"
#include "BaseLibraryItem.h"
#include "BaseLibraryManager.h"
#include "Dialogs/QGroupDialog.h"
#include "Util/Clipboard.h"
#include "Controls/QuestionDialog.h"
#include <FilePathUtil.h>

class CUndoSelectLibraryUndo : public IUndoObject
{
public:
	CUndoSelectLibraryUndo(const CString& libraryName, const CString& wndClssName)
	{
		m_LibraryName = libraryName;
		m_WndClassName = wndClssName;
	}
	~CUndoSelectLibraryUndo(){}

protected:
	const char* GetDescription() { return "Select database library."; };
	void        Undo(bool bUndo)
	{
		SelectLibrary(bUndo);
	}
	void Redo()
	{
		SelectLibrary(true);
	}

private:

	void SelectLibrary(bool bUndo)
	{
		CDatabaseFrameWnd* pDatabaseEditor = (CDatabaseFrameWnd*)GetIEditorImpl()->FindView(m_WndClassName);
		if (pDatabaseEditor == NULL)
			return;

		CString libraryNameForUndo(m_LibraryName);
		if (bUndo)
			m_LibraryName = pDatabaseEditor->GetSelectedLibraryName();
		pDatabaseEditor->SelectLibrary(libraryNameForUndo);
	}

	CString m_LibraryName;
	CString m_WndClassName;

};

static const int LIBRARY_CB_WIDTH(150);

IMPLEMENT_DYNAMIC(CDatabaseFrameWnd, CBaseFrameWnd)

BEGIN_MESSAGE_MAP(CDatabaseFrameWnd, CBaseFrameWnd)
ON_COMMAND(ID_DB_ADDLIB, OnAddLibrary)
ON_COMMAND(ID_DB_DELLIB, OnRemoveLibrary)
ON_COMMAND(ID_DB_REMOVE, OnRemoveItem)
ON_COMMAND(ID_DB_RENAME, OnRenameItem)
ON_COMMAND(ID_DB_SAVE, OnSave)
ON_COMMAND(ID_DB_EXPORTLIBRARY, OnExportLibrary)
ON_COMMAND(ID_DB_LOADLIB, OnLoadLibrary)
ON_COMMAND(ID_DB_RELOAD, OnReloadLib)
ON_COMMAND(ID_DB_RELOADLIB, OnReloadLib)
ON_CBN_SELENDOK(ID_DB_LIBRARY, OnChangedLibrary)
ON_NOTIFY(TVN_SELCHANGED, IDC_LIBRARY_ITEMS_TREE, OnSelChangedItemTree)
ON_NOTIFY(TVN_KEYDOWN, IDC_LIBRARY_ITEMS_TREE, OnKeyDownItemTree)
ON_COMMAND(ID_DB_CUT, OnCut)
ON_COMMAND(ID_DB_COPY, OnCopy)
ON_COMMAND(ID_DB_PASTE, OnPaste)
ON_COMMAND(ID_DB_CLONE, OnClone)
ON_UPDATE_COMMAND_UI(ID_DB_COPY, OnUpdateSelected)
ON_UPDATE_COMMAND_UI(ID_DB_CUT, OnUpdateSelected)
ON_UPDATE_COMMAND_UI(ID_DB_CLONE, OnUpdateSelected)
ON_UPDATE_COMMAND_UI(ID_DB_PASTE, OnUpdatePaste)
ON_COMMAND(ID_UNDO, OnUndo)
ON_COMMAND(ID_REDO, OnRedo)
END_MESSAGE_MAP()

CDatabaseFrameWnd::CDatabaseFrameWnd()
{
	m_sortRecursionType = SORT_RECURSION_FULL;
	m_pItemManager = NULL;
	m_pLibrary = NULL;
	m_pCurrentItem = NULL;
	GetIEditorImpl()->RegisterNotifyListener(this);
}

CDatabaseFrameWnd::~CDatabaseFrameWnd()
{
	GetIEditorImpl()->UnregisterNotifyListener(this);
}

void CDatabaseFrameWnd::InitToolBars()
{
	CXTPCommandBars* pCommandBars = GetCommandBars();
	if (pCommandBars == NULL)
		return;

	CXTPToolBar* pLibToolBar = pCommandBars->Add(_T("Lens Flare Toolbar"), xtpBarTop);
	pLibToolBar->LoadToolBar(IDR_DB_LIBRARY_BAR);

	CXTPControl* pCtrl = pLibToolBar->GetControls()->FindControl(xtpControlButton, ID_DB_LIBRARY, TRUE, FALSE);
	if (pCtrl)
	{
		CRect rc(0, 0, LIBRARY_CB_WIDTH, 16);
		m_LibraryListComboBox.Create(WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_SORT, rc, this, ID_DB_LIBRARY);

		CXTPControlCustom* pCustomCtrl = (CXTPControlCustom*)pLibToolBar->GetControls()->SetControlType(pCtrl->GetIndex(), xtpControlCustom);
		pCustomCtrl->SetSize(CSize(LIBRARY_CB_WIDTH, 16));
		pCustomCtrl->SetControl(&m_LibraryListComboBox);
		pCustomCtrl->SetFlags(xtpFlagManualUpdate);
	}

	CXTPToolBar* pStdToolBar = GetCommandBars()->Add(_T("StandartToolBar"), xtpBarTop);
	pStdToolBar->EnableCustomization(FALSE);
	VERIFY(pStdToolBar->LoadToolBar(IDR_DB_STANDART));
	DockRightOf(pStdToolBar, pLibToolBar);

	CXTPToolBar* pItemToolBar = GetCommandBars()->Add(_T("ItemToolBar"), xtpBarTop);
	pItemToolBar->EnableCustomization(FALSE);
	VERIFY(pItemToolBar->LoadToolBar(IDR_DB_LIBRARY_ITEM_BAR));
	DockRightOf(pItemToolBar, pLibToolBar);

	pLibToolBar->SetCloseable(false);
	pStdToolBar->SetCloseable(false);
	pItemToolBar->SetCloseable(false);
}

void CDatabaseFrameWnd::InitStatusBar()
{
	m_StatusBar.Create(this);
	static UINT indicators[] = { 1 };
	m_StatusBar.SetIndicators(indicators, sizeof(indicators) / sizeof(*indicators));
	m_StatusBar.SetPaneInfo(0, 1, SBPS_STRETCH, 100);
	m_StatusBar.ShowWindow(SW_SHOW);
}

void CDatabaseFrameWnd::InitTreeCtrl()
{
	CRect rc;
	GetClientRect(rc);
	GetTreeCtrl().Create(WS_VISIBLE | WS_CHILD | WS_TABSTOP | WS_BORDER | TVS_HASBUTTONS | TVS_SHOWSELALWAYS | TVS_LINESATROOT | TVS_HASLINES | TVS_EDITLABELS |
	                     TVS_FULLROWSELECT | TVS_NOHSCROLL | TVS_INFOTIP, rc, this, IDC_LIBRARY_ITEMS_TREE);
}

void CDatabaseFrameWnd::ReloadLibs()
{
	LOADING_TIME_PROFILE_SECTION;
	if (!m_pItemManager)
		return;

	SelectItem(0);

	CString selectedLib;
	if (m_LibraryListComboBox)
	{
		m_LibraryListComboBox.ResetContent();
		m_LibraryListComboBox.SetDroppedWidth(LIBRARY_CB_WIDTH);
	}
	bool bFound = false;
	for (int i = 0; i < m_pItemManager->GetLibraryCount(); i++)
	{
		CString library = m_pItemManager->GetLibrary(i)->GetName();
		if (selectedLib.IsEmpty())
			selectedLib = library;
		if (m_LibraryListComboBox)
		{
			int index = m_LibraryListComboBox.AddString(library);
			m_LibraryListComboBox.SetItemDataPtr(index, (CBaseLibrary*)m_pItemManager->GetLibrary(i));
			if (m_LibraryListComboBox.GetDC())
			{
				int nWidth = (m_LibraryListComboBox.GetDC()->GetTextExtent(library)).cx + ::GetSystemMetrics(SM_CXVSCROLL) + 2 * ::GetSystemMetrics(SM_CXEDGE);
				if (m_LibraryListComboBox.GetDroppedWidth() < nWidth)
					m_LibraryListComboBox.SetDroppedWidth(nWidth);
			}
		}
	}
	m_selectedLib = "";
	SelectLibrary(selectedLib);
	m_bLibsLoaded = true;
}

void CDatabaseFrameWnd::ReloadItems()
{
	m_bIgnoreSelectionChange = true;
	GetTreeCtrl().SetRedraw(FALSE);
	SelectItem(0);
	m_selectedGroup = "";
	m_pCurrentItem = 0;
	m_cpoSelectedLibraryItems.clear();
	if (m_pItemManager)
		m_pItemManager->SetSelectedItem(0);
	m_itemsToTree.clear();
	ReleasePreviewControl();
	DeleteAllTreeItems();
	m_bIgnoreSelectionChange = false;

	if (!m_pLibrary)
	{
		GetTreeCtrl().SetRedraw(TRUE);
		return;
	}

	CString filter = "";
	bool bFilter = !filter.IsEmpty();

	HTREEITEM hLibItem = GetRootItem();

	m_bIgnoreSelectionChange = true;

	std::map<CString, HTREEITEM, stl::less_stricmp<CString>> items;
	for (int i = 0; i < m_pLibrary->GetItemCount(); i++)
	{
		CBaseLibraryItem* pItem = (CBaseLibraryItem*)m_pLibrary->GetItem(i);
		CString name = pItem->GetName();

		if (bFilter)
		{
			if (strstri(name, filter) == 0)
				continue;
		}

		HTREEITEM hRoot = hLibItem;
		char* token;
		char itempath[1024];
		cry_strcpy(itempath, name);

		token = strtok(itempath, "\\/.");

		CString itemName;
		while (token)
		{
			CString strToken = token;

			token = strtok(NULL, "\\/.");
			if (!token)
				break;

			itemName += strToken + "/";

			HTREEITEM hParentItem = stl::find_in_map(items, itemName, 0);
			if (!hParentItem)
			{
				hRoot = GetTreeCtrl().InsertItem(strToken, 0, 1, hRoot);
				items[itemName] = hRoot;
			}
			else
				hRoot = hParentItem;
		}

		HTREEITEM hNewItem = InsertItemToTree(pItem, hRoot);
	}

	SortItems(m_sortRecursionType);
	GetTreeCtrl().Expand(hLibItem, TVE_EXPAND);
	GetTreeCtrl().SetRedraw(TRUE);

	m_bIgnoreSelectionChange = false;
}

void CDatabaseFrameWnd::SelectLibrary(const CString& library, bool bForceSelect)
{
	CWaitCursor wait;
	if (m_selectedLib != library || bForceSelect)
	{
		if (CUndo::IsRecording())
			CUndo::Record(new CUndoSelectLibraryUndo(m_selectedLib, GetClassName()));

		SelectItem(0);
		m_pLibrary = FindLibrary(library);
		if (m_pLibrary)
		{
			m_selectedLib = library;
		}
		else
		{
			m_selectedLib = "";
		}
		ReloadItems();
	}
	if (m_LibraryListComboBox)
		m_LibraryListComboBox.SetCurSel(GetComboBoxIndex(m_pLibrary));
}

void CDatabaseFrameWnd::SelectLibrary(CBaseLibrary* pItem, bool bForceSelect)
{
	CWaitCursor wait;
	if (m_pLibrary != pItem || bForceSelect)
	{
		if (CUndo::IsRecording())
			CUndo::Record(new CUndoSelectLibraryUndo(m_selectedLib, GetClassName()));

		SelectItem(0);
		m_pLibrary = pItem;
		if (m_pLibrary)
		{
			m_selectedLib = pItem->GetName();
		}
		else
		{
			m_selectedLib = "";
		}
		ReloadItems();
	}
	if (m_LibraryListComboBox)
		m_LibraryListComboBox.SetCurSel(GetComboBoxIndex(pItem));
}

void CDatabaseFrameWnd::SelectItem(CBaseLibraryItem* item, bool bForceReload)
{
	if (item == m_pCurrentItem && !bForceReload)
		return;

	if (item)
	{
		if (item->GetLibrary() != m_pLibrary && item->GetLibrary())
			SelectLibrary((CBaseLibrary*)item->GetLibrary());
	}

	m_pCurrentItem = item;

	if (item)
	{
		m_selectedGroup = item->GetGroupName();
	}
	else
		m_selectedGroup = "";

	HTREEITEM hItem = stl::find_in_map(m_itemsToTree, item, (HTREEITEM)0);
	if (hItem)
	{
		GetTreeCtrl().SelectItem(hItem);
		GetTreeCtrl().EnsureVisible(hItem);
	}

	m_pItemManager->SetSelectedItem(item);
}

CBaseLibrary* CDatabaseFrameWnd::FindLibrary(const CString& libraryName)
{
	return (CBaseLibrary*)m_pItemManager->FindLibrary(libraryName);
}

int CDatabaseFrameWnd::GetComboBoxIndex(CBaseLibrary* pLibrary)
{
	int nCount = m_LibraryListComboBox.GetCount();
	for (int i = 0; i < nCount; ++i)
	{
		CBaseLibrary* pItemDataLibrary = (CBaseLibrary*)m_LibraryListComboBox.GetItemDataPtr(i);
		if (pLibrary == pItemDataLibrary)
		{
			return i;
		}
	}

	return -1;
}

CBaseLibrary* CDatabaseFrameWnd::NewLibrary(const CString& libraryName)
{
	return (CBaseLibrary*)m_pItemManager->AddLibrary(libraryName, true);
}

void CDatabaseFrameWnd::DeleteLibrary(CBaseLibrary* pLibrary)
{
	m_pItemManager->DeleteLibrary(pLibrary->GetName());
}

void CDatabaseFrameWnd::DeleteItem(CBaseLibraryItem* pItem)
{
	m_pItemManager->DeleteItem(pItem);
}

void CDatabaseFrameWnd::DeleteAllTreeItems()
{
	std::vector<HTREEITEM> allItems;
	GetAllItems(allItems);

	for (int i = 0, iSize(allItems.size()); i < iSize; ++i)
	{
		HTREEITEM hItem = allItems[i];
		if (hItem == NULL)
			continue;
		GetTreeCtrl().SetItemData(hItem, NULL);
	}

	GetTreeCtrl().DeleteAllItems();
}

HTREEITEM CDatabaseFrameWnd::InsertItemToTree(CBaseLibraryItem* pItem, HTREEITEM hParent)
{
	assert(pItem);
	HTREEITEM hItem = GetTreeCtrl().InsertItem(pItem->GetShortName(), 2, 3, hParent);
	GetTreeCtrl().SetItemData(hItem, (DWORD_PTR)pItem);
	m_itemsToTree[pItem] = hItem;
	return hItem;
}

void CDatabaseFrameWnd::SortItems(SortRecursionType recursionType)
{
	struct Recurser
	{
		static void Sort(CTreeCtrl& treeCtrl, HTREEITEM hItem, int nLevels)
		{
			if (hItem)
			{
				treeCtrl.SortChildren(hItem);
				if (--nLevels > 0)
				{
					HTREEITEM hChildItem = treeCtrl.GetChildItem(hItem);
					while (hChildItem != NULL)
					{
						HTREEITEM hNextItem = treeCtrl.GetNextItem(hChildItem, TVGN_NEXT);
						Recurser::Sort(treeCtrl, hChildItem, nLevels);
						hChildItem = hNextItem;
					}
				}
			}
		}
	};

	if ((int)recursionType > 0)
		Recurser::Sort(GetTreeCtrl(), GetRootItem(), (int)recursionType);
}

void CDatabaseFrameWnd::GetAllItems(std::vector<HTREEITEM>& outItemList) const
{
	HTREEITEM hItem = GetTreeCtrl().GetRootItem();
	while (hItem)
	{
		GetItemsUnderSpecificItem(hItem, outItemList);
		hItem = GetTreeCtrl().GetNextItem(hItem, TVGN_NEXT);
	}
	;
}

void CDatabaseFrameWnd::GetItemsUnderSpecificItem(HTREEITEM hItem, std::vector<HTREEITEM>& outItemList) const
{
	if (hItem)
		outItemList.push_back(hItem);

	if (!GetTreeCtrl().ItemHasChildren(hItem))
		return;

	HTREEITEM hChildItem = GetTreeCtrl().GetChildItem(hItem);
	while (hChildItem)
	{
		GetItemsUnderSpecificItem(hChildItem, outItemList);
		hChildItem = GetTreeCtrl().GetNextItem(hChildItem, TVGN_NEXT);
	}
}

void CDatabaseFrameWnd::OnAddLibrary()
{
	QStringDialog dlg(QObject::tr("New Library Name"));
	CUndo undo("Add Database Library");
	if (dlg.exec() == QDialog::Accepted)
	{
		if (!dlg.GetString().IsEmpty())
		{
			SelectItem(0);
			// Make new library.
			CString library = dlg.GetString();
			NewLibrary(library);
			ReloadLibs();
			SelectLibrary(library);
			GetIEditorImpl()->SetModifiedFlag();
		}
	}
}

void CDatabaseFrameWnd::OnRemoveLibrary()
{
	CString library = m_selectedLib;
	if (library.IsEmpty())
		return;
	if (m_pLibrary->IsModified())
	{
		CString ask;
		ask.Format("Save changes to the library %s?", (const char*)library);

		if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(ask)))
		{
			OnSave();
		}
	}
	CString ask;
	ask.Format("When removing library All items contained in this library will be deleted.\r\nAre you sure you want to remove libarary %s?\r\n(Note: Library file will not be deleted from the disk)", (const char*)library);

	if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(ask)))
	{
		SelectItem(0);
		DeleteLibrary(m_pLibrary);
		m_selectedLib = "";
		m_pLibrary = 0;
		ReleasePreviewControl();
		DeleteAllTreeItems();
		ReloadLibs();
		GetIEditorImpl()->SetModifiedFlag();
	}
}

void CDatabaseFrameWnd::OnAddItem()
{
}

void CDatabaseFrameWnd::OnRemoveItem()
{
	// When we have no set of selected items, it may be the case that we are
	// dealing with old fashioned selection. If such is the case, let's deal
	// with it using the old code system, which should be deprecated.
	if (m_cpoSelectedLibraryItems.empty())
	{
		if (m_pCurrentItem)
		{
			// Remove prototype from prototype manager and library.
			CString str;
			str.Format(_T("Delete %s?"), (const char*)m_pCurrentItem->GetName());
			if (MessageBox(str, _T("Delete Confirmation"), MB_YESNO | MB_ICONQUESTION) == IDYES)
			{
				CUndo undo("Remove library item");
				TSmartPtr<CBaseLibraryItem> pCurrent = m_pCurrentItem;
				DeleteItem(pCurrent);
				HTREEITEM hItem = stl::find_in_map(m_itemsToTree, pCurrent, (HTREEITEM)0);
				if (hItem)
				{
					DeleteTreeItem(hItem);
					m_itemsToTree.erase(pCurrent);
				}
				GetIEditorImpl()->SetModifiedFlag();
				SelectItem(0);
			}
		}
	}
	else // This is to be used when deleting multiple items...
	{
		CString strMessageString("Delete the following items:\n");
		int nItemCount(0);
		std::set<CBaseLibraryItem*>::iterator itCurrentIterator;
		std::set<CBaseLibraryItem*>::iterator itEnd;

		itEnd = m_cpoSelectedLibraryItems.end();
		itCurrentIterator = m_cpoSelectedLibraryItems.begin();
		// For now, we have a maximum limit of 7 items per messagebox...
		for (nItemCount = 0; nItemCount < 7; ++nItemCount, itCurrentIterator++)
		{
			if (itCurrentIterator == itEnd)
			{
				// As there were less than 7 items selected, we got to put them all
				// into the formated string for the messagebox.
				break;
			}
			strMessageString.AppendFormat("%s\n", (*itCurrentIterator)->GetName());
		}
		if (itCurrentIterator != itEnd)
		{
			strMessageString.Append("...");
		}

		if (MessageBox(strMessageString, _T("Delete Confirmation"), MB_YESNO | MB_ICONQUESTION) == IDYES)
		{
			for (itCurrentIterator = m_cpoSelectedLibraryItems.begin(); itCurrentIterator != itEnd; itCurrentIterator++)
			{
				CBaseLibraryItem* pCurrent = *itCurrentIterator;
				DeleteItem(pCurrent);
				HTREEITEM hItem = stl::find_in_map(m_itemsToTree, pCurrent, (HTREEITEM)0);
				if (hItem)
				{
					DeleteTreeItem(hItem);
					m_itemsToTree.erase(pCurrent);
				}
			}
			m_cpoSelectedLibraryItems.clear();
			GetIEditorImpl()->SetModifiedFlag();
			SelectItem(0);
		}
	}
}

void CDatabaseFrameWnd::OnRenameItem()
{
	if (m_pCurrentItem)
	{
		QGroupDialog dlg;
		dlg.SetGroup(m_pCurrentItem->GetGroupName());
		dlg.SetString(m_pCurrentItem->GetShortName());
		if (dlg.exec() == QDialog::Accepted)
		{
			static bool warn = true;
			if (warn)
			{
				warn = false;
				MessageBox("Levels referencing this archetype will need to be exported.", "Warning", MB_OK | MB_ICONEXCLAMATION);
			}

			CUndo undo("Rename library item");
			TSmartPtr<CBaseLibraryItem> curItem = m_pCurrentItem;
			SetItemName(curItem, dlg.GetGroup(), dlg.GetString());
			ReloadItems();
			SelectItem(curItem, true);
			curItem->SetModified();
			//m_pCurrentItem->Update();
		}
		GetIEditorImpl()->SetModifiedFlag();
	}
}

void CDatabaseFrameWnd::OnChangedLibrary()
{
	CString library;
	CBaseLibrary* pBaseLibrary = NULL;
	if (m_LibraryListComboBox)
	{
		pBaseLibrary = (CBaseLibrary*)m_LibraryListComboBox.GetItemDataPtr(m_LibraryListComboBox.GetCurSel());
	}

	CUndo undo("Change database library");
	if (pBaseLibrary && pBaseLibrary != m_pLibrary)
		SelectLibrary(pBaseLibrary);
}

void CDatabaseFrameWnd::OnExportLibrary()
{
	if (!m_pLibrary)
		return;

	CString filename;
	if (CFileUtil::SelectSaveFile("Library XML Files (*.xml)|*.xml", "xml", PathUtil::Make(PathUtil::GetGameFolder(), "Materials").c_str(), filename))
	{
		XmlNodeRef libNode = XmlHelpers::CreateXmlNode("MaterialLibrary");
		m_pLibrary->Serialize(libNode, false);
		XmlHelpers::SaveXmlNode(libNode, filename);
	}
}

void CDatabaseFrameWnd::OnSave()
{
	m_pItemManager->SaveAllLibs();
	m_pLibrary->SetModified(false);
}

void CDatabaseFrameWnd::OnReloadLib()
{
	if (!m_pLibrary)
		return;

	CString libname = m_pLibrary->GetName();
	CString file = m_pLibrary->GetFilename();
	if (m_pLibrary->IsModified())
	{
		CString str;
		str.Format("Layer %s was modified.\nReloading layer will discard all modifications to this library!",
		           (const char*)libname);

		if (QDialogButtonBox::StandardButton::Ok != CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(str), QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel))
		{
			return;
		}
	}

	m_pItemManager->DeleteLibrary(libname);
	m_pItemManager->LoadLibrary(file, true);
	ReloadLibs();
	SelectLibrary(libname);
}

void CDatabaseFrameWnd::OnLoadLibrary()
{
	assert(m_pItemManager);
	CUndo undo("Load Database Library");
	LoadLibrary();
}

void CDatabaseFrameWnd::LoadLibrary()
{
	// Load new material library.
	CString file;
	if (CFileUtil::SelectSingleFile(EFILE_TYPE_XML, file, "", PathUtil::GamePathToCryPakPath(m_pItemManager->GetLibsPath().GetString()).c_str()))
	{
		if (!file.IsEmpty())
		{
			GetIEditorImpl()->GetIUndoManager()->Suspend();
			IDataBaseLibrary* matLib = m_pItemManager->LoadLibrary(file);
			GetIEditorImpl()->GetIUndoManager()->Resume();
			ReloadLibs();
			if (matLib)
				SelectLibrary((CBaseLibrary*)matLib);
		}
	}
}

bool CDatabaseFrameWnd::SetItemName(CBaseLibraryItem* item, const CString& groupName, const CString& itemName)
{
	assert(item);
	// Make prototype name.
	string name;
	if (!groupName.IsEmpty())
		name = groupName + ".";
	name += itemName;
	string fullName = name;
	if (item->GetLibrary())
		fullName = item->GetLibrary()->GetName() + "." + name;
	IDataBaseItem* pOtherItem = m_pItemManager->FindItemByName(fullName);
	if (pOtherItem && pOtherItem != item)
	{
		// Ensure uniqness of name.
		Warning("Duplicate Item Name %s", (const char*)name);
		return false;
	}
	else
	{
		item->SetName(name);
	}
	return true;
}

void CDatabaseFrameWnd::OnSelChangedItemTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (m_bIgnoreSelectionChange)
		return;
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);

	bool bSelected = false;
	m_pCurrentItem = 0;
	if (GetTreeCtrl())
	{
		HTREEITEM hItem = pNMTreeView->itemNew.hItem;
		if (hItem != 0 && hItem != GetRootItem())
		{
			// Change currently selected item.
			CBaseLibraryItem* prot = (CBaseLibraryItem*)GetTreeCtrl().GetItemData(hItem);
			if (prot)
			{
				SelectItem(prot);
				bSelected = true;
			}
			else
			{
				m_selectedGroup = GetTreeCtrl().GetItemText(hItem);
			}
		}
	}
	if (!bSelected)
		SelectItem(0);

	*pResult = 0;
}

void CDatabaseFrameWnd::OnKeyDownItemTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	GetAsyncKeyState(VK_CONTROL);
	bool bCtrl = GetAsyncKeyState(VK_CONTROL) != 0;
	// Key press in items tree view.
	NMTVKEYDOWN* nm = (NMTVKEYDOWN*)pNMHDR;
	if (bCtrl && (nm->wVKey == 'c' || nm->wVKey == 'C'))
	{
		OnCopy(); // Ctrl+C
	}
	if (bCtrl && (nm->wVKey == 'v' || nm->wVKey == 'V'))
	{
		OnPaste(); // Ctrl+V
	}
	if (bCtrl && (nm->wVKey == 'x' || nm->wVKey == 'X'))
	{
		OnCut(); // Ctrl+X
	}
	if (bCtrl && (nm->wVKey == 'd' || nm->wVKey == 'D'))
	{
		OnClone(); // Ctrl+D
	}
	if (nm->wVKey == VK_DELETE)
	{
		OnRemoveItem();
	}
	if (nm->wVKey == VK_INSERT)
	{
		OnAddItem();
	}
}

void CDatabaseFrameWnd::OnUpdateSelected(CCmdUI* pCmdUI)
{
	if (m_pCurrentItem)
		pCmdUI->Enable(TRUE);
	else
		pCmdUI->Enable(FALSE);
}

void CDatabaseFrameWnd::OnUpdatePaste(CCmdUI* pCmdUI)
{
	CClipboard clipboard;
	if (clipboard.IsEmpty())
		pCmdUI->Enable(FALSE);
	else
		pCmdUI->Enable(TRUE);
}

void CDatabaseFrameWnd::OnCut()
{
	if (m_pCurrentItem)
	{
		OnCopy();
		OnRemoveItem();
	}
}

void CDatabaseFrameWnd::OnClone()
{
	OnCopy();
	OnPaste();
}

void CDatabaseFrameWnd::DoesItemExist(const string& itemName, bool& bOutExist) const
{
	for (int i = 0, iItemCount(m_pLibrary->GetItemCount()); i < iItemCount; ++i)
	{
		IDataBaseItem* pItem = m_pLibrary->GetItem(i);
		if (pItem == NULL)
			continue;
		if (pItem->GetName() == itemName)
		{
			bOutExist = true;
			return;
		}
	}
	bOutExist = false;
}

string CDatabaseFrameWnd::MakeValidName(const string& candidateName, Functor2<const string&, bool&>& FuncForCheck) const
{
	bool bCheck = false;
	FuncForCheck(candidateName, bCheck);
	if (!bCheck)
		return candidateName;

	int nCounter = 0;
	const int nEnoughBigNumber = 1000000;
	do
	{
		string counterBuffer;
		counterBuffer.Format("%d", nCounter);
		string newName = candidateName + counterBuffer;
		FuncForCheck(newName, bCheck);
		if (!bCheck)
			return newName;
	}
	while (nCounter++ < nEnoughBigNumber);

	assert(0 && "CDatabaseFrameWnd::MakeValidName()");
	return candidateName;
}

void CDatabaseFrameWnd::DoesGroupExist(const string& groupName, bool& bOutExist) const
{
	HTREEITEM hRootItem = GetTreeCtrl().GetRootItem();
	while (hRootItem)
	{
		if (groupName == GetTreeCtrl().GetItemText(hRootItem).GetString())
		{
			bOutExist = true;
			return;
		}
		hRootItem = GetTreeCtrl().GetNextSiblingItem(hRootItem);
	}
	bOutExist = false;
}

void CDatabaseFrameWnd::OnSetModifedCurrentLibrary(bool modified)
{
	int nCurSel = m_LibraryListComboBox.GetCurSel();
	CBaseLibrary* pItemDataLibrary = (CBaseLibrary*)m_LibraryListComboBox.GetItemDataPtr(nCurSel);
	if (m_pLibrary != pItemDataLibrary)
		return;

	CString szModifiedText = "";
	CString szCurrentText = "";
	m_LibraryListComboBox.GetLBText(nCurSel, szCurrentText);
	if (modified)
	{
		szModifiedText = pItemDataLibrary->GetName() + "*";
	}
	else
	{
		szModifiedText = pItemDataLibrary->GetName();
	}

	//change combox item text.
	if (szModifiedText != szCurrentText)
	{
		m_LibraryListComboBox.DeleteString(nCurSel);
		m_LibraryListComboBox.InsertString(nCurSel, szModifiedText);
		m_LibraryListComboBox.SetItemDataPtr(nCurSel, pItemDataLibrary);
		m_LibraryListComboBox.SendMessage(CB_SETCURSEL, nCurSel, 0);
		m_selectedLib = szModifiedText;
	}
}

void CDatabaseFrameWnd::DeleteTreeItem(HTREEITEM hItem)
{
	if (hItem == NULL)
		return;
	GetTreeCtrl().DeleteItem(hItem);
}

void CDatabaseFrameWnd::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnBeginNewScene:
		m_bLibsLoaded = false;
		// Clear all prototypes and libraries.
		SelectItem(0);
		if (m_LibraryListComboBox)
		{
			m_LibraryListComboBox.ResetContent();
			m_LibraryListComboBox.SetDroppedWidth(LIBRARY_CB_WIDTH);
		}
		m_selectedLib = "";
		break;

	case eNotify_OnEndSceneOpen:
		m_bLibsLoaded = false;
		ReloadLibs();
		break;

	case eNotify_OnClearLevelContents:
		{
			m_bLibsLoaded = false;
			CUndo undo("Close Database Library");
			SelectLibrary("");
			SelectItem(0);
		}
		break;

	case eNotify_OnDataBaseUpdate:
		if (m_pLibrary && m_pLibrary->IsModified())
			ReloadItems();
		break;
	case eNotify_OnIdleUpdate:
		if (m_pLibrary)
			OnSetModifedCurrentLibrary(m_pLibrary->IsModified());
		break;
	}
}

