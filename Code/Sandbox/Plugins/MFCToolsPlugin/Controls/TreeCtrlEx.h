// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#define UTVN_ENDDRAG (TVN_FIRST - 70)

/** CTreeCtrlEx is an extended version of CTreeCtrl,
    It allows Drag&Drop and copying of items.
 */
class PLUGIN_API CTreeCtrlEx : public CTreeCtrl
{
	DECLARE_DYNAMIC(CTreeCtrlEx)

public:
	CTreeCtrlEx();
	virtual ~CTreeCtrlEx();

	/** Get next item using standard CTreeCtrl call.
	 */
	HTREEITEM GetNextItem(HTREEITEM hItem, UINT nCode);

	/** Get next item as if outline was completely expanded.
	    @return	The item immediately below the reference item.
	    @param	The reference item.
	 */
	HTREEITEM GetNextItem(HTREEITEM hItem);

	/** FindNextItem traverses a tree searching for an item matching all the item attributes set in a TV_ITEM structure.
	    In the TV_ITEM structure, the mask member specifies which attributes make up the search criteria.
	    If a match is found, the function returns the handle otherwise NULL.
	    The function only searches in one direction (down) and if hItem is NULL starts from the root of the tree.
	 */
	HTREEITEM FindNextItem(TV_ITEM* pItem, HTREEITEM hItem);

	/** Copies an item to a new location
	    @return	Handle of the new item.
	    @parans hItem	Item to be copied.
	    @param	htiNewParent	Handle of the parent for new item.
	    @param	htiAfter	Item after which the new item should be created.
	 */
	HTREEITEM CopyItem(HTREEITEM hItem, HTREEITEM htiNewParent, HTREEITEM htiAfter = TVI_LAST);

	/** Copies all items in a branch to a new location.
	    @return	The new branch node.
	    @param	htiBranch	The node that starts the branch.
	    @param	htiNewParent	Handle of the parent for new branch.
	    @param	htiAfter	Item after which the new branch should be created.
	 */
	HTREEITEM CopyBranch(HTREEITEM htiBranch, HTREEITEM htiNewParent, HTREEITEM htiAfter = TVI_LAST);

	/** To disallow all dragging
	 */
	void SetNoDrag(bool bNoDrag) { m_bNoDrag = bNoDrag; }

protected:
	DECLARE_MESSAGE_MAP()

	//! Callback called when items are copied.
	virtual void      OnItemCopied(HTREEITEM hItem, HTREEITEM hNewItem);
	//! Check if drop source.
	virtual BOOL      IsDropSource(HTREEITEM hItem);
	//! Get item which is target for drop of this item.
	virtual HTREEITEM GetDropTarget(HTREEITEM hItem);

	BOOL              CompareItems(TV_ITEM* pItem, TV_ITEM& tvTempItem);

	//////////////////////////////////////////////////////////////////////////
	// Message Handlers.
	//////////////////////////////////////////////////////////////////////////
	afx_msg void OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	virtual BOOL PreTranslateMessage(MSG* pMsg);

	//////////////////////////////////////////////////////////////////////////
	// For dragging.
	CImageList* m_pDragImage;
	bool        m_bLDragging;
	HTREEITEM   m_hitemDrag, m_hitemDrop;
	HCURSOR     m_dropCursor, m_noDropCursor;
	bool        m_bNoDrag;
	UINT        m_nDropHitTestFlags;
};

