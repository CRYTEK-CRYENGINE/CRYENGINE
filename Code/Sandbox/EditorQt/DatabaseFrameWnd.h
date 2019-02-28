// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2011.
// -------------------------------------------------------------------------
//  File name:   DatabaseFrameWnd.h
//  Created:     10/Dec/2012 by Jaesik.
////////////////////////////////////////////////////////////////////////////

#include "Dialogs/BaseFrameWnd.h"
class CBaseLibrary;
class CBaseLibraryItem;
class CBaseLibraryManager;

#define IDC_LIBRARY_ITEMS_TREE AFX_IDW_PANE_FIRST

class CDatabaseFrameWnd : public CBaseFrameWnd, public IEditorNotifyListener
{
	DECLARE_DYNAMIC(CDatabaseFrameWnd)

public:
	CDatabaseFrameWnd();
	virtual ~CDatabaseFrameWnd();

	enum SortRecursionType
	{
		SORT_RECURSION_NONE = 1,
		SORT_RECURSION_ITEM = 2,
		SORT_RECURSION_FULL = 9999
	};

	virtual void          ReloadLibs();
	virtual void          ReloadItems();
	virtual HTREEITEM     InsertItemToTree(CBaseLibraryItem* pItem, HTREEITEM hParent);
	virtual void          SelectLibrary(const CString& library, bool bForceSelect = false);
	virtual void          SelectLibrary(CBaseLibrary* pItem, bool bForceSelect = false);
	virtual void          SelectItem(CBaseLibraryItem* item, bool bForceReload = false);

	virtual CBaseLibrary* FindLibrary(const CString& libraryName);
	virtual CBaseLibrary* NewLibrary(const CString& libraryName);
	virtual void          DeleteLibrary(CBaseLibrary* pLibrary);
	virtual void          DeleteItem(CBaseLibraryItem* pItem);

	virtual void          DeleteTreeItem(HTREEITEM hItem);
	virtual void          DeleteAllTreeItems();
	virtual void          ReleasePreviewControl() {}

	virtual bool          SetItemName(CBaseLibraryItem* item, const CString& groupName, const CString& itemName);

	// for CString conversion
	inline CBaseLibrary* FindLibrary(const char*  libraryName)
	{
		return FindLibrary(CString(libraryName));
	}
	inline CBaseLibrary* NewLibrary(const char*  libraryName)
	{
		return NewLibrary(CString(libraryName));
	}
	inline void          SelectLibrary(const char*  library, bool bForceSelect = false)
	{
		SelectLibrary(CString(library), bForceSelect);
	}
	inline bool			  SetItemName(CBaseLibraryItem* item, const char* groupName, const char* itemName)
	{
		return SetItemName(item, CString(groupName), CString(itemName));
	}

	void                  DoesItemExist(const string& itemName, bool& bOutExist) const;
	void                  DoesGroupExist(const string& groupName, bool& bOutExist) const;
	void                  OnSetModifedCurrentLibrary(bool modified);

	virtual void          OnEditorNotifyEvent(EEditorNotifyEvent event);

	CString               GetSelectedLibraryName() const
	{
		return m_selectedLib;
	}

	virtual const char* GetClassName() = 0;

protected:

	void              SortItems(SortRecursionType recursionType);

	virtual HTREEITEM GetRootItem()
	{
		return TVI_ROOT;
	}

	void GetAllItems(std::vector<HTREEITEM>& outItemList) const;
	void GetItemsUnderSpecificItem(HTREEITEM hItem, std::vector<HTREEITEM>& outItemList) const;
	int  GetComboBoxIndex(CBaseLibrary* pLibrary);

protected:

	DECLARE_MESSAGE_MAP()

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

	virtual afx_msg void OnUpdateSelected(CCmdUI* pCmdUI);
	afx_msg void         OnSelChangedItemTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void         OnKeyDownItemTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void         OnUpdatePaste(CCmdUI* pCmdUI);

	virtual void         OnCopy() = 0;
	virtual void         OnPaste() = 0;
	virtual void         OnCut();
	virtual void         OnClone();

	afx_msg void         OnUndo() { GetIEditorImpl()->GetIUndoManager()->Undo(); }
	afx_msg void         OnRedo() { GetIEditorImpl()->GetIUndoManager()->Redo(); }

	void                 InitToolBars();
	void                 InitStatusBar();
	void                 InitTreeCtrl();

	void                 LoadLibrary();

	string              MakeValidName(const string& candidateName, Functor2<const string&, bool&>& cb) const;

	virtual CTreeCtrl&   GetTreeCtrl()
	{
		return m_TreeCtrl;
	}

	virtual const CTreeCtrl& GetTreeCtrl() const
	{
		return m_TreeCtrl;
	}

private:
	CComboBox     m_LibraryListComboBox;
	CXTPStatusBar m_StatusBar;
	CXTTreeCtrl   m_TreeCtrl;

	bool          m_bIgnoreSelectionChange;

	// Map of library items to tree ctrl.
	typedef std::map<CBaseLibraryItem*, HTREEITEM> ItemsToTreeMap;

	//CXTToolBar m_toolbar;
	// Name of currently selected library.
	CString m_selectedLib;
	bool    m_bLibsLoaded;

protected:

	//! Selected library.
	_smart_ptr<CBaseLibrary> m_pLibrary;

	//! Last selected Item. (kept here for compatibility reasons)
	// See comments on m_cpoSelectedLibraryItems for more details.
	_smart_ptr<CBaseLibraryItem> m_pCurrentItem;

	// A set containing all the currently selected items
	// (it's disabled for MOST, but not ALL cases).
	// This should be the new standard way of storing selections as
	// opposed to the former mean, it allows us to store multiple selections.
	// The migration to this new style should be done according to the needs
	// for multiple selection.
	std::set<CBaseLibraryItem*> m_cpoSelectedLibraryItems;

	//! Pointer to item manager.
	CBaseLibraryManager* m_pItemManager;
	SortRecursionType    m_sortRecursionType;
	ItemsToTreeMap       m_itemsToTree;
	CString              m_selectedGroup;
};

