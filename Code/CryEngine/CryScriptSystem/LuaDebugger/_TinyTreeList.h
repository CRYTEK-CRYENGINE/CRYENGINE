// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _TINYTREELIST_H_
#define _TINYTREELIST_H_

#ifndef _TINY_WINDOW_H_
	#error "_TinyTreeList requires <_TinyWindow.h>"
#endif

#define ID_HEADER 1
#define ID_TREE   2
class _TinyTreeList : public _TinyWindow
{
protected:
	_BEGIN_MSG_MAP(_TinyTreeList)
	_MESSAGE_HANDLER(WM_SIZE, OnSize)
	_MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkGnd)
	_END_MSG_MAP()

public:
	_TinyTreeList()
	{
		m_nNumOfColumns = 0;
	}
	BOOL Create(ULONG nID = 0, const RECT* pRect = NULL, _TinyWindow* pParentWnd = NULL)
	{
		if (!_TinyWindow::Create(NULL, _T(""), WS_CHILD | WS_VISIBLE, NULL, pRect, pParentWnd, nID))
			return FALSE;
		m_ttvTreeView.Create(ID_TREE, WS_CHILD | TVS_HASLINES | TVS_HASBUTTONS | WS_VISIBLE, 0, pRect, this);
		m_thHeader.Create(ID_HEADER, WS_VISIBLE | WS_CHILD | WS_BORDER | HDS_BUTTONS | HDS_HORZ, 0, 0, this);
		if (pParentWnd)
		{
			_TinyRect rect;
			GetClientRect(&rect);
			Reshape(rect.right - rect.left, rect.bottom - rect.top);
		}
		return TRUE;
	}
	BOOL AddCoulumn(LPSTR sName, int nWidth, BOOL bTree = FALSE)
	{
		m_thHeader.InsertItem(m_nNumOfColumns, nWidth, sName);
		m_nNumOfColumns++;
		return TRUE;
	}

private:
	LRESULT OnSize(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		Reshape(LOWORD(lParam), HIWORD(lParam));
		return 0;
	}
	LRESULT OnEraseBkGnd(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		return 1;
	}

	void Reshape(int w, int h)
	{
		m_thHeader.SetWindowPos(0, 0, w, 20, 0);
		m_ttvTreeView.SetWindowPos(0, 20, w, h - 20, 0);
	}
	_TinyTreeView m_ttvTreeView;
	_TinyHeader   m_thHeader;
	int           m_nNumOfColumns;
};

#endif
