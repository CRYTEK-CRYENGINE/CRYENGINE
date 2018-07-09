// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _TINYCOOLEDIT_H_
#define _TINYCOOLEDIT_H_

#ifndef _TINY_WINDOW_H_
	#error "_TinyCoolEdit require <_TinyWindow.h>"
#endif

class _TinyCoolEdit : public _TinyRichEdit
{
	virtual BOOL Create(ULONG nID = 0, DWORD dwStyle = WS_VISIBLE, DWORD dwExStyle = 0, const RECT* pRect = NULL, _TinyWindow* pParentWnd = NULL)
	{
		BOOL bRes = _TinyEdit::Create(nID, dwStyle, dwExStyle, pRect, pParentWnd);
		if (!bRes) return FALSE;
		return TRUE;
	}
	bool Reshape(int w, int h)
	{
	}
};

#endif //_TINYCOOLEDIT_H_
