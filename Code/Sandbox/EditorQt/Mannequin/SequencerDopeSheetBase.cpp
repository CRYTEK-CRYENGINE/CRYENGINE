// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SequencerDopeSheetBase.h"
#include "Controls\MemDC.h"

#include "SequencerUndo.h"

#include "Util/Clipboard.h"

#include "SequencerTrack.h"
#include "SequencerNode.h"
#include "SequencerSequence.h"
#include "SequencerUtils.h"
#include "SequencerKeyPropertiesDlg.h"
#include "MannequinDialog.h"
#include "MannPreferences.h"
#include "MannequinUtil.h"
#include "FragmentTrack.h"

#include <ISourceControl.h>

#include "QtUtil.h"
#include "Controls/QuestionDialog.h"
#include "Controls/SharedFonts.h"

enum ETVMouseMode
{
	MOUSE_MODE_NONE   = 0,
	MOUSE_MODE_SELECT = 1,
	MOUSE_MODE_MOVE,
	MOUSE_MODE_CLONE,
	MOUSE_MODE_DRAGTIME,
	MOUSE_MODE_DRAGSTARTMARKER,
	MOUSE_MODE_DRAGENDMARKER,
	MOUSE_MODE_PASTE_DRAGTIME,
	MOUSE_MODE_SELECT_WITHIN_TIME,
};

static const int MARGIN_FOR_MAGNET_SNAPPING = 10;

// CSequencerKeys
IMPLEMENT_DYNAMIC(CSequencerDopeSheetBase, CWnd)

class CSequencerDopeSheetBaseDropTarget : public COleDropTarget
{
public:

	CSequencerDopeSheetBaseDropTarget(CSequencerDopeSheetBase* dlg)
		: m_pDlg(dlg)
	{}

	virtual DROPEFFECT OnDragEnter(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
	{
		// Check for dropping a fragment into the preview panel
		if (m_pDlg->IsPointValidForFragmentInPreviewDrop(point, pDataObject))
		{
			return DROPEFFECT_MOVE;
		}
		// Check for dropping an animation into the Fragment editor in an animation layer
		if (m_pDlg->IsPointValidForAnimationInLayerDrop(point, pDataObject))
		{
			return DROPEFFECT_MOVE;
		}

		return DROPEFFECT_NONE;
	}

	virtual DROPEFFECT OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
	{
		// Do the same as the enter
		return OnDragEnter(pWnd, pDataObject, dwKeyState, point);
	}

	virtual BOOL OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point)
	{
		// Check for dropping a fragment into the preview panel
		if (m_pDlg->IsPointValidForFragmentInPreviewDrop(point, pDataObject))
		{
			return m_pDlg->CreatePointForFragmentInPreviewDrop(point, pDataObject);
		}

		// Check for dropping an animation into the Fragment editor in an animation layer
		if (m_pDlg->IsPointValidForAnimationInLayerDrop(point, pDataObject))
		{
			return m_pDlg->CreatePointForAnimationInLayerDrop(point, pDataObject);
		}

		return false;
	}

private:

	CSequencerDopeSheetBase* m_pDlg;
};

CSequencerDopeSheetBase::CSequencerDopeSheetBase()
{
	m_prePasteSheetData = NULL;
	m_pSequence = 0;
	//m_wndTrack = NULL;
	m_bkgrBrush.CreateSolidBrush(GetSysColor(COLOR_3DFACE));
	//m_bkgrBrushEmpty.CreateHatchBrush( HS_BDIAGONAL,GetSysColor(COLOR_3DFACE) );
	m_bkgrBrushEmpty.CreateSolidBrush(RGB(190, 190, 190));
	m_timeBkgBrush.CreateSolidBrush(RGB(0xE0, 0xE0, 0xE0));
	m_timeHighlightBrush.CreateSolidBrush(RGB(0xFF, 0x0, 0x0));
	m_selectedBrush.CreateSolidBrush(RGB(200, 200, 230));
	//m_visibilityBrush.CreateSolidBrush( RGB(0,150,255) );
	m_visibilityBrush.CreateSolidBrush(RGB(120, 120, 255));
	m_selectTrackBrush.CreateSolidBrush(RGB(100, 190, 255));

	m_mouseMoveStartTimeOffset = 0.0f;
	m_mouseMoveStartTrackOffset = 0;
	m_timeScale = 1;
	m_ticksStep = 10;

	m_bZoomDrag = false;
	m_bMoveDrag = false;

	m_bMouseOverKey = false;

	m_leftOffset = 0;
	m_scrollOffset = CPoint(0, 0);
	m_bAnySelected = 0;
	m_mouseMode = MOUSE_MODE_NONE;
	m_currentTime = 40;
	m_storedTime = m_currentTime;
	m_rcSelect = CRect(0, 0, 0, 0);
	m_keyTimeOffset = 0;
	m_currCursor = NULL;
	m_mouseActionMode = SEQMODE_MOVEKEY;

	m_itemWidth = 1000;
	m_scrollMin = 0;
	m_scrollMax = 1000;

	m_descriptionFont = new CFont();
	m_descriptionFont->CreateFont(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
	                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Verdana");

	m_bCursorWasInKey = false;
	m_fJustSelected = 0.f;
	m_snappingMode = SEQKEY_SNAP_NONE;
	m_snapFrameTime = 1.0 / 30.0;
	m_bMouseMovedAfterRButtonDown = false;

	m_pLastTrackSelectedOnSpot = NULL;

	m_changeCount = 0;

	m_tickDisplayMode = SEQTICK_INSECONDS;
	ComputeFrameSteps(GetVisibleRange());

}

CSequencerDopeSheetBase::~CSequencerDopeSheetBase()
{
	GetIEditorImpl()->UnregisterNotifyListener(this);

	m_descriptionFont->DeleteObject();
	delete m_descriptionFont;
}

BEGIN_MESSAGE_MAP(CSequencerDopeSheetBase, CWnd)
ON_WM_CREATE()
ON_WM_DESTROY()
ON_WM_MEASUREITEM_REFLECT()
ON_WM_CTLCOLOR_REFLECT()
ON_WM_SIZE()
ON_WM_MOUSEWHEEL()
ON_WM_HSCROLL()
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONDBLCLK()
ON_WM_RBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_MBUTTONDOWN()
ON_WM_MBUTTONUP()
ON_WM_MOUSEMOVE()
ON_WM_PAINT()
ON_WM_SETCURSOR()
ON_WM_ERASEBKGND()
ON_WM_RBUTTONDOWN()
ON_WM_KEYDOWN()
ON_WM_RBUTTONUP()
ON_WM_INPUT()
END_MESSAGE_MAP()

// CSequencerKeys message handlers

//////////////////////////////////////////////////////////////////////////
int CSequencerDopeSheetBase::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	CBitmap bms[SNumOfBmps];
	bms[0].LoadBitmap(IDB_SEQUENCE_KEY_COL_1);
	bms[1].LoadBitmap(IDB_SEQUENCE_KEY_COL_2);
	bms[2].LoadBitmap(IDB_SEQUENCE_KEY_COL_3);
	bms[3].LoadBitmap(IDB_SEQUENCE_KEY_COL_4);
	bms[4].LoadBitmap(IDB_SEQUENCE_KEY_COL_5);
	bms[5].LoadBitmap(IDB_SEQUENCE_KEY_COL_6);
	bms[6].LoadBitmap(IDB_SEQUENCE_KEY_COL_7);
	for (int i = 0; i < SNumOfBmps; i++)
	{
		m_imageSeqKeyBody[i].Create(16, 32, ILC_COLOR32, 8, 0);
		m_imageSeqKeyBody[i].Add(&bms[i], RGB(255, 0, 255));
	}

	m_imageList.Create(MAKEINTRESOURCE(IDB_SEQUENCER_KEYS), 14, 0, RGB(255, 0, 255));
	m_imgMarker.Create(MAKEINTRESOURCE(IDB_MARKER), 8, 0, RGB(255, 0, 255));
	m_crsLeftRight = AfxGetApp()->LoadStandardCursor(IDC_SIZEWE);
	m_crsAddKey = AfxGetApp()->LoadCursor(IDC_ARROW_ADDKEY);
	m_crsCross = AfxGetApp()->LoadCursor(IDC_POINTER_OBJHIT);
	m_crsMoveBlend = AfxGetApp()->LoadCursor(IDC_LEFTRIGHT);

	GetIEditorImpl()->RegisterNotifyListener(this);

	m_pDropTarget = new CSequencerDopeSheetBaseDropTarget(this);
	m_pDropTarget->Register(this);
	//InitializeFlatSB(GetSafeHwnd());

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::OnDestroy()
{
	HideKeyPropertyCtrlOnSpot();
	GetIEditorImpl()->UnregisterNotifyListener(this);
	if (m_pDropTarget)
	{
		delete m_pDropTarget;
	}
	CWnd::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::DrawSelectedKeyIndicators(CDC* dc)
{
	if (m_pSequence == NULL)
		return;

	CPen selPen(PS_SOLID, 1, RGB(255, 255, 0));
	CPen* prevPen = dc->SelectObject(&selPen);
	CSequencerUtils::SelectedKeys selectedKeys;
	CSequencerUtils::GetSelectedKeys(m_pSequence, selectedKeys);
	for (int i = 0; i < selectedKeys.keys.size(); ++i)
	{
		CSequencerTrack* pTrack = selectedKeys.keys[i].pTrack;
		int keyIndex = selectedKeys.keys[i].nKey;
		int x = TimeToClient(pTrack->GetKeyTime(keyIndex));
		dc->MoveTo(x, m_rcClient.top);
		dc->LineTo(x, m_rcClient.bottom);
	}
	dc->SelectObject(prevPen);
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::ComputeFrameSteps(const Range& VisRange)
{
	float fNbFrames = fabsf((VisRange.end - VisRange.start) / m_snapFrameTime);
	float afStepTable[4] = { 1.0f, 0.5f, 0.2f, 0.1f };
	bool bNotDone = true;
	float fFact = 1.0f;
	int nAttempts = 10;
	int i;
	while (bNotDone && --nAttempts > 0)
	{
		bool bLess = true;
		for (i = 0; i < 4; ++i)
		{
			float fFactNbFrames = fNbFrames / (afStepTable[i] * fFact);
			if (fFactNbFrames >= 3 && fFactNbFrames <= 9)
			{
				bNotDone = false;
				break;
			}
			else if (fFactNbFrames < 3)
			{
				bLess = true;
			}
			else
			{
				bLess = false;
			}
		}
		if (bNotDone)
		{
			fFact *= (bLess) ? 0.1 : 10.0f;
		}
	}

	int nBIntermediateTicks = 5;
	m_fFrameLabelStep = fFact * afStepTable[i];
	if (m_fFrameLabelStep <= 1.0f)
	{
		m_fFrameLabelStep = 1.0f;
	}

	if (TimeToClient(m_fFrameLabelStep) - TimeToClient(0) > 1300)
	{
		nBIntermediateTicks = 10;
	}

	while (m_fFrameLabelStep / double(nBIntermediateTicks) < 1.0f && nBIntermediateTicks != 1)
	{
		nBIntermediateTicks = int (nBIntermediateTicks / 2.0f);
	}

	m_fFrameTickStep = m_fFrameLabelStep * double (m_snapFrameTime) / double(nBIntermediateTicks);
}

void CSequencerDopeSheetBase::DrawTimeLineInFrames(CDC* dc, CRect& rc, COLORREF& lineCol, COLORREF& textCol, double step)
{
	float fFramesPerSec = 1.0f / m_snapFrameTime;
	float fInvFrameLabelStep = 1.0f / m_fFrameLabelStep;
	Range VisRange = GetVisibleRange();

	const Range& timeRange = m_timeRange;

	CPen ltgray(PS_SOLID, 1, RGB(90, 90, 90));
	CPen black(PS_SOLID, 1, textCol);

	for (double t = TickSnap(timeRange.start); t <= timeRange.end + m_fFrameTickStep; t += m_fFrameTickStep)
	{
		double st = t;
		if (st > timeRange.end)
			st = timeRange.end;
		if (st < VisRange.start)
			continue;
		if (st > VisRange.end)
			break;
		if (st < m_timeRange.start || st > m_timeRange.end)
			continue;
		int x = TimeToClient(st);
		dc->MoveTo(x, rc.bottom - 2);

		float fFrame = st * fFramesPerSec;
		/*float fPowerOfN = float(pow (double(fN), double(nFramePowerOfN)));
		   float fFramePow = fFrame * fPowerOfN;
		   float nFramePow = float(pos_directed_rounding(fFramePow));*/

		float fFrameScaled = fFrame * fInvFrameLabelStep;
		if (fabsf(fFrameScaled - pos_directed_rounding(fFrameScaled)) < 0.001f)
		{
			dc->SelectObject(black);
			dc->LineTo(x, rc.bottom - 14);
			char str[32];
			cry_sprintf(str, "%g", fFrame);
			dc->TextOut(x + 2, rc.top, str);
			dc->SelectObject(ltgray);
		}
		else
			dc->LineTo(x, rc.bottom - 6);
	}
}

void CSequencerDopeSheetBase::DrawTimeLineInSeconds(CDC* dc, CRect& rc, COLORREF& lineCol, COLORREF& textCol, double step)
{
	Range VisRange = GetVisibleRange();
	const Range& timeRange = m_timeRange;
	int nNumberTicks = 10;

	CPen ltgray(PS_SOLID, 1, RGB(90, 90, 90));
	CPen black(PS_SOLID, 1, textCol);

	for (double t = TickSnap(timeRange.start); t <= timeRange.end + step; t += step)
	{
		double st = TickSnap(t);
		if (st > timeRange.end)
			st = timeRange.end;
		if (st < VisRange.start)
			continue;
		if (st > VisRange.end)
			break;
		if (st < m_timeRange.start || st > m_timeRange.end)
			continue;
		int x = TimeToClient(st);
		dc->MoveTo(x, rc.bottom - 2);

		int k = pos_directed_rounding(st * m_ticksStep);
		if (k % nNumberTicks == 0)
		{
			dc->SelectObject(black);
			dc->LineTo(x, rc.bottom - 14);
			char str[32];
			cry_sprintf(str, "%g", st);
			dc->TextOut(x + 2, rc.top, str);
			dc->SelectObject(ltgray);
		}
		else
			dc->LineTo(x, rc.bottom - 6);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::DrawTimeline(CDC* dc, const CRect& rcUpdate)
{
	CRect rc, temprc;

	COLORREF lineCol = RGB(255, 0, 255);
	COLORREF textCol = RGB(0, 0, 0);

	// Draw vertical line showing current time.
	{
		int x = TimeToClient(m_currentTime);
		if (x > m_rcClient.left && x < m_rcClient.right)
		{
			CPen pen(PS_SOLID, 1, lineCol);
			CPen* prevPen = dc->SelectObject(&pen);
			dc->MoveTo(x, 0);
			dc->LineTo(x, m_rcClient.bottom);
			dc->SelectObject(prevPen);
		}
	}

	rc = m_rcTimeline;
	if (temprc.IntersectRect(rc, rcUpdate) == 0)
		return;

	XTPPaintManager()->GradientFill(dc, rc, RGB(250, 250, 250), RGB(180, 180, 180), FALSE);

	CPen* prevPen;

	// Draw time ticks every tick step seconds.
	const Range& timeRange = m_timeRange;
	CString str;

	dc->SetTextColor(textCol);
	dc->SetBkMode(TRANSPARENT);
	dc->SelectObject(SMFCFonts::GetInstance().hSystemFont);

	CPen ltgray(PS_SOLID, 1, RGB(90, 90, 90));
	dc->SelectObject(ltgray);

	double step = 1.0 / double(m_ticksStep);
	if (GetTickDisplayMode() == SEQTICK_INFRAMES)
		DrawTimeLineInFrames(dc, rc, lineCol, textCol, step);
	else if (GetTickDisplayMode() == SEQTICK_INSECONDS)
		DrawTimeLineInSeconds(dc, rc, lineCol, textCol, step);
	else
		assert(0);

	// Draw time markers.
	int x;

	x = TimeToClient(m_timeMarked.start);
	m_imgMarker.Draw(dc, 1, CPoint(x, m_rcTimeline.bottom - 9), ILD_TRANSPARENT);
	x = TimeToClient(m_timeMarked.end);
	m_imgMarker.Draw(dc, 0, CPoint(x - 7, m_rcTimeline.bottom - 9), ILD_TRANSPARENT);

	CPen redpen(PS_SOLID, 1, lineCol);
	prevPen = dc->SelectObject(&redpen);
	x = TimeToClient(m_currentTime);
	dc->SelectObject(GetStockObject(NULL_BRUSH));
	dc->Rectangle(x - 3, rc.top, x + 4, rc.bottom);

	dc->SelectObject(redpen);
	dc->MoveTo(x, rc.top);
	dc->LineTo(x, rc.bottom);
	dc->SelectObject(GetStockObject(NULL_BRUSH));
	//	dc->Rectangle( x-3,rc.top,x+4,rc.bottom );

	dc->SelectObject(prevPen);
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::DrawSummary(CDC* dc, CRect rcUpdate)
{
	if (m_pSequence == NULL)
		return;

	CRect rc, temprc;
	COLORREF lineCol = RGB(0, 0, 0);
	COLORREF fillCol = RGB(150, 100, 220);

	rc = m_rcSummary;
	if (temprc.IntersectRect(rc, rcUpdate) == 0)
		return;

	dc->FillSolidRect(rc.left, rc.top, rc.Width(), rc.Height(), fillCol);

	CPen* prevPen;
	CPen blackPen(PS_SOLID, 3, lineCol);
	Range timeRange = m_timeRange;

	prevPen = dc->SelectObject(&blackPen);

	// Draw a short thick line at each place where there is a key in any tracks.
	CSequencerUtils::SelectedKeys selectedKeys;
	CSequencerUtils::GetAllKeys(m_pSequence, selectedKeys);
	for (int i = 0; i < selectedKeys.keys.size(); ++i)
	{
		CSequencerTrack* pTrack = selectedKeys.keys[i].pTrack;
		int keyIndex = selectedKeys.keys[i].nKey;
		int x = TimeToClient(pTrack->GetKeyTime(keyIndex));
		dc->MoveTo(x, rc.bottom - 2);
		dc->LineTo(x, rc.top + 2);
	}

	dc->SelectObject(prevPen);
}

//////////////////////////////////////////////////////////////////////////
int CSequencerDopeSheetBase::TimeToClient(float time) const
{
	int x = m_leftOffset - m_scrollOffset.x + time * m_timeScale;
	return x;
}

//////////////////////////////////////////////////////////////////////////
Range CSequencerDopeSheetBase::GetVisibleRange()
{
	Range r;
	r.start = (m_scrollOffset.x - m_leftOffset) / m_timeScale;
	r.end = r.start + (m_rcClient.Width()) / m_timeScale;
	// Intersect range with global time range.
	r = m_timeRange & r;
	return r;
}

//////////////////////////////////////////////////////////////////////////
Range CSequencerDopeSheetBase::GetTimeRange(CRect& rc)
{
	Range r;
	r.start = (rc.left - m_leftOffset + m_scrollOffset.x) / m_timeScale;
	r.end = r.start + (rc.Width()) / m_timeScale;

	r.start = TickSnap(r.start);
	r.end = TickSnap(r.end);
	// Intersect range with global time range.
	r = m_timeRange & r;
	return r;
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::DrawTicks(CDC* dc, CRect& rc, Range& timeRange)
{
	// Draw time ticks every tick step seconds.
	CPen ltgray(PS_SOLID, 1, RGB(90, 90, 90));
	CPen* prevPen = dc->SelectObject(&ltgray);
	Range VisRange = GetVisibleRange();
	int nNumberTicks = 10;
	if (GetTickDisplayMode() == SEQTICK_INFRAMES)
		nNumberTicks = pos_directed_rounding(1.0f / m_snapFrameTime);
	double step = 1.0 / m_ticksStep;
	for (double t = TickSnap(timeRange.start); t <= timeRange.end + step; t += step)
	{
		double st = TickSnap(t);
		if (st > timeRange.end)
			st = timeRange.end;
		if (st < VisRange.start)
			continue;
		if (st > VisRange.end)
			break;
		int x = TimeToClient(st);
		if (x < 0)
			continue;
		dc->MoveTo(x, rc.bottom - 1);

		int k = pos_directed_rounding(st * m_ticksStep);
		if (k % nNumberTicks == 0)
		{
			dc->SelectObject(GetStockObject(BLACK_PEN));
			dc->LineTo(x, rc.bottom - 5);
			dc->SelectObject(ltgray);
		}
		else
			dc->LineTo(x, rc.bottom - 3);
	}
	dc->SelectObject(prevPen);
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::DrawKeys(CSequencerTrack* track, CDC* dc, CRect& rc, Range& timeRange, EDSRenderFlags renderFlags)
{
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::RedrawItem(int item)
{
	CRect rc;
	if (GetItemRect(item, rc) != LB_ERR)
	{
		RedrawWindow(rc, NULL, RDW_INVALIDATE | RDW_ERASE);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::MeasureItem(LPMEASUREITEMSTRUCT lpMIS)
{
	lpMIS->itemWidth = 1000;
	lpMIS->itemHeight = 16;
}

//////////////////////////////////////////////////////////////////////////
HBRUSH CSequencerDopeSheetBase::CtlColor(CDC* pDC, UINT nCtlColor)
{
	return m_bkgrBrush;

	// TODO:  Return a non-NULL brush if the parent's handler should not be called
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::SetTimeRange(float start, float end)
{
	/*
	   if (m_timeMarked.start==m_timeRange.start)
	   m_timeMarked.start=start;
	   if (m_timeMarked.end==m_timeRange.end)
	   m_timeMarked.end=end;
	   if (m_timeMarked.end>end)
	   m_timeMarked.end=end;
	 */
	if (m_timeMarked.start < start)
		m_timeMarked.start = start;
	if (m_timeMarked.end > end)
		m_timeMarked.end = end;

	m_realTimeRange.Set(start, end);
	m_timeRange.Set(start, end);
	//SetHorizontalExtent( m_timeRange.Length() *m_timeScale + 2*m_leftOffset );

	SetHorizontalExtent(m_timeRange.start * m_timeScale - m_leftOffset, m_timeRange.end * m_timeScale - m_leftOffset);
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::SetTimeScale(float timeScale, float fAnchorTime)
{
	//m_leftOffset - m_scrollOffset.x + time*m_timeScale
	double fOldOffset = -fAnchorTime * m_timeScale;

	double fOldScale = m_timeScale;
	if (timeScale < 0.001f)
		timeScale = 0.001f;
	if (timeScale > 100000.0f)
		timeScale = 100000.0f;
	m_timeScale = timeScale;
	double fPixelsPerTick;

	int steps = 0;
	if (GetTickDisplayMode() == SEQTICK_INSECONDS)
		m_ticksStep = 10.0;
	else if (GetTickDisplayMode() == SEQTICK_INFRAMES)
		m_ticksStep = 1.0 / m_snapFrameTime;
	else
		assert(0);
	do
	{
		fPixelsPerTick = (1.0 / m_ticksStep) * (double)m_timeScale;
		if (fPixelsPerTick < 6.0)
		{
			//if (m_ticksStep>=10)
			m_ticksStep *= 0.5;
		}
		if (m_ticksStep <= 0.0)
		{
			m_ticksStep = 1.0;
			break;
		}
		steps++;
	}
	while (fPixelsPerTick < 6.0 && steps < 100);

	steps = 0;
	do
	{
		fPixelsPerTick = (1.0 / m_ticksStep) * (double)m_timeScale;
		if (fPixelsPerTick >= 12.0)
		{
			m_ticksStep *= 2.0;
		}
		if (m_ticksStep <= 0.0)
		{
			m_ticksStep = 1.0;
			break;
		}
		steps++;
	}
	while (fPixelsPerTick >= 12.0 && steps < 100);

	//float
	//m_scrollOffset.x*=timeScale/fOldScale;

	float fCurrentOffset = -fAnchorTime * m_timeScale;
	m_scrollOffset.x += fOldOffset - fCurrentOffset;

	Invalidate();

	//SetHorizontalExtent( m_timeRange.Length()*m_timeScale + 2*m_leftOffset );
	SetHorizontalExtent(m_timeRange.start * m_timeScale - m_leftOffset, m_timeRange.end * m_timeScale);

	ComputeFrameSteps(GetVisibleRange());
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);

	GetClientRect(m_rcClient);

	if (m_offscreenBitmap.GetSafeHandle() != NULL)
		m_offscreenBitmap.DeleteObject();

	CDC* dc = GetDC();
	m_offscreenBitmap.CreateCompatibleBitmap(dc, m_rcClient.Width(), m_rcClient.Height());
	ReleaseDC(dc);

	GetClientRect(m_rcTimeline);
	m_rcTimeline.bottom = m_rcTimeline.top + gMannequinPreferences.trackSize;
	m_rcSummary = m_rcTimeline;
	m_rcSummary.top = m_rcTimeline.bottom;
	m_rcSummary.bottom = m_rcSummary.top + 8;

	SetHorizontalExtent(m_scrollMin, m_scrollMax);

	if (m_tooltip.m_hWnd)
	{
		m_tooltip.DelTool(this, 1);
		m_tooltip.AddTool(this, "", m_rcClient, 1);
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CSequencerDopeSheetBase::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		if (pMsg->wParam == VK_RIGHT || pMsg->wParam == VK_LEFT)
		{
			const bool bLeft = pMsg->wParam == VK_LEFT;
			const bool bUseFrames = GetTickDisplayMode() == SEQTICK_INFRAMES;
			const float fOffset = (bLeft ? -1.0f : 1.0f) * (bUseFrames ? m_snapFrameTime * (0.5f + 0.01f) : GetTickTime());
			const float fTime = max(0.0f, TickSnap(GetCurrTime() + fOffset));
			if (CMannequinDialog::GetCurrentInstance()->GetDockingPaneManager()->IsPaneSelected(CMannequinDialog::IDW_FRAGMENT_EDITOR_PANE))
				CMannequinDialog::GetCurrentInstance()->FragmentEditor()->SetTime(fTime);
			else if (CMannequinDialog::GetCurrentInstance()->GetDockingPaneManager()->IsPaneSelected(CMannequinDialog::IDW_PREVIEWER_PANE))
				CMannequinDialog::GetCurrentInstance()->Previewer()->SetTime(fTime);
			else if (CMannequinDialog::GetCurrentInstance()->GetDockingPaneManager()->IsPaneSelected(CMannequinDialog::IDW_TRANSITION_EDITOR_PANE))
				CMannequinDialog::GetCurrentInstance()->TransitionEditor()->SetTime(fTime);

			return TRUE;
		}
	}

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
static float GetTimelineWheelScaleFactor()
{
	const float defaultScaleFactor = 1.25f;
	return pow(defaultScaleFactor, clamp_tpl(gMannequinPreferences.timelineWheelZoomSpeed, 0.1f, 5.0f));
}

//////////////////////////////////////////////////////////////////////////
BOOL CSequencerDopeSheetBase::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	GetCursorPos(&pt);
	ScreenToClient(&pt);

	const float fAnchorTime = TimeFromPointUnsnapped(pt);
	const float newTimeScale = m_timeScale * pow(GetTimelineWheelScaleFactor(), float(zDelta) / WHEEL_DELTA);

	SetTimeScale(newTimeScale, fAnchorTime);
	return 1;
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
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

	m_scrollOffset.x = curpos;
	Invalidate();

	CWnd::OnHScroll(nSBCode, nPos, pScrollBar);
}

//////////////////////////////////////////////////////////////////////////
double CSequencerDopeSheetBase::GetTickTime() const
{
	if (GetTickDisplayMode() == SEQTICK_INFRAMES)
		return m_fFrameTickStep;
	else
		return 1.0f / m_ticksStep;
}

//////////////////////////////////////////////////////////////////////////
float CSequencerDopeSheetBase::TickSnap(float time) const
{
	// const Range & timeRange = m_timeRange;
	const bool bUseFrames = GetTickDisplayMode() == SEQTICK_INFRAMES;
	const double tickTime = bUseFrames ? m_snapFrameTime : GetTickTime();
	double t = floor((double)time / tickTime + 0.5);
	t *= tickTime;
	return t;
}

//////////////////////////////////////////////////////////////////////////
float CSequencerDopeSheetBase::TimeFromPoint(CPoint point) const
{
	int x = point.x - m_leftOffset + m_scrollOffset.x;
	double t = (double)x / m_timeScale;
	return (float)TickSnap(t);
}

//////////////////////////////////////////////////////////////////////////
float CSequencerDopeSheetBase::TimeFromPointUnsnapped(CPoint point) const
{
	int x = point.x - m_leftOffset + m_scrollOffset.x;
	double t = (double)x / m_timeScale;
	return t;
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::AddItem(const Item& item)
{
	m_tracks.push_back(item);
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
const CSequencerDopeSheetBase::Item& CSequencerDopeSheetBase::GetItem(int item) const
{
	return m_tracks[item];
}

//////////////////////////////////////////////////////////////////////////
CSequencerTrack* CSequencerDopeSheetBase::GetTrack(int item) const
{
	if (item < 0 || item >= GetCount())
		return 0;
	CSequencerTrack* track = m_tracks[item].track;
	return track;
}

//////////////////////////////////////////////////////////////////////////
CSequencerNode* CSequencerDopeSheetBase::GetNode(int item) const
{
	if (item < 0 || item >= GetCount())
		return 0;
	return m_tracks[item].node;
}

//////////////////////////////////////////////////////////////////////////
int CSequencerDopeSheetBase::FirstKeyFromPoint(CPoint point, bool exact) const
{
	return -1;
}

//////////////////////////////////////////////////////////////////////////
int CSequencerDopeSheetBase::LastKeyFromPoint(CPoint point, bool exact) const
{
	return -1;
}

bool CSequencerDopeSheetBase::IsDragging() const
{
	return (m_mouseMode != MOUSE_MODE_NONE);
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::OnLButtonDown(UINT nFlags, CPoint point)
{
	CWnd::OnLButtonDown(nFlags, point);

	HideKeyPropertyCtrlOnSpot();

	SetFocus();

	m_mouseDownPos = point;

	if (m_rcTimeline.PtInRect(point))
	{
		// Clicked inside timeline.
		m_mouseMode = MOUSE_MODE_DRAGTIME;
		// If mouse over selected key and it can be moved, change cursor to left-right arrows.
		SetMouseCursor(m_crsLeftRight);
		SetCapture();

		SetCurrTime(TimeFromPoint(point));
		return;
	}

	if (m_mouseMode == MOUSE_MODE_PASTE_DRAGTIME)
	{
		FinalizePasteKeys();
		return;
	}

	// The summary region is used for moving already selected keys.
	if (m_rcSummary.PtInRect(point) && m_bAnySelected)
	{
		/// Move/Clone Key Undo Begin
		GetIEditorImpl()->GetIUndoManager()->Begin();

		CSequencerUtils::SelectedKeys selectedKeys;
		CSequencerUtils::GetSelectedKeys(m_pSequence, selectedKeys);
		CSequencerTrack* pTrack = NULL;
		for (int k = 0; k < (int)selectedKeys.keys.size(); ++k)
		{
			const CSequencerUtils::SelectedKey& skey = selectedKeys.keys[k];
			if (pTrack != skey.pTrack)
			{
				CUndo::Record(new CUndoSequencerSequenceModifyObject(skey.pTrack, m_pSequence));
				pTrack = skey.pTrack;
			}
		}

		m_keyTimeOffset = 0;
		m_mouseMode = MOUSE_MODE_MOVE;
		bool canAnyKeyBeMoved = CSequencerUtils::CanAnyKeyBeMoved(selectedKeys);
		if (canAnyKeyBeMoved)
			SetMouseCursor(m_crsLeftRight);
		else
			SetMouseCursor(m_crsCross);
		return;
	}

	int key = LastKeyFromPoint(point);
	if (key >= 0)
	{
		const int item = ItemFromPoint(point);
		CSequencerTrack* track = GetTrack(item);

		const float t1 = TimeFromPointUnsnapped(CPoint(point.x - 4, point.y));
		const float t2 = TimeFromPointUnsnapped(CPoint(point.x + 4, point.y));

		int secondarySelKey;
		int secondarySelection = track->FindSecondarySelectionPt(secondarySelKey, t1, t2);
		if (secondarySelection)
		{
			if (track->CanMoveSecondarySelection(secondarySelKey, secondarySelection))
			{
				key = secondarySelKey;
			}
			else
			{
				secondarySelection = 0;
			}
		}

		/// Move/Clone Key Undo Begin
		GetIEditorImpl()->GetIUndoManager()->Begin();

		if (!track->IsKeySelected(key) && !(nFlags & MK_CONTROL))
		{
			UnselectAllKeys(false);
		}
		m_bAnySelected = true;
		m_fJustSelected = gEnv->pTimer->GetCurrTime();
		m_keyTimeOffset = 0;
		if (secondarySelection)
		{
			// snap the mouse cursor to the key
			const int xPointNew = TimeToClient(track->GetSecondaryTime(key, secondarySelection));
			point.x = xPointNew;
			m_mouseDownPos = point;
			m_grabOffset = 0;
			// set the mouse position on-screen
			CPoint mpos = point;
			ClientToScreen(&mpos);
			SetCursorPos(mpos.x, mpos.y);
		}
		else
		{
			m_grabOffset = point.x - TimeToClient(track->GetKeyTime(key));
		}
		track->SelectKey(key, true);

		int nNodeCount = m_pSequence->GetNodeCount();
		for (int nodeID = 0; nodeID < nNodeCount; ++nodeID)
		{
			CSequencerNode* pAnimNode = m_pSequence->GetNode(nodeID);

			int nTrackCount = pAnimNode->GetTrackCount();
			for (int trackID = 0; trackID < nTrackCount; ++trackID)
			{
				CSequencerTrack* pAnimTrack = pAnimNode->GetTrackByIndex(trackID);
				CUndo::Record(new CUndoSequencerSequenceModifyObject(pAnimTrack, m_pSequence));
			}
		}

		m_secondarySelection = secondarySelection;
		if (secondarySelection)
		{
			m_mouseMode = MOUSE_MODE_MOVE;
			SetMouseCursor(m_crsMoveBlend);
		}
		else if ((nFlags & MK_SHIFT) && track->CanAddKey(track->GetKeyTime(key))) // NOTE: this time is just a guess
		{
			m_mouseMode = MOUSE_MODE_CLONE;
			SetMouseCursor(m_crsLeftRight);
		}
		else if (track->CanMoveKey(key))
		{
			if (track->GetParameterType() == SEQUENCER_PARAM_TRANSITIONPROPS) // TODO: fragments also?
			{
				m_mouseMode = MOUSE_MODE_MOVE;
			}
			else
			{
				StartDraggingKeys(point);
			}
			SetMouseCursor(m_crsLeftRight);
		}
		else
		{
			m_mouseMode = MOUSE_MODE_MOVE;
			SetMouseCursor(m_crsCross);
		}

		Invalidate();
		SetKeyInfo(track, key);
		return;
	}

	if (m_mouseActionMode == SEQMODE_ADDKEY)
	{
		// Add key here.
		int item = ItemFromPoint(point);
		CSequencerTrack* track = GetTrack(item);
		if (track)
		{
			float keyTime = TimeFromPoint(point);
			if (IsOkToAddKeyHere(track, keyTime))
			{
				RecordTrackUndo(GetItem(item));
				track->CreateKey(keyTime);
				Invalidate();
				UpdateAnimation(item);
			}
		}
		return;
	}

	if (nFlags & MK_SHIFT)
		m_mouseMode = MOUSE_MODE_SELECT_WITHIN_TIME;
	else
		m_mouseMode = MOUSE_MODE_SELECT;
	SetCapture();
	if (m_bAnySelected && !(nFlags & MK_CONTROL))
	{
		// First unselect all buttons.
		UnselectAllKeys(true);
		Invalidate();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_mouseMode == MOUSE_MODE_SELECT)
	{
		bool prevSelected = m_bAnySelected;
		// Check if any key are selected.
		m_rcSelect -= m_scrollOffset;
		SelectKeys(m_rcSelect);
		NotifyKeySelectionUpdate();
		/*
		   if (prevSelected == m_bAnySelected)
		   Invalidate();
		   else
		   {
		   CDC *dc = GetDC();
		   dc->DrawDragRect( CRect(0,0,0,0),CSize(0,0),m_rcSelect,CSize(1,1) );
		   ReleaseDC(dc);
		   }
		 */
		Invalidate();
		m_rcSelect = CRect(0, 0, 0, 0);
	}
	else if (m_mouseMode == MOUSE_MODE_SELECT_WITHIN_TIME)
	{
		m_rcSelect -= m_scrollOffset;
		SelectAllKeysWithinTimeFrame(m_rcSelect);
		NotifyKeySelectionUpdate();
		Invalidate();
		m_rcSelect = CRect(0, 0, 0, 0);
	}
	else if (m_mouseMode == MOUSE_MODE_DRAGTIME)
	{
		SetMouseCursor(NULL);
	}
	else if (m_mouseMode == MOUSE_MODE_PASTE_DRAGTIME)
	{
		FinalizePasteKeys();
	}
	else if (m_mouseMode == MOUSE_MODE_MOVE)
	{
		CSequencerUtils::SelectedKeys selectedKeys;
		CSequencerUtils::GetSelectedKeys(m_pSequence, selectedKeys);
		for (int i = 0; i < (int)selectedKeys.keys.size(); ++i)
		{
			UpdateAnimation(selectedKeys.keys[i].pTrack);
		}
	}

	if (GetCapture() == this)
	{
		ReleaseCapture();
	}

	if (m_pSequence == NULL)
	{
		m_bAnySelected = false;
	}

	if (m_bAnySelected)
	{
		CSequencerTrack* track = 0;
		int key = 0;
		if (FindSingleSelectedKey(track, key))
		{
			SetKeyInfo(track, key);
		}
	}
	m_keyTimeOffset = 0;
	m_secondarySelection = 0;

	//if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
	//GetIEditorImpl()->GetIUndoManager()->Accept( "Track Modify" );

	/// Move/Clone Key Undo End
	if (m_mouseMode == MOUSE_MODE_MOVE || m_mouseMode == MOUSE_MODE_CLONE)
		GetIEditorImpl()->GetIUndoManager()->Accept("Move/Clone Keys");

	CWnd::OnLButtonUp(nFlags, point);

	m_mouseMode = MOUSE_MODE_NONE;
}

//////////////////////////////////////////////////////////////////////////
bool CSequencerDopeSheetBase::IsPointValidForFragmentInPreviewDrop(const CPoint& point, COleDataObject* pDataObject) const
{
	if (!CMannequinDialog::GetCurrentInstance()->GetDockingPaneManager()->IsPaneSelected(CMannequinDialog::IDW_PREVIEWER_PANE))
	{
		return false;
	}

	UINT clipFormat = MannequinDragDropHelpers::GetFragmentClipboardFormat();
	HGLOBAL hData = pDataObject->GetGlobalData(clipFormat);

	if (nullptr == hData)
	{
		return false;
	}

	SDropFragmentData* pFragData = static_cast<SDropFragmentData*>(GlobalLock(hData));
	GlobalUnlock(hData);

	if (nullptr == pFragData)
	{
		return false;
	}

	// inside timeline.
	if (m_rcTimeline.PtInRect(point))
	{
		return false;
	}

	int key = LastKeyFromPoint(point, true);
	if (key >= 0)
	{
		return false;
	}

	// Add key here.
	int item = ItemFromPoint(point);
	CSequencerTrack* pTrack = GetTrack(item);
	if (!pTrack)
	{
		return false;
	}

	if (SEQUENCER_PARAM_FRAGMENTID != pTrack->GetParameterType())
	{
		return false;
	}

	float keyTime = TimeFromPoint(point);
	if (!IsOkToAddKeyHere(pTrack, keyTime))
	{
		return false;
	}

	CFragmentTrack* pFragTrack = static_cast<CFragmentTrack*>(pTrack);
	uint32 nScopeID = pFragTrack->GetScopeData().scopeID;

	ActionScopes scopes = CMannequinDialog::GetCurrentInstance()->Contexts()->m_controllerDef->GetScopeMask
	                      (
	  pFragData->fragID,
	  pFragData->tagState
	                      );

	if ((scopes & BIT64(nScopeID)) == 0)
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Assume IsPointValidForFragmentInPreviewDrop returned true
//////////////////////////////////////////////////////////////////////////
bool CSequencerDopeSheetBase::CreatePointForFragmentInPreviewDrop(const CPoint& point, COleDataObject* pDataObject)
{
	UINT clipFormat = MannequinDragDropHelpers::GetFragmentClipboardFormat();
	HGLOBAL hData = pDataObject->GetGlobalData(clipFormat);
	SDropFragmentData* pFragData = static_cast<SDropFragmentData*>(GlobalLock(hData));
	GlobalUnlock(hData);

	// Add key here.
	int item = ItemFromPoint(point);
	CSequencerTrack* pTrack = GetTrack(item);
	if (nullptr != pTrack)
	{
		bool keyCreated = false;
		float keyTime = TimeFromPoint(point);
		keyTime = max(keyTime, 0.0f);

		if (IsOkToAddKeyHere(pTrack, keyTime))
		{
			RecordTrackUndo(GetItem(item));
			int keyID = pTrack->CreateKey(keyTime);

			CFragmentKey newKey;
			newKey.m_time = keyTime;
			newKey.fragmentID = pFragData->fragID;
			newKey.fragOptionIdx = pFragData->option;
			newKey.tagStateFull = pFragData->tagState;
			pTrack->SetKey(keyID, &newKey);

			UnselectAllKeys(false);

			m_bAnySelected = true;
			m_fJustSelected = gEnv->pTimer->GetCurrTime();
			pTrack->SelectKey(keyID, true);

			keyCreated = true;
		}

		if (keyCreated)
		{
			// Find the first track (the preview window global Tag State track)
			CSequencerTrack* pTagTrack = NULL;
			for (int trackItem = 0; trackItem < GetCount(); ++trackItem)
			{
				pTagTrack = GetTrack(trackItem);
				if (nullptr != pTagTrack && pTagTrack->GetParameterType() == SEQUENCER_PARAM_TAGS)
				{
					break;
				}
				else
				{
					pTagTrack = nullptr;
				}
			}

			// Add global tag state to the track
			if (nullptr != pTagTrack)
			{
				RecordTrackUndo(GetItem(item));
				int keyID = pTagTrack->CreateKey(keyTime);

				CTagKey newKey;
				newKey.m_time = keyTime;
				newKey.tagState = pFragData->tagState.globalTags;
				pTagTrack->SetKey(keyID, &newKey);
			}
			Invalidate();
			UpdateAnimation(item);
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CSequencerDopeSheetBase::IsPointValidForAnimationInLayerDrop(const CPoint& point, COleDataObject* pDataObject) const
{
	if (!CMannequinDialog::GetCurrentInstance()->GetDockingPaneManager()->IsPaneSelected(CMannequinDialog::IDW_FRAGMENT_EDITOR_PANE))
		return false;

	UINT clipFormat = MannequinDragDropHelpers::GetAnimationNameClipboardFormat();
	HGLOBAL hData = pDataObject->GetGlobalData(clipFormat);

	if (nullptr == hData)
	{
		return false;
	}

	string sAnimName = static_cast<char*>(GlobalLock(hData));
	GlobalUnlock(hData);

	if (sAnimName.empty())
	{
		return false;
	}

	// inside timeline.
	if (m_rcTimeline.PtInRect(point))
	{
		return false;
	}

	int key = FirstKeyFromPoint(point, true);
	if (key >= 0)
		return false;

	// Add key here.
	int item = ItemFromPoint(point);
	CSequencerTrack* pTrack = GetTrack(item);
	if (nullptr == pTrack)
	{
		return false;
	}

	if (SEQUENCER_PARAM_ANIMLAYER != pTrack->GetParameterType())
	{
		return false;
	}

	float keyTime = TimeFromPoint(point);
	if (!IsOkToAddKeyHere(pTrack, keyTime))
	{
		return false;
	}

	CClipTrack* pClipTrack = static_cast<CClipTrack*>(pTrack);

	int nAnimID = pClipTrack->GetContext()->animSet->GetAnimIDByName(sAnimName);
	if (-1 == nAnimID)
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Assume IsPointValidForAnimationInLayerDrop returned true
//////////////////////////////////////////////////////////////////////////
bool CSequencerDopeSheetBase::CreatePointForAnimationInLayerDrop(const CPoint& point, COleDataObject* pDataObject)
{
	UINT clipFormat = MannequinDragDropHelpers::GetAnimationNameClipboardFormat();
	HGLOBAL hData = pDataObject->GetGlobalData(clipFormat);
	string sAnimName = static_cast<char*>(GlobalLock(hData));
	GlobalUnlock(hData);

	// Add key here.
	int item = ItemFromPoint(point);
	CSequencerTrack* pTrack = GetTrack(item);
	if (nullptr != pTrack)
	{
		float keyTime = TimeFromPoint(point);

		if (IsOkToAddKeyHere(pTrack, keyTime))
		{
			CClipTrack* pClipTrack = static_cast<CClipTrack*>(pTrack);
			IAnimationSet* pAnimationSet = pClipTrack->GetContext()->animSet;

			RecordTrackUndo(GetItem(item));
			int keyID = pTrack->CreateKey(keyTime);

			CClipKey newKey;
			newKey.m_time = keyTime;

			newKey.SetAnimation(sAnimName, pAnimationSet);
			pTrack->SetKey(keyID, &newKey);

			UnselectAllKeys(false);

			m_bAnySelected = true;
			m_fJustSelected = gEnv->pTimer->GetCurrTime();
			pTrack->SelectKey(keyID, true);
			UpdateAnimation(pTrack);
			Invalidate();
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	if (m_rcTimeline.PtInRect(point))
	{
		// Clicked inside timeline.
		return;
	}

	int key = LastKeyFromPoint(point, true);
	if (key >= 0)
	{
		int item = ItemFromPoint(point);
		CSequencerTrack* track = GetTrack(item);
		UnselectAllKeys(false);
		track->SelectKey(key, true);
		m_bAnySelected = true;
		m_keyTimeOffset = 0;
		Invalidate();

		SetKeyInfo(track, key, true);

		CPoint p;
		::GetCursorPos(&p);

		bool bKeyChangeInSameTrack
		  = m_pLastTrackSelectedOnSpot
		    && track == m_pLastTrackSelectedOnSpot;
		m_pLastTrackSelectedOnSpot = track;

		ShowKeyPropertyCtrlOnSpot(p.x, p.y, false, bKeyChangeInSameTrack);
		return;
	}

	// Add key here.
	int item = ItemFromPoint(point);
	CSequencerTrack* track = GetTrack(item);
	CSequencerNode* node = GetNode(item);
	if (track)
	{
		bool keyCreated = false;
		float keyTime = TimeFromPoint(point);

		if (IsOkToAddKeyHere(track, keyTime))
		{
			RecordTrackUndo(GetItem(item));
			int keyID = track->CreateKey(keyTime);

			UnselectAllKeys(false);

			m_bAnySelected = true;
			m_fJustSelected = gEnv->pTimer->GetCurrTime();
			track->SelectKey(keyID, true);

			keyCreated = true;
		}

		if (keyCreated)
		{
			Invalidate();
			UpdateAnimation(item);
		}
	}

	m_mouseMode = MOUSE_MODE_NONE;
	CWnd::OnLButtonDblClk(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::OnMButtonDown(UINT nFlags, CPoint point)
{
	SetFocus();
	SetCapture();
	m_mouseDownPos = point;
	m_bMoveDrag = true;
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::OnMButtonUp(UINT nFlags, CPoint point)
{
	m_bMoveDrag = false;
	if (GetCapture() == this)
	{
		ReleaseCapture();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::OnRButtonDown(UINT nFlags, CPoint point)
{
	//CWnd::OnRButtonDown(nFlags, point);

	HideKeyPropertyCtrlOnSpot();

	m_bCursorWasInKey = false;
	m_bMouseMovedAfterRButtonDown = false;

	SetFocus();

	if (m_rcTimeline.PtInRect(point))
	{
		// Clicked inside timeline.
		// adjust markers.
		int nMarkerStart = TimeToClient(m_timeMarked.start);
		int nMarkerEnd = TimeToClient(m_timeMarked.end);
		if ((abs(point.x - nMarkerStart)) < (abs(point.x - nMarkerEnd)))
		{
			SetStartMarker(TimeFromPoint(point));
			m_mouseMode = MOUSE_MODE_DRAGSTARTMARKER;
		}
		else
		{
			SetEndMarker(TimeFromPoint(point));
			m_mouseMode = MOUSE_MODE_DRAGENDMARKER;
		}
		SetCapture();
		return;
	}

	//int item = ItemFromPoint(point);
	//SetCurSel(item);

	m_mouseDownPos = point;

	if (nFlags & MK_SHIFT)  // alternative zoom
	{
		m_bZoomDrag = true;
		SetCapture();
		return;
	}

	int key = LastKeyFromPoint(point);
	if (key >= 0)
	{
		m_bCursorWasInKey = true;

		int item = ItemFromPoint(point);
		CSequencerTrack* track = GetTrack(item);
		track->SelectKey(key, true);
		CSequencerKey* sequencerKey = track->GetKey(key);
		m_bAnySelected = true;
		m_keyTimeOffset = 0;
		Invalidate();

		// Show a little pop-up menu for copy & delete.
		enum { COPY_KEY = 100, DELETE_KEY, EDIT_KEY_ON_SPOT, EXPLORE_KEY, OPEN_FOR_EDIT, CHK_OR_ADD_SOURCE_CONTROL };
		CMenu menu;
		menu.CreatePopupMenu();

		CSequencerUtils::SelectedKeys selectedKeys;
		CSequencerUtils::GetSelectedKeys(m_pSequence, selectedKeys);

		const bool bEnableEditOnSpot = (!selectedKeys.keys.empty());
		const bool bEnableDelete = std::all_of(selectedKeys.keys.begin(), selectedKeys.keys.end(), [](const CSequencerUtils::SelectedKey& skey) { return skey.pTrack->CanRemoveKey(skey.nKey); });

		menu.AppendMenu(bEnableEditOnSpot ? MF_STRING : MF_STRING | MF_GRAYED, EDIT_KEY_ON_SPOT, "Edit On Spot");
		menu.AppendMenu(MF_SEPARATOR, 0, "");
		menu.AppendMenu(MF_STRING, COPY_KEY, "Copy");
		menu.AppendMenu(bEnableDelete ? MF_STRING : MF_STRING | MF_GRAYED, DELETE_KEY, "Delete");

		std::vector<CString> paths;
		CString relativePath;
		CString fileName;
		CString editableExtension;
		const bool isOnDisk = sequencerKey->IsFileOnDisk();
		const bool isInPak = sequencerKey->IsFileInsidePak();

		// If the key represents a resource with a file, then allow explorer and perforce options
		if (sequencerKey->HasFileRepresentation())
		{
			sequencerKey->GetFilePaths(paths, relativePath, fileName, editableExtension);

			menu.AppendMenu(MF_SEPARATOR, 0, "");
			menu.AppendMenu(MF_STRING, OPEN_FOR_EDIT, "Open for editing");
			menu.AppendMenu(MF_STRING, EXPLORE_KEY, "Show in explorer");

			if (GetIEditorImpl()->IsSourceControlAvailable())
			{
				menu.AppendMenu(MF_SEPARATOR, 0, "");
				menu.AppendMenu(MF_STRING, CHK_OR_ADD_SOURCE_CONTROL, "Checkout or Add to Perforce.");
			}
		}

		track->InsertKeyMenuOptions(menu, key);

		CPoint p;
		::GetCursorPos(&p);
		int cmd = menu.TrackPopupMenu(TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON, p.x, p.y, this, NULL);
		if (cmd == EDIT_KEY_ON_SPOT)
		{
			bool bKeyChangeInSameTrack
			  = m_pLastTrackSelectedOnSpot
			    && selectedKeys.keys.size() == 1
			    && selectedKeys.keys[0].pTrack == m_pLastTrackSelectedOnSpot;

			if (selectedKeys.keys.size() == 1)
				m_pLastTrackSelectedOnSpot = selectedKeys.keys[0].pTrack;
			else
				m_pLastTrackSelectedOnSpot = NULL;

			ShowKeyPropertyCtrlOnSpot(p.x, p.y, selectedKeys.keys.size() > 1, bKeyChangeInSameTrack);
		}
		else if (cmd == COPY_KEY)
		{
			CopyKeys();
		}
		else if (cmd == DELETE_KEY)
		{
			DelSelectedKeys(true);
		}
		else if (cmd == OPEN_FOR_EDIT || cmd == EXPLORE_KEY)
		{
			if (isInPak && GetIEditorImpl()->IsSourceControlAvailable())
			{
				EDSSourceControlResponse getLatestRes = TryGetLatestOnFiles(paths, TRUE);
				if (getLatestRes == USER_ABORTED_OPERATION || getLatestRes == FAILED_OPERATION)
				{
					return;
				}

				EDSSourceControlResponse checkoutRes = TryCheckoutFiles(paths, TRUE);
				if (checkoutRes == USER_ABORTED_OPERATION || checkoutRes == FAILED_OPERATION)
				{
					return;
				}

				// Attempt to open file/explorer
				if (cmd == OPEN_FOR_EDIT)
				{
					TryOpenFile(relativePath, fileName, editableExtension);
				}
				else if (cmd == EXPLORE_KEY)
				{
					CFileUtil::ShowInExplorer(relativePath + fileName + editableExtension);
				}

				// Update flags, file will now be on HDD
				sequencerKey->UpdateFlags();
			}
			else if (isOnDisk)
			{
				// Attempt to open file/explorer
				if (cmd == OPEN_FOR_EDIT)
				{
					TryOpenFile(relativePath, fileName, editableExtension);
				}
				else if (cmd == EXPLORE_KEY)
				{
					CFileUtil::ShowInExplorer(relativePath + fileName + editableExtension);
				}
			}
			else
			{
				CQuestionDialog::SCritical(QObject::tr("Error"), QObject::tr("File does not exist on local HDD, and source control is unavailable to obtain file"));
			}
		}
		else if (cmd == CHK_OR_ADD_SOURCE_CONTROL)
		{
			bool bFilesExist = true;
			std::vector<bool> vExists;
			vExists.reserve(paths.size());
			for (std::vector<CString>::const_iterator iter = paths.begin(), itEnd = paths.end(); iter != itEnd; ++iter)
			{
				const bool thisFileExists = CFileUtil::FileExists(*iter);
				vExists.push_back(thisFileExists);
				bFilesExist = (bFilesExist && thisFileExists);
			}

			bool tryToGetLatestFiles = false;
			if (!bFilesExist)
			{

				QDialogButtonBox::StandardButton res = CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Some files could not be found, do you want to attempt to get the latest version from Perforce?"), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No | QDialogButtonBox::StandardButton::Cancel);

				if (res == QDialogButtonBox::StandardButton::Cancel)
				{
					return;
				}
				else if (res == QDialogButtonBox::StandardButton::Yes)
				{
					tryToGetLatestFiles = true;
				}
			}

			if (GetIEditorImpl()->IsSourceControlAvailable())
			{
				string outMsg;
				bool overall_vcs_success = true;
				for (int idx = 0, idxEnd = vExists.size(); idx < idxEnd; ++idx)
				{
					bool vcs_success = false;
					const string& currentPath = paths[idx].GetString();
					if (vExists[idx])
					{
						vcs_success = AddOrCheckoutFile(currentPath);
					}
					else if (tryToGetLatestFiles)
					{
						const string gamePath = PathUtil::ToDosPath(PathUtil::GetGameFolder() + "\\" + currentPath);
						const uint32 attr = GetIEditorImpl()->GetSourceControl()->GetFileAttributesA(gamePath);
						if ((attr != SCC_FILE_ATTRIBUTE_INVALID) && (attr & SCC_FILE_ATTRIBUTE_MANAGED))
						{
							if (GetIEditorImpl()->GetSourceControl()->GetLatestVersion(gamePath))
								vcs_success = GetIEditorImpl()->GetSourceControl()->CheckOut(gamePath);
						}
					}

					overall_vcs_success = (overall_vcs_success && vcs_success);

					if (vcs_success)
						outMsg += string("The ") + PathUtil::GetExt(currentPath) + string(" file was successfully added/checked out.\n");
					else
						outMsg += string("The ") + PathUtil::GetExt(currentPath) + string(" file could NOT be added/checked out.\n");
				}

				if (!overall_vcs_success)
					outMsg += "\nAt least one file failed to be added or checked out.\nCheck that the file exists on the disk, or if already in Perforce then is not exclusively checked out by another user.";

				CQuestionDialog::SWarning(QObject::tr(""), QObject::tr(outMsg));
			}
		}
		else
		{
			track->OnKeyMenuOption(cmd, key);
		}

	}
}

bool CSequencerDopeSheetBase::AddOrCheckoutFile(const string& filename)
{
	const uint32 attr = CFileUtil::GetAttributes(filename);

	if ((attr & SCC_FILE_ATTRIBUTE_MANAGED))
	{
		if (!(attr & SCC_FILE_ATTRIBUTE_CHECKEDOUT) && !(attr & SCC_FILE_ATTRIBUTE_BYANOTHER))
		{
			// not checked out
			return GetIEditorImpl()->GetSourceControl()->CheckOut(filename);
		}
		return false;
	}
	else if (attr & SCC_FILE_ATTRIBUTE_INPAK)
	{
		// In Pak
		return false;
	}
	else
	{
		// Ok it's good to add it!
		return GetIEditorImpl()->GetSourceControl()->Add(filename);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::TryOpenFile(const CString& relativePath, const CString& fileName, const CString& extension) const
{
	const CString filePath = relativePath + fileName + extension;
	const bool bFileExists = CFileUtil::FileExists(filePath);

	if (bFileExists)
	{
		CFileUtil::EditFile(filePath, FALSE, TRUE);
	}
	else
	{
		CFileUtil::ShowInExplorer(filePath);
	}
}

//////////////////////////////////////////////////////////////////////////
CSequencerDopeSheetBase::EDSSourceControlResponse CSequencerDopeSheetBase::TryGetLatestOnFiles(const std::vector<CString>& paths, bool bPromptUser) const
{
	ISourceControl* pSourceControl = GetIEditorImpl()->GetSourceControl();
	if (pSourceControl == NULL)
	{
		return SOURCE_CONTROL_UNAVAILABLE;
	}

	// File states
	bool bAllFilesExist = true;
	bool bAllFilesAreUpToDate = true;
	std::vector<bool> vExists;
	std::vector<bool> vNeedsUpdate;
	vExists.reserve(paths.size());
	vNeedsUpdate.reserve(paths.size());

	// Check state of all files
	for (std::vector<CString>::const_iterator iter = paths.begin(), itEnd = paths.end(); iter != itEnd; ++iter)
	{
		const CString workspacePath = PathUtil::ToDosPath(PathUtil::GetGameFolder() + "\\" + (*iter).GetString());
		ESccFileAttributes fileAttributes = (ESccFileAttributes)pSourceControl->GetFileAttributes(workspacePath);
		const bool bThisFileExists = CFileUtil::FileExists(*iter) && !(fileAttributes & SCC_FILE_ATTRIBUTE_INPAK);
		const bool bThisFileNeedsUpdate = (!bThisFileExists) || !pSourceControl->GetLatestVersion(workspacePath, GETLATEST_ONLY_CHECK);

		vExists.push_back(bThisFileExists);
		vNeedsUpdate.push_back(bThisFileNeedsUpdate);

		bAllFilesExist = bAllFilesExist && bThisFileExists;
		bAllFilesAreUpToDate = bAllFilesAreUpToDate && !bThisFileNeedsUpdate;
	}

	// User input
	bool bGetLatest = false;

	// If some files need updating, prompt user
	if (bPromptUser && (!bAllFilesAreUpToDate || !bAllFilesExist))
	{

		QDialogButtonBox::StandardButton res = CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Some files are not up to date, do you want to update them?", "Out of date files!"), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No | QDialogButtonBox::StandardButton::Cancel);

		if (res == QDialogButtonBox::StandardButton::Cancel)
		{
			return USER_ABORTED_OPERATION;
		}
		else if (res == QDialogButtonBox::StandardButton::Yes)
		{
			bGetLatest = true;
		}
	}

	// If the user wants to get latest
	if (bGetLatest)
	{
		std::vector<CString> vFailures;
		vFailures.reserve(paths.size());

		for (std::vector<CString>::const_iterator iter = paths.begin(), itEnd = paths.end(); iter != itEnd; ++iter)
		{
			const string workspacePath = PathUtil::ToDosPath(PathUtil::GetGameFolder() + "\\" + (*iter).GetString());
			const bool bThisFileUpdatedSuccessfully = pSourceControl->GetLatestVersion(workspacePath);

			if (!bThisFileUpdatedSuccessfully)
			{
				vFailures.push_back(*iter);
			}
		}

		if (vFailures.size() > 0)
		{
			CString errorMsg = "Failed to get latest on the following files:";
			for (std::vector<CString>::const_iterator iter = vFailures.begin(), itEnd = vFailures.end(); iter != itEnd; ++iter)
			{
				errorMsg += "\n";
				errorMsg += *iter;
			}
			errorMsg += "\nDo you wish to continue?";

			QDialogButtonBox::StandardButton res = CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(errorMsg), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No);

			if (res == QDialogButtonBox::StandardButton::No)
			{
				return FAILED_OPERATION;
			}
		}
	}

	return SUCCEEDED_OPERATION;
}

//////////////////////////////////////////////////////////////////////////
// N.B. Does NOT check if latest version - use TryGetLatestOnFiles()
CSequencerDopeSheetBase::EDSSourceControlResponse CSequencerDopeSheetBase::TryCheckoutFiles(const std::vector<CString>& paths, bool bPromptUser) const
{
	ISourceControl* pSourceControl = GetIEditorImpl()->GetSourceControl();
	if (pSourceControl == NULL)
	{
		return SOURCE_CONTROL_UNAVAILABLE;
	}

	// File states
	bool bAllFilesAreCheckedOut = true;
	std::vector<bool> vCheckedOut;
	vCheckedOut.reserve(paths.size());

	// Check state of all files
	for (std::vector<CString>::const_iterator iter = paths.begin(), itEnd = paths.end(); iter != itEnd; ++iter)
	{
		const CString workspacePath = PathUtil::ToDosPath(PathUtil::GetGameFolder() + "\\" + (*iter).GetString());
		ESccFileAttributes fileAttributes = (ESccFileAttributes)pSourceControl->GetFileAttributes(workspacePath);
		const bool bThisFileCheckedOut = fileAttributes & SCC_FILE_ATTRIBUTE_CHECKEDOUT;
		vCheckedOut.push_back(bThisFileCheckedOut);
		bAllFilesAreCheckedOut = bAllFilesAreCheckedOut && bThisFileCheckedOut;
	}

	// User input
	bool bCheckOut = false;

	// If files are up to date and not checked out, no checking out out of date files
	if (bPromptUser && !bAllFilesAreCheckedOut)
	{
		QDialogButtonBox::StandardButton res = CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Do you want to check out these files?"), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No | QDialogButtonBox::StandardButton::Cancel);

		if (res == QDialogButtonBox::StandardButton::Cancel)
		{
			return USER_ABORTED_OPERATION;
		}
		else if (res == QDialogButtonBox::StandardButton::Yes)
		{
			bCheckOut = true;
		}
	}

	// If user wants to check out these files
	if (bCheckOut)
	{
		std::vector<CString> vFailures;
		vFailures.reserve(paths.size());

		for (std::vector<CString>::const_iterator iter = paths.begin(), itEnd = paths.end(); iter != itEnd; ++iter)
		{
			const string workspacePath = PathUtil::ToDosPath(PathUtil::GetGameFolder() + "\\" + (*iter).GetString());
			const bool bThisFileCheckedOut = pSourceControl->CheckOut(workspacePath);

			if (!bThisFileCheckedOut)
			{
				vFailures.push_back(*iter);
			}
		}

		if (vFailures.size() > 0)
		{
			CString errorMsg = "Failed to check out the following files:";
			for (std::vector<CString>::const_iterator iter = vFailures.begin(), itEnd = vFailures.end(); iter != itEnd; ++iter)
			{
				errorMsg += "\n";
				errorMsg += *iter;
			}
			errorMsg += "\nDo you wish to continue?";

			QDialogButtonBox::StandardButton res = CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(errorMsg), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No);

			if (res == QDialogButtonBox::StandardButton::No)
			{
				return FAILED_OPERATION;
			}
		}
	}

	return SUCCEEDED_OPERATION;
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::OnRButtonUp(UINT nFlags, CPoint point)
{
	if (GetCapture() == this)
	{
		ReleaseCapture();
	}
	m_mouseMode = MOUSE_MODE_NONE;
	m_bZoomDrag = false;

	CClipboard clip;
	if (m_bCursorWasInKey == false && clip.GetTitle() == "Track view keys")
	{
		bool bHasCopiedKey = true;

		if (clip.IsEmpty())
			bHasCopiedKey = false;

		{
			XmlNodeRef copyNode = clip.Get();
			if (copyNode == NULL || strcmp(copyNode->getTag(), "CopyKeysNode"))
				bHasCopiedKey = false;
			else
			{
				int nNumTracksToPaste = copyNode->getChildCount();
				if (nNumTracksToPaste == 0)
					bHasCopiedKey = false;
			}
		}
		if (bHasCopiedKey
		    && m_bMouseMovedAfterRButtonDown == false) // Once moved, it means the user wanted to scroll, so no paste pop-up.
		{
			// Show a little pop-up menu for paste.
			enum { PASTE_KEY = 100 };
			CMenu menu;
			menu.CreatePopupMenu();
			menu.AppendMenu(MF_STRING, PASTE_KEY, "Paste");

			CPoint p;
			::GetCursorPos(&p);
			int cmd = menu.TrackPopupMenu(TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON, p.x, p.y, this, NULL);
			if (cmd == PASTE_KEY)
			{
				m_bUseClipboard = true;
				StartPasteKeys();
			}
		}
	}

	//	CWnd::OnRButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::OnMouseMove(UINT nFlags, CPoint point)
{
	// To prevent the key moving while selecting

	float fCurrentTime = gEnv->pTimer->GetCurrTime();
	float fSelectionThreshold = 0.1f;

	if (fCurrentTime - m_fJustSelected < fSelectionThreshold)
	{
		return;
	}

	m_bMouseMovedAfterRButtonDown = true;

	m_mouseOverPos = point;
	m_bMouseOverKey = false;
	if (m_bZoomDrag && (nFlags & MK_SHIFT))
	{
		float fAnchorTime = TimeFromPointUnsnapped(m_mouseDownPos);
		SetTimeScale(m_timeScale * (1.0f + (point.x - m_mouseDownPos.x) * 0.0025f), fAnchorTime);
		m_mouseDownPos = point;
		return;
	}
	else
		m_bZoomDrag = false;
	if (m_bMoveDrag)
	{
		m_scrollOffset.x += m_mouseDownPos.x - point.x;
		if (m_scrollOffset.x < m_scrollMin)
			m_scrollOffset.x = m_scrollMin;
		if (m_scrollOffset.x > m_scrollMax)
			m_scrollOffset.x = m_scrollMax;
		m_mouseDownPos = point;
		// Set the new position of the thumb (scroll box).
		SetScrollPos(SB_HORZ, m_scrollOffset.x);
		Invalidate();
		SetMouseCursor(m_crsLeftRight);
		return;
	}

	if (m_mouseMode == MOUSE_MODE_SELECT
	    || m_mouseMode == MOUSE_MODE_SELECT_WITHIN_TIME)
	{
		MouseMoveSelect(point);
	}
	else if (m_mouseMode == MOUSE_MODE_MOVE)
	{
		MouseMoveMove(point, nFlags);
	}
	else if (m_mouseMode == MOUSE_MODE_CLONE)
	{
		CloneSelectedKeys();
		m_mouseMode = MOUSE_MODE_MOVE;
	}
	else if (m_mouseMode == MOUSE_MODE_DRAGTIME)
	{
		MouseMoveDragTime(point, nFlags);
	}
	else if (m_mouseMode == MOUSE_MODE_DRAGSTARTMARKER)
	{
		MouseMoveDragStartMarker(point, nFlags);
	}
	else if (m_mouseMode == MOUSE_MODE_DRAGENDMARKER)
	{
		MouseMoveDragEndMarker(point, nFlags);
	}
	else if (m_mouseMode == MOUSE_MODE_PASTE_DRAGTIME)
	{
		MouseMovePaste(point, nFlags);
	}
	else
	{
		//////////////////////////////////////////////////////////////////////////
		if (m_mouseActionMode == SEQMODE_ADDKEY)
		{
			SetMouseCursor(m_crsAddKey);
		}
		else
		{
			MouseMoveOver(point);
		}
	}

	CWnd::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::OnPaint()
{
	//	CPaintDC dc(this); // device context for painting
	// TODO: Add your message handler code here
	//	CWnd::OnPaint();

	CPaintDC PaintDC(this);

	CMemoryDC dc(PaintDC, &m_offscreenBitmap);

	XTPPaintManager()->GradientFill(&dc, &PaintDC.m_ps.rcPaint, RGB(250, 250, 250), RGB(220, 220, 220), FALSE);

	DrawControl(dc, PaintDC.m_ps.rcPaint);
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::DrawTrack(int item, CDC* dc, CRect& rcItem)
{
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::DrawControl(CDC* dc, const CRect& rcUpdate)
{
	CRect rc;
	CRect rcTemp;

	// Draw all items.
	int count = GetCount();
	for (int i = 0; i < count; i++)
	{
		GetItemRect(i, rc);
		//if (rcTemp.IntersectRect(rc,rcUpdate) != 0)
		{
			DrawTrack(i, dc, rc);
		}
	}

	DrawTimeline(dc, rcUpdate);

	DrawSummary(dc, rcUpdate);

	DrawSelectedKeyIndicators(dc);
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::UnselectAllKeys(bool bNotify)
{
	CSequencerUtils::SelectedKeys selectedKeys;
	CSequencerUtils::GetSelectedKeys(m_pSequence, selectedKeys);
	for (int i = 0; i < selectedKeys.keys.size(); ++i)
	{
		CSequencerTrack* pTrack = selectedKeys.keys[i].pTrack;
		int keyIndex = selectedKeys.keys[i].nKey;
		pTrack->SelectKey(keyIndex, false);
	}
	m_bAnySelected = false;

	if (bNotify)
		NotifyKeySelectionUpdate();
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::SelectKeys(const CRect& rc)
{}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::SelectAllKeysWithinTimeFrame(const CRect& rc)
{
	if (m_pSequence == NULL)
		return;

	// put selection rectangle from client to item space.
	CRect rci = rc;
	rci.OffsetRect(m_scrollOffset);

	Range selTime = GetTimeRange(rci);

	for (int k = 0; k < m_pSequence->GetNodeCount(); ++k)
	{
		CSequencerNode* node = m_pSequence->GetNode(k);

		for (int i = 0; i < node->GetTrackCount(); i++)
		{
			CSequencerTrack* track = node->GetTrackByIndex(i);

			// Check which keys we intersect.
			for (int j = 0; j < track->GetNumKeys(); j++)
			{
				float time = track->GetKeyTime(j);
				if (selTime.IsInside(time))
				{
					track->SelectKey(j, true);
					m_bAnySelected = true;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CSequencerDopeSheetBase::SelectFirstKey()
{
	return SelectFirstKey(SEQUENCER_PARAM_TOTAL);
}

//////////////////////////////////////////////////////////////////////////
bool CSequencerDopeSheetBase::SelectFirstKey(const ESequencerParamType type)
{
	const bool selectAnyTrackType = (type == SEQUENCER_PARAM_TOTAL);
	UnselectAllKeys(false);

	for (int i = 0; i < GetCount(); ++i)
	{
		CSequencerTrack* pTrack = GetTrack(i);
		if (pTrack)
		{
			const ESequencerParamType trackType = pTrack->GetParameterType();
			const bool isOfRequiredType = (selectAnyTrackType || (trackType == type));
			if (isOfRequiredType)
			{
				const int numKeys = pTrack->GetNumKeys();
				if (0 < numKeys)
				{
					pTrack->SelectKey(0, true);
					NotifyKeySelectionUpdate();
					return true;
				}
			}
		}
	}

	NotifyKeySelectionUpdate();
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::DelSelectedKeys(bool bPrompt, bool bAllowUndo, bool bIgnorePermission)
{
	CSequencerUtils::SelectedKeys selectedKeys;
	CSequencerUtils::GetSelectedKeys(m_pSequence, selectedKeys);

	if (!bIgnorePermission)
	{
		bool keysCanBeDeleted = false;
		for (int k = 0; k < (int)selectedKeys.keys.size(); ++k)
		{
			CSequencerUtils::SelectedKey& skey = selectedKeys.keys[k];
			keysCanBeDeleted |= skey.pTrack->CanRemoveKey(skey.nKey);
		}

		if (!keysCanBeDeleted)
		{
			MessageBeep(MB_ICONEXCLAMATION);
			return;
		}
	}

	// Confirm.
	if (bPrompt)
	{
		const auto promptReply = CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Delete selected keys?"), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::Cancel);
		if (promptReply != QDialogButtonBox::StandardButton::Yes)
		{
			return;
		}
	}

	if (m_pSequence && bAllowUndo)
	{
		CAnimSequenceUndo(m_pSequence, "Delete Keys");
	}

	Invalidate();

	for (int k = (int)selectedKeys.keys.size() - 1; k >= 0; --k)
	{
		CSequencerUtils::SelectedKey& skey = selectedKeys.keys[k];
		if (bIgnorePermission || skey.pTrack->CanRemoveKey(skey.nKey))
		{
			skey.pTrack->RemoveKey(skey.nKey);

			UpdateAnimation(skey.pTrack);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::OffsetSelectedKeys(const float timeOffset, const bool bSnap)
{
	CSequencerUtils::SelectedKeys selectedKeys;
	CSequencerUtils::GetSelectedKeys(m_pSequence, selectedKeys);
	for (int k = 0; k < (int)selectedKeys.keys.size(); ++k)
	{
		CSequencerUtils::SelectedKey skey = selectedKeys.keys[k];

		OffsetKey(skey.pTrack, skey.nKey, timeOffset);

		UpdateAnimation(skey.pTrack);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::ScaleSelectedKeys(float timeOffset, bool bSnapKeys)
{
	if (timeOffset <= 0)
		return;
	CSequencerUtils::SelectedKeys selectedKeys;
	CSequencerUtils::GetSelectedKeys(m_pSequence, selectedKeys);
	for (int k = 0; k < (int)selectedKeys.keys.size(); ++k)
	{
		CSequencerUtils::SelectedKey skey = selectedKeys.keys[k];
		float keyt = skey.pTrack->GetKeyTime(skey.nKey) * timeOffset;
		if (bSnapKeys)
			keyt = TickSnap(keyt);
		skey.pTrack->SetKeyTime(skey.nKey, keyt);
		UpdateAnimation(skey.pTrack);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::CloneSelectedKeys()
{
	CSequencerUtils::SelectedKeys selectedKeys;
	CSequencerUtils::GetSelectedKeys(m_pSequence, selectedKeys);
	CSequencerTrack* pTrack = 0;
	// In case of multiple cloning, indices cannot be used as a solid pointer to the original.
	// So use the time of keys as an identifier, instead.
	std::vector<float> selectedKeyTimes;
	for (size_t k = 0; k < selectedKeys.keys.size(); ++k)
	{
		CSequencerUtils::SelectedKey skey = selectedKeys.keys[k];
		if (pTrack != skey.pTrack)
		{
			CUndo::Record(new CUndoSequencerSequenceModifyObject(skey.pTrack, m_pSequence));
			pTrack = skey.pTrack;
		}

		selectedKeyTimes.push_back(skey.pTrack->GetKeyTime(skey.nKey));
	}

	// Now, do the actual cloning.
	for (size_t k = 0; k < selectedKeyTimes.size(); ++k)
	{
		CSequencerUtils::SelectedKey skey = selectedKeys.keys[k];

		int keyIndex = skey.pTrack->FindKey(selectedKeyTimes[k]);
		assert(keyIndex > -1);
		if (keyIndex <= -1)
			continue;

		if (skey.pTrack->IsKeySelected(keyIndex))
		{
			// Unselect cloned key.
			skey.pTrack->SelectKey(keyIndex, false);

			int newKey = skey.pTrack->CloneKey(keyIndex);

			// Select new key.
			skey.pTrack->SelectKey(newKey, true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CSequencerDopeSheetBase::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	if (m_currCursor != NULL)
	{
		return 0;
	}

	return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

void CSequencerDopeSheetBase::SetMouseCursor(HCURSOR crs)
{
	m_currCursor = crs;
	if (m_currCursor != NULL)
		SetCursor(crs);
}

//////////////////////////////////////////////////////////////////////////
BOOL CSequencerDopeSheetBase::OnEraseBkgnd(CDC* pDC)
{
	//return CWnd::OnEraseBkgnd(pDC);
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
float CSequencerDopeSheetBase::GetCurrTime() const
{
	return m_currentTime;
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::SetCurrTime(float time, bool bForce)
{
	if (time < m_timeRange.start)
		time = m_timeRange.start;
	if (time > m_timeRange.end)
		time = m_timeRange.end;

	//bool bChange = fabs(time-m_currentTime) >= (1.0f/m_ticksStep);
	bool bChange = fabs(time - m_currentTime) >= 0.001f;

	if (bChange || bForce)
	{
		int x1 = TimeToClient(m_currentTime);
		int x2 = TimeToClient(time);
		m_currentTime = time;

		CRect rc(x1 - 3, m_rcClient.top, x1 + 4, m_rcClient.bottom);
		RedrawWindow(rc, NULL, RDW_INVALIDATE);
		CRect rc1(x2 - 3, m_rcClient.top, x2 + 4, m_rcClient.bottom);
		RedrawWindow(rc1, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
	}
}

void CSequencerDopeSheetBase::SetStartMarker(float fTime)
{
	m_timeMarked.start = fTime;
	if (m_timeMarked.start < m_timeRange.start)
		m_timeMarked.start = m_timeRange.start;
	if (m_timeMarked.start > m_timeRange.end)
		m_timeMarked.start = m_timeRange.end;
	if (m_timeMarked.start > m_timeMarked.end)
		m_timeMarked.end = m_timeMarked.start;
	Invalidate();
}

void CSequencerDopeSheetBase::SetEndMarker(float fTime)
{
	m_timeMarked.end = fTime;
	if (m_timeMarked.end < m_timeRange.start)
		m_timeMarked.end = m_timeRange.start;
	if (m_timeMarked.end > m_timeRange.end)
		m_timeMarked.end = m_timeRange.end;
	if (m_timeMarked.start > m_timeMarked.end)
		m_timeMarked.start = m_timeMarked.end;
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::SetMouseActionMode(ESequencerActionMode mode)
{
	m_mouseActionMode = mode;
	if (mode == SEQMODE_ADDKEY)
	{
		SetMouseCursor(m_crsAddKey);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CSequencerDopeSheetBase::CanCopyPasteKeys()
{
	CSequencerTrack* pCopyFromTrack = NULL;
	// are all selected keys from the same source track ?
	if (!m_bAnySelected)
		return false;
	CSequencerUtils::SelectedKeys selectedKeys;
	CSequencerUtils::GetSelectedKeys(m_pSequence, selectedKeys);
	for (int k = 0; k < (int)selectedKeys.keys.size(); ++k)
	{
		CSequencerUtils::SelectedKey skey = selectedKeys.keys[k];
		if (!pCopyFromTrack)
		{
			pCopyFromTrack = skey.pTrack;
		}
		else
		{
			if (pCopyFromTrack != skey.pTrack)
				return false;
		}
	}
	if (!pCopyFromTrack)
		return false;

	// is a destination-track selected ?
	CSequencerUtils::SelectedTracks selectedTracks;
	CSequencerUtils::GetSelectedTracks(m_pSequence, selectedTracks);
	if (selectedTracks.tracks.size() != 1)
		return false;
	CSequencerTrack* pCurrTrack = selectedTracks.tracks[0].pTrack;
	if (!pCurrTrack)
		return false;
	return (pCopyFromTrack == pCurrTrack);
}

//////////////////////////////////////////////////////////////////////////
bool CSequencerDopeSheetBase::CopyPasteKeys()
{
	CSequencerTrack* pCopyFromTrack = NULL;
	std::vector<int> vecKeysToCopy;
	if (!CanCopyPasteKeys())
		return false;
	if (!m_bAnySelected)
		return false;
	CSequencerUtils::SelectedKeys selectedKeys;
	CSequencerUtils::GetSelectedKeys(m_pSequence, selectedKeys);
	for (int k = 0; k < (int)selectedKeys.keys.size(); ++k)
	{
		CSequencerUtils::SelectedKey skey = selectedKeys.keys[k];
		pCopyFromTrack = skey.pTrack;
		vecKeysToCopy.push_back(skey.nKey);
	}
	if (!pCopyFromTrack)
		return false;

	// Get Selected Track.
	CSequencerTrack* pCurrTrack = 0;
	CSequencerUtils::SelectedTracks selectedTracks;
	CSequencerUtils::GetSelectedTracks(m_pSequence, selectedTracks);
	if (selectedTracks.tracks.size() == 1)
	{
		pCurrTrack = selectedTracks.tracks[0].pTrack;
	}

	if (!pCurrTrack)
		return false;
	for (int i = 0; i < (int)vecKeysToCopy.size(); i++)
	{
		pCurrTrack->CopyKey(pCopyFromTrack, vecKeysToCopy[i]);
	}
	Invalidate();
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::SetKeyInfo(CSequencerTrack* track, int key, bool openWindow)
{
	NotifyKeySelectionUpdate();
}

//////////////////////////////////////////////////////////////////////////
bool CSequencerDopeSheetBase::FindSingleSelectedKey(CSequencerTrack*& selTrack, int& selKey)
{
	selTrack = 0;
	selKey = 0;
	CSequencerUtils::SelectedKeys selectedKeys;
	CSequencerUtils::GetSelectedKeys(m_pSequence, selectedKeys);
	if (selectedKeys.keys.size() != 1)
		return false;
	CSequencerUtils::SelectedKey skey = selectedKeys.keys[0];
	selTrack = skey.pTrack;
	selKey = skey.nKey;
	return true;
}

//////////////////////////////////////////////////////////////////////////
int CSequencerDopeSheetBase::GetItemRect(int item, CRect& rect) const
{
	if (item < 0 || item >= GetCount())
		return -1;

	int y = -m_scrollOffset.y;

	int x = 0;
	for (int i = 0, num = (int)m_tracks.size(); i < num && i < item; i++)
	{
		y += m_tracks[i].nHeight;
	}
	rect.SetRect(x, y, x + m_rcClient.Width(), y + m_tracks[item].nHeight);
	return 0;
}

//////////////////////////////////////////////////////////////////////////
int CSequencerDopeSheetBase::ItemFromPoint(CPoint pnt) const
{
	CRect rc;
	int num = GetCount();
	for (int i = 0; i < num; i++)
	{
		GetItemRect(i, rc);
		if (rc.PtInRect(pnt))
			return i;
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::SetHorizontalExtent(int min, int max)
{
	m_scrollMin = min;
	m_scrollMax = max;
	m_itemWidth = max - min;
	int nPage = m_rcClient.Width() / 2;
	int sx = m_itemWidth - nPage + m_leftOffset;

	SCROLLINFO si;
	ZeroStruct(si);
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = m_scrollMin;
	si.nMax = m_scrollMax - nPage + m_leftOffset;
	si.nPage = m_rcClient.Width() / 2;
	si.nPos = m_scrollOffset.x;
	//si.nPage = max(0,m_rcClient.Width() - m_leftOffset*2);
	//si.nPage = 1;
	//si.nPage = 1;
	SetScrollInfo(SB_HORZ, &si, TRUE);
};

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::UpdateAnimation(int item)
{
	m_changeCount++;

	CSequencerTrack* animTrack = GetTrack(item);
	if (animTrack)
	{
		animTrack->OnChange();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::UpdateAnimation(CSequencerTrack* animTrack)
{
	m_changeCount++;

	if (animTrack)
	{
		animTrack->OnChange();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CSequencerDopeSheetBase::CopyKeys(bool bPromptAllowed, bool bUseClipboard, bool bCopyTrack)
{
	CSequencerNode* pCurNode = NULL;

	CSequencerUtils::SelectedKeys selectedKeys;
	CSequencerUtils::GetSelectedKeys(m_pSequence, selectedKeys);
	const bool bCopySelected = !bCopyTrack && !selectedKeys.keys.empty();
	bool bIsSeveralEntity = false;
	for (int k = 0; k < (int)selectedKeys.keys.size(); ++k)
	{
		CSequencerUtils::SelectedKey skey = selectedKeys.keys[k];
		if (pCurNode && pCurNode != skey.pNode)
			bIsSeveralEntity = true;
		pCurNode = skey.pNode;
	}

	if (bIsSeveralEntity)
	{
		if (bPromptAllowed)
		{
			CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("Copying aborted. You should select keys in a single node only."));
		}
		return false;
	}

	// Get Selected Track.
	CSequencerUtils::SelectedTracks selectedTracks;
	CSequencerUtils::GetSelectedTracks(m_pSequence, selectedTracks);
	if (!bCopySelected && !CSequencerUtils::IsOneTrackSelected(selectedTracks))
		return false;

	int iSelTrackCount = (int)selectedTracks.tracks.size();

	XmlNodeRef copyNode = XmlHelpers::CreateXmlNode("CopyKeysNode");

	if (bCopySelected)
	{
		CSequencerUtils::SelectedKeys selectedKeys;
		CSequencerUtils::GetSelectedKeys(m_pSequence, selectedKeys);
		int iSelKeyCount = (int)selectedKeys.keys.size();

		// Find the earliest point of all tracks (to allow us to copy relative time)
		bool bFoundStart = false;
		float fAbsoluteStartTime = 0;
		for (int i = 0; i < iSelKeyCount; ++i)
		{
			CSequencerUtils::SelectedKey sKey = selectedKeys.keys[i];

			if (!bFoundStart || sKey.fTime < fAbsoluteStartTime)
			{
				bFoundStart = true;
				fAbsoluteStartTime = sKey.fTime;
			}
		}

		CSequencerTrack* pCopyFromTrack = NULL;
		XmlNodeRef childNode;

		int firstTrack = -1;
		for (int k = 0; k < iSelKeyCount; ++k)
		{
			CSequencerUtils::SelectedKey skey = selectedKeys.keys[k];

			if (pCopyFromTrack != skey.pTrack)
			{
				// Find which track this is on the node
				int trackOffset = 0;
				for (int trackID = 0; trackID < skey.pNode->GetTrackCount(); ++trackID)
				{
					CSequencerTrack* thisTrack = skey.pNode->GetTrackByIndex(trackID);
					if (skey.pTrack == thisTrack)
					{
						if (firstTrack == -1)
						{
							firstTrack = trackOffset;
						}
						trackOffset = trackOffset - firstTrack;
						break;
					}

					if (skey.pTrack->GetParameterType() == thisTrack->GetParameterType())
					{
						trackOffset++;
					}
				}

				childNode = copyNode->newChild("Track");
				childNode->setAttr("paramId", skey.pTrack->GetParameterType());
				childNode->setAttr("paramIndex", trackOffset);
				skey.pTrack->SerializeSelection(childNode, false, true, -fAbsoluteStartTime);
				pCopyFromTrack = skey.pTrack;
			}
		}
	}
	else
	{
		// Find the earliest point of all tracks (to allow us to copy relative time)
		bool bFoundStart = false;
		float fAbsoluteStartTime = 0;
		for (int i = 0; i < iSelTrackCount; ++i)
		{
			CSequencerUtils::SelectedTrack sTrack = selectedTracks.tracks[i];
			Range trackTimeRange = sTrack.pTrack->GetTimeRange();

			if (!bFoundStart || trackTimeRange.start < fAbsoluteStartTime)
			{
				fAbsoluteStartTime = trackTimeRange.start;
			}
		}

		for (int i = 0; i < iSelTrackCount; ++i)
		{
			CSequencerTrack* pCurrTrack = selectedTracks.tracks[i].pTrack;
			if (!pCurrTrack)
				continue;

			XmlNodeRef childNode = copyNode->newChild("Track");
			childNode->setAttr("paramId", pCurrTrack->GetParameterType());
			if (pCurrTrack->GetNumKeys() > 0)
			{
				pCurrTrack->SerializeSelection(childNode, false, false, -fAbsoluteStartTime);
			}
		}
	}

	// Drag and drop operations don't want to use the clipboard
	m_bUseClipboard = bUseClipboard;
	if (bUseClipboard)
	{
		CClipboard clip;
		clip.Put(copyNode, "Track view keys");
	}
	else
	{
		m_dragClipboard = copyNode;
	}

	return true;
}

void CSequencerDopeSheetBase::CopyTrack()
{
	CSequencerUtils::SelectedTracks selectedTracks;
	CSequencerUtils::GetSelectedTracks(m_pSequence, selectedTracks);
	if (!CSequencerUtils::IsOneTrackSelected(selectedTracks))
		return;

	CSequencerTrack* pCurrTrack = selectedTracks.tracks[0].pTrack;
	if (!pCurrTrack)
		return;

	XmlNodeRef copyNode = XmlHelpers::CreateXmlNode("TrackCopy");

	XmlNodeRef childNode = copyNode->newChild("Track");
	childNode->setAttr("paramId", pCurrTrack->GetParameterType());
	if (pCurrTrack->GetNumKeys() > 0)
	{
		pCurrTrack->SerializeSelection(childNode, false, false, 0.0f);
	}

	CClipboard clip;
	clip.Put(copyNode, "Track Copy");
}

void CSequencerDopeSheetBase::StartDraggingKeys(CPoint point)
{
	CSequencerUtils::SelectedKeys selectedKeys;
	CSequencerUtils::GetSelectedKeys(m_pSequence, selectedKeys);

	// Dragging mode entered but no keys selected
	if (selectedKeys.keys.size() == 0)
	{
		return;
	}

	CSequencerTrack* pFirstTrack = selectedKeys.keys[0].pTrack;
	CSequencerNode* pFirstNode = selectedKeys.keys[0].pNode;
	for (std::vector<CSequencerUtils::SelectedKey>::iterator iter = selectedKeys.keys.begin(), iterEnd = selectedKeys.keys.end(); iter != iterEnd; ++iter)
	{
		// If we are not allowed to move this key
		if (!(*iter).pTrack->CanMoveKey((*iter).nKey))
		{
			return;
		}

		// If the keys are from different nodes then we abort
		if ((*iter).pNode != pFirstNode)
		{
			return;
		}
	}

	m_startDragMouseOverItemID = ItemFromPoint(point);
	CSequencerTrack* mouseOverTrack = GetTrack(m_startDragMouseOverItemID);
	CSequencerNode* mouseOverNode = GetNode(m_startDragMouseOverItemID);
	CSequencerTrack* firstKeyTrack = selectedKeys.keys[0].pTrack;

	// We want to know which track it is, but there could be multiple keys on a single track
	int mouseOverTrackID = 0;
	int firstKeyTrackID = 0;
	bool bMouseOverFound = false;
	bool bFirstKeyFound = false;

	for (int trackID = 0; trackID < mouseOverNode->GetTrackCount(); ++trackID)
	{
		CSequencerTrack* currentTrack = mouseOverNode->GetTrackByIndex(trackID);

		if (currentTrack->GetParameterType() == mouseOverTrack->GetParameterType())
		{
			// If this is the correct track, then we've found it
			if (currentTrack == mouseOverTrack)
			{
				bMouseOverFound = true;
			}

			if (!bMouseOverFound)
			{
				mouseOverTrackID++;
			}
		}

		if (currentTrack->GetParameterType() == firstKeyTrack->GetParameterType())
		{
			// If this is the correct track, then we've found it
			if (currentTrack == firstKeyTrack)
			{
				bFirstKeyFound = true;
			}

			if (!bFirstKeyFound)
			{
				firstKeyTrackID++;
			}
		}
	}

	// We want to know where the cursor is compared to the earliest key
	float earliestKeyTime = 9999.9f;
	for (std::vector<CSequencerUtils::SelectedKey>::iterator iter = selectedKeys.keys.begin(), iterEnd = selectedKeys.keys.end(); iter != iterEnd; ++iter)
	{
		if (earliestKeyTime > (*iter).fTime)
		{
			earliestKeyTime = (*iter).fTime;
		}
	}

	// Get the time and track offset so we can keep the clip(s) in the same place under the cursor
	m_mouseMoveStartTimeOffset = TimeFromPointUnsnapped(point) - earliestKeyTime;
	m_mouseMoveStartTrackOffset = mouseOverTrackID - firstKeyTrackID;

	// Use the copy paste interface as it is required when moving onto different tracks anyway
	if (CopyKeys(false, false))
	{
		DelSelectedKeys(false, false, true);
		StartPasteKeys();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::StartPasteKeys()
{
	if (m_pSequence)
	{
		CAnimSequenceUndo(m_pSequence, "Paste Keys");
	}

	// Store the current state of everything (we need this to be able to "undo" previous pastes during the operation)
	XmlNodeRef serializedData = XmlHelpers::CreateXmlNode("PrePaste");
	SerializeTracks(serializedData);
	m_prePasteSheetData = serializedData;

	m_mouseMode = MOUSE_MODE_PASTE_DRAGTIME;
	// If mouse over selected key, change cursor to left-right arrows.
	SetMouseCursor(m_crsLeftRight);
	SetCapture();
	m_mouseDownPos = m_mouseOverPos;

	// Paste at mouse cursor
	float time = TimeFromPointUnsnapped(m_mouseDownPos);
	PasteKeys(NULL, NULL, time - m_mouseMoveStartTimeOffset);
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::FinalizePasteKeys()
{
	GetIEditorImpl()->GetIUndoManager()->Accept("Paste Keys");
	SetMouseCursor(NULL);
	ReleaseCapture();
	m_mouseMode = MOUSE_MODE_NONE;

	// Clean out the serialized data
	m_prePasteSheetData = NULL;

	CSequencerUtils::SelectedKeys selectedKeys;
	CSequencerUtils::GetSelectedKeys(m_pSequence, selectedKeys);
	for (int i = 0; i < (int)selectedKeys.keys.size(); ++i)
	{
		UpdateAnimation(selectedKeys.keys[i].pTrack);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::SerializeTracks(XmlNodeRef& destination)
{
	int nNodeCount = m_pSequence->GetNodeCount();
	for (int nodeID = 0; nodeID < nNodeCount; ++nodeID)
	{
		CSequencerNode* pAnimNode = m_pSequence->GetNode(nodeID);

		// Create a new node and add it as a child
		XmlNodeRef animXmlNode = XmlHelpers::CreateXmlNode(pAnimNode->GetName());
		destination->addChild(animXmlNode);

		int nTrackCount = pAnimNode->GetTrackCount();
		for (int trackID = 0; trackID < nTrackCount; ++trackID)
		{
			CSequencerTrack* pAnimTrack = pAnimNode->GetTrackByIndex(trackID);

			// Add the track to the anim node
			XmlNodeRef trackXmlNode = XmlHelpers::CreateXmlNode("track");
			animXmlNode->addChild(trackXmlNode);

			//If transition property track, abort! These cannot be serialized
			if (pAnimTrack->GetParameterType() == SEQUENCER_PARAM_TRANSITIONPROPS)
				continue;

			pAnimTrack->Serialize(trackXmlNode, false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::DeserializeTracks(const XmlNodeRef& source)
{
	int nNodeCount = m_pSequence->GetNodeCount();
	for (int nodeID = 0; nodeID < nNodeCount; ++nodeID)
	{
		CSequencerNode* pAnimNode = m_pSequence->GetNode(nodeID);
		XmlNodeRef animXmlNode = source->getChild(nodeID);

		// Tracks can be added during pasting, the xml serialization won't have a child for the new track
		int xmlNodeCount = animXmlNode->getChildCount();
		int nTrackCount = pAnimNode->GetTrackCount();
		for (int trackID = 0; trackID < nTrackCount; ++trackID)
		{
			CSequencerTrack* pAnimTrack = pAnimNode->GetTrackByIndex(trackID);

			//If transition property track, abort! These cannot be serialized
			if (pAnimTrack->GetParameterType() == SEQUENCER_PARAM_TRANSITIONPROPS)
				continue;

			// Remove existing keys
			int nKeyCount = pAnimTrack->GetNumKeys();
			for (int keyID = nKeyCount - 1; keyID >= 0; --keyID)
			{
				pAnimTrack->RemoveKey(keyID);
			}

			// If the track was added (test by it not having been serialized at start) then there wont be data for it
			if (trackID < xmlNodeCount)
			{
				// Serialize in the source data
				XmlNodeRef trackXmlNode = animXmlNode->getChild(trackID);
				pAnimTrack->Serialize(trackXmlNode, true);
			}
		}
	}

	m_pSequence->UpdateKeys();
}

//////////////////////////////////////////////////////////////////////////
bool CSequencerDopeSheetBase::PasteKeys(CSequencerNode* pAnimNode, CSequencerTrack* pAnimTrack, float fTimeOffset)
{
	int nPasteToItem = -1;
	if (!pAnimNode)
	{
		// Find mouse-over item.
		int nPasteToItem = ItemFromPoint(m_mouseOverPos);
		// If not a valid item, use default item where dragging has been started
		if (nPasteToItem < 0) nPasteToItem = m_startDragMouseOverItemID;

		if (nPasteToItem >= 0)
		{
			// If actual valid item cannot be used for dropping, use default item where dragging has been started
			Item const* itemTemp = &GetItem(nPasteToItem);
			const Item& item = (!itemTemp || !(itemTemp->node) || !(itemTemp->track)) ? GetItem(m_startDragMouseOverItemID) : *itemTemp;

			// If cursor is dragged over something which isn't a valid track
			if (item.node == NULL || item.track == NULL)
			{
				Invalidate();
				return false;
			}

			pAnimNode = item.node;
			pAnimTrack = item.track;

			// If drag/drop then it's possible we are dragging from a key different from the first, so we need to figure out an offset from the cursor
			if (m_mouseMoveStartTrackOffset != 0)
			{
				int hoverTrackID = 0;
				int trackCount = pAnimNode->GetTrackCount();
				for (int trackID = 0; trackID < trackCount; ++trackID)
				{
					CSequencerTrack* currentTrack = pAnimNode->GetTrackByIndex(trackID);
					if (currentTrack->GetParameterType() == item.track->GetParameterType())
					{
						if (item.track == currentTrack)
						{
							break;
						}
						hoverTrackID++;
					}
				}

				int offsetTrackID = CLAMP(hoverTrackID - m_mouseMoveStartTrackOffset, 0, trackCount - 1);
				pAnimTrack = pAnimNode->GetTrackForParameter(item.track->GetParameterType(), offsetTrackID);
			}
		}
	}

	if (!pAnimNode || !pAnimTrack)
		return false;

	CClipboard clip;
	if (m_bUseClipboard && clip.IsEmpty())
		return false;

	// When dragging we don't save to clipboard - functionality is identical though
	XmlNodeRef copyNode = m_bUseClipboard ? clip.Get() : m_dragClipboard;
	if (copyNode == NULL || strcmp(copyNode->getTag(), "CopyKeysNode"))
		return false;

	int nNumTracksToPaste = copyNode->getChildCount();

	if (nNumTracksToPaste == 0)
		return false;

	bool bUseGivenTrackIfPossible = false;
	if (nNumTracksToPaste == 1 && pAnimTrack)
		bUseGivenTrackIfPossible = true;

	bool bIsPasted = false;

	UnselectAllKeys(false);
	m_bAnySelected = true;

	{
		// Find how many of the type we're dragging exist
		int numOfMouseOverType = 0;
		for (int i = 0; i < copyNode->getChildCount(); i++)
		{
			XmlNodeRef trackNode = copyNode->getChild(i);
			int intParamId;
			uint32 paramIndex = 0;
			trackNode->getAttr("paramId", intParamId);
			trackNode->getAttr("paramIndex", paramIndex);

			if ((ESequencerParamType)intParamId == pAnimTrack->GetParameterType())
			{
				numOfMouseOverType++;
			}
		}

		bool bNewTrackCreated = false;
		for (int i = 0; i < copyNode->getChildCount(); i++)
		{
			XmlNodeRef trackNode = copyNode->getChild(i);
			int intParamId;
			uint32 paramIndex = 0;
			trackNode->getAttr("paramId", intParamId);
			trackNode->getAttr("paramIndex", paramIndex);

			const ESequencerParamType paramId = static_cast<ESequencerParamType>(intParamId);

			// Find which track the mouse is over
			int mouseOverTrackId = 0;
			int numOfTrackType = 0;
			bool trackFound = false;
			for (int j = 0; j < pAnimNode->GetTrackCount(); ++j)
			{
				CSequencerTrack* pTrack = pAnimNode->GetTrackByIndex(j);

				if (pTrack->GetParameterType() == paramId)
				{
					if (pTrack == pAnimTrack)
					{
						trackFound = true;
					}

					if (!trackFound)
					{
						mouseOverTrackId++;
					}

					numOfTrackType++;
				}
			}

			if (!trackFound)
			{
				mouseOverTrackId = 0;
			}

			// To stop cursor from allowing you to drag further than will fit on the sheet (otherwise the clips would compress onto the last track)
			mouseOverTrackId = CLAMP(mouseOverTrackId, 0, numOfTrackType - numOfMouseOverType);

			// Sanitize the track number (don't want to create lots of spare tracks)
			int trackId = paramIndex + mouseOverTrackId;

			// If the track would be otherwise pasted on a track which doesn't exist, then abort
			trackId = CLAMP(trackId, 0, numOfTrackType - 1);

			CSequencerTrack* pTrack = pAnimNode->GetTrackForParameter(paramId, trackId);
			if (bUseGivenTrackIfPossible && pAnimTrack->GetParameterType() == paramId)
				pTrack = pAnimTrack;
			if (!pTrack)
			{
				// Make this track.
				pTrack = pAnimNode->CreateTrack(paramId);
				if (pTrack)
					bNewTrackCreated = true;
			}
			if (pTrack)
			{
				// Serialize the pasted data onto the track
				pTrack->SerializeSelection(trackNode, true, true, fTimeOffset);
			}
		}
		if (bNewTrackCreated)
		{
			GetIEditorImpl()->Notify(eNotify_OnUpdateSequencer);
			NotifyKeySelectionUpdate();
		}
	}

	Invalidate();

	return true;
}

void CSequencerDopeSheetBase::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_DELETE)
	{

		if (m_mouseMode == MOUSE_MODE_PASTE_DRAGTIME)
		{
			FinalizePasteKeys();
		}

		DelSelectedKeys(true);
	}

	if (m_mouseMode != MOUSE_MODE_PASTE_DRAGTIME)
	{
		if (nChar == 'C')
		{
			if (GetKeyState(VK_CONTROL))
				if (CopyKeys())
					return;
		}

		if (nChar == 'X')
		{
			if (GetKeyState((VK_CONTROL)))
			{
				// Copying via keyboard shouldn't use mouse offset
				m_mouseMoveStartTimeOffset = 0.0f;
				if (CopyKeys())
				{
					DelSelectedKeys(false);
					return;
				}
			}
		}

		if (nChar == 'V')
		{
			if (GetKeyState(VK_CONTROL))
			{
				// Don't want to pick up a previous drag as a paste!
				m_bUseClipboard = true;

				StartPasteKeys();
				return;
			}
		}

		if (nChar == 'Z')
		{
			if (GetKeyState(VK_CONTROL))
			{
				GetIEditorImpl()->GetIUndoManager()->Undo();
				return;
			}
		}
	}

	CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::RecordTrackUndo(const Item& item)
{
	if (item.track != 0)
	{
		CUndo undo("Track Modify");
		CUndo::Record(new CUndoSequencerSequenceModifyObject(item.track, m_pSequence));
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::ShowKeyTooltip(CSequencerTrack* pTrack, int nKey, int secondarySelection, CPoint point)
{
	if (m_lastTooltipPos.x == point.x && m_lastTooltipPos.y == point.y)
		return;

	m_lastTooltipPos = point;

	assert(pTrack);
	float time = pTrack->GetKeyTime(nKey);
	const char* desc = "";
	CString additionalDesc = "";
	float duration = 0;
	pTrack->GetTooltip(nKey, desc, duration);

	if (secondarySelection)
	{
		time = pTrack->GetSecondaryTime(nKey, secondarySelection);
		additionalDesc = pTrack->GetSecondaryDescription(nKey, secondarySelection);
	}

	CString tipText;
	if (GetTickDisplayMode() == SEQTICK_INSECONDS)
		tipText.Format("%.3f, %s", time, desc);
	else if (GetTickDisplayMode() == SEQTICK_INFRAMES)
		tipText.Format("%d, %s", pos_directed_rounding(time / m_snapFrameTime), desc);
	else assert(0);
	if (!additionalDesc.IsEmpty())
	{
		tipText.Append(" - ");
		tipText.Append(additionalDesc);
	}
	m_tooltip.UpdateTipText(tipText, this, 1);
	m_tooltip.Activate(TRUE);
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::InvalidateItem(int item)
{
	CRect rc;
	if (GetItemRect(item, rc) != -1)
	{
		InvalidateRect(rc);

	}
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnUpdateSequencerKeys:
		Invalidate();
		m_changeCount++;
		break;
	case eNotify_OnBeginGameMode:
		m_storedTime = m_currentTime;
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::OnRawInput(UINT wParam, HRAWINPUT lParam)
{
	// Forward raw input to the Track View dialog
	CWnd* pWnd = GetOwner();
	if (pWnd)
		pWnd->SendMessage(WM_INPUT, wParam, (LPARAM)lParam);
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::ClearSelection()
{
	for (int i = 0; i < GetCount(); i++)
	{
		m_tracks[i].bSelected = false;
	}
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
bool CSequencerDopeSheetBase::GetSelectedTracks(std::vector<CSequencerTrack*>& tracks) const
{
	CSequencerUtils::SelectedKeys selectedKeys;
	CSequencerUtils::GetSelectedKeys(m_pSequence, selectedKeys);

	for (std::vector<CSequencerUtils::SelectedKey>::iterator it = selectedKeys.keys.begin(),
	     end = selectedKeys.keys.end(); it != end; ++it)
	{
		stl::push_back_unique(tracks, (*it).pTrack);
	}

	CSequencerUtils::SelectedTracks selectedTracks;
	CSequencerUtils::GetSelectedTracks(m_pSequence, selectedTracks);

	for (std::vector<CSequencerUtils::SelectedTrack>::iterator it = selectedTracks.tracks.begin(),
	     end = selectedTracks.tracks.end(); it != end; ++it)
	{
		stl::push_back_unique(tracks, (*it).pTrack);
	}

	return !tracks.empty();
}

//////////////////////////////////////////////////////////////////////////
bool CSequencerDopeSheetBase::GetSelectedNodes(std::vector<CSequencerNode*>& nodes) const
{
	CSequencerUtils::SelectedKeys selectedKeys;
	CSequencerUtils::GetSelectedKeys(m_pSequence, selectedKeys);

	for (std::vector<CSequencerUtils::SelectedKey>::iterator it = selectedKeys.keys.begin(),
	     end = selectedKeys.keys.end(); it != end; ++it)
	{
		stl::push_back_unique(nodes, (*it).pNode);
	}

	CSequencerUtils::SelectedTracks selectedTracks;
	CSequencerUtils::GetSelectedTracks(m_pSequence, selectedTracks);

	for (std::vector<CSequencerUtils::SelectedTrack>::iterator it = selectedTracks.tracks.begin(),
	     end = selectedTracks.tracks.end(); it != end; ++it)
	{
		stl::push_back_unique(nodes, (*it).pNode);
	}

	return !nodes.empty();
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::SelectItem(int item)
{
	if (item >= 0 && item < GetCount())
	{
		m_tracks[item].bSelected = true;
	}
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
bool CSequencerDopeSheetBase::IsSelectedItem(int item)
{
	if (item >= 0 && item < GetCount())
	{
		return m_tracks[item].bSelected;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::OffsetKey(CSequencerTrack* track, const int keyIndex, const float timeOffset) const
{
	if (m_secondarySelection)
	{
		const float time = track->GetSecondaryTime(keyIndex, m_secondarySelection) + timeOffset;
		track->SetSecondaryTime(keyIndex, m_secondarySelection, time);
	}
	else
	{
		// do the actual setting of the key time
		const float keyt = track->GetKeyTime(keyIndex) + timeOffset;
		track->SetKeyTime(keyIndex, keyt);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::NotifyKeySelectionUpdate()
{
	GetIEditorImpl()->Notify(eNotify_OnUpdateSequencerKeySelection);

	// In the end, focus this again in order to properly catch 'KeyDown' messages.
	SetFocus();
}

//////////////////////////////////////////////////////////////////////////
bool CSequencerDopeSheetBase::IsOkToAddKeyHere(const CSequencerTrack* pTrack, float time) const
{
	const float timeEpsilon = 0.05f;

	if (!pTrack->CanAddKey(time))
	{
		return false;
	}

	for (int i = 0; i < pTrack->GetNumKeys(); ++i)
	{
		if (fabs(pTrack->GetKeyTime(i) - time) < timeEpsilon)
		{
			return false;
		}
	}

	return true;
}

void CSequencerDopeSheetBase::MouseMoveSelect(CPoint point)
{
	SetMouseCursor(NULL);
	CRect rc(m_mouseDownPos.x, m_mouseDownPos.y, point.x, point.y);
	rc.NormalizeRect();
	CRect rcClient;
	GetClientRect(rcClient);
	rc.IntersectRect(rc, rcClient);

	CDC* dc = GetDC();
	if (m_mouseMode == MOUSE_MODE_SELECT_WITHIN_TIME)
	{
		rc.top = m_rcClient.top;
		rc.bottom = m_rcClient.bottom;
	}
	dc->DrawDragRect(rc, CSize(1, 1), m_rcSelect, CSize(1, 1));
	ReleaseDC(dc);
	m_rcSelect = rc;
}

void CSequencerDopeSheetBase::MouseMoveMove(CPoint point, UINT nFlags)
{
	{
		CSequencerUtils::SelectedKeys selectedKeys;
		CSequencerUtils::GetSelectedKeys(m_pSequence, selectedKeys);
		bool canAnyKeyBeMoved = CSequencerUtils::CanAnyKeyBeMoved(selectedKeys);
		if (canAnyKeyBeMoved)
			SetMouseCursor(m_crsLeftRight);
		else
			SetMouseCursor(m_crsCross);
	}

	if (point.x < m_rcClient.left)
		point.x = m_rcClient.left;
	if (point.x > m_rcClient.right)
		point.x = m_rcClient.right;
	const CPoint ofs = point - m_mouseDownPos;

	if (nFlags & MK_CONTROL)
		SetSnappingMode(SEQKEY_SNAP_NONE);
	else if (nFlags & MK_SHIFT)
		SetSnappingMode(SEQKEY_SNAP_TICK);
	else
		SetSnappingMode(SEQKEY_SNAP_MAGNET);

	const ESequencerSnappingMode snappingMode = GetSnappingMode();
	const bool bTickSnap = snappingMode == SEQKEY_SNAP_TICK;

	if (m_mouseActionMode == SEQMODE_SCALEKEY)
	{
		// Offset all selected keys back by previous offset.
		if (m_keyTimeOffset != 0)
			ScaleSelectedKeys(1.0f / (1.0f + m_keyTimeOffset), bTickSnap);
	}
	else
	{
		// Offset all selected keys back by previous offset.
		if (m_keyTimeOffset != 0)
			OffsetSelectedKeys(-m_keyTimeOffset, bTickSnap);
	}

	CPoint keyStartPt = point;
	if (!m_secondarySelection)
	{
		keyStartPt.x -= m_grabOffset;
	}
	float newTime;
	float oldTime;
	newTime = TimeFromPointUnsnapped(keyStartPt);

	CSequencerNode* node = 0;
	const int key = LastKeyFromPoint(m_mouseDownPos);
	if ((key >= 0) && (!m_secondarySelection))
	{
		int item = ItemFromPoint(m_mouseDownPos);
		CSequencerTrack* track = GetTrack(item);
		node = GetNode(item);

		oldTime = track->GetKeyTime(key);
	}
	else
	{
		oldTime = TimeFromPointUnsnapped(m_mouseDownPos);
	}

	// Snap it, if necessary.
	if (GetSnappingMode() == SEQKEY_SNAP_MAGNET)
	{
		newTime = MagnetSnap(newTime, node);
	}
	else if (GetSnappingMode() == SEQKEY_SNAP_TICK)
	{
		newTime = TickSnap(newTime);
	}

	m_realTimeRange.ClipValue(newTime);

	const float timeOffset = newTime - oldTime;

	if (m_mouseActionMode == SEQMODE_SCALEKEY)
	{
		const float tscale = 0.005f;
		const float tofs = ofs.x * tscale;
		// Offset all selected keys by this offset.
		ScaleSelectedKeys(1.0f + tofs, bTickSnap);
		m_keyTimeOffset = tofs;
	}
	else
	{
		// Offset all selected keys by this offset.
		OffsetSelectedKeys(timeOffset, bTickSnap);
		m_keyTimeOffset = timeOffset;

		CSequencerUtils::SelectedKeys selectedKeys;
		CSequencerUtils::GetSelectedKeys(m_pSequence, selectedKeys);
		if (selectedKeys.keys.size() > 0)
		{
			ShowKeyTooltip(selectedKeys.keys[0].pTrack, selectedKeys.keys[0].nKey, m_secondarySelection, point);
		}

		if (QtUtil::IsModifierKeyDown(Qt::AltModifier) && CMannequinDialog::GetCurrentInstance()->GetDockingPaneManager()->IsPaneSelected(CMannequinDialog::IDW_FRAGMENT_EDITOR_PANE))
		{
			CMannequinDialog::GetCurrentInstance()->FragmentEditor()->SetTimeToSelectedKey();
		}
	}
	Invalidate();
}

void CSequencerDopeSheetBase::MouseMoveDragTime(CPoint point, UINT nFlags)
{
	CPoint p = point;
	if (p.x < m_rcClient.left)
		p.x = m_rcClient.left;
	if (p.x > m_rcClient.right)
		p.x = m_rcClient.right;
	if (p.y < m_rcClient.top)
		p.y = m_rcClient.top;
	if (p.y > m_rcClient.bottom)
		p.y = m_rcClient.bottom;

	bool bCtrlHeld = (nFlags & MK_CONTROL) != 0;
	bool bCtrlForSnap = gMannequinPreferences.bCtrlForScrubSnapping;
	bool bSnap = !(bCtrlHeld ^ bCtrlForSnap);

	float time = TimeFromPointUnsnapped(p);
	m_realTimeRange.ClipValue(time);
	if (bSnap)
		time = TickSnap(time);
	SetCurrTime(time);
}

void CSequencerDopeSheetBase::MouseMoveDragStartMarker(CPoint point, UINT nFlags)
{
	CPoint p = point;
	if (p.x < m_rcClient.left)
		p.x = m_rcClient.left;
	if (p.x > m_rcClient.right)
		p.x = m_rcClient.right;
	if (p.y < m_rcClient.top)
		p.y = m_rcClient.top;
	if (p.y > m_rcClient.bottom)
		p.y = m_rcClient.bottom;

	bool bCtrlHeld = (nFlags & MK_CONTROL) != 0;
	bool bCtrlForSnap = gMannequinPreferences.bCtrlForScrubSnapping;
	bool bSnap = !(bCtrlHeld ^ bCtrlForSnap);

	float time = TimeFromPointUnsnapped(p);
	m_realTimeRange.ClipValue(time);
	if (bSnap)
		time = TickSnap(time);
	SetStartMarker(time);
}

void CSequencerDopeSheetBase::MouseMoveDragEndMarker(CPoint point, UINT nFlags)
{
	CPoint p = point;
	if (p.x < m_rcClient.left)
		p.x = m_rcClient.left;
	if (p.x > m_rcClient.right)
		p.x = m_rcClient.right;
	if (p.y < m_rcClient.top)
		p.y = m_rcClient.top;
	if (p.y > m_rcClient.bottom)
		p.y = m_rcClient.bottom;

	bool bCtrlHeld = (nFlags & MK_CONTROL) != 0;
	bool bCtrlForSnap = gMannequinPreferences.bCtrlForScrubSnapping;
	bool bSnap = !(bCtrlHeld ^ bCtrlForSnap);

	float time = TimeFromPointUnsnapped(p);
	m_realTimeRange.ClipValue(time);
	if (bSnap)
		time = TickSnap(time);
	SetEndMarker(time);
}

void CSequencerDopeSheetBase::MouseMovePaste(CPoint point, UINT nFlags)
{
	// Remove the excess keys inserted during last paste operation
	if (m_prePasteSheetData != NULL)
	{
		DeserializeTracks(m_prePasteSheetData);
	}

	CPoint p = point;
	if (p.x < m_rcClient.left)
		p.x = m_rcClient.left;
	if (p.x > m_rcClient.right)
		p.x = m_rcClient.right;

	bool bCtrlHeld = (nFlags & MK_CONTROL) != 0;
	bool bCtrlForSnap = gMannequinPreferences.bCtrlForScrubSnapping;
	bool bSnap = !(bCtrlHeld ^ bCtrlForSnap);

	// Tick snapping
	float time = TimeFromPointUnsnapped(p) - m_mouseMoveStartTimeOffset;
	m_realTimeRange.ClipValue(time);
	if (bSnap)
	{
		time = TickSnap(time);
	}

	// If shift is not held, then magnet snap
	if ((nFlags & MK_SHIFT) == 0)
	{
		time = MagnetSnap(time, NULL); // second parameter isn't actually used...
	}

	PasteKeys(NULL, NULL, time);
}

void CSequencerDopeSheetBase::MouseMoveOver(CPoint point)
{
	// No mouse mode.
	SetMouseCursor(NULL);
	int key = LastKeyFromPoint(point);
	if (key >= 0)
	{
		m_bMouseOverKey = true;
		int item = ItemFromPoint(point);
		CSequencerTrack* track = GetTrack(item);

		float t1 = TimeFromPointUnsnapped(CPoint(point.x - 4, point.y));
		float t2 = TimeFromPointUnsnapped(CPoint(point.x + 4, point.y));

		int secondarySelKey;
		int secondarySelection = track->FindSecondarySelectionPt(secondarySelKey, t1, t2);
		if (secondarySelection)
		{
			if (!track->CanMoveSecondarySelection(secondarySelKey, secondarySelection))
			{
				secondarySelection = 0;
			}
		}

		if (secondarySelection)
		{
			SetMouseCursor(m_crsMoveBlend);
		}
		else if (track->IsKeySelected(key) && track->CanMoveKey(key))
		{
			// If mouse over selected key and it can be moved, change cursor to left-right arrows.
			SetMouseCursor(m_crsLeftRight);
		}
		else
		{
			SetMouseCursor(m_crsCross);
		}

		if (track)
		{
			ShowKeyTooltip(track, key, secondarySelection, point);
		}
	}
	else
	{
		if (m_tooltip.m_hWnd)
			m_tooltip.Activate(FALSE);
	}
}

int CSequencerDopeSheetBase::GetAboveKey(CSequencerTrack*& aboveTrack)
{
	CSequencerTrack* track;
	int keyIndex;
	if (FindSingleSelectedKey(track, keyIndex) == false)
		return -1;

	// First, finds a track above.
	CSequencerTrack* pLastTrack = NULL;
	for (int i = 0; i < GetCount(); ++i)
	{
		if (m_tracks[i].track == track)
			break;

		bool bValidSingleTrack = m_tracks[i].track && m_tracks[i].track->GetNumKeys() > 0;
		if (bValidSingleTrack)
			pLastTrack = m_tracks[i].track;
	}

	if (pLastTrack == NULL)
		return -1;
	else
		aboveTrack = pLastTrack;

	// A track found. Now gets a proper key index there.
	int k;
	float selectedKeyTime = track->GetKeyTime(keyIndex);
	for (k = 0; k < aboveTrack->GetNumKeys(); ++k)
	{
		if (aboveTrack->GetKeyTime(k) >= selectedKeyTime)
			break;
	}

	return k % aboveTrack->GetNumKeys();
}

int CSequencerDopeSheetBase::GetBelowKey(CSequencerTrack*& belowTrack)
{
	CSequencerTrack* track;
	int keyIndex;
	if (FindSingleSelectedKey(track, keyIndex) == false)
		return -1;

	// First, finds a track below.
	CSequencerTrack* pLastTrack = NULL;
	for (int i = GetCount() - 1; i >= 0; --i)
	{
		if (m_tracks[i].track == track)
			break;

		bool bValidSingleTrack = m_tracks[i].track && m_tracks[i].track->GetNumKeys() > 0;
		if (bValidSingleTrack)
			pLastTrack = m_tracks[i].track;
	}

	if (pLastTrack == NULL)
		return -1;
	else
		belowTrack = pLastTrack;

	// A track found. Now gets a proper key index there.
	int k;
	float selectedKeyTime = track->GetKeyTime(keyIndex);
	for (k = 0; k < belowTrack->GetNumKeys(); ++k)
	{
		if (belowTrack->GetKeyTime(k) >= selectedKeyTime)
			break;
	}

	return k % belowTrack->GetNumKeys();
}

int CSequencerDopeSheetBase::GetRightKey(CSequencerTrack*& track)
{
	int keyIndex;
	if (FindSingleSelectedKey(track, keyIndex) == false)
		return -1;

	return (keyIndex + 1) % track->GetNumKeys();
}

int CSequencerDopeSheetBase::GetLeftKey(CSequencerTrack*& track)
{
	int keyIndex;
	if (FindSingleSelectedKey(track, keyIndex) == false)
		return -1;

	return (keyIndex + track->GetNumKeys() - 1) % track->GetNumKeys();
}

float CSequencerDopeSheetBase::MagnetSnap(const float origTime, const CSequencerNode* node) const
{
	CSequencerUtils::SelectedKeys selectedKeys;
	CSequencerUtils::GetKeysInTimeRange(m_pSequence, selectedKeys,
	                                    origTime - MARGIN_FOR_MAGNET_SNAPPING / m_timeScale,
	                                    origTime + MARGIN_FOR_MAGNET_SNAPPING / m_timeScale);

	float newTime = origTime;
	if (selectedKeys.keys.size() > 0)
	{
		// By default, just use the first key that belongs to the time range as a magnet.
		newTime = selectedKeys.keys[0].fTime;
		// But if there is an in-range key in a sibling track, use it instead.
		// Here a 'sibling' means a track that belongs to a same node.
		float bestDiff = fabsf(origTime - newTime);
		for (int i = 0; i < selectedKeys.keys.size(); ++i)
		{
			const float fTime = selectedKeys.keys[i].fTime;
			const float newDiff = fabsf(origTime - fTime);
			if (newDiff <= bestDiff)
			{
				bestDiff = newDiff;
				newTime = fTime;
			}
		}
	}
	return newTime;
}

//////////////////////////////////////////////////////////////////////////
float CSequencerDopeSheetBase::FrameSnap(float time) const
{
	double t = double(time / m_snapFrameTime + 0.5);
	t = t * m_snapFrameTime;
	return t;
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::ShowKeyPropertyCtrlOnSpot(int x, int y, bool bMultipleKeysSelected, bool bKeyChangeInSameTrack)
{
	if (m_keyPropertiesDlg == NULL)
		return;

	if (m_wndPropsOnSpot.m_hWnd == 0)
	{
		CRect rc;
		GetClientRect(rc);
		m_wndPropsOnSpot.Create(WS_POPUP, rc, this);
		m_wndPropsOnSpot.ModifyStyleEx(0, WS_EX_PALETTEWINDOW | WS_EX_CLIENTEDGE);
		bKeyChangeInSameTrack = false;
	}
	m_wndPropsOnSpot.MoveWindow(x, y, 200, 200);
	if (bKeyChangeInSameTrack)
	{
		m_wndPropsOnSpot.ClearSelection();
		m_wndPropsOnSpot.ReloadValues();
	}
	else
	{
		m_keyPropertiesDlg->PopulateVariables(m_wndPropsOnSpot);
	}
	if (bMultipleKeysSelected)
		m_wndPropsOnSpot.SetDisplayOnlyModified(true);
	else
		m_wndPropsOnSpot.SetDisplayOnlyModified(false);
	m_wndPropsOnSpot.ShowWindow(SW_SHOW);
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::HideKeyPropertyCtrlOnSpot()
{
	if (m_wndPropsOnSpot.m_hWnd)
	{
		m_wndPropsOnSpot.ShowWindow(SW_HIDE);
		m_wndPropsOnSpot.ClearSelection();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetBase::SetScrollOffset(int hpos)
{
	SetScrollPos(SB_HORZ, hpos);
	m_scrollOffset.x = hpos;
	Invalidate();
}

