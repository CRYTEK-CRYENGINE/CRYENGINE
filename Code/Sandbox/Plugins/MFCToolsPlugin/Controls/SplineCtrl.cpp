// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SplineCtrl.h"
#include "MemDC.h"
#include "TimelineCtrl.h"

IMPLEMENT_DYNAMIC(CSplineCtrl, CWnd)

#define MIN_TIME_EPSILON 0.01f

//////////////////////////////////////////////////////////////////////////
CSplineCtrl::CSplineCtrl()
{
	m_nActiveKey = -1;
	m_nHitKeyIndex = -1;
	m_nKeyDrawRadius = 3;
	m_bTracking = false;
	m_pSpline = 0;
	m_gridX = 10;
	m_gridY = 10;
	m_fMinTime = -1;
	m_fMaxTime = 1;
	m_fMinValue = -1;
	m_fMaxValue = 1;
	m_fTooltipScaleX = 1;
	m_fTooltipScaleY = 1;
	m_bLockFirstLastKey = false;
	m_pTimelineCtrl = 0;

	m_bSelectedKeys.reserve(0);

	m_fTimeMarker = -10;
}

CSplineCtrl::~CSplineCtrl()
{
}

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CSplineCtrl, CWnd)
ON_WM_SIZE()
ON_WM_ERASEBKGND()
ON_WM_PAINT()
ON_WM_RBUTTONDOWN()
ON_WM_LBUTTONDOWN()
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONUP()
ON_WM_SETCURSOR()
ON_WM_LBUTTONDBLCLK()
ON_WM_KEYDOWN()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSplineCtrl message handlers

/////////////////////////////////////////////////////////////////////////////
BOOL CSplineCtrl::Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID)
{
	LPCTSTR lpClassName = AfxRegisterWndClass(CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, AfxGetApp()->LoadStandardCursor(IDC_ARROW), NULL, NULL);
	return CreateEx(0, lpClassName, "SplineCtrl", dwStyle, rc, pParentWnd, nID);
}

//////////////////////////////////////////////////////////////////////////
BOOL CSplineCtrl::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrl::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);
	GetClientRect(m_rcSpline);

	if (m_pTimelineCtrl)
	{
		CRect rct(m_rcSpline);
		rct.bottom = rct.top + 20;
		m_rcSpline.top = rct.bottom + 1;
		m_pTimelineCtrl->MoveWindow(rct, FALSE);
	}

	m_rcSpline.DeflateRect(2, 2);

	m_offscreenBitmap.DeleteObject();
	if (!m_offscreenBitmap.GetSafeHandle())
	{
		CDC* pDC = GetDC();
		m_offscreenBitmap.CreateCompatibleBitmap(pDC, cx, cy);
		ReleaseDC(pDC);
	}

	if (m_tooltip.m_hWnd)
	{
		m_tooltip.DelTool(this, 1);
		m_tooltip.AddTool(this, "", m_rcSpline, 1);
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CSplineCtrl::PreTranslateMessage(MSG* pMsg)
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
CPoint CSplineCtrl::KeyToPoint(int nKey)
{
	if (nKey >= 0)
	{
		return TimeToPoint(m_pSpline->GetKeyTime(nKey));
	}
	return CPoint(0, 0);
}

//////////////////////////////////////////////////////////////////////////
CPoint CSplineCtrl::TimeToPoint(float time)
{
	CPoint point;
	point.x = (time - m_fMinTime) * (m_rcSpline.Width() / (m_fMaxTime - m_fMinTime)) + m_rcSpline.left;
	float val = 0;
	if (m_pSpline)
		m_pSpline->InterpolateFloat(time, val);
	point.y = floor((m_fMaxValue - val) * (m_rcSpline.Height() / (m_fMaxValue - m_fMinValue)) + 0.5f) + m_rcSpline.top;
	return point;
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrl::PointToTimeValue(CPoint point, float& time, float& value)
{
	time = XOfsToTime(point.x);
	float t = float(m_rcSpline.bottom - point.y) / m_rcSpline.Height();
	value = LERP(m_fMinValue, m_fMaxValue, t);
}

//////////////////////////////////////////////////////////////////////////
float CSplineCtrl::XOfsToTime(int x)
{
	// m_fMinTime to m_fMaxTime time range.
	float t = float(x - m_rcSpline.left) / m_rcSpline.Width();
	return LERP(m_fMinTime, m_fMaxTime, t);
}

//////////////////////////////////////////////////////////////////////////
CPoint CSplineCtrl::XOfsToPoint(int x)
{
	return TimeToPoint(XOfsToTime(x));
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrl::OnPaint()
{
	CPaintDC PaintDC(this);

	CRect rcClient;
	GetClientRect(&rcClient);

	if (m_pSpline)
		m_bSelectedKeys.resize(m_pSpline->GetKeyCount());

	if (!m_offscreenBitmap.GetSafeHandle())
		m_offscreenBitmap.CreateCompatibleBitmap(&PaintDC, rcClient.Width(), rcClient.Height());

	{
		CMemoryDC dc(PaintDC, &m_offscreenBitmap);

		if (m_TimeUpdateRect != CRect(PaintDC.m_ps.rcPaint))
		{
			CBrush bkBrush;
			bkBrush.CreateSolidBrush(RGB(140, 140, 140));
			dc.FillRect(&PaintDC.m_ps.rcPaint, &bkBrush);

			//Draw Grid
			DrawGrid(&dc);

			//Draw Keys and Curve
			if (m_pSpline)
			{
				DrawSpline(&dc);
				DrawKeys(&dc);
			}
		}
		m_TimeUpdateRect.SetRectEmpty();
	}
	DrawTimeMarker(&PaintDC);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrl::DrawGrid(CDC* pDC)
{
	if (GetStyle() & SPLINE_STYLE_NOGRID)
		return;

	int cx = m_rcSpline.Width();
	int cy = m_rcSpline.Height();

	LOGBRUSH logBrush;
	logBrush.lbStyle = BS_SOLID;
	logBrush.lbColor = RGB(90, 90, 90);

	CPen pen;
	pen.CreatePen(PS_COSMETIC | PS_ALTERNATE, 1, &logBrush);
	CPen* pOldPen = pDC->SelectObject(&pen);

	//Draw Vertical Grid Lines
	for (int y = 1; y < m_gridX; y++)
	{
		pDC->MoveTo(m_rcSpline.left + y * cx / m_gridX, m_rcSpline.top + cy);
		pDC->LineTo(m_rcSpline.left + y * cx / m_gridX, m_rcSpline.top);
	}

	//Draw Horizontal Grid Lines
	for (int x = 1; x < m_gridY; x++)
	{
		pDC->MoveTo(m_rcSpline.left, m_rcSpline.top + x * cy / m_gridY);
		pDC->LineTo(m_rcSpline.left + cx, m_rcSpline.top + x * cy / m_gridY);
	}

	CPen pen1;
	pen1.CreatePen(PS_SOLID, 1, RGB(75, 75, 75));
	pDC->SelectObject(pen1);

	pDC->MoveTo(m_rcSpline.left + (m_gridX / 2) * cx / m_gridX, m_rcSpline.top + cy);
	pDC->LineTo(m_rcSpline.left + (m_gridX / 2) * cx / m_gridX, m_rcSpline.top + 0);
	pDC->MoveTo(m_rcSpline.left + 0, m_rcSpline.left + (m_gridY / 2) * cy / m_gridY);
	pDC->LineTo(m_rcSpline.left + cx, m_rcSpline.left + (m_gridY / 2) * cy / m_gridY);

	pDC->MoveTo(m_rcSpline.left, m_rcSpline.top);
	pDC->LineTo(m_rcSpline.right, m_rcSpline.top);
	pDC->LineTo(m_rcSpline.right, m_rcSpline.bottom);
	pDC->LineTo(m_rcSpline.left, m_rcSpline.bottom);
	pDC->LineTo(m_rcSpline.left, m_rcSpline.top);

	pDC->SelectObject(pOldPen);

}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrl::DrawSpline(CDC* pDC)
{
	int cx = m_rcSpline.Width();
	int cy = m_rcSpline.Height();

	//Draw Curve
	// create and select a thick, white pen
	CPen pen;
	pen.CreatePen(PS_SOLID, 1, RGB(128, 255, 128));
	CPen* pOldPen = pDC->SelectObject(&pen);

	CRect rcClip;
	pDC->GetClipBox(rcClip);
	rcClip.IntersectRect(rcClip, m_rcSpline);

	bool bFirst = true;
	for (int x = rcClip.left; x < rcClip.right; x++)
	{
		CPoint pt = XOfsToPoint(x);
		if (!bFirst)
		{
			pDC->LineTo(pt);
		}
		else
		{
			pDC->MoveTo(pt);
			bFirst = false;
		}
	}

	// Put back the old objects
	pDC->SelectObject(pOldPen);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrl::DrawKeys(CDC* pDC)
{
	if (!m_pSpline)
		return;

	// create and select a white pen
	CPen pen;
	pen.CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
	CPen* pOldPen = pDC->SelectObject(&pen);

	m_bSelectedKeys.resize(m_pSpline->GetKeyCount());

	for (int i = 0; i < m_pSpline->GetKeyCount(); i++)
	{
		float time = m_pSpline->GetKeyTime(i);
		CPoint pt = TimeToPoint(time);

		COLORREF clr = RGB(220, 220, 0);
		if (m_bSelectedKeys[i])
			clr = RGB(255, 0, 0);
		CBrush brush(clr);
		CBrush* pOldBrush = pDC->SelectObject(&brush);

		// Draw this key.
		CRect rc;
		rc.left = pt.x - m_nKeyDrawRadius;
		rc.right = pt.x + m_nKeyDrawRadius;
		rc.top = pt.y - m_nKeyDrawRadius;
		rc.bottom = pt.y + m_nKeyDrawRadius;
		//pDC->Ellipse(&rc);
		pDC->Rectangle(rc);

		pDC->SelectObject(pOldBrush);
	}

	pDC->SelectObject(pOldPen);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrl::DrawTimeMarker(CDC* pDC)
{
	CPen timePen;
	timePen.CreatePen(PS_SOLID, 1, RGB(255, 0, 255));
	if (!(GetStyle() & SPLINE_STYLE_NO_TIME_MARKER))
	{
		pDC->SelectObject(&timePen);
		CPoint pt = TimeToPoint(m_fTimeMarker);
		pDC->MoveTo(pt.x, m_rcSpline.top + 1);
		pDC->LineTo(pt.x, m_rcSpline.bottom - 1);
	}
}

void CSplineCtrl::UpdateToolTip()
{
	if (m_nHitKeyIndex >= 0 && m_pSpline)
	{
		float time = m_pSpline->GetKeyTime(m_nHitKeyIndex);
		float val;
		m_pSpline->GetKeyValueFloat(m_nHitKeyIndex, val);
		int cont_s = (m_pSpline->GetKeyFlags(m_nHitKeyIndex) >> SPLINE_KEY_TANGENT_IN_SHIFT) & SPLINE_KEY_TANGENT_LINEAR ? 1 : 2;
		int cont_d = (m_pSpline->GetKeyFlags(m_nHitKeyIndex) >> SPLINE_KEY_TANGENT_OUT_SHIFT) & SPLINE_KEY_TANGENT_LINEAR ? 1 : 2;

		CString tipText;
		tipText.Format("%.3f, %.3f [%d|%d]",
		               time * m_fTooltipScaleX, val * m_fTooltipScaleY, cont_s, cont_d);
		m_tooltip.UpdateTipText(tipText, this, 1);
		m_tooltip.Activate(TRUE);
	}
}

/////////////////////////////////////////////////////////////////////////////
//Mouse Message Handlers
//////////////////////////////////////////////////////////////////////////
void CSplineCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
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

	case HIT_SPLINE:
		// Cycle the spline slope of the nearest key.
		ToggleKeySlope(m_nHitKeyIndex, m_nHitKeyDist);
		SetActiveKey(-1);
		break;

	case HIT_NOTHING:
		SetActiveKey(-1);
		break;
	}
	Invalidate();

	CWnd::OnLButtonDown(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrl::OnRButtonDown(UINT nFlags, CPoint point)
{
	SetFocus();

	CWnd::OnRButtonDown(nFlags, point);
	if (!m_pSpline)
		return;

	NMHDR nmh;
	nmh.hwndFrom = m_hWnd;
	nmh.idFrom = ::GetDlgCtrlID(m_hWnd);
	nmh.code = NM_RCLICK;

	GetOwner()->SendMessage(WM_NOTIFY, (WPARAM)GetDlgCtrlID(), (LPARAM)&nmh);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrl::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	if (!m_pSpline)
		return;

	RECT rc;
	GetClientRect(&rc);

	switch (m_hitCode)
	{
	case HIT_NOTHING:
		{
			int iIndex = InsertKey(point);
			SetActiveKey(iIndex);

			RedrawWindow();
		}
		break;
	case HIT_KEY:
		{
			RemoveKey(m_nHitKeyIndex);
		}
		break;
	}

	CWnd::OnLButtonDblClk(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	if (!m_pSpline)
		return;

	if (m_bTracking)
	{
		TrackKey(point);
		UpdateToolTip();
	}

	CWnd::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (!m_pSpline)
		return;

	if (m_bTracking)
		StopTracking();

	CWnd::OnLButtonUp(nFlags, point);
}

/////////////////////////////////////////////////////////////////////////////
void CSplineCtrl::SetActiveKey(int nIndex)
{
	ClearSelection();

	// Activate New Key
	if (nIndex >= 0)
	{
		m_bSelectedKeys[nIndex] = true;
	}
	m_nActiveKey = nIndex;
	Invalidate();
}

/////////////////////////////////////////////////////////////////////////////
void CSplineCtrl::SetSpline(ISplineInterpolator* pSpline, BOOL bRedraw)
{
	if (pSpline != m_pSpline)
		m_pSpline = pSpline;

	ValidateSpline();
	ClearSelection();

	if (bRedraw)
		RedrawWindow();
}

void CSplineCtrl::ValidateSpline()
{
	// Add initial control points (will be  serialised only if edited).
	if (m_pSpline->GetKeyCount() == 0)
	{
		m_pSpline->InsertKeyFloat(0.f, 1.f);
		m_pSpline->InsertKeyFloat(1.f, 1.f);
		m_pSpline->Update();
	}
}

//////////////////////////////////////////////////////////////////////////
ISplineInterpolator* CSplineCtrl::GetSpline()
{
	return m_pSpline;
}

/////////////////////////////////////////////////////////////////////////////
BOOL CSplineCtrl::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
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
			UpdateToolTip();
		else if (!m_bTracking)
			m_tooltip.Activate(FALSE);
	}

	if (!b)
		return CWnd::OnSetCursor(pWnd, nHitTest, message);
	else return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
void CSplineCtrl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	BOOL bProcessed = false;

	if (m_nActiveKey != -1 && m_pSpline)
	{
		switch (nChar)
		{
		case VK_SPACE:
			{
				ToggleKeySlope(m_nActiveKey, 0);
				bProcessed = true;
			} break;
		case VK_DELETE:
			{
				RemoveKey(m_nActiveKey);
				bProcessed = true;
			} break;
		case VK_UP:
			{
				CUndo undo("Move Spline Key");
				CPoint point = KeyToPoint(m_nActiveKey);
				point.y -= 1;
				SendNotifyEvent(SPLN_BEFORE_CHANGE);
				TrackKey(point);
				bProcessed = true;
			} break;
		case VK_DOWN:
			{
				CUndo undo("Move Spline Key");
				CPoint point = KeyToPoint(m_nActiveKey);
				point.y += 1;
				SendNotifyEvent(SPLN_BEFORE_CHANGE);
				TrackKey(point);
				bProcessed = true;
			} break;
		case VK_LEFT:
			{
				CUndo undo("Move Spline Key");
				CPoint point = KeyToPoint(m_nActiveKey);
				point.x -= 1;
				SendNotifyEvent(SPLN_BEFORE_CHANGE);
				TrackKey(point);
				bProcessed = true;
			} break;
		case VK_RIGHT:
			{
				CUndo undo("Move Spline Key");
				CPoint point = KeyToPoint(m_nActiveKey);
				point.y += 1;
				SendNotifyEvent(SPLN_BEFORE_CHANGE);
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
CSplineCtrl::EHitCode CSplineCtrl::HitTest(CPoint point)
{
	if (!m_pSpline)
		return HIT_NOTHING;

	float time, val;
	PointToTimeValue(point, time, val);

	m_nHitKeyIndex = -1;
	m_nHitKeyDist = 0xFFFF;
	m_hitCode = HIT_NOTHING;

	CPoint splinePt = TimeToPoint(time);
	if (abs(splinePt.y - point.y) < 4)
	{
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

	return m_hitCode;
}

///////////////////////////////////////////////////////////////////////////////
void CSplineCtrl::StartTracking()
{
	m_bTracking = TRUE;
	SetCapture();

	GetIEditor()->GetIUndoManager()->Begin();

	SendNotifyEvent(SPLN_BEFORE_CHANGE);

	HCURSOR hCursor;
	hCursor = AfxGetApp()->LoadCursor(IDC_ARRBLCKCROSS);
	SetCursor(hCursor);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrl::TrackKey(CPoint point)
{
	int nKey = m_nHitKeyIndex;

	if (nKey >= 0)
	{
		float time, val;

		// Editing time & value.
		Limit(point.x, m_rcSpline.left, m_rcSpline.right);
		Limit(point.y, m_rcSpline.top, m_rcSpline.bottom);
		PointToTimeValue(point, time, val);

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

		m_pSpline->SetKeyValueFloat(nKey, val);
		if ((nKey != 0 && nKey != m_pSpline->GetKeyCount() - 1) || !m_bLockFirstLastKey)
		{
			m_pSpline->SetKeyTime(nKey, time);
		}
		else if (m_bLockFirstLastKey)
		{
			int first = 0;
			int last = m_pSpline->GetKeyCount() - 1;
			if (nKey == first)
				m_pSpline->SetKeyValueFloat(last, val);
			else if (nKey == last)
				m_pSpline->SetKeyValueFloat(first, val);
		}

		m_pSpline->Update();
		SendNotifyEvent(SPLN_CHANGE);
		if (m_updateCallback)
			m_updateCallback(this);

		RedrawWindow();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrl::StopTracking()
{
	if (!m_bTracking)
		return;

	GetIEditor()->GetIUndoManager()->Accept("Spline Move");

	m_bTracking = FALSE;
	ReleaseCapture();
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrl::RemoveKey(int nKey)
{
	if (!m_pSpline)
		return;
	if (nKey)
	{
		if (m_bLockFirstLastKey)
		{
			if (nKey == 0 || nKey == m_pSpline->GetKeyCount() - 1)
				return;
		}
	}

	CUndo undo("Remove Spline Key");

	SendNotifyEvent(SPLN_BEFORE_CHANGE);
	m_nActiveKey = -1;
	m_nHitKeyIndex = -1;
	if (m_pSpline)
	{
		m_pSpline->RemoveKey(nKey);
		m_pSpline->Update();
		ValidateSpline();
	}
	SendNotifyEvent(SPLN_CHANGE);
	if (m_updateCallback)
		m_updateCallback(this);
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
int CSplineCtrl::InsertKey(CPoint point)
{
	CUndo undo("Spline Insert Key");

	float time, val;
	PointToTimeValue(point, time, val);

	int i;
	for (i = 0; i < m_pSpline->GetKeyCount(); i++)
	{
		// Skip if any key already have time that is very close.
		if (fabs(m_pSpline->GetKeyTime(i) - time) < MIN_TIME_EPSILON)
			return i;
	}

	SendNotifyEvent(SPLN_BEFORE_CHANGE);

	m_pSpline->InsertKeyFloat(time, val);
	m_pSpline->Update();
	ClearSelection();
	Invalidate();

	SendNotifyEvent(SPLN_CHANGE);
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

void CSplineCtrl::ToggleKeySlope(int nIndex, int nDir)
{
	if (nIndex >= 0)
	{
		int flags = m_pSpline->GetKeyFlags(nIndex);
		if (nDir <= 0)
			// Toggle left side.
			flags ^= SPLINE_KEY_TANGENT_LINEAR << SPLINE_KEY_TANGENT_IN_SHIFT;
		if (nDir >= 0)
			// Toggle right side.
			flags ^= SPLINE_KEY_TANGENT_LINEAR << SPLINE_KEY_TANGENT_OUT_SHIFT;
		m_pSpline->SetKeyFlags(nIndex, flags);
		m_pSpline->Update();
		SendNotifyEvent(SPLN_CHANGE);
		if (m_updateCallback)
			m_updateCallback(this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrl::ClearSelection()
{
	m_nActiveKey = -1;
	if (m_pSpline)
		m_bSelectedKeys.resize(m_pSpline->GetKeyCount());
	for (int i = 0; i < (int)m_bSelectedKeys.size(); i++)
		m_bSelectedKeys[i] = false;
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrl::SetTimeMarker(float fTime)
{
	if (!m_pSpline)
		return;

	if (fTime == m_fTimeMarker)
		return;

	// Erase old first.
	CPoint pt0 = TimeToPoint(m_fTimeMarker);
	CPoint pt1 = TimeToPoint(fTime);
	CRect rc = CRect(pt0.x, m_rcSpline.top, pt1.x, m_rcSpline.bottom);
	rc.NormalizeRect();
	rc.InflateRect(5, 0);
	rc.IntersectRect(rc, m_rcSpline);

	m_TimeUpdateRect = rc;
	InvalidateRect(rc);

	m_fTimeMarker = fTime;
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrl::SendNotifyEvent(int nEvent)
{
	NMHDR nmh;
	nmh.hwndFrom = m_hWnd;
	nmh.idFrom = ::GetDlgCtrlID(m_hWnd);
	nmh.code = nEvent;

	GetOwner()->SendMessage(WM_NOTIFY, (WPARAM)GetDlgCtrlID(), (LPARAM)&nmh);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrl::SetTimelineCtrl(CTimelineCtrl* pTimelineCtrl)
{
	m_pTimelineCtrl = pTimelineCtrl;
	if (m_pTimelineCtrl)
		m_pTimelineCtrl->SetParent(this);
}

