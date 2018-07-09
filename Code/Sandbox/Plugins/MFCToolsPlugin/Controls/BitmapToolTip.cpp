// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BitmapToolTip.h"
#include "QtUtil.h"
#include <QGuiApplication>
#include "Controls/SharedFonts.h"
#include "Util/MFCUtil.h"
#include "Util/Image.h"
#include "Util/ImageUtil.h"

static const int STATIC_TEXT_C_HEIGHT = 42;
static const int HISTOGRAM_C_HEIGHT = 130;

/////////////////////////////////////////////////////////////////////////////
// CBitmapToolTip
CBitmapToolTip::CBitmapToolTip()
{
	m_nTimer = 0;
	m_hToolWnd = 0;
	m_toolRect.SetRectEmpty();
	m_bShowHistogram = true;
	m_bShowFullsize = false;
}

CBitmapToolTip::~CBitmapToolTip()
{
	if (m_previewBitmap.m_hObject)
		m_previewBitmap.DeleteObject();
}

BEGIN_MESSAGE_MAP(CBitmapToolTip, CWnd)
ON_WM_CREATE()
ON_WM_SHOWWINDOW()
ON_WM_ERASEBKGND()
ON_WM_TIMER()
ON_WM_DESTROY()
ON_WM_KEYDOWN()
ON_WM_KEYUP()
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_MBUTTONDOWN()
ON_WM_MBUTTONUP()
ON_WM_MOUSEMOVE()
ON_WM_RBUTTONDOWN()
ON_WM_RBUTTONUP()
ON_WM_MOUSEWHEEL()
ON_WM_SIZE()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CBitmapToolTip::GetShowMode(EShowMode& eShowMode, bool& bShowInOriginalSize) const
{
	bShowInOriginalSize = Deprecated::CheckVirtualKey(VK_SPACE);
	eShowMode = ESHOW_RGB;

	Qt::KeyboardModifiers modifiers = QGuiApplication::queryKeyboardModifiers();
	if (m_bHasAlpha)
	{
		if (modifiers & Qt::ControlModifier)
		{
			eShowMode = ESHOW_RGB_ALPHA;
		}
		else if (modifiers & Qt::AltModifier)
		{
			eShowMode = ESHOW_ALPHA;
		}
		else if (modifiers & Qt::ShiftModifier)
		{
			eShowMode = ESHOW_RGBA;
		}
	}
	else if (m_bIsLimitedHDR)
	{
		if (modifiers & Qt::ShiftModifier)
		{
			eShowMode = ESHOW_RGBE;
		}
	}
}

const char* CBitmapToolTip::GetShowModeDescription(EShowMode eShowMode, bool bShowInOriginalSize) const
{
	switch (eShowMode)
	{
	case ESHOW_RGB:
		return "RGB";
	case ESHOW_RGB_ALPHA:
		return "RGB+A";
	case ESHOW_ALPHA:
		return "Alpha";
	case ESHOW_RGBA:
		return "RGBA";
	case ESHOW_RGBE:
		return "RGBExp";
	}

	return "";
}

void CBitmapToolTip::RefreshLayout()
{
	if (m_staticBitmap.m_hWnd)
	{
		CRect rcClient;
		GetClientRect(rcClient);

		if (rcClient.Width() <= 0)
			return;
		if (rcClient.Height() <= 0)
			return;

		CRect rc(rcClient);
		int histoHeight = (m_bShowHistogram ? HISTOGRAM_C_HEIGHT : 0);

		rc.bottom -= STATIC_TEXT_C_HEIGHT - histoHeight;
		m_staticBitmap.MoveWindow(rc, FALSE);

		rc = rcClient;
		rc.top = rc.bottom - STATIC_TEXT_C_HEIGHT - histoHeight;
		m_staticText.MoveWindow(rc, FALSE);

		if (m_bShowHistogram && m_alphaChannelHistogram.GetSafeHwnd() && m_rgbaHistogram.GetSafeHwnd())
		{
			CRect rcHistoRgba, rcHistoAlpha;

			rcHistoRgba.SetRectEmpty();
			rcHistoAlpha.SetRectEmpty();

			if (m_eShowMode == ESHOW_RGB_ALPHA || m_eShowMode == ESHOW_RGBA)
			{
				rc = rcClient;
				rc.top = rc.bottom - histoHeight;
				rc.right = rc.Width() / 2;
				rcHistoRgba = rc;

				rc = rcClient;
				rc.top = rc.bottom - histoHeight;
				rc.left = rcHistoRgba.right;
				rc.right = rcClient.Width();
				rcHistoAlpha = rc;

				m_rgbaHistogram.ShowWindow(TRUE);
				m_alphaChannelHistogram.ShowWindow(TRUE);
			}
			else if (m_eShowMode == ESHOW_ALPHA)
			{
				rc = rcClient;
				rc.top = rc.bottom - histoHeight;
				rc.right = rc.Width();
				rcHistoAlpha = rc;

				m_rgbaHistogram.ShowWindow(FALSE);
				m_alphaChannelHistogram.ShowWindow(TRUE);
			}
			else
			{
				rc = rcClient;
				rc.top = rc.bottom - histoHeight;
				rc.right = rc.Width();
				rcHistoRgba = rc;

				m_rgbaHistogram.ShowWindow(TRUE);
				m_alphaChannelHistogram.ShowWindow(FALSE);
			}

			m_rgbaHistogram.MoveWindow(rcHistoRgba);
			m_alphaChannelHistogram.MoveWindow(rcHistoAlpha);
		}
	}
}

void CBitmapToolTip::RefreshViewmode()
{
	EShowMode eShowMode = ESHOW_RGB;
	bool bShowInOriginalSize = false;

	GetShowMode(eShowMode, bShowInOriginalSize);

	if ((m_eShowMode != eShowMode) || (m_bShowFullsize != bShowInOriginalSize))
	{
		LoadImage(m_filename);

		RefreshLayout();
		CorrectPosition();
		UpdateWindow();
	}
}

bool CBitmapToolTip::LoadImage(const CString& imageFilename)
{
	if (!m_hWnd)
		return false;

	EShowMode eShowMode = ESHOW_RGB;
	const char* pShowModeDescription = "RGB";
	bool bShowInOriginalSize = false;

	GetShowMode(eShowMode, bShowInOriginalSize);
	pShowModeDescription = GetShowModeDescription(eShowMode, bShowInOriginalSize);

	if ((m_filename == imageFilename) && (m_eShowMode == eShowMode) && (m_bShowFullsize == bShowInOriginalSize))
		return true;

	m_eShowMode = eShowMode;
	m_bShowFullsize = bShowInOriginalSize;

	CImageEx image;
	image.SetHistogramEqualization(QtUtil::IsModifierKeyDown(Qt::ShiftModifier));

	if (CImageUtil::LoadImage(imageFilename, image))
	{
		CString imginfo;
		const char* infoformat;

		m_filename = imageFilename;
		m_bHasAlpha = image.HasAlphaChannel();
		m_bIsLimitedHDR = image.IsLimitedHDR();

		GetShowMode(eShowMode, bShowInOriginalSize);
		pShowModeDescription = GetShowModeDescription(eShowMode, bShowInOriginalSize);

		if (m_bHasAlpha)
		{
			infoformat = "%dx%d %s\nShowing %s (ALT=Alpha, SHIFT=RGBA, CTRL=RGB+A, SPACE=see in original size)";
		}
		else if (m_bIsLimitedHDR)
		{
			infoformat = "%dx%d %s\nShowing %s (SHIFT=see hist.-equalized, SPACE=see in original size)";
		}
		else
		{
			infoformat = "%dx%d %s\nShowing %s (SPACE=see in original size)";
		}

		imginfo.Format(infoformat, image.GetWidth(), image.GetHeight(),
		               (const char*)image.GetFormatDescription(), pShowModeDescription);

		m_staticText.SetWindowText(imginfo);

		int w = image.GetWidth();
		int h = image.GetHeight();
		int multiplier = (m_eShowMode == ESHOW_RGB_ALPHA ? 2 : 1);
		int originalW = w * multiplier;
		int originalH = h;

		if (!bShowInOriginalSize || (w == 0))
			w = 256;
		if (!bShowInOriginalSize || (h == 0))
			h = 256;

		w *= multiplier;

		SetWindowPos(NULL, 0, 0, w + 4, h + 4 + STATIC_TEXT_C_HEIGHT + HISTOGRAM_C_HEIGHT, SWP_NOMOVE);
		ShowWindow(SW_SHOW);

		CImageEx scaledImage;

		if (bShowInOriginalSize && (originalW < w))
			w = originalW;
		if (bShowInOriginalSize && (originalH < h))
			h = originalH;

		scaledImage.Allocate(w, h);

		if (m_eShowMode == ESHOW_RGB_ALPHA)
			CImageUtil::ScaleToDoubleFit(image, scaledImage);
		else
			CImageUtil::ScaleToFit(image, scaledImage);

		if (m_eShowMode == ESHOW_RGB || m_eShowMode == ESHOW_RGBE)
		{
			scaledImage.SwapRedAndBlue();
			scaledImage.FillAlpha();
		}
		else if (m_eShowMode == ESHOW_ALPHA)
		{
			for (int h = 0; h < scaledImage.GetHeight(); h++)
				for (int w = 0; w < scaledImage.GetWidth(); w++)
				{
					int a = scaledImage.ValueAt(w, h) >> 24;
					scaledImage.ValueAt(w, h) = RGB(a, a, a);
				}
		}
		else if (m_eShowMode == ESHOW_RGB_ALPHA)
		{
			int halfWidth = scaledImage.GetWidth() / 2;
			for (int h = 0; h < scaledImage.GetHeight(); h++)
				for (int w = 0; w < halfWidth; w++)
				{
					int r = GetRValue(scaledImage.ValueAt(w, h));
					int g = GetGValue(scaledImage.ValueAt(w, h));
					int b = GetBValue(scaledImage.ValueAt(w, h));
					int a = scaledImage.ValueAt(w, h) >> 24;
					scaledImage.ValueAt(w, h) = RGB(b, g, r);
					scaledImage.ValueAt(w + halfWidth, h) = RGB(a, a, a);
				}
		}
		else //if (m_showMode == ESHOW_RGBA)
			scaledImage.SwapRedAndBlue();

		Invalidate();

		if (m_previewBitmap.m_hObject)
			m_previewBitmap.DeleteObject();

		m_previewBitmap.CreateBitmap(scaledImage.GetWidth(), scaledImage.GetHeight(), 1, 32, scaledImage.GetData());
		m_staticBitmap.SetBitmap(m_previewBitmap);

		if (m_bShowHistogram && scaledImage.GetData() && m_alphaChannelHistogram.GetSafeHwnd() && m_rgbaHistogram.GetSafeHwnd())
		{
			m_rgbaHistogram.ComputeHistogram((BYTE*)image.GetData(), image.GetWidth(), image.GetHeight(), CImageHistogramCtrl::eImageFormat_32BPP_BGRA);
			m_rgbaHistogram.m_drawMode = CImageHistogramCtrl::eDrawMode_OverlappedRGB;
			m_alphaChannelHistogram.CopyComputedDataFrom(&m_rgbaHistogram);
			m_alphaChannelHistogram.m_drawMode = CImageHistogramCtrl::eDrawMode_AlphaChannel;
		}

		CorrectPosition();
		UpdateWindow();
		return true;
	}
	else
	{
		m_staticBitmap.SetBitmap(NULL);
	}

	RedrawWindow();
	return false;
}

//////////////////////////////////////////////////////////////////////////
BOOL CBitmapToolTip::Create(const RECT& rect)
{
	CString myClassName = AfxRegisterWndClass(CS_VREDRAW | CS_HREDRAW, ::LoadCursor(NULL, IDC_ARROW), (HBRUSH)::GetStockObject(NULL_BRUSH), NULL);

	BOOL bRes = CreateEx(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, myClassName, 0, WS_VISIBLE | WS_POPUP | WS_BORDER, rect, GetDesktopWindow(), 0);
	if (bRes)
	{
		//ModifyStyleEx( 0,WS_EX_TOOLWINDOW|WS_EX_TOPMOST );
		CRect rcIn;
		GetClientRect(rcIn);
		m_staticBitmap.Create("", WS_CHILD | WS_VISIBLE | SS_BITMAP, rcIn, this, 0);
		m_staticText.Create("", WS_CHILD | WS_VISIBLE | SS_LEFT, CRect(0, 0, 0, 0), this, 1);
		m_staticBitmap.ModifyStyleEx(0, WS_EX_STATICEDGE);
		m_staticText.ModifyStyleEx(0, WS_EX_STATICEDGE);
		m_staticText.SetFont(CFont::FromHandle((HFONT)gMFCFonts.hSystemFont));
		m_rgbaHistogram.Create(NULL, "Histogram", WS_CHILD | WS_VISIBLE, CRect(0, 0, 10, 10), this, 2);
		m_rgbaHistogram.m_graphHeightPercent = 0.8f;
		m_alphaChannelHistogram.Create(NULL, "HistogramAlpha", WS_CHILD | WS_VISIBLE, CRect(0, 0, 10, 10), this, 3);
		m_alphaChannelHistogram.m_graphHeightPercent = 0.8f;
	}
	return bRes;
}

//////////////////////////////////////////////////////////////////////////
int CBitmapToolTip::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (__super::OnCreate(lpCreateStruct) == -1)
		return -1;

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CBitmapToolTip::PreSubclassWindow()
{
	__super::PreSubclassWindow();
}

//////////////////////////////////////////////////////////////////////////
BOOL CBitmapToolTip::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}

void CBitmapToolTip::OnTimer(UINT_PTR nIDEvent)
{
	/*
	   if (IsWindowVisible())
	   {
	   if (m_bHaveAnythingToRender)
	    Invalidate();
	   }
	 */
	if (::IsWindow(m_hToolWnd) && CWnd::FromHandle(m_hToolWnd))
	{
		CWnd* pWnd = CWnd::FromHandle(m_hToolWnd);
		CRect toolRc(m_toolRect);
		pWnd->ClientToScreen(&toolRc);
		CPoint cursorPos;
		::GetCursorPos(&cursorPos);
		if (!toolRc.PtInRect(cursorPos))
		{
			ShowWindow(SW_HIDE);
		}
		else
		{
			RefreshViewmode();
		}
	}

	CWnd::OnTimer(nIDEvent);
}

//////////////////////////////////////////////////////////////////////////
void CBitmapToolTip::OnDestroy()
{
	m_filename = "";
	CWnd::OnDestroy();

	if (m_nTimer)
		KillTimer(m_nTimer);
}

//////////////////////////////////////////////////////////////////////////
void CBitmapToolTip::OnShowWindow(BOOL bShow, UINT nStatus)
{
	if (!bShow)
	{
		m_filename = "";
		if (m_nTimer)
			KillTimer(m_nTimer);
	}
	else
	{
		m_nTimer = SetTimer(1, 500, NULL);
	}
}

//////////////////////////////////////////////////////////////////////////

void CBitmapToolTip::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_CONTROL || nChar == VK_MENU || nChar == VK_SHIFT)
	{
		RefreshViewmode();
	}
}

void CBitmapToolTip::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_CONTROL || nChar == VK_MENU || nChar == VK_SHIFT)
	{
		RefreshViewmode();
	}
}

void CBitmapToolTip::OnLButtonDown(UINT nFlags, CPoint point)
{
}

void CBitmapToolTip::OnLButtonUp(UINT nFlags, CPoint point)
{
}

void CBitmapToolTip::OnMButtonDown(UINT nFlags, CPoint point)
{
}

void CBitmapToolTip::OnMButtonUp(UINT nFlags, CPoint point)
{
}

void CBitmapToolTip::OnMouseMove(UINT nFlags, CPoint point)
{
}

void CBitmapToolTip::OnRButtonDown(UINT nFlags, CPoint point)
{
}

void CBitmapToolTip::OnRButtonUp(UINT nFlags, CPoint point)
{
}

BOOL CBitmapToolTip::OnMouseWheel(UINT nFlags, short zDelta, CPoint point)
{
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CBitmapToolTip::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);
	RefreshLayout();
}

void CBitmapToolTip::CorrectPosition()
{
	// Move window inside monitor
	CRect rc;
	GetWindowRect(rc);

	POINT pos;
	pos.x = rc.right;
	pos.y = rc.bottom;

	HMONITOR hMon = MonitorFromPoint(pos, MONITOR_DEFAULTTONULL);
	if (!hMon)
	{
		GetCursorPos(&pos);

		hMon = MonitorFromPoint(pos, MONITOR_DEFAULTTOPRIMARY);
		if (!hMon)
			return;
	}

	MONITORINFO mi = { 0 };
	mi.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(hMon, &mi);

	CWnd* pWnd = CWnd::FromHandle(m_hToolWnd);
	CRect toolRc(m_toolRect);
	pWnd->ClientToScreen(&toolRc);

	bool bOffset = false;
	int cy = GetSystemMetrics(SM_CYCURSOR);
	int dy;
	CRect go = rc;

	// move to the default position
	go.right = toolRc.right + rc.Width();
	go.left = toolRc.right;
	go.bottom = toolRc.top + rc.Height() + m_toolRect.Height();
	go.top = toolRc.top + m_toolRect.Height();

	// If the texture clips monitor's right, move it left.
	// We move in such a way that the tool is not covered.
	if (go.right > mi.rcMonitor.right)
	{
		go.left = toolRc.left - rc.Width();
		go.right = toolRc.left;
	}

	// if the texture clips monitor's bottom, move it up
	if (go.bottom > mi.rcMonitor.bottom)
	{
		dy = mi.rcMonitor.bottom - go.bottom;
		go.OffsetRect(0, dy);
	}

	// if the texture clips monitor's top, move it down
	if (go.top < mi.rcMonitor.top)
	{
		dy = mi.rcMonitor.bottom - go.bottom;
		go.OffsetRect(0, dy);
	}

	// move window only if position changed
	if (go != rc)
	{
		MoveWindow(go);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBitmapToolTip::SetTool(CWnd* pWnd, const CRect& rect)
{
	assert(pWnd);
	m_hToolWnd = pWnd->GetSafeHwnd();
	m_toolRect = rect;
}

