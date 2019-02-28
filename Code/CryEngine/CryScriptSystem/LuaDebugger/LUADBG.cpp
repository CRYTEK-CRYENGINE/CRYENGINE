// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if CRY_PLATFORM_WINDOWS

	#include "LUADBG.h"
	#include "_TinyRegistry.h"
	#include "_TinyFileEnum.h"
	#include "_TinyBrowseFolder.h"
	#include "AboutWnd.h"
	#include "GoToWnd.h"
	#include "GoToFuncWnd.h"
	#include <CrySystem/File/ICryPak.h>
	#include <CryString/CryPath.h>
	#include <CryInput/IHardwareMouse.h>
	#include <algorithm>
	#include "Coverage.h"
	#include <CryInput/IInput.h>

	#include <direct.h>
	#if CRY_PLATFORM_32BIT
		#include "Shlwapi.h"
		#pragma comment(lib, "Shlwapi.lib")
	#endif

	#pragma warning(disable: 4244)

_TINY_DECLARE_APP();

	#ifndef _LIB
extern HANDLE gDLLHandle;
	#else //_LIB
static HANDLE gDLLHandle = 0;
	#endif //_LIB

//////////////////////////////////////////////////////////////////////////
// CSourceEdit
//////////////////////////////////////////////////////////////////////////
BOOL CSourceEdit::Create(DWORD dwStyle, DWORD dwExStyle, const RECT* pRect, _TinyWindow* pParentWnd, ULONG nID)
{
	_TinyRect erect;
	erect.left = BORDER_SIZE;
	erect.right = pRect->right;
	erect.top = 0;
	erect.bottom = pRect->bottom;
	if (!_TinyWindow::Create(NULL, _T("asd"), WS_CHILD | WS_VISIBLE, 0, pRect, pParentWnd, nID))
		return FALSE;
	if (!m_wEditWindow.Create(IDC_SOURCE, WS_VISIBLE | WS_CHILD | ES_WANTRETURN | WS_HSCROLL | WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL, WS_EX_CLIENTEDGE, &erect, this))
		return FALSE;
	m_wEditWindow.SetFont((HFONT) GetStockObject(ANSI_FIXED_FONT));

	/*
	   // Set 1 tab.
	   PARAFORMAT pf;
	   memset(&pf,0,sizeof(pf));
	   pf.cbSize = sizeof(pf);
	   pf.dwMask = PFM_TABSTOPS;
	   m_wEditWindow.SendMessage(EM_GETPARAFORMAT, (WPARAM)0,(LPARAM)&pf );
	   pf.cTabCount = 2;
	   m_wEditWindow.SendMessage(EM_SETPARAFORMAT, (WPARAM)0,(LPARAM)&pf );
	 */

	HDC hDC = GetDC(m_hWnd);
	m_hFont = CreateFont(-::MulDiv(9, GetDeviceCaps(hDC, LOGPIXELSY), 72), 0, 0, 0,
	                     FW_DONTCARE, FALSE, FALSE, FALSE,
	                     ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
	                     DEFAULT_PITCH, "Verdana"); //Tahoma
	ReleaseDC(m_hWnd, hDC);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
LRESULT CSourceEdit::OnEraseBkGnd(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) { return 1; };

//////////////////////////////////////////////////////////////////////////
LRESULT CSourceEdit::OnPaint(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	_TinyRect rect;
	PAINTSTRUCT ps;
	HDC hDC;
	BeginPaint(m_hWnd, &ps);
	hDC = ::GetDC(m_hWnd);
	int w, h;
	UINT i;
	int iLine;
	const char* pszFile = NULL;
	/////////////////////////////
	GetClientRect(&rect);
	w = rect.right - rect.left;
	h = rect.bottom - rect.top;
	_TinyRect rc(0, 0, BORDER_SIZE, h);
	::DrawFrameControl(hDC, &rc, DFC_BUTTON, DFCS_BUTTONPUSH | DFCS_PUSHED);
	::SetBkMode(hDC, TRANSPARENT);
	POINT pt;
	m_wEditWindow.GetScrollPos(&pt);
	int iFirstLine = m_wEditWindow.GetFirstVisibleLine() + 1;
	TEXTMETRIC tm;
	m_wEditWindow.GetTextMetrics(&tm);

	const int iFontY = tm.tmHeight;
	const int iFontHalf = iFontY / 2;

	int y = -pt.y % iFontY + 3;
	char sTemp[10];
	HRGN hOldRgn = ::CreateRectRgn(0, 0, 0, 0);
	HRGN hClippedRgn = ::CreateRectRgn(0, 0, BORDER_SIZE, h - 1);
	::GetWindowRgn(m_hWnd, hOldRgn);
	::SelectClipRgn(hDC, hClippedRgn);
	HGDIOBJ hPrevFont = ::SelectObject(hDC, m_hFont);
	COLORREF prevClr;
	while (y < h)
	{
		cry_sprintf(sTemp, "%d", iFirstLine);
		if (iFirstLine == m_iLineMarkerPos)
			prevClr = ::SetTextColor(hDC, RGB(255, 0, 0));

		int length = (int)strlen(sTemp);
		::TextOut(hDC, BORDER_SIZE - (length * tm.tmAveCharWidth) - 5, y, sTemp, length);
		if (iFirstLine == m_iLineMarkerPos)
			::SetTextColor(hDC, prevClr);
		y += iFontY;
		iFirstLine++;
	}
	iFirstLine = m_wEditWindow.GetFirstVisibleLine();

	HBRUSH brsh;

	if (!m_strSourceFile.empty() && m_strSourceFile[0] == '@')
	{
		if (CLUACodeCoverage* cov = m_pLuaDbg->GetCoverage())
		{
			HGDIOBJ pen = ::SelectObject(hDC, ::GetStockObject(NULL_PEN));
			brsh = ::CreateSolidBrush(0x00FFFF00);
			::SelectObject(hDC, brsh);
			int line = iFirstLine;
			int cy = -pt.y % iFontY + 3;
			while (cy < h)
			{
				if (cov->GetCoverage(&m_strSourceFile[1], line))
				{
					INT iY = (-pt.y % iFontY + 3) + (line - iFirstLine - 1) * iFontY + 7;
					Rectangle(hDC, 41 - 7, iY - 8, 41 + 8, iY + 8);
				}
				cy += iFontY;
				line++;
			}
			::DeleteObject(brsh);
			::SelectObject(hDC, pen);
		}
	}

	COLORREF crColor = 0x00BBBBBB;  // Grey colour for disabled
	// What debug mode are we in?
	if (ICVar* pDebug = gEnv->pConsole->GetCVar("lua_debugger"))
		if (pDebug->GetIVal() == eLDM_FullDebug)
			crColor = 0x000000FF;

	brsh = ::CreateSolidBrush(crColor);

	BreakPoints* pBreakPoints = m_pLuaDbg->GetBreakPoints();
	for (i = 0; i < (UINT)pBreakPoints->size(); i++)
	{
		BreakPoint& bp = (*pBreakPoints)[i];
		if (bp.nLine && strlen(bp.sSourceFile) > 0)
		{
			iLine = bp.nLine;
			if (stricmp(bp.sSourceFile, m_strSourceFile) == 0)
			{
				INT iY = (-pt.y % iFontY + 3) + (iLine - iFirstLine - 1) * iFontY + 7;
				::SelectObject(hDC, brsh);

				if (bp.bTracepoint)
				{
					POINT sTriangle[4];
					sTriangle[0].x = 0;
					sTriangle[0].y = iFontHalf;
					sTriangle[1].x = iFontHalf;
					sTriangle[1].y = 0;
					sTriangle[2].x = iFontY;
					sTriangle[2].y = iFontHalf;
					sTriangle[3].x = iFontHalf;
					sTriangle[3].y = iFontY;

					for (int j = 0; j < 4; ++j)
					{
						sTriangle[j].x += 3;
						sTriangle[j].y += iY - iFontHalf;
					}

					Polygon(hDC, sTriangle, 4);
				}
				else
				{
					Ellipse(hDC, 3, iY - iFontHalf, iFontY + 3, iY + iFontHalf);
				}
			}
		}
	}
	::DeleteObject(brsh);

	// Draw current line marker
	brsh = ::CreateSolidBrush(0x0000FFFF);
	::SelectObject(hDC, brsh);
	POINT sTriangle[7];
	sTriangle[0].x = 2;
	sTriangle[0].y = iFontHalf - 3;
	sTriangle[1].x = iFontHalf;
	sTriangle[1].y = iFontHalf - 3;
	sTriangle[2].x = iFontHalf;
	sTriangle[2].y = 2;
	sTriangle[3].x = iFontY - 2;
	sTriangle[3].y = iFontHalf;
	sTriangle[4].x = iFontHalf;
	sTriangle[4].y = iFontY - 2;
	sTriangle[5].x = iFontHalf;
	sTriangle[5].y = iFontHalf + 3;
	sTriangle[6].x = 2;
	sTriangle[6].y = iFontHalf + 3;

	for (i = 0; i < 7; ++i)
	{
		sTriangle[i].x += 3;
		sTriangle[i].y += ((-pt.y % iFontY + 3) + (m_iLineMarkerPos - iFirstLine - 1) * iFontY + 7) - iFontHalf;
	}

	Polygon(hDC, sTriangle, 7);

	// Breakpoints
	::DeleteObject(brsh);

	::SelectObject(hDC, hPrevFont);
	::SelectClipRgn(hDC, hOldRgn);
	::DeleteObject(hOldRgn);
	::DeleteObject(hClippedRgn);
	//::DeleteObject(hBrush);
	/////////////////////////////
	EndPaint(m_hWnd, &ps);
	return 0;
}

LRESULT CSourceEdit::OnRButtonDown(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (m_strSourceFile.empty())
		return 0;

	const int xPos = _TINY_GET_X_LPARAM(lParam);
	const int yPos = _TINY_GET_Y_LPARAM(lParam);

	if (xPos > 0 && xPos < BORDER_SIZE)
	{
		_TinyRect rect;
		GetClientRect(&rect);
		const int h = rect.bottom - rect.top;

		if (yPos > 0 && yPos < h)
		{
			TEXTMETRIC tm;
			m_wEditWindow.GetTextMetrics(&tm);

			const int fontY = tm.tmHeight;
			const int firstLine = m_wEditWindow.GetFirstVisibleLine() + 1;
			const int clickedLine = (yPos / fontY) + firstLine;

			if (m_pLuaDbg->HaveBreakPointAt(m_strSourceFile.c_str(), clickedLine))
			{
				m_pLuaDbg->ToggleTracepoint(m_strSourceFile.c_str(), clickedLine);
			}

			InvalidateEditWindow();
		}
	}

	return 0;
}

LRESULT CSourceEdit::OnLButtonDown(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (m_strSourceFile.empty())
		return 0;

	const int xPos = _TINY_GET_X_LPARAM(lParam);
	const int yPos = _TINY_GET_Y_LPARAM(lParam);

	if (xPos > 0 && xPos < BORDER_SIZE)
	{
		_TinyRect rect;
		GetClientRect(&rect);
		const int h = rect.bottom - rect.top;

		if (yPos > 0 && yPos < h)
		{
			TEXTMETRIC tm;
			m_wEditWindow.GetTextMetrics(&tm);

			const int fontY = tm.tmHeight;
			const int firstLine = m_wEditWindow.GetFirstVisibleLine() + 1;
			const int clickedLine = (yPos / fontY) + firstLine;

			if (m_pLuaDbg->HaveBreakPointAt(m_strSourceFile.c_str(), clickedLine))
			{
				m_pLuaDbg->RemoveBreakPoint(m_strSourceFile.c_str(), clickedLine);
			}
			else
			{
				m_pLuaDbg->AddBreakPoint(m_strSourceFile.c_str(), clickedLine);
			}

			InvalidateEditWindow();
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
LRESULT CSourceEdit::OnSize(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int w = LOWORD(lParam);
	int h = HIWORD(lParam);
	_TinyRect erect;
	erect.left = BORDER_SIZE;
	erect.right = w - BORDER_SIZE;
	erect.top = 0;
	erect.bottom = h;
	m_wEditWindow.SetWindowPos(BORDER_SIZE, 0, w - BORDER_SIZE, h, 0);
	return 0;
}

//////////////////////////////////////////////////////////////////////////
LRESULT CSourceEdit::OnScroll(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	_TinyRect rect;
	GetClientRect(&rect);

	rect.top = 0;
	rect.left = 0;
	rect.right = BORDER_SIZE;
	return InvalidateRect(&rect);
}

//////////////////////////////////////////////////////////////////////////
CLUADbg::CLUADbg(CScriptSystem* pScriptSystem)
{
	m_pScriptSystem = NULL;
	m_pIPak = NULL;
	m_pIVariable = NULL;
	m_hRoot = NULL;
	m_pTreeToAdd = NULL;
	m_pIPak = gEnv->pCryPak;
	m_pScriptSystem = pScriptSystem;

	m_wScriptWindow.SetLuaDbg(this);
	m_wScriptWindow.SetScriptSystem(m_pScriptSystem);

	m_bQuitMsgLoop = false;
	m_bForceBreak = false;
	m_breakState = bsNoBreak;
	m_nCallLevelUntilBreak = 0;
	m_bCppCallstackEnabled = false;

	LoadBreakpoints("breakpoints.lst");

	_Tiny_InitApp((HINSTANCE)CryGetCurrentModule(), (HINSTANCE) gDLLHandle, NULL, NULL, IDI_SMALL);

	m_coverage.reset(new CLUACodeCoverage());
	m_trackCoverage = false;
}

CLUADbg::~CLUADbg()
{
	_TinyRegistry cTReg;
	_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "MainHorzSplitter", m_wndMainHorzSplitter.GetSplitterPos()));
	_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "SrcEditSplitter", m_wndSrcEditSplitter.GetSplitterPos()));
	_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "WatchSplitter", m_wndWatchSplitter.GetSplitterPos()));
	_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "WatchCallstackSplitter", m_wndWatchCallstackSplitter.GetSplitterPos()));
	_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "Fullscreen", ::IsZoomed(m_hWnd) ? 1 : 0));
}

TBBUTTON tbButtonsAdd[] =
{
	{ 0, ID_DEBUG_RUN,              TBSTATE_ENABLED, BTNS_BUTTON, { 0 }, 0L, 0 },
	{ 1, ID_DEBUG_BREAK,            TBSTATE_ENABLED, BTNS_BUTTON, { 0 }, 0L, 0 },
	{ 2, ID_DEBUG_STOP,             TBSTATE_ENABLED, BTNS_BUTTON, { 0 }, 0L, 0 },

	{ 0, 0,                         TBSTATE_ENABLED, BTNS_SEP,    { 0 }, 0L, 0 },

	{ 3, ID_DEBUG_STEPINTO,         TBSTATE_ENABLED, BTNS_BUTTON, { 0 }, 0L, 0 },
	{ 4, ID_DEBUG_STEPOVER,         TBSTATE_ENABLED, BTNS_BUTTON, { 0 }, 0L, 0 },
	{ 5, ID_DEBUG_TOGGLEBREAKPOINT, TBSTATE_ENABLED, BTNS_BUTTON, { 0 }, 0L, 0 },
	{ 6, ID_DEBUG_DISABLE,          TBSTATE_ENABLED, BTNS_BUTTON, { 0 }, 0L, 0 },

	{ 0, 0,                         TBSTATE_ENABLED, BTNS_SEP,    { 0 }, 0L, 0 },

	{ 7, IDM_ABOUT,                 TBSTATE_ENABLED, BTNS_BUTTON, { 0 }, 0L, 0 },
};

//! add a backslash if needed
inline void MyPathAddBackslash(char* szPath)
{
	if (szPath[0] == 0)
		return;

	size_t nLen = strlen(szPath);

	if (szPath[nLen - 1] == '\\')
		return;

	if (szPath[nLen - 1] == '/')
	{
		szPath[nLen - 1] = '\\';
		return;
	}

	szPath[nLen] = '\\';
	szPath[nLen + 1] = '\0';
}

LRESULT CLUADbg::RegSanityCheck(bool bForceDelete)
{
	_TinyRegistry cTReg;

	// The registry often gets corrupted, hence check and possibly delete it
	if (!cTReg.CheckNumber("Software\\Tiny\\LuaDebugger\\", "MainHorzSplitter", 10, 1000)
	    || !cTReg.CheckNumber("Software\\Tiny\\LuaDebugger\\", "SrcEditSplitter", 10, 1000)
	    || !cTReg.CheckNumber("Software\\Tiny\\LuaDebugger\\", "WatchSplitter", 10, 1000)
	    || !cTReg.CheckNumber("Software\\Tiny\\LuaDebugger\\", "WatchCallstackSplitter", 10, 1000)

	    || !cTReg.CheckNumber("Software\\Tiny\\LuaDebugger\\", "XOrigin", 0, 4096)
	    || !cTReg.CheckNumber("Software\\Tiny\\LuaDebugger\\", "YOrigin", 0, 4096)
	    || !cTReg.CheckNumber("Software\\Tiny\\LuaDebugger\\", "XSize", 10, 4096)
	    || !cTReg.CheckNumber("Software\\Tiny\\LuaDebugger\\", "YSize", 10, 4096)
	    || bForceDelete)
	{
		// One of the checks failed, so lets delete all the subkeys
		cTReg.DeleteValue("Software\\Tiny\\LuaDebugger\\", "MainHorzSplitter");
		cTReg.DeleteValue("Software\\Tiny\\LuaDebugger\\", "SrcEditSplitter");
		cTReg.DeleteValue("Software\\Tiny\\LuaDebugger\\", "WatchSplitter");
		cTReg.DeleteValue("Software\\Tiny\\LuaDebugger\\", "WatchCallstackSplitter");
		cTReg.DeleteValue("Software\\Tiny\\LuaDebugger\\", "XOrigin");
		cTReg.DeleteValue("Software\\Tiny\\LuaDebugger\\", "YOrigin");
		cTReg.DeleteValue("Software\\Tiny\\LuaDebugger\\", "XSize");
		cTReg.DeleteValue("Software\\Tiny\\LuaDebugger\\", "YSize");
		cTReg.DeleteValue("Software\\Tiny\\LuaDebugger\\", "Fullscreen");
	}

	return 0;
}

LRESULT CLUADbg::SetupSplitters(void)
{
	_TinyRegistry cTReg;
	_TinyRect wrect;
	GetClientRect(&wrect);

	// Read splitter window locations from registry
	DWORD dwVal;
	cTReg.ReadNumber("Software\\Tiny\\LuaDebugger\\", "MainHorzSplitter", dwVal, 150);
	m_wndMainHorzSplitter.SetSplitterPos(dwVal);
	m_wndMainHorzSplitter.Reshape();
	cTReg.ReadNumber("Software\\Tiny\\LuaDebugger\\", "SrcEditSplitter", dwVal, 190);
	m_wndSrcEditSplitter.SetSplitterPos(dwVal);
	m_wndSrcEditSplitter.Reshape();
	cTReg.ReadNumber("Software\\Tiny\\LuaDebugger\\", "WatchSplitter", dwVal, wrect.right / 3 * 2);
	m_wndWatchSplitter.SetSplitterPos(dwVal);
	m_wndWatchSplitter.Reshape();
	cTReg.ReadNumber("Software\\Tiny\\LuaDebugger\\", "WatchCallstackSplitter", dwVal, wrect.right / 3);
	m_wndWatchCallstackSplitter.SetSplitterPos(dwVal);
	m_wndWatchCallstackSplitter.Reshape();

	return 1;
}

LRESULT CLUADbg::OnCreate(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	_TinyRect erect;
	_TinyRect lrect;
	_TinyRect wrect;
	_TinyRegistry cTReg;

	RegSanityCheck();

	DWORD dwXOrigin = 0, dwYOrigin = 0, dwXSize = 800, dwYSize = 600;
	bool usingDefaultSettings = true;
	if (cTReg.ReadNumber("Software\\Tiny\\LuaDebugger\\", "XOrigin", dwXOrigin))
	{
		usingDefaultSettings = false;
		cTReg.ReadNumber("Software\\Tiny\\LuaDebugger\\", "YOrigin", dwYOrigin);
		cTReg.ReadNumber("Software\\Tiny\\LuaDebugger\\", "XSize", dwXSize);
		cTReg.ReadNumber("Software\\Tiny\\LuaDebugger\\", "YSize", dwYSize);
	}

	SetWindowPos(dwXOrigin, dwYOrigin, dwXSize, dwYSize, SWP_NOZORDER | SWP_NOMOVE);
	// TODO: Make sure fullscreen flag is loaded and used
	// WS_MAXIMIZE
	// DWORD dwFullscreen = 0;
	// cTReg.ReadNumber("Software\\Tiny\\LuaDebugger\\", "Fullscreen", dwFullscreen);
	// if (dwFullscreen == 1)
	// ShowWindow(SW_MAXIMIZE);
	if (usingDefaultSettings)
		CenterOnScreen();
	GetClientRect(&wrect);

	erect = wrect;
	erect.top = 0;
	erect.bottom = 32;
	m_wToolbar.Create((ULONG_PTR) GetMenu(m_hWnd), WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS, 0, &erect, this);  //AMD Port
	m_wToolbar.AddButtons(IDC_LUADBG, tbButtonsAdd, 10);

	m_wndStatus.Create(0, NULL, this);

	// Client area window
	_TinyVerify(m_wndClient.Create(NULL, _T(""), WS_CHILD | WS_VISIBLE, 0, &wrect, this));
	m_wndClient.NotifyReflection(TRUE);

	// Splitter dividing source/file and watch windows
	m_wndMainHorzSplitter.Create(&m_wndClient, NULL, NULL, true);

	// Divides source and file view
	m_wndSrcEditSplitter.Create(&m_wndMainHorzSplitter);

	// Divides two watch windows
	m_wndWatchSplitter.Create(&m_wndMainHorzSplitter);

	// Divides the watch window and the cllstack
	m_wndWatchCallstackSplitter.Create(&m_wndWatchSplitter);

	// Add all scripts to the file tree
	m_wFilesTree.Create(erect, &m_wndSrcEditSplitter, IDC_FILES);

	string sScriptsFolder;
	const char* szFolder = m_pIPak->GetAlias("SCRIPTS");
	if (szFolder)
		sScriptsFolder = szFolder + string("/");
	else
		sScriptsFolder = "SCRIPTS/";
	m_wFilesTree.ScanFiles((char*)sScriptsFolder.c_str());

	string sUIFolder = "LIBS/UI/UIACTIONS/";
	m_wFilesTree.ScanFiles((char*)sUIFolder.c_str());
	string sItemsFolder = "ITEMS/";
	m_wFilesTree.ScanFiles((char*)sItemsFolder.c_str());

	_TinyVerify(m_wScriptWindow.Create(WS_VISIBLE | WS_CHILD | ES_WANTRETURN | WS_HSCROLL | WS_VSCROLL |
	                                   ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE, 0, &erect, &m_wndSrcEditSplitter));
	m_wScriptWindow.SendMessage(EM_SETEVENTMASK, 0, ENM_SCROLL);

	m_wndFileViewCaption.Create("Scripts", &m_wFilesTree, &m_wndSrcEditSplitter);
	m_wndSourceCaption.Create("Source", &m_wScriptWindow, &m_wndSrcEditSplitter);
	m_wndSrcEditSplitter.SetFirstPan(&m_wndFileViewCaption);
	m_wndSrcEditSplitter.SetSecondPan(&m_wndSourceCaption);

	m_wLocals.Create(IDC_LOCALS, WS_CHILD | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | WS_VISIBLE, WS_EX_CLIENTEDGE, &erect, &m_wndWatchSplitter);
	m_wWatch.Create(IDC_WATCH, WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT, WS_EX_CLIENTEDGE, &erect, &m_wndWatchSplitter);
	m_wCallstack.Create(0, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHOWSELALWAYS, LVS_EX_TRACKSELECT | LVS_EX_FULLROWSELECT | WS_EX_CLIENTEDGE, &erect, &m_wndWatchSplitter);

	m_wndLocalsCaption.Create("Locals", &m_wLocals, &m_wndWatchSplitter);
	m_wndWatchCaption.Create("self", &m_wWatch, &m_wndWatchCallstackSplitter);
	m_wndCallstackCaption.Create("Call Stack", &m_wCallstack, &m_wndWatchCallstackSplitter);

	m_wndWatchSplitter.SetFirstPan(&m_wndWatchCallstackSplitter);

	m_wndWatchCallstackSplitter.SetFirstPan(&m_wndWatchCaption);
	m_wndWatchCallstackSplitter.SetSecondPan(&m_wndCallstackCaption);

	m_wndWatchSplitter.SetSecondPan(&m_wndLocalsCaption);

	m_wndMainHorzSplitter.SetFirstPan(&m_wndSrcEditSplitter);
	m_wndMainHorzSplitter.SetSecondPan(&m_wndWatchSplitter);

	Reshape(wrect.right - wrect.left, wrect.bottom - wrect.top);

	SetupSplitters();

	m_wCallstack.InsertColumn(0, "Function", 300, 0);
	m_wCallstack.InsertColumn(1, "Line", 40, 1);
	m_wCallstack.InsertColumn(2, "File", 100, 2);

	//_TinyVerify(LoadFile("C:\\MASTERCD\\SCRIPTS\\Default\\Entities\\PLAYER\\BasicPlayer.lua"));
	//_TinyVerify(LoadFile("C:\\MASTERCD\\SCRIPTS\\Default\\Entities\\PLAYER\\player.lua"));
	// _TINY_CHECK_LAST_ERROR

	return 0;
}

void CLUADbg::StrSuperCat(char** dest, const char* source, UINT& iFormattedLength, UINT& iDestBufPos, size_t n)
{
	int length = (n == -1 ? strlen(source) : n);
	while (iDestBufPos + length >= iFormattedLength)
	{
		gEnv->pLog->Log("[LuaDebugger] Required buffer reallocation on load");

		// Reallocate buffer
		iFormattedLength *= 2;
		char* temp = new char[iFormattedLength];
		strcpy(temp, *dest);
		delete[] (*dest);
		*dest = temp;
	}
	strncat(*dest + iDestBufPos, source, length);  // iDestBufPos saves scanning over the whole string
	iDestBufPos += length;
}

bool CLUADbg::LoadFile(const char* pszFile, bool bForceReload)
{
	// Note that this code ignores the concept of DOS newlines, which seems unwise
	// To allow dynamic reallocation should estimate fail, refactored to use StrSuperCat throughout
	// Now it could easily be refactored again to just use CryString, which concatenates efficiently

	FILE* hFile = NULL;
	char* pszScript = NULL, * pszFormattedScript = NULL;
	UINT iLength, iFormattedLength, iCmpBufPos, iSrcBufPos, iDestBufPos, iNumChars, iStrStartPos, iCurKWrd, i;
	char szCmpBuf[2048];
	bool bIsKeyWord;
	string strLuaFormatFileName = "@";

	if (!pszFile)
		return false;

	// Create lua specific filename and check if we already have that file loaded
	strLuaFormatFileName += pszFile;
	strLuaFormatFileName.replace('\\', '/');
	strLuaFormatFileName.MakeLower();

	if (bForceReload == false && m_wScriptWindow.GetSourceFile() == strLuaFormatFileName)
		return true;

	m_wScriptWindow.SetSourceFile(strLuaFormatFileName.c_str());

	// hFile = fopen(pszFile, "rb");
	hFile = m_pIPak->FOpen(pszFile, "rb");

	if (!hFile)
		return false;

	// set filename in window title
	{
		char str[_MAX_PATH];

		cry_sprintf(str, "Lua Debugger [%s]", pszFile);
		SetWindowText(str);
	}

	// Get file size
	// fseek(hFile, 0, SEEK_END);
	m_pIPak->FSeek(hFile, 0, SEEK_END);
	// iLength = ftell(hFile);
	iLength = m_pIPak->FTell(hFile);
	// fseek(hFile, 0, SEEK_SET);
	m_pIPak->FSeek(hFile, 0, SEEK_SET);

	pszScript = new char[iLength + 1];
	iFormattedLength = 512 + iLength * 6;  // We still use original estimate, but just 512 works fine now
	pszFormattedScript = new char[iFormattedLength];

	// _TinyVerify(fread(pszScript, iLength, 1, hFile) == 1);
	_TinyVerify(m_pIPak->FRead(pszScript, iLength, hFile) == iLength);
	pszScript[iLength] = '\0';
	_TinyAssert(strlen(pszScript) == iLength);

	// RTF text, font Courier, green comments and blue keyword
	strcpy(pszFormattedScript, "{\\rtf1\\ansi\\ansicpg1252\\deff0\\deflang1033"                   \
	  "{\\fonttbl{\\f0\\fmodern\\fprq1\\fcharset0 Courier New;}{\\f1\\fswiss\\fcharset0 Arial;}}" \
	  "{\\colortbl ;\\red0\\green0\\blue255;\\red0\\green128\\blue0;\\red160\\green160\\blue160;}\\f0\\fs20");

	const char szKeywords[][32] =
	{
		"function",
		"do",
		"for",
		"end",
		"and",
		"or",
		"not",
		"while",
		"return",
		"if",
		"then",
		"else",
		"elseif",
		"self",
		"local",
		"in",
		"nil",
		"repeat",
		"until",
		"break",
	};

	// Format text with syntax coloring
	iDestBufPos = strlen(pszFormattedScript);

	iSrcBufPos = 0;
	while (pszScript[iSrcBufPos] != '\0')
	{
		// Scan next token
		iNumChars = 1;
		iStrStartPos = iSrcBufPos;
		while (pszScript[iSrcBufPos] != ' ' &&
		       pszScript[iSrcBufPos] != '\n' &&
		       pszScript[iSrcBufPos] != '\r' &&
		       pszScript[iSrcBufPos] != '\t' &&
		       pszScript[iSrcBufPos] != '\0' &&
		       pszScript[iSrcBufPos] != '(' &&
		       pszScript[iSrcBufPos] != ')' &&
		       pszScript[iSrcBufPos] != '[' &&
		       pszScript[iSrcBufPos] != ']' &&
		       pszScript[iSrcBufPos] != '{' &&
		       pszScript[iSrcBufPos] != '}' &&
		       pszScript[iSrcBufPos] != ',' &&
		       pszScript[iSrcBufPos] != '.' &&
		       pszScript[iSrcBufPos] != ';' &&
		       pszScript[iSrcBufPos] != ':' &&
		       pszScript[iSrcBufPos] != '=' &&
		       pszScript[iSrcBufPos] != '==' &&
		       pszScript[iSrcBufPos] != '*' &&
		       pszScript[iSrcBufPos] != '+' &&
		       pszScript[iSrcBufPos] != '/' &&
		       pszScript[iSrcBufPos] != '~' &&
		       pszScript[iSrcBufPos] != '"')
		{
			iSrcBufPos++;
			iNumChars++;

			// Special treatment of '-' to allow parsing of '--'
			if (pszScript[iSrcBufPos - 1] == '-' && pszScript[iSrcBufPos] != '-')
				break;
		}
		if (iNumChars == 1)
			iSrcBufPos++;
		else
			iNumChars--;

		// Copy token and add escapes
		iCmpBufPos = 0;
		for (i = iStrStartPos; i < iStrStartPos + iNumChars; i++)
		{
			_TinyAssert(i - iStrStartPos < sizeof(szCmpBuf));

			if (pszScript[i] == '{' || pszScript[i] == '}' || pszScript[i] == '\\')
			{
				// Add \ to mark it as non-escape character
				szCmpBuf[iCmpBufPos++] = '\\';
				szCmpBuf[iCmpBufPos++] = pszScript[i];
				szCmpBuf[iCmpBufPos] = '\0';
			}
			else
			{
				szCmpBuf[iCmpBufPos++] = pszScript[i];
				szCmpBuf[iCmpBufPos] = '\0';
			}
		}

		// Comment
		if (strncmp(szCmpBuf, "--", 2) == 0)
		{
			// Green
			StrSuperCat(&pszFormattedScript, "\\cf2 ", iFormattedLength, iDestBufPos);

			StrSuperCat(&pszFormattedScript, szCmpBuf, iFormattedLength, iDestBufPos);

			// Parse until newline, which next iteration will handle
			int iAdvancePos = iSrcBufPos;
			while (pszScript[iAdvancePos] != '\n' && pszScript[iAdvancePos] != '\0')
			{
				switch (pszScript[iAdvancePos])
				{
				case '{':
				case '}':
				case '\\':
					// Special char found - concatenate string so far
					StrSuperCat(&pszFormattedScript, &(pszScript[iSrcBufPos]), iFormattedLength, iDestBufPos, iAdvancePos - iSrcBufPos);
					StrSuperCat(&pszFormattedScript, "\\", iFormattedLength, iDestBufPos);
					StrSuperCat(&pszFormattedScript, &(pszScript[iAdvancePos]), iFormattedLength, iDestBufPos, 1);
					// Continue after here
					iSrcBufPos = iAdvancePos + 1;
				}
				iAdvancePos++;
			}

			// Concatenate from beginning of comment or last special char to end of line
			StrSuperCat(&pszFormattedScript, &(pszScript[iSrcBufPos]), iFormattedLength, iDestBufPos, iAdvancePos - iSrcBufPos);
			iSrcBufPos = iAdvancePos;

			// Restore color
			StrSuperCat(&pszFormattedScript, "\\cf0 ", iFormattedLength, iDestBufPos);
			continue;
		}

		// String
		if (strncmp(szCmpBuf, "\"", 2) == 0)
		{
			// Gray
			StrSuperCat(&pszFormattedScript, "\\cf3 ", iFormattedLength, iDestBufPos);

			StrSuperCat(&pszFormattedScript, szCmpBuf, iFormattedLength, iDestBufPos);

			// Parse until end of string (or newline, which next iteration will handle)
			int iAdvancePos = iSrcBufPos;
			while (pszScript[iAdvancePos] != '\n' && pszScript[iAdvancePos] != '\0' && pszScript[iAdvancePos] != '"')
			{
				switch (pszScript[iAdvancePos])
				{
				case '{':
				case '}':
				case '\\':
					// Special char found - concatenate string so far
					StrSuperCat(&pszFormattedScript, &(pszScript[iSrcBufPos]), iFormattedLength, iDestBufPos, iAdvancePos - iSrcBufPos);
					// Add escaping slash and the actual character found
					StrSuperCat(&pszFormattedScript, "\\", iFormattedLength, iDestBufPos);
					StrSuperCat(&pszFormattedScript, &(pszScript[iAdvancePos]), iFormattedLength, iDestBufPos, 1);

					// If we escaped a slash, peek ahead because of the escaped newline case!
					if (pszScript[iAdvancePos] == '\\' && pszScript[iAdvancePos + 1] == '"')
					{
						// Copy in that quote, and advance past it
						StrSuperCat(&pszFormattedScript, "\"", iFormattedLength, iDestBufPos);
						iAdvancePos++;
					}

					// Continue after here
					iSrcBufPos = iAdvancePos + 1;
				}
				iAdvancePos++;
			}

			// If we finished on a quote, advance to include it
			if (pszScript[iAdvancePos] == '"') iAdvancePos++;

			// Concatenate from beginning of comment or last special char to end of line
			StrSuperCat(&pszFormattedScript, &(pszScript[iSrcBufPos]), iFormattedLength, iDestBufPos, iAdvancePos - iSrcBufPos);
			iSrcBufPos = iAdvancePos;

			// Restore color
			StrSuperCat(&pszFormattedScript, "\\cf0 ", iFormattedLength, iDestBufPos);

			continue;
		}

		// Have we parsed a keyword ?
		bIsKeyWord = false;
		for (iCurKWrd = 0; iCurKWrd < CRY_ARRAY_COUNT(szKeywords); iCurKWrd++)
		{
			if (strcmp(szKeywords[iCurKWrd], szCmpBuf) == 0)
			{
				StrSuperCat(&pszFormattedScript, "\\cf1 ", iFormattedLength, iDestBufPos);
				StrSuperCat(&pszFormattedScript, szKeywords[iCurKWrd], iFormattedLength, iDestBufPos);
				StrSuperCat(&pszFormattedScript, "\\cf0 ", iFormattedLength, iDestBufPos);
				bIsKeyWord = true;
			}
		}
		if (bIsKeyWord)
			continue;

		if (strcmp(szCmpBuf, "\n") == 0)
		{
			// Newline
			StrSuperCat(&pszFormattedScript, "\\par ", iFormattedLength, iDestBufPos);
		}
		else if (strcmp(szCmpBuf, "\t") == 0)
		{
			strcat(pszFormattedScript, "  ");
			iDestBufPos += 2;
		}
		else
		{
			// Anything else, just append
			StrSuperCat(&pszFormattedScript, szCmpBuf, iFormattedLength, iDestBufPos);
		}
	}

	// Ensure we have a terminating null
	pszFormattedScript[iDestBufPos] = '\0';

	// Did we overrun the buffer? (better late than never!)
	if (iDestBufPos > iFormattedLength)
		gEnv->pLog->LogError("[LuaDebugger] Overran output buffer, %d of %d", iDestBufPos, iFormattedLength);

	if (hFile)
		m_pIPak->FClose(hFile);

	if (pszScript)
	{
		delete[] pszScript;
		pszScript = NULL;
	}

	//FILE *dbgFile = fopen("C:\\Debug.txt", "w");
	//fwrite(pszFormattedScript, 1, strlen(pszFormattedScript), dbgFile);
	//fprintf(dbgFile,"-- iLength %d, formattedBuff %d, iDestBufPos at end %d",iLength, iFormattedLength, iDestBufPos);
	//fclose(dbgFile);

	m_wScriptWindow.SetText(pszFormattedScript);

	delete[] pszFormattedScript;

	return true;
}

LRESULT CLUADbg::OnResetView(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	RegSanityCheck(true); // Force registry deletion
	SetupSplitters();     // Setup splitters from default values
	return 1;
}

LRESULT CLUADbg::OnEraseBkGnd(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	return 1;
}

LRESULT CLUADbg::OnClose(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// Quit();
	WINDOWINFO wi;
	if (::GetWindowInfo(m_hWnd, &wi))
	{
		_TinyRegistry cTReg;
		_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "XOrigin", wi.rcWindow.left));
		_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "YOrigin", wi.rcWindow.top));
		_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "XSize", wi.rcWindow.right - wi.rcWindow.left));
		_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "YSize", wi.rcWindow.bottom - wi.rcWindow.top));

		_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "MainHorzSplitter", m_wndMainHorzSplitter.GetSplitterPos()));
		_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "SrcEditSplitter", m_wndSrcEditSplitter.GetSplitterPos()));
		_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "WatchSplitter", m_wndWatchSplitter.GetSplitterPos()));
		_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "WatchCallstackSplitter", m_wndWatchCallstackSplitter.GetSplitterPos()));

	}
	m_bQuitMsgLoop = true;
	return 0;
}

LRESULT CLUADbg::OnSize(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	int w = LOWORD(lParam);
	int h = HIWORD(lParam);
	Reshape(w, h);

	return 0;
}

LRESULT CLUADbg::OnAbout(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	CAboutWnd wndAbout;
	wndAbout.DoModal(this);
	return 0;
}

LRESULT CLUADbg::OnGoTo(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	CGoToWnd wndGoTo;
	wndGoTo.DoModal(this);
	SetFocusToEditControl();
	return 0;
}

LRESULT CLUADbg::OnToggleBreakpoint(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	const char* source = m_wScriptWindow.GetSourceFile();
	int line = m_wScriptWindow.GetCurLine();
	if (HaveBreakPointAt(source, line))
		RemoveBreakPoint(source, line);
	else
	{
		AddBreakPoint(source, line);
	}
	m_wScriptWindow.InvalidateEditWindow();
	return 0;
}

LRESULT CLUADbg::OnDeleteAllBreakpoints(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	m_breakPoints.clear();
	SaveBreakpoints("breakpoints.lst");
	m_wScriptWindow.InvalidateEditWindow();
	return 0;
}

	#define TOOLBAR_HEIGHT    28
	#define STATUS_BAR_HEIGHT 20

bool CLUADbg::Reshape(int w, int h)
{
	/*
	   int nWatchHeight=(((float)h)*WATCH_HEIGHT_MULTIPLIER);
	   int nFilesWidth=(((float)h)*FILES_WIDTH_MULTIPLIER);
	   m_wScriptWindow.SetWindowPos(nFilesWidth,TOOLBAR_HEIGHT,w-nFilesWidth,h-TOOLBAR_HEIGHT-nWatchHeight,SWP_DRAWFRAME);
	   m_wLocals.SetWindowPos(0,h-nWatchHeight,w/2,nWatchHeight,SWP_DRAWFRAME);
	   m_wFilesTree.SetWindowPos(nFilesWidth,TOOLBAR_HEIGHT,nFilesWidth,h-TOOLBAR_HEIGHT-nWatchHeight,SWP_DRAWFRAME);
	   m_wWatch.SetWindowPos(w/2,h-nWatchHeight,w/2,nWatchHeight,SWP_DRAWFRAME);
	 */

	m_wndClient.Reshape(w, h - TOOLBAR_HEIGHT - STATUS_BAR_HEIGHT);
	m_wndClient.SetWindowPos(0, TOOLBAR_HEIGHT, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
	m_wndMainHorzSplitter.Reshape(w, h - TOOLBAR_HEIGHT - STATUS_BAR_HEIGHT);
	m_wToolbar.SetWindowPos(0, 0, w, TOOLBAR_HEIGHT, 0);
	m_wndStatus.SetWindowPos(0, h - STATUS_BAR_HEIGHT, w, STATUS_BAR_HEIGHT, NULL);

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CLUADbg::PlaceLineMarker(UINT iLine)
{
	m_wScriptWindow.SetLineMarker(iLine);
	m_wScriptWindow.ScrollToLine(iLine - 1);
}

//////////////////////////////////////////////////////////////////////////
LRESULT CLUADbg::UpdateCheckboxes(HWND hWnd)
{
	ELuaDebugMode mode = eLDM_NoDebug;

	if (ICVar* pDebug = gEnv->pConsole->GetCVar("lua_debugger"))
		mode = (ELuaDebugMode) pDebug->GetIVal();

	UINT modeItem;
	switch (mode)
	{
	case eLDM_FullDebug:
		modeItem = ID_DEBUG_ENABLE;
		break;
	case eLDM_OnlyErrors:
		modeItem = ID_DEBUG_ERRORS;
		break;
	default:
	// An invalid debugging mode
	case eLDM_NoDebug:
		modeItem = ID_DEBUG_DISABLE;
		break;
	}

	int success = CheckMenuRadioItem(GetMenu(m_hWnd), ID_DEBUG_DISABLE, ID_DEBUG_ENABLE, modeItem, MF_BYCOMMAND);
	if (!success)
		_TINY_CHECK_LAST_ERROR;
	return 1;
}

//////////////////////////////////////////////////////////////////////////
void CLUADbg::SetStatusBarText(const char* pszText)
{
	// TODO: Find out why setting the panel text using the
	//       dedicated message doesn't work. For some reason the
	//       text gets drawn once but never again.
	// m_wndStatus.SetPanelText(pszText);

	m_wndStatus.SetWindowText(pszText);
}

//////////////////////////////////////////////////////////////////////////
string CLUADbg::GetLineFromFile(const char* sFilename, int nLine)
{
	CCryFile file;

	if (!file.Open(sFilename, "rb"))
	{
		return "";
	}
	int nLen = file.GetLength();
	char* sScript = new char[nLen + 1];
	char* sString = new char[nLen + 1];
	file.ReadRaw(sScript, nLen);
	sScript[nLen] = '\0';

	int nCurLine = 1;
	string strLine;

	strcpy(sString, "");

	const char* strLast = sScript + nLen;

	const char* str = sScript;
	while (str < strLast)
	{
		char* s = sString;
		while (str < strLast && *str != '\n' && *str != '\r')
		{
			*s++ = *str++;
		}
		*s = '\0';
		// Skip \n\r
		str += 2;

		if (nCurLine == nLine)
		{
			strLine = sString;
			strLine.replace('\t', ' ');
			strLine.Trim();
			break;
		}
		nCurLine++;
	}

	delete[]sString;
	delete[]sScript;

	return strLine;
}

//////////////////////////////////////////////////////////////////////////
void CLUADbg::GetStackAndLocals()
{
	m_wCallstack.SendMessage(WM_SETREDRAW, (WPARAM)FALSE, (LPARAM)FALSE);
	m_wWatch.SendMessage(WM_SETREDRAW, (WPARAM)FALSE, (LPARAM)FALSE);

	///////////
	// Stack //
	///////////
	{
		std::vector<SLuaStackEntry> callstack;
		m_pScriptSystem->GetCallStack(callstack);
		const char* pszText = NULL;
		int iItem = 0;
		m_wCallstack.Clear();

		if (!callstack.empty())
		{
			for (int i = callstack.size() - 1; i >= 0; i--)
			{
				SLuaStackEntry& le = callstack[i];

				string fileline;

				iItem = m_wCallstack.InsertItem(0, le.description.c_str(), 0);

				char s[64];
				itoa(le.line, s, 10);
				m_wCallstack.SetItemText(iItem, 1, s);

				if (!le.source.empty())
				{
					if (_stricmp(le.source, "=C") == 0)
						m_wCallstack.SetItemText(iItem, 2, "Native C Function");
					else
					{
						string strLine = GetLineFromFile(le.source.c_str() + 1, le.line);
						if (!strLine.empty())
						{
							string str = le.description + "  (" + strLine + ")";
							m_wCallstack.SetItemText(iItem, 0, str.c_str());
						}

						m_wCallstack.SetItemText(iItem, 2, le.source.c_str() + 1);
					}
				}
				else
					m_wCallstack.SetItemText(iItem, 2, "No Source");

			}
		}
		else
		{
			iItem = m_wCallstack.InsertItem(0, "No Lua callstack available", 0);
		}

		//////////////////////////////////////////////////////////////////////////
		// Get C++ call stack.
		//////////////////////////////////////////////////////////////////////////
		if (m_bCppCallstackEnabled)
		{
			const char* funcs[32];
			int nFuncCount = 32;
			GetISystem()->debug_GetCallStack(funcs, nFuncCount);
			if (nFuncCount > 0)
			{
				iItem = m_wCallstack.GetItemCount();
				iItem = m_wCallstack.InsertItem(iItem + 1, "", 0);
				iItem = m_wCallstack.InsertItem(iItem + 1, "-------------- C++ Call Stack --------------", 0);
				iItem = m_wCallstack.InsertItem(iItem + 1, "", 0);
				int nIndex = 1;
				for (int i = 8; i < nFuncCount; i++) // Skip some irrelevant functions
				{
					const char* s = funcs[i];
					// Skip all Lua functions (written in C)
					if (strstr(s, ".c:") == 0)
					{
						int thisItem = m_wCallstack.InsertItem(iItem + nIndex, funcs[i], 0);

						string str = funcs[i];
						string::size_type nBracketStartPos = str.find("[");
						if (nBracketStartPos != string::npos)
						{
							string::size_type nColonPos = str.find(":", nBracketStartPos);
							if (nColonPos != string::npos)
							{
								string::size_type nBracketEndPos = str.find("]", nColonPos);
								if (nBracketEndPos != string::npos)
								{
									m_wCallstack.SetItemText(thisItem, 0, str.substr(0, nBracketStartPos - 1));
									m_wCallstack.SetItemText(thisItem, 1, str.substr(nColonPos + 1, nBracketEndPos - nColonPos - 1));
									m_wCallstack.SetItemText(thisItem, 2, str.substr(nBracketStartPos + 1, nColonPos - nBracketStartPos - 1));
								}
							}
						}

						nIndex++;
					}
				}
			}
		}
		else
		{
			iItem = m_wCallstack.GetItemCount();
			iItem = m_wCallstack.InsertItem(iItem + 1, "", 0);
			iItem = m_wCallstack.InsertItem(iItem + 1, "C++ call stack can be activated in menu", 0);
		}
	}

	m_wCallstack.SendMessage(WM_SETREDRAW, (WPARAM)TRUE, (LPARAM)FALSE);

	// ShowLocalsForFrame will also call ShowSelf (for that frame)
	ShowLocalsForFrame(0);
	m_nodeLookupByName.clear();
}

void CLUADbg::ShowSelf(IScriptTable* pFrame)
{
	const char* pszText = NULL;

	m_wWatch.Clear();
	IScriptTable* pIScriptObj = pFrame;

	bool bVarFound = false;
	IScriptTable::Iterator iter = pIScriptObj->BeginIteration();
	while (pIScriptObj->MoveNext(iter))
	{
		if (iter.sKey)
		{
			pszText = iter.sKey;
			if (strcmp("self", pszText) == 0)
			{
				SmartScriptTable pIEntry(m_pScriptSystem, true);
				if (iter.value.GetType() == EScriptAnyType::Table)
					m_pIVariable = iter.value.GetScriptTable();
				else
					m_pIVariable = pIEntry;

				m_pTreeToAdd = &m_wWatch;

				if (m_pIVariable)
				{
					bVarFound = true;

					m_hRoot = NULL;
					m_iRecursionLevel = 0;

					// Dump only works for tables, in case of values call the sink directly
					if (iter.value.GetType() == EScriptAnyType::Table)
						m_pIVariable->Dump((IScriptTableDumpSink*) this);
					else
					{
						m_pIVariable = pIScriptObj; // Value needs to be retrieved from parent table
						OnElementFound(pszText, iter.value.GetVarType());
					}

					// No AddRef() !
					// m_pIVariable->Release();

				}

				m_pTreeToAdd = NULL;
			}
		}
	}
	pIScriptObj->EndIteration(iter);
	m_pIVariable = NULL;

	if (!bVarFound)
		m_wWatch.AddItemToTree("self = ?");

	m_wWatch.SendMessage(WM_SETREDRAW, (WPARAM)TRUE, (LPARAM)FALSE);

}

void CLUADbg::ShowLocalsForFrame(int frame)
{
	m_wLocals.SendMessage(WM_SETREDRAW, (WPARAM)FALSE, (LPARAM)FALSE);

	m_nodeLookupByName.clear();
	m_wLocals.Clear();
	IScriptTable* pFrame = m_pScriptSystem->GetLocalVariables(frame, false);
	m_pTreeToAdd = &m_wLocals;
	if (pFrame)
	{
		m_hRoot = NULL;
		m_iRecursionLevel = 0;
		m_pIVariable = pFrame;
		pFrame->Dump((IScriptTableDumpSink*) this);
		ShowSelf(pFrame);
		pFrame->Release();
	}
	else
		m_wLocals.AddItemToTree("No Locals Available");
	m_pTreeToAdd = NULL;
	m_pIVariable = NULL;
	m_wLocals.SendMessage(WM_SETREDRAW, (WPARAM)TRUE, (LPARAM)FALSE);
}

HTREEITEM CLUADbg::AddVariableToTree(const char* sName, ScriptVarType type, HTREEITEM hParent)
{
	char szBuf[2048];
	char szType[32];
	bool bRetrieved = false;
	int iIdx;

	if (m_pTreeToAdd == NULL)
	{
		_TinyAssert(m_pTreeToAdd != NULL);
		return 0;
	}

	switch (type)
	{
	case svtNull:
		cry_strcpy(szType, "[nil]");
		break;
	case svtString:
		cry_strcpy(szType, "[string]");
		break;
	case svtNumber:
		cry_strcpy(szType, "[numeric]");
		break;
	case svtFunction:
		cry_strcpy(szType, "[function]");
		break;
	case svtObject:
		cry_strcpy(szType, "[table]");
		break;
	case svtUserData:
	case svtPointer:
		cry_strcpy(szType, "[user data]");
		break;
	case svtBool:
		cry_strcpy(szType, "[bool]");
		break;
	default:
		cry_strcpy(szType, "[unknown]");
		break;
	}

	if (type == svtString || type == svtNumber || type == svtBool || type == svtPointer || type == svtUserData)
	{
		float fValue = 0;
		char temp[128];
		const char* szContent = temp;
		if (sName[0] == '[')
		{
			cry_strcpy(szBuf, &sName[1]);
			szBuf[strlen(szBuf) - 1] = '\0';
			iIdx = atoi(szBuf);
			if (type == svtString)
			{
				bRetrieved = m_pIVariable->GetAt(iIdx, szContent);
			}
			else if (type == svtNumber)
			{
				bRetrieved = m_pIVariable->GetAt(iIdx, fValue);
				cry_sprintf(temp, "%g", fValue);
			}
			else if (type == svtBool)
			{
				bool bValue = false;
				bRetrieved = m_pIVariable->GetAt(iIdx, bValue);
				if (bValue)
					cry_strcpy(temp, "true");
				else
					cry_strcpy(temp, "false");
			}
			else if (type == svtPointer)
			{
				ScriptHandle ptr(0);
				bRetrieved = m_pIVariable->GetAt(iIdx, ptr);
				cry_sprintf(temp, "0x%016p", ptr.ptr);
			}
		}
		else
		{
			float fVal = 0;
			if (type == svtString)
			{
				bRetrieved = m_pIVariable->GetValue(sName, szContent);
			}
			else if (type == svtNumber)
			{
				bRetrieved = m_pIVariable->GetValue(sName, fValue);
				cry_sprintf(temp, "%g", fValue);
			}
			else if (type == svtBool)
			{
				bool bValue = false;
				bRetrieved = m_pIVariable->GetValue(sName, bValue);
				if (bValue)
					cry_strcpy(temp, "true");
				else
					cry_strcpy(temp, "false");
			}
			else if (type == svtPointer)
			{
				ScriptHandle ptr(0);
				bRetrieved = m_pIVariable->GetValue(sName, ptr);
				cry_sprintf(temp, "0x%016p", ptr.ptr);
			}
		}

		if (bRetrieved)
		{
			if (type == svtString)
				cry_sprintf(szBuf, "%s %s = \"%s\"", szType, sName, szContent);
			else
				cry_sprintf(szBuf, "%s %s = %s", szType, sName, szContent);
		}
		else
			cry_sprintf(szBuf, "%s %s = (Unknown)", szType, sName);
	}
	else
		cry_sprintf(szBuf, "%s %s", szType, sName);

	HTREEITEM hNewItem = m_pTreeToAdd->AddItemToTree(szBuf, NULL, hParent);
	TreeView_SortChildren(m_pTreeToAdd->m_hWnd, hNewItem, FALSE);

	return hNewItem;
}

void CLUADbg::OnElementFound(const char* sName, ScriptVarType type)
{
	HTREEITEM hRoot = NULL;
	UINT iRecursionLevel = 0;
	SmartScriptTable pTable(m_pScriptSystem, true);
	IScriptTable* pIOldTbl = NULL;
	HTREEITEM hOldRoot = NULL;

	if (!sName) sName = "[table idx]";
	hRoot = m_nodeLookupByName[m_hRoot][sName];
	if (hRoot == NULL)
	{
		hRoot = AddVariableToTree(sName, type, m_hRoot);
		m_nodeLookupByName[m_hRoot][sName] = hRoot;
	}

	if (type == svtObject && m_iRecursionLevel < 10)
	{
		int iIndex;
		if (m_pIVariable->GetValue(sName, pTable) ||
		    (sName[0] == '[' && ((iIndex = atoi(sName + 1)) > 0) && m_pIVariable->GetAt(iIndex, pTable)))
		{
			pIOldTbl = m_pIVariable;
			hOldRoot = m_hRoot;
			m_pIVariable = pTable;
			m_hRoot = hRoot;
			m_iRecursionLevel++;
			pTable->Dump((IScriptTableDumpSink*) this);
			m_iRecursionLevel--;
			m_pIVariable = pIOldTbl;
			m_hRoot = hOldRoot;
		}
	}
}

void CLUADbg::OnElementFound(int nIdx, ScriptVarType type)
{
	char szBuf[32];
	cry_sprintf(szBuf, "[%i]", nIdx);
	OnElementFound(szBuf, type);
}

//////////////////////////////////////////////////////////////////////////
void CLUADbg::Break()
{
	m_bForceBreak = true;
	m_nCallLevelUntilBreak = 0;
	m_breakState = bsNoBreak;
}

//////////////////////////////////////////////////////////////////////////
bool CLUADbg::InvokeDebugger(const char* pszSourceFile, int iLine, const char* pszReason)
{
	HACCEL hAccelerators = NULL;
	MSG msg;
	IScriptSystem* pIScriptSystem = NULL;

	pIScriptSystem = gEnv->pScriptSystem;

	if (!::IsWindow(m_hWnd))
	{
		_TinyVerify(Create(NULL, _T("Lua Debugger"), WS_OVERLAPPEDWINDOW, 0, NULL,
		                   NULL, (ULONG_PTR) LoadMenu(_Tiny_GetResourceInstance(), MAKEINTRESOURCE(IDC_LUADBG))));
	}
	UpdateCheckboxes(m_hWnd);
	if (!::IsWindow(m_hWnd))
		return false;

	if ((hAccelerators = LoadAccelerators(_Tiny_GetResourceInstance(), MAKEINTRESOURCE(IDR_LUADGB_ACCEL))) == NULL)
	{
		// No accelerators
	}

	// Make sure the debugger is displayed maximized when it was left like that last time
	// TODO: Maybe serialize this with the other window settings
	if (::IsZoomed(m_hWnd))
		::ShowWindow(m_hWnd, SW_MAXIMIZE);
	else
		::ShowWindow(m_hWnd, SW_NORMAL);

	m_wScriptWindow.InvalidateEditWindow();

	::SetForegroundWindow(m_hWnd);
	if (gEnv && gEnv->pSystem && gEnv->pSystem->GetIHardwareMouse())
		gEnv->pSystem->GetIHardwareMouse()->IncrementCounter();
	if (gEnv->IsDedicated())
		gEnv->pInput->ShowCursor(true);

	if (pszSourceFile && pszSourceFile[0] == '@')
	{
		LoadFile(&pszSourceFile[1]);
		iLine = __max(0, iLine);

		if (!strcmp(pszReason, "StepOver") || !strcmp(pszReason, "StepInt"))
		{
			m_wScriptWindow.SetLineMarker(iLine);

			//check if we reached the end of visible lines to set the new scroll position
			int lastVisibleLine = m_wScriptWindow.GetLastVisibleLine();
			static int previousLine = 0;

			if (lastVisibleLine < iLine || iLine < previousLine || previousLine == 0)
				m_wScriptWindow.ScrollToLine(iLine);

			previousLine = iLine;
		}
		else
			PlaceLineMarker(iLine);
	}
	else
	{
		const char* file = m_wScriptWindow.GetSourceFile();
		if (*file == '@')
			LoadFile(file + 1, true);
	}

	if (pszReason)
		SetStatusBarText(pszReason);

	GetStackAndLocals();

	m_bQuitMsgLoop = false;
	while (GetMessage(&msg, NULL, 0, 0) && !m_bQuitMsgLoop)
	{
		if (hAccelerators == NULL || TranslateAccelerator(m_hWnd, hAccelerators, &msg) == 0)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	if (!IsWindow(m_hWnd))
		CryDebugBreak();

	// Don't hide the window when the debugger will be triggered next frame anyway
	if (m_breakState != bsStepNext && m_breakState != bsStepInto && !m_bForceBreak)
	{
		::ShowWindow(m_hWnd, SW_HIDE);
	}

	if (gEnv->pHardwareMouse)
		gEnv->pHardwareMouse->DecrementCounter();

	DestroyAcceleratorTable(hAccelerators);

	return (int) msg.wParam == 0;

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLUADbg::HaveBreakPointAt(const char* sSourceFile, int nLine)
{
	int nCount = m_breakPoints.size();
	for (int i = 0; i < nCount; i++)
	{
		if (strcmp(m_breakPoints[i].sSourceFile, sSourceFile) == 0)
			if (m_breakPoints[i].nLine == nLine)
				return true;
	}
	return false;
}

bool CLUADbg::IsTracepoint(const char* sSourceFile, int nLine)
{
	int nCount = m_breakPoints.size();
	for (int i = 0; i < nCount; i++)
	{
		if (strcmp(m_breakPoints[i].sSourceFile, sSourceFile) == 0)
			if (m_breakPoints[i].nLine == nLine)
				return m_breakPoints[i].bTracepoint;
	}
	return false;
}

void CLUADbg::ToggleTracepoint(const char* sSourceFile, int nLine)
{
	int nCount = m_breakPoints.size();
	for (int i = 0; i < nCount; i++)
	{
		if (strcmp(m_breakPoints[i].sSourceFile, sSourceFile) == 0)
		{
			if (m_breakPoints[i].nLine == nLine)
			{
				m_breakPoints[i].bTracepoint = !m_breakPoints[i].bTracepoint;
				return;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CLUADbg::AddBreakPoint(const char* sSourceFile, int nLine, bool bTracepoint, bool bSave)
{
	string sourceFile = sSourceFile;
	sourceFile.replace("@", "");
	string line = GetLineFromFile(sourceFile.c_str(), nLine);

	if (line.empty() || line.substr(0, 2) == "--" || line.substr(0, 8) == "function")
		return;

	BreakPoint bp;
	bp.nLine = nLine;
	bp.sSourceFile = sSourceFile;
	bp.bTracepoint = bTracepoint;
	stl::push_back_unique(m_breakPoints, bp);

	if (bSave)
	{
		SaveBreakpoints("breakpoints.lst");

		if (ICVar* pDebugMode = gEnv->pConsole->GetCVar("lua_debugger"))
		{
			pDebugMode->Set(eLDM_FullDebug);
			UpdateCheckboxes(m_hWnd);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CLUADbg::RemoveBreakPoint(const char* sSourceFile, int nLine)
{
	int nCount = m_breakPoints.size();
	for (int i = 0; i < nCount; i++)
	{
		if (m_breakPoints[i].nLine == nLine && strcmp(m_breakPoints[i].sSourceFile, sSourceFile) == 0)
		{
			m_breakPoints.erase(m_breakPoints.begin() + i);
			SaveBreakpoints("breakpoints.lst");
			return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CLUADbg::SaveBreakpoints(const char* filename)
{
	if (m_breakPoints.size() > 0)
	{
		FILE* file = fopen(filename, "wt");
		if (file)
		{
			int nCount = m_breakPoints.size();
			for (int i = 0; i < nCount; i++)
			{
				fprintf(file, "%d:%d:%s\n", m_breakPoints[i].nLine, m_breakPoints[i].bTracepoint ? 1 : 0, m_breakPoints[i].sSourceFile.c_str());
			}
			fclose(file);
		}
	}
	else
		DeleteFile(filename);
}

//////////////////////////////////////////////////////////////////////////
void CLUADbg::LoadBreakpoints(const char* filename)
{
	FILE* file = fopen(filename, "rt");
	if (file)
	{
		m_breakPoints.clear();
		char str[2048];
		int nLine;
		int fileLine = 1;
		int tracepoint = 0;
		while (!feof(file))
		{
			const int result = fscanf(file, "%d:%d:%2047s", &nLine, &tracepoint, str);
			if (result == 3)
			{
				string scriptName(str);
				scriptName.erase(0, 1);

				string finalPath = PathUtil::GetGameFolder() + "/" + scriptName;
				FILE* scriptFile = fopen(finalPath.c_str(), "rt");

				if (scriptFile)
				{
					AddBreakPoint(str, nLine, !!tracepoint, false);
					fclose(scriptFile);
				}
				else
				{
					CryWarning(VALIDATOR_MODULE_SCRIPTSYSTEM, VALIDATOR_WARNING, "Script: %s for breakpoint at line %d doesn't exist. Skipped adding breakpoint.", str, nLine);
				}
			}
			else if (result != EOF)
			{
				CryWarning(VALIDATOR_MODULE_SCRIPTSYSTEM, VALIDATOR_WARNING, "Malformed breakpoint information at line %d. Skipping line. Information read: %d:%s", fileLine, nLine, str);
				fscanf(file, "%*[^\n]");   // Skip to the End of the Line
				fscanf(file, "%*1[\n]");   // Skip One Newline
			}

			++fileLine;
		}
		fclose(file);
	}
}

//////////////////////////////////////////////////////////////////////////
void CLUADbg::OnDebugHook(lua_State* L, lua_Debug* ar)
{
	switch (ar->event)
	{
	// function call
	case LUA_HOOKCALL:
		m_nCallLevelUntilBreak++;

		// Fetch a description of the call and add it to the exposed call stack
		{
			lua_getinfo(L, "lnS", ar);
			const char* name = ar->name ? ar->name : "-";
			const char* source = ar->source ? ar->source : "-";
			m_pScriptSystem->ExposedCallstackPush(name, ar->currentline, source);
		}
		break;

	// return from function
	case LUA_HOOKRET:
	case LUA_HOOKTAILRET:
		if (m_breakState == bsStepOut && m_nCallLevelUntilBreak <= 0)
			Break();
		else
			m_nCallLevelUntilBreak--;

		// Remove the last call from the exposed call stack
		m_pScriptSystem->ExposedCallstackPop();
		break;

	// on each line
	case LUA_HOOKLINE:
		lua_getinfo(L, "S", ar);
		if (m_trackCoverage && *(ar->source) == '@')
			m_coverage->VisitLine(ar->source + 1, ar->currentline);

		if (m_bForceBreak)
		{
			m_nCallLevelUntilBreak = 0;
			m_breakState = bsNoBreak;
			m_bForceBreak = false;
			InvokeDebugger(ar->source, ar->currentline, "Break");
			return;
		}

		switch (m_breakState)
		{
		case bsStepInto:
			InvokeDebugger(ar->source, ar->currentline, "StepInt");
			break;
		case bsStepNext:
			if (m_nCallLevelUntilBreak <= 0 || HaveBreakPointAt(ar->source, ar->currentline))
				InvokeDebugger(ar->source, ar->currentline, "StepOver");
			break;
		default:
			{
				const bool hasBreakpoint = HaveBreakPointAt(ar->source, ar->currentline);
				const bool isTracepoint = IsTracepoint(ar->source, ar->currentline);
				if (hasBreakpoint && !isTracepoint)
				{
					InvokeDebugger(ar->source, ar->currentline, "BreakPoint");
				}
				else if (hasBreakpoint && isTracepoint)
				{
					CryLogAlways("$2Lua Tracepoint: %s - Line %d: %s", ar->short_src, ar->currentline, GetLineFromFile(ar->short_src, ar->currentline).c_str());
				}
			}
		}
	}
}

LRESULT CLUADbg::OnGoToFunc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	CGoToFuncWnd wndGoToFunc;
	wndGoToFunc.DoModal(this);
	SetFocusToEditControl();
	return 0;
}

void CLUADbg::EnableCodeCoverage(bool enable)
{
	m_trackCoverage = enable;
}

void CLUADbg::SetFocusToEditControl()
{
	::SetFocus(m_wScriptWindow.GetEditWindowHandle());
}
#endif // CRY_PLATFORM_WINDOWS
