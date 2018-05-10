// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ColorCtrl.h"

class PLUGIN_API CCustomButton : public CXTPButton
{
public:
	CCustomButton();
	//! Set icon to display on button, (must not be shared).
	void SetIcon(HICON hIcon, int nIconAlign = BS_CENTER, bool bDrawText = false);
	void SetIcon(LPCSTR lpszIconResource, int nIconAlign = BS_CENTER, bool bDrawText = false);
	void SetToolTip(const char* tooltipText);
};
typedef CCustomButton CColoredPushButton;

