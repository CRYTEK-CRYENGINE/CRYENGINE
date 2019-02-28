// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FillSliderCtrl.h"

IMPLEMENT_DYNCREATE(CFillSliderCtrl, CSliderCtrlEx)

/////////////////////////////////////////////////////////////////////////////
// CFillSliderCtrl

CFillSliderCtrl::CFillSliderCtrl()
{
	m_bFilled = true;
	m_fillStyle = 0;
	m_fillColorStart = ::GetSysColor(COLOR_HIGHLIGHT);//RGB(100, 100, 100);
	m_fillColorEnd = ::GetSysColor(COLOR_GRAYTEXT);//RGB(180, 180, 180);
	m_mousePos.SetPoint(0, 0);
}

CFillSliderCtrl::~CFillSliderCtrl()
{
}

BEGIN_MESSAGE_MAP(CFillSliderCtrl, CSliderCtrlEx)
ON_WM_PAINT()
ON_WM_MOUSEMOVE()
ON_WM_ERASEBKGND()
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_ENABLE()
ON_WM_SETFOCUS()
ON_WM_SIZE()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
BOOL CFillSliderCtrl::OnEraseBkgnd(CDC* pDC)
{
	if (!m_bFilled)
		return __super::OnEraseBkgnd(pDC);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CFillSliderCtrl::DrawFill(CDC& dc, CRect& rect)
{
	// Reloading values in case the skin was changed
	//m_fillColorStart = ::GetSysColor(COLOR_HIGHLIGHT);
	//m_fillColorEnd = ::GetSysColor(COLOR_GRAYTEXT);
	m_fillColorStart = RGB(140, 140, 140);
	m_fillColorEnd = RGB(140, 140, 140);
	if (m_fillStyle & eFillStyle_ColorHueGradient)
	{
		int width = rect.Width();
		int height = rect.Height();
		f32 step = 1.0f / f32(width);
		f32 weight = 0.0f;
		ColorF color;
		for (int i = 0; i < width; ++i)
		{
			color.fromHSV(weight, 1.0f, 1.0f);
			::XTPDrawHelpers()->DrawLine(
			  &dc, rect.left + i, rect.top, 0, height, color.pack_bgr888());
			weight += step;
		}
		return;
	}

	COLORREF colorStart = m_fillColorStart;
	COLORREF colorEnd = m_fillColorEnd;
	if (!IsWindowEnabled())
	{
		colorStart = RGB(190, 190, 190);
		colorEnd = RGB(200, 200, 200);
	}

	if (m_fillStyle & eFillStyle_Gradient)
	{
		::XTPPaintManager()->GradientFill(
		  &dc, rect, colorStart, colorEnd, !(m_fillStyle & eFillStyle_Vertical));
	}
	else
	{
		CBrush brush(colorStart);
		dc.FillRect(rect, &brush);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFillSliderCtrl::OnPaint()
{
	if (!m_bFilled)
	{
		__super::OnPaint();
		return;
	}

	CPaintDC dc(this); // device context for painting

	float val = m_value;

	CRect rc;
	GetClientRect(rc);
	rc.top += 1;
	rc.bottom -= 1;
	float pos = (val - m_min) / fabs(m_max - m_min);
	int splitPos = pos * rc.Width();

	if (splitPos < rc.left)
		splitPos = rc.left;
	if (splitPos > rc.right)
		splitPos = rc.right;

	if (m_fillStyle & eFillStyle_Background)
	{
		DrawFill(dc, rc);
	}
	else
	{
		//dc.Draw3dRect(&rc,GetXtremeColor(COLOR_3DSHADOW),GetXtremeColor(COLOR_3DLIGHT));
		//rc.DeflateRect(1,1);

		// Paint filled rect.
		CRect fillRc = rc;
		fillRc.right = splitPos;
		DrawFill(dc, fillRc);

		// Paint empty rect.
		CRect emptyRc = rc;
		emptyRc.left = splitPos + 1;
		emptyRc.IntersectRect(emptyRc, rc);
		COLORREF colour = RGB(100, 100, 100);
		if (!IsWindowEnabled())
			colour = RGB(230, 230, 230);
		CBrush brush(colour);
		dc.FillRect(emptyRc, &brush);
	}

	if (IsWindowEnabled())
	{
		if (splitPos == rc.left)
			splitPos += 1;
		if (splitPos == rc.right)
			splitPos -= 1;
		// Draw marker at split position, with yellow pen.
		CPen markerPen1, markerPen2, markerPen3;
		markerPen1.CreatePen(PS_SOLID, 1, RGB(255, 170, 0));
		markerPen2.CreatePen(PS_SOLID, 1, RGB(175, 130, 45));
		markerPen3.CreatePen(PS_SOLID, 1, RGB(190, 150, 62));
		CPen* pOldPen = dc.GetCurrentPen();
		dc.SelectObject(&markerPen1);
		int yofs = 0;
		dc.MoveTo(CPoint(splitPos, rc.top + yofs));
		dc.LineTo(CPoint(splitPos, rc.bottom - yofs));
		dc.SelectObject(&markerPen2);
		dc.MoveTo(CPoint(splitPos + 1, rc.top + yofs));
		dc.LineTo(CPoint(splitPos + 1, rc.bottom - yofs));
		dc.SelectObject(&markerPen3);
		dc.MoveTo(CPoint(splitPos - 1, rc.top + yofs));
		dc.LineTo(CPoint(splitPos - 1, rc.bottom - yofs));
		dc.SelectObject(pOldPen);
	}

	// Do not call __super::OnPaint() for painting messages
}

//////////////////////////////////////////////////////////////////////////
void CFillSliderCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (!m_bFilled)
	{
		__super::OnLButtonDown(nFlags, point);
		return;
	}

	if (!IsWindowEnabled())
		return;

	CWnd* parent = GetParent();
	if (parent)
	{
		::SendMessage(parent->GetSafeHwnd(), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), WMU_FS_LBUTTONDOWN), (LPARAM)GetSafeHwnd());
	}

	m_bUndoStarted = false;
	// Start undo.
	if (m_bUndoEnabled)
	{
		GetIEditor()->GetIUndoManager()->Begin();
		m_bUndoStarted = true;
	}

	ChangeValue(point.x, false);

	m_mousePos = point;
	SetCapture();
	m_bDragging = true;
}

//////////////////////////////////////////////////////////////////////////
void CFillSliderCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (!m_bFilled)
	{
		__super::OnLButtonUp(nFlags, point);
		return;
	}

	if (!IsWindowEnabled())
		return;

	bool bLButonDown = false;

	if (m_bUndoStarted && GetIEditor()->GetIUndoManager()->IsUndoSuspended())
		GetIEditor()->GetIUndoManager()->Resume();

	m_bDragging = false;
	if (GetCapture() == this)
	{
		bLButonDown = true;
		ReleaseCapture();
	}

	if (bLButonDown && m_bUndoStarted)
	{
		if (GetIEditor()->GetIUndoManager()->IsUndoRecording())
			GetIEditor()->GetIUndoManager()->Accept(m_undoText);
		m_bUndoStarted = false;
	}

	CWnd* parent = GetParent();
	if (parent)
	{
		::SendMessage(parent->GetSafeHwnd(), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), WMU_FS_LBUTTONUP), (LPARAM)GetSafeHwnd());
	}
}

void CFillSliderCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	if (!m_bFilled)
	{
		__super::OnMouseMove(nFlags, point);
		return;
	}

	if (!IsWindowEnabled())
		return;

	if (point == m_mousePos)
		return;
	m_mousePos = point;

	if (m_bDragging)
	{
		ChangeValue(point.x, true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFillSliderCtrl::ChangeValue(int sliderPos, bool bTracking)
{
	if (m_bLocked)
		return;

	CRect rc;
	GetClientRect(rc);
	if (sliderPos < rc.left)
		sliderPos = rc.left;
	if (sliderPos > rc.right)
		sliderPos = rc.right;

	float pos = (float)sliderPos / rc.Width();
	m_value = m_min + pos * fabs(m_max - m_min);

	NotifyUpdate(bTracking);
	RedrawWindow();
}

//////////////////////////////////////////////////////////////////////////
void CFillSliderCtrl::SetValue(float val)
{
	__super::SetValue(val);
	if (m_hWnd && m_bFilled)
		RedrawWindow();
}

//////////////////////////////////////////////////////////////////////////
void CFillSliderCtrl::NotifyUpdate(bool tracking)
{
	if (m_noNotify)
		return;

	if (m_updateCallback)
		m_updateCallback(this);

	if (tracking && m_bUndoStarted && CUndo::IsRecording() && !GetIEditor()->GetIUndoManager()->IsUndoSuspended())
	{
		GetIEditor()->GetIUndoManager()->Suspend();
	}

	if (m_hWnd)
	{
		CWnd* parent = GetParent();
		if (parent)
		{
			::SendMessage(parent->GetSafeHwnd(), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), WMU_FS_CHANGED), (LPARAM)GetSafeHwnd());
		}
	}
	m_lastUpdateValue = m_value;
}

//////////////////////////////////////////////////////////////////////////
void CFillSliderCtrl::SetFilledLook(bool bFilled)
{
	m_bFilled = bFilled;
}

