// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ColorGradientCtrl_h__
#define __ColorGradientCtrl_h__
#pragma once

#include <CryMath/ISplines.h>
#include "Controls/WndGridHelper.h"

// Custom styles for this control.
#define CLRGRD_STYLE_AUTO_DELETE    0x0001
#define CLRGRD_STYLE_MARKERS_AT_TOP 0x0002
#define CLRGRD_STYLE_NO_TIME_MARKER 0x0004

// Notify event sent when spline is being modified.
#define CLRGRDN_CHANGE        (0x0001)
// Notify event sent just before when spline is modified.
#define CLRGRDN_BEFORE_CHANGE (0x0002)

//////////////////////////////////////////////////////////////////////////
// Spline control.
//////////////////////////////////////////////////////////////////////////
class CColorGradientCtrl : public CWnd
{
public:
	DECLARE_DYNAMIC(CColorGradientCtrl)

	CColorGradientCtrl();
	virtual ~CColorGradientCtrl();

	BOOL Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID);

	//Key functions
	int  GetActiveKey() { return m_nActiveKey; };
	void SetActiveKey(int nIndex);
	int  InsertKey(CPoint point);

	// Turns on/off zooming and scroll support.
	void                 SetNoZoom(bool bNoZoom)                { m_bNoZoom = false; };

	void                 SetTimeRange(float tmin, float tmax)   { m_fMinTime = tmin; m_fMaxTime = tmax; }
	void                 SetValueRange(float tmin, float tmax)  { m_fMinValue = tmin; m_fMaxValue = tmax; }
	void                 SetTooltipValueScale(float x, float y) { m_fTooltipScaleX = x; m_fTooltipScaleY = y; };
	// Lock value of first and last key to be the same.
	void                 LockFirstAndLastKeys(bool bLock)       { m_bLockFirstLastKey = bLock; }

	void                 SetSpline(ISplineInterpolator* pSpline, BOOL bRedraw = FALSE);
	ISplineInterpolator* GetSpline();

	void                 SetTimeMarker(float fTime);

	// Zoom in pixels per time unit.
	void SetZoom(float fZoom);
	void SetOrigin(float fOffset);

	typedef Functor1<CColorGradientCtrl*> UpdateCallback;
	void SetUpdateCallback(const UpdateCallback& cb) { m_updateCallback = cb; };

protected:
	enum EHitCode
	{
		HIT_NOTHING,
		HIT_KEY,
		HIT_SPLINE,
	};

	DECLARE_MESSAGE_MAP()

	virtual void PostNcDestroy();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);

	// Drawing functions
	void     DrawGradient(CDC* pDC);
	void     DrawKeys(CDC* pDC);
	void     UpdateTooltip();

	EHitCode HitTest(CPoint point);

	//Tracking support helper functions
	void     StartTracking();
	void     TrackKey(CPoint point);
	void     StopTracking();
	void     RemoveKey(int nKey);
	void     EditKey(int nKey);

	CPoint   KeyToPoint(int nKey);
	CPoint   TimeToPoint(float time);
	void     PointToTimeValue(CPoint point, float& time, ISplineInterpolator::ValueType& val);
	float    XOfsToTime(int x);
	CPoint   XOfsToPoint(int x);

	COLORREF XOfsToColor(int x);
	COLORREF TimeToColor(float time);

	void     ClearSelection();

	void     SendNotifyEvent(int nEvent);

	COLORREF ValueToColor(ISplineInterpolator::ValueType val);
	void     ColorToValue(COLORREF col, ISplineInterpolator::ValueType& val);

private:
	void OnKeyColorChanged(COLORREF color);

private:
	ISplineInterpolator* m_pSpline;

	bool                 m_bAutoDelete;
	bool                 m_bNoZoom;

	CRect                m_rcClipRect;
	CRect                m_rcGradient;
	CRect                m_rcKeys;

	CPoint               m_hitPoint;
	EHitCode             m_hitCode;
	int                  m_nHitKeyIndex;
	int                  m_nHitKeyDist;
	CPoint               m_curvePoint;

	float                m_fTimeMarker;

	int                  m_nActiveKey;
	int                  m_nKeyDrawRadius;

	bool                 m_bTracking;

	float                m_fMinTime, m_fMaxTime;
	float                m_fMinValue, m_fMaxValue;
	float                m_fTooltipScaleX, m_fTooltipScaleY;

	bool                 m_bLockFirstLastKey;

	std::vector<int>     m_bSelectedKeys;

	CToolTipCtrl         m_tooltip;
	CBitmap              m_offscreenBitmap;

	UpdateCallback       m_updateCallback;

	CWndGridHelper       m_grid;
};

#endif // __ColorGradientCtrl_h__

