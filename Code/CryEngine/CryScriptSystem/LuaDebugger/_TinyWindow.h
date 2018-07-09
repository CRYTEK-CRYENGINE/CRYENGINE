// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _TINY_WINDOW_H_
#define _TINY_WINDOW_H_

#pragma warning(disable:4264)

#include <Richedit.h>

#ifndef __TINY_MAIN_H__
	#error "_TinyWindow require <_TinyMain.h>"
#endif

#include <CrySystem/ISystem.h>

///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK     _TinyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK _TinyDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
inline ATOM __RegisterSmartClass(HINSTANCE hInstance, DWORD nIcon)
{
	WNDCLASS wc;

	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = (WNDPROC) _TinyWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(nIcon));
	wc.hCursor = 0;
	// wc.hbrBackground	= (HBRUSH) GetStockObject(LTGRAY_BRUSH);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = 0;
	wc.lpszClassName = _T("_default_TinyWindowClass");

	return RegisterClass(&wc);
}

///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
#define _BEGIN_MSG_MAP(__class)                                                            \
  virtual LRESULT __Tiny_WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) \
  { int wmId = 0, wmEvent = 0;                                                             \
    switch (message) {

#define _BEGIN_DLG_MSG_MAP(__class)                                                        \
  virtual LRESULT __Tiny_WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) \
  { int wmId = 0, wmEvent = 0;                                                             \
    switch (message) {

#define _MESSAGE_HANDLER(__message, __handler)       \
  case __message:                                    \
    return __handler(hWnd, message, wParam, lParam); \
    break;

#define _BEGIN_COMMAND_HANDLER() \
  case WM_COMMAND:               \
    wmId = LOWORD(wParam);       \
    wmEvent = HIWORD(wParam);    \
    switch (wmId) {              \

#define _COMMAND_HANDLER(__wmId, __command_handler)          \
  case __wmId:                                               \
    return __command_handler(hWnd, message, wParam, lParam); \
    break;
#define _DEFAULT_DLG_COMMAND_HANDLERS() \
  case IDOK:                            \
    m_nModalRet = IDOK;                 \
    DestroyWindow(m_hWnd);              \
    break;                              \
  case IDCANCEL:                        \
    m_nModalRet = IDCANCEL;             \
    DestroyWindow(m_hWnd);              \
    break;

#define _BEGIN_CMD_EVENT_FILTER(__wmId) \
  case __wmId:                          \
    switch (wmEvent) {

#define _EVENT_FILTER(__wmEvent, __command_handler)          \
  case __wmEvent:                                            \
    return __command_handler(hWnd, message, wParam, lParam); \
    break;

#define _END_CMD_EVENT_FILTER() \
  default:                      \
    break;                      \
    }                           \
    break;

#define _END_COMMAND_HANDLER()                           \
  default:                                               \
    return DefWindowProc(hWnd, message, wParam, lParam); \
    break;                                               \
    }                                                    \
    break;

#define  _END_MSG_MAP()                                  \
  default:                                               \
    return DefWindowProc(hWnd, message, wParam, lParam); \
    break;                                               \
    }                                                    \
    return 0;                                            \
    }

#define  _END_DLG_MSG_MAP() \
  default:                  \
    return FALSE;           \
    break;                  \
    }                       \
    return 0;               \
    }

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
class _TinyWindow
{

public:
	_TinyWindow()
	{
		m_hWnd = NULL;
	}
	_TinyWindow(HWND hWnd)
	{
		m_hWnd = hWnd;
	}

	virtual ~_TinyWindow()
	{
		Close();
	}
	virtual void Close()
	{
		if (m_hWnd)
		{
			SetWindowLong(m_hWnd, GWLP_USERDATA, NULL);
			DestroyWindow(m_hWnd);
			m_hWnd = NULL;
		}
	}
	virtual void Quit()
	{
		PostQuitMessage(0);
		//		g_bDone=true;
		//Close();

	}

	virtual LRESULT __Tiny_WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) { return ::DefWindowProc(hWnd, message, wParam, lParam); }

	virtual BOOL    Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle = WS_VISIBLE, DWORD dwExStyle = 0, const RECT* pRect = NULL, _TinyWindow* pParentWnd = NULL, ULONG_PTR nID = 0)
	{
		HWND hParent = NULL;
		BOOL bSmart = FALSE;
		int x = CW_USEDEFAULT, y = CW_USEDEFAULT, width = CW_USEDEFAULT, height = CW_USEDEFAULT;
		//class name
		if (!lpszClassName)
		{
			lpszClassName = _T("_default_TinyWindowClass");
			bSmart = TRUE;
		}
		//parent
		if (pParentWnd != NULL) hParent = pParentWnd->m_hWnd;
		//rect
		if (pRect)
		{
			x = pRect->left;
			y = pRect->top;
			width = (pRect->right - pRect->left);
			height = (pRect->bottom - pRect->top);
		}
		//create
		m_hWnd = ::CreateWindowEx(dwExStyle, lpszClassName,
		                          lpszWindowName,
		                          dwStyle,
		                          x,
		                          y,
		                          width,
		                          height,
		                          hParent,
		                          (HMENU)nID,
		                          _Tiny_GetInstance(),
		                          NULL);
		if (!m_hWnd)
		{
			_TINY_CHECK_LAST_ERROR
			return FALSE;
		}
		__Tiny_WindowProc(m_hWnd, WM_CREATE, 0, 0);
		::SetWindowLongPtr(m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
		return TRUE;

	}

	virtual BOOL IsCreated() { return IsWindow(m_hWnd); };

	virtual void MakeChild()
	{
		_TinyAssert(IsCreated());
		DWORD dwStyle = GetWindowLong(m_hWnd, GWL_STYLE);
		dwStyle |= WS_CHILD;
		SetWindowLong(m_hWnd, GWL_STYLE, dwStyle);
	}

	virtual bool Reshape(int w, int h)
	{
		SetWindowPos(0, 0, w, h, SWP_NOZORDER | SWP_NOMOVE);
		return true;
	}

	virtual void CenterOnScreen()
	{
		_TinyRect rc;
		UINT iX = GetSystemMetrics(SM_CXFULLSCREEN);
		UINT iY = GetSystemMetrics(SM_CYFULLSCREEN);
		GetClientRect(&rc);
		SetWindowPos(iX / 2 - rc.right / 2, iY / 2 - rc.bottom / 2, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
	}

public:
	HWND m_hWnd;
public:
	//wrappers
	virtual LRESULT SetTimer(UINT nIDEvent, UINT uElapse)
	{
		return (LRESULT)::SetTimer(m_hWnd, nIDEvent, uElapse, NULL);
	}
	virtual LRESULT KillTimer(UINT nIDEvent)
	{
		return (LRESULT)::KillTimer(m_hWnd, nIDEvent);
	}
	virtual LRESULT ShowWindow(int nCmdShow = SW_SHOW)
	{
		return (LRESULT)::ShowWindow(m_hWnd, nCmdShow);
	}
	virtual LRESULT SendMessage(UINT Msg, WPARAM wParam = 0, LPARAM lParam = 0)
	{
		return (LRESULT)::SendMessage(m_hWnd, Msg, wParam, lParam);
	}
	virtual LRESULT PostMessage(UINT Msg, WPARAM wParam = 0, LPARAM lParam = 0)
	{
		return (LRESULT)::PostMessage(m_hWnd, Msg, wParam, lParam);
	}
	virtual LRESULT SetWindowText(LPCTSTR lpString)
	{
		return (LRESULT)::SetWindowText(m_hWnd, lpString);
	}
	virtual LRESULT GetWindowText(LPTSTR lpString, int nMaxCount)
	{
		return (LRESULT)::GetWindowText(m_hWnd, lpString, nMaxCount);
	}
	virtual LRESULT GetClientRect(_TinyRect* pRect)
	{
		return (LRESULT)::GetClientRect(m_hWnd, pRect);
	}
	virtual LRESULT SetWindowPos(int x, int y, int cx, int cy, UINT flags)
	{
		return (LRESULT)::SetWindowPos(m_hWnd, 0, x, y, cx, cy, flags);
	}
	virtual LRESULT InvalidateRect(_TinyRect* pRect = NULL, BOOL bErase = FALSE)
	{
		return ::InvalidateRect(m_hWnd, pRect, bErase);
	}
	virtual HWND SetParent(_TinyWindow* pParent)
	{
		if (pParent)
			return ::SetParent(m_hWnd, pParent->m_hWnd);
		else
			return ::SetParent(m_hWnd, NULL);
	}
	virtual BOOL SetCapture()
	{
		return ::SetCapture(m_hWnd) ? TRUE : FALSE;
	}
	virtual void NotifyReflection(BOOL bEnable)
	{
		m_bReflectNotify = bEnable ? true : false;
	}
	virtual BOOL HasNotifyReflection()
	{
		return m_bReflectNotify;
	}
private:
	BOOL m_bReflectNotify;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
class _TinyDialog : public _TinyWindow
{
public:
	BOOL Create(LPCTSTR lpTemplate, _TinyWindow* pParent = NULL)
	{
		m_nModalRet = 0;
		HWND hParent = NULL;
		if (pParent) hParent = pParent->m_hWnd;
		m_hWnd = CreateDialog(_Tiny_GetResourceInstance(), lpTemplate, hParent, (DLGPROC) _TinyDlgProc);
		if (!m_hWnd)
		{
			_TINY_CHECK_LAST_ERROR
			return FALSE;
		}
		FakeCreate();
		return TRUE;
	};

	int DoModal(LPCTSTR lpTemplate, _TinyWindow* pParent = NULL)
	{
		INT_PTR nRet = DialogBoxParam(_Tiny_GetResourceInstance(), lpTemplate, (pParent != NULL) ?
		                              pParent->m_hWnd : NULL, (DLGPROC) _TinyDlgProc, (LPARAM) this);
		if (nRet == -1)
		{
			_TINY_CHECK_LAST_ERROR
			return 0;
		}
		return (int)nRet;
	};

	/*
	   int DoModal(){
	   MSG msg;
	   while (GetMessage(&msg, NULL, 0, 0))
	   {
	    TranslateMessage(&msg);
	    DispatchMessage(&msg);
	   }
	   return m_nModalRet;
	   }
	 */

protected:
	void FakeCreate()
	{
		// Fake the WM_CREATE message
		__Tiny_WindowProc(m_hWnd, WM_CREATE, 0, 0);
		::SetWindowLongPtr(m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
	};

public:
	int m_nModalRet;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
class _TinyFrameWindow : public _TinyWindow
{
public:
	_TinyFrameWindow()
	{
		m_hMenu = NULL;
	}
#ifdef WIN_CE
	BOOL AddBarMenu(WORD idMenu)
	{
		m_hCommandBar = ::CommandBar_Create(_Tiny_GetResourceInstance(), m_hWnd, 1);
		::CommandBar_InsertMenubar(m_hCommandBar, _Tiny_GetResourceInstance(), idMenu, 0);
		return ::CommandBar_AddAdornments(m_hCommandBar, 0, 0);
	}
	HWND m_hCommandBar;
#else
	bool AddMenu(WORD idMenu)
	{
		CryFatalError("AddMenu");
		m_hMenu = LoadMenu(_Tiny_GetResourceInstance(), MAKEINTRESOURCE(idMenu));
		//<<FIXME>>
	}
	HMENU m_hMenu;
#endif
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
class _TinyEdit : public _TinyWindow
{
public:
	virtual BOOL Create(ULONG nID = 0, DWORD dwStyle = WS_VISIBLE, DWORD dwExStyle = 0, const RECT* pRect = NULL, _TinyWindow* pParentWnd = NULL)
	{
		BOOL bRes = _TinyWindow::Create(_T("EDIT"), _T(""), dwStyle, dwExStyle, pRect, pParentWnd, nID);
		if (!bRes) return FALSE;
		return TRUE;
	}
	virtual void AppendText(const TCHAR* sText)
	{
		SendMessage(EM_SETSEL, -1, -1);
		SendMessage(EM_REPLACESEL, 0, (LPARAM)sText);
		SendMessage(EM_SCROLLCARET, 0, 0);
	}
	virtual int GetFirstVisibleLine()
	{
		return (int)SendMessage(EM_GETFIRSTVISIBLELINE, 0, 0);
	}
	virtual int GetScrollPos()
	{
		TEXTMETRIC tm;
		POINT pt;
		int iSel = GetSel();
		int iLine = LineFromChar(iSel);
		GetCaretPos(&pt);
		GetTextMetrics(&tm);
		int cyLine = tm.tmHeight;

		// Calculate the first visible line.
		// While the vertical coordinate of the caret is greater than
		// tmHeight, subtract tmHeight from the vertical coordinate and
		// subtract 1 from the line number of the caret.
		// The value remaining in the line number variable is the line
		// number of the first visible line in the edit control.
		int iTopLine = iLine;
		while (pt.y > cyLine)
		{
			pt.y -= cyLine;
			iTopLine--;
		}
		return iTopLine;
	}

	virtual int LineFromChar(int nSel)
	{
		return (int)SendMessage(EM_LINEFROMCHAR, nSel, 0L);
	}
	virtual int GetSel()
	{
		return (int)SendMessage(EM_GETSEL, NULL, 0L);
	}
	virtual void GetTextMetrics(TEXTMETRIC* tm)
	{
		HDC hDC;
		HFONT hFont;
		hDC = GetDC(m_hWnd);
		hFont = (HFONT)SendMessage(WM_GETFONT, 0, 0L);
		if (hFont != NULL)
			SelectObject(hDC, hFont);
		::GetTextMetrics(hDC, (TEXTMETRIC*)tm);
		ReleaseDC(m_hWnd, hDC);
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
class _TinyRichEdit : public _TinyWindow
{
public:
	virtual BOOL Create(ULONG nID = 0, DWORD dwStyle = WS_VISIBLE, DWORD dwExStyle = 0, const RECT* pRect = NULL, _TinyWindow* pParentWnd = NULL)
	{
		BOOL bRes = _TinyWindow::Create(RICHEDIT_CLASS, _T(""), dwStyle, dwExStyle, pRect, pParentWnd, nID);
		int n = ::GetLastError();
		if (!bRes) return FALSE;
		return TRUE;
	}
	virtual void AppendText(const TCHAR* sText)
	{
		SendMessage(EM_SETSEL, -1, -1);
		SendMessage(EM_REPLACESEL, 0, (LPARAM)sText);
		SendMessage(EM_SCROLLCARET, 0, 0);
	}
	virtual int GetFirstVisibleLine()
	{
		return (int)SendMessage(EM_GETFIRSTVISIBLELINE, 0, 0);
	}

	virtual int GetCenterLine()
	{
		return static_cast<int>(static_cast<float>((GetLastVisibleLine() - GetFirstVisibleLine())) * 0.5f);
	}

	virtual int GetLastVisibleLine()
	{
		RECT rfFormattingRect = { 0 };
		SendMessage(EM_GETRECT, 0, (LPARAM)&rfFormattingRect);

		rfFormattingRect.left++;
		rfFormattingRect.bottom -= 2;

		POINT point;
		point.x = rfFormattingRect.left;
		point.y = rfFormattingRect.bottom;
		int nCharIndex = (int)SendMessage(EM_CHARFROMPOS, 0, (LPARAM)&point);

		return LineFromChar(nCharIndex - 1);
	}

	virtual int GetLineCount()
	{
		return (int)SendMessage(EM_GETLINECOUNT, 0, 0);
	}

	virtual int GetScrollPos(POINT* pt)
	{
		return (int)SendMessage(EM_GETSCROLLPOS, 0, (LPARAM)pt);
	}

	virtual void ScrollToLine(UINT iLine)
	{
		UINT iChar = (UINT)SendMessage(EM_LINEINDEX, iLine, 0);
		UINT iLineLength = (UINT)SendMessage(EM_LINELENGTH, iChar - 1, 0);
		SendMessage(EM_SETSEL, iChar, iChar);
		SendMessage(EM_LINESCROLL, 0, -99999);
		SendMessage(EM_LINESCROLL, 0, iLine - GetCenterLine());
	}

	virtual int LineFromChar(int nSel)
	{
		int nLine = (int)SendMessage(EM_EXLINEFROMCHAR, (WPARAM)0, (LPARAM)nSel) + 1;
		return nLine;
	}

	virtual int GetSel()
	{
		CHARRANGE chr;
		SendMessage(EM_EXGETSEL, (WPARAM)0, (LPARAM)&chr);
		return chr.cpMin;
	}

	virtual void GetTextMetrics(TEXTMETRIC* tm)
	{
		HDC hDC;
		HFONT hFont;
		hDC = GetDC(m_hWnd);
		hFont = (HFONT)SendMessage(WM_GETFONT, 0, 0L);
		if (hFont != NULL)
			SelectObject(hDC, hFont);
		::GetTextMetrics(hDC, (TEXTMETRIC*)tm);
		ReleaseDC(m_hWnd, hDC);
	}
	virtual void SetFont(HFONT hFont)
	{
		SendMessage(WM_SETFONT, (WPARAM) hFont, (LPARAM) 0);
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
class _TinyStatic : public _TinyWindow
{
public:
	virtual BOOL Create(ULONG nID = 0, DWORD dwStyle = WS_VISIBLE, DWORD dwStyleEx = 0, const RECT* pRect = NULL, _TinyWindow* pParentWnd = NULL)
	{
		BOOL bRes = _TinyWindow::Create(_T("STATIC"), _T(""), dwStyle, dwStyleEx, pRect, pParentWnd, nID);
		if (!bRes) return FALSE;
		return TRUE;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
class _TinyToolbar : public _TinyWindow
{
public:
	virtual BOOL Create(ULONG nMenuID = 0, DWORD dwStyle = WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS, DWORD dwStyleEx = 0, const RECT* pRect = NULL, _TinyWindow* pParentWnd = NULL)
	{
		BOOL bRes = _TinyWindow::Create(TOOLBARCLASSNAME, _T(""), dwStyle, dwStyleEx, pRect, pParentWnd, nMenuID);
		if (!bRes)
			return FALSE;
		SendMessage(TB_BUTTONSTRUCTSIZE, (WPARAM) sizeof(TBBUTTON), 0);
		return TRUE;
	}

	virtual BOOL AddButtons(DWORD nBitmapID, TBBUTTON* pButtons, DWORD nNumOfButtons)
	{
		TBADDBITMAP tb;
		tb.hInst = _Tiny_GetResourceInstance();
		tb.nID = nBitmapID;
		DWORD stdidx = (DWORD)SendMessage(TB_ADDBITMAP, nNumOfButtons, (LPARAM)&tb);
		for (DWORD index = 0; index < nNumOfButtons; index++)
			pButtons[index].iBitmap += stdidx;
		SendMessage(TB_ADDBUTTONS, nNumOfButtons, (LPARAM) pButtons);
		return TRUE;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
class _TinyListBox : public _TinyWindow
{
public:
	virtual BOOL Create(ULONG nID = 0, DWORD dwStyle = WS_CHILD, DWORD dwStyleEx = 0, const RECT* pRect = NULL, _TinyWindow* pParentWnd = NULL)
	{
		BOOL bRes = _TinyWindow::Create(_T("LISTBOX"), _T(""), dwStyle, dwStyleEx, pRect, pParentWnd, nID);
		if (!bRes) return FALSE;
		return TRUE;
	}
	virtual int AddString(const TCHAR* sText)
	{
		return (int)SendMessage(LB_ADDSTRING, 0, (LPARAM)sText);
	}
	virtual int GetSelection()
	{
		int nIndex = LB_ERR;
		int res = (int)SendMessage(LB_GETSEL, (WPARAM)&nIndex, 0);
		if (res == LB_ERR)
			return LB_ERR;
		return nIndex;
	}

};
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
class _TinyListView : public _TinyWindow
{
public:
	virtual BOOL Create(ULONG nID = 0, DWORD dwStyle = WS_CHILD, DWORD dwStyleEx = 0, const RECT* pRect = NULL, _TinyWindow* pParentWnd = NULL)
	{
		BOOL bRes = _TinyWindow::Create(WC_LISTVIEW, _T(""), dwStyle, dwStyleEx, pRect, pParentWnd, nID);
		if (!bRes) return FALSE;
		return TRUE;
	}
	virtual void SetViewStyle(DWORD dwView)
	{
		// Retrieve the current window style.
		DWORD dwStyle = GetWindowLong(m_hWnd, GWL_STYLE);

		// Only set the window style if the view bits have changed.
		if ((dwStyle & LVS_TYPEMASK) != dwView)
			SetWindowLong(m_hWnd, GWL_STYLE,
			              (dwStyle & ~LVS_TYPEMASK) | dwView);
	}
	virtual int InsertColumn(int nCol, LPCTSTR lpszColumnHeading, int nWidth, int nSubItem = 0, int nFormat = LVCFMT_LEFT)
	{
		LVCOLUMN lvc;
		lvc.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
		lvc.cx = nWidth;
		lvc.pszText = (LPSTR)lpszColumnHeading;
		lvc.iSubItem = nSubItem;
		return ListView_InsertColumn(m_hWnd, nCol, &lvc);

	}
	virtual int InsertItem(int nCol, LPCTSTR pszText, int iSubItem = 0, int nParam = NULL)
	{
		LVITEM lvi;
		lvi.mask = LVIF_PARAM | LVIF_TEXT;
		lvi.iItem = nCol;
		lvi.iSubItem = iSubItem;
		lvi.pszText = (LPTSTR)pszText;
		lvi.lParam = (LPARAM) nParam;
		return ListView_InsertItem(m_hWnd, &lvi);
	}

	virtual int SetItemText(int nItem, int nSubItem, LPCTSTR pszText)
	{
		ListView_SetItemText(m_hWnd, nItem, nSubItem, (LPTSTR)pszText);
		return 0;
	}
	virtual int GetItemText(int iItem, int iSubItem, LPTSTR pszText, int cchTextMax)
	{
		ListView_GetItemText(m_hWnd, iItem, iSubItem, pszText, cchTextMax);
		return 0;
	}
	virtual int GetItemCount()
	{
		return (int)SendMessage(LVM_GETITEMCOUNT, (WPARAM)0, (LPARAM)0);
	}
	virtual int GetSelection()
	{
		return ListView_GetSelectionMark(m_hWnd); //-1 mean no selection
	}
	virtual void Clear() { ListView_DeleteAllItems(m_hWnd); };

};

class _TinyHeader : public _TinyWindow
{
public:
	virtual BOOL Create(ULONG nID = 0, DWORD dwStyle = WS_VISIBLE | WS_CHILD | WS_BORDER | HDS_BUTTONS | HDS_HORZ, DWORD dwStyleEx = 0, const RECT* pRect = NULL, _TinyWindow* pParentWnd = NULL)
	{
		BOOL bRes = _TinyWindow::Create(WC_HEADER, _T(""), dwStyle, dwStyleEx, pRect, pParentWnd, nID);
		if (!bRes)
			return FALSE;
		/*if(pParentWnd)
		   {
		   _TinyRect rect;
		   pParentWnd->GetClientRect(&rect);
		   HDLAYOUT hdl;
		   WINDOWPOS wp;
		   hdl.prc = &rect;
		   hdl.pwpos = &wp;
		   if (!SendMessage(HDM_LAYOUT, 0, (LPARAM) &hdl))
		    return FALSE;
		   SetWindowPos(wp.x, wp.y,wp.cx, wp.cy, wp.flags | SWP_SHOWWINDOW);
		   }*/
		return TRUE;

	}
	virtual int InsertItem(int iInsertAfter, int nWidth, LPSTR lpsz)
	{
		HDITEM hdi;
		int index;

		hdi.mask = HDI_TEXT | HDI_FORMAT | HDI_WIDTH;
		hdi.pszText = lpsz;
		hdi.cxy = nWidth;
		hdi.cchTextMax = lstrlen(hdi.pszText);
		hdi.fmt = HDF_LEFT | HDF_STRING;

		index = (int)SendMessage(HDM_INSERTITEM,
		                         (WPARAM) iInsertAfter, (LPARAM) &hdi);

		return index;

	}

};
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
class _TinyTreeView : public _TinyWindow
{
public:
	virtual BOOL Create(ULONG nID = 0, DWORD dwStyle = WS_CHILD | TVS_HASLINES | TVS_HASBUTTONS | WS_VISIBLE, DWORD dwStyleEx = 0, const RECT* pRect = NULL, _TinyWindow* pParentWnd = NULL)
	{
		BOOL bRes = _TinyWindow::Create(WC_TREEVIEW, _T(""), dwStyle, dwStyleEx, pRect, pParentWnd, nID);
		NotifyReflection(TRUE);
		if (!bRes)
			return FALSE;
		return TRUE;

	}
	virtual HTREEITEM AddItemToTree(LPTSTR lpszItem, LPARAM ud = NULL, HTREEITEM hParent = NULL, UINT iImage = 0)
	{
		TVITEM tv;
		TVINSERTSTRUCT tvins;
		static HTREEITEM hPrev = (HTREEITEM) TVI_FIRST;
		memset(&tv, 0, sizeof(TVITEM));
		tv.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		tv.pszText = lpszItem;
		tv.lParam = ud;
		tv.iImage = iImage;
		tv.iSelectedImage = iImage;
		tvins.item = tv;

		tvins.hInsertAfter = hPrev;
		if (hParent == NULL)
			tvins.hParent = TVI_ROOT;
		else
			tvins.hParent = hParent;
		hPrev = (HTREEITEM) SendMessage(TVM_INSERTITEM, 0,
		                                (LPARAM) (LPTVINSERTSTRUCT) &tvins);

		TreeView_SortChildren(m_hWnd, hParent, 0);
		return hPrev;
	};

	virtual void SetImageList(HIMAGELIST hLst)
	{
		_TinyAssert(m_hWnd);
		TreeView_SetImageList(m_hWnd, hLst, TVSIL_NORMAL);
	};

	HTREEITEM GetSelectedItem() { return TreeView_GetSelection(m_hWnd); };

	LPARAM    GetItemUserData(HTREEITEM hItem)
	{
		TVITEM tvItem;
		BOOL bRet;

		tvItem.hItem = hItem;
		tvItem.mask = TVIF_HANDLE;

		bRet = TreeView_GetItem(m_hWnd, &tvItem);
		_TinyAssert(bRet);

		return tvItem.lParam;
	};

	void Expand(HTREEITEM hItem)
	{
		TreeView_Expand(m_hWnd, hItem, TVE_EXPAND);
	};

	void Clear() { TreeView_DeleteAllItems(m_hWnd); };
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
class _TinyScrollBar : public _TinyWindow
{
public:
	virtual BOOL Create(ULONG nID = 0, DWORD dwStyle = WS_VISIBLE, DWORD dwStyleEx = 0, const RECT* pRect = NULL, _TinyWindow* pParentWnd = NULL)
	{
		BOOL bRes = _TinyWindow::Create(_T("SCROLLBAR"), _T(""), dwStyle, dwStyleEx, pRect, pParentWnd, nID);
		int n = GetLastError();
		if (SBS_VERT & dwStyle) m_nBar = SB_VERT;
		if (SBS_HORZ & dwStyle) m_nBar = SB_HORZ;
		if (!bRes) return FALSE;
		return TRUE;
	}
	virtual BOOL SetScrollPos(int nPos, BOOL bRedraw = TRUE)
	{
		return ::SetScrollPos(m_hWnd, SB_CTL, nPos, bRedraw);
	}
	virtual BOOL SetScrollInfo(LPSCROLLINFO lpsi, BOOL bRedraw = TRUE)
	{
		return ::SetScrollInfo(m_hWnd, SB_CTL, lpsi, bRedraw);
	}
	virtual int GetScrollPos()
	{
		SCROLLINFO si;
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_POS;
		::GetScrollInfo(m_hWnd, SB_CTL, &si);
		return si.nPos;
	}
private:
	int m_nBar;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
static LRESULT CALLBACK _TinyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	_TinyWindow* pWnd;
	pWnd = (_TinyWindow*)::GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (pWnd)
	{
		if (message == WM_NOTIFY && pWnd->HasNotifyReflection())
		{
			HWND hParentWnd = ::GetParent(hWnd);
			if (hParentWnd)
			{
				_TinyWndProc(hParentWnd, WM_NOTIFY, wParam, lParam);
			}
		}
		return pWnd->__Tiny_WindowProc(hWnd, message, wParam, lParam);
	}
	else
	{

		return DefWindowProc(hWnd, message, wParam, lParam);
	}
}

static BOOL CALLBACK _TinyDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	_TinyWindow* pWnd = NULL;
	if (message == WM_INITDIALOG)
	{
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) lParam);
		pWnd = reinterpret_cast<_TinyWindow*>(lParam);
		_TinyAssert(!IsBadReadPtr(pWnd, sizeof(_TinyWindow)));
		pWnd->m_hWnd = hWnd;
	}
	pWnd = (_TinyWindow*)::GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (pWnd)
		return (BOOL) pWnd->__Tiny_WindowProc(hWnd, message, wParam, lParam);
	else
		return FALSE;
}

#pragma warning(default:4264)

#endif //_TINY_WINDOW_H_
