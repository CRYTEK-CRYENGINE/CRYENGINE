// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SCROLLABLEDIALOG_H__
#define __SCROLLABLEDIALOG_H__

#include "Util.h"

template <class T> class CScrollableDialog : public CDialogImpl<T>
{
public:
	typedef T DescendentType;

	CScrollableDialog()
		: m_nCurHeight(-1)
		, m_nScrollPos(-1)
	{
	}
	
	BEGIN_MSG_MAP(CScrollableDialog)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_VSCROLL, OnVScroll)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		DEFAULT_REFLECTION_HANDLER()
	END_MSG_MAP()
	
	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnVScroll(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

private:
	int m_nCurHeight;
	int m_nScrollPos;
	RECT m_rect;
};

template <class T> LRESULT CScrollableDialog<T>::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	GetWindowRect(&m_rect);
	m_nScrollPos = 0;
	return FALSE;
}

template <class T> LRESULT CScrollableDialog<T>::OnVScroll(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	UINT nSBCode = LOWORD(wParam);
	UINT nPos = HIWORD(wParam); // Only valid for SB_THUMBPOSITION or SB_THUMBTRACK.

	int nDelta;
	int nMaxPos = m_rect.bottom - m_rect.top - m_nCurHeight;

	switch (nSBCode)
	{
	case SB_LINEDOWN:
		if (m_nScrollPos >= nMaxPos)
			return 0;
		nDelta = Util::getMin(nMaxPos/10,nMaxPos-m_nScrollPos);
		break;

	case SB_LINEUP:
		if (m_nScrollPos <= 0)
			return 0;
		nDelta = -Util::getMin(nMaxPos/10,m_nScrollPos);
		break;

	case SB_PAGEDOWN:
		if (m_nScrollPos >= nMaxPos)
			return 0;
		nDelta = Util::getMin(nMaxPos/4,nMaxPos-m_nScrollPos);
		break;

	case SB_THUMBPOSITION:
		nDelta = (int)nPos - m_nScrollPos;
		break;

	case SB_PAGEUP:
		if (m_nScrollPos <= 0)
			return 0;
		nDelta = -Util::getMin(nMaxPos/4,m_nScrollPos);
		break;

	default:
		return 0;
	}

	m_nScrollPos += nDelta;
	SetScrollPos(SB_VERT,m_nScrollPos,TRUE);
	ScrollWindow(0,-nDelta);

	return 0;
}

template <class T> LRESULT CScrollableDialog<T>::OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	int cy = HIWORD(lParam);

	m_nCurHeight = cy;
	int nScrollMax;
	if (cy < m_rect.bottom - m_rect.top)
		nScrollMax = m_rect.bottom - m_rect.top - cy;
	else
		nScrollMax = 0;

	SCROLLINFO si;
	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_ALL; // SIF_ALL = SIF_PAGE | SIF_RANGE | SIF_POS;
	si.nMin = 0;
	si.nMax = nScrollMax;
	si.nPage = si.nMax/4;
	si.nPos = 0;
	SetScrollInfo(SB_VERT, &si, TRUE); 

	return 0;
}

#endif //__SCROLLABLEDIALOG_H__
