// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2011.
// -------------------------------------------------------------------------
//  File name:   LensFlareEditor.h
//  Created:     7/Dec/2012 by Jaesik.
////////////////////////////////////////////////////////////////////////////

#include "DatabaseFrameWnd.h"
#include "LensFlareUtil.h"
#include "ILensFlareListener.h"
#include "LensFlareItemTree.h"
#include "UserMessageDefines.h"

class CLensFlareView;
class CLensFlareElementTree;
class CLensFlareAtomicList;
class CLensFlareElementPropertyView;
class CLensFlareItem;
class CLensFlareLibrary;
class CPropertyCtrl;
class CLensFlareLightEntityTree;
class CLensFlareReferenceTree;

class CLensFlareEditor : public CDatabaseFrameWnd, public ILensFlareChangeElementListener
{
	DECLARE_DYNCREATE(CLensFlareEditor)
public:

	CLensFlareEditor();
	~CLensFlareEditor();

	CLensFlareItem*    GetSelectedLensFlareItem() const;
	CLensFlareItem*    GetLensFlareItem(HTREEITEM hItem) const;
	bool               GetSelectedLensFlareName(string& outName) const;
	void               UpdateLensFlareItem(CLensFlareItem* pLensFlareItem);
	void               ResetElementTreeControl();

	CLensFlareLibrary* GetCurrentLibrary() const
	{
		if (m_pLibrary == NULL)
			return NULL;
		return (CLensFlareLibrary*)&(*m_pLibrary);
	}

	bool                   IsExistTreeItem(const string& name, bool bExclusiveSelectedItem = false);
	void                   RenameLensFlareItem(CLensFlareItem* pLensFlareItem, const string& newGroupName, const string& newShortName);

	IOpticsElementBasePtr  FindOptics(const string& itemPath, const string& opticsPath);

	CLensFlareElementTree* GetLensFlareElementTree()
	{
		return m_pLensFlareElementTree;
	}

	CLensFlareView* GetLensFlareView() const
	{
		return m_pLensFlareView;
	}

	CLensFlareItemTree* GetLensFlareItemTree()
	{
		return &m_LensFlareItemTree;
	}

	void           RemovePropertyItems();

	CPropertyCtrl* GetPropertyCtrl()
	{
		return m_pWndProps;
	}

	bool                     IsPointInLensFlareElementTree(const CPoint& screenPos) const;
	bool                     IsPointInLensFlareItemTree(const CPoint& screenPos) const;

	void                     UpdateLensOpticsNames(const string& oldFullName, const string& newFullName);
	bool                     GetNameFromTreeItem(HTREEITEM hItem, string& outName) const;
	void                     SelectItemInLensFlareElementTreeByName(const string& name);
	void                     ReloadItems();

	void                     RegisterLensFlareItemChangeListener(ILensFlareChangeItemListener* pListener);
	void                     UnregisterLensFlareItemChangeListener(ILensFlareChangeItemListener* pListener);

	bool                     SelectItemByName(const string& itemName);

	static CLensFlareEditor* GetLensFlareEditor()
	{
		return s_pLensFlareEditor;
	}

	bool GetFullSelectedFlareItemName(string& outFullName) const
	{
		return GetFullLensFlareItemName(GetTreeCtrl().GetSelectedItem(), outFullName);
	}
	void        SelectLensFlareItem(const string& fullItemName);

	const char* GetClassName()
	{
		return s_pLensFlareEditorClassName;
	}

	void       Paste(XmlNodeRef node);
	XmlNodeRef CreateXML(const char* type) const;

	static const char* s_pLensFlareEditorClassName;

	CTreeCtrl&         GetTreeCtrl()
	{
		return m_LensFlareItemTree;
	}

	const CTreeCtrl& GetTreeCtrl() const
	{
		return m_LensFlareItemTree;
	}

	void AddNewItemByAtomicOptics(EFlareType flareType);

protected:

	static CLensFlareEditor* s_pLensFlareEditor;

	DECLARE_MESSAGE_MAP()

	BOOL            OnInitDialog();
	void            DoDataExchange(CDataExchange* pDX);

	void            OnCopy();
	void            OnPaste();
	void            OnCut();

	void            UpdateClipboard(const char* type) const;
	bool            GetClipboardDataList(std::vector<LensFlareUtil::SClipboardData>& outList, string& outGroupName) const;
	void            OnLensFlareChangeElement(CLensFlareElement* pLensFlareElement);

	afx_msg void    OnAddLibrary();
	afx_msg void    OnAssignFlareToLightEntities();
	afx_msg void    OnSelectAssignedObjects();
	afx_msg void    OnGetFlareFromSelection();
	afx_msg void    OnRenameItem();
	afx_msg void    OnAddItem();
	afx_msg void    OnRemoveItem();
	afx_msg void    OnSize(UINT nType, int cx, int cy);
	afx_msg void    OnCopyNameToClipboard();
	afx_msg LRESULT OnUpdateTreeCtrl(WPARAM wp, LPARAM lp);
	afx_msg void    OnNotifyTreeRClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void    OnTvnBeginlabeleditTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void    OnTvnEndlabeleditTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void    OnTvnKeydownTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void    OnTvnItemSelChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void    OnReloadLib();
	afx_msg void    OnUpdateSelected(CCmdUI* pCmdUI);

	void            ExpandTreeCtrl();
	void            CreateWindowsToBePutIntoPanels();
	void            ReleaseWindowsToBePutIntoPanels();
	void            CreatePanes();
	void            AttachWindowsToPanes();
	void            SelectLensFlareItem(HTREEITEM hItem, HTREEITEM hPrevItem);

	void            OnEditLensFlare();
	HTREEITEM       GetLensFlareTreeItem(CLensFlareItem* pItem) const;
	void            StartEditItem(HTREEITEM hItem);
	bool            GetFullLensFlareItemName(HTREEITEM hItem, string& outFullName) const;
	bool            GetFullLensFlareItemName(CLensFlareItem* pItem, string& outFullName) const
	{
		HTREEITEM hItem = GetLensFlareTreeItem(pItem);
		if (hItem == NULL)
			return false;
		return GetFullLensFlareItemName(hItem, outFullName);
	}

	void            ChangeGroupName(HTREEITEM hItem, const string& newGroupName);
	CLensFlareItem* AddNewLensFlareItem(const string& groupName, const string& shortName);
	HTREEITEM       GetTreeLensFlareItem(CLensFlareItem* pItem) const;

	void            ResetLensFlareItems();
	void            GetAllLensFlareItems(std::vector<HTREEITEM>& outItemList) const;
	void            GetLensFlareItemsUnderSpecificItem(HTREEITEM hItem, std::vector<HTREEITEM>& outItemList) const;

	HTREEITEM       FindLensFlareItemByFullName(const string& fullName) const;

	enum ESelectedItemStatus
	{
		eSIS_Unselected,
		eSIS_Group,
		eSIS_Flare
	};
	ESelectedItemStatus GetSelectedItemStatus() const;

	CXTPDockingPane*    CreatePane(const string& name, const CSize& minSize, UINT nID, CRect rc, XTPDockingPaneDirection direction, CXTPDockingPaneBase* pNeighbour = NULL);
	void                AttachWindowToPane(UINT nID, CWnd* pWin);
	void                OnUpdateProperties(IVariable* var);

private:

	CLensFlareView*                            m_pLensFlareView;
	CLensFlareAtomicList*                      m_pLensFlareAtomicList;
	CLensFlareElementTree*                     m_pLensFlareElementTree;
	CPropertyCtrl*                             m_pWndProps;
	CLensFlareLightEntityTree*                 m_pLensFlareLightEntityTree;
	CLensFlareReferenceTree*                   m_pLensFlareReferenceTree;
	CLensFlareItemTree                         m_LensFlareItemTree;

	std::vector<ILensFlareChangeItemListener*> m_LensFlareChangeItemListenerList;
	std::vector<HTREEITEM>                     m_AllLensFlareTreeItems;
};

