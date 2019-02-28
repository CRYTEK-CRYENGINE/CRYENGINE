// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ImageButton.h"
#include "comutil.h"

IMPLEMENT_DYNAMIC(CImageButton, CButton)

CImageButton::CImageButton()
	: CButton()
	, m_hasFocus(false)
	, m_mouseOver(false)
	, m_iWidth(0)
	, m_iHeight(0)
	, m_imageType(eT_NONE)
	, m_pImageList(NULL)
	, m_imageListIndex(0)
{
	// First detect uxtheme.dll
	m_hUxThemeLib = LoadLibrary(_T("uxtheme.dll"));
	if (m_hUxThemeLib != NULL)
	{
		OpenThemeData = (PFNOpenThemeData)GetProcAddress(m_hUxThemeLib, "OpenThemeData");
		DrawThemeBackground = (PFNDrawThemeBackground)GetProcAddress(m_hUxThemeLib, "DrawThemeBackground");
		DrawThemeText = (PFNDrawThemeText)GetProcAddress(m_hUxThemeLib, "DrawThemeText");
		CloseThemeData = (PFNCloseThemeData)GetProcAddress(m_hUxThemeLib, "CloseThemeData");
	}
}

CImageButton::~CImageButton()
{
	// Free the theme library
	if (m_hUxThemeLib)
	{
		FreeLibrary(m_hUxThemeLib);
		m_hUxThemeLib = NULL;
	}
}

void CImageButton::OnNMThemeChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	// This feature requires Windows XP or greater.
	// The symbol _WIN32_WINNT must be >= 0x0501.
	InvalidateRect(NULL);
	*pResult = 0;
}

void CImageButton::SetImage(CImageList& imageList, uint32 imageIndex, eImageAlignment alignment)
{
	m_pImageList = &imageList;
	m_imageListIndex = imageIndex;
	m_eAlignment = alignment;

	IMAGEINFO imageInfo;
	m_pImageList->GetImageInfo(m_imageListIndex, &imageInfo);
	m_iWidth = imageInfo.rcImage.right - imageInfo.rcImage.left;
	m_iHeight = imageInfo.rcImage.bottom - imageInfo.rcImage.top;

	m_imageType = eT_IMAGELIST;
}

BEGIN_MESSAGE_MAP(CImageButton, CButton)
//{{AFX_MSG_MAP(CImageButton)
ON_WM_KILLFOCUS()
ON_WM_SETFOCUS()
ON_MESSAGE(WM_MOUSELEAVE, &CImageButton::OnMouseLeave)
ON_WM_NCHITTEST()
ON_NOTIFY_REFLECT(NM_THEMECHANGED, &CImageButton::OnNMThemeChanged)
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CImageButton message handlers

void CImageButton::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	// This code only works with buttons
	CRY_ASSERT(lpDrawItemStruct->CtlType == ODT_BUTTON);

	// Attach to the buttons device context
	CDC dc;
	dc.Attach(lpDrawItemStruct->hDC);

	// Test for visual styles
	HWND hButton = lpDrawItemStruct->hwndItem;
	HTHEME hTheme = NULL;
	if (m_hUxThemeLib != NULL)
	{
		hTheme = OpenThemeData(hButton, L"Button");
	}

	// Set the button class and style
	UINT type = DFC_BUTTON;
	UINT style = 0;

	if (hTheme != NULL)
	{
		type = BP_PUSHBUTTON;

		// Test the mouse over flag for making the button hot
		if (m_mouseOver)
		{
			style |= PBS_HOT;
		}
	}
	else
	{
		style |= DFCS_BUTTONPUSH;
	}

	// Modify the style if the button is selected
	bool selected = ((lpDrawItemStruct->itemState & ODS_SELECTED) != 0);
	if (selected)
	{
		style |= (hTheme != NULL) ? PBS_PRESSED : DFCS_PUSHED;
	}

	// Modify the style if the button is disabled
	bool disabled = ((lpDrawItemStruct->itemState & ODS_DISABLED) != 0);
	if (disabled)
	{
		style |= (hTheme != NULL) ? PBS_DISABLED : DFCS_INACTIVE;
	}

	// Draw the button frame
	if (hTheme == NULL)
	{
		::DrawFrameControl(lpDrawItemStruct->hDC, &lpDrawItemStruct->rcItem, type, style);
	}
	else
	{
		DrawThemeBackground(hTheme, lpDrawItemStruct->hDC, type, style, &lpDrawItemStruct->rcItem, NULL);
	}

	// Get the button's text and colour
	CString csButtonText;
	GetWindowText(csButtonText);
	COLORREF crTextColor = (!disabled ? ::GetSysColor(COLOR_BTNTEXT) : ::GetSysColor(COLOR_GRAYTEXT));

	// Get the button's draw rectangle and deflate by the border size
	CRect ButtonRect = lpDrawItemStruct->rcItem;
	ButtonRect.DeflateRect(4, 4);
	int offset = selected ? 1 : 0;

	CRect TextRect(ButtonRect);
	CRect ImageRect(ButtonRect);

	if (csButtonText.GetLength())
	{
		// Adjust the text rectangle to the right or left of the image rectangle
		if (m_eAlignment == eIA_LEFT)
		{
			TextRect.left += m_iWidth + 2;
			ImageRect.right = TextRect.left;
		}
		else
		{
			TextRect.right -= m_iWidth + 2;
			ImageRect.left = TextRect.right;
		}

		CString csTextExtent = csButtonText;
		csTextExtent.Remove('&');

		CSize textSize;
		textSize = dc.GetOutputTextExtent(csTextExtent);

		// Deflate the rectangle around the text, and apply any offset
		TextRect.DeflateRect((TextRect.Width() - textSize.cx) >> 1, (TextRect.Height() - textSize.cy) >> 1);
		TextRect.OffsetRect(offset, offset);

		COLORREF crOldColor = ::SetTextColor(lpDrawItemStruct->hDC, crTextColor);

		if (hTheme == NULL)
		{
			dc.DrawText(csButtonText, &TextRect, DT_SINGLELINE);
		}
		else
		{
			_bstr_t btButtonText = csButtonText;
			DrawThemeText(hTheme, lpDrawItemStruct->hDC, type, style, btButtonText, btButtonText.length(), DT_SINGLELINE, 0, &TextRect);
		}

		::SetTextColor(lpDrawItemStruct->hDC, crOldColor);
	}

	// Deflate the rectangle around the image, and apply any offset
	ImageRect.DeflateRect((ImageRect.Width() - m_iWidth) >> 1, (ImageRect.Height() - m_iHeight) >> 1);
	ImageRect.OffsetRect(offset, offset);

	// Output the bitmap
	switch (m_imageType)
	{
	case eT_IMAGELIST:
		// Text is weighted to sit lower than the centre of the text rectangle (letters descend to the bottom, but never
		// ascend to the top of the text rectangle), so the image is adjusted down by +1 to help visually match
		m_pImageList->DrawEx(&dc, m_imageListIndex, CPoint(ImageRect.left, ImageRect.top + 1), ImageRect.Size(), CLR_DEFAULT, crTextColor, (!disabled) ? ILD_NORMAL : ILD_BLEND);
		break;
	}

	// Draw the focus rectangle
	if (m_hasFocus)
	{
		ButtonRect.InflateRect(1, 1);
		dc.DrawFocusRect(ButtonRect);
	}

	// Close the buttons theme data
	if (hTheme)
	{
		CloseThemeData(hTheme);
	}

	dc.Detach();
}

void CImageButton::OnKillFocus(CWnd* pNewWnd)
{
	CButton::OnKillFocus(pNewWnd);
	m_hasFocus = false;
	InvalidateRect(NULL);
	RedrawWindow();
}

void CImageButton::OnSetFocus(CWnd* pOldWnd)
{
	CButton::OnSetFocus(pOldWnd);
	m_hasFocus = true;
	InvalidateRect(NULL);
	RedrawWindow();
}

LRESULT CImageButton::OnNcHitTest(CPoint point)
{
	if (!m_mouseOver)
	{
		m_mouseOver = true;
		InvalidateRect(NULL);
		TRACKMOUSEEVENT Track;
		Track.cbSize = sizeof(TRACKMOUSEEVENT);
		Track.dwFlags = TME_LEAVE;
		Track.dwHoverTime = 0;
		Track.hwndTrack = GetSafeHwnd();
		TrackMouseEvent(&Track);
	}

	return CButton::OnNcHitTest(point);
}

LRESULT CImageButton::OnMouseLeave(WPARAM wParam, LPARAM lParam)
{
	if (m_mouseOver)
	{
		m_mouseOver = false;
		InvalidateRect(NULL);
	}
	return 1;
}

