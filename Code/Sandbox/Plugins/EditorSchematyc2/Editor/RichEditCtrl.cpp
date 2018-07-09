// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RichEditCtrl.h"

namespace Schematyc2
{
	namespace
	{
		//////////////////////////////////////////////////////////////////////////
		DWORD __stdcall EditStreamCallback(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb)
		{
			LONG	copyLength = 0;
			if(const char** ppSource = reinterpret_cast<const char**>(dwCookie))
			{
				if(const char* pSource = *ppSource)
				{
					if(copyLength = std::min<LONG>(cb, pSource ? strlen(pSource) : 0))
					{
						memcpy(pbBuff, pSource, copyLength);
						*ppSource += copyLength;
					}
				}
			}
			*pcb = copyLength;
			return copyLength == 0;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL CCustomRichEditCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
	{
		LOADING_TIME_PROFILE_SECTION;
		if(CRichEditCtrl::Create(dwStyle, rect, pParentWnd, nID))
		{
			CRichEditCtrl::SetTextMode(SF_TEXT);
			CRichEditCtrl::SetFont(CFont::FromHandle(static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT))));
			return TRUE;
		}
		return FALSE;
	}

	//////////////////////////////////////////////////////////////////////////
	void CCustomRichEditCtrl::SetText(const char* text)
	{
		CRY_ASSERT(text);
		if(text)
		{
			EDITSTREAM	editStream;
			const char*	pos = text;
			editStream.dwCookie			= reinterpret_cast<DWORD_PTR>(&pos);
			editStream.pfnCallback	= EditStreamCallback; 
			CRichEditCtrl::StreamIn(SF_TEXT, editStream);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CCustomRichEditCtrl::AppendText(const char* text)
	{
		CRY_ASSERT(text);
		if(text)
		{
			long	textLength = CRichEditCtrl::GetTextLength();
			long	selStart = textLength;
			long	selEnd = -1;
			CRichEditCtrl::GetSel(selStart, selEnd);
			CRichEditCtrl::SetSel(textLength, -1);
			EDITSTREAM	editStream;
			const char*	pos = text;
			editStream.dwCookie			= reinterpret_cast<DWORD_PTR>(&pos);
			editStream.pfnCallback	= EditStreamCallback; 
			CRichEditCtrl::StreamIn(SF_TEXT | SFF_SELECTION, editStream);
			CRichEditCtrl::SetSel(selStart, selEnd);
			CRichEditCtrl::LineScroll(CRichEditCtrl::GetLineCount());
		}
	}
}
