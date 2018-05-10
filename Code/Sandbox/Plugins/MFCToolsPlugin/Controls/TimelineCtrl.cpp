// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TimelineCtrl.h"
#include "MemDC.h"
#include <CrySandbox/ScopedVariableSetter.h>
#include <CryAnimation/IFacialAnimation.h> // TODO: Remove!
#include "Util/GridUtils.h"
#include "Controls/SharedFonts.h"
#include <Cry3DEngine/I3dEngine.h>

IMPLEMENT_DYNAMIC(CTimelineCtrl, CWnd)

COLORREF InterpolateColorRef(COLORREF c1, COLORREF c2, float fraction)
{
	DWORD r = (GetRValue(c2) - GetRValue(c1)) * fraction + GetRValue(c1);
	DWORD g = (GetGValue(c2) - GetGValue(c1)) * fraction + GetGValue(c1);
	DWORD b = (GetBValue(c2) - GetBValue(c1)) * fraction + GetBValue(c1);
	return RGB(r, g, b);
}

//////////////////////////////////////////////////////////////////////////
CTimelineCtrl::CTimelineCtrl()
{
	m_bAutoDelete = false;

	m_timeRange.start = 0;
	m_timeRange.end = 1;

	m_timeScale = 1;
	m_fTicksTextScale = 1.0f;

	m_fTimeMarker = -10;

	m_nTicksStep = 100;

	m_trackingMode = TRACKING_MODE_NONE;

	m_leftOffset = 0;
	m_scrollOffset = 0;

	m_ticksStep = 10.0f;

	m_grid.zoom.x = 100;

	m_bIgnoreSetTime = false;

	m_pKeyTimeSet = 0;

	m_markerStyle = MARKER_STYLE_SECONDS;
	m_fps = 30.0f;

	m_imgMarker.Create(MAKEINTRESOURCE(IDB_MARKER), 8, 0, RGB(255, 0, 255));

	m_copyKeyTimes = false;
	m_bTrackingSnapToFrames = false;
}

CTimelineCtrl::~CTimelineCtrl()
{
}

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CTimelineCtrl, CWnd)
ON_WM_SIZE()
ON_WM_ERASEBKGND()
ON_WM_PAINT()
ON_WM_LBUTTONDOWN()
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONUP()
ON_WM_SETCURSOR()
ON_WM_RBUTTONDOWN()
ON_WM_RBUTTONUP()
ON_WM_KEYDOWN()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTimelineCtrl message handlers

/////////////////////////////////////////////////////////////////////////////
BOOL CTimelineCtrl::Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID)
{
	if (dwStyle & TL_STYLE_AUTO_DELETE)
		m_bAutoDelete = true;
	LPCTSTR lpClassName = AfxRegisterWndClass(CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, AfxGetApp()->LoadStandardCursor(IDC_ARROW), NULL, NULL);
	return CreateEx(0, lpClassName, "TimelineCtrl", dwStyle, rc, pParentWnd, nID);
}

//////////////////////////////////////////////////////////////////////////
void CTimelineCtrl::PostNcDestroy()
{
	if (m_bAutoDelete)
		delete this;
}

//////////////////////////////////////////////////////////////////////////
BOOL CTimelineCtrl::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CTimelineCtrl::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	GetClientRect(m_rcClient);
	m_rcTimeline = m_rcClient;
	m_grid.rect = m_rcTimeline;

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
		m_tooltip.AddTool(this, "", m_rcClient, 1);
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CTimelineCtrl::PreTranslateMessage(MSG* pMsg)
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
int CTimelineCtrl::TimeToClient(float time)
{
	return m_grid.WorldToClient(Vec2(time, 0)).x;
}

//////////////////////////////////////////////////////////////////////////
float CTimelineCtrl::ClientToTime(int x)
{
	return m_grid.ClientToWorld(CPoint(x, 0)).x;
}

//////////////////////////////////////////////////////////////////////////
void CTimelineCtrl::OnPaint()
{
	CPaintDC PaintDC(this);

	CRect rcClient;
	GetClientRect(&rcClient);

	{
		if (!m_offscreenBitmap.GetSafeHandle())
			m_offscreenBitmap.CreateCompatibleBitmap(&PaintDC, rcClient.Width(), rcClient.Height());

		CMemoryDC dc(PaintDC, &m_offscreenBitmap);
		//CDC &dc = PaintDC;

		//////////////////////////////////////////////////////////////////////////
		// Fill keys background.
		//////////////////////////////////////////////////////////////////////////
		CRect rc;
		rc.IntersectRect(rcClient, CRect(PaintDC.m_ps.rcPaint));
		CBrush keysBrush;
		keysBrush.CreateSolidBrush(GetXtremeColor(COLOR_BTNFACE));
		dc.FillRect(rc, &keysBrush);
		dc.Draw3dRect(rcClient, GetSysColor(COLOR_3DHILIGHT), GetSysColor(COLOR_3DDKSHADOW));
		//////////////////////////////////////////////////////////////////////////

		m_grid.CalculateGridLines();

		if (!(GetStyle() & TL_STYLE_NO_TICKS))
			DrawTicks(dc);

	}
}

//////////////////////////////////////////////////////////////////////////
float CTimelineCtrl::SnapTime(float time)
{
	double t = floor((double)time * m_ticksStep + 0.5);
	t = t / m_ticksStep;
	return t;
}

//////////////////////////////////////////////////////////////////////////
static const COLORREF timeMarkerCol = RGB(255, 0, 255);
static const COLORREF textCol = RGB(0, 0, 0);
static const COLORREF ltgrayCol = RGB(110, 110, 110);
void CTimelineCtrl::DrawTicks(CDC& dc)
{
	CRect rc;
	dc.GetClipBox(rc);
	if (rc.IntersectRect(rc, m_rcTimeline) == FALSE)
		return;

	CPen* pOldPen = dc.GetCurrentPen();

	CPen ltgray(PS_SOLID, 1, ltgrayCol);
	CPen black(PS_SOLID, 1, textCol);
	CPen redpen(PS_SOLID, 1, timeMarkerCol);

	// Draw time ticks every tick step seconds.
	Range timeRange = m_timeRange;
	CString str;

	dc.SetTextColor(textCol);
	dc.SetBkMode(TRANSPARENT);
	dc.SelectObject(gMFCFonts.hSystemFont);

	dc.SelectObject(ltgray);

	switch (m_markerStyle)
	{
	case MARKER_STYLE_SECONDS:
		DrawSecondTicks(dc);
		break;

	case MARKER_STYLE_FRAMES:
		DrawFrameTicks(dc);
		break;
	}

	dc.SelectObject(&redpen);
	int x = TimeToClient(m_fTimeMarker);
	dc.SelectObject(GetStockObject(NULL_BRUSH));
	dc.Rectangle(x - 3, rc.top, x + 4, rc.bottom);

	dc.SelectObject(redpen);
	dc.MoveTo(x, rc.top);
	dc.LineTo(x, rc.bottom);
	dc.SelectObject(GetStockObject(NULL_BRUSH));

	// Draw vertical line showing current time.
	{
		int x = TimeToClient(m_fTimeMarker);
		if (x > m_rcTimeline.left && x < m_rcTimeline.right)
		{
			CPen pen(PS_SOLID, 1, timeMarkerCol);
			dc.SelectObject(&pen);
			dc.MoveTo(x, 0);
			dc.LineTo(x, m_rcTimeline.bottom);
		}
	}

	// Draw the key times.
	dc.SelectObject(&redpen);
	dc.SelectObject(GetStockObject(NULL_BRUSH));
	COLORREF keySelectedMarkerCol = RGB(100, 255, 255);
	CPen keySelectedPen(PS_SOLID, 1, keySelectedMarkerCol);
	CBrush keySelectedBrush(keySelectedMarkerCol);
	for (int keyTimeIndex = 0; m_pKeyTimeSet && keyTimeIndex < m_pKeyTimeSet->GetKeyTimeCount(); ++keyTimeIndex)
	{
		int keyCountBound = __max(m_pKeyTimeSet->GetKeyCountBound(), 1);
		int keyCount = __min(m_pKeyTimeSet->GetKeyCount(keyTimeIndex), keyCountBound);
		float colorCodeFraction = float(keyCount) / keyCountBound;
		COLORREF keyMarkerCol = InterpolateColorRef(RGB(0, 255, 0), RGB(255, 0, 0), colorCodeFraction);

		CPen keyPen(PS_SOLID, 1, keyMarkerCol);
		CBrush keyBrush(keyMarkerCol);
		bool keyTimeSelected = m_pKeyTimeSet && m_pKeyTimeSet->GetKeyTimeSelected(keyTimeIndex);
		CBrush& brushToUse = (keyTimeSelected ? keySelectedBrush : keyBrush);
		dc.SelectObject(&brushToUse);
		CPen& penToUse = (keyTimeSelected ? keySelectedPen : keyPen);
		dc.SelectObject(&penToUse);

		float keyTime = (m_pKeyTimeSet ? m_pKeyTimeSet->GetKeyTime(keyTimeIndex) : 0.0f);

		int x = TimeToClient(keyTime);
		dc.Rectangle(x - 2, rc.top, x + 3, rc.bottom);
	}

	dc.SelectObject(pOldPen);
}

//////////////////////////////////////////////////////////////////////////
Range CTimelineCtrl::GetVisibleRange() const
{
	Range r;

	r.start = (m_scrollOffset - m_leftOffset) / m_timeScale;
	r.end = r.start + (m_rcTimeline.Width()) / m_timeScale;
	// Intersect range with global time range.
	r = m_timeRange & r;

	return r;
}

/////////////////////////////////////////////////////////////////////////////
//Mouse Message Handlers
//////////////////////////////////////////////////////////////////////////
void CTimelineCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	CWnd::OnLButtonDown(nFlags, point);

	SetFocus();

	if (m_trackingMode != TRACKING_MODE_NONE)
		return;

	SendNotifyEvent(NM_CLICK);

	int hitKeyTimeIndex = HitKeyTimes(point);
	bool autoDeselect = !(nFlags & MK_CONTROL) && (m_pKeyTimeSet && ((hitKeyTimeIndex >= 0) ? !m_pKeyTimeSet->GetKeyTimeSelected(hitKeyTimeIndex) : true));
	for (int keyTimeIndex = 0; m_pKeyTimeSet && keyTimeIndex < m_pKeyTimeSet->GetKeyTimeCount(); ++keyTimeIndex)
	{
		bool shouldBeSelected;
		if (keyTimeIndex == hitKeyTimeIndex)
			shouldBeSelected = (nFlags & MK_CONTROL) || !((nFlags & MK_SHIFT) && m_pKeyTimeSet->GetKeyTimeSelected(keyTimeIndex));
		else
			shouldBeSelected = (!autoDeselect || (nFlags & MK_SHIFT)) && m_pKeyTimeSet->GetKeyTimeSelected(keyTimeIndex);

		m_pKeyTimeSet->SetKeyTimeSelected(keyTimeIndex, shouldBeSelected);
	}
	TrackingMode trackingMode = (hitKeyTimeIndex >= 0 ? TRACKING_MODE_MOVE_KEYS : TRACKING_MODE_NONE);
	if (trackingMode == TRACKING_MODE_NONE)
		trackingMode = ((nFlags & MK_CONTROL) ? TRACKING_MODE_SELECTION_RANGE : TRACKING_MODE_SET_TIME);
	StartTracking(trackingMode);

	switch (m_trackingMode)
	{
	case TRACKING_MODE_SET_TIME:
		{
			if (m_bTrackingSnapToFrames)
				SetTimeMarker(FacialEditorSnapTimeToFrame(ClientToTime(point.x)));
			else
				SetTimeMarker(ClientToTime(point.x));
			CScopedVariableSetter<bool> ignoreSetTime(m_bIgnoreSetTime, true);
			SendNotifyEvent(TLN_START_CHANGE);
			SendNotifyEvent(TLN_CHANGE);
		}
		break;

	case TRACKING_MODE_MOVE_KEYS:
		m_bChangedKeyTimeSet = false;
		m_copyKeyTimes = (nFlags & MK_CONTROL ? true : false);
		break;

	case TRACKING_MODE_NONE:
		break;
	}

	m_lastPoint = point;

	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CTimelineCtrl::OnRButtonDown(UINT nFlags, CPoint point)
{
	CWnd::OnRButtonDown(nFlags, point);

	SendNotifyEvent(NM_RCLICK);

	SetFocus();

	if (m_trackingMode != TRACKING_MODE_NONE)
		return;

	StartTracking(TRACKING_MODE_SET_TIME);

	if (m_bTrackingSnapToFrames)
		SetTimeMarker(FacialEditorSnapTimeToFrame(ClientToTime(point.x)));
	else
		SetTimeMarker(ClientToTime(point.x));
	CScopedVariableSetter<bool> ignoreSetTime(m_bIgnoreSetTime, true);
	SendNotifyEvent(TLN_START_CHANGE);
	SendNotifyEvent(TLN_CHANGE);

	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CTimelineCtrl::OnRButtonUp(UINT nFlags, CPoint point)
{
	switch (m_trackingMode)
	{
	case TRACKING_MODE_SET_TIME:
		{
			SendNotifyEvent(TLN_END_CHANGE);
		}
		break;
	}

	if (m_trackingMode != TRACKING_MODE_NONE)
	{
		StopTracking();
	}

	CWnd::OnRButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CTimelineCtrl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_DELETE)
		SendNotifyEvent(TLN_DELETE);
	if (nChar == VK_SPACE && m_playCallback)
		m_playCallback();
}

//////////////////////////////////////////////////////////////////////////
void CTimelineCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	if (GetCapture() != this)
		StopTracking();

	switch (m_trackingMode)
	{
	case TRACKING_MODE_SET_TIME:
		{
			if (m_bTrackingSnapToFrames)
				SetTimeMarker(FacialEditorSnapTimeToFrame(ClientToTime(point.x)));
			else
				SetTimeMarker(ClientToTime(point.x));
			CScopedVariableSetter<bool> ignoreSetTime(m_bIgnoreSetTime, true);
			SendNotifyEvent(TLN_CHANGE);
		}
		break;

	case TRACKING_MODE_MOVE_KEYS:
		{
			if (m_pKeyTimeSet && !m_bChangedKeyTimeSet)
			{
				m_bChangedKeyTimeSet = true;
				m_pKeyTimeSet->BeginEdittingKeyTimes();
			}

			bool altClicked = ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0);
			float scale, offset;
			float startTime = ClientToTime(m_lastPoint.x);
			float endTime = ClientToTime(point.x);
			if (altClicked)
			{
				// Alt was pressed, so we should scale the key times rather than translate.
				// Calculate the scaling parameters (ie t1 = t0 * M + C).
				scale = 1.0f;
				if (fabsf(startTime - m_fTimeMarker) > 0.1)
					scale = (endTime - m_fTimeMarker) / (startTime - m_fTimeMarker);
				offset = endTime - startTime * scale;
			}
			else
			{
				// Simply move the keys.
				offset = endTime - startTime;
				scale = 1.0f;
			}

			MoveSelectedKeyTimes(scale, offset);
		}
		break;

	case TRACKING_MODE_SELECTION_RANGE:
		{
			float start = min(ClientToTime(m_lastPoint.x), ClientToTime(point.x));
			float end = max(ClientToTime(m_lastPoint.x), ClientToTime(point.x));
			SelectKeysInRange(start, end, !(nFlags & MK_SHIFT));
			m_lastPoint = point;
			Invalidate();
		}
		break;

	case TRACKING_MODE_NONE:
		break;
	}

	//m_lastPoint = point;

	CWnd::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
CString CTimelineCtrl::TimeToString(float time)
{
	CString str;
	str.Format("%.3f", time);
	return str;
}

//////////////////////////////////////////////////////////////////////////
void CTimelineCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
	switch (m_trackingMode)
	{
	case TRACKING_MODE_MOVE_KEYS:
		{
			if (m_pKeyTimeSet && m_bChangedKeyTimeSet)
				m_pKeyTimeSet->EndEdittingKeyTimes();
		}
		break;

	case TRACKING_MODE_SET_TIME:
		{
			SendNotifyEvent(TLN_END_CHANGE);
		}
		break;

	case TRACKING_MODE_NONE:
		break;
	}

	if (m_trackingMode != TRACKING_MODE_NONE)
	{
		StopTracking();
	}

	CWnd::OnLButtonUp(nFlags, point);
}

/////////////////////////////////////////////////////////////////////////////
BOOL CTimelineCtrl::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	BOOL b = FALSE;
	HCURSOR hCursor = 0;
	hCursor = AfxGetApp()->LoadCursor(IDC_ARRWHITE);

	SetCursor(hCursor);

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
void CTimelineCtrl::StartTracking(TrackingMode trackingMode)
{
	m_trackingMode = trackingMode;
	SetCapture();

	if (m_trackingMode == TRACKING_MODE_SET_TIME)
		GetIEditor()->Get3DEngine()->SetShadowsGSMCache(false);
}

//////////////////////////////////////////////////////////////////////////
void CTimelineCtrl::StopTracking()
{
	if (!m_trackingMode)
		return;

	if (m_trackingMode == TRACKING_MODE_SET_TIME)
	{
		GetIEditor()->Get3DEngine()->SetShadowsGSMCache(true);
		GetIEditor()->Get3DEngine()->SetRecomputeCachedShadows();
	}

	m_trackingMode = TRACKING_MODE_NONE;
	ReleaseCapture();
}

//////////////////////////////////////////////////////////////////////////
void CTimelineCtrl::SetTimeMarker(float fTime)
{
	if (fTime < m_timeRange.start)
		fTime = m_timeRange.start;
	else if (fTime > m_timeRange.end)
		fTime = m_timeRange.end;

	if (fTime == m_fTimeMarker || m_bIgnoreSetTime)
		return;

	int x0 = TimeToClient(m_fTimeMarker);
	int x1 = TimeToClient(fTime);
	CRect rc = CRect(x0, m_rcClient.top, x1, m_rcClient.bottom);
	rc.NormalizeRect();
	rc.InflateRect(5, 0);
	InvalidateRect(rc);

	m_fTimeMarker = fTime;
}

//////////////////////////////////////////////////////////////////////////
void CTimelineCtrl::SetZoom(float fZoom)
{
	m_grid.zoom.x = fZoom;
}

//////////////////////////////////////////////////////////////////////////
void CTimelineCtrl::SetOrigin(float fOffset)
{
	m_grid.origin.x = fOffset;
}

//////////////////////////////////////////////////////////////////////////
void CTimelineCtrl::SetKeyTimeSet(IKeyTimeSet* pKeyTimeSet)
{
	m_pKeyTimeSet = pKeyTimeSet;
}

//////////////////////////////////////////////////////////////////////////
int CTimelineCtrl::HitKeyTimes(CPoint point)
{
	const int threshold = 3;

	int hitKeyTimeIndex = -1;
	for (int keyTimeIndex = 0; m_pKeyTimeSet && keyTimeIndex < m_pKeyTimeSet->GetKeyTimeCount(); ++keyTimeIndex)
	{
		float keyTime = (m_pKeyTimeSet ? m_pKeyTimeSet->GetKeyTime(keyTimeIndex) : 0.0f);
		int x = TimeToClient(keyTime);
		if (abs(point.x - x) <= threshold)
			hitKeyTimeIndex = keyTimeIndex;
	}

	return hitKeyTimeIndex;
}

//////////////////////////////////////////////////////////////////////////
void CTimelineCtrl::MoveSelectedKeyTimes(float scale, float offset)
{
	std::vector<int> indices;
	for (int keyTimeIndex = 0; m_pKeyTimeSet && keyTimeIndex < m_pKeyTimeSet->GetKeyTimeCount(); ++keyTimeIndex)
	{
		if (m_pKeyTimeSet && m_pKeyTimeSet->GetKeyTimeSelected(keyTimeIndex))
			indices.push_back(keyTimeIndex);
	}

	if (m_pKeyTimeSet)
		m_pKeyTimeSet->MoveKeyTimes(int(indices.size()), &indices[0], scale, offset, m_copyKeyTimes);
}

//////////////////////////////////////////////////////////////////////////
void CTimelineCtrl::SelectKeysInRange(float start, float end, bool select)
{
	for (int keyTimeIndex = 0; m_pKeyTimeSet && keyTimeIndex < m_pKeyTimeSet->GetKeyTimeCount(); ++keyTimeIndex)
	{
		float time = (m_pKeyTimeSet ? m_pKeyTimeSet->GetKeyTime(keyTimeIndex) : 0.0f);
		if (m_pKeyTimeSet && time >= start && time <= end)
			m_pKeyTimeSet->SetKeyTimeSelected(keyTimeIndex, select);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimelineCtrl::SendNotifyEvent(int nEvent)
{
	NMHDR nmh;
	nmh.hwndFrom = m_hWnd;
	nmh.idFrom = ::GetDlgCtrlID(m_hWnd);
	nmh.code = nEvent;

	CWnd* pOwner = GetOwner();
	pOwner->SendMessage(WM_NOTIFY, (WPARAM)GetDlgCtrlID(), (LPARAM)&nmh);
}

//////////////////////////////////////////////////////////////////////////
void CTimelineCtrl::SetMarkerStyle(MarkerStyle markerStyle)
{
	m_markerStyle = markerStyle;
}

//////////////////////////////////////////////////////////////////////////
void CTimelineCtrl::SetFPS(float fps)
{
	m_fps = fps;
}

//////////////////////////////////////////////////////////////////////////
void CTimelineCtrl::DrawSecondTicks(CDC& dc)
{
	CPen ltgray(PS_SOLID, 1, ltgrayCol);
	CPen black(PS_SOLID, 1, textCol);
	CPen redpen(PS_SOLID, 1, timeMarkerCol);

	for (int gx = m_grid.firstGridLine.x; gx < m_grid.firstGridLine.x + m_grid.numGridLines.x + 1; gx++)
	{
		dc.SelectObject(ltgray);

		int x = m_grid.GetGridLineX(gx);
		if (x < 0)
			continue;

		dc.MoveTo(m_rcTimeline.left + x, m_rcTimeline.bottom - 2);
		dc.LineTo(m_rcTimeline.left + x, m_rcTimeline.bottom - 4);

		//if (gx % 10 == 0)
		{
			char str[32];
			float t = m_grid.GetGridLineXValue(gx);
			t = floor(t * 1000.0f + 0.5f) / 1000.0f;
			cry_sprintf(str, "%g", t * m_fTicksTextScale);

			//t = t / pow(10,precision);
			dc.SelectObject(black);
			dc.MoveTo(m_rcTimeline.left + x, m_rcTimeline.bottom - 2);
			dc.LineTo(m_rcTimeline.left + x, m_rcTimeline.bottom - 14);
			dc.TextOut(m_rcTimeline.left + x + 2, m_rcTimeline.top, str);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
class TickDrawer
{
public:
	TickDrawer(CDC& dc, const CRect& rect)
		: rect(rect),
		dc(dc),
		ltgray(PS_SOLID, 1, ltgrayCol),
		black(PS_SOLID, 1, textCol),
		redpen(PS_SOLID, 1, timeMarkerCol)
	{
	}

	void operator()(int frameIndex, int x)
	{
		dc.SelectObject(ltgray);
		dc.MoveTo(int(rect.left + x), rect.bottom - 2);
		dc.LineTo(int(rect.left + x), rect.bottom - 4);

		{
			char str[32];
			cry_sprintf(str, "%d", frameIndex);

			//t = t / pow(10,precision);
			dc.SelectObject(black);
			dc.MoveTo(int(rect.left + x), rect.bottom - 2);
			dc.LineTo(int(rect.left + x), rect.bottom - 14);
			dc.TextOut(int(rect.left + x + 2), rect.top, str);
		}
	}

	CRect rect;
	CDC&  dc;
	CPen  ltgray;
	CPen  black;
	CPen  redpen;
};

void CTimelineCtrl::DrawFrameTicks(CDC& dc)
{
	TickDrawer tickDrawer(dc, m_rcTimeline);
	GridUtils::IterateGrid(tickDrawer, 50.0f, m_grid.zoom.x, m_grid.origin.x, m_fps, m_grid.rect.left, m_grid.rect.right);
}

void CTimelineCtrl::SetPlayCallback(const std::function<void()>& callback)
{
	m_playCallback = callback;
}

