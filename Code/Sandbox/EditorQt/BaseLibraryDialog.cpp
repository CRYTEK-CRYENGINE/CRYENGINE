// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BaseLibraryDialog.h"

#include "Dialogs/QStringDialog.h"
#include "Dialogs/QGroupDialog.h"

#include "BaseLibrary.h"
#include "BaseLibraryItem.h"
#include "BaseLibraryManager.h"
#include "Util/Clipboard.h"
#include "Controls/QuestionDialog.h"
#include "Controls/SharedFonts.h"
#include "FilePathUtil.h"

#define LIBRARY_CB_WIDTH 150

IMPLEMENT_DYNAMIC(CBaseLibraryDialog, CToolbarDialog)

//////////////////////////////////////////////////////////////////////////
// CBaseLibraryDialog implementation.
//////////////////////////////////////////////////////////////////////////
CBaseLibraryDialog::CBaseLibraryDialog(UINT nID, CWnd* pParent)
	: CDataBaseDialogPage(nID, pParent)
{
	m_sortRecursionType = SORT_RECURSION_FULL;

	m_bIgnoreSelectionChange = false;

	m_pItemManager = 0;

	// Load cusrors.
	m_hCursorDefault = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
	m_hCursorNoDrop = AfxGetApp()->LoadCursor(IDC_NODROP);
	m_hCursorCreate = AfxGetApp()->LoadCursor(IDC_HIT_CURSOR);
	m_hCursorReplace = AfxGetApp()->LoadCursor(IDC_HAND_INTERNAL);
	m_bLibsLoaded = false;

	GetIEditorImpl()->RegisterNotifyListener(this);
}

CBaseLibraryDialog::~CBaseLibraryDialog()
{
	GetIEditorImpl()->UnregisterNotifyListener(this);
}

void CBaseLibraryDialog::DoDataExchange(CDataExchange* pDX)
{
	CToolbarDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CBaseLibraryDialog, CToolbarDialog)
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
ON_COMMAND(ID_DB_COPY_PATH, OnCopyPath)
ON_UPDATE_COMMAND_UI(ID_DB_COPY, OnUpdateSelected)
ON_UPDATE_COMMAND_UI(ID_DB_CUT, OnUpdateSelected)
ON_UPDATE_COMMAND_UI(ID_DB_CLONE, OnUpdateSelected)
ON_UPDATE_COMMAND_UI(ID_DB_PASTE, OnUpdatePaste)
ON_COMMAND(ID_UNDO, OnUndo)
ON_COMMAND(ID_REDO, OnRedo)

ON_WM_SIZE()
ON_WM_DESTROY()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnDestroy()
{
	SelectItem(0);
	m_pLibrary = 0;
	m_pCurrentItem = 0;

	m_cpoSelectedLibraryItems.clear();
	m_itemsToTree.clear();

	CToolbarDialog::OnDestroy();
}

// CTVSelectKeyDialog message handlers
BOOL CBaseLibraryDialog::OnInitDialog()
{
	// Initialize command bars.
	VERIFY(InitCommandBars());
	GetCommandBars()->GetCommandBarsOptions()->bShowExpandButtonAlways = FALSE;
	GetCommandBars()->EnableCustomization(FALSE);

	__super::OnInitDialog();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::InitLibraryToolbar()
{
	CRect rc;
	GetClientRect(rc);
	// Create Library toolbar.
	CXTPToolBar* pLibToolBar = GetCommandBars()->Add(_T("ToolBar"), xtpBarTop);
	//pLibToolBar->EnableCustomization(FALSE);
	VERIFY(pLibToolBar->LoadToolBar(IDR_DB_LIBRARY_BAR));

	// Create library control.
	{
		CXTPControl* pCtrl = pLibToolBar->GetControls()->FindControl(xtpControlButton, ID_DB_LIBRARY, TRUE, FALSE);
		if (pCtrl)
		{
			CRect rc(0, 0, LIBRARY_CB_WIDTH, 16);
			m_libraryCtrl.Create(WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_SORT | WS_VSCROLL, rc, this, ID_DB_LIBRARY);
			//m_libraryCtrl.SetParent( this );
			//m_libraryCtrl.SetFont( CFont::FromHandle((HFONT)SMFCFonts::GetInstance().hSystemFont) );

			CXTPControlCustom* pCustomCtrl = (CXTPControlCustom*)pLibToolBar->GetControls()->SetControlType(pCtrl->GetIndex(), xtpControlCustom);
			pCustomCtrl->SetSize(CSize(LIBRARY_CB_WIDTH, 16));
			pCustomCtrl->SetControl(&m_libraryCtrl);
			pCustomCtrl->SetFlags(xtpFlagManualUpdate);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Standard toolbar.
	CXTPToolBar* pStdToolBar = GetCommandBars()->Add(_T("StandartToolBar"), xtpBarTop);
	pStdToolBar->EnableCustomization(FALSE);
	VERIFY(pStdToolBar->LoadToolBar(IDR_DB_STANDART));
	DockRightOf(pStdToolBar, pLibToolBar);

	//////////////////////////////////////////////////////////////////////////
	// Create Item toolbar.
	CXTPToolBar* pItemToolBar = GetCommandBars()->Add(_T("ItemToolBar"), xtpBarTop);
	pItemToolBar->EnableCustomization(FALSE);
	VERIFY(pItemToolBar->LoadToolBar(IDR_DB_LIBRARY_ITEM_BAR));
	DockRightOf(pItemToolBar, pLibToolBar);
	//////////////////////////////////////////////////////////////////////////

}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::InitItemToolbar()
{
}

//////////////////////////////////////////////////////////////////////////
// Create the toolbar
void CBaseLibraryDialog::InitToolbar(UINT nToolbarResID)
{
	// Tool bar must be already created in derived class.
	ASSERT(m_toolbar.m_hWnd);

	// Resize the toolbar
	CRect rc;
	GetClientRect(rc);
	m_toolbar.SetWindowPos(NULL, 0, 0, rc.right, 70, SWP_NOZORDER);
	CSize sz = m_toolbar.CalcDynamicLayout(TRUE, TRUE);

	//////////////////////////////////////////////////////////////////////////
	int index;
	index = m_toolbar.CommandToIndex(ID_DB_LIBRARY);
	if (index >= 0)
	{
		m_toolbar.SetButtonInfo(index, ID_DB_LIBRARY, TBBS_SEPARATOR, 150);
		m_toolbar.GetItemRect(index, &rc);
		rc.top++;
		rc.bottom += 400;
	}
	m_libraryCtrl.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | CBS_SORT, rc, this, ID_DB_LIBRARY);
	m_libraryCtrl.SetParent(&m_toolbar);
	m_libraryCtrl.SetFont(CFont::FromHandle((HFONT)SMFCFonts::GetInstance().hSystemFont));
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnSize(UINT nType, int cx, int cy)
{
	CToolbarDialog::OnSize(nType, cx, cy);

	// resize splitter window.
	if (m_toolbar.m_hWnd)
	{
		CRect rc;
		GetClientRect(rc);
		//m_toolbar.SetWindowPos(NULL, 0, 0, rc.right, 70, SWP_NOZORDER);
		m_toolbar.MoveWindow(0, 0, rc.right, 70, FALSE);
		RecalcLayout();
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnBeginNewScene:
		m_bLibsLoaded = false;
		// Clear all prototypes and libraries.
		SelectItem(0);
		if (m_libraryCtrl)
		{
			m_libraryCtrl.ResetContent();
			m_libraryCtrl.SetDroppedWidth(LIBRARY_CB_WIDTH);
		}
		m_selectedLib = "";
		break;

	case eNotify_OnEndSceneOpen:
		m_bLibsLoaded = false;
		ReloadLibs();
		break;

	case eNotify_OnClearLevelContents:
		m_bLibsLoaded = false;
		SelectLibrary("");
		SelectItem(0);
		break;

	case eNotify_OnDataBaseUpdate:
		if (m_pLibrary && m_pLibrary->IsModified())
			ReloadItems();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
CBaseLibrary* CBaseLibraryDialog::FindLibrary(const string& libraryName)
{
	return (CBaseLibrary*)m_pItemManager->FindLibrary(libraryName);
}

CBaseLibrary* CBaseLibraryDialog::NewLibrary(const string& libraryName)
{
	return (CBaseLibrary*)m_pItemManager->AddLibrary(libraryName, true);
}

void CBaseLibraryDialog::DeleteLibrary(CBaseLibrary* pLibrary)
{
	m_pItemManager->DeleteLibrary(pLibrary->GetName());
}

void CBaseLibraryDialog::DeleteItem(CBaseLibraryItem* pItem)
{
	m_pItemManager->DeleteItem(pItem);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::ReloadLibs()
{
	LOADING_TIME_PROFILE_SECTION;
	if (!m_pItemManager)
		return;

	SelectItem(0);

	string selectedLib;
	if (m_libraryCtrl)
	{
		m_libraryCtrl.ResetContent();
		m_libraryCtrl.SetDroppedWidth(LIBRARY_CB_WIDTH);
	}
	bool bFound = false;
	for (int i = 0; i < m_pItemManager->GetLibraryCount(); i++)
	{
		CString library = m_pItemManager->GetLibrary(i)->GetName().GetString();
		if (selectedLib.IsEmpty())
			selectedLib = library;
		if (m_libraryCtrl)
		{
			m_libraryCtrl.AddString(library);
			if (m_libraryCtrl.GetDC())
			{
				int nWidth = (m_libraryCtrl.GetDC()->GetTextExtent(library)).cx + ::GetSystemMetrics(SM_CXVSCROLL) + 2 * ::GetSystemMetrics(SM_CXEDGE);
				if (m_libraryCtrl.GetDroppedWidth() < nWidth)
					m_libraryCtrl.SetDroppedWidth(nWidth);
			}
		}
	}
	m_selectedLib = "";
	SelectLibrary(selectedLib);
	m_bLibsLoaded = true;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::ReloadItems()
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

	HTREEITEM hLibItem = GetRootItem();

	m_bIgnoreSelectionChange = true;

	std::map<string, HTREEITEM, stl::less_stricmp<string>> items;
	for (int i = 0; i < m_pLibrary->GetItemCount(); i++)
	{
		CBaseLibraryItem* pItem = (CBaseLibraryItem*)m_pLibrary->GetItem(i);
		string name = pItem->GetName();

		// Insert only top-level items here; children are handled by InsertItemToTree.
		int pos = name.ReverseFind('.');
		if (pos > 0)
		{
			string parentName = name.Left(pos);
			if (m_pLibrary->FindItem(parentName))
				continue;
		}

		HTREEITEM hRoot = hLibItem;
		char* token;
		char itempath[1024];
		cry_strcpy(itempath, name);

		token = strtok(itempath, "\\/.");

		string itemName;
		while (token)
		{
			string strToken = token;

			token = strtok(NULL, "\\/.");
			if (!token)
				break;

			itemName += strToken + ".";

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

//////////////////////////////////////////////////////////////////////////
HTREEITEM CBaseLibraryDialog::InsertItemToTree(CBaseLibraryItem* pItem, HTREEITEM hParent)
{
	assert(pItem);
	HTREEITEM hItem = GetTreeCtrl().InsertItem(pItem->GetShortName(), 2, 3, hParent);
	//GetTreeCtrl().SetItemState( hItem, TVIS_BOLD,TVIS_BOLD );
	GetTreeCtrl().SetItemData(hItem, (DWORD_PTR)pItem);
	m_itemsToTree[pItem] = hItem;
	return hItem;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::SelectLibrary(const string& library, bool bForceSelect)
{
	CWaitCursor wait;
	if (m_selectedLib != library || bForceSelect)
	{
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
	if (m_libraryCtrl)
		m_libraryCtrl.SelectString(-1, m_selectedLib);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnChangedLibrary()
{
	CString library;
	if (m_libraryCtrl)
		m_libraryCtrl.GetWindowText(library);
	if (library != m_selectedLib.GetString())
	{
		SelectLibrary(library.GetString());
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnAddLibrary()
{
	QStringDialog dlg(QObject::tr("New Library Name"), nullptr, false, true);

	dlg.SetCheckCallback([this](const string& library) -> bool
	{
		string path = m_pItemManager->MakeFilename(library).GetString();
		if (CFileUtil::FileExists(path))
		{
		  CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, string("Library '") + library + "' already exists.");
		  return false;
		}
		return true;
	});

	if (dlg.exec() == QDialog::Accepted)
	{
		if (!dlg.GetString().IsEmpty())
		{
			SelectItem(0);
			// Make new library.
			string library = dlg.GetString();
			NewLibrary(library);
			ReloadLibs();
			SelectLibrary(library);
			GetIEditorImpl()->SetModifiedFlag();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnExportLibrary()
{
	if (!m_pLibrary)
		return;

	string filename;
	if (CFileUtil::SelectSaveFile("Library XML Files (*.xml)|*.xml", "xml", PathUtil::Make(PathUtil::GetGameFolder(), "Materials").c_str(), filename))
	{
		XmlNodeRef libNode = XmlHelpers::CreateXmlNode("MaterialLibrary");
		m_pLibrary->Serialize(libNode, false);
		XmlHelpers::SaveXmlNode(libNode, filename);
	}
}

bool CBaseLibraryDialog::SetItemName(CBaseLibraryItem* item, const string& groupName, const string& itemName)
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

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnAddItem()
{
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnRemoveItem()
{
	// When we have no set of selected items, it may be the case that we are
	// dealing with old fashioned selection. If such is the case, let's deal
	// with it using the old code system, which should be deprecated.
	if (m_cpoSelectedLibraryItems.empty())
	{
		if (m_pCurrentItem)
		{
			// Remove prototype from prototype manager and library.
			string str;
			str.Format(_T("Delete %s?"), (const char*)m_pCurrentItem->GetName());
			if (MessageBox(str, _T("Delete Confirmation"), MB_YESNO | MB_ICONQUESTION) == IDYES)
			{
				CUndo undo("Remove library item");
				TSmartPtr<CBaseLibraryItem> pCurrent = m_pCurrentItem;
				DeleteItem(pCurrent);
				HTREEITEM hItem = stl::find_in_map(m_itemsToTree, pCurrent, (HTREEITEM)0);
				HTREEITEM hParentItem = m_treeCtrl.GetParentItem(hItem);
				if (hItem)
				{
					DeleteTreeItem(hItem);
					m_itemsToTree.erase(pCurrent);
				}
				GetIEditorImpl()->SetModifiedFlag();
				SelectItem((CBaseLibraryItem*)m_treeCtrl.GetItemData(hParentItem));
				if (CanRemoveEmptyFolder())
				{
					while (hParentItem && !m_treeCtrl.ItemHasChildren(hParentItem))
					{
						hItem = hParentItem;
						hParentItem = m_treeCtrl.GetParentItem(hItem);
						DeleteTreeItem(hItem);
					}
				}
			}
		}
	}
	else // This is to be used when deleting multiple items...
	{
		string strMessageString("Delete the following items:\n");
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

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnRenameItem()
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
			SetItemName(curItem, dlg.GetGroup().GetString(), dlg.GetString().GetString());
			ReloadItems();
			SelectItem(curItem, true);
			curItem->SetModified();
			//m_pCurrentItem->Update();
		}
		GetIEditorImpl()->SetModifiedFlag();
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnRemoveLibrary()
{
	string library = m_selectedLib;
	if (library.IsEmpty())
		return;
	if (m_pLibrary->IsModified())
	{
		string ask;
		ask.Format("Save changes to the library %s?", (const char*)library);
		if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(ask)))
		{
			OnSave();
		}
	}
	string ask;
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

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnSelChangedItemTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (m_bIgnoreSelectionChange)
		return;
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);

	// For the time being this is deactivated due to lack of testing.
	//// This might be true always.
	//if (pNMTreeView->itemNew.mask&TVIF_STATE)
	//{
	//	HTREEITEM					hCurrentItem(pNMTreeView->itemNew.hItem);
	//	CBaseLibraryItem* poBaseTreeItem((CBaseLibraryItem*)GetTreeCtrl().GetItemData(hCurrentItem));

	//	// When the item was not selected and will be selected now...
	//	if (pNMTreeView->itemNew.state&TVIS_SELECTED)
	//	{
	//		m_cpoSelectedLibraryItems.insert(poBaseTreeItem);
	//	}
	//	else // If the item was selected and it will not be selected anymore...
	//	{
	//		m_cpoSelectedLibraryItems.erase(poBaseTreeItem);
	//	}
	//}

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
				m_selectedGroup = GetItemFullName(hItem, ".");
			}
		}
	}
	if (!bSelected)
		SelectItem(0);

	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnKeyDownItemTree(NMHDR* pNMHDR, LRESULT* pResult)
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

//////////////////////////////////////////////////////////////////////////
bool CBaseLibraryDialog::CanSelectItem(CBaseLibraryItem* pItem)
{
	assert(pItem);
	
	//The manager may not have a uid to item map, need to search both by uid and by name.
	if (m_pItemManager->HasUidMap() && m_pItemManager->FindItem(pItem->GetGUID()) == pItem)
		return true;

	if (m_pItemManager->HasNameMap() && m_pItemManager->FindItemByName(pItem->GetFullName()) == pItem)
		return true;

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::SelectItem(CBaseLibraryItem* item, bool bForceReload)
{
	if (item == m_pCurrentItem && !bForceReload)
		return;

	if (item)
	{
		// Selecting item from different library.
		if (item->GetLibrary() != m_pLibrary && item->GetLibrary())
		{
			// Select library first.
			SelectLibrary(item->GetLibrary()->GetName());
		}
	}

	m_pCurrentItem = item;

	if (item)
	{
		m_selectedGroup = item->GetGroupName();
	}
	else
		m_selectedGroup = "";

	m_pCurrentItem = item;

	// Set item visible.
	HTREEITEM hItem = stl::find_in_map(m_itemsToTree, item, (HTREEITEM)0);
	if (hItem)
	{
		GetTreeCtrl().SelectItem(hItem);
		GetTreeCtrl().EnsureVisible(hItem);
	}

	// [MichaelS 17/4/2007] Select this item in the manager so that other systems
	// can find out the selected item without having to access the ui directly.
	m_pItemManager->SetSelectedItem(item);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::Update()
{
	// do nothing here.
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnSave()
{
	m_pItemManager->SaveAllLibs();
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::Reload()
{
	ReloadLibs();
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::SetActive(bool bActive)
{
	if (bActive && !m_bLibsLoaded)
	{
		ReloadLibs();
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnUpdateSelected(CCmdUI* pCmdUI)
{
	if (m_pCurrentItem)
		pCmdUI->Enable(TRUE);
	else
		pCmdUI->Enable(FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnUpdatePaste(CCmdUI* pCmdUI)
{
	CClipboard clipboard;
	if (clipboard.IsEmpty())
		pCmdUI->Enable(FALSE);
	else
		pCmdUI->Enable(TRUE);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnClone()
{
	OnCopy();
	OnPaste();
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnCopyPath()
{
	if (!m_pCurrentItem)
		return;

	string str;
	if (m_pCurrentItem->GetLibrary())
		str = m_pCurrentItem->GetLibrary()->GetName() + ".";
	str += m_pCurrentItem->GetName();

	CClipboard clipboard;
	clipboard.PutString(str);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnCut()
{
	if (m_pCurrentItem)
	{
		OnCopy();
		OnRemoveItem();
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnReloadLib()
{
	if (!m_pLibrary)
		return;

	string libname = m_pLibrary->GetName();
	string file = m_pLibrary->GetFilename();
	if (m_pLibrary->IsModified())
	{
		string str;
		str.Format("Layer %s was modified.\nReloading layer will discard all modifications to this library!", (const char*)libname);

		if (QDialogButtonBox::StandardButton::Yes != CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(str)))
		{
			return;
		}
	}

	m_pItemManager->DeleteLibrary(libname);
	m_pItemManager->LoadLibrary(file, true);
	ReloadLibs();
	SelectLibrary(libname);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnLoadLibrary()
{
	assert(m_pItemManager);
	LoadLibrary();
}

void CBaseLibraryDialog::LoadLibrary()
{
	// Load new material library.
	std::vector<string> files;
	if (CFileUtil::SelectMultipleFiles(EFILE_TYPE_XML, files, "", PathUtil::GamePathToCryPakPath(m_pItemManager->GetLibsPath().GetString()).c_str()))
	{
		int selectedFiles = files.size();
		if (selectedFiles > 0)
		{
			GetIEditorImpl()->GetIUndoManager()->Suspend();
			IDataBaseLibrary* matLib = nullptr;

			for (int i = 0; i < selectedFiles; ++i)
			{
				matLib = m_pItemManager->LoadLibrary(files[i]);
			}

			GetIEditorImpl()->GetIUndoManager()->Resume();
			ReloadLibs();

			if (matLib)
			{
				SelectLibrary(matLib->GetName());
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
string CBaseLibraryDialog::GetItemFullName(HTREEITEM hItem, string const& sep)
{
	string name = m_treeCtrl.GetItemText(hItem);
	HTREEITEM hParent = hItem;
	while (hParent)
	{
		hParent = m_treeCtrl.GetParentItem(hParent);
		if (hParent)
		{
			name = m_treeCtrl.GetItemText(hParent).GetString() + sep + name;
		}
	}
	return name;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::SortItems(SortRecursionType recursionType)
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

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::DeleteAllTreeItems()
{
	HTREEITEM pVisibleItem(GetTreeCtrl().GetFirstVisibleItem());

	while (pVisibleItem)
	{
		GetTreeCtrl().SetItemData(pVisibleItem, NULL);
		pVisibleItem = GetTreeCtrl().GetNextVisibleItem(pVisibleItem);
	}

	GetTreeCtrl().DeleteAllItems();
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::DeleteTreeItem(HTREEITEM hItem)
{
	if (hItem == NULL)
		return;
	GetTreeCtrl().DeleteItem(hItem);
}

//////////////////////////////////////////////////////////////////////////
bool CBaseLibraryDialog::IsExistItem(const string& itemName) const
{
	for (int i = 0, iItemCount(m_pLibrary->GetItemCount()); i < iItemCount; ++i)
	{
		IDataBaseItem* pItem = m_pLibrary->GetItem(i);
		if (pItem == NULL)
			continue;
		if (pItem->GetName() == itemName)
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
string CBaseLibraryDialog::MakeValidName(const string& candidateName) const
{
	if (!IsExistItem(candidateName))
		return candidateName;

	int nCounter = 0;
	const int nEnoughBigNumber = 1000000;
	do
	{
		string counterBuffer;
		counterBuffer.Format("%d", nCounter);
		string newName = candidateName + counterBuffer;
		if (!IsExistItem(newName))
			return newName;
	}
	while (nCounter++ < nEnoughBigNumber);

	assert(0 && "CBaseLibraryDialog::MakeValidName()");
	return candidateName;
}

