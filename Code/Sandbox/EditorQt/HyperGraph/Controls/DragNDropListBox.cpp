// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DragNDropListBox.h"

IMPLEMENT_DYNAMIC(CDragNDropListBox, CListBox)

CDragNDropListBox::CDragNDropListBox(const DragNDropCallback &cb)
	: m_dropCb(cb)
	, m_isDragging(false)
	, m_itemPrevIdx(LB_ERR)
	, m_itemNextIdx(LB_ERR)
	, m_Interval(0)
{}

BEGIN_MESSAGE_MAP(CDragNDropListBox, CListBox)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_SETCURSOR()
	ON_WM_CAPTURECHANGED()
END_MESSAGE_MAP()


void CDragNDropListBox::DrawLine(int idx, bool isActiveLine)
{
	// unlike CDragListBox, no return on idx=LB_ERR, that means it's the last position

	CDC *pDC = GetDC(); // device context

	CRect clientRect;
	GetClientRect(&clientRect);
	CRect itemRect;

	int halfLineWidth = 1;
	COLORREF lineColor = (isActiveLine ? GetSysColor(COLOR_HIGHLIGHT) : GetSysColor(COLOR_WINDOW));

	CPen Pen(PS_SOLID, halfLineWidth<<1, lineColor);
	CPen *pOldPen = pDC->SelectObject(&Pen);

	if (idx != LB_ERR)
	{
		GetItemRect(idx, &itemRect);
		if (clientRect.PtInRect(itemRect.TopLeft()))
		{
			pDC->MoveTo(itemRect.left + halfLineWidth, itemRect.top + halfLineWidth);
			pDC->LineTo(itemRect.right - halfLineWidth, itemRect.top + halfLineWidth);
		}
	}
	else
	{
		GetItemRect(GetCount() - 1, &itemRect);
		if (clientRect.PtInRect(CPoint(0, itemRect.bottom)))
		{
			pDC->MoveTo(itemRect.left + halfLineWidth, itemRect.bottom + halfLineWidth);
			pDC->LineTo(itemRect.right - halfLineWidth, itemRect.bottom + halfLineWidth);
		}
	}

	pDC->SelectObject(pOldPen);
	ReleaseDC(pDC);
}


void CDragNDropListBox::UpdateSelection(int hoverIdx)
{
	if (m_itemNextIdx != m_itemPrevIdx)
	{
		DrawLine(m_itemNextIdx, false);
	}
	m_itemNextIdx = hoverIdx;

	if (m_itemNextIdx != m_itemPrevIdx)
	{
		DrawLine(m_itemNextIdx, true);
	}

}

void CDragNDropListBox::Scroll(CPoint Point, CRect ClientRect)
{
	if (Point.y > ClientRect.Height())
	{
		DWORD Interval = 250 / (1 + ((Point.y - ClientRect.Height()) / GetItemHeight(0)));
		if (Interval != m_Interval)
		{
			m_Interval = Interval;
			SetTimer(TID_SCROLLDOWN, Interval, NULL);
			OnTimer(TID_SCROLLDOWN);
		}
	}
	else if (Point.y < 0)
	{
		DWORD Interval = 250 / (1 + (abs(Point.y) / GetItemHeight(1)));
		if (Interval != m_Interval)
		{
			m_Interval = Interval;
			SetTimer(TID_SCROLLUP, Interval, NULL);
			OnTimer(TID_SCROLLUP);
		}
	}
	else
	{
		StopScrolling();
	}
}

void CDragNDropListBox::StopScrolling()
{
	KillTimer(TID_SCROLLDOWN);
	KillTimer(TID_SCROLLUP);
	m_Interval = 0;
}

void CDragNDropListBox::OnTimer(UINT nIDEvent)
{
	if (nIDEvent == TID_SCROLLDOWN)
	{
		SetTopIndex(GetTopIndex() + 1);
	}
	else if (nIDEvent == TID_SCROLLUP)
	{
		SetTopIndex(GetTopIndex() - 1);
	}

	CListBox::OnTimer(nIDEvent);
}


void CDragNDropListBox::OnDrop(int curIdx, int newIdx)
{
	if (m_dropCb)
	{
		m_dropCb(curIdx, newIdx);
	}
}

void CDragNDropListBox::DropDraggedItem()
{
	if (m_itemNextIdx == LB_ERR)
	{
		m_itemNextIdx = GetCount();
	}

	if (m_itemPrevIdx == m_itemNextIdx || m_itemPrevIdx + 1 == m_itemNextIdx)
	{
		// dropped on top of self. Nothing to do here
		return;
	}

	DWORD_PTR pItemData = GetItemData(m_itemPrevIdx);
	CString sItemText;
	GetText(m_itemPrevIdx, sItemText);

	SetRedraw(FALSE);

	int topIdx = GetTopIndex();
	int newIdx;
	if (m_itemPrevIdx < m_itemNextIdx)
	{
		newIdx = InsertString(m_itemNextIdx, sItemText);
		SetItemData(newIdx, pItemData);
		DeleteString(m_itemPrevIdx);
		SetCurSel(newIdx - 1);
	}
	else if (m_itemPrevIdx > m_itemNextIdx)
	{
		DeleteString(m_itemPrevIdx);
		newIdx = InsertString(m_itemNextIdx, sItemText);
		SetItemData(newIdx, pItemData);
		SetCurSel(newIdx);
	}
	SetTopIndex(topIdx);

	SetRedraw(TRUE);

	GetParent()->SendMessage(WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), LBN_SELCHANGE), (LPARAM)m_hWnd);

	OnDrop(m_itemPrevIdx, newIdx);
}


void CDragNDropListBox::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (!m_isDragging)
	{
		CListBox::OnLButtonDown(nFlags, point);
	}

	// clear state
	m_isDragging = false;
	m_itemPrevIdx = LB_ERR;
	m_itemNextIdx = LB_ERR;
	m_Interval = 0;

	// get index of clicked item
	BOOL bOutside;
	int idx = ItemFromPoint(point, bOutside);
	if (idx != LB_ERR && !bOutside)
	{
		m_itemPrevIdx = idx;
		SetCurSel(idx);
	}
}

void CDragNDropListBox::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_isDragging && m_itemPrevIdx != LB_ERR)
	{
		// check if button was released inside listBox area and, if so, drop item
		CRect rect;
		GetClientRect(&rect);
		if (rect.PtInRect(point))
		{
			DropDraggedItem();
		}

		Invalidate();
		UpdateWindow();

		// release mouse capture, this will reset the dragging state
		ReleaseCapture();
	}

	CListBox::OnLButtonUp(nFlags, point);
}

void CDragNDropListBox::OnCaptureChanged(CWnd* pWnd)
{
	// clears the dragging state but doesn't imply the drop
	// this is called when releasing from LButtonUp, but also when alt-tabbing
	// also called when the mouse is captured
	if (m_isDragging && m_itemPrevIdx != LB_ERR)
	{
		StopScrolling();

		m_itemPrevIdx = LB_ERR;
		m_itemNextIdx = LB_ERR;
		m_isDragging = false;
	}
}

void CDragNDropListBox::OnMouseMove(UINT nFlags, CPoint point)
{
	if (!m_isDragging)
	{
		CListBox::OnMouseMove(nFlags, point);
	}

	if (nFlags & MK_LBUTTON)
	{
		// start d&d operation
		if (!m_isDragging && m_itemPrevIdx != LB_ERR)
		{
			SetCapture();
			m_isDragging = true;
			m_itemNextIdx = m_itemPrevIdx;
		}

		// get nearest item, update visual feedback and scroll if needed
		BOOL bOutside;
		int idx = ItemFromPoint(point, bOutside);
		if (bOutside)
		{
			CRect rect;
			GetClientRect(&rect);

			// if still inside area, but not on top of an item, we are at the bottom of the list
			if (rect.PtInRect(point))
			{
				StopScrolling();
				idx = LB_ERR;
				bOutside = FALSE;
			}
			else
			{
				Scroll(point, rect);
			}
		}
		else
		{
			// perfectly inside -> no scrolling
			StopScrolling();
		}

		// hovering over a new 'nextId' -> move the visual feedback
		if (idx != m_itemNextIdx && !bOutside)
		{
			UpdateSelection(idx);
		}
	}
}



