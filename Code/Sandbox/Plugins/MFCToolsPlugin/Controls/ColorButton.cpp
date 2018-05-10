// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ColorButton.h"

CColorButton::CColorButton()
	: m_color(::GetSysColor(COLOR_BTNFACE))
	, m_showText(true)
{
}

CColorButton::CColorButton(COLORREF color, bool showText)
	: m_color(color)
	, m_showText(showText)
{
}

CColorButton::~CColorButton()
{
}

BEGIN_MESSAGE_MAP(CColorButton, CButton)
//ON_WM_CREATE()
END_MESSAGE_MAP()
/*
   void CColorButton::::PreSubclassWindow()
   {
   ChangeStyle();
   BASE_TYPE::PreSubclassWindow();
   }

   int CColorButton::::OnCreate(LPCREATESTRUCT lpCreateStruct)
   {
   if (BASE_TYPE::OnCreate(lpCreateStruct) == -1)
    return -1;
   ChangeStyle();
   return 0;
   }
 */

void CColorButton::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	ASSERT(0 != lpDrawItemStruct);

	CDC* pDC(CDC::FromHandle(lpDrawItemStruct->hDC));
	ASSERT(0 != pDC);

	DWORD style = GetStyle();

	UINT uiDrawState(DFCS_ADJUSTRECT);
	UINT state(lpDrawItemStruct->itemState);

	/*
	   switch (lpDrawItemStruct->itemAction)
	   {
	   case ODA_DRAWENTIRE:
	   {
	    uiDrawState |= DFCS_BUTTONPUSH;
	    break;
	   }
	   case ODA_FOCUS:
	   {
	    uiDrawState |= DFCS_BUTTONPUSH;
	    break;
	   }
	   case ODA_SELECT:
	   {
	    uiDrawState |= DFCS_BUTTONPUSH;
	    if( 0 != ( state & ODS_SELECTED ) )
	    {
	      uiDrawState |= DFCS_PUSHED;
	    }
	    break;
	   }
	   }

	   if( 0 != ( state & ODS_SELECTED ) )
	   {
	   uiDrawState |= DFCS_PUSHED;
	   }
	 */

	CRect rc;
	rc.CopyRect(&lpDrawItemStruct->rcItem);

	// draw frame
	if (((state & ODS_DEFAULT) || (state & ODS_FOCUS)))
	{
		// draw def button black rectangle
		pDC->Draw3dRect(rc, GetSysColor(COLOR_WINDOWFRAME), GetSysColor(COLOR_WINDOWFRAME));
		rc.DeflateRect(1, 1);
	}

	if (style & BS_FLAT)
	{
		pDC->Draw3dRect(rc, GetSysColor(COLOR_WINDOWFRAME), GetSysColor(COLOR_WINDOWFRAME));
		//pDC->Draw3dRect(rc, GetSysColor(COLOR_INACTIVEBORDER), GetSysColor(COLOR_INACTIVEBORDER));
		rc.DeflateRect(1, 1);
		pDC->Draw3dRect(rc, GetSysColor(COLOR_WINDOW), GetSysColor(COLOR_WINDOW));
		rc.DeflateRect(1, 1);
	}
	else
	{
		if (state & ODS_SELECTED)
		{
			pDC->Draw3dRect(rc, GetSysColor(COLOR_3DSHADOW), GetSysColor(COLOR_3DSHADOW));
			rc.DeflateRect(1, 1);
			//pDC->Draw3dRect(rc, GetSysColor(COLOR_3DFACE), GetSysColor(COLOR_3DFACE));
			//rc.DeflateRect(1, 1);
		}
		else
		{
			pDC->Draw3dRect(rc, GetSysColor(COLOR_3DHILIGHT), GetSysColor(COLOR_3DDKSHADOW));
			rc.DeflateRect(1, 1);
			//pDC->Draw3dRect(rc, GetSysColor(COLOR_3DFACE), GetSysColor(COLOR_3DSHADOW));
			//rc.DeflateRect(1, 1);
		}
	}

	if (state & ODS_SELECTED)
	{
		pDC->SetBkMode(TRANSPARENT);
		pDC->FillSolidRect(&rc, m_color);
	}
	else
	{
		pDC->SetBkMode(TRANSPARENT);
		pDC->FillSolidRect(&rc, m_color);
	}

	/*

	   pDC->DrawFrameControl( &rc, DFC_BUTTON, uiDrawState );

	   rc.DeflateRect( 1, 1 );
	   pDC->FillSolidRect( &rc, m_color );

	   if( 0 != ( state & ODS_FOCUS ) )
	   {
	   pDC->DrawFocusRect( &rc );
	   }
	 */

	if (false != m_showText)
	{
		CString wndText;
		GetWindowText(wndText);

		if (!wndText.IsEmpty())
		{
			//COLORREF txtCol( RGB( ~GetRValue( m_color ), ~GetGValue( m_color ), ~GetBValue( m_color ) ) );
			pDC->SetTextColor(m_textColor);
			pDC->DrawText(wndText, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		}
	}
}

void
CColorButton::PreSubclassWindow()
{
	CButton::PreSubclassWindow();
	SetButtonStyle(BS_PUSHBUTTON | BS_OWNERDRAW);
}

void CColorButton::SetColor(const COLORREF& col)
{
	m_color = col;
	Invalidate();
}

void CColorButton::SetTextColor(const COLORREF& col)
{
	m_textColor = col;
	Invalidate();
}

COLORREF
CColorButton::GetColor() const
{
	return(m_color);
}

void
CColorButton::SetShowText(bool showText)
{
	m_showText = showText;
	Invalidate();
}

bool
CColorButton::GetShowText() const
{
	return(m_showText);
}

