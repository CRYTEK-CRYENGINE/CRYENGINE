// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ImageListCtrl.h"
#include "MemDC.h"
#include "Controls/SharedFonts.h"

static const int kCharacterHeightInItem = 14;

//////////////////////////////////////////////////////////////////////////
CImageListCtrlItem::CImageListCtrlItem()
{
	pUserData = 0;
	pUserData2 = 0;
	pImage = 0;
	bBitmapValid = false;
	bSelected = false;
}

//////////////////////////////////////////////////////////////////////////
CImageListCtrlItem::~CImageListCtrlItem()
{
	if (pImage)
		delete pImage;
}

//////////////////////////////////////////////////////////////////////////
CImageListCtrl::CImageListCtrl()
{
	m_scrollOffset = CPoint(0, 0);
	m_itemSize = CSize(60, 60);
	m_borderSize = CSize(4, 4);
	m_bkgrBrush.CreateSysColorBrush(COLOR_WINDOW);
	m_DragDropStatus = eMDDS_None;
	m_PointWhenHoldingLButton = CPoint(0, 0);
	m_selectedPen.CreatePen(PS_SOLID, 1, RGB(255, 55, 50));
}

//////////////////////////////////////////////////////////////////////////
CImageListCtrl::~CImageListCtrl()
{
}

BEGIN_MESSAGE_MAP(CImageListCtrl, CWnd)
ON_WM_CREATE()
ON_WM_SIZE()
ON_WM_DESTROY()
ON_WM_PAINT()
ON_WM_HSCROLL()
ON_WM_VSCROLL()
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_RBUTTONDOWN()
ON_WM_RBUTTONUP()
ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
BOOL CImageListCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	LPCTSTR lpClassName = AfxRegisterWndClass(CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW,
	                                          AfxGetApp()->LoadStandardCursor(IDC_ARROW),
	                                          (HBRUSH)::GetStockObject(LTGRAY_BRUSH), NULL);
	BOOL bRes = CreateEx(0, lpClassName, NULL, dwStyle, rect, pParentWnd, nID);
	return bRes;
}

//////////////////////////////////////////////////////////////////////////
int CImageListCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	InitializeFlatSB(GetSafeHwnd());
	FlatSB_SetScrollProp(GetSafeHwnd(), WSB_PROP_CXVSCROLL, 8, FALSE);
	FlatSB_SetScrollProp(GetSafeHwnd(), WSB_PROP_CYVSCROLL, 8, FALSE);
	FlatSB_SetScrollProp(GetSafeHwnd(), WSB_PROP_VSTYLE, FSB_ENCARTA_MODE, FALSE);
	FlatSB_EnableScrollBar(GetSafeHwnd(), SB_BOTH, ESB_ENABLE_BOTH);

	SetFont(CFont::FromHandle(gMFCFonts.hSystemFont));

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnDestroy()
{
	m_items.clear();
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	if (m_offscreenBitmap.GetSafeHandle() != NULL)
		m_offscreenBitmap.DeleteObject();

	CRect rc;
	GetClientRect(rc);

	CDC* dc = GetDC();
	m_offscreenBitmap.CreateCompatibleBitmap(dc, rc.Width(), rc.Height());
	ReleaseDC(dc);

	if (m_hWnd)
	{
		CalcLayout();
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CImageListCtrl::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnPaint()
{
	CPaintDC PaintDC(this);

	CRect rcClient;
	GetClientRect(rcClient);
	if (m_offscreenBitmap.GetSafeHandle() == NULL)
	{
		m_offscreenBitmap.CreateCompatibleBitmap(&PaintDC, rcClient.Width(), rcClient.Height());
	}
	CMemoryDC dc(PaintDC, &m_offscreenBitmap);

	XTPPaintManager()->GradientFill(dc, rcClient, GetXtremeColor(COLOR_WINDOW), GetXtremeColor(COLOR_BTNSHADOW), FALSE, &PaintDC.m_ps.rcPaint);
	DrawControl(dc, PaintDC.m_ps.rcPaint);
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::DrawControl(CDC* pDC, const CRect& rcUpdate)
{
	// Get items in update rect.
	Items items;
	GetItemsInRect(rcUpdate, items);
	for (int i = 0; i < items.size(); i++)
		DrawItem(pDC, items[i], false);
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::DrawItem(CDC* pDC, CImageListCtrlItem* pItem, bool bForceMoveToOrigin)
{
	if (!pItem->bBitmapValid)
	{
		// Make a bitmap from image.
		OnUpdateItem(pItem);
	}

	COLORREF clrSelected = RGB(255, 55, 50);

	CDC bmpDC;
	VERIFY(bmpDC.CreateCompatibleDC(pDC));

	CRect initialRect = pItem->rect;
	if (bForceMoveToOrigin)
		initialRect.OffsetRect(-pItem->rect.left, -pItem->rect.top);

	CBitmap* pOldBitmap = bmpDC.SelectObject(&pItem->bitmap);
	pDC->BitBlt(initialRect.left, initialRect.top, initialRect.Width(), initialRect.Height(), &bmpDC, 0, 0, SRCCOPY);
	bmpDC.SelectObject(pOldBitmap);

	CRect rc = initialRect;
	rc.InflateRect(1, 1);
	pDC->Draw3dRect(rc, GetXtremeColor(COLOR_WINDOWFRAME), GetXtremeColor(COLOR_WINDOWFRAME));

	{
		CRect rcText = initialRect;
		rcText.top = rcText.bottom;
		rcText.bottom = rcText.top + kCharacterHeightInItem;
		rcText.left -= 1;
		rcText.right += 1;

		if (!pItem->bSelected)
		{
			pDC->FillRect(rcText, &m_bkgrBrush);
		}
		else
		{
			CBrush brush;
			brush.CreateSolidBrush(clrSelected);
			pDC->FillRect(rcText, &brush);
		}

		pDC->Draw3dRect(rcText, GetXtremeColor(COLOR_WINDOWFRAME), GetXtremeColor(COLOR_WINDOWFRAME));

		HGDIOBJ pOldFont = 0;
		pDC->SetBkMode(TRANSPARENT);
		if (!pItem->bSelected)
		{
			pOldFont = pDC->SelectObject(gMFCFonts.hSystemFont);
			pDC->SetTextColor(GetSysColor(COLOR_WINDOWTEXT));
		}
		else
		{
			pOldFont = pDC->SelectObject(gMFCFonts.hSystemFontBold);
			pDC->SetTextColor(RGB(255, 255, 255));
		}
		pDC->DrawText(pItem->text, rcText, DT_NOCLIP | DT_CENTER | DT_SINGLELINE | DT_VCENTER);
		if (!pOldFont)
			pDC->SelectObject(pOldFont);
	}

	if (pItem->bSelected)
	{
		CRect rc = initialRect;

		// Draw selection border.
		pDC->Draw3dRect(rc, clrSelected, clrSelected);
		rc.InflateRect(1, 1);
		pDC->Draw3dRect(rc, clrSelected, clrSelected);
		rc.InflateRect(1, 1);
		pDC->Draw3dRect(rc, clrSelected, clrSelected);
	}

	bmpDC.DeleteDC();
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::CalcLayout(bool bUpdateScrollBar)
{
	static bool bNoRecurse = false;
	if (bNoRecurse)
		return;
	bNoRecurse = true;

	CRect rc;
	GetClientRect(rc);

	int nStyle = GetStyle();

	if (nStyle & ILC_STYLE_HORZ)
	{
		if (bUpdateScrollBar)
		{
			// Set scroll info.
			int nPage = rc.Width();
			int w = m_items.size() * (m_itemSize.cx + m_borderSize.cx) + m_borderSize.cx;

			SCROLLINFO si;
			ZeroStruct(si);
			si.cbSize = sizeof(si);
			si.fMask = SIF_ALL;
			si.nMin = 0;
			si.nMax = w;
			si.nPage = nPage;
			si.nPos = m_scrollOffset.x;
			FlatSB_SetScrollInfo(GetSafeHwnd(), SB_HORZ, &si, TRUE);
			FlatSB_GetScrollInfo(GetSafeHwnd(), SB_HORZ, &si);
			m_scrollOffset.x = si.nPos;
		}

		// Horizontal Layout.
		int x = m_borderSize.cx - m_scrollOffset.x;
		int y = m_borderSize.cy;
		for (int i = 0; i < m_items.size(); i++)
		{
			CImageListCtrlItem* pItem = m_items[i];
			pItem->rect.left = x;
			pItem->rect.top = y;
			pItem->rect.right = pItem->rect.left + m_itemSize.cx;
			pItem->rect.bottom = pItem->rect.top + m_itemSize.cy;
			x += m_itemSize.cx + m_borderSize.cx;
		}
	}
	else if (nStyle & ILC_STYLE_VERT)
	{
		// Vertical Layout
	}
	else
	{
		int nPageHorz = rc.Width();
		int nPageVert = rc.Height();

		if (nPageHorz == 0 || nPageVert == 0 || m_items.empty())
		{
			bNoRecurse = false;
			return;
		}

		int nTextHeight = 12;
		int nItemWidth = m_itemSize.cx + m_borderSize.cx;
		int nItemHeight = m_itemSize.cy + m_borderSize.cy + nTextHeight;

		int nNumOfHorzItems = nPageHorz / nItemWidth;
		if (nNumOfHorzItems <= 0)
			nNumOfHorzItems = 1;
		int nNumOfVertItems = m_items.size() / nNumOfHorzItems;

		if (bUpdateScrollBar)
		{
			int w = nNumOfHorzItems * nItemWidth + m_borderSize.cx;
			int h = nNumOfVertItems * nItemHeight + m_borderSize.cy;

			SCROLLINFO si;
			ZeroStruct(si);
			si.cbSize = sizeof(si);
			si.fMask = SIF_ALL;
			si.nMin = 0;
			si.nMax = w;
			si.nPage = nPageHorz;
			si.nPos = m_scrollOffset.x;
			FlatSB_SetScrollInfo(GetSafeHwnd(), SB_HORZ, &si, TRUE);
			FlatSB_GetScrollInfo(GetSafeHwnd(), SB_HORZ, &si);
			m_scrollOffset.x = si.nPos;

			ZeroStruct(si);
			si.cbSize = sizeof(si);
			si.fMask = SIF_ALL;
			si.nMin = 0;
			si.nMax = h;
			si.nPage = nPageVert;
			si.nPos = m_scrollOffset.y;
			FlatSB_SetScrollInfo(GetSafeHwnd(), SB_VERT, &si, TRUE);
			FlatSB_GetScrollInfo(GetSafeHwnd(), SB_VERT, &si);
			m_scrollOffset.y = si.nPos;
		}

		int x = m_borderSize.cx - m_scrollOffset.x;
		int y = m_borderSize.cy - m_scrollOffset.y;
		for (int itemCounter = 0; itemCounter < m_items.size(); ++itemCounter)
		{
			CImageListCtrlItem* pItem = m_items[itemCounter];
			pItem->rect.left = x;
			pItem->rect.top = y;
			pItem->rect.right = pItem->rect.left + m_itemSize.cx;
			pItem->rect.bottom = pItem->rect.top + m_itemSize.cy;

			if ((itemCounter + 1) % nNumOfHorzItems == 0)
			{
				y += nItemHeight;
				x = m_borderSize.cx - m_scrollOffset.x;
			}
			else
			{
				x += nItemWidth;
			}
		}
	}

	bNoRecurse = false;
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::GetAllItems(Items& items) const
{
	items = m_items;
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::GetItemsInRect(const CRect& rect, Items& items) const
{
	items.clear();
	CRect rcTemp;
	// Get all items inside specified rectangle.
	for (int i = 0; i < m_items.size(); i++)
	{
		CImageListCtrlItem* pItem = m_items[i];
		rcTemp.IntersectRect(pItem->rect, rect);
		if (!rcTemp.IsRectEmpty())
			items.push_back(pItem);
	}
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::SetItemSize(CSize size)
{
	assert(size.cx != 0 && size.cy != 0);
	m_itemSize = size;
	CalcLayout();
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::SetBorderSize(CSize size)
{
	m_borderSize = size;
	CalcLayout();
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
bool CImageListCtrl::InsertItem(CImageListCtrlItem* pItem)
{
	m_items.push_back(pItem);
	CalcLayout();
	Invalidate();
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::DeleteAllItems()
{
	m_items.clear();
	CalcLayout();
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	SCROLLINFO si;
	ZeroStruct(si);
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	FlatSB_GetScrollInfo(GetSafeHwnd(), SB_HORZ, &si);

	// Get the minimum and maximum scroll-bar positions.
	int minpos = si.nMin;
	int maxpos = si.nMax;
	int nPage = si.nPage;

	// Get the current position of scroll box.
	int curpos = si.nPos;

	// Determine the new position of scroll box.
	switch (nSBCode)
	{
	case SB_LEFT:      // Scroll to far left.
		curpos = minpos;
		break;

	case SB_RIGHT:      // Scroll to far right.
		curpos = maxpos;
		break;

	case SB_ENDSCROLL:   // End scroll.
		break;

	case SB_LINELEFT:      // Scroll left.
		if (curpos > minpos)
			curpos--;
		break;

	case SB_LINERIGHT:   // Scroll right.
		if (curpos < maxpos)
			curpos++;
		break;

	case SB_PAGELEFT:    // Scroll one page left.
		if (curpos > minpos)
			curpos = max(minpos, curpos - (int)nPage);
		break;

	case SB_PAGERIGHT:      // Scroll one page right.
		if (curpos < maxpos)
			curpos = min(maxpos, curpos + (int)nPage);
		break;

	case SB_THUMBPOSITION: // Scroll to absolute position. nPos is the position
		curpos = nPos;       // of the scroll box at the end of the drag operation.
		break;

	case SB_THUMBTRACK:  // Drag scroll box to specified position. nPos is the
		curpos = nPos;     // position that the scroll box has been dragged to.
		break;
	}

	// Set the new position of the thumb (scroll box).
	FlatSB_SetScrollPos(GetSafeHwnd(), SB_HORZ, curpos, TRUE);

	m_scrollOffset.x = curpos;
	CalcLayout(false);
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	SCROLLINFO si;
	ZeroStruct(si);
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	FlatSB_GetScrollInfo(GetSafeHwnd(), SB_VERT, &si);

	// Get the minimum and maximum scroll-bar positions.
	int minpos = si.nMin;
	int maxpos = si.nMax;
	int nPage = si.nPage;

	// Get the current position of scroll box.
	int curpos = si.nPos;

	// Determine the new position of scroll box.
	switch (nSBCode)
	{
	case SB_LEFT:      // Scroll to far left.
		curpos = minpos;
		break;

	case SB_RIGHT:      // Scroll to far right.
		curpos = maxpos;
		break;

	case SB_ENDSCROLL:   // End scroll.
		break;

	case SB_LINELEFT:      // Scroll left.
		if (curpos > minpos)
			curpos--;
		break;

	case SB_LINERIGHT:   // Scroll right.
		if (curpos < maxpos)
			curpos++;
		break;

	case SB_PAGELEFT:    // Scroll one page left.
		if (curpos > minpos)
			curpos = max(minpos, curpos - (int)nPage);
		break;

	case SB_PAGERIGHT:      // Scroll one page right.
		if (curpos < maxpos)
			curpos = min(maxpos, curpos + (int)nPage);
		break;

	case SB_THUMBPOSITION: // Scroll to absolute position. nPos is the position
		curpos = nPos;       // of the scroll box at the end of the drag operation.
		break;

	case SB_THUMBTRACK:  // Drag scroll box to specified position. nPos is the
		curpos = nPos;     // position that the scroll box has been dragged to.
		break;
	}

	// Set the new position of the thumb (scroll box).
	FlatSB_SetScrollPos(GetSafeHwnd(), SB_VERT, curpos, TRUE);

	m_scrollOffset.y = curpos;
	CalcLayout(false);
	Invalidate();

	//CWnd::OnHScroll(nSBCode, nPos, pScrollBar);
}

//////////////////////////////////////////////////////////////////////////
CImageListCtrlItem* CImageListCtrl::HitTest(CPoint point)
{
	CRect rc;
	GetClientRect(rc);
	if (!rc.PtInRect(point))
		return 0;
	for (int i = 0; i < m_items.size(); i++)
	{
		CImageListCtrlItem* pItem = m_items[i];
		if (pItem->rect.PtInRect(point) == TRUE)
			return pItem;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	if (nFlags & MK_LBUTTON)
	{
		if (m_DragDropStatus == eMDDS_Begin)
		{
			if (m_PointWhenHoldingLButton != point)
			{
				if (OnBeginDrag(nFlags, point))
					m_DragDropStatus = eMDDS_InTheMiddle;
				else
					m_DragDropStatus = eMDDS_None;
			}
		}
		else if (m_DragDropStatus == eMDDS_InTheMiddle)
		{

			if (m_DraggedImage.DragMove(GetDragPos(point)))
			{
				OnDraggingItem(point);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

CPoint CImageListCtrl::GetDragPos(const CPoint& point)
{
	CRect rc;
	AfxGetMainWnd()->GetWindowRect(rc);
	CPoint p(point);
	ClientToScreen(&p);
	p.x -= rc.left;
	p.y -= rc.top;

	return p;
}

bool CImageListCtrl::OnBeginDrag(UINT nFlags, CPoint point)
{
	if (!m_pSelectedItem)
		return false;

	CBitmap bitmap;
	CDC* pDC = GetDC();
	if (pDC == NULL)
		return false;

	const int kExtraItemHeight = 0;//kCharacterHeightInItem;

	int itemWidth = m_pSelectedItem->rect.Width();
	int itemHeight = m_pSelectedItem->rect.Height() + kExtraItemHeight;

	if (!bitmap.CreateCompatibleBitmap(pDC, itemWidth, itemHeight))
		return false;

	CDC memDC;
	memDC.CreateCompatibleDC(pDC);
	memDC.SelectObject(bitmap);
	DrawItem(&memDC, m_pSelectedItem, true);

	m_DraggedImage.DeleteImageList();
	bool bCreated = m_DraggedImage.Create(itemWidth, itemHeight, ILC_COLOR16, 1, 2);

	assert(bCreated);
	if (!bCreated)
		return false;

	int nIndex = m_DraggedImage.Add(&bitmap, RGB(255, 255, 255));

	assert(nIndex >= 0);
	if (nIndex < 0)
		return false;

	if (!m_DraggedImage.BeginDrag(0, CPoint(itemWidth / 2, itemHeight / 2)))
	{
		m_DraggedImage.EndDrag();
		if (!m_DraggedImage.BeginDrag(0, CPoint(itemWidth / 2, itemHeight / 2)))
			return false;
	}

	if (!m_DraggedImage.DragEnter(AfxGetMainWnd(), GetDragPos(point)))
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	m_pSelectedItem = HitTest(point);
	if (m_pSelectedItem)
	{
		OnSelectItem(m_pSelectedItem, true);
		m_DragDropStatus = eMDDS_Begin;
		m_PointWhenHoldingLButton = point;
	}
	SetCapture();
	__super::OnLButtonDown(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
	ReleaseCapture();

	if (m_DragDropStatus == eMDDS_InTheMiddle)
	{
		m_DraggedImage.EndDrag();
		m_DragDropStatus = eMDDS_None;

		OnDropItem(point);
	}

	__super::OnLButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnRButtonDown(UINT nFlags, CPoint point)
{
	CImageListCtrlItem* pItem = HitTest(point);
	if (pItem)
	{
		OnSelectItem(pItem, true);
	}
	__super::OnRButtonDown(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnRButtonUp(UINT nFlags, CPoint point)
{
	__super::OnRButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnSelectItem(CImageListCtrlItem* pItem, bool bSelected)
{
	SelectItem(pItem, bSelected);
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::SelectItem(CImageListCtrlItem* pItem, bool bSelect)
{
	assert(pItem);
	if (pItem->bSelected == bSelect)
		return;

	int nStyle = GetStyle();
	if (!(nStyle & ILC_MULTI_SELECTION))
	{
		ResetSelection();
	}
	pItem->bSelected = bSelect;
	InvalidateItemRect(pItem);
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::SelectItem(int nIndex, bool bSelect)
{
	int nStyle = GetStyle();
	if (!(nStyle & ILC_MULTI_SELECTION))
	{
		ResetSelection();
	}
	if (nIndex >= 0 && nIndex < m_items.size())
	{
		ResetSelection();
		SelectItem(m_items[nIndex], bSelect);
	}
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::ResetSelection()
{
	for (int i = 0; i < m_items.size(); i++)
	{
		CImageListCtrlItem* pItem = m_items[i];
		if (pItem->bSelected)
			InvalidateItemRect(pItem);
		pItem->bSelected = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::InvalidateItemRect(CImageListCtrlItem* pItem)
{
	CRect rc = pItem->rect;
	rc.left -= 2;
	rc.top -= 2;
	rc.right += 3;
	rc.bottom += 3 + kCharacterHeightInItem;
	InvalidateRect(rc, FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnUpdateItem(CImageListCtrlItem* pItem)
{
	// Make a bitmap from image.
	assert(pItem->pImage);

	unsigned int* pImageData = pItem->pImage->GetData();
	int w = pItem->pImage->GetWidth();
	int h = pItem->pImage->GetHeight();

	if (pItem->bitmap.GetSafeHandle() != NULL)
		pItem->bitmap.DeleteObject();
	VERIFY(pItem->bitmap.CreateBitmap(w, h, 1, 32, pImageData));
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::InvalidateAllItems()
{
	for (int i = 0; i < m_items.size(); i++)
		m_items[i]->bBitmapValid = false;
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::InvalidateAllBitmaps()
{
	for (int i = 0; i < m_items.size(); i++)
		m_items[i]->bBitmapValid = false;
}

