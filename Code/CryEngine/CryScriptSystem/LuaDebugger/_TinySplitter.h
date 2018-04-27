// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _TINYSPLITTER_H_
#define _TINYSPLITTER_H_

#ifndef _TINY_WINDOW_H_
	#error "_TinySplitter require <_TinyWindow.h>"
#endif

#define SPLITTER_WIDTH 3
#define DRAW_RAISED_SPLITTER

class _TinySplitter : public _TinyWindow
{
public:
	virtual BOOL Create(_TinyWindow* pParentWnd = NULL, _TinyWindow* pPan0 = NULL, _TinyWindow* pPan1 = NULL, bool bVertical = false, const RECT* pRect = NULL)
	{
		BOOL bRes = _TinyWindow::Create(_T("_default_TinyWindowClass"), _T(""), WS_VISIBLE | WS_CHILD | WS_CLIPCHILDREN, NULL, pRect, pParentWnd);
		if (!bRes) return FALSE;
		m_bVertical = bVertical;
		m_bDragging = false;
		SetFirstPan(pPan0);
		SetSecondPan(pPan1);
		CenterSplitter();
		NotifyReflection(TRUE);
		return TRUE;
	}

	void SetFirstPan(_TinyWindow* pWnd)
	{
		m_pPan0 = pWnd;
		if (m_pPan0 != NULL)
		{
			m_pPan0->SetParent(this);
			m_pPan0->MakeChild();
			m_pPan0->NotifyReflection(TRUE);
		}
	};
	void SetSecondPan(_TinyWindow* pWnd)
	{
		m_pPan1 = pWnd;
		if (m_pPan1)
		{
			m_pPan1->SetParent(this);
			m_pPan1->MakeChild();
			m_pPan1->NotifyReflection(TRUE);
		}
	};

	bool Reshape(int w, int h)
	{
		if (m_pPan0 == NULL || m_pPan1 == NULL)
			return false;
		const int iHlfSplitterWdh = SPLITTER_WIDTH / 2;
		SetWindowPos(0, 0, w, h, SWP_NOZORDER | SWP_NOMOVE);

		if (m_bVertical)
		{
			int iEndUpper = ReverseYAxis(m_iSplitterPos) - iHlfSplitterWdh;
			m_pPan0->Reshape(w, iEndUpper);
			m_pPan0->SetWindowPos(0, 0, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
			m_pPan1->Reshape(w, h - (iEndUpper + iHlfSplitterWdh) - 2);
			m_pPan1->SetWindowPos(0, iEndUpper + iHlfSplitterWdh * 2, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
		}
		else
		{
			int iEndLeft = m_iSplitterPos - iHlfSplitterWdh;
			m_pPan0->Reshape(iEndLeft, h);
			m_pPan0->SetWindowPos(0, 0, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
			m_pPan1->Reshape(w - (iEndLeft + iHlfSplitterWdh) - 2, h);
			m_pPan1->SetWindowPos(iEndLeft + iHlfSplitterWdh * 2, 0, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
		}

		return true;
	}

	bool Reshape()
	{
		_TinyRect rc;
		GetClientRect(&rc);
		return Reshape(rc.right, rc.bottom);
	}

	void CenterSplitter()
	{
		if (IsCreated())
		{
			HWND hWndParent = GetParent(m_hWnd);
			if (IsWindow(hWndParent))
			{
				_TinyRect rc;
				::GetClientRect(hWndParent, &rc);
				m_iSplitterPos = m_bVertical ? rc.bottom / 2 : rc.right / 2;
			}
		}
	}

	UINT GetSplitterPos() const    { return m_iSplitterPos; };
	void SetSplitterPos(UINT iPos) { m_iSplitterPos = iPos; };

protected:
	_BEGIN_MSG_MAP(CSourceEdit)
	_MESSAGE_HANDLER(WM_PAINT, OnPaint)
	_MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
	_MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseLeave)
	_MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
	_MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
	_END_MSG_MAP()

	void ObtainSplitterRect(_TinyRect& rcOut)
	{
		const int iHlfSplitterWdh = SPLITTER_WIDTH / 2;
		GetClientRect(&rcOut);
		if (m_bVertical)
		{
			rcOut.top = ReverseYAxis(m_iSplitterPos) - iHlfSplitterWdh - 2;
			rcOut.bottom = ReverseYAxis(m_iSplitterPos) + iHlfSplitterWdh + 2;
		}
		else
		{
			rcOut.left = m_iSplitterPos - iHlfSplitterWdh;
			rcOut.right = m_iSplitterPos + iHlfSplitterWdh;
		}
	}

	void EnableMouseTracking()
	{
		// Enable hover / leave messages
		TRACKMOUSEEVENT evt;
		evt.cbSize = sizeof(TRACKMOUSEEVENT);
		evt.dwFlags = TME_HOVER | TME_LEAVE;
		evt.hwndTrack = m_hWnd;
		evt.dwHoverTime = 1;
		_TrackMouseEvent(&evt);
	}

	LRESULT OnMouseMove(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		// To receive the leave message
		EnableMouseTracking();

		HCURSOR hCur = LoadCursor(NULL, m_bVertical ? IDC_SIZENS : IDC_SIZEWE);
		_TinyAssert(hCur != NULL);
		SetCursor(hCur);

		if (m_bDragging)
		{
			int xPos = _TINY_GET_X_LPARAM(lParam);
			int yPos = _TINY_GET_Y_LPARAM(lParam);
			m_iSplitterPos = m_bVertical ? ReverseYAxis(yPos) : xPos;
			MoveSplitterToValidPos();
			Reshape();
		}

		return 0;
	}

	LRESULT OnMouseLeave(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		HCURSOR hCur = LoadCursor(NULL, IDC_ARROW);
		SetCursor(hCur);

		return 0;
	}

	LRESULT OnLButtonDown(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		m_bDragging = true;
		SetCapture();

		return 0;
	}

	LRESULT OnLButtonUp(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		m_bDragging = false;
		_TinyVerify(ReleaseCapture());

		int xPos = _TINY_GET_X_LPARAM(lParam);
		int yPos = _TINY_GET_Y_LPARAM(lParam);
		m_iSplitterPos = m_bVertical ? ReverseYAxis(yPos) : xPos;
		MoveSplitterToValidPos();
		Reshape();

		return 0;
	}

	LRESULT OnPaint(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		_TinyRect rc;
		PAINTSTRUCT ps;
		HDC hDC;
		::BeginPaint(m_hWnd, &ps);
		hDC = ::GetDC(m_hWnd);

		GetClientRect(&rc);
		::FillRect(hDC, &rc, GetSysColorBrush(COLOR_BTNFACE));

		ObtainSplitterRect(rc);
#ifdef DRAW_RAISED_SPLITTER
		DrawFrameControl(hDC, &rc, DFC_BUTTON, DFCS_BUTTONPUSH);
#else
		DrawFrameControl(hDC, &rc, DFC_BUTTON, DFCS_BUTTONPUSH | DFCS_PUSHED);
#endif
		EndPaint(m_hWnd, &ps);

		return 0;
	}

	void MoveSplitterToValidPos()
	{
		return; // TODO
		_TinyRect rc;
		GetClientRect(&rc);
		m_iSplitterPos = __min(__max(m_iSplitterPos, SPLITTER_WIDTH), rc.bottom - SPLITTER_WIDTH);
	}

	UINT ReverseYAxis(UINT iY)
	{
		_TinyRect rc;
		_TinyVerify(GetClientRect(&rc));
		return rc.bottom - iY;
	}

	long         m_iSplitterPos;
	_TinyWindow* m_pPan0;
	_TinyWindow* m_pPan1;
	bool         m_bVertical;
	bool         m_bDragging;

};

#endif
