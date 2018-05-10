// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ColorGradientCtrl.h"
#include "MemDC.h"
#include "Dialogs/CustomColorDialog.h"
#include "WndGridHelper.h"
#include "Util/MFCUtil.h"

IMPLEMENT_DYNAMIC(CColorGradientCtrl, CWnd)

#define MIN_TIME_EPSILON 0.01f

//////////////////////////////////////////////////////////////////////////
CColorGradientCtrl::CColorGradientCtrl()
{
	m_bAutoDelete = false;
	m_nActiveKey = -1;
	m_nHitKeyIndex = -1;
	m_nKeyDrawRadius = 3;
	m_bTracking = false;
	m_pSpline = 0;
	m_fMinTime = -1;
	m_fMaxTime = 1;
	m_fMinValue = -1;
	m_fMaxValue = 1;
	m_fTooltipScaleX = 1;
	m_fTooltipScaleY = 1;
	m_bLockFirstLastKey = false;
	m_bNoZoom = true;

	ClearSelection();

	m_bSelectedKeys.reserve(0);

	m_fTimeMarker = -10;

	m_grid.zoom.x = 100;
}

CColorGradientCtrl::~CColorGradientCtrl()
{
}

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CColorGradientCtrl, CWnd)
ON_WM_SIZE()
ON_WM_ERASEBKGND()
ON_WM_PAINT()
ON_WM_LBUTTONDOWN()
ON_WM_RBUTTONDOWN()
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONUP()
ON_WM_RBUTTONUP()
ON_WM_SETCURSOR()
ON_WM_LBUTTONDBLCLK()
ON_WM_RBUTTONDOWN()
ON_WM_KEYDOWN()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CColorGradientCtrl message handlers

/////////////////////////////////////////////////////////////////////////////
BOOL CColorGradientCtrl::Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID)
{
	if (dwStyle & CLRGRD_STYLE_AUTO_DELETE)
		m_bAutoDelete = true;
	LPCTSTR lpClassName = AfxRegisterWndClass(CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, AfxGetApp()->LoadStandardCursor(IDC_ARROW), NULL, NULL);
	return CreateEx(0, lpClassName, "SplineCtrl", dwStyle, rc, pParentWnd, nID);
}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::PostNcDestroy()
{
	if (m_bAutoDelete)
		delete this;
}

//////////////////////////////////////////////////////////////////////////
BOOL CColorGradientCtrl::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	m_offscreenBitmap.DeleteObject();
	if (!m_offscreenBitmap.GetSafeHandle())
	{
		CDC* pDC = GetDC();
		m_offscreenBitmap.CreateCompatibleBitmap(pDC, cx, cy);
		ReleaseDC(pDC);
	}

	CRect rc;
	GetClientRect(rc);
	m_rcGradient = rc;
	m_rcGradient.bottom -= 11;
	//m_rcGradient.DeflateRect(4,4);

	m_grid.rect = m_rcGradient;
	if (m_bNoZoom)
		m_grid.zoom.x = m_grid.rect.Width();

	m_rcKeys = rc;
	m_rcKeys.top = m_rcKeys.bottom - 10;

	if (m_tooltip.m_hWnd)
	{
		m_tooltip.DelTool(this, 1);
		m_tooltip.AddTool(this, "", m_rcGradient, 1);
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CColorGradientCtrl::PreTranslateMessage(MSG* pMsg)
{
	if (!m_tooltip.m_hWnd)
	{
		CRect rc;
		GetClientRect(rc);
		m_tooltip.Create(this);
		m_tooltip.SetDelayTime(TTDT_AUTOPOP, 500);
		m_tooltip.SetDelayTime(TTDT_INITIAL, 0);
		m_tooltip.SetDelayTime(TTDT_RESHOW, 0);
		m_tooltip.SetMaxTipWidth(600);
		m_tooltip.AddTool(this, "", rc, 1);
		m_tooltip.Activate(FALSE);
	}
	m_tooltip.RelayEvent(pMsg);

	return __super::PreTranslateMessage(pMsg);
}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::SetZoom(float fZoom)
{
	m_grid.zoom.x = fZoom;
}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::SetOrigin(float fOffset)
{
	m_grid.origin.x = fOffset;
}

//////////////////////////////////////////////////////////////////////////
CPoint CColorGradientCtrl::KeyToPoint(int nKey)
{
	if (nKey >= 0)
	{
		return TimeToPoint(m_pSpline->GetKeyTime(nKey));
	}
	return CPoint(0, 0);
}

//////////////////////////////////////////////////////////////////////////
CPoint CColorGradientCtrl::TimeToPoint(float time)
{
	return CPoint(m_grid.WorldToClient(Vec2(time, 0)).x, m_rcGradient.Height() / 2);

	CPoint point;
	point.x = (time - m_fMinTime) * (m_rcGradient.Width() / (m_fMaxTime - m_fMinTime)) + m_rcGradient.left;
	point.y = m_rcGradient.Height() / 2;
	return point;
}

//////////////////////////////////////////////////////////////////////////
COLORREF CColorGradientCtrl::TimeToColor(float time)
{
	ISplineInterpolator::ValueType val;
	m_pSpline->Interpolate(time, val);
	COLORREF col = ValueToColor(val);
	return col;
}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::PointToTimeValue(CPoint point, float& time, ISplineInterpolator::ValueType& val)
{
	time = XOfsToTime(point.x);
	ColorToValue(TimeToColor(time), val);
}

//////////////////////////////////////////////////////////////////////////
float CColorGradientCtrl::XOfsToTime(int x)
{
	return m_grid.ClientToWorld(CPoint(x, 0)).x;

	// m_fMinTime to m_fMaxTime time range.
	float time = m_fMinTime + (float)((m_fMaxTime - m_fMinTime) * (x - m_rcGradient.left)) / m_rcGradient.Width();
	return time;
}

//////////////////////////////////////////////////////////////////////////
CPoint CColorGradientCtrl::XOfsToPoint(int x)
{
	return TimeToPoint(XOfsToTime(x));
}

//////////////////////////////////////////////////////////////////////////
COLORREF CColorGradientCtrl::XOfsToColor(int x)
{
	return TimeToColor(XOfsToTime(x));
}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::OnPaint()
{
	CPaintDC PaintDC(this);

	CRect rcClient;
	GetClientRect(&rcClient);

	if (m_pSpline)
		m_bSelectedKeys.resize(m_pSpline->GetKeyCount());
	{
		if (!m_offscreenBitmap.GetSafeHandle())
			m_offscreenBitmap.CreateCompatibleBitmap(&PaintDC, rcClient.Width(), rcClient.Height());

		CMemoryDC dc(PaintDC, &m_offscreenBitmap);

		if (!IsWindowEnabled())
		{
			CRect rc;
			GetClientRect(rc);
			rc.IntersectRect(rc, CRect(PaintDC.m_ps.rcPaint));
			CBrush keysBrush;
			keysBrush.CreateSolidBrush(GetXtremeColor(COLOR_BTNFACE));
			dc.FillRect(rc, &keysBrush);
			return;
		}

		//////////////////////////////////////////////////////////////////////////
		// Fill keys backgound.
		//////////////////////////////////////////////////////////////////////////
		CRect rcKeys;
		rcKeys.IntersectRect(m_rcKeys, CRect(PaintDC.m_ps.rcPaint));
		CBrush keysBrush;
		keysBrush.CreateSolidBrush(GetXtremeColor(COLOR_BTNFACE));
		dc.FillRect(rcKeys, &keysBrush);
		//////////////////////////////////////////////////////////////////////////

		//Draw Keys and Curve
		if (m_pSpline)
		{
			DrawGradient(&dc);
			DrawKeys(&dc);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::DrawGradient(CDC* pDC)
{
	int cx = m_rcGradient.Width();
	int cy = m_rcGradient.Height();

	//Draw Curve
	// create and select a thick, white pen
	CPen pen;
	pen.CreatePen(PS_SOLID, 1, RGB(128, 255, 128));
	CPen* pOldPen = pDC->SelectObject(&pen);

	CRect rcClip;
	pDC->GetClipBox(rcClip);
	rcClip.IntersectRect(rcClip, m_rcGradient);

	for (int x = rcClip.left; x < rcClip.right; x++)
	{
		COLORREF col = XOfsToColor(x);
		CPen pen;
		pen.CreatePen(PS_SOLID, 1, col);
		pDC->SelectObject(&pen);
		pDC->MoveTo(CPoint(x, m_rcGradient.top));
		pDC->LineTo(CPoint(x, m_rcGradient.bottom));
	}

	// Put back the old objects
	pDC->SelectObject(pOldPen);
}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::DrawKeys(CDC* pDC)
{
	if (!m_pSpline)
		return;

	// create and select a white pen
	CPen pen;
	pen.CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
	CPen* pOldPen = pDC->SelectObject(&pen);

	CRect rcClip;
	pDC->GetClipBox(rcClip);

	m_bSelectedKeys.resize(m_pSpline->GetKeyCount());

	for (int i = 0; i < m_pSpline->GetKeyCount(); i++)
	{
		float time = m_pSpline->GetKeyTime(i);
		CPoint pt = TimeToPoint(time);

		if (pt.x < rcClip.left - 8 || pt.x > rcClip.right + 8)
			continue;

		COLORREF clr = TimeToColor(time);
		CBrush brush(clr);
		CBrush* pOldBrush = pDC->SelectObject(&brush);

		// Find the midpoints of the top, right, left, and bottom
		// of the client area. They will be the vertices of our polygon.
		CPoint pts[3];
		pts[0].x = pt.x;
		pts[0].y = m_rcKeys.top + 1;
		pts[1].x = pt.x - 5;
		pts[1].y = m_rcKeys.top + 8;
		pts[2].x = pt.x + 5;
		pts[2].y = m_rcKeys.top + 8;
		pDC->Polygon(pts, 3);

		if (m_bSelectedKeys[i])
		{
			CPen pen;
			pen.CreatePen(PS_SOLID, 1, RGB(200, 0, 0));
			CPen* pOldPen = pDC->SelectObject(&pen);
			pDC->Polygon(pts, 3);
			pDC->SelectObject(pOldPen);
		}

		pDC->SelectObject(pOldBrush);
	}

	CPen timePen;
	timePen.CreatePen(PS_SOLID, 1, RGB(255, 0, 255));
	if (!(GetStyle() & CLRGRD_STYLE_NO_TIME_MARKER))
	{
		pDC->SelectObject(&timePen);
		CPoint pt = TimeToPoint(m_fTimeMarker);
		pDC->MoveTo(pt.x, m_rcGradient.top + 1);
		pDC->LineTo(pt.x, m_rcGradient.bottom - 1);
	}

	pDC->SelectObject(pOldPen);
}

void CColorGradientCtrl::UpdateTooltip()
{
	if (m_nHitKeyIndex >= 0)
	{
		float time = m_pSpline->GetKeyTime(m_nHitKeyIndex);
		ISplineInterpolator::ValueType val;
		m_pSpline->GetKeyValue(m_nHitKeyIndex, val);

		COLORREF col = TimeToColor(time);
		int cont_s = (m_pSpline->GetKeyFlags(m_nHitKeyIndex) >> SPLINE_KEY_TANGENT_IN_SHIFT) & SPLINE_KEY_TANGENT_LINEAR ? 1 : 2;
		int cont_d = (m_pSpline->GetKeyFlags(m_nHitKeyIndex) >> SPLINE_KEY_TANGENT_OUT_SHIFT) & SPLINE_KEY_TANGENT_LINEAR ? 1 : 2;

		CString tipText;
		tipText.Format("%.2f : %d,%d,%d [%d,%d]",
		               time * m_fTooltipScaleX, GetRValue(col), GetGValue(col), GetBValue(col), cont_s, cont_d);
		m_tooltip.UpdateTipText(tipText, this, 1);
		m_tooltip.Activate(TRUE);
	}
}

/////////////////////////////////////////////////////////////////////////////
//Mouse Message Handlers
//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	CWnd::OnLButtonDown(nFlags, point);

	if (m_bTracking)
		return;
	if (!m_pSpline)
		return;

	SetFocus();

	switch (m_hitCode)
	{
	case HIT_KEY:
		StartTracking();
		SetActiveKey(m_nHitKeyIndex);
		break;

	/*
	   case HIT_SPLINE:
	   {
	   // Cycle the spline slope of the nearest key.
	   int flags = m_pSpline->GetKeyFlags(m_nHitKeyIndex);
	   if (m_nHitKeyDist < 0)
	    // Toggle left side.
	    flags ^= SPLINE_KEY_TANGENT_LINEAR << SPLINE_KEY_TANGENT_IN_SHIFT;
	   if (m_nHitKeyDist > 0)
	    // Toggle right side.
	    flags ^= SPLINE_KEY_TANGENT_LINEAR << SPLINE_KEY_TANGENT_OUT_SHIFT;
	   m_pSpline->SetKeyFlags(m_nHitKeyIndex, flags);
	   m_pSpline->Update();

	   SetActiveKey(-1);
	   SendNotifyEvent( CLRGRDN_CHANGE );
	   if (m_updateCallback)
	    m_updateCallback(this);
	   break;
	   }
	 */

	case HIT_NOTHING:
		SetActiveKey(-1);
		break;
	}
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::OnRButtonDown(UINT nFlags, CPoint point)
{
	CWnd::OnRButtonDown(nFlags, point);

	NMHDR nmh;
	nmh.hwndFrom = m_hWnd;
	nmh.idFrom = ::GetDlgCtrlID(m_hWnd);
	nmh.code = NM_RCLICK;

	GetOwner()->SendMessage(WM_NOTIFY, (WPARAM)GetDlgCtrlID(), (LPARAM)&nmh);
}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	if (!m_pSpline)
		return;

	RECT rc;
	GetClientRect(&rc);

	switch (m_hitCode)
	{
	case HIT_SPLINE:
		{
			int iIndex = InsertKey(point);
			SetActiveKey(iIndex);
			EditKey(iIndex);

			RedrawWindow();
		}
		break;
	case HIT_KEY:
		{
			EditKey(m_nHitKeyIndex);
		}
		break;
	}

	CWnd::OnLButtonDblClk(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	if (!m_pSpline)
		return;

	if (GetCapture() != this)
		StopTracking();

	if (m_bTracking)
		TrackKey(point);

	if (m_bTracking)
		UpdateTooltip();

	CWnd::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (!m_pSpline)
		return;

	if (m_bTracking)
		StopTracking();

	CWnd::OnLButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::OnRButtonUp(UINT nFlags, CPoint point)
{
	if (!m_pSpline)
		return;
	CWnd::OnRButtonUp(nFlags, point);
}

/////////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::SetActiveKey(int nIndex)
{
	ClearSelection();

	//Activate New Key
	if (nIndex >= 0)
	{
		m_bSelectedKeys[nIndex] = true;
	}
	m_nActiveKey = nIndex;
	Invalidate();
}

/////////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::SetSpline(ISplineInterpolator* pSpline, BOOL bRedraw)
{
	if (pSpline != m_pSpline)
	{
		//if (pSpline && pSpline->GetNumDimensions() != 3)
		//return;
		m_pSpline = pSpline;
		m_nActiveKey = -1;
	}

	ClearSelection();

	if (bRedraw)
		RedrawWindow();
}

//////////////////////////////////////////////////////////////////////////
ISplineInterpolator* CColorGradientCtrl::GetSpline()
{
	return m_pSpline;
}

/////////////////////////////////////////////////////////////////////////////
BOOL CColorGradientCtrl::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	BOOL b = FALSE;

	CPoint point;
	GetCursorPos(&point);
	ScreenToClient(&point);

	switch (HitTest(point))
	{
	case HIT_SPLINE:
		{
			HCURSOR hCursor;
			hCursor = AfxGetApp()->LoadCursor(IDC_ARRWHITE);
			SetCursor(hCursor);
			b = TRUE;
		} break;
	case HIT_KEY:
		{
			HCURSOR hCursor;
			hCursor = AfxGetApp()->LoadCursor(IDC_ARRBLCK);
			SetCursor(hCursor);
			b = TRUE;
		} break;
	default:
		break;
	}

	if (m_tooltip.m_hWnd && m_pSpline)
	{
		if (m_nHitKeyIndex >= 0)
		{
			UpdateTooltip();
		}
		else if (!m_bTracking)
		{
			m_tooltip.Activate(FALSE);
		}
	}

	if (!b)
		return CWnd::OnSetCursor(pWnd, nHitTest, message);
	else return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	BOOL bProcessed = false;

	if (m_nActiveKey != -1 && m_pSpline)
	{
		switch (nChar)
		{
		case VK_DELETE:
			{
				RemoveKey(m_nActiveKey);
				bProcessed = true;
			} break;
		case VK_UP:
			{
				CUndo undo("Move Spline Key");
				CPoint point = KeyToPoint(m_nActiveKey);
				point.x -= 1;
				SendNotifyEvent(CLRGRDN_BEFORE_CHANGE);
				TrackKey(point);
				bProcessed = true;
			} break;
		case VK_DOWN:
			{
				CUndo undo("Move Spline Key");
				CPoint point = KeyToPoint(m_nActiveKey);
				point.x += 1;
				SendNotifyEvent(CLRGRDN_BEFORE_CHANGE);
				TrackKey(point);
				bProcessed = true;
			} break;
		case VK_LEFT:
			{
				CUndo undo("Move Spline Key");
				CPoint point = KeyToPoint(m_nActiveKey);
				point.x -= 1;
				SendNotifyEvent(CLRGRDN_BEFORE_CHANGE);
				TrackKey(point);
				bProcessed = true;
			} break;
		case VK_RIGHT:
			{
				CUndo undo("Move Spline Key");
				CPoint point = KeyToPoint(m_nActiveKey);
				point.y += 1;
				SendNotifyEvent(CLRGRDN_BEFORE_CHANGE);
				TrackKey(point);
				bProcessed = true;
			} break;

		default:
			break; //do nothing
		}

		RedrawWindow();
	}

	if (!bProcessed)
		CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

//////////////////////////////////////////////////////////////////////////////
CColorGradientCtrl::EHitCode CColorGradientCtrl::HitTest(CPoint point)
{
	if (!m_pSpline)
		return HIT_NOTHING;

	ISplineInterpolator::ValueType val;
	float time;
	PointToTimeValue(point, time, val);

	CPoint splinePt = TimeToPoint(time);

	//bool bSplineHit = abs(splinePt.x-point.x) < 4 && abs(splinePt.y-point.y) < 4;

	CRect rc;
	GetClientRect(rc);

	m_nHitKeyIndex = -1;

	if (rc.PtInRect(point))
	{
		m_nHitKeyDist = 0xFFFF;
		m_hitCode = HIT_SPLINE;

		for (int i = 0; i < m_pSpline->GetKeyCount(); i++)
		{
			CPoint splinePt = TimeToPoint(m_pSpline->GetKeyTime(i));
			if (abs(point.x - splinePt.x) < abs(m_nHitKeyDist))
			{
				m_nHitKeyIndex = i;
				m_nHitKeyDist = point.x - splinePt.x;
			}
		}
		if (abs(m_nHitKeyDist) < 4)
			m_hitCode = HIT_KEY;
	}
	else
		m_hitCode = HIT_NOTHING;

	return m_hitCode;
}

///////////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::StartTracking()
{
	m_bTracking = TRUE;
	SetCapture();

	GetIEditor()->GetIUndoManager()->Begin();
	SendNotifyEvent(CLRGRDN_BEFORE_CHANGE);

	HCURSOR hCursor;
	hCursor = AfxGetApp()->LoadCursor(IDC_ARRBLCKCROSS);
	SetCursor(hCursor);

}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::TrackKey(CPoint point)
{
	if (point.x < m_rcGradient.left || point.y > m_rcGradient.right)
		return;

	int nKey = m_nHitKeyIndex;

	if (nKey >= 0)
	{
		ISplineInterpolator::ValueType val;
		float time;
		PointToTimeValue(point, time, val);

		// Clamp to min/max time.
		if (time < m_fMinTime || time > m_fMaxTime)
			return;

		int i;
		for (i = 0; i < m_pSpline->GetKeyCount(); i++)
		{
			// Switch to next key.
			if (fabs(m_pSpline->GetKeyTime(i) - time) < MIN_TIME_EPSILON)
			{
				if (i != nKey)
					return;
			}
		}

		if (!m_bLockFirstLastKey || (nKey != 0 && nKey != m_pSpline->GetKeyCount() - 1))
		{
			m_pSpline->SetKeyTime(nKey, time);
			m_pSpline->Update();
		}

		SendNotifyEvent(CLRGRDN_CHANGE);
		if (m_updateCallback)
			m_updateCallback(this);

		RedrawWindow();
	}
}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::StopTracking()
{
	if (!m_bTracking)
		return;

	GetIEditor()->GetIUndoManager()->Accept("Spline Move");

	if (m_nHitKeyIndex >= 0)
	{
		CPoint point;
		GetCursorPos(&point);
		ScreenToClient(&point);

		CRect rc;
		GetClientRect(rc);
		rc.InflateRect(100, 100);
		if (!rc.PtInRect(point))
		{
			RemoveKey(m_nHitKeyIndex);
		}
	}

	m_bTracking = FALSE;
	ReleaseCapture();
}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::EditKey(int nKey)
{
	if (!m_pSpline)
		return;

	if (nKey < 0 || nKey >= m_pSpline->GetKeyCount())
		return;

	SetActiveKey(nKey);

	ISplineInterpolator::ValueType val;
	m_pSpline->GetKeyValue(nKey, val);

	SendNotifyEvent(CLRGRDN_BEFORE_CHANGE);

	CCustomColorDialog dlg(ValueToColor(val));
	dlg.SetColorChangeCallback(functor(*this, &CColorGradientCtrl::OnKeyColorChanged));
	if (dlg.DoModal() == IDOK)
	{
		CUndo undo("Modify Gradient Color");
		OnKeyColorChanged(dlg.GetColor());
	}
	else
	{
		OnKeyColorChanged(ValueToColor(val));
	}
}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::OnKeyColorChanged(COLORREF color)
{
	int nKey = m_nActiveKey;
	if (!m_pSpline)
		return;
	if (nKey < 0 || nKey >= m_pSpline->GetKeyCount())
		return;

	ISplineInterpolator::ValueType val;
	ColorToValue(color, val);
	m_pSpline->SetKeyValue(nKey, val);
	Invalidate();

	if (m_bLockFirstLastKey)
	{
		if (nKey == 0)
			m_pSpline->SetKeyValue(m_pSpline->GetKeyCount() - 1, val);
		else if (nKey == m_pSpline->GetKeyCount() - 1)
			m_pSpline->SetKeyValue(0, val);
	}
	m_pSpline->Update();
	SendNotifyEvent(CLRGRDN_CHANGE);
	if (m_updateCallback)
		m_updateCallback(this);

	GetIEditor()->UpdateViews(eRedrawViewports);
}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::RemoveKey(int nKey)
{
	if (!m_pSpline)
		return;
	if (m_bLockFirstLastKey)
	{
		if (nKey == 0 || nKey == m_pSpline->GetKeyCount() - 1)
			return;
	}

	CUndo undo("Remove Spline Key");

	SendNotifyEvent(CLRGRDN_BEFORE_CHANGE);
	m_nActiveKey = -1;
	m_nHitKeyIndex = -1;
	if (m_pSpline)
	{
		m_pSpline->RemoveKey(nKey);
		m_pSpline->Update();
	}
	SendNotifyEvent(CLRGRDN_CHANGE);
	if (m_updateCallback)
		m_updateCallback(this);

	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
int CColorGradientCtrl::InsertKey(CPoint point)
{
	CUndo undo("Spline Insert Key");

	ISplineInterpolator::ValueType val;

	float time;
	PointToTimeValue(point, time, val);

	if (time < m_fMinTime || time > m_fMaxTime)
		return -1;

	int i;
	for (i = 0; i < m_pSpline->GetKeyCount(); i++)
	{
		// Skip if any key already have time that is very close.
		if (fabs(m_pSpline->GetKeyTime(i) - time) < MIN_TIME_EPSILON)
			return i;
	}

	SendNotifyEvent(CLRGRDN_BEFORE_CHANGE);

	m_pSpline->InsertKey(time, val);
	m_pSpline->Interpolate(time, val);
	ClearSelection();
	Invalidate();

	SendNotifyEvent(CLRGRDN_CHANGE);
	if (m_updateCallback)
		m_updateCallback(this);

	for (i = 0; i < m_pSpline->GetKeyCount(); i++)
	{
		// Find key with added time.
		if (m_pSpline->GetKeyTime(i) == time)
			return i;
	}

	return -1;
}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::ClearSelection()
{
	m_nActiveKey = -1;
	if (m_pSpline)
		m_bSelectedKeys.resize(m_pSpline->GetKeyCount());
	for (int i = 0; i < (int)m_bSelectedKeys.size(); i++)
		m_bSelectedKeys[i] = false;
}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::SetTimeMarker(float fTime)
{
	if (!m_pSpline)
		return;

	{
		CPoint pt = TimeToPoint(m_fTimeMarker);
		CRect rc = CRect(pt.x, m_rcGradient.top, pt.x, m_rcGradient.bottom);
		rc.NormalizeRect();
		rc.InflateRect(1, 0);
		InvalidateRect(rc);
	}
	{
		CPoint pt = TimeToPoint(fTime);
		CRect rc = CRect(pt.x, m_rcGradient.top, pt.x, m_rcGradient.bottom);
		rc.NormalizeRect();
		rc.InflateRect(1, 0);
		InvalidateRect(rc);
	}
	m_fTimeMarker = fTime;
}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::SendNotifyEvent(int nEvent)
{
	NMHDR nmh;
	nmh.hwndFrom = m_hWnd;
	nmh.idFrom = ::GetDlgCtrlID(m_hWnd);
	nmh.code = nEvent;

	GetOwner()->SendMessage(WM_NOTIFY, (WPARAM)GetDlgCtrlID(), (LPARAM)&nmh);
}

//////////////////////////////////////////////////////////////////////////
COLORREF CColorGradientCtrl::ValueToColor(ISplineInterpolator::ValueType val)
{
	return CMFCUtils::ColorLinearToGamma(ColorF(val[0], val[1], val[2]));
}

//////////////////////////////////////////////////////////////////////////
void CColorGradientCtrl::ColorToValue(COLORREF col, ISplineInterpolator::ValueType& val)
{
	ColorF colLin = CMFCUtils::ColorGammaToLinear(col);
	val[0] = colLin.r;
	val[1] = colLin.g;
	val[2] = colLin.b;
	val[3] = 0;
}

