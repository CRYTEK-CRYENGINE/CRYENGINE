// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "SharedFonts.h"

SMFCFonts gMFCFonts;

SMFCFonts& SMFCFonts::GetInstance()
{
	return gMFCFonts;
}

SMFCFonts::SMFCFonts()
{
	int lfHeight = -MulDiv(8, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72);
	nDefaultFontHieght = lfHeight;
	hSystemFont = ::CreateFont(lfHeight, 0, 0, 0, FW_NORMAL, 0, 0, 0,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH, "Ms Shell Dlg 2");

	hSystemFontBold = ::CreateFont(lfHeight, 0, 0, 0, FW_BOLD, 0, 0, 0,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH, "Ms Shell Dlg 2");

	hSystemFontItalic = ::CreateFont(lfHeight, 0, 0, 0, FW_NORMAL, TRUE, 0, 0,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH, "Ms Shell Dlg 2");
}

