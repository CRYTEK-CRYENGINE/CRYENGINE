// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "TreeCtrlEx.h"

// CTreeCtrlEx

IMPLEMENT_DYNAMIC(CTreeCtrlEx, CTreeCtrl)
CTreeCtrlEx::CTreeCtrlEx()
{
	// When this tree is initialized, no dragging of course
	m_bLDragging = false;
	m_pDragImage = NULL;
	m_hitemDrag = NULL;
	m_hitemDrop = NULL;
	m_dropCursor = LoadCursor(NULL, IDC_ARROW);
	m_noDropCursor = LoadCursor(NULL, IDC_NO);
	m_bNoDrag = false;
}

CTreeCtrlEx::~CTreeCtrlEx()
{
}

BEGIN_MESSAGE_MAP(CTreeCtrlEx, CTreeCtrl)
ON_NOTIFY_REFLECT(TVN_BEGINDRAG, OnBeginDrag)
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONUP()
ON_WM_LBUTTONDOWN()
ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

// CTreeCtrlEx message handlers

//////////////////////////////////////////////////////////////////////////
// CopyItem             - Copies an item to a new location
// Returns              - Handle of the new item
// hItem                - Item to be copied
// htiNewParent         - Handle of the parent for new item
// htiAfter             - Item after which the new item should be created
HTREEITEM CTreeCtrlEx::CopyItem(HTREEITEM hItem, HTREEITEM htiNewParent,
                                HTREEITEM htiAfter /*= TVI_LAST*/)
{
	TV_INSERTSTRUCT tvstruct;
	HTREEITEM hNewItem;
	CString sText;

	// get information of the source item
	tvstruct.item.hItem = hItem;
	tvstruct.item.mask = TVIF_CHILDREN | TVIF_HANDLE |
	                     TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	GetItem(&tvstruct.item);
	sText = GetItemText(hItem);

	tvstruct.item.cchTextMax = sText.GetLength();
	tvstruct.item.pszText = sText.LockBuffer();

	// Insert the item at proper location
	tvstruct.hParent = htiNewParent;
	tvstruct.hInsertAfter = htiAfter;
	tvstruct.item.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT;
	hNewItem = InsertItem(&tvstruct);
	sText.ReleaseBuffer();

	// Now copy item data and item state.
	SetItemData(hNewItem, GetItemData(hItem));
	SetItemState(hNewItem, GetItemState(hItem, TVIS_STATEIMAGEMASK | TVIS_EXPANDED),
	             TVIS_STATEIMAGEMASK | TVIS_EXPANDED);

	// Call virtual function to allow further processing in derived class
	OnItemCopied(hItem, hNewItem);

	return hNewItem;
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlEx::OnItemCopied(HTREEITEM /*hItem*/, HTREEITEM /*hNewItem*/)
{
	// Virtual function
}

//////////////////////////////////////////////////////////////////////////
// CopyBranch           - Copies all items in a branch to a new location
// Returns              - The new branch node
// htiBranch            - The node that starts the branch
// htiNewParent - Handle of the parent for new branch
// htiAfter             - Item after which the new branch should be created
HTREEITEM CTreeCtrlEx::CopyBranch(HTREEITEM htiBranch, HTREEITEM htiNewParent,
                                  HTREEITEM htiAfter /*= TVI_LAST*/)
{
	HTREEITEM hChild;

	HTREEITEM hNewItem = CopyItem(htiBranch, htiNewParent, htiAfter);
	SetItemState(hNewItem, GetItemState(htiBranch, 0xffffffff), 0xffffffff);
	hChild = GetChildItem(htiBranch);
	while (hChild != NULL)
	{
		// recursively transfer all the items
		CopyBranch(hChild, hNewItem);
		hChild = GetNextSiblingItem(hChild);
	}
	return hNewItem;
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlEx::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
	*pResult = 0;

	m_hitemDrag = pNMTreeView->itemNew.hItem;
	m_hitemDrop = NULL;

	if (m_bNoDrag || !IsDropSource(m_hitemDrag))
		return;

	m_pDragImage = CreateDragImage(m_hitemDrag);  // get the image list for dragging
	// CreateDragImage() returns NULL if no image list
	// associated with the tree view control
	if (!m_pDragImage)
		return;

	m_bLDragging = true;

	// Calculate the offset to the hotspot
	CPoint offsetPt(8, 8);   // Initialize a default offset

	CPoint dragPt = pNMTreeView->ptDrag;    // Get the Drag point
	UINT nHitFlags = 0;
	HTREEITEM htiHit = HitTest(dragPt, &nHitFlags);
	if (NULL != htiHit)
	{
		// The drag point has Hit an item in the tree
		CRect itemRect;

		// Get the text bounding rectangle
		if (GetItemRect(htiHit, &itemRect, TRUE))
		{
			// Calculate the new offset
			offsetPt.y = dragPt.y - itemRect.top;
			offsetPt.x = dragPt.x - (itemRect.left - GetIndent());
		}
		/*
		   if (GetItemRect(htiHit, &itemRect, FALSE))
		   {
		   // Count indent levels
		   HTREEITEM htiParent = htiHit;
		   int nIndentCnt = 0;
		   while (htiParent != NULL)
		   {
		    htiParent = GetParentItem(htiParent);
		    nIndentCnt++;
		   }

		   if (!(GetStyle() & TVS_LINESATROOT))
		    nIndentCnt--;

		   // Calculate the new offset
		   offsetPt.y = dragPt.y - itemRect.top;
		   offsetPt.x = dragPt.x - (nIndentCnt * GetIndent()) + GetScrollPos(SB_HORZ);

		   CImageList* pImageListState = GetImageList(TVSIL_STATE);
		   UINT nState = GetItemState( htiHit, LVIS_STATEIMAGEMASK );
		   if (pImageListState && nState)
		   {
		    (nState>>=12)--;
		    IMAGEINFO ImageInfo;
		    //State Image list
		    pImageListState->GetImageInfo(nState,&ImageInfo);
		    offsetPt.x -= (ImageInfo.rcImage.right-ImageInfo.rcImage.left);
		   }
		   }
		 */
	}

	/*
	   CImageList* pList = GetImageList(TVSIL_STATE);
	   if (pList)
	   {
	   IMAGEINFO info;
	   pList->GetImageInfo(1, &info);
	   offsetPt.x -= info.rcImage.right-info.rcImage.left;
	   }
	 */

	// Begin the Drag operation using the Drag image and the calculated hotspot offset
	m_pDragImage->BeginDrag(0, offsetPt);

	//m_pDragImage->BeginDrag(0, CPoint(-15,-15));
	POINT pt = pNMTreeView->ptDrag;
	ClientToScreen(&pt);
	m_pDragImage->DragEnter(NULL, pt);
	SetCapture();
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlEx::OnMouseMove(UINT nFlags, CPoint point)
{
	HTREEITEM hitem;

	if (m_bLDragging)
	{
		POINT pt = point;
		ClientToScreen(&pt);
		CImageList::DragMove(pt);

		hitem = HitTest(point, &m_nDropHitTestFlags);
		if (m_hitemDrop != hitem)
		{
			CImageList::DragShowNolock(FALSE);
			if ((hitem != NULL) && (m_nDropHitTestFlags & TVHT_ONITEM))
				SelectDropTarget(hitem);
			else
				SelectDropTarget(NULL);
			m_hitemDrop = hitem;
			CImageList::DragShowNolock(TRUE);
		}

		hitem = GetDropTarget(hitem);
		if (hitem)
			SetCursor(m_dropCursor);
		else
			SetCursor(m_noDropCursor);

	}

	CTreeCtrl::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlEx::OnLButtonUp(UINT nFlags, CPoint point)
{
	CTreeCtrl::OnLButtonUp(nFlags, point);

	if (m_bLDragging)
	{
		m_bLDragging = false;
		CImageList::DragLeave(this);
		CImageList::EndDrag();
		ReleaseCapture();

		delete m_pDragImage;

		// Remove drop target highlighting
		SelectDropTarget(NULL);

		m_hitemDrop = GetDropTarget(m_hitemDrop);
		if (m_hitemDrop == NULL)
			return;

		if (m_hitemDrag == m_hitemDrop)
			return;

		Expand(m_hitemDrop, TVE_EXPAND);

		HTREEITEM htiNew = CopyBranch(m_hitemDrag, m_hitemDrop, TVI_LAST);
		DeleteItem(m_hitemDrag);
		SelectItem(htiNew);
		if (GetParent())
		{
			NMHDR param;
			param.hwndFrom = GetSafeHwnd();
			param.code = UTVN_ENDDRAG;
			param.idFrom = GetDlgCtrlID();
			GetParent()->SendMessage(WM_NOTIFY, GetDlgCtrlID(), (LPARAM)&param);
		}
	}
}

void CTreeCtrlEx::OnLButtonDown(UINT nFlags, CPoint point)
{
	// Set focus on tree when clicked.
	SetFocus();

	// Clear selection if clicking off all items
	UINT flags;
	HTREEITEM item = HitTest(point, &flags);
	if (!(flags & TVHT_ONITEM))
		SelectItem(NULL);

	CTreeCtrl::OnLButtonDown(nFlags, point);
}

BOOL CTreeCtrlEx::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		bool bHandledHere = false;

		// When an item is being edited make sure the edit control
		// receives certain important key strokes
		if (GetKeyState(VK_CONTROL))
		{
			bHandledHere = true;
		}
		CString m_Test;
		switch (pMsg->wParam)
		{
		case VK_RETURN:
			//GetEditControl()->GetWindowText(m_Test);
			//if(SetItemText(m_CurItem.p_Item, m_Test) == NULL)
			//TRACE("Unable to Change Item Text(Enter Message)!!!\n");
			bHandledHere = true;
			break;
		case VK_DELETE:
			//GetEditControl()->SetWindowText("");
			bHandledHere = true;
			break;
		case VK_ESCAPE:
			break;
			bHandledHere = true;
		}

		if (bHandledHere)
		{
			::TranslateMessage(pMsg);
			::DispatchMessage(pMsg);
			return TRUE;  // DO NOT process further
		}
	}

	if (pMsg->message == WM_KEYDOWN && m_bLDragging)
	{
		if (pMsg->wParam == VK_ESCAPE)
		{
			m_bLDragging = false;
			CImageList::DragLeave(NULL);
			CImageList::EndDrag();
			ReleaseCapture();
			SelectDropTarget(NULL);
			delete m_pDragImage;
			m_pDragImage = NULL;
		}
		return TRUE;    // DO NOT process further
	}

	return CTreeCtrl::PreTranslateMessage(pMsg);
}

//////////////////////////////////////////////////////////////////////////
BOOL CTreeCtrlEx::IsDropSource(HTREEITEM hItem)
{
	return TRUE;  // all other items are valid sources
}

//////////////////////////////////////////////////////////////////////////
HTREEITEM CTreeCtrlEx::GetDropTarget(HTREEITEM hItem)
{
	if (!(m_nDropHitTestFlags & TVHT_ONITEM))
		return TVI_ROOT;

	if (!hItem)
		return NULL;

	if (hItem != m_hitemDrag && hItem != GetParentItem(m_hitemDrag))
	{
		HTREEITEM htiParent = hItem;
		while ((htiParent = GetParentItem(htiParent)) != NULL)
		{
			if (htiParent == m_hitemDrag)
				return NULL;
		}
		return hItem;
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
HTREEITEM CTreeCtrlEx::GetNextItem(HTREEITEM hItem, UINT nCode)
{
	return CTreeCtrl::GetNextItem(hItem, nCode);
}

//////////////////////////////////////////////////////////////////////////
HTREEITEM CTreeCtrlEx::GetNextItem(HTREEITEM hItem)
{
	HTREEITEM hti = NULL;

	if (ItemHasChildren(hItem))
		hti = GetChildItem(hItem);  // return first child

	if (hti == NULL)
	{
		// return next sibling item
		// Go up the tree to find a parent's sibling if needed.
		while ((hti = GetNextSiblingItem(hItem)) == NULL)
		{
			if ((hItem = GetParentItem(hItem)) == NULL)
				return NULL;
		}
	}
	return hti;
}

//////////////////////////////////////////////////////////////////////////
HTREEITEM CTreeCtrlEx::FindNextItem(TV_ITEM* pItem, HTREEITEM hItem)
{
	ASSERT(::IsWindow(m_hWnd));

	TV_ITEM hNextItem;

	//Clear Item data
	ZeroMemory(&hNextItem, sizeof(hNextItem));

	//The mask is used to retrieve the data to compare
	hNextItem.mask = pItem->mask;
	hNextItem.hItem = (hItem) ? GetNextItem(hItem) : GetRootItem();

	//Prepare to compare pszText
	//Testing pItem->pszText protects the code from a client setting the
	//TVIF_TEXT bit but passing in a NULL pointer.
	if ((pItem->mask & TVIF_TEXT) && pItem->pszText)
	{
		hNextItem.cchTextMax = strlen(pItem->pszText);

		if (hNextItem.cchTextMax)
			hNextItem.pszText = new char[++hNextItem.cchTextMax];
	}

	while (hNextItem.hItem)
	{
		if (CompareItems(pItem, hNextItem))
		{
			//Copy all the information into pItem and return
			memcpy(pItem, &hNextItem, sizeof(TV_ITEM));

			//Free resources
			if (hNextItem.pszText)
				delete hNextItem.pszText;

			return pItem->hItem;
		}

		//The mask is used to retrieve the data to compare and must be
		//reset before calling Compare
		hNextItem.mask = pItem->mask;
		hNextItem.hItem = GetNextItem(hNextItem.hItem);
	}

	//Set hItem in pItem
	pItem->hItem = NULL;

	//Free resources
	if (hNextItem.pszText)
		delete hNextItem.pszText;

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
BOOL CTreeCtrlEx::CompareItems(TV_ITEM* pItem, TV_ITEM& tvTempItem)
{
	//This call uses the .mask setting to just retrieve the values
	//that the client wants to compare.
	//Get all the data passed in by pItem
	GetItem(&tvTempItem);

	//Reset the mask so I can keep track of the matching attributes
	tvTempItem.mask = 0;

	if ((pItem->mask & TVIF_STATE) &&
	    (pItem->state == tvTempItem.state))
		tvTempItem.mask |= TVIF_STATE;

	if ((pItem->mask & TVIF_IMAGE) &&
	    (pItem->iImage == tvTempItem.iImage))
		tvTempItem.mask |= TVIF_IMAGE;

	if ((pItem->mask & TVIF_PARAM) &&
	    (pItem->lParam == tvTempItem.lParam))
		tvTempItem.mask |= TVIF_PARAM;

	if ((pItem->mask & TVIF_TEXT) &&
	    pItem->pszText && tvTempItem.pszText && //Don't compare if either is NULL
	    !strcmp(pItem->pszText, tvTempItem.pszText))
		tvTempItem.mask |= TVIF_TEXT;

	if ((pItem->mask & TVIF_CHILDREN) &&
	    (pItem->cChildren == tvTempItem.cChildren))
		tvTempItem.mask |= TVIF_CHILDREN;

	if ((pItem->mask & TVIF_SELECTEDIMAGE) &&
	    (pItem->iSelectedImage == tvTempItem.iSelectedImage))
		tvTempItem.mask |= TVIF_SELECTEDIMAGE;

	//If by this point these two values are the same.
	//tvTempItem.hItem is the desired item
	return (pItem->mask == tvTempItem.mask);
}

//////////////////////////////////////////////////////////////////////////
BOOL CTreeCtrlEx::OnEraseBkgnd(CDC* pDC)
{
	return FALSE;
}

