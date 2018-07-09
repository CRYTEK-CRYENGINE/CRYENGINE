// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SplineCtrlEx_h__
#define __SplineCtrlEx_h__
#pragma once

#include <CryMath/ISplines.h>
#include <CryMath/Range.h>
#include "Controls/WndGridHelper.h"
#include "IUndoObject.h"

// Custom styles for this control.
#define SPLINE_STYLE_NOGRID         0x0001
#define SPLINE_STYLE_NO_TIME_MARKER 0x0002

// Notify event sent when spline is being modified.
#define SPLN_CHANGE            (0x0001)
// Notify event sent just before when spline is modified.
#define SPLN_BEFORE_CHANGE     (0x0002)
// Notify when splnie control is scrolled/zoomed.
#define SPLN_SCROLL_ZOOM       (0x0003)
// Notify when time changed.
#define SPLN_TIME_START_CHANGE (0x0001)
#define SPLN_TIME_END_CHANGE   (0x0002)
#define SPLN_TIME_CHANGE       (0x0004)

class CTimelineCtrl;

class IKeyTimeSet
{
public:
	virtual int   GetKeyTimeCount() const = 0;
	virtual float GetKeyTime(int index) const = 0;
	virtual void  MoveKeyTimes(int numChanges, int* indices, float scale, float offset, bool copyKeys) = 0;
	virtual bool  GetKeyTimeSelected(int index) const = 0;
	virtual void  SetKeyTimeSelected(int index, bool selected) = 0;
	virtual int   GetKeyCount(int index) const = 0;
	virtual int   GetKeyCountBound() const = 0;
	virtual void  BeginEdittingKeyTimes() = 0;
	virtual void  EndEdittingKeyTimes() = 0;
};

class ISplineSet
{
public:
	virtual ISplineInterpolator* GetSplineFromID(const string& id) = 0;
	virtual string               GetIDFromSpline(ISplineInterpolator* pSpline) = 0;
	virtual int                  GetSplineCount() const = 0;
	virtual int                  GetKeyCountAtTime(float time, float threshold) const = 0;
};

class ISplineCtrlUndo : public IUndoObject
{
public:
	virtual bool IsSelectionChanged() const = 0;
};

//////////////////////////////////////////////////////////////////////////
// Spline control.
//////////////////////////////////////////////////////////////////////////
class PLUGIN_API CSplineCtrlEx : public CWnd, public IKeyTimeSet
{
	typedef TRange<float> Range;
	friend class CUndoSplineCtrlEx;
public:
	DECLARE_DYNAMIC(CSplineCtrlEx)

	CSplineCtrlEx();
	virtual ~CSplineCtrlEx();

	BOOL                  Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID);

	int                   InsertKey(ISplineInterpolator* pSpline, ISplineInterpolator* pDetailSpline, CPoint point);

	void                  SetGrid(int numX, int numY)                          { m_gridX = numX; m_gridY = numY; };
	void                  SetTimeRange(const Range& range)                     { m_timeRange = range; }
	void                  SetValueRange(const Range& range)                    { m_valueRange = range; }
	void                  SetDefaultValueRange(const Range& range)             { m_defaultValueRange = range; }
	void                  SetDefaultKeyTangentType(ESplineKeyTangentType type) { m_defaultKeyTangentType = type; }
	ESplineKeyTangentType GetDefaultKeyTangentType() const                     { return m_defaultKeyTangentType; }
	void                  SetTooltipValueScale(float x, float y)               { m_fTooltipScaleX = x; m_fTooltipScaleY = y; };
	void                  SetSplineSet(ISplineSet* pSplineSet);

	void                  AddSpline(ISplineInterpolator* pSpline, ISplineInterpolator* pDetailSpline, COLORREF color);
	void                  AddSpline(ISplineInterpolator* pSpline, ISplineInterpolator* pDetailSpline, COLORREF anColorArray[4]);
	void                  RemoveSpline(ISplineInterpolator* pSpline);
	void                  RemoveAllSplines();
	int                   GetSplineCount() const      { return m_splines.size(); }
	ISplineInterpolator*  GetSpline(int nIndex) const { return m_splines[nIndex].pSpline; }

	void                  SetTimeMarker(float fTime);
	float                 GetTimeMarker() const                  { return m_fTimeMarker; }
	void                  SetTimeScale(float timeScale)          { m_fTimeScale = timeScale; }
	void                  SetGridTimeScale(float fGridTimeScale) { m_fGridTimeScale = fGridTimeScale; }
	float                 GetGridTimeScale()                     { return m_fGridTimeScale; }
	void                  SetTimelineCtrl(CTimelineCtrl* pTimelineCtrl);

	void                  SetMinTimeEpsilon(float fMinTimeEpsilon) { m_fMinTimeEpsilon = fMinTimeEpsilon; }
	float                 GetMinTimeEpsilon() const                { return m_fMinTimeEpsilon; }

	void                  SetSnapTime(bool bOn)                    { m_bSnapTime = bOn; }
	void                  SetSnapValue(bool bOn)                   { m_bSnapValue = bOn; }
	bool                  IsSnapTime() const                       { return m_bSnapTime; }
	bool                  IsSnapValue() const                      { return m_bSnapValue; }

	float                 SnapTimeToGridVertical(float time);

	void                  OnUserCommand(UINT cmd);
	void                  FitSplineToViewWidth();
	void                  FitSplineToViewHeight();

	void                  CopyKeys();
	void                  PasteKeys();

	void                  StoreUndo();

	void                  ZeroAll();
	void                  KeyAll();
	void                  SelectAll();

	void                  RemoveSelectedKeyTimes();

	void                  RedrawWindowAroundMarker();

	void                  SplinesChanged();
	void                  SetControlAmplitude(bool controlAmplitude);
	bool                  GetControlAmplitude() const;

	//void SelectPreviousKey();
	//void SelectNextKey();
	void GotoNextKey(bool previousKey);
	void RemoveAllKeysButThis();

	//////////////////////////////////////////////////////////////////////////
	// Scrolling/Zooming.
	//////////////////////////////////////////////////////////////////////////
	Vec2   ClientToWorld(CPoint point);
	CPoint WorldToClient(Vec2 v);
	Vec2   GetZoom();
	void   SetZoom(Vec2 zoom, CPoint center);
	void   SetZoom(Vec2 zoom);
	void   SetScrollOffset(Vec2 ofs);
	Vec2   GetScrollOffset();
	float  SnapTime(float time);
	float  SnapValue(float val);
	//////////////////////////////////////////////////////////////////////////

	// IKeyTimeSet Implementation
	virtual int   GetKeyTimeCount() const;
	virtual float GetKeyTime(int index) const;
	virtual void  MoveKeyTimes(int numChanges, int* indices, float scale, float offset, bool copyKeys);
	virtual bool  GetKeyTimeSelected(int index) const;
	virtual void  SetKeyTimeSelected(int index, bool selected);
	virtual int   GetKeyCount(int index) const;
	virtual int   GetKeyCountBound() const;
	virtual void  BeginEdittingKeyTimes();
	virtual void  EndEdittingKeyTimes();

	void          SetEditLock(bool bLock) { m_bEditLock = bLock; }

protected:
	enum EHitCode
	{
		HIT_NOTHING,
		HIT_KEY,
		HIT_SPLINE,
		HIT_TIMEMARKER,
		HIT_TANGENT_HANDLE
	};
	enum EEditMode
	{
		NothingMode = 0,
		SelectMode,
		TrackingMode,
		ScrollZoomMode,
		ScrollMode,
		ZoomMode,
		TimeMarkerMode,
	};

	struct SSplineInfo
	{
		COLORREF             anColorArray[4];
		ISplineInterpolator* pSpline;
		ISplineInterpolator* pDetailSpline;
	};

	DECLARE_MESSAGE_MAP()

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
	afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnCaptureChanged(CWnd* pWnd);

	// Drawing functions
	void                 DrawGrid(CDC* pDC);
	void                 DrawSpline(CDC* pDC, SSplineInfo& splineInfo, float startTime, float endTime);
	void                 DrawDetailSpline(CDC* pDC, SSplineInfo& splineInfo, float startTime, float endTime);
	void                 DrawKeys(CDC* pDC, int splineIndex, float startTime, float endTime);
	void                 DrawTimeMarker(CDC* pDC);
	void                 DrawTooltip(CDC* pDC);

	virtual bool         GetTangentHandlePts(CPoint& inTangentPt, CPoint& pt, CPoint& outTangentPt, int nSpline, int nKey, int nDimension);
	void                 DrawTangentHandle(CDC* pDC, int nSpline, int nKey, int nDimension);

	EHitCode             HitTest(CPoint point);
	ISplineInterpolator* HitSpline(CPoint point);

	//Tracking support helper functions
	void                     StartTracking(bool copyKeys);
	void                     StopTracking();
	void                     RemoveKey(ISplineInterpolator* pSpline, int nKey);
	void                     RemoveSelectedKeys();
	void                     RemoveSelectedKeyTimesImpl();
	void                     MoveSelectedKeys(Vec2 offset, bool copyKeys);
	void                     ScaleAmplitudeKeys(float time, float startValue, float offset);
	void                     TimeScaleKeys(float time, float startTime, float endTime);
	void                     ValueScaleKeys(float startValue, float endValue);
	void                     ModifySelectedKeysFlags(int nRemoveFlags, int nAddFlags);

	CPoint                   TimeToPoint(float time, ISplineInterpolator* pSpline);
	float                    TimeToXOfs(float x);
	void                     PointToTimeValue(CPoint point, float& time, float& value);
	float                    XOfsToTime(int x);
	CPoint                   XOfsToPoint(int x, ISplineInterpolator* pSpline);

	virtual void             ClearSelection();
	virtual void             SelectKey(ISplineInterpolator* pSpline, int nKey, int nDimension, bool bSelect);
	bool                     IsKeySelected(ISplineInterpolator* pSpline, int nKey, int nDimension) const;
	int                      GetNumSelected();

	void                     SetHorizontalExtent(int min, int max);

	void                     SendNotifyEvent(int nEvent);

	virtual void             SelectRectangle(CRect rc, bool bSelect);
	//////////////////////////////////////////////////////////////////////////
	bool                     CheckVirtualKey(int virtualKey);

	void                     UpdateKeyTimes() const;

	void                     ConditionalStoreUndo();

	void                     ClearSelectedKeys();
	void                     DuplicateSelectedKeys();

	virtual ISplineCtrlUndo* CreateSplineCtrlUndoObject(std::vector<ISplineInterpolator*>& splineContainer);

	CRect                m_rcClipRect;
	CRect                m_rcSpline;
	CRect                m_rcClient;

	CPoint               m_cMousePos;
	CPoint               m_cMouseDownPos;
	CPoint               m_hitPoint;
	EHitCode             m_hitCode;
	int                  m_nHitKeyIndex;
	int                  m_nHitDimension;
	int                  m_bHitIncomingHandle;
	ISplineInterpolator* m_pHitSpline;
	ISplineInterpolator* m_pHitDetailSpline;
	CPoint               m_curvePoint;

	float                m_fTimeMarker;

	int                  m_nKeyDrawRadius;

	bool                 m_bSnapTime;
	bool                 m_bSnapValue;
	bool                 m_bBitmapValid;

	int                  m_gridX;
	int                  m_gridY;

	float                m_fMinTime, m_fMaxTime;
	float                m_fMinValue, m_fMaxValue;
	float                m_fTooltipScaleX, m_fTooltipScaleY;

	float                m_fMinTimeEpsilon;

	CToolTipCtrl         m_tooltip;
	CPoint               m_lastToolTipPos;
	CString              m_tooltipText;

	CTimelineCtrl*       m_pTimelineCtrl;

	CBitmap              m_offscreenBitmap;

	CRect                m_rcSelect;

	CRect                m_TimeUpdateRect;

	float                m_fTimeScale;
	float                m_fValueScale;
	float                m_fGridTimeScale;

	Range                m_timeRange;
	Range                m_valueRange;
	Range                m_defaultValueRange;

	//! This is how often to place ticks.
	//! value of 10 means place ticks every 10 second.
	double         m_ticksStep;

	EEditMode      m_editMode;

	int            m_nLeftOffset;
	CWndGridHelper m_grid;

	//////////////////////////////////////////////////////////////////////////
	std::vector<SSplineInfo> m_splines;

	mutable bool             m_bKeyTimesDirty;
	class KeyTime
	{
	public:
		KeyTime(float time, int count) : time(time), oldTime(0.0f), selected(false), count(count) {}
		bool operator<(const KeyTime& other) const { return this->time < other.time; }
		float time;
		float oldTime;
		bool  selected;
		int   count;
	};
	mutable std::vector<KeyTime> m_keyTimes;
	mutable int                  m_totalSplineCount;

	static const float           threshold;

	bool                         m_copyKeys;
	bool                         m_startedDragging;

	bool                         m_controlAmplitude;
	ESplineKeyTangentType        m_defaultKeyTangentType;
	// Improving mouse control...
	bool                         m_boLeftMouseButtonDown;

	ISplineSet*                  m_pSplineSet;

	bool                         m_bEditLock;

	ISplineCtrlUndo*             m_pCurrentUndo;
};

#endif // __SplineCtrl_h__

