// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SplineCtrlEx.h"
#include "MemDC.h"
#include "TimelineCtrl.h"
#include "IUndoManager.h"
#include "Util/Clipboard.h"
#include "Util/GridUtils.h"
#include "Controls/SharedFonts.h"
#include "IUndoManager.h"

IMPLEMENT_DYNAMIC(CSplineCtrlEx, CWnd)

#define DEFAULT_MIN_TIME_EPSILON     0.001f
#define MIN_TIME_EPSILON_FOR_SCALING 0.1f

#define ACTIVE_BKG_COLOR             RGB(190, 190, 190)
#define GRID_COLOR                   RGB(110, 110, 110)
#define EDIT_SPLINE_COLOR            RGB(128, 255, 128)

#define MIN_PIXEL_PER_GRID_X         50
#define MIN_PIXEL_PER_GRID_Y         10

#define LEFT_BORDER_OFFSET           40

const float CSplineCtrlEx::threshold = 0.015f;

//////////////////////////////////////////////////////////////////////////

static void SetKeyTangentType(ISplineInterpolator* pSpline, int key, ESplineKeyTangentType type)
{
	int flags = (pSpline->GetKeyFlags(key) & ~SPLINE_KEY_TANGENT_IN_MASK) & ~SPLINE_KEY_TANGENT_OUT_MASK;
	pSpline->SetKeyFlags(key, flags | (type << SPLINE_KEY_TANGENT_IN_SHIFT) | (type << SPLINE_KEY_TANGENT_OUT_SHIFT));
}

//////////////////////////////////////////////////////////////////////////
class CUndoSplineCtrlEx : public ISplineCtrlUndo
{
public:
	CUndoSplineCtrlEx(CSplineCtrlEx* pCtrl, std::vector<ISplineInterpolator*>& splineContainer)
	{
		m_pCtrl = FindControl(pCtrl);

		for (size_t splineIndex = 0; splineIndex < splineContainer.size(); ++splineIndex)
			AddSpline(splineContainer[splineIndex]);

		SerializeSplines(&SplineEntry::undo, false);
	}

protected:
	void AddSpline(ISplineInterpolator* pSpline)
	{
		CSplineCtrlEx* pCtrl = FindControl(m_pCtrl);
		m_splineEntries.resize(m_splineEntries.size() + 1);
		SplineEntry& entry = m_splineEntries.back();
		ISplineSet* pSplineSet = (pCtrl ? pCtrl->m_pSplineSet : 0);
		entry.id = (pSplineSet ? pSplineSet->GetIDFromSpline(pSpline) : 0);
		entry.pSpline = pSpline;

		const int numKeys = pSpline->GetKeyCount();
		entry.keySelectionFlags.reserve(numKeys);
		for (int i = 0; i < numKeys; ++i)
			entry.keySelectionFlags.push_back(pSpline->GetKeyFlags(i) & ESPLINE_KEY_UI_SELECTED_MASK);
	}

	virtual int         GetSize()        { return sizeof(*this); }
	virtual const char* GetDescription() { return "UndoSplineCtrlEx"; };

	virtual void        Undo(bool bUndo)
	{
		CSplineCtrlEx* pCtrl = FindControl(m_pCtrl);
		if (pCtrl)
			pCtrl->SendNotifyEvent(SPLN_BEFORE_CHANGE);
		if (bUndo)
			SerializeSplines(&SplineEntry::redo, false);
		SerializeSplines(&SplineEntry::undo, true);
		if (pCtrl && bUndo)
		{
			pCtrl->m_bKeyTimesDirty = true;
			pCtrl->SendNotifyEvent(SPLN_CHANGE);
			pCtrl->Invalidate();
		}
	}

	virtual void Redo()
	{
		CSplineCtrlEx* pCtrl = FindControl(m_pCtrl);
		if (pCtrl)
			pCtrl->SendNotifyEvent(SPLN_BEFORE_CHANGE);
		SerializeSplines(&SplineEntry::redo, true);
		if (pCtrl)
		{
			pCtrl->m_bKeyTimesDirty = true;
			pCtrl->SendNotifyEvent(SPLN_CHANGE);
			pCtrl->Invalidate();
		}
	}

private:
	class SplineEntry
	{
	public:
		std::vector<int>          keySelectionFlags;
		_smart_ptr<ISplineBackup> undo;
		_smart_ptr<ISplineBackup> redo;
		string                    id;
		ISplineInterpolator*      pSpline;
	};

	void SerializeSplines(_smart_ptr<ISplineBackup> SplineEntry::* backup, bool bLoading)
	{
		CSplineCtrlEx* pCtrl = FindControl(m_pCtrl);
		ISplineSet* pSplineSet = (pCtrl ? pCtrl->m_pSplineSet : 0);
		for (auto it = m_splineEntries.begin(); it != m_splineEntries.end(); ++it)
		{
			SplineEntry& entry = *it;
			ISplineInterpolator* pSpline = (pSplineSet ? pSplineSet->GetSplineFromID(entry.id) : entry.pSpline);

			if (!pSpline && pCtrl && pCtrl->GetSplineCount() > 0)
				pSpline = pCtrl->GetSpline(0);

			if (pSpline && bLoading)
				pSpline->Restore(entry.*backup);
			else if (pSpline)
				(entry.*backup) = pSpline->Backup();
		}
	}

public:
	typedef std::list<CSplineCtrlEx*> CSplineCtrls;

	static CSplineCtrlEx* FindControl(CSplineCtrlEx* pCtrl)
	{
		if (!pCtrl)
			return 0;

		auto iter = std::find(s_activeCtrls.begin(), s_activeCtrls.end(), pCtrl);
		if (iter == s_activeCtrls.end())
			return 0;

		return *iter;
	}

	static void RegisterControl(CSplineCtrlEx* pCtrl)
	{
		if (!FindControl(pCtrl))
			s_activeCtrls.push_back(pCtrl);
	}

	static void UnregisterControl(CSplineCtrlEx* pCtrl)
	{
		if (FindControl(pCtrl))
			s_activeCtrls.remove(pCtrl);
	}

	static CSplineCtrls s_activeCtrls;

	virtual bool IsSelectionChanged() const
	{
		CSplineCtrlEx* pCtrl = FindControl(m_pCtrl);
		ISplineSet* pSplineSet = (pCtrl ? pCtrl->m_pSplineSet : 0);

		for (auto it = m_splineEntries.begin(); it != m_splineEntries.end(); ++it)
		{
			const SplineEntry& entry = *it;
			ISplineInterpolator* pSpline = (pSplineSet ? pSplineSet->GetSplineFromID(entry.id) : entry.pSpline);

			if (!pSpline && pCtrl && pCtrl->GetSplineCount() > 0)
				pSpline = pCtrl->GetSpline(0);

			if (!pSpline)
				return false;

			if (pSpline->GetKeyCount() != entry.keySelectionFlags.size())
				return true;

			for (int i = 0; i < pSpline->GetKeyCount(); ++i)
			{
				if (entry.keySelectionFlags[i] != (pSpline->GetKeyFlags(i) & ESPLINE_KEY_UI_SELECTED_MASK))
					return true;
			}
		}

		return false;
	}

private:
	CSplineCtrlEx*           m_pCtrl;
	std::vector<SplineEntry> m_splineEntries;
	std::vector<float>       m_keyTimes;
};

CUndoSplineCtrlEx::CSplineCtrls CUndoSplineCtrlEx::s_activeCtrls;

//////////////////////////////////////////////////////////////////////////
CSplineCtrlEx::CSplineCtrlEx()
	: m_defaultKeyTangentType(SPLINE_KEY_TANGENT_NONE)
{
	m_totalSplineCount = 0;
	m_pHitSpline = 0;
	m_pHitDetailSpline = 0;
	m_nHitKeyIndex = -1;
	m_nHitDimension = -1;
	m_bHitIncomingHandle = true;
	m_nKeyDrawRadius = 3;
	m_gridX = 10;
	m_gridY = 10;
	m_timeRange.start = 0;
	m_timeRange.end = 1;
	m_fMinValue = -1;
	m_fMaxValue = 1;
	m_valueRange.Set(-1, 1);
	m_fTooltipScaleX = 1;
	m_fTooltipScaleY = 1;
	m_pTimelineCtrl = 0;

	m_cMousePos.SetPoint(0, 0);
	m_fTimeScale = 1;
	m_fValueScale = 1;
	m_fGridTimeScale = 30.0f;

	m_ticksStep = 10;

	m_fTimeMarker = -10;
	m_editMode = NothingMode;

	m_bSnapTime = false;
	m_bSnapValue = false;
	m_bBitmapValid = false;

	m_nLeftOffset = LEFT_BORDER_OFFSET;

	m_grid.zoom.x = 200;
	m_grid.zoom.y = 100;

	m_bKeyTimesDirty = false;

	m_rcSelect.SetRectEmpty();
	m_rcSpline = CRect(0.0f, 0.0f, 1.0f, 1.0f);

	m_boLeftMouseButtonDown = false;

	m_pSplineSet = 0;

	m_controlAmplitude = false;

	m_fMinTimeEpsilon = DEFAULT_MIN_TIME_EPSILON;

	m_defaultValueRange.Set(-1.1f, 1.1f);

	m_bEditLock = false;

	m_pCurrentUndo = nullptr;

	CUndoSplineCtrlEx::RegisterControl(this);
}

//////////////////////////////////////////////////////////////////////////
CSplineCtrlEx::~CSplineCtrlEx()
{
	CUndoSplineCtrlEx::UnregisterControl(this);
}

//////////////////////////////////////////////////////////////////////////
Vec2 CSplineCtrlEx::GetZoom()
{
	return m_grid.zoom;
}

Vec2 CSplineCtrlEx::GetScrollOffset()
{
	return m_grid.origin;
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::SetZoom(Vec2 zoom, CPoint center)
{
	m_grid.SetZoom(zoom, CPoint(center.x, m_rcSpline.bottom - center.y));
	SetScrollOffset(m_grid.origin);
	if (m_pTimelineCtrl)
	{
		m_pTimelineCtrl->SetZoom(zoom.x);
		m_pTimelineCtrl->SetOrigin(m_grid.origin.x);
		m_pTimelineCtrl->Invalidate();
	}
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::SetZoom(Vec2 zoom)
{
	m_grid.zoom = zoom;
	SetScrollOffset(m_grid.origin);
	if (m_pTimelineCtrl)
	{
		m_pTimelineCtrl->SetZoom(zoom.x);
		m_pTimelineCtrl->SetOrigin(m_grid.origin.x);
		m_pTimelineCtrl->Invalidate();
	}
	SendNotifyEvent(SPLN_SCROLL_ZOOM);
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::SetScrollOffset(Vec2 ofs)
{
	m_grid.origin = ofs;
	if (m_pTimelineCtrl)
	{
		m_pTimelineCtrl->SetOrigin(m_grid.origin.x);
		m_pTimelineCtrl->RedrawWindow();
	}
	SendNotifyEvent(SPLN_SCROLL_ZOOM);
	RedrawWindow();
}

//////////////////////////////////////////////////////////////////////////
float CSplineCtrlEx::SnapTime(float time)
{
	if (m_bSnapTime)
	{
		float step = m_grid.step.x / 10.0f;
		return floor((time / step) + 0.5f) * step;
	}
	return time;
}

//////////////////////////////////////////////////////////////////////////
float CSplineCtrlEx::SnapValue(float val)
{
	if (m_bSnapValue)
	{
		float step = m_grid.step.y;
		return floor((val / step) + 0.5f) * step;
	}
	return val;
}

//////////////////////////////////////////////////////////////////////////
int CSplineCtrlEx::GetKeyTimeCount() const
{
	UpdateKeyTimes();

	return int(m_keyTimes.size());
}

//////////////////////////////////////////////////////////////////////////
float CSplineCtrlEx::GetKeyTime(int index) const
{
	UpdateKeyTimes();

	return m_keyTimes[index].time;
}

//////////////////////////////////////////////////////////////////////////
bool CSplineCtrlEx::GetKeyTimeSelected(int index) const
{
	UpdateKeyTimes();

	return m_keyTimes[index].selected;
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::SetKeyTimeSelected(int index, bool selected)
{
	m_keyTimes[index].selected = selected;
}

//////////////////////////////////////////////////////////////////////////
int CSplineCtrlEx::GetKeyCount(int index) const
{
	UpdateKeyTimes();

	return m_keyTimes[index].count;
}

//////////////////////////////////////////////////////////////////////////
int CSplineCtrlEx::GetKeyCountBound() const
{
	UpdateKeyTimes();

	return m_totalSplineCount;
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::BeginEdittingKeyTimes()
{
	if (CUndo::IsRecording())
	{
		GetIEditor()->GetIUndoManager()->Cancel();
		m_pCurrentUndo = nullptr;
	}
	GetIEditor()->GetIUndoManager()->Begin();

	for (int keyTimeIndex = 0; keyTimeIndex < int(m_keyTimes.size()); ++keyTimeIndex)
		m_keyTimes[keyTimeIndex].oldTime = m_keyTimes[keyTimeIndex].time;
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::EndEdittingKeyTimes()
{
	if (CUndo::IsRecording())
	{
		GetIEditor()->GetIUndoManager()->Accept("Batch key move");
		m_pCurrentUndo = nullptr;
	}

	m_bKeyTimesDirty = true;
	RedrawWindow();
	if (m_pTimelineCtrl)
		m_pTimelineCtrl->Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::MoveKeyTimes(int numChanges, int* indices, float scale, float offset, bool copyKeys)
{
	if (CUndo::IsRecording())
	{
		GetIEditor()->GetIUndoManager()->Restore();

		std::vector<ISplineInterpolator*> splines;
		for (int splineIndex = 0; splineIndex < int(m_splines.size()); ++splineIndex)
		{
			ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline;
			if (pSpline)
			{
				splines.push_back(pSpline);
			}
		}
		CUndo::Record(m_pCurrentUndo = CreateSplineCtrlUndoObject(splines));

		for (int keyTimeIndex = 0; keyTimeIndex < int(m_keyTimes.size()); ++keyTimeIndex)
		{
			m_keyTimes[keyTimeIndex].time = m_keyTimes[keyTimeIndex].oldTime;
		}
	}

	class KeyChange
	{
	public:
		KeyChange(ISplineInterpolator* pSpline, int keyIndex, float oldTime, float newTime, int flags)
			: pSpline(pSpline), keyIndex(keyIndex), oldTime(oldTime), newTime(newTime), flags(flags) {}

		ISplineInterpolator*           pSpline;
		int                            keyIndex;
		float                          oldTime;
		float                          newTime;
		ISplineInterpolator::ValueType value;
		int                            flags;
		ISplineInterpolator::ValueType tin, tout;
	};

	std::vector<KeyChange> individualKeyChanges;
	for (int changeIndex = 0; indices && changeIndex < numChanges; ++changeIndex)
	{
		int index = (indices ? indices[changeIndex] : 0);

		float oldTime = m_keyTimes[index].time;
		float time = __max(m_timeRange.start, __min(m_timeRange.end, scale * oldTime + offset));

		for (int splineIndex = 0; splineIndex < int(m_splines.size()); ++splineIndex)
		{
			ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline;

			for (int keyIndex = 0; pSpline && keyIndex < pSpline->GetKeyCount(); ++keyIndex)
			{
				float keyTime = pSpline->GetKeyTime(keyIndex);
				KeyChange change(pSpline, keyIndex, keyTime, SnapTimeToGridVertical(time), pSpline->GetKeyFlags(keyIndex));

				pSpline->GetKeyValue(keyIndex, change.value);
				pSpline->GetKeyTangents(keyIndex, change.tin, change.tout);

				if (fabsf(keyTime - oldTime) < threshold)
				{
					individualKeyChanges.push_back(change);
				}
			}
		}

		m_keyTimes[index].time = SnapTimeToGridVertical(time);
	}

	for (std::vector<KeyChange>::iterator itChange = individualKeyChanges.begin(); itChange != individualKeyChanges.end(); ++itChange)
	{
		(*itChange).pSpline->SetKeyTime((*itChange).keyIndex, (*itChange).newTime);
	}

	if (copyKeys)
	{
		for (std::vector<KeyChange>::iterator keyToAdd = individualKeyChanges.begin(), endKeysToAdd = individualKeyChanges.end(); keyToAdd != endKeysToAdd; ++keyToAdd)
		{
			int keyIndex = (*keyToAdd).pSpline->InsertKey((*keyToAdd).oldTime, (*keyToAdd).value);
			(*keyToAdd).pSpline->SetKeyTangents(keyIndex, (*keyToAdd).tin, (*keyToAdd).tout);
			(*keyToAdd).pSpline->SetKeyFlags(keyIndex, (*keyToAdd).flags & (~ESPLINE_KEY_UI_SELECTED_MASK));
		}
	}

	// Loop through all moved keys, checking whether there are multiple keys on the same frame.
	for (int splineIndex = 0; splineIndex < int(m_splines.size()); ++splineIndex)
	{
		ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline;

		float lastKeyTime = -FLT_MAX;
		pSpline->Update();
		for (int keyIndex = 0, keys = pSpline->GetKeyCount(); keyIndex <= keys; )
		{
			float keyTime = pSpline->GetKeyTime(keyIndex);
			if (fabsf(keyTime - lastKeyTime) < m_fMinTimeEpsilon)
			{
				--keys;
				pSpline->RemoveKey(keyIndex);
			}
			else
			{
				++keyIndex;
				lastKeyTime = keyTime;
			}
		}
	}

	SendNotifyEvent(SPLN_CHANGE);
	RedrawWindow();
	if (m_pTimelineCtrl)
	{
		m_pTimelineCtrl->Invalidate();
	}
}

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CSplineCtrlEx, CWnd)
ON_WM_SIZE()
ON_WM_ERASEBKGND()
ON_WM_PAINT()
ON_WM_LBUTTONDOWN()
ON_WM_RBUTTONDOWN()
ON_WM_MBUTTONDOWN()
ON_WM_MBUTTONUP()
ON_WM_MOUSEMOVE()
ON_WM_MOUSEWHEEL()
ON_WM_LBUTTONUP()
ON_WM_RBUTTONUP()
ON_WM_SETCURSOR()
ON_WM_LBUTTONDBLCLK()
ON_WM_RBUTTONDOWN()
ON_WM_KEYDOWN()
ON_WM_CAPTURECHANGED()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSplineCtrlEx message handlers

/////////////////////////////////////////////////////////////////////////////
BOOL CSplineCtrlEx::Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID)
{
	LPCTSTR lpClassName = AfxRegisterWndClass(CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, AfxGetApp()->LoadStandardCursor(IDC_ARROW), NULL, NULL);
	return CreateEx(0, lpClassName, "SplineCtrl", dwStyle, rc, pParentWnd, nID);
}

//////////////////////////////////////////////////////////////////////////
BOOL CSplineCtrlEx::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::OnSize(UINT nType, int cx, int cy)
{
	CRect oldRect = m_rcSpline;
	;

	__super::OnSize(nType, cx, cy);

	GetClientRect(m_rcClient);
	m_rcSpline = m_rcClient;

	if (m_pTimelineCtrl)
	{
		CRect rct(m_rcSpline);
		rct.bottom = rct.top + 16;
		m_rcSpline.top = rct.bottom + 1;
		rct.left += m_nLeftOffset;
		m_pTimelineCtrl->MoveWindow(rct, TRUE);
	}

	m_rcSpline.left += m_nLeftOffset;

	m_grid.rect = m_rcSpline;

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

	int oldW = oldRect.Width();
	int oldH = oldRect.Height();

	if (GetSafeHwnd() && cx > 1 && cy > 1 && oldW > 1 && oldH > 1 && m_rcSpline.Width() > 0 && m_rcSpline.Height())
		SetZoom(Vec2(float(m_rcSpline.Width()) / oldW * GetZoom().x, float(m_rcSpline.Height()) / oldH * GetZoom().y));
}

//////////////////////////////////////////////////////////////////////////
BOOL CSplineCtrlEx::PreTranslateMessage(MSG* pMsg)
{
	if (!m_tooltip.m_hWnd)
	{
		CRect rc;
		GetClientRect(rc);
		m_tooltip.Create(this);
		m_tooltip.SetDelayTime(TTDT_AUTOPOP, 5000);
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
CPoint CSplineCtrlEx::TimeToPoint(float time, ISplineInterpolator* pSpline)
{
	float val = 0;
	if (pSpline)
		pSpline->InterpolateFloat(time, val);

	return WorldToClient(Vec2(time, val));
	;
}

//////////////////////////////////////////////////////////////////////////
float CSplineCtrlEx::TimeToXOfs(float x)
{
	return WorldToClient(Vec2(float(x), 0.0f)).x;
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::PointToTimeValue(CPoint point, float& time, float& value)
{
	Vec2 v = ClientToWorld(point);
	value = v.y;
	time = XOfsToTime(point.x);
}

//////////////////////////////////////////////////////////////////////////
float CSplineCtrlEx::XOfsToTime(int x)
{
	Vec2 v = ClientToWorld(CPoint(x, 0));
	float time = v.x;
	return time;
}

//////////////////////////////////////////////////////////////////////////
CPoint CSplineCtrlEx::XOfsToPoint(int x, ISplineInterpolator* pSpline)
{
	return TimeToPoint(XOfsToTime(x), pSpline);
}

//////////////////////////////////////////////////////////////////////////
CPoint CSplineCtrlEx::WorldToClient(Vec2 v)
{
	CPoint p = m_grid.WorldToClient(v);
	p.y = m_rcSpline.bottom - p.y;
	return p;
}

//////////////////////////////////////////////////////////////////////////
Vec2 CSplineCtrlEx::ClientToWorld(CPoint point)
{
	Vec2 v = m_grid.ClientToWorld(CPoint(point.x, m_rcSpline.bottom - point.y));
	return v;
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::OnPaint()
{
	CRect updateRect;
	GetUpdateRect(&updateRect);
	//OutputDebugString("OnPaint\n");

	CPaintDC PaintDC(this);

	CRect rcClient;
	GetClientRect(&rcClient);

	if (!m_offscreenBitmap.GetSafeHandle())
	{
		m_offscreenBitmap.CreateCompatibleBitmap(&PaintDC, rcClient.Width(), rcClient.Height());
	}

	{
		CMemoryDC dc(PaintDC, &m_offscreenBitmap);
		CRect rcClip;
		PaintDC.GetClipBox(rcClip);
		CRgn clipRgn;
		clipRgn.CreateRectRgnIndirect(rcClip);
		dc.SelectClipRgn(&clipRgn);

		if (m_TimeUpdateRect != CRect(PaintDC.m_ps.rcPaint))
		{
			CBrush bkBrush;
			bkBrush.CreateSolidBrush(RGB(160, 160, 160));
			dc.FillRect(&PaintDC.m_ps.rcPaint, &bkBrush);

			m_grid.CalculateGridLines();

			//Draw Grid
			DrawGrid(&dc);

			CRect drawSplineRect = updateRect;
			drawSplineRect.IntersectRect(drawSplineRect, m_rcSpline);

			// Calculate the times corresponding to the left and right of the area to be painted -
			// we can use this to draw only the necessary parts of the splines.
			float startTime = XOfsToTime(drawSplineRect.left);
			float endTime = XOfsToTime(drawSplineRect.right);

			//Draw Keys and Curve
			for (int i = 0; i < int(m_splines.size()); ++i)
			{
				DrawSpline(&dc, m_splines[i], startTime, endTime);
				DrawKeys(&dc, i, startTime, endTime);
			}

			if (!m_tooltipText.IsEmpty())
			{
				DrawTooltip(dc);
			}
		}
		m_TimeUpdateRect.SetRectEmpty();
	}

	DrawTimeMarker(&PaintDC);
}

//////////////////////////////////////////////////////////////////////////
class VerticalLineDrawer
{
public:
	VerticalLineDrawer(CDC& dc, const CRect& rect)
		: rect(rect),
		dc(dc)
	{
	}

	void operator()(int frameIndex, int x)
	{
		dc.MoveTo(x, rect.top);
		dc.LineTo(x, rect.bottom);
	}

	CDC&  dc;
	CRect rect;
};

void CSplineCtrlEx::DrawGrid(CDC* pDC)
{
	if (GetStyle() & SPLINE_STYLE_NOGRID)
		return;

	CPoint ptTop = WorldToClient(Vec2(0.0f, m_valueRange.end));
	CPoint ptBottom = WorldToClient(Vec2(0.0f, m_valueRange.start));
	CPoint pt0 = WorldToClient(Vec2(m_timeRange.start, 0));
	CPoint pt1 = WorldToClient(Vec2(m_timeRange.end, 0));
	CRect timeRc = CRect(pt0.x - 2, ptTop.y, pt1.x + 2, ptBottom.y);
	timeRc.IntersectRect(timeRc, m_rcSpline);
	pDC->FillSolidRect(timeRc, ACTIVE_BKG_COLOR);

	CPen penGridSolid;
	penGridSolid.CreatePen(PS_SOLID, 1, GRID_COLOR);
	//////////////////////////////////////////////////////////////////////////
	CPen* pOldPen = pDC->SelectObject(&penGridSolid);

	/// Draw Left Separator.
	pDC->FillSolidRect(CRect(m_rcClient.left, m_rcClient.top, m_rcClient.left + m_nLeftOffset - 1, m_rcClient.bottom), ACTIVE_BKG_COLOR);
	pDC->MoveTo(m_rcClient.left + m_nLeftOffset, m_rcClient.bottom);
	pDC->LineTo(m_rcClient.left + m_nLeftOffset, m_rcClient.top);
	//////////////////////////////////////////////////////////////////////////

	int gy;

	LOGBRUSH logBrush;
	logBrush.lbStyle = BS_SOLID;
	logBrush.lbColor = GRID_COLOR;

	CPen pen;
	pen.CreatePen(PS_COSMETIC | PS_ALTERNATE, 1, &logBrush);
	pDC->SelectObject(&pen);

	pDC->SetTextColor(RGB(0, 0, 0));
	pDC->SetBkMode(TRANSPARENT);
	pDC->SelectObject(gMFCFonts.hSystemFont);

	//if (m_grid.pixelsPerGrid.y >= MIN_PIXEL_PER_GRID_Y)
	{
		char str[128];
		// Draw horizontal grid lines.
		for (gy = m_grid.firstGridLine.y; gy < m_grid.firstGridLine.y + m_grid.numGridLines.y + 1; gy++)
		{
			int y = m_grid.GetGridLineY(gy);
			if (y < 0)
				continue;
			int py = m_rcSpline.bottom - (m_rcSpline.top + y);
			if (py < m_rcSpline.top || py > m_rcSpline.bottom)
				continue;
			pDC->MoveTo(m_rcSpline.left, py);
			pDC->LineTo(m_rcSpline.right, py);

			float v = m_grid.GetGridLineYValue(gy);
			v = floor(v * 1000.0f + 0.5f) / 1000.0f;

			if ((v >= m_valueRange.start && v <= m_valueRange.end) || fabs(v - m_valueRange.start) < 0.01f || fabs(v - m_valueRange.end) < 0.01f)
			{
				cry_sprintf(str, "%g", v);
				pDC->TextOut(m_rcClient.left + 2, py - 8, str);
			}
		}
	}

	// Draw vertical grid lines.
	VerticalLineDrawer verticalLineDrawer(*pDC, m_rcSpline);
	GridUtils::IterateGrid(verticalLineDrawer, 50.0f, m_grid.zoom.x, m_grid.origin.x, m_fGridTimeScale, m_grid.rect.left, m_grid.rect.right);

	//////////////////////////////////////////////////////////////////////////
	{
		CPen pen0;
		pen0.CreatePen(PS_SOLID, 2, RGB(110, 100, 100));
		CPoint p = WorldToClient(Vec2(0, 0));

		pDC->SelectObject(&pen0);

		/// Draw X axis.
		pDC->MoveTo(m_rcSpline.left, p.y);
		pDC->LineTo(m_rcSpline.right, p.y);

		// Draw Y Axis.
		if (p.x > m_rcSpline.left && p.y < m_rcSpline.right)
		{
			pDC->MoveTo(p.x, m_rcSpline.top);
			pDC->LineTo(p.x, m_rcSpline.bottom);
		}
	}
	//////////////////////////////////////////////////////////////////////////

	pDC->SelectObject(pOldPen);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::DrawTooltip(CDC* pDC)
{
	pDC->SetTextColor(RGB(0, 0, 0));
	pDC->SetBkMode(TRANSPARENT);
	pDC->SelectObject(gMFCFonts.hSystemFont);

	pDC->TextOut(m_lastToolTipPos.x + 10, m_lastToolTipPos.y - 10, m_tooltipText);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::DrawSpline(CDC* pDC, SSplineInfo& splineInfo, float startTime, float endTime)
{
	CPen* pOldPen = (CPen*)pDC->GetCurrentPen();

	CRect rcClip;
	pDC->GetClipBox(rcClip);
	rcClip.IntersectRect(rcClip, m_rcSpline);

	//////////////////////////////////////////////////////////////////////////
	ISplineInterpolator* pSpline = splineInfo.pSpline;
	ISplineInterpolator* pDetailSpline = splineInfo.pDetailSpline;
	if (!pSpline)
	{
		return;
	}

	int nTotalNumberOfDimensions(0);
	int nCurrentDimension(0);

	int left = TimeToXOfs(startTime);//rcClip.left;
	int right = TimeToXOfs(endTime);//rcClip.right;
	CPoint p0 = TimeToPoint(pSpline->GetKeyTime(0), pSpline);
	CPoint p1 = TimeToPoint(pSpline->GetKeyTime(pSpline->GetKeyCount() - 1), pSpline);

	nTotalNumberOfDimensions = pSpline->GetNumDimensions();
	for (nCurrentDimension = 0; nCurrentDimension < nTotalNumberOfDimensions; nCurrentDimension++)
	{
		COLORREF splineColor = EDIT_SPLINE_COLOR;
		CPen pen;
		splineColor = splineInfo.anColorArray[nCurrentDimension];
		pen.CreatePen(PS_SOLID, 2, splineColor);

		if (p0.x > left && !pDetailSpline)
		{
			LOGBRUSH logBrush;
			logBrush.lbStyle = BS_SOLID;
			logBrush.lbColor = splineColor;

			CPen alternatePen;
			alternatePen.CreatePen(PS_COSMETIC | PS_ALTERNATE, 1, &logBrush);
			pDC->SelectObject(&alternatePen);

			pDC->MoveTo(CPoint(m_rcSpline.left, p0.y));
			pDC->LineTo(CPoint(p0.x, p0.y));
			left = p0.x;
		}

		if (p1.x < right && !pDetailSpline)
		{
			LOGBRUSH logBrush;
			logBrush.lbStyle = BS_SOLID;
			logBrush.lbColor = splineColor;

			CPen alternatePen;
			alternatePen.CreatePen(PS_COSMETIC | PS_ALTERNATE, 1, &logBrush);
			pDC->SelectObject(&alternatePen);

			pDC->MoveTo(CPoint(p1.x, p1.y));
			pDC->LineTo(CPoint(m_rcSpline.right, p1.y));
			right = p1.x;
		}

		pDC->SelectObject(&pen);

		int linesDrawn = 0;
		int pixels = 0;

		float gradient = 0.0f;
		int pointsInLine = -1;
		CPoint lineStart;
		for (int x = left; x <= right; x++)
		{
			++pixels;

			float time = XOfsToTime(x);
			ISplineInterpolator::ValueType value;
			ISplineInterpolator::ZeroValue(value);

			pSpline->Interpolate(time, value);

			if (pDetailSpline)
			{
				ISplineInterpolator::ValueType value2;
				ISplineInterpolator::ZeroValue(value2);

				pDetailSpline->Interpolate(time, value2);

				value[nCurrentDimension] = value[nCurrentDimension] + value2[nCurrentDimension];
			}

			CPoint pt = WorldToClient(Vec2(time, value[nCurrentDimension]));

			if ((x == right && pointsInLine >= 0) || (pointsInLine > 0 && fabs(lineStart.y + gradient * (pt.x - lineStart.x) - pt.y) > 1.0f))
			{
				lineStart = CPoint(pt.x - 1, lineStart.y + gradient * (pt.x - 1 - lineStart.x));
				pDC->LineTo(lineStart);
				gradient = float(pt.y - lineStart.y) / (pt.x - lineStart.x);
				pointsInLine = 1;
				++linesDrawn;
			}
			else if ((x == right && pointsInLine >= 0) || (pointsInLine > 0 && fabs(lineStart.y + gradient * (pt.x - lineStart.x) - pt.y) == 1.0f))
			{
				lineStart = pt;
				pDC->LineTo(lineStart);
				gradient = 0.0f;
				pointsInLine = 0;
				++linesDrawn;
			}
			else if (pointsInLine > 0)
			{
				++pointsInLine;
			}
			else if (pointsInLine == 0)
			{
				gradient = float(pt.y - lineStart.y) / (pt.x - lineStart.x);
				++pointsInLine;
			}
			else
			{
				pDC->MoveTo(pt);
				lineStart = pt;
				++pointsInLine;
				gradient = 0.0f;
			}
		}

		// Put back the old objects
		pDC->SelectObject(pOldPen);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::DrawKeys(CDC* pDC, int splineIndex, float startTime, float endTime)
{
	SSplineInfo& splineInfo = m_splines[splineIndex];
	ISplineInterpolator* pSpline = splineInfo.pSpline;
	ISplineInterpolator* pDetailSpline = splineInfo.pDetailSpline;

	if (!pSpline)
	{
		return;
	}

	// create and select a white pen
	CPen pen;
	pen.CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
	CPen* pOldPen = pDC->SelectObject(&pen);

	int lastKeyX = m_rcSpline.left - 100;

	int i;

	int nTotalNumberOfDimensions(0);
	int nCurrentDimension(0);

	nTotalNumberOfDimensions = pSpline->GetNumDimensions();
	for (nCurrentDimension = 0; nCurrentDimension < nTotalNumberOfDimensions; nCurrentDimension++)
	{
		// Why is this here? Not even god knows...
		//for (i = 0; i < pSpline->GetKeyCount() && pSpline->GetKeyTime(i) < startTime; ++i);
		int lastKeyX = m_rcSpline.left - 100;

		int numKeys = pSpline->GetKeyCount();
		for (i = 0; i < numKeys; i++)
		{
			float time = pSpline->GetKeyTime(i);
			if (time >= endTime)
				break;

			ISplineInterpolator::ValueType value;
			ISplineInterpolator::ZeroValue(value);

			pSpline->Interpolate(time, value);

			if (pDetailSpline)
			{
				ISplineInterpolator::ValueType value2;
				ISplineInterpolator::ZeroValue(value2);

				pDetailSpline->Interpolate(time, value2);

				value[nCurrentDimension] = value[nCurrentDimension] + value2[nCurrentDimension];
			}
			CPoint pt = WorldToClient(Vec2(time, value[nCurrentDimension]));
			;

			if (pt.x < m_rcSpline.left)
			{
				continue;
			}

			if (abs(pt.x - lastKeyX) < 4)
			{
				continue;
			}

			COLORREF clr = RGB(220, 220, 0);
			if (pSpline->IsKeySelectedAtDimension(i, nCurrentDimension))
			{
				clr = RGB(255, 0, 0);
				DrawTangentHandle(pDC, splineIndex, i, nCurrentDimension);
			}

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

			lastKeyX = pt.x;

			pDC->SelectObject(pOldBrush);
		}
	}

	pDC->SelectObject(pOldPen);
}

bool CSplineCtrlEx::GetTangentHandlePts(CPoint&, CPoint&, CPoint&, int, int, int)
{
	return false;
}

void CSplineCtrlEx::DrawTangentHandle(CDC* pDC, int nSpline, int nKey, int nDimension)
{
	// create and select a white pen
	CPen pen;
	pen.CreatePen(PS_SOLID, 1, RGB(96, 96, 96));
	CPen* pOldPen = pDC->SelectObject(&pen);

	// Draw in-tangent & out-tangent lines.
	CPoint a, b, pt;
	if (GetTangentHandlePts(a, pt, b, nSpline, nKey, nDimension))
	{
		pDC->MoveTo(a);
		pDC->LineTo(pt);
		pDC->LineTo(b);

		// Draw end-effectors.
		COLORREF clr = RGB(0, 220, 0);
		CBrush brush(clr);
		CBrush* pOldBrush = pDC->SelectObject(&brush);

		CRect rc;
		rc.left = a.x - m_nKeyDrawRadius;
		rc.right = a.x + m_nKeyDrawRadius;
		rc.top = a.y - m_nKeyDrawRadius;
		rc.bottom = a.y + m_nKeyDrawRadius;
		pDC->Rectangle(rc);
		rc.left = b.x - m_nKeyDrawRadius;
		rc.right = b.x + m_nKeyDrawRadius;
		rc.top = b.y - m_nKeyDrawRadius;
		rc.bottom = b.y + m_nKeyDrawRadius;
		pDC->Rectangle(rc);

		pDC->SelectObject(pOldBrush);
	}

	pDC->SelectObject(pOldPen);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::DrawTimeMarker(CDC* pDC)
{
	if (!(GetStyle() & SPLINE_STYLE_NO_TIME_MARKER))
	{
		CPen timePen;
		timePen.CreatePen(PS_SOLID, 1, RGB(255, 0, 255));
		CPen* pOldPen = pDC->SelectObject(&timePen);
		float x = TimeToXOfs(m_fTimeMarker);
		if (x >= m_rcSpline.left && x <= m_rcSpline.right)
		{
			pDC->MoveTo(x, m_rcSpline.top);
			pDC->LineTo(x, m_rcSpline.bottom);
		}
		pDC->SelectObject(pOldPen);
	}
}

/////////////////////////////////////////////////////////////////////////////
//Mouse Message Handlers
//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::OnLButtonDown(UINT nFlags, CPoint point)
{
	m_pCurrentUndo = nullptr;

	if (m_bEditLock)
	{
		return;
	}

	m_boLeftMouseButtonDown = true;

	if (m_editMode == TrackingMode)
	{
		return;
	}

	SetFocus();
	SendNotifyEvent(NM_CLICK);

	m_cMouseDownPos = point;

	ISplineInterpolator* pSpline = HitSpline(point);

	// Get control key status.
	bool bAltClick = CheckVirtualKey(VK_MENU);
	bool bCtrlClick = CheckVirtualKey(VK_CONTROL);
	bool bShiftClick = CheckVirtualKey(VK_SHIFT);

	switch (m_hitCode)
	{
	case HIT_KEY:
		{
			{
				CUndo undo("Select Spline Key");
				StoreUndo();

				SendNotifyEvent(SPLN_BEFORE_CHANGE);
				bool bHitSelection = IsKeySelected(m_pHitSpline, m_nHitKeyIndex, m_nHitDimension);
				bool bAddSelect = bCtrlClick;
				if (!bAddSelect && !bHitSelection)
				{
					ClearSelection();
				}
				SelectKey(pSpline, m_nHitKeyIndex, m_nHitDimension, true);
				SendNotifyEvent(SPLN_CHANGE);

				if (m_pCurrentUndo && !m_pCurrentUndo->IsSelectionChanged())
				{
					undo.Cancel();
				}

				m_pCurrentUndo = nullptr;
			}

			GetIEditor()->GetIUndoManager()->Begin();
			StartTracking(bCtrlClick);
		}
		break;

	case HIT_TANGENT_HANDLE:
		{
			{
				CUndo undo("Select Tangent Handle");
				SendNotifyEvent(SPLN_BEFORE_CHANGE);
				ClearSelection();
				SelectKey(pSpline, m_nHitKeyIndex, m_nHitDimension, true);
				SendNotifyEvent(SPLN_CHANGE);
			}
			StartTracking(false);
		}
		break;

	case HIT_SPLINE:
		{
			if (GetNumSelected() > 0)
			{
				StartTracking(bCtrlClick);
			}
		}
		break;

	case HIT_TIMEMARKER:
		{
			SendNotifyEvent(SPLN_TIME_START_CHANGE);
			m_editMode = TimeMarkerMode;
			SetCapture();
		}
		break;

	case HIT_NOTHING:
		{
			if (m_rcSpline.PtInRect(point))
			{
				GetIEditor()->GetIUndoManager()->Begin();
				StoreUndo();
				m_rcSelect.SetRectEmpty();
				m_editMode = SelectMode;
				SetCapture();
			}
		}
		break;
	}
	Invalidate();

	CWnd::OnLButtonDown(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::OnRButtonDown(UINT nFlags, CPoint point)
{
	m_pCurrentUndo = nullptr;

	CWnd::OnRButtonDown(nFlags, point);

	SetFocus();

	SendNotifyEvent(NM_RCLICK);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::OnMButtonDown(UINT nFlags, CPoint point)
{
	m_pCurrentUndo = nullptr;

	CWnd::OnMButtonDown(nFlags, point);
	SetFocus();

	bool bShiftClick = CheckVirtualKey(VK_SHIFT);

	if (m_editMode == NothingMode)
	{
		if (bShiftClick)
		{
			m_editMode = ZoomMode;
			SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEALL));
		}
		else
		{
			SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEALL));
			m_editMode = ScrollMode;
		}
		m_cMouseDownPos = point;
		SetCapture();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::OnMButtonUp(UINT nFlags, CPoint point)
{
	CWnd::OnMButtonUp(nFlags, point);

	if (m_editMode == ScrollMode || m_editMode == ZoomMode)
	{
		m_editMode = NothingMode;
		ReleaseCapture();
		return;
	}
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	m_pCurrentUndo = nullptr;

	if (m_bEditLock)
	{
		return;
	}

	RECT rc;
	GetClientRect(&rc);

	switch (m_hitCode)
	{
	case HIT_SPLINE:
		{
			if (m_pHitSpline)
				InsertKey(m_pHitSpline, m_pHitDetailSpline, point);
			RedrawWindow();
			if (m_pTimelineCtrl)
				m_pTimelineCtrl->Invalidate();
		}
		break;
	case HIT_KEY:
		{
			RemoveKey(m_pHitSpline, m_nHitKeyIndex);
		}
		break;
	case HIT_TANGENT_HANDLE:
		{
			if (m_bHitIncomingHandle)
				ModifySelectedKeysFlags(SPLINE_KEY_TANGENT_IN_MASK, 0);
			else
				ModifySelectedKeysFlags(SPLINE_KEY_TANGENT_OUT_MASK, 0);
		}
		break;
	}

	CWnd::OnLButtonDblClk(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_bEditLock)
	{
		CWnd::OnMouseMove(nFlags, point);
		return;
	}

	m_cMousePos = point;

	if (m_editMode == SelectMode)
	{
		SetCursor(NULL);
		CRect rc(m_cMouseDownPos, point);
		rc.NormalizeRect();
		rc.IntersectRect(rc, m_rcSpline);

		CDC* dc = GetDC();
		dc->DrawDragRect(rc, CSize(1, 1), m_rcSelect, CSize(1, 1));
		ReleaseDC(dc);
		m_rcSelect = rc;
	}

	if (m_editMode == TimeMarkerMode)
	{
		SetCursor(NULL);
		SetTimeMarker(XOfsToTime(point.x));
		SendNotifyEvent(SPLN_TIME_CHANGE);
	}

	if (m_boLeftMouseButtonDown)
	{
		if (m_editMode == TrackingMode && point != m_cMouseDownPos)
		{
			m_startedDragging = true;
			GetIEditor()->GetIUndoManager()->Restore();
			m_pCurrentUndo = nullptr;

			StoreUndo();

			bool bAltClick = CheckVirtualKey(VK_MENU);
			bool bCtrlClick = CheckVirtualKey(VK_CONTROL);
			bool bShiftClick = CheckVirtualKey(VK_SHIFT);

			Vec2 v0 = ClientToWorld(m_cMouseDownPos);
			Vec2 v1 = ClientToWorld(point);
			if (bAltClick)
			{
				TimeScaleKeys(m_fTimeMarker, v0.x, v1.x);
			}
			else if (m_controlAmplitude)
			{
				ScaleAmplitudeKeys(v0.x, v0.y, v1.y - v0.y);
			}
			else
			{
				MoveSelectedKeys(v1 - v0, m_copyKeys);
			}
		}
	}

	if (m_editMode == TrackingMode && GetNumSelected() == 1)
	{
		float time = 0;
		ISplineInterpolator::ValueType afValue;
		CString tipText;
		bool boFoundTheSelectedKey(false);

		for (int splineIndex = 0, endSpline = m_splines.size(); splineIndex < endSpline; ++splineIndex)
		{
			ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline;
			for (int i = 0; i < pSpline->GetKeyCount(); i++)
			{
				for (int nCurrentDimension = 0; nCurrentDimension < pSpline->GetNumDimensions(); nCurrentDimension++)
				{
					if (pSpline->IsKeySelectedAtDimension(i, nCurrentDimension))
					{
						time = pSpline->GetKeyTime(i);
						pSpline->GetKeyValue(i, afValue);
						tipText.Format("t=%.3f  v=%2.3f", time * m_fTooltipScaleX, afValue[nCurrentDimension] * m_fTooltipScaleY);
						boFoundTheSelectedKey = true;
						break;
					}
				}
				if (boFoundTheSelectedKey)
				{
					break;
				}
			}
		}

		if (point != m_lastToolTipPos)
		{
			m_lastToolTipPos = point;
			m_tooltipText = tipText;
			Invalidate();
			//m_tooltip.UpdateTipText( tipText,this,1 );
			//m_tooltip.Activate(TRUE);

		}
	}

	switch (m_editMode)
	{
	case ScrollMode:
		{
			// Set the new scrolled coordinates
			float ofsx = m_grid.origin.x - (point.x - m_cMouseDownPos.x) / m_grid.zoom.x;
			float ofsy = m_grid.origin.y + (point.y - m_cMouseDownPos.y) / m_grid.zoom.y;
			SetScrollOffset(Vec2(ofsx, ofsy));
			m_cMouseDownPos = point;
		}
		break;

	case ZoomMode:
		{
			float ofsx = (point.x - m_cMouseDownPos.x) * 0.01f;
			float ofsy = (point.y - m_cMouseDownPos.y) * 0.01f;

			Vec2 z = m_grid.zoom;
			if (ofsx != 0)
				z.x = max(z.x * (1.0f + ofsx), 0.001f);
			if (ofsy != 0)
				z.y = max(z.y * (1.0f + ofsy), 0.001f);
			SetZoom(z, m_cMouseDownPos);
			m_cMouseDownPos = point;
		}
		break;
	}

	CWnd::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
bool CSplineCtrlEx::CheckVirtualKey(int virtualKey)
{
	GetAsyncKeyState(virtualKey);
	if (GetAsyncKeyState(virtualKey))
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::UpdateKeyTimes() const
{
	if (!m_bKeyTimesDirty)
		return;

	std::vector<float> selectedKeyTimes;
	selectedKeyTimes.reserve(m_keyTimes.size());
	for (std::vector<KeyTime>::iterator it = m_keyTimes.begin(), end = m_keyTimes.end(); it != end; ++it)
	{
		if ((*it).selected)
			selectedKeyTimes.push_back((*it).time);
	}
	std::sort(selectedKeyTimes.begin(), selectedKeyTimes.end());

	m_keyTimes.clear();
	for (int splineIndex = 0; splineIndex < int(m_splines.size()); ++splineIndex)
	{
		ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline;

		for (int keyIndex = 0; pSpline && keyIndex < pSpline->GetKeyCount(); ++keyIndex)
		{
			float value = pSpline->GetKeyTime(keyIndex);

			int lower = 0;
			int upper = int(m_keyTimes.size());
			while (lower < upper - 1)
			{
				int mid = ((lower + upper) >> 1);
				((m_keyTimes[mid].time >= value) ? upper : lower) = mid;
			}

			if ((lower >= int(m_keyTimes.size()) || fabsf(m_keyTimes[lower].time - value) > threshold) &&
			    (upper >= int(m_keyTimes.size()) || fabsf(m_keyTimes[upper].time - value) > threshold))
				m_keyTimes.insert(m_keyTimes.begin() + upper, KeyTime(value, 0));
		}
	}

	for (std::vector<KeyTime>::iterator it = m_keyTimes.begin(), end = m_keyTimes.end(); it != end; ++it)
		(*it).count = (m_pSplineSet ? m_pSplineSet->GetKeyCountAtTime((*it).time, threshold) : 0);

	std::vector<float>::iterator itSelected = selectedKeyTimes.begin(), endSelected = selectedKeyTimes.end();
	for (std::vector<KeyTime>::iterator it = m_keyTimes.begin(), end = m_keyTimes.end(); it != end; ++it)
	{
		const float threshold = 0.01f;
		for (; itSelected != endSelected && (*itSelected) < (*it).time - threshold; ++itSelected)
			;
		if (itSelected != endSelected && fabsf((*itSelected) - (*it).time) < threshold)
			(*it).selected = true;
	}

	m_totalSplineCount = (m_pSplineSet ? m_pSplineSet->GetSplineCount() : 0);

	m_bKeyTimesDirty = false;
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_bEditLock)
	{
		return;
	}

	m_boLeftMouseButtonDown = false;

	if (m_editMode == TrackingMode)
	{
		StopTracking();

		if (!m_startedDragging)
		{
			// Get control key status.
			bool bAltClick = CheckVirtualKey(VK_MENU);
			bool bCtrlClick = CheckVirtualKey(VK_CONTROL);
			bool bShiftClick = CheckVirtualKey(VK_SHIFT);

			bool bAddSelect = bCtrlClick;
			bool bUnselect = bAltClick;

			HitSpline(m_cMouseDownPos);
		}
	}

	if (m_editMode == SelectMode)
	{
		// Get control key status.
		bool bAltClick = CheckVirtualKey(VK_MENU);
		bool bCtrlClick = CheckVirtualKey(VK_CONTROL);
		bool bShiftClick = CheckVirtualKey(VK_SHIFT);

		bool bAddSelect = bCtrlClick;
		bool bUnselect = bAltClick;

		if (!bAddSelect && !bUnselect)
		{
			ClearSelection();
		}

		SelectRectangle(m_rcSelect, !bUnselect);

		// Because of SPLN_CHANGE event which is created in SelectRectangle() function
		// a focus move to PropertyCtrl window. So we should bring the focus to this window to keep getting keydown message.
		SetFocus();

		m_rcSelect.SetRectEmpty();

		StopTracking();
	}

	if (m_editMode == TimeMarkerMode)
	{
		m_editMode = NothingMode;
		ReleaseCapture();
		SendNotifyEvent(SPLN_TIME_END_CHANGE);
	}

	if (m_pTimelineCtrl)
	{
		m_pTimelineCtrl->Invalidate();
	}

	m_tooltipText = "";
	Invalidate();
	m_editMode = NothingMode;

	CWnd::OnLButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::OnRButtonUp(UINT nFlags, CPoint point)
{
	CWnd::OnRButtonUp(nFlags, point);
}

/////////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::SelectKey(ISplineInterpolator* pSpline, int nKey, int nDimension, bool bSelect)
{
	if (nKey >= 0)
	{
		pSpline->SelectKeyAtDimension(nKey, nDimension, bSelect);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CSplineCtrlEx::IsKeySelected(ISplineInterpolator* pSpline, int nKey, int nHitDimension) const
{
	if (pSpline && nKey >= 0)
	{
		return (pSpline->IsKeySelectedAtDimension(nKey, nHitDimension));
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
int CSplineCtrlEx::GetNumSelected()
{
	int nSelected = 0;
	for (int splineIndex = 0, splineCount = m_splines.size(); splineIndex < splineCount; ++splineIndex)
	{
		if (ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline)
		{
			for (int i = 0; i < (int)pSpline->GetKeyCount(); i++)
			{

				for (int nCurrentDimension = 0; nCurrentDimension < pSpline->GetNumDimensions(); nCurrentDimension++)
				{
					if (pSpline->IsKeySelectedAtDimension(i, nCurrentDimension))
					{
						nSelected++;
					}
				}
			}
		}
	}
	return nSelected;
}

/////////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::SetSplineSet(ISplineSet* pSplineSet)
{
	m_pSplineSet = pSplineSet;
}

/////////////////////////////////////////////////////////////////////////////
BOOL CSplineCtrlEx::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
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
	case HIT_TANGENT_HANDLE:
		{
			HCURSOR hCursor;
			hCursor = AfxGetApp()->LoadCursor(IDC_ARRBLCK);
			SetCursor(hCursor);
			b = TRUE;
		} break;
	default:
		break;
	}

	if (m_tooltip.m_hWnd)
	{
		if (m_pHitSpline && m_nHitKeyIndex >= 0)
		{
			float time = m_pHitSpline->GetKeyTime(m_nHitKeyIndex);
			ISplineInterpolator::ValueType afValue;
			m_pHitSpline->GetKeyValue(m_nHitKeyIndex, afValue);
			CString tipText;
			tipText.Format("t=%.3f  v=%2.3f", time * m_fTooltipScaleX, afValue[m_nHitDimension] * m_fTooltipScaleY);

			m_tooltipText = tipText;
			if (m_lastToolTipPos != point)
			{
				m_lastToolTipPos = point;
				Invalidate();
			}
		}
		else if (m_editMode != TrackingMode)
		{
			if (!m_tooltipText.IsEmpty())
			{
				m_tooltipText = "";
				Invalidate();
			}
		}
	}

	if (!b)
		return CWnd::OnSetCursor(pWnd, nHitTest, message);
	else return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	BOOL bProcessed = false;

	switch (nChar)
	{
	case VK_DELETE:
		{
			RemoveSelectedKeys();
			RemoveSelectedKeyTimes();
			bProcessed = true;
		} break;
	case VK_UP:
		{
			CUndo undo("Move Spline Key(s)");
			SendNotifyEvent(SPLN_BEFORE_CHANGE);
			MoveSelectedKeys(ClientToWorld(CPoint(0, -1)), false);
			bProcessed = true;
		} break;
	case VK_DOWN:
		{
			CUndo undo("Move Spline Key(s)");
			SendNotifyEvent(SPLN_BEFORE_CHANGE);
			MoveSelectedKeys(ClientToWorld(CPoint(0, 1)), false);
			bProcessed = true;
		} break;
	case VK_LEFT:
		{
			CUndo undo("Move Spline Key(s)");
			SendNotifyEvent(SPLN_BEFORE_CHANGE);
			MoveSelectedKeys(ClientToWorld(CPoint(-1, 0)), false);
			bProcessed = true;
		} break;
	case VK_RIGHT:
		{
			CUndo undo("Move Spline Key(s)");
			SendNotifyEvent(SPLN_BEFORE_CHANGE);
			MoveSelectedKeys(ClientToWorld(CPoint(1, 0)), false);
			bProcessed = true;
		} break;

	default:
		break; //do nothing
	}

	bool bCtrl = GetKeyState(VK_CONTROL) != 0;
	if (nChar == 'C' && bCtrl)
	{
		CopyKeys();
		return;
	}

	if (nChar == 'V' && bCtrl)
	{
		PasteKeys();
		return;
	}

	if (nChar == 'Z' && bCtrl)
	{
		GetIEditor()->GetIUndoManager()->Undo();
		return;
	}
	if (nChar == 'Y' && bCtrl)
	{
		GetIEditor()->GetIUndoManager()->Redo();
		return;
	}

	if (!bProcessed)
		CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

//////////////////////////////////////////////////////////////////////////
BOOL CSplineCtrlEx::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if (zDelta == 0)
		return 0;
	Vec2 z = m_grid.zoom;
	float scale = 1.2f * fabs(zDelta / 120.0f);
	if (zDelta > 0)
	{
		z *= scale;
	}
	else
	{
		z /= scale;
	}
	SetZoom(z, m_cMousePos);

	return 1;
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::SetHorizontalExtent(int min, int max)
{
	/*
	   m_scrollMin.x = min;
	   m_scrollMax.x = max;
	   int width = max - min;
	   int nPage = m_rcClient.Width()/2;
	   int sx = width - nPage + m_rcSpline.left;

	   SCROLLINFO si;
	   ZeroStruct(si);
	   si.cbSize = sizeof(si);
	   si.fMask = SIF_ALL;
	   si.nMin = m_scrollMin.x;
	   si.nMax = m_scrollMax.x - nPage + m_rcSpline.left;
	   si.nPage = m_rcClient.Width()/2;
	   si.nPos = m_scrollOffset.x;
	   //si.nPage = max(0,m_rcClient.Width() - m_leftOffset*2);
	   //si.nPage = 1;
	   //si.nPage = 1;
	   SetScrollInfo( SB_HORZ,&si,TRUE );
	 */
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	SCROLLINFO si;
	GetScrollInfo(SB_HORZ, &si);

	// Get the minimum and maximum scroll-bar positions.
	int minpos = si.nMin;
	int maxpos = si.nMax;
	int nPage = si.nPage;

	// Get the current position of scroll box.
	int curpos = si.nPos;

	// Determine the new position of scroll box.
	switch (nSBCode)
	{
	case SB_LEFT:      // Scroll to far left.
		curpos = minpos;
		break;

	case SB_RIGHT:      // Scroll to far right.
		curpos = maxpos;
		break;

	case SB_ENDSCROLL:   // End scroll.
		break;

	case SB_LINELEFT:      // Scroll left.
		if (curpos > minpos)
			curpos--;
		break;

	case SB_LINERIGHT:   // Scroll right.
		if (curpos < maxpos)
			curpos++;
		break;

	case SB_PAGELEFT:    // Scroll one page left.
		if (curpos > minpos)
			curpos = max(minpos, curpos - (int)nPage);
		break;

	case SB_PAGERIGHT:      // Scroll one page right.
		if (curpos < maxpos)
			curpos = min(maxpos, curpos + (int)nPage);
		break;

	case SB_THUMBPOSITION: // Scroll to absolute position. nPos is the position
		curpos = nPos;       // of the scroll box at the end of the drag operation.
		break;

	case SB_THUMBTRACK:  // Drag scroll box to specified position. nPos is the
		curpos = nPos;     // position that the scroll box has been dragged to.
		break;
	}

	// Set the new position of the thumb (scroll box).
	SetScrollPos(SB_HORZ, curpos);

	//m_scrollOffset.x = curpos;
	Invalidate();

	CWnd::OnHScroll(nSBCode, nPos, pScrollBar);
}

//////////////////////////////////////////////////////////////////////////
ISplineInterpolator* CSplineCtrlEx::HitSpline(CPoint point)
{
	if (HitTest(point) != HIT_NOTHING)
	{
		return m_pHitSpline;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////////
CSplineCtrlEx::EHitCode CSplineCtrlEx::HitTest(CPoint point)
{
	float time, val;
	int nTotalNumberOfDimensions(0);
	int nCurrentDimension(0);

	PointToTimeValue(point, time, val);

	m_hitCode = HIT_NOTHING;
	m_pHitSpline = NULL;
	m_pHitDetailSpline = NULL;
	m_nHitKeyIndex = -1;
	m_nHitDimension = -1;
	m_bHitIncomingHandle = true;

	if (abs(point.x - TimeToXOfs(m_fTimeMarker)) < 4)
	{
		m_hitCode = HIT_TIMEMARKER;
	}

	// For each Spline...
	for (int splineIndex = 0, splineCount = m_splines.size(); splineIndex < splineCount; ++splineIndex)
	{
		ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline;
		ISplineInterpolator* pDetailSpline = m_splines[splineIndex].pDetailSpline;

		// If there is no spline, you can't hit nor a spline nor a key... isn't that logical?
		if (!pSpline)
		{
			return m_hitCode;
		}

		ISplineInterpolator::ValueType stSplineValue;
		ISplineInterpolator::ValueType stDetailSplineValue;
		ISplineInterpolator::ZeroValue(stSplineValue);
		ISplineInterpolator::ZeroValue(stDetailSplineValue);

		pSpline->Interpolate(time, stSplineValue);

		if (pDetailSpline)
		{
			pDetailSpline->Interpolate(time, stDetailSplineValue);
		}

		// For each dimension...
		nTotalNumberOfDimensions = pSpline->GetNumDimensions();
		for (nCurrentDimension = 0; nCurrentDimension < nTotalNumberOfDimensions; nCurrentDimension++)
		{
			if (pDetailSpline)
			{
				stSplineValue[nCurrentDimension] = stSplineValue[nCurrentDimension] + stDetailSplineValue[nCurrentDimension];
			}

			for (int i = 0; i < pSpline->GetKeyCount(); i++)
			{
				if (pSpline->IsKeySelectedAtDimension(i, nCurrentDimension))
				// Check tangent handles first.
				{
					CPoint incomingHandlePt, outgoingHandlePt, pt;
					if (GetTangentHandlePts(incomingHandlePt, pt, outgoingHandlePt, splineIndex, i, nCurrentDimension))
					{
						// For the incoming handle
						if (abs(incomingHandlePt.x - point.x) < 4 && abs(incomingHandlePt.y - point.y) < 4)
						{
							m_hitCode = HIT_TANGENT_HANDLE;
							m_pHitSpline = pSpline;
							m_pHitDetailSpline = m_splines[splineIndex].pDetailSpline;
							m_nHitKeyIndex = i;
							m_nHitDimension = nCurrentDimension;
							m_bHitIncomingHandle = true;
							return m_hitCode;
						}
						// For the outgoing handle
						else if (abs(outgoingHandlePt.x - point.x) < 4 && abs(outgoingHandlePt.y - point.y) < 4)
						{
							m_hitCode = HIT_TANGENT_HANDLE;
							m_pHitSpline = pSpline;
							m_pHitDetailSpline = m_splines[splineIndex].pDetailSpline;
							m_nHitKeyIndex = i;
							m_nHitDimension = nCurrentDimension;
							m_bHitIncomingHandle = false;
							return m_hitCode;
						}
					}
				}
			}

			CPoint splinePt = WorldToClient(Vec2(time, stSplineValue[nCurrentDimension]));
			;
			bool bSplineHit = abs(splinePt.x - point.x) < 4 && abs(splinePt.y - point.y) < 4;

			if (bSplineHit)
			{
				m_hitCode = HIT_SPLINE;
				m_pHitSpline = pSpline;
				m_pHitDetailSpline = m_splines[splineIndex].pDetailSpline;
				for (int i = 0; i < pSpline->GetKeyCount(); i++)
				{
					CPoint splinePt = TimeToPoint(pSpline->GetKeyTime(i), pSpline);
					if (abs(splinePt.x - point.x) < 4 /* && abs(splinePt.y-point.y) < 4*/)
					{
						m_nHitKeyIndex = i;
						m_nHitDimension = nCurrentDimension;
						m_hitCode = HIT_KEY;
						return m_hitCode;
					}
				}
			}
		}
	}

	return m_hitCode;
}

///////////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::StartTracking(bool copyKeys)
{
	m_copyKeys = copyKeys;
	m_startedDragging = false;

	m_editMode = TrackingMode;
	SetCapture();

	GetIEditor()->GetIUndoManager()->Begin();

	SendNotifyEvent(SPLN_BEFORE_CHANGE);

	HCURSOR hCursor;
	hCursor = AfxGetApp()->LoadCursor(IDC_ARRBLCKCROSS);
	SetCursor(hCursor);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::StopTracking()
{
	if ((m_editMode == TrackingMode) && (m_cMousePos != m_cMouseDownPos))
	{
		GetIEditor()->GetIUndoManager()->Accept("Spline Move");
	}
	else if ((m_editMode == SelectMode) || ((m_editMode == TrackingMode) && (m_cMousePos == m_cMouseDownPos)))
	{
		if (m_pCurrentUndo && m_pCurrentUndo->IsSelectionChanged())
		{
			GetIEditor()->GetIUndoManager()->Accept("Key Selection");
		}
	}
	else
	{
		GetIEditor()->GetIUndoManager()->Cancel();
	}

	m_editMode = NothingMode;
	ReleaseCapture();
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::ScaleAmplitudeKeys(float time, float startValue, float offset)
{
	//TODO: Test it in the facial animation pane and fix it...
	m_pHitSpline = 0;
	m_pHitDetailSpline = 0;
	m_nHitKeyIndex = -1;
	m_nHitDimension = -1;

	for (int splineIndex = 0, splineCount = m_splines.size(); splineIndex < splineCount; ++splineIndex)
	{
		ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline;

		// Find the range of keys to process.
		int keyCount = (pSpline ? pSpline->GetKeyCount() : 0);
		int firstKeyIndex = keyCount;
		int lastKeyIndex = -1;
		for (int i = 0; i < keyCount; ++i)
		{
			if (pSpline->IsKeySelectedAtAnyDimension(i))
			{
				firstKeyIndex = min(firstKeyIndex, i);
				lastKeyIndex = max(lastKeyIndex, i);
			}
		}

		// Find the parameters of a line between the start and end points. This will form the centre line
		// around which the amplitude of the keys will be scaled.
		float rangeStartTime = (firstKeyIndex >= 0 && pSpline ? pSpline->GetKeyTime(firstKeyIndex) : 0.0f);
		float rangeEndTime = (lastKeyIndex >= 0 && pSpline ? pSpline->GetKeyTime(lastKeyIndex) : 0.0f);
		float rangeLength = max(0.01f, rangeEndTime - rangeStartTime);

		for (int nCurrentDimension = 0; nCurrentDimension < pSpline->GetNumDimensions(); nCurrentDimension++)
		{
			ISplineInterpolator::ValueType afRangeStartValue;
			if (firstKeyIndex >= 0 && pSpline)
			{
				pSpline->GetKeyValue(firstKeyIndex, afRangeStartValue);
			}
			else
			{
				memset(afRangeStartValue, 0, sizeof(ISplineInterpolator::ValueType));
			}

			ISplineInterpolator::ValueType afRangeEndValue;
			if (lastKeyIndex >= 0 && pSpline)
			{
				pSpline->GetKeyValue(lastKeyIndex, afRangeEndValue);
			}
			else
			{
				memset(afRangeEndValue, 0, sizeof(ISplineInterpolator::ValueType));
			}
			float centreM = (afRangeEndValue[nCurrentDimension] - afRangeStartValue[nCurrentDimension]) / rangeLength;
			float centreC = afRangeStartValue[nCurrentDimension] - centreM * rangeStartTime;
			// Calculate the scale factor, based on how the mouse was dragged.
			float dragCentreValue = centreM * time + centreC;
			float dragCentreOffset = startValue - dragCentreValue;
			float offsetScale = (fabs(dragCentreOffset) > 0.001 ? (offset + dragCentreOffset) / dragCentreOffset : 1.0f);
			// Scale all the selected keys around this central line.
			for (int i = 0; i < keyCount; ++i)
			{
				if (pSpline->IsKeySelectedAtDimension(i, nCurrentDimension))
				{
					float keyTime = (pSpline ? pSpline->GetKeyTime(i) : 0.0f);
					float centreValue = keyTime * centreM + centreC;
					ISplineInterpolator::ValueType afKeyValue;
					if (pSpline)
					{
						pSpline->GetKeyValue(i, afKeyValue);
					}
					else
					{
						memset(afKeyValue, 0, sizeof(ISplineInterpolator::ValueType));
					}
					float keyOffset = afKeyValue[nCurrentDimension] - centreValue;
					float newKeyOffset = keyOffset * offsetScale;
					if (pSpline)
					{
						afKeyValue[nCurrentDimension] = centreValue + newKeyOffset;
						pSpline->SetKeyValue(i, afKeyValue);
					}
				}
			}
		}
	}

	RedrawWindow(0);
	if (m_pTimelineCtrl)
		m_pTimelineCtrl->Invalidate();
	SendNotifyEvent(SPLN_CHANGE);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::TimeScaleKeys(float time, float startTime, float endTime)
{
	// Calculate the scaling parameters (ie t1 = t0 * M + C).
	float timeScaleM = 1.0f;
	if (fabsf(startTime - time) > MIN_TIME_EPSILON_FOR_SCALING)
		timeScaleM = (endTime - time) / (startTime - time);
	float timeScaleC = endTime - startTime * timeScaleM;

	// Loop through all keys that are selected.
	m_pHitSpline = 0;
	m_pHitDetailSpline = 0;
	m_nHitKeyIndex = -1;

	float affectedRangeMin = FLT_MAX;
	float affectedRangeMax = -FLT_MAX;
	for (int splineIndex = 0, splineCount = m_splines.size(); splineIndex < splineCount; ++splineIndex)
	{
		ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline;

		int keyCount = pSpline->GetKeyCount();
		float keyRangeMin = FLT_MAX;
		float keyRangeMax = -FLT_MAX;
		for (int i = 0; i < keyCount; i++)
		{
			if (pSpline->IsKeySelectedAtAnyDimension(i))
			{
				float oldTime = pSpline->GetKeyTime(i);
				float t = SnapTime(oldTime * timeScaleM + timeScaleC);

				pSpline->SetKeyTime(i, SnapTimeToGridVertical(t));

				keyRangeMin = min(keyRangeMin, oldTime);
				keyRangeMin = min(keyRangeMin, t);
				keyRangeMax = max(keyRangeMax, oldTime);
				keyRangeMax = max(keyRangeMax, t);
			}
		}
		if (keyRangeMin <= keyRangeMax)
		{
			// Changes to a key's value affect spline up to two keys away.
			int lastMovedKey = 0;
			for (int keyIndex = 0; keyIndex < keyCount; ++keyIndex)
			{
				if (pSpline->GetKeyTime(keyIndex) <= keyRangeMax)
					lastMovedKey = keyIndex + 1;
			}
			int firstMovedKey = pSpline->GetKeyCount();
			for (int keyIndex = pSpline->GetKeyCount() - 1; keyIndex >= 0; --keyIndex)
			{
				if (pSpline->GetKeyTime(keyIndex) >= keyRangeMin)
					firstMovedKey = keyIndex;
			}

			int firstAffectedKey = max(0, firstMovedKey - 2);
			int lastAffectedKey = min(keyCount - 1, lastMovedKey + 2);

			affectedRangeMin = min(affectedRangeMin, (firstAffectedKey <= 0 ? m_timeRange.start : pSpline->GetKeyTime(firstAffectedKey)));
			affectedRangeMax = max(affectedRangeMax, (lastAffectedKey >= keyCount - 1 ? m_timeRange.end : pSpline->GetKeyTime(lastAffectedKey)));

			// Loop through all moved keys, checking whether there are multiple keys on the same frame.
			float lastKeyTime = -FLT_MAX;
			pSpline->Update();
			for (int keyIndex = 0, keys = pSpline->GetKeyCount(); keyIndex <= keys; )
			{
				float keyTime = pSpline->GetKeyTime(keyIndex);
				if (fabsf(keyTime - lastKeyTime) < m_fMinTimeEpsilon)
				{
					--keys;
					pSpline->RemoveKey(keyIndex);
				}
				else
				{
					++keyIndex;
					lastKeyTime = keyTime;
				}
			}
		}
	}

	int rangeMin = TimeToXOfs(affectedRangeMin);
	int rangeMax = TimeToXOfs(affectedRangeMax);

	if (m_timeRange.start == affectedRangeMin)
		rangeMin = m_rcSpline.left;
	if (m_timeRange.end == affectedRangeMax)
		rangeMax = m_rcSpline.right;

	CRect invalidRect(rangeMin - 3, m_rcSpline.top, rangeMax + 3, m_rcSpline.bottom);
	RedrawWindow(&invalidRect);
	if (m_pTimelineCtrl)
		m_pTimelineCtrl->RedrawWindow();

	m_bKeyTimesDirty = true;
	SendNotifyEvent(SPLN_CHANGE);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::ValueScaleKeys(float startValue, float endValue)
{
	// Calculate the scaling parameters.
	float valueScale = 1.0f;
	if (fabsf(startValue) > MIN_TIME_EPSILON_FOR_SCALING)
		valueScale = endValue / startValue;

	// Loop through all keys that are selected.
	m_pHitSpline = 0;
	m_pHitDetailSpline = 0;
	m_nHitKeyIndex = -1;
	m_nHitDimension = -1;

	for (int splineIndex = 0, splineCount = m_splines.size(); splineIndex < splineCount; ++splineIndex)
	{
		ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline;

		int keyCount = pSpline->GetKeyCount();
		for (int i = 0; i < keyCount; i++)
		{
			for (int nCurrentDimension = 0; nCurrentDimension < pSpline->GetNumDimensions(); nCurrentDimension++)
			{
				if (pSpline->IsKeySelectedAtDimension(i, nCurrentDimension))
				{
					ISplineInterpolator::ValueType afValue;
					pSpline->GetKeyValue(i, afValue);

					afValue[nCurrentDimension] = SnapValue(afValue[nCurrentDimension] * valueScale);
					pSpline->SetKeyValue(i, afValue);
				}
			}
		}
	}

	RedrawWindow(0);
	SendNotifyEvent(SPLN_CHANGE);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::MoveSelectedKeys(Vec2 offset, bool copyKeys)
{
	m_pHitSpline = 0;
	m_pHitDetailSpline = 0;
	m_nHitKeyIndex = -1;
	m_nHitDimension = -1;

	if (copyKeys)
	{
		DuplicateSelectedKeys();
	}

	float affectedRangeMin = FLT_MAX;
	float affectedRangeMax = -FLT_MAX;
	// For each spline...
	for (int splineIndex = 0, splineCount = m_splines.size(); splineIndex < splineCount; ++splineIndex)
	{
		ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline;

		int keyCount = pSpline->GetKeyCount();
		float keyRangeMin = FLT_MAX;
		float keyRangeMax = -FLT_MAX;
		for (int i = 0; i < keyCount; i++)
		{
			float oldTime = pSpline->GetKeyTime(i);
			float t = SnapTime(oldTime + offset.x);

			if (pSpline->IsKeySelectedAtAnyDimension(i))
			{
				if (pSpline->FindKey(t, m_fMinTimeEpsilon) < 0)
				{
					pSpline->SetKeyTime(i, SnapTimeToGridVertical(t));
				}

				keyRangeMin = min(keyRangeMin, oldTime);
				keyRangeMin = min(keyRangeMin, t);
				keyRangeMax = max(keyRangeMax, oldTime);
				keyRangeMax = max(keyRangeMax, t);
			}

			for (int nCurrentDimension = 0; nCurrentDimension < pSpline->GetNumDimensions(); nCurrentDimension++)
			{
				if (pSpline->IsKeySelectedAtDimension(i, nCurrentDimension))
				{
					ISplineInterpolator::ValueType afValue;
					pSpline->GetKeyValue(i, afValue);

					afValue[nCurrentDimension] = SnapValue(afValue[nCurrentDimension] + offset.y);
					pSpline->SetKeyValue(i, afValue);
				}
			}
		}
		if (keyRangeMin <= keyRangeMax)
		{
			// Changes to a key's value affect spline up to two keys away.
			int lastMovedKey = 0;
			for (int keyIndex = 0; keyIndex < keyCount; ++keyIndex)
			{
				if (pSpline->GetKeyTime(keyIndex) <= keyRangeMax)
					lastMovedKey = keyIndex + 1;
			}
			int firstMovedKey = pSpline->GetKeyCount();
			for (int keyIndex = pSpline->GetKeyCount() - 1; keyIndex >= 0; --keyIndex)
			{
				if (pSpline->GetKeyTime(keyIndex) >= keyRangeMin)
					firstMovedKey = keyIndex;
			}

			int firstAffectedKey = max(0, firstMovedKey - 2);
			int lastAffectedKey = min(keyCount - 1, lastMovedKey + 2);

			affectedRangeMin = min(affectedRangeMin, (firstAffectedKey <= 0 ? m_timeRange.start : pSpline->GetKeyTime(firstAffectedKey)));
			affectedRangeMax = max(affectedRangeMax, (lastAffectedKey >= keyCount - 1 ? m_timeRange.end : pSpline->GetKeyTime(lastAffectedKey)));
		}
	}

	int rangeMin = TimeToXOfs(affectedRangeMin);
	int rangeMax = TimeToXOfs(affectedRangeMax);

	if (m_timeRange.start == affectedRangeMin)
		rangeMin = m_rcSpline.left;
	if (m_timeRange.end == affectedRangeMax)
		rangeMax = m_rcSpline.right;

	if (m_pTimelineCtrl)
		m_pTimelineCtrl->RedrawWindow();

	m_bKeyTimesDirty = true;
	SendNotifyEvent(SPLN_CHANGE);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::RemoveKey(ISplineInterpolator* pSpline, int nKey)
{
	CUndo undo("Remove Spline Key");
	ConditionalStoreUndo();

	m_bKeyTimesDirty = true;

	SendNotifyEvent(SPLN_BEFORE_CHANGE);

	m_pHitSpline = 0;
	m_pHitDetailSpline = 0;
	m_nHitKeyIndex = -1;
	pSpline->RemoveKey(nKey);

	SendNotifyEvent(SPLN_CHANGE);
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::RemoveSelectedKeys()
{
	CUndo undo("Remove Spline Key");
	StoreUndo();

	SendNotifyEvent(SPLN_BEFORE_CHANGE);

	m_pHitSpline = 0;
	m_pHitDetailSpline = 0;
	m_nHitKeyIndex = -1;

	for (int splineIndex = 0, splineCount = m_splines.size(); splineIndex < splineCount; ++splineIndex)
	{
		ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline;

		for (int i = 0; i < (int)pSpline->GetKeyCount(); )
		{
			if (pSpline->IsKeySelectedAtAnyDimension(i))
				pSpline->RemoveKey(i);
			else
				i++;
		}
	}

	m_bKeyTimesDirty = true;
	SendNotifyEvent(SPLN_CHANGE);
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::RemoveSelectedKeyTimesImpl()
{
	int numSelectedKeyTimes = 0;
	for (std::vector<KeyTime>::iterator it = m_keyTimes.begin(), end = m_keyTimes.end(); it != end; ++it)
	{
		if ((*it).selected)
			++numSelectedKeyTimes;
	}

	if (numSelectedKeyTimes)
	{
		CUndo undo("Remove Spline Key");
		StoreUndo();
		SendNotifyEvent(SPLN_BEFORE_CHANGE);

		for (int splineIndex = 0, end = m_splines.size(); splineIndex < end; ++splineIndex)
		{
			std::vector<KeyTime>::iterator itTime = m_keyTimes.begin(), endTime = m_keyTimes.end();
			for (int keyIndex = 0, endIndex = m_splines[splineIndex].pSpline->GetKeyCount(); keyIndex < endIndex; )
			{
				const float threshold = 0.01f;
				for (; itTime != endTime && (*itTime).time < m_splines[splineIndex].pSpline->GetKeyTime(keyIndex) - threshold; ++itTime)
					;
				if (itTime != endTime && fabsf((*itTime).time - m_splines[splineIndex].pSpline->GetKeyTime(keyIndex)) < threshold && (*itTime).selected)
					m_splines[splineIndex].pSpline->RemoveKey(keyIndex);
				else
					++keyIndex;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::RemoveSelectedKeyTimes()
{
	RemoveSelectedKeyTimesImpl();

	m_bKeyTimesDirty = true;
	SendNotifyEvent(SPLN_CHANGE);
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::RedrawWindowAroundMarker()
{
	UpdateKeyTimes();
	std::vector<KeyTime>::iterator itKeyTime = std::lower_bound(m_keyTimes.begin(), m_keyTimes.end(), KeyTime(m_fTimeMarker, 0));
	int keyTimeIndex = (itKeyTime != m_keyTimes.end() ? itKeyTime - m_keyTimes.begin() : m_keyTimes.size());
	int redrawRangeStart = (keyTimeIndex >= 2 ? TimeToXOfs(m_keyTimes[keyTimeIndex - 2].time) : m_rcSpline.left);
	int redrawRangeEnd = (keyTimeIndex < int(m_keyTimes.size()) - 2 ? TimeToXOfs(m_keyTimes[keyTimeIndex + 2].time) : m_rcSpline.right);

	CRect rc = CRect(redrawRangeStart, m_rcSpline.top, redrawRangeEnd, m_rcSpline.bottom);
	rc.NormalizeRect();
	rc.IntersectRect(rc, m_rcSpline);

	InvalidateRect(&rc, FALSE);
	m_TimeUpdateRect = CRect(1, 2, 3, 4);
	RedrawWindow(&rc, NULL, RDW_UPDATENOW);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::SplinesChanged()
{
	m_bKeyTimesDirty = true;
	UpdateKeyTimes();
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::SetControlAmplitude(bool controlAmplitude)
{
	m_controlAmplitude = controlAmplitude;
}

//////////////////////////////////////////////////////////////////////////
bool CSplineCtrlEx::GetControlAmplitude() const
{
	return m_controlAmplitude;
}

float CSplineCtrlEx::SnapTimeToGridVertical(float time)
{
	//float fSnapTime = int((time * m_fGridTimeScale) + 0.5f) * (1.0f / m_fGridTimeScale);
	float fSnapTime = time;
	return fSnapTime;
}

//////////////////////////////////////////////////////////////////////////
int CSplineCtrlEx::InsertKey(ISplineInterpolator* pSpline, ISplineInterpolator* pDetailSpline, CPoint point)
{
	CUndo undo("Spline Insert Key");
	StoreUndo();

	float time, val;
	PointToTimeValue(point, time, val);

	time = SnapTimeToGridVertical(time);

	int i;
	for (i = 0; i < pSpline->GetKeyCount(); i++)
	{
		// Skip if any key already have time that is very close.
		if (fabs(pSpline->GetKeyTime(i) - time) < m_fMinTimeEpsilon)
			return i;
	}

	SendNotifyEvent(SPLN_BEFORE_CHANGE);

	// The proper key value for a spline that has a detail spline is not what is shown in the control - we have
	// to remove the detail value to get back to the underlying spline value.
	if (pDetailSpline)
	{
		float offset = 0.0f;
		pDetailSpline->InterpolateFloat(time, offset);
		val -= offset;
	}

	ClearSelection();
	ISplineInterpolator::ValueType currValue;
	ISplineInterpolator::ZeroValue(currValue);

	pSpline->Interpolate(time, currValue);

	if (pSpline->GetNumDimensions() > 1)
	{

	}

	int nKey = pSpline->InsertKey(time, currValue);  // TODO: Don't use FE specific snapping!
	if (m_defaultKeyTangentType != SPLINE_KEY_TANGENT_NONE)
	{
		SetKeyTangentType(pSpline, nKey, m_defaultKeyTangentType);
	}

	//int nKey = pSpline->InsertKeyFloat( time,val ); // TODO: Don't use FE specific snapping!
	SelectKey(pSpline, nKey, 0, true);
	Invalidate();

	m_bKeyTimesDirty = true;

	SendNotifyEvent(SPLN_CHANGE);

	return nKey;
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::ClearSelection()
{
	ConditionalStoreUndo();

	for (int splineIndex = 0, splineCount = m_splines.size(); splineIndex < splineCount; ++splineIndex)
	{
		ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline;

		for (int i = 0; i < (int)pSpline->GetKeyCount(); i++)
		{
			pSpline->SelectKeyAllDimensions(i, false);
		}
	}

	ClearSelectedKeys();
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::SetTimeMarker(float fTime)
{
	if (m_pTimelineCtrl)
		m_pTimelineCtrl->SetTimeMarker(fTime);

	if (fTime == m_fTimeMarker)
		return;

	// Erase old first.
	float x1 = TimeToXOfs(m_fTimeMarker);
	float x2 = TimeToXOfs(fTime);
	CRect rc = CRect(x1, m_rcSpline.top, x2, m_rcSpline.bottom);
	rc.NormalizeRect();
	rc.InflateRect(3, 0);
	rc.IntersectRect(rc, m_rcSpline);

	m_TimeUpdateRect = rc;
	InvalidateRect(rc);

	m_fTimeMarker = fTime;
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::StoreUndo()
{
	if (CUndo::IsRecording() && !m_pCurrentUndo)
	{
		std::vector<ISplineInterpolator*> splines(m_splines.size());
		for (int splineIndex = 0, splineCount = m_splines.size(); splineIndex < splineCount; ++splineIndex)
			splines[splineIndex] = m_splines[splineIndex].pSpline;
		CUndo::Record(m_pCurrentUndo = CreateSplineCtrlUndoObject(splines));
	}
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::ConditionalStoreUndo()
{
	if (m_editMode == TrackingMode || m_editMode == SelectMode)
		StoreUndo();
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::ClearSelectedKeys()
{
	for (std::vector<KeyTime>::iterator it = m_keyTimes.begin(), end = m_keyTimes.end(); it != end; ++it)
		(*it).selected = false;
}

//////////////////////////////////////////////////////////////////////////
class CKeyCopyInfo
{
public:
	ISplineInterpolator::ValueType value;
	float                          time;
	int                            flags;
	ISplineInterpolator::ValueType tin, tout;
};
void CSplineCtrlEx::DuplicateSelectedKeys()
{
	m_pHitSpline = 0;
	m_pHitDetailSpline = 0;
	m_nHitKeyIndex = -1;

	typedef std::vector<CKeyCopyInfo> KeysToAddContainer;
	KeysToAddContainer keysToInsert;
	for (int splineIndex = 0, splineCount = m_splines.size(); splineIndex < splineCount; ++splineIndex)
	{
		ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline;

		keysToInsert.resize(0);
		for (int i = 0; i < pSpline->GetKeyCount(); i++)
		{
			// In this particular case, the dimension doesn't matter.
			if (pSpline->IsKeySelectedAtAnyDimension(i))
			{
				keysToInsert.resize(keysToInsert.size() + 1);
				CKeyCopyInfo& copyInfo = keysToInsert.back();

				copyInfo.time = pSpline->GetKeyTime(i);
				pSpline->GetKeyValue(i, copyInfo.value);
				pSpline->GetKeyTangents(i, copyInfo.tin, copyInfo.tout);
				copyInfo.flags = pSpline->GetKeyFlags(i);
			}
		}

		for (KeysToAddContainer::iterator keyToAdd = keysToInsert.begin(), endKeysToAdd = keysToInsert.end(); keyToAdd != endKeysToAdd; ++keyToAdd)
		{
			int keyIndex = pSpline->InsertKey(SnapTimeToGridVertical((*keyToAdd).time), (*keyToAdd).value);
			pSpline->SetKeyTangents(keyIndex, (*keyToAdd).tin, (*keyToAdd).tout);
			pSpline->SetKeyFlags(keyIndex, (*keyToAdd).flags & (~ESPLINE_KEY_UI_SELECTED_MASK));
		}
	}

	m_bKeyTimesDirty = true;
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::ZeroAll()
{
	GetIEditor()->GetIUndoManager()->Begin();

	typedef std::vector<ISplineInterpolator*> SplineContainer;
	SplineContainer splines;
	for (int splineIndex = 0; splineIndex < int(m_splines.size()); ++splineIndex)
	{
		ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline;
		int keyIndex = (pSpline ? pSpline->FindKey(m_fTimeMarker, 0.015f) : -1);
		if (pSpline && keyIndex >= 0)
			splines.push_back(pSpline);
	}

	CUndo::Record(CreateSplineCtrlUndoObject(splines));

	for (SplineContainer::iterator itSpline = splines.begin(); itSpline != splines.end(); ++itSpline)
	{
		int keyIndex = ((*itSpline) ? (*itSpline)->FindKey(m_fTimeMarker, 0.015f) : -1);
		if ((*itSpline) && keyIndex >= 0)
			(*itSpline)->SetKeyValueFloat(keyIndex, 0.0f);
	}

	GetIEditor()->GetIUndoManager()->Accept("Zero All");
	m_pCurrentUndo = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::KeyAll()
{
	GetIEditor()->GetIUndoManager()->Begin();

	typedef std::vector<ISplineInterpolator*> SplineContainer;
	SplineContainer splines;
	for (int splineIndex = 0; splineIndex < int(m_splines.size()); ++splineIndex)
	{
		ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline;
		int keyIndex = (pSpline ? pSpline->FindKey(m_fTimeMarker, 0.015f) : -1);
		if (pSpline && keyIndex == -1)
			splines.push_back(pSpline);
	}

	CUndo::Record(CreateSplineCtrlUndoObject(splines));

	for (SplineContainer::iterator itSpline = splines.begin(); itSpline != splines.end(); ++itSpline)
	{
		float value = 0.0f;
		(*itSpline)->InterpolateFloat(m_fTimeMarker, value);
		int keyIndex = (*itSpline)->InsertKeyFloat(SnapTimeToGridVertical(m_fTimeMarker), value);
		if (m_defaultKeyTangentType != SPLINE_KEY_TANGENT_NONE)
		{
			SetKeyTangentType(*itSpline, keyIndex, m_defaultKeyTangentType);
		}
	}

	GetIEditor()->GetIUndoManager()->Accept("Key All");
	m_pCurrentUndo = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::SelectAll()
{
	for (int splineIndex = 0, splineCount = m_splines.size(); splineIndex < splineCount; ++splineIndex)
	{
		ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline;
		ISplineInterpolator* pDetailSpline = m_splines[splineIndex].pDetailSpline;

		for (int i = 0; i < (int)pSpline->GetKeyCount(); i++)
			pSpline->SelectKeyAllDimensions(i, true);
	}
	RedrawWindow();
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::SendNotifyEvent(int nEvent)
{
	if (nEvent == SPLN_BEFORE_CHANGE)
	{
		ConditionalStoreUndo();
	}
	NMHDR nmh;
	nmh.hwndFrom = m_hWnd;
	nmh.idFrom = ::GetDlgCtrlID(m_hWnd);
	nmh.code = nEvent;

	GetOwner()->SendMessage(WM_NOTIFY, (WPARAM)GetDlgCtrlID(), (LPARAM)&nmh);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::SetTimelineCtrl(CTimelineCtrl* pTimelineCtrl)
{
	m_pTimelineCtrl = pTimelineCtrl;
	if (m_pTimelineCtrl)
	{
		CWnd* pOwner = pTimelineCtrl->GetOwner();
		m_pTimelineCtrl->SetParent(this);
		pTimelineCtrl->SetOwner(pOwner);
		m_pTimelineCtrl->SetZoom(m_grid.zoom.x);
		m_pTimelineCtrl->SetOrigin(m_grid.origin.x);
		m_pTimelineCtrl->SetKeyTimeSet(this);
		m_pTimelineCtrl->Invalidate();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::AddSpline(ISplineInterpolator* pSpline, ISplineInterpolator* pDetailSpline, COLORREF color)
{
	for (int i = 0; i < (int)m_splines.size(); i++)
	{
		if (m_splines[i].pSpline == pSpline)
			return;
	}
	SSplineInfo si;

	for (int nCurrentDimension = 0; nCurrentDimension < pSpline->GetNumDimensions(); nCurrentDimension++)
	{
		si.anColorArray[nCurrentDimension] = color;
	}

	si.pSpline = pSpline;
	si.pDetailSpline = pDetailSpline;
	m_splines.push_back(si);
	m_bKeyTimesDirty = true;
	Invalidate();
}

void CSplineCtrlEx::AddSpline(ISplineInterpolator* pSpline, ISplineInterpolator* pDetailSpline, COLORREF anColorArray[4])
{
	for (int i = 0; i < (int)m_splines.size(); i++)
	{
		if (m_splines[i].pSpline == pSpline)
			return;
	}
	SSplineInfo si;

	for (int nCurrentDimension = 0; nCurrentDimension < pSpline->GetNumDimensions(); nCurrentDimension++)
	{
		si.anColorArray[nCurrentDimension] = anColorArray[nCurrentDimension];
	}

	si.pSpline = pSpline;
	si.pDetailSpline = pDetailSpline;
	m_splines.push_back(si);
	m_bKeyTimesDirty = true;
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::RemoveSpline(ISplineInterpolator* pSpline)
{
	for (int i = 0; i < (int)m_splines.size(); i++)
	{
		if (m_splines[i].pSpline == pSpline)
		{
			m_splines.erase(m_splines.begin() + i);
			return;
		}
	}
	m_bKeyTimesDirty = true;
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::RemoveAllSplines()
{
	m_splines.clear();
	m_bKeyTimesDirty = true;
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::SelectRectangle(CRect rc, bool bSelect)
{
	ConditionalStoreUndo();

	ClearSelectedKeys();

	Vec2 vec0 = ClientToWorld(CPoint(rc.left, rc.top));
	Vec2 vec1 = ClientToWorld(CPoint(rc.right, rc.bottom));
	float t0 = vec0.x;
	float t1 = vec1.x;
	float v0 = vec0.y;
	float v1 = vec1.y;
	if (v0 > v1)
		std::swap(v0, v1);
	if (t0 > t1)
		std::swap(t0, t1);
	for (int splineIndex = 0, splineCount = m_splines.size(); splineIndex < splineCount; ++splineIndex)
	{
		ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline;
		ISplineInterpolator* pDetailSpline = m_splines[splineIndex].pDetailSpline;

		for (int i = 0; i < (int)pSpline->GetKeyCount(); i++)
		{
			float t = pSpline->GetKeyTime(i);
			ISplineInterpolator::ValueType afValue;
			pSpline->GetKeyValue(i, afValue);

			int nTotalNumberOfDimensions(pSpline->GetNumDimensions());

			ISplineInterpolator::ValueType afDetailValue;
			if (pDetailSpline)
			{
				ISplineInterpolator::ZeroValue(afDetailValue);
				pDetailSpline->Interpolate(t, afDetailValue);
			}

			for (int nCurrentDimension = 0; nCurrentDimension < nTotalNumberOfDimensions; nCurrentDimension++)
			{
				if (pDetailSpline)
				{
					afValue[nCurrentDimension] = afValue[nCurrentDimension] + afDetailValue[nCurrentDimension];
				}
				if (t >= t0 && t <= t1 && afValue[nCurrentDimension] >= v0 && afValue[nCurrentDimension] <= v1)
				{
					pSpline->SelectKeyAtDimension(i, nCurrentDimension, bSelect);
				}
			}
		}
	}
	SendNotifyEvent(SPLN_CHANGE);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::CopyKeys()
{
	// Copy selected keys.
	if (m_splines.empty() || GetNumSelected() == 0)
		return;

	XmlNodeRef rootNode = XmlHelpers::CreateXmlNode("SplineKeys");

	int i;
	float minTime = FLT_MAX;
	float maxTime = -FLT_MAX;

	ISplineInterpolator* pSpline = m_splines[0].pSpline;

	for (i = 0; i < (int)pSpline->GetKeyCount(); i++)
	{
		if (!pSpline->IsKeySelectedAtAnyDimension(i))
			continue;
		float t = pSpline->GetKeyTime(i);
		if (t < minTime)
			minTime = t;
		if (t > maxTime)
			maxTime = t;
	}

	rootNode->setAttr("start", minTime);
	rootNode->setAttr("end", maxTime);

	for (i = 0; i < (int)pSpline->GetKeyCount(); i++)
	{
		if (!pSpline->IsKeySelectedAtAnyDimension(i))
			continue;

		float t = pSpline->GetKeyTime(i); // Store offset time from copy/paste range.
		ISplineInterpolator::ValueType afValue;
		pSpline->GetKeyValue(i, afValue);

		float tin, tout;
		ISplineInterpolator::ValueType vtin, vtout;
		pSpline->GetKeyTangents(i, vtin, vtout);
		tin = vtin[0];
		tout = vtout[0];

		XmlNodeRef keyNode = rootNode->newChild("Key");
		keyNode->setAttr("time", t);
		keyNode->setAttr("flags", (int)pSpline->GetKeyFlags(i));
		keyNode->setAttr("in", tin);
		keyNode->setAttr("out", tout);

		for (int i = 0; i < pSpline->GetNumDimensions(); ++i)
		{
			XmlNodeRef dimensionNode = keyNode->newChild("values");
			dimensionNode->setAttr("value", afValue[i]);
		}
	}

	CClipboard clipboard;
	clipboard.Put(rootNode);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::PasteKeys()
{
	if (m_splines.empty() || GetNumSelected() == 0)
		return;

	ISplineInterpolator* pSpline = m_splines[0].pSpline;

	CClipboard clipboard;
	if (clipboard.IsEmpty())
		return;

	XmlNodeRef rootNode = clipboard.Get();
	if (!rootNode)
		return;
	if (!rootNode->isTag("SplineKeys"))
		return;

	float minTime = 0;
	float maxTime = 0;
	rootNode->getAttr("start", minTime);
	rootNode->getAttr("end", maxTime);

	CPoint point;
	GetCursorPos(&point);
	ScreenToClient(&point);
	float fTime = XOfsToTime(point.x);
	float fTimeRange = (maxTime - minTime);

	CUndo undo("Paste Spline Keys");

	ConditionalStoreUndo();

	ClearSelection();

	int i;
	// Delete keys in range min to max time.
	for (i = 0; i < pSpline->GetKeyCount(); )
	{
		float t = pSpline->GetKeyTime(i);
		if (t >= fTime && t <= fTime + fTimeRange)
		{
			pSpline->RemoveKey(i);
		}
		else
			i++;
	}

	for (i = 0; i < rootNode->getChildCount(); i++)
	{
		XmlNodeRef keyNode = rootNode->getChild(i);
		float t = 0;
		float tin = 0;
		float tout = 0;
		int flags = 0;

		keyNode->getAttr("time", t);
		keyNode->getAttr("flags", flags);
		keyNode->getAttr("in", tin);
		keyNode->getAttr("out", tout);

		int nNumberOfChildXMLNodes(0);
		int nCurrentChildXMLNode(0);
		int nCurrentValue(0);

		ISplineInterpolator::ValueType afValue;

		nNumberOfChildXMLNodes = keyNode->getChildCount();
		for (nCurrentChildXMLNode = 0; nCurrentChildXMLNode < nNumberOfChildXMLNodes; ++nCurrentChildXMLNode)
		{
			XmlNodeRef rSubKeyNode = keyNode->getChild(nCurrentChildXMLNode);
			if (rSubKeyNode->isTag("values"))
			{
				rSubKeyNode->getAttr("value", afValue[nCurrentValue]);
				nCurrentValue++;
			}
		}

		int key = pSpline->InsertKey(SnapTimeToGridVertical(t - minTime + fTime), afValue);
		if (key >= 0)
		{
			pSpline->SelectKeyAllDimensions(key, true);
			ISplineInterpolator::ValueType vtin, vtout;
			vtin[0] = tin;
			vtout[0] = tout;
			pSpline->SetKeyTangents(key, vtin, vtout);
		}
	}
	m_bKeyTimesDirty = true;
	RedrawWindow();
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::ModifySelectedKeysFlags(int nRemoveFlags, int nAddFlags)
{
	CUndo undo("Modify Spline Keys");
	StoreUndo();

	SendNotifyEvent(SPLN_BEFORE_CHANGE);

	for (int splineIndex = 0, splineCount = m_splines.size(); splineIndex < splineCount; ++splineIndex)
	{
		ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline;

		for (int i = 0; i < (int)pSpline->GetKeyCount(); i++)
		{
			// If the key is selected in any dimension...
			for (
			  int nCurrentDimension = 0;
			  nCurrentDimension < pSpline->GetNumDimensions();
			  nCurrentDimension++
			  )
			{
				if (IsKeySelected(pSpline, i, nCurrentDimension))
				{
					int flags = pSpline->GetKeyFlags(i);
					flags &= ~nRemoveFlags;
					flags |= nAddFlags;

					pSpline->SetKeyFlags(i, flags);
					break;
				}
			}
		}
	}

	SendNotifyEvent(SPLN_CHANGE);
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::FitSplineToViewWidth()
{
	// Calculate time zoom so that whole time range fits.
	float t0 = FLT_MAX;
	float t1 = -FLT_MAX;

	bool bAnyKey = false;
	for (int i = 0; i < int(m_splines.size()); ++i)
	{
		//////////////////////////////////////////////////////////////////////////
		ISplineInterpolator* pSpline = m_splines[i].pSpline;
		if (!pSpline)
			continue;

		for (int keyIndex = 0; pSpline && keyIndex < pSpline->GetKeyCount(); ++keyIndex)
		{
			float keyTime = pSpline->GetKeyTime(keyIndex);
			t0 = std::min(t0, keyTime);
			t1 = std::max(t1, keyTime);
			bAnyKey = true;
		}
	}
	if (!bAnyKey)
	{
		t0 = m_timeRange.start;
		t1 = m_timeRange.end;
	}

	float zoom = abs(m_rcSpline.Width() - 20) / max(1.0f, fabs(t1 - t0));
	SetZoom(Vec2(zoom, m_grid.zoom.y));
	SetScrollOffset(Vec2(t0, m_grid.origin.y));
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::FitSplineToViewHeight()
{
	// Calculate time zoom so that whole value range fits.
	float vmin = FLT_MAX;
	float vmax = -FLT_MAX;

	bool bAnyKey = false;
	for (int i = 0; i < int(m_splines.size()); ++i)
	{
		//////////////////////////////////////////////////////////////////////////
		ISplineInterpolator* pSpline = m_splines[i].pSpline;
		if (!pSpline)
			continue;

		ISplineInterpolator::ValueType value;
		for (int keyIndex = 0; pSpline && keyIndex < pSpline->GetKeyCount(); ++keyIndex)
		{
			pSpline->GetKeyValue(keyIndex, value);
			for (int d = 0, numDim = pSpline->GetNumDimensions(); d < numDim; d++)
			{
				vmin = std::min(vmin, value[d]);
				vmax = std::max(vmax, value[d]);
				bAnyKey = true;
			}
		}
	}
	if (!bAnyKey)
	{
		vmin = m_defaultValueRange.start;
		vmax = m_defaultValueRange.end;
	}

	float zoom = abs(m_rcSpline.Height() - 40) / max(1.0f, fabs(vmax - vmin));
	SetZoom(Vec2(m_grid.zoom.x, zoom));
	SetScrollOffset(Vec2(m_grid.origin.x, vmin));
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::OnUserCommand(UINT cmd)
{
	switch (cmd)
	{
	case ID_TANGENT_IN_ZERO:
		ModifySelectedKeysFlags(SPLINE_KEY_TANGENT_IN_MASK, SPLINE_KEY_TANGENT_ZERO << SPLINE_KEY_TANGENT_IN_SHIFT);
		break;
	case ID_TANGENT_IN_STEP:
		ModifySelectedKeysFlags(SPLINE_KEY_TANGENT_IN_MASK, SPLINE_KEY_TANGENT_STEP << SPLINE_KEY_TANGENT_IN_SHIFT);
		break;
	case ID_TANGENT_IN_LINEAR:
		ModifySelectedKeysFlags(SPLINE_KEY_TANGENT_IN_MASK, SPLINE_KEY_TANGENT_LINEAR << SPLINE_KEY_TANGENT_IN_SHIFT);
		break;

	case ID_TANGENT_OUT_ZERO:
		ModifySelectedKeysFlags(SPLINE_KEY_TANGENT_OUT_MASK, SPLINE_KEY_TANGENT_ZERO << SPLINE_KEY_TANGENT_OUT_SHIFT);
		break;
	case ID_TANGENT_OUT_STEP:
		ModifySelectedKeysFlags(SPLINE_KEY_TANGENT_OUT_MASK, SPLINE_KEY_TANGENT_STEP << SPLINE_KEY_TANGENT_OUT_SHIFT);
		break;
	case ID_TANGENT_OUT_LINEAR:
		ModifySelectedKeysFlags(SPLINE_KEY_TANGENT_OUT_MASK, SPLINE_KEY_TANGENT_LINEAR << SPLINE_KEY_TANGENT_OUT_SHIFT);
		break;

	case ID_TANGENT_AUTO:
		ModifySelectedKeysFlags(SPLINE_KEY_TANGENT_IN_MASK | SPLINE_KEY_TANGENT_OUT_MASK, 0);
		break;

	case ID_SPLINE_FIT_X:
		FitSplineToViewWidth();
		break;
	case ID_SPLINE_FIT_Y:
		FitSplineToViewHeight();
		break;
	case ID_SPLINE_SNAP_GRID_X:
		SetSnapTime(!m_bSnapTime);
		break;
	case ID_SPLINE_SNAP_GRID_Y:
		SetSnapValue(!m_bSnapValue);
		break;
	case ID_SPLINE_PREVIOUS_KEY:
		GotoNextKey(true);
		break;
	case ID_SPLINE_NEXT_KEY:
		GotoNextKey(false);
		break;
	case ID_SPLINE_FLATTEN_ALL:
		RemoveAllKeysButThis();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::GotoNextKey(bool previousKey)
{
	if (GetNumSelected() == 1)
	{
		bool boFoundTheSelectedKey(false);

		for (int splineIndex = 0, endSpline = m_splines.size(); splineIndex < endSpline; ++splineIndex)
		{
			ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline;
			for (int i = 0; i < pSpline->GetKeyCount(); i++)
			{
				for (int nCurrentDimension = 0; nCurrentDimension < pSpline->GetNumDimensions(); nCurrentDimension++)
				{
					if (pSpline->IsKeySelectedAtDimension(i, nCurrentDimension))
					{
						boFoundTheSelectedKey = true;

						if ((previousKey && i > 0) || (!previousKey && i + 1 < pSpline->GetKeyCount()))
						{
							int nextKey = previousKey ? i - 1 : i + 1;
							float keyTime = pSpline->GetKeyTime(nextKey);

							SetTimeMarker(keyTime);

							ISplineInterpolator::ValueType afValue;
							pSpline->GetKeyValue(nextKey, afValue);

							pSpline->SelectKeyAtDimension(i, nCurrentDimension, false);
							pSpline->SelectKeyAtDimension(nextKey, nCurrentDimension, true);

							// Set the new scrolled coordinates
							float ofsx = keyTime - (m_grid.rect.right / 2) / m_grid.zoom.x;
							float ofsy = afValue[nCurrentDimension] - (m_grid.rect.bottom / 2) / m_grid.zoom.y;

							SetScrollOffset(Vec2(ofsx, ofsy));
						}
						break;
					}
				}
				if (boFoundTheSelectedKey)
				{
					break;
				}
			}
		}
	}
	else
	{
		for (int splineIndex = 0, endSpline = m_splines.size(); splineIndex < endSpline; ++splineIndex)
		{
			ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline;

			float fClosestKeyTime = -1.0f;
			float fClosestDist = 1E8;

			for (int i = 0; i < pSpline->GetKeyCount(); i++)
			{
				float fKeyTime = pSpline->GetKeyTime(i);
				float fKeyDist = previousKey ? m_fTimeMarker - fKeyTime : fKeyTime - m_fTimeMarker;

				if ((fKeyDist > 0.0f) && (fKeyDist < fClosestDist))
				{
					fClosestDist = fKeyDist;
					fClosestKeyTime = pSpline->GetKeyTime(i);
				}
			}

			if (fClosestKeyTime >= 0.0f)
			{
				SetTimeMarker(fClosestKeyTime);

				float averageValue = 0.f;
				int dimensions = pSpline->GetNumDimensions();

				for (int i = 0; i < dimensions; i++)
				{
					float keyValue;
					int keyNum = pSpline->FindKey(fClosestKeyTime);

					pSpline->GetKeyValueFloat(keyNum, keyValue);
					averageValue += keyValue;
				}

				// Set the new scrolled coordinates
				float ofsx = fClosestKeyTime - (m_grid.rect.right / 2) / m_grid.zoom.x;
				float ofsy = averageValue / dimensions - (m_grid.rect.bottom / 2) / m_grid.zoom.y;

				SetScrollOffset(Vec2(ofsx, ofsy));
			}
		}
	}
	SendNotifyEvent(SPLN_TIME_CHANGE);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::RemoveAllKeysButThis()
{
	std::vector<int> keys;

	for (int splineIndex = 0, endSpline = m_splines.size(); splineIndex < endSpline; ++splineIndex)
	{
		ISplineInterpolator* pSpline = m_splines[splineIndex].pSpline;

		for (int i = 0; i < pSpline->GetKeyCount(); i++)
		{
			if (pSpline->IsKeySelectedAtAnyDimension(i))
			{
				keys.push_back(i);
			}
		}

		for (int i = pSpline->GetKeyCount(); i >= 0; i--)
		{
			bool saveKey = false;

			for (int nIndex = 0; nIndex < keys.size(); nIndex++)
			{
				if (keys[nIndex] == i)
					saveKey = true;
			}

			if (!saveKey)
				RemoveKey(pSpline, i);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
ISplineCtrlUndo* CSplineCtrlEx::CreateSplineCtrlUndoObject(std::vector<ISplineInterpolator*>& splineContainer)
{
	return new CUndoSplineCtrlEx(this, splineContainer);
}

//////////////////////////////////////////////////////////////////////////
void CSplineCtrlEx::OnCaptureChanged(CWnd* pWnd)
{
	StopTracking();
}

