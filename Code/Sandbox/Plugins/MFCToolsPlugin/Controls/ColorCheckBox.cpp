// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ColorCheckBox.h"

void CCustomButton::SetIcon(LPCSTR lpszIconResource, int nIconAlign, bool bDrawText)
{
	SetWindowText("");
	HICON hIcon = (HICON)::LoadImage(AfxGetInstanceHandle(), lpszIconResource, IMAGE_ICON, 0, 0, 0);
	__super::SetIcon(CSize(32, 32), hIcon, NULL, FALSE);
}

void CCustomButton::SetIcon(HICON hIcon, int nIconAlign, bool bDrawText)
{
	SetWindowText("");
	__super::SetIcon(CSize(32, 32), hIcon, NULL, FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CCustomButton::SetToolTip(const char* tooltipText)
{

}

CCustomButton::CCustomButton()
{
	SetTheme(xtpButtonThemeOffice2003);
}

