// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _TINY_CAPTION_WINDOW_H_
#define _TINY_CAPTION_WINDOW_H_

#pragma once

#include "_TinyWindow.h"

class _TinyCaptionWindow : public _TinyWindow
{
public:
	_TinyCaptionWindow() { m_pContent = NULL; };
	~_TinyCaptionWindow() {};

	BOOL Create(const char* pszTitle, _TinyWindow* pContent, _TinyWindow* pParent)
	{
		m_strCaption = pszTitle;
		if (!_TinyWindow::Create(_T("_default_TinyWindowClass"), pszTitle, WS_VISIBLE | WS_CHILD, 0, NULL, pParent))
			return FALSE;
		m_pContent = pContent;
		m_pContent->MakeChild();
		m_pContent->SetParent(this);
		NotifyReflection(TRUE);
		return TRUE;
	};

	bool Reshape(int iCX, int iCY)
	{
		_TinyRect rcClient;
		_TinyAssert(m_pContent);
		_TinyWindow::Reshape(iCX, iCY);
		GetClientRect(&rcClient);
		m_pContent->SetWindowPos(0, iTitleBarHeight, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		m_pContent->Reshape(rcClient.right, rcClient.bottom - iTitleBarHeight);
		return true;
	};

protected:
	_TinyWindow* m_pContent;
	string       m_strCaption;

	enum { iTitleBarHeight = 13 };

	_BEGIN_MSG_MAP(_TinyCaptionWindow)
	_MESSAGE_HANDLER(WM_PAINT, OnPaint)
	_END_MSG_MAP()

	LRESULT OnPaint(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		_TinyRect rcClient;
		PAINTSTRUCT ps;
		HDC hDC;

		::BeginPaint(m_hWnd, &ps);
		hDC = ::GetDC(m_hWnd);
		GetClientRect(&rcClient);
		rcClient.bottom = iTitleBarHeight;
		rcClient.right -= 1;
		::FillRect(hDC, &rcClient, GetSysColorBrush(COLOR_ACTIVECAPTION));
		::SelectObject(hDC, GetStockObject(ANSI_VAR_FONT));
		_TinyVerify(::SetTextColor(hDC, GetSysColor(COLOR_CAPTIONTEXT)) != CLR_INVALID);
		::SetBkMode(hDC, TRANSPARENT);
		::DrawTextA(hDC, m_strCaption.c_str(), -1, &rcClient, DT_CENTER | DT_VCENTER);
		EndPaint(m_hWnd, &ps);

		return 0;
	};
};

#endif
