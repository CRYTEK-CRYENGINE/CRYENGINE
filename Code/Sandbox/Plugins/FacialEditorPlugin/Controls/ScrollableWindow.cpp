// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ScrollableWindow.h"

BEGIN_MESSAGE_MAP(CScrollableWindow, CWnd)
ON_WM_HSCROLL()
ON_WM_VSCROLL()
ON_WM_SIZE()
END_MESSAGE_MAP()

CScrollableWindow::CScrollableWindow()
{
	m_stDesiredClientSize = CRect(-1, -1, -2, -2);
	m_bHVisible = false;
	m_bVVisible = false;
	m_boShowing = false;
	m_boAutoScrollWindow = true;
}

CScrollableWindow::~CScrollableWindow()
{
}

void CScrollableWindow::SetClientSize(unsigned int nWidth, unsigned int nHeight)
{
	m_stDesiredClientSize = CRect(0, 0, nWidth, nHeight);
	UpdateScrollBars();
}

void CScrollableWindow::SetAutoScrollWindowFlag(bool boAutoScrollWindow)
{
	m_boAutoScrollWindow = boAutoScrollWindow;
}

void CScrollableWindow::OnClientSizeUpdated()
{
	UpdateScrollBars();
}

void CScrollableWindow::UpdateScrollBars()
{
	CRect stCurrentRect;
	CRect clientArea;
	int cx;
	int cy;

	m_boShowing = false;

	if (m_boShowing)
		return;

	GetClientRect(clientArea);
	cx = clientArea.Width();
	cy = clientArea.Height();

	if ((m_stDesiredClientSize.left < 0) || (m_stDesiredClientSize.top < 0))
	{
		stCurrentRect = clientArea;
	}
	else
	{
		stCurrentRect = m_stDesiredClientSize;
	}

	if (m_bHVisible)
		cy += GetSystemMetrics(SM_CYHSCROLL);
	if (m_bVVisible)
		cx += GetSystemMetrics(SM_CXVSCROLL);

	bool newHVis = false;
	bool newVVis = false;

	if (cx < stCurrentRect.Width())
		newHVis = true;
	if (cy < stCurrentRect.Height())
		newVVis = true;
	if (newVVis && !newHVis && cx - GetSystemMetrics(SM_CXVSCROLL) < stCurrentRect.Width())
		newHVis = true;
	if (newHVis && !newVVis && cy - GetSystemMetrics(SM_CYHSCROLL) < stCurrentRect.Height())
		newVVis = true;

	if (m_bHVisible)
		cy -= GetSystemMetrics(SM_CYHSCROLL);
	if (m_bVVisible)
		cx -= GetSystemMetrics(SM_CXVSCROLL);

	if (newHVis && !m_bHVisible)
		cx -= GetSystemMetrics(SM_CXVSCROLL);
	if (newVVis && !m_bVVisible)
		cy -= GetSystemMetrics(SM_CYHSCROLL);

	m_boShowing = true;
	if (m_bHVisible != newHVis)
	{
		ShowScrollBar(SB_HORZ, newHVis);
	}

	SCROLLINFO si;

	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_PAGE | SIF_RANGE;
	si.nPage = cx;
	si.nMax = stCurrentRect.Width();
	si.nMin = 0;

	// If called from a thread different of the thread that owns the control it will
	// cause the window owning the scroll bar to freeze until you click in another window
	// and than click again in the window you were using (who owns the control).
	SetScrollInfo(SB_HORZ, &si);
	if (m_bHVisible != newHVis)
	{
		EnableScrollBarCtrl(SB_HORZ, newHVis);
	}

	if (m_bVVisible != newVVis)
	{
		ShowScrollBar(SB_VERT, newVVis);
	}
	si.nPage = cy;
	si.nMax = stCurrentRect.Height();
	si.nMin = 0;

	// If called from a thread different of the thread that owns the control it will
	// cause the window owning the scroll bar to freeze until you click in another window
	// and than click again in the window you were using (who owns the control).
	SetScrollInfo(SB_VERT, &si);
	if (m_bVVisible != newVVis)
	{
		EnableScrollBarCtrl(SB_VERT, newVVis);
	}
	m_boShowing = false;

	m_bHVisible = newHVis;
	m_bVVisible = newVVis;
}

void CScrollableWindow::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	int nCurPos = GetScrollPos(SB_HORZ);
	int nPrevPos = nCurPos;
	CRect oClientRect;

	GetClientRect(oClientRect);

	switch (nSBCode)
	{
	case SB_LEFT:
		nCurPos = 0;
		break;
	case SB_RIGHT:
		nCurPos = GetScrollLimit(SB_HORZ) - 1;
		break;
	case SB_LINELEFT:
		nCurPos = max(nCurPos - 6, 0);
		break;
	case SB_LINERIGHT:
		nCurPos = min(nCurPos + 6, GetScrollLimit(SB_HORZ) - 1);
		break;
	case SB_PAGELEFT:
		nCurPos = max(nCurPos - oClientRect.Width(), 0);
		break;
	case SB_PAGERIGHT:
		nCurPos = min(nCurPos + oClientRect.Width(), GetScrollLimit(SB_HORZ) - 1);
		break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION:
		{
			SCROLLINFO info;
			if (GetScrollInfo(SB_HORZ, &info, SIF_TRACKPOS))
			{
				nCurPos = info.nTrackPos;
			}
		}
		break;
	}

	SetScrollPos(SB_HORZ, nCurPos);
	if (m_boAutoScrollWindow)
	{
		ScrollWindow(nPrevPos - nCurPos, 0);
	}
	CWnd::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CScrollableWindow::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	int nCurPos = GetScrollPos(SB_VERT);
	int nPrevPos = nCurPos;
	CRect oClientRect;

	GetClientRect(oClientRect);

	switch (nSBCode)
	{
	case SB_LEFT:
		nCurPos = 0;
		break;
	case SB_RIGHT:
		nCurPos = GetScrollLimit(SB_VERT) - 1;
		break;
	case SB_LINELEFT:
		nCurPos = max(nCurPos - 6, 0);
		break;
	case SB_LINERIGHT:
		nCurPos = min(nCurPos + 6, GetScrollLimit(SB_VERT) - 1);
		break;
	case SB_PAGELEFT:
		nCurPos = max(nCurPos - oClientRect.Height(), 0);
		break;
	case SB_PAGERIGHT:
		nCurPos = min(nCurPos + oClientRect.Height(), GetScrollLimit(SB_VERT) - 1);
		break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION:
		{
			SCROLLINFO info;
			if (GetScrollInfo(SB_VERT, &info, SIF_TRACKPOS))
			{
				nCurPos = info.nTrackPos;
			}
		}
		break;
	}

	SetScrollPos(SB_VERT, nCurPos);
	if (m_boAutoScrollWindow)
	{
		ScrollWindow(0, nPrevPos - nCurPos);
	}
	CWnd::OnVScroll(nSBCode, nPos, pScrollBar);
}

void CScrollableWindow::OnSize(UINT nType, int cx, int cy)
{
	UpdateScrollBars();

	CWnd::OnSize(nType, cx, cy);
}

