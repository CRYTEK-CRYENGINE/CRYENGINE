// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _C_COLORBUTTON_H__INCLUDED
#define _C_COLORBUTTON_H__INCLUDED

#pragma once

// To use:
// 1) Add button to dialog (optionally set "Owner draw" property to "True",
//    but it's been enforced by the code already -- see CColorButton::PreSubclassWindow() )
// 2) Create member variable for button in dialog (via wizard or manually) .
// 3) Initialize button (color, text display) cia c/tor or Set*** in dialog's Constructor/OnInitDialog.
// 4) If added manually (step 2) add a something like "DDX_Control( pDX, IDC_COLOR_BUTTON, m_myColButton );"
//    to your dialog's DoDataExchange( ... ) method.

class CColorButton : public CButton
{
public:

public:
	CColorButton();
	CColorButton(COLORREF color, bool showText);
	virtual ~CColorButton();

	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
	virtual void PreSubclassWindow();

	void         SetColor(const COLORREF& col);
	COLORREF     GetColor() const;

	void         SetTextColor(const COLORREF& col);
	COLORREF     GetTexColor() const { return m_textColor; };

	void         SetShowText(bool showText);
	bool         GetShowText() const;

protected:

	DECLARE_MESSAGE_MAP()

	COLORREF m_color;
	COLORREF m_textColor;
	bool     m_showText;
};

#endif // #ifndef _C_COLOREDBUTTON_H__INCLUDED

