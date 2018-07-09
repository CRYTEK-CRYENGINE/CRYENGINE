// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2011.
// -------------------------------------------------------------------------
//  File name:   LensFlareElementTree.h
//  Created:     7/Dec/2012 by Jaesik.
////////////////////////////////////////////////////////////////////////////

#include "LensFlareElement.h"
#include "LensFlareUtil.h"
#include "ILensFlareListener.h"

class CLensFlareItem;

class CLensFlareElementTree : public CXTTreeCtrl, public ILensFlareChangeItemListener
{
public:

	CLensFlareElementTree();
	~CLensFlareElementTree();

	void RegisterListener(ILensFlareChangeElementListener* pListener)
	{
		if (pListener)
			m_LensFlaresElementListeners.push_back(pListener);
	}

	void UnregisterListener(ILensFlareChangeElementListener* pListener)
	{
		if (pListener == NULL)
			return;
		std::vector<ILensFlareChangeElementListener*>::iterator ii = m_LensFlaresElementListeners.begin();
		for (; ii != m_LensFlaresElementListeners.end(); )
		{
			if (*ii == pListener)
				ii = m_LensFlaresElementListeners.erase(ii);
			else
				++ii;
		}
	}

	IOpticsElementBasePtr GetOpticsElement() const;
	CLensFlareItem*       GetLensFlareItem() const
	{
		return m_pLensFlareItem;
	}

	CLensFlareElement::LensFlareElementPtr AddAtomicElement(EFlareType flareType, IOpticsElementBasePtr pParent);
	CLensFlareElement::LensFlareElementPtr InsertAtomicElement(int nIndex, EFlareType flareType, IOpticsElementBasePtr pParent);
	CLensFlareElement::LensFlareElementPtr AddElement(IOpticsElementBasePtr pOptics, IOpticsElementBasePtr pParent);
	bool                                   AddElement(EFlareType flareType);

	CLensFlareElement::LensFlareElementPtr GetElement(int nIndex) const
	{
		if (nIndex < 0 || nIndex >= m_LensFlareElements.size())
			return NULL;
		return m_LensFlareElements[nIndex];
	}

	int GetElementCount() const
	{
		return m_LensFlareElements.size();
	}

	CLensFlareElement::LensFlareElementPtr FindLensFlareElement(IOpticsElementBasePtr pElement) const;
	void                                   MakeValidElementName(const string& seedName, string& outValidName) const;
	bool                                   IsExistElement(const string& name) const;
	void                                   StartEditItem(HTREEITEM hItem);

	void                                   OnInternalVariableChange(IVariable* pVar);

	CLensFlareElement::LensFlareElementPtr GetCurrentLensFlareElement() const;
	CLensFlareElement::LensFlareElementPtr GetLensFlareElement(HTREEITEM hItem) const;
	void                                   SelectTreeItemByLensFlareElement(const CLensFlareElement* pElement);
	void                                   UpdateProperty();
	void                                   UpdateClipboard(const char* type, bool bPasteAtSameLevel);
	void                                   GatherItemsRecusively(HTREEITEM hItem, CLensFlareElement::LensFlareElementList& outList);
	IOpticsElementBasePtr                  GetOpticsElementByTreeItem(HTREEITEM hItem) const;
	HTREEITEM                              GetTreeItemByOpticsElement(const IOpticsElementBasePtr pOptics) const;
	void                                   InvalidateLensFlareItem()
	{
		m_pLensFlareItem = NULL;
	}

	void SelectTreeItemByName(const CString& name);
	void SelectTreeItemByCursorPos(bool* pOutChanged = NULL);

	void Paste(XmlNodeRef xmlNode);
	void UpdateLensFlareElement(HTREEITEM hSelectedItem, const CPoint& screenPos);
	void Drop(XmlNodeRef xmlNode, const CPoint& screenPos);
	void UpdateDraggingFromOtherWindow();

protected:

	void                                   UpdateLensFlareItem(CLensFlareItem* pLensFlareItem);
	void                                   ClearElementList();
	void                                   DeleteOnesInElementList(CLensFlareElement::LensFlareElementList& elementList);
	void                                   Reload();

	void                                   OnLensFlareChangeItem(CLensFlareItem* pLensFlareItem);
	void                                   OnLensFlareDeleteItem(CLensFlareItem* pLensFlareItem);

	void                                   CallChangeListeners();

	void                                   OnNotifyTreeRClick(NMHDR* pNMHDR, LRESULT* pResult);
	void                                   OnTvnSelchangedTree(NMHDR* pNMHDR, LRESULT* pResult);
	void                                   OnTvnBeginlabeleditTree(NMHDR* pNMHDR, LRESULT* pResult);
	void                                   OnTvnItemClicked(NMHDR* pNMHDR, LRESULT* pResult);
	void                                   OnTvnEndlabeleditTree(NMHDR* pNMHDR, LRESULT* pResult);
	void                                   OnTvnKeydownTree(NMHDR* pNMHDR, LRESULT* pResult);
	void                                   OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult);

	void                                   StoreUndo(const CString& undoDescription = "");
	XmlNodeRef                             CreateXML(const char* type) const;
	bool                                   GetClipboardList(const char* type, std::vector<LensFlareUtil::SClipboardData>& outList) const;

	CLensFlareElement::LensFlareElementPtr FindRootElement() const;
	int                                    FindElementIndexByOptics(IOpticsElementBasePtr pOptics) const;

	void                                   AddLensElementRecursively(HTREEITEM hParent, IOpticsElementBasePtr pOptics);

	//! Only the basic operations like following methods must have routine for undoing.
	CLensFlareElement::LensFlareElementPtr CreateElement(IOpticsElementBasePtr pOptics);
	void                                   RemoveElement(HTREEITEM hItem);
	void                                   RemoveAllElements();
	void                                   RenameElement(CLensFlareElement* pLensFlareElement, const CString& newName);
	void                                   EnableElement(CLensFlareElement* pLensFlareElement, bool bEnable);
	void                                   ElementChanged(CLensFlareElement* pPrevLensFlareElement);
	/////////////////////////////////////////////////////////////////////////////////////////

	afx_msg void OnAddGroup();
	afx_msg void OnCopy();
	afx_msg void OnCut();
	afx_msg void OnPaste();
	afx_msg void OnClone();
	afx_msg void OnRenameItem();
	afx_msg void OnRemoveItem();
	afx_msg void OnRemoveAll();
	afx_msg void OnItemUp();
	afx_msg void OnItemDown();
	afx_msg void OnMouseMove();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);

	void         Cut(bool bPasteAtSameLevel);

	DECLARE_MESSAGE_MAP()

private:

	HTREEITEM InsertElementToTree(int nIndex, HTREEITEM hParent);
	void      UpdateLensFlareElementsRecusively(IOpticsElementBasePtr pOptics);

	std::vector<ILensFlareChangeElementListener*>       m_LensFlaresElementListeners;
	std::vector<CLensFlareElement::LensFlareElementPtr> m_LensFlareElements;
	CLensFlareItem* m_pLensFlareItem;
};

