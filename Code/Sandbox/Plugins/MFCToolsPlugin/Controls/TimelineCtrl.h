// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __TimelineCtrl_h__
#define __TimelineCtrl_h__
#pragma once

#include <CryMath/Range.h>
#include "SplineCtrlEx.h"
#include "Controls/WndGridHelper.h"

// Custom styles for this control.
#define TL_STYLE_AUTO_DELETE    0x0001
#define TL_STYLE_NO_TICKS       0x0002
#define TL_STYLE_NO_TIME_MARKER 0x0004
#define TL_STYLE_NO_TEXT        0x0008

// Notify event sent when current time is change on the timeline control.
#define TLN_START_CHANGE (0x0001)
#define TLN_END_CHANGE   (0x0002)
#define TLN_CHANGE       (0x0003)
#define TLN_DELETE       (0x0004)

//////////////////////////////////////////////////////////////////////////
// Timeline control.
//////////////////////////////////////////////////////////////////////////
class PLUGIN_API CTimelineCtrl : public CWnd
{
public:
	DECLARE_DYNAMIC(CTimelineCtrl)

	CTimelineCtrl();
	virtual ~CTimelineCtrl();

	BOOL  Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID);

	void  SetTimeRange(const Range& r) { m_timeRange = r; }
	void  SetTimeMarker(float fTime);
	float GetTimeMarker() const        { return m_fTimeMarker; }

	void  SetZoom(float fZoom);
	void  SetOrigin(float fOffset);

	void  SetKeyTimeSet(IKeyTimeSet* pKeyTimeSet);

	void  SetTicksTextScale(float fScale)       { m_fTicksTextScale = fScale; }
	float GetTicksTextScale() const             { return m_fTicksTextScale; }

	void  SetTrackingSnapToFrames(bool bEnable) { m_bTrackingSnapToFrames = bEnable; }

	enum MarkerStyle
	{
		MARKER_STYLE_SECONDS,
		MARKER_STYLE_FRAMES
	};
	void  SetMarkerStyle(MarkerStyle markerStyle);
	void  SetFPS(float fps); // Only referred to if MarkerStyle == MARKER_STYLE_FRAMES.
	float GetFPS() const
	{ return m_fps; }

	void SetPlayCallback(const std::function<void()>& callback);

protected:
	enum TrackingMode
	{
		TRACKING_MODE_NONE,
		TRACKING_MODE_SET_TIME,
		TRACKING_MODE_MOVE_KEYS,
		TRACKING_MODE_SELECTION_RANGE
	};

	int  HitKeyTimes(CPoint point);
	void MoveSelectedKeyTimes(float scale, float offset);
	void SelectKeysInRange(float start, float end, bool select);

	DECLARE_MESSAGE_MAP()

	virtual void PostNcDestroy();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);

	// Drawing functions
	float   ClientToTime(int x);
	int     TimeToClient(float fTime);

	void    DrawTicks(CDC& dc);

	Range   GetVisibleRange() const;

	void    SendNotifyEvent(int nEvent);

	void    StartTracking(TrackingMode trackingMode);
	void    StopTracking();

	CString TimeToString(float time);
	// Convert time in seconds into the milliseconds.
	int     ToMillis(float time)      { return pos_directed_rounding(time * 1000.0f); };
	float   MillisToTime(int nMillis) { return nMillis / 1000.0f; }
	float   SnapTime(float time);

	void    DrawSecondTicks(CDC& dc);
	void    DrawFrameTicks(CDC& dc);

private:
	bool         m_bAutoDelete;
	CRect        m_rcClient;
	CRect        m_rcTimeline;
	float        m_fTimeMarker;
	float        m_fTicksTextScale;
	TrackingMode m_trackingMode;
	CPoint       m_lastPoint;

	Range        m_timeRange;

	float        m_timeScale;

	int          m_scrollOffset;
	int          m_leftOffset;

	// Tick every Nth millisecond.
	int                   m_nTicksStep;

	double                m_ticksStep;

	CToolTipCtrl          m_tooltip;
	CBitmap               m_offscreenBitmap;

	CImageList            m_imgMarker;

	CWndGridHelper        m_grid;

	bool                  m_bIgnoreSetTime;

	IKeyTimeSet*          m_pKeyTimeSet;
	bool                  m_bChangedKeyTimeSet;

	MarkerStyle           m_markerStyle;
	float                 m_fps;
	bool                  m_copyKeyTimes;
	bool                  m_bTrackingSnapToFrames;
	std::function<void()> m_playCallback;
};

#endif // __TimelineCtrl_h__

