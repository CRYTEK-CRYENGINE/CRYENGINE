// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SplineCtrl_h__
#define __SplineCtrl_h__
#pragma once

#include <CryMath/ISplines.h>

// Custom styles for this control.
#define SPLINE_STYLE_NOGRID         0x0001
#define SPLINE_STYLE_NO_TIME_MARKER 0x0002

// Notify event sent when spline is being modified.
#define SPLN_CHANGE        (0x0001)
// Notify event sent just before when spline is modified.
#define SPLN_BEFORE_CHANGE (0x0002)

class CTimelineCtrl;

//////////////////////////////////////////////////////////////////////////
// Spline control.
//////////////////////////////////////////////////////////////////////////
class PLUGIN_API CSplineCtrl : public CWnd
{
public:
	DECLARE_DYNAMIC(CSplineCtrl)

	CSplineCtrl();
	virtual ~CSplineCtrl();

	BOOL Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID);

	//Key functions
	int                  GetActiveKey() { return m_nActiveKey; };
	void                 SetActiveKey(int nIndex);
	int                  InsertKey(CPoint point);
	void                 ToggleKeySlope(int nIndex, int nDist);

	void                 SetGrid(int numX, int numY)            { m_gridX = numX; m_gridY = numY; };
	void                 SetTimeRange(float tmin, float tmax)   { m_fMinTime = tmin; m_fMaxTime = tmax; }
	void                 SetValueRange(float tmin, float tmax)  { m_fMinValue = tmin; m_fMaxValue = tmax; if (m_fMinValue == m_fMaxValue) m_fMaxValue = m_fMinValue + 0.001f; }
	void                 SetTooltipValueScale(float x, float y) { m_fTooltipScaleX = x; m_fTooltipScaleY = y; };
	// Lock value of first and last key to be the same.
	void                 LockFirstAndLastKeys(bool bLock)       { m_bLockFirstLastKey = bLock; }

	void                 SetSpline(ISplineInterpolator* pSpline, BOOL bRedraw = FALSE);
	ISplineInterpolator* GetSpline();

	void                 SetTimeMarker(float fTime);
	void                 SetTimelineCtrl(CTimelineCtrl* pTimelineCtrl);
	void                 UpdateToolTip();

	typedef Functor1<CSplineCtrl*> UpdateCallback;
	void SetUpdateCallback(const UpdateCallback& cb) { m_updateCallback = cb; };

protected:
	enum EHitCode
	{
		HIT_NOTHING,
		HIT_KEY,
		HIT_SPLINE,
	};

	DECLARE_MESSAGE_MAP()

	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);

	// Drawing functions
	void     DrawGrid(CDC* pDC);
	void     DrawSpline(CDC* pDC);
	void     DrawKeys(CDC* pDC);
	void     DrawTimeMarker(CDC* pDC);

	EHitCode HitTest(CPoint point);

	//Tracking support helper functions
	void   StartTracking();
	void   TrackKey(CPoint point);
	void   StopTracking();
	void   RemoveKey(int nKey);

	CPoint KeyToPoint(int nKey);
	CPoint TimeToPoint(float time);
	void   PointToTimeValue(CPoint point, float& time, float& value);
	float  XOfsToTime(int x);
	CPoint XOfsToPoint(int x);

	void   ClearSelection();
	void   ValidateSpline();

	void   SendNotifyEvent(int nEvent);

private:
	ISplineInterpolator* m_pSpline;

	CRect                m_rcClipRect;
	CRect                m_rcSpline;

	CPoint               m_hitPoint;
	EHitCode             m_hitCode;
	int                  m_nHitKeyIndex;
	int                  m_nHitKeyDist;

	float                m_fTimeMarker;

	int                  m_nActiveKey;
	int                  m_nKeyDrawRadius;

	bool                 m_bTracking;

	int                  m_gridX;
	int                  m_gridY;

	float                m_fMinTime, m_fMaxTime;
	float                m_fMinValue, m_fMaxValue;
	float                m_fTooltipScaleX, m_fTooltipScaleY;

	bool                 m_bLockFirstLastKey;

	std::vector<int>     m_bSelectedKeys;

	CToolTipCtrl         m_tooltip;

	CTimelineCtrl*       m_pTimelineCtrl;

	CBitmap              m_offscreenBitmap;
	CRect                m_TimeUpdateRect;

	UpdateCallback       m_updateCallback;
};

#endif // __SplineCtrl_h__

