// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DataBaseDialog.h"
#include "Controls/HotTrackingTreeCtrl.h"

class CBaseLibrary;
class CBaseLibraryItem;
class CBaseLibraryManager;

#define IDC_LIBRARY_ITEMS_TREE AFX_IDW_PANE_FIRST

/** Base class for all BasLibrary base dialogs.
    Provides common methods for handling library items.
 */
class CBaseLibraryDialog : public CDataBaseDialogPage, public IEditorNotifyListener
{
	DECLARE_DYNAMIC(CBaseLibraryDialog);
public:
	CBaseLibraryDialog(UINT nID, CWnd* pParent);
	~CBaseLibraryDialog();

	//! Reload all data in dialog.
	virtual void Reload();

	// Called every frame.
	virtual void Update();

	//! This dialog is activated.
	virtual void SetActive(bool bActive);
	virtual void SelectLibrary(const string& library, bool bForceSelect = false);
	virtual void SelectItem(CBaseLibraryItem* item, bool bForceReload = false);
	virtual bool CanSelectItem(CBaseLibraryItem* pItem);
	virtual bool CanRemoveEmptyFolder() const { return false; }

	//! Returns menu for this dialog.
	virtual UINT  GetDialogMenuID() { return 0; };

	CBaseLibrary* GetCurrentLibrary()
	{
		return m_pLibrary;
	}

protected:
	virtual void         OnOK()     {};
	virtual void         OnCancel() {};

	virtual void         LoadLibrary();

	void                 DoDataExchange(CDataExchange* pDX);
	BOOL                 OnInitDialog();
	afx_msg void         OnDestroy();
	afx_msg void         OnSize(UINT nType, int cx, int cy);
	afx_msg void         OnSelChangedItemTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void         OnKeyDownItemTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void         OnUpdateSelected(CCmdUI* pCmdUI);
	afx_msg void         OnUpdatePaste(CCmdUI* pCmdUI);

	virtual afx_msg void OnAddLibrary();
	virtual afx_msg void OnRemoveLibrary();
	virtual afx_msg void OnAddItem();
	virtual afx_msg void OnRemoveItem();
	virtual afx_msg void OnRenameItem();
	virtual afx_msg void OnChangedLibrary();
	virtual afx_msg void OnExportLibrary();
	virtual afx_msg void OnSave();
	virtual afx_msg void OnReloadLib();
	virtual afx_msg void OnLoadLibrary();

	//////////////////////////////////////////////////////////////////////////
	// Must be overloaded in derived classes.
	//////////////////////////////////////////////////////////////////////////
	virtual CBaseLibrary* FindLibrary(const string& libraryName);
	virtual CBaseLibrary* NewLibrary(const string& libraryName);
	virtual void          DeleteLibrary(CBaseLibrary* pLibrary);
	virtual void          DeleteItem(CBaseLibraryItem* pItem);

	//////////////////////////////////////////////////////////////////////////
	// Some functions can be overridden to modify standard functionality.
	//////////////////////////////////////////////////////////////////////////
	virtual void      InitToolbar(UINT nToolbarResID);
	virtual void      InitLibraryToolbar();
	virtual void      InitItemToolbar();
	virtual void      ReloadLibs();
	virtual void      ReloadItems();
	virtual HTREEITEM InsertItemToTree(CBaseLibraryItem* pItem, HTREEITEM hParent);
	virtual bool      SetItemName(CBaseLibraryItem* item, const string& groupName, const string& itemName);

	//////////////////////////////////////////////////////////////////////////
	// IEditorNotifyListener listener implementation
	//////////////////////////////////////////////////////////////////////////
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Copying and cloning of items.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnCopy() = 0;
	virtual void OnPaste() = 0;
	virtual void OnCut();
	virtual void OnClone();
	virtual void OnCopyPath();

	afx_msg void OnUndo() { GetIEditorImpl()->GetIUndoManager()->Undo(); }
	afx_msg void OnRedo() { GetIEditorImpl()->GetIUndoManager()->Redo(); }

	bool         IsExistItem(const string& itemName) const;
	string       MakeValidName(const string& candidateName) const;

	string       GetItemFullName(HTREEITEM hItem, string const& sep);

	enum SortRecursionType
	{
		SORT_RECURSION_NONE = 1,
		SORT_RECURSION_ITEM = 2,
		SORT_RECURSION_FULL = 9999
	};

	void               SortItems(SortRecursionType recursionType);
	virtual void       DeleteTreeItem(HTREEITEM hItem);
	virtual void       DeleteAllTreeItems();
	virtual void       ReleasePreviewControl() {}

	virtual CTreeCtrl& GetTreeCtrl()
	{
		return m_treeCtrl;
	}

	virtual HTREEITEM GetRootItem()
	{
		return TVI_ROOT;
	}

protected:
	DECLARE_MESSAGE_MAP()

	// Dialog Toolbar.
	CDlgToolBar m_toolbar;

	// Tree control.
	CHotTrackingTreeCtrl m_treeCtrl;
	// Library control.
	CComboBox            m_libraryCtrl;

	// Map of library items to tree ctrl.
	typedef std::map<CBaseLibraryItem*, HTREEITEM> ItemsToTreeMap;
	ItemsToTreeMap m_itemsToTree;

	//CXTToolBar m_toolbar;
	// Name of currently selected library.
	string m_selectedLib;
	string m_selectedGroup;
	bool    m_bLibsLoaded;

	//! Selected library.
	TSmartPtr<CBaseLibrary> m_pLibrary;

	//! Last selected Item. (kept here for compatibility reasons)
	// See comments on m_cpoSelectedLibraryItems for more details.
	TSmartPtr<CBaseLibraryItem> m_pCurrentItem;

	// A set containing all the currently selected items
	// (it's disabled for MOST, but not ALL cases).
	// This should be the new standard way of storing selections as
	// opposed to the former mean, it allows us to store multiple selections.
	// The migration to this new style should be done according to the needs
	// for multiple selection.
	std::set<CBaseLibraryItem*> m_cpoSelectedLibraryItems;

	//! Pointer to item manager.
	CBaseLibraryManager* m_pItemManager;

	//////////////////////////////////////////////////////////////////////////
	// Dragging support.
	//////////////////////////////////////////////////////////////////////////
	CBaseLibraryItem* m_pDraggedItem;
	CImageList*       m_dragImage;

	bool              m_bIgnoreSelectionChange;

	HCURSOR           m_hCursorDefault;
	HCURSOR           m_hCursorNoDrop;
	HCURSOR           m_hCursorCreate;
	HCURSOR           m_hCursorReplace;
	SortRecursionType m_sortRecursionType;
};

