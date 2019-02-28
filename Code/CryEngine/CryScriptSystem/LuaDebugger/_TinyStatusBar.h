// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __TINY_STATUS_BAR__
#define __TINY_STATUS_BAR__

#pragma once

#ifndef _TINY_WINDOW_H_
	#error "_TinyStatusBar requires <_TinyWindow.h>"
#endif

class _TinyStatusBar : public _TinyWindow
{
public:
	_TinyStatusBar() {};
	virtual ~_TinyStatusBar() {};

	BOOL Create(ULONG nID = 0, const _TinyRect* pRect = NULL, _TinyWindow* pParentWnd = NULL)
	{
		if (!_TinyWindow::Create(STATUSCLASSNAME, _T(""), WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, NULL, pRect, pParentWnd, nID))
		{
			_TINY_CHECK_LAST_ERROR
			return FALSE;
		}
		if (pParentWnd)
		{
			_TinyRect rc;
			GetClientRect(&rc);
			Reshape(rc.right - rc.left, rc.bottom - rc.top);
		}
		return TRUE;
	};

	void SetPanelText(const char* pszText)
	{
		SendMessage(SB_SETTEXT, SB_SIMPLEID, (LPARAM) pszText);
	};

protected:

};

#endif
