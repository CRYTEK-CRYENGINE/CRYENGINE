// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/StlUtils.h>
#include "CurveEditorContent.h"
#include <QWidget>
#include <CryMath/Range.h>
#include <CryMovie/AnimTime.h>

class QToolBar;

namespace CurveEditorHelpers
{
// Picks a nice color value for a curve. Wraps around after 4.
EDITOR_COMMON_API ColorB GetCurveColor(const uint n);
}

class EDITOR_COMMON_API CCurveEditor : public QWidget
{
	Q_OBJECT
public:
	CCurveEditor(QWidget* parent);
	~CCurveEditor();

	void                 SetContent(SCurveEditorContent* pContent);
	SCurveEditorContent* Content() const { return m_pContent; }

	SAnimTime            Time() const    { return m_time; }
	void                 SetTime(const SAnimTime time);

	enum class ELimit : uint
	{
		None,    // No range limiting
		Snap,    // Snap to range limits
		Clamp    // Clamp to range limits
	};

	// The background in the time and value range will be drawn a bit brighter to indicate where keys
	// should be placed. The curve editor does not enforce that the curves actually stay in those ranges.
	void  SetTimeRange(const SAnimTime start, const SAnimTime end, ELimit limit = ELimit::None);
	void  SetValueRange(const float min, const float max, ELimit limit = ELimit::None);

	void  ZoomToTimeRange(const float start, const float end);
	void  ZoomToValueRange(const float min, const float max);
	void  SetFitMargin(float margin);

	void  SetPriorityCurve(SCurveEditorCurve const& curve);
	bool  IsPriorityCurve(SCurveEditorCurve const& curve) const;

	void  SetHandlesVisible(bool bVisible);
	void  SetRulerVisible(bool bVisible);
	void  SetRulerHeight(int height);
	void  SetRulerTicksYOffset(int offset);
	void  SetTimeSliderVisible(bool bVisible);
	void  SetGridVisible(bool bVisible);
	void  SetFrameRate(SAnimTime::EFrameRate frameRate) { m_frameRate = frameRate; }
	void  SetTimeSnapping(bool snapTime)                {}
	void  SetKeySnapping(bool snapKeys)                 {}
	void  SetAllowDiscontinuous(bool allow)             { m_bAllowDiscontinuous = allow; }

	Range GetVisibleTimeRange() const;

	// Tools added to tool bar depend on options above
	void FillWithCurveToolsAndConnect(QToolBar* pToolBar);

	void paintEvent(QPaintEvent* pEvent) override;
	void mousePressEvent(QMouseEvent* pEvent) override;
	void mouseDoubleClickEvent(QMouseEvent* pEvent) override;
	void mouseMoveEvent(QMouseEvent* pEvent) override;
	void mouseReleaseEvent(QMouseEvent* pEvent) override;
	void focusOutEvent(QFocusEvent* pEvent) override;
	void wheelEvent(QWheelEvent* pEvent) override;
	void keyPressEvent(QKeyEvent* pEvent) override;
	void keyReleaseEvent(QKeyEvent* pEvent) override;

signals:
	void SignalContentChanging();
	void SignalContentAboutToBeChanged();
	void SignalContentChanged();
	void SignalScrub();
	void SignalZoom();
	void SignalPan();
	void SignalSelectionChanged();
	void SignalDrawRulerBackground(QPainter& painter, const QRect& rulerRect, const Range& visibleRange);

public slots:
	void OnDeleteSelectedKeys();
	void OnSetSelectedKeysTangentAuto();
	void OnSetSelectedKeysInTangentZero();
	void OnSetSelectedKeysInTangentStep();
	void OnSetSelectedKeysInTangentLinear();
	void OnSetSelectedKeysOutTangentZero();
	void OnSetSelectedKeysOutTangentStep();
	void OnSetSelectedKeysOutTangentLinear();
	void OnFitCurvesHorizontally();
	void OnFitCurvesVertically();
	void OnBreakTangents();
	void OnUnifyTangents();

	void OnContentDestroyed();

private:
	enum class ETangent
	{
		In,
		Out
	};

	struct SMouseHandler;
	struct SSelectionHandler;
	struct SPanHandler;
	struct SZoomHandler;
	struct SMoveHandler;
	struct SHandleMoveHandler;
	struct SScrubHandler;

	void DrawGrid(QPainter& painter);

	void CyclePriorityCurve();

	typedef stl::indirect_container<std::vector<SCurveEditorCurve*>> TCurveReferences;
	TCurveReferences                                                            GetPrioritizedCurves(bool backwards = false);

	void                                                                        LeftButtonMousePressEvent(QMouseEvent* pEvent);
	void                                                                        RightButtonMousePressEvent(QMouseEvent* pEvent);
	void                                                                        MiddleButtonMousePressEvent(QMouseEvent* pEvent);

	void                                                                        customEvent(QEvent* pEvent) override;

	void                                                                        SelectInRect(const QRect& rect, bool bToggleSelected = false, bool bDeselect = false);

	void                                                                        PreContentUpdate();
	void                                                                        ContentUpdate();
	void                                                                        PostContentUpdate();

	void                                                                        DeleteMarkedKeys();

	std::pair<SCurveEditorCurve*, Vec2>                                         HitDetectCurve(const QPoint point);
	std::pair<SCurveEditorCurve*, SCurveEditorKey*>                             HitDetectKey(const QPoint point);
	std::tuple<SCurveEditorCurve*, SCurveEditorKey, SCurveEditorKey*, ETangent> HitDetectHandle(const QPoint point);
	Vec2                                                                        ClosestPointOnCurve(const Vec2 point, const SCurveEditorCurve& curve);

	void                                                                        AddPointToCurve(Vec2 point, SCurveEditorCurve* pCurve);
	void                                                                        SortKeys(SCurveEditorCurve& curve);

	QRect                                                                       GetCurveArea();

	void                                                                        SetSelectedKeysTangentType(const ETangent tangent, const SBezierControlPoint::ETangentType type);

	SCurveEditorContent*           m_pContent;
	int                            m_curveFocusIndex;
	std::unique_ptr<SMouseHandler> m_pMouseHandler;

	SAnimTime::EFrameRate          m_frameRate;

	bool                           m_bHandlesVisible     : 1;
	bool                           m_bRulerVisible       : 1;
	bool                           m_bTimeSliderVisible  : 1;
	bool                           m_bGridVisible        : 1;
	ELimit                         m_timeLimiting        : 2;
	ELimit                         m_valueLimiting       : 2;
	bool                           m_bAllowDiscontinuous : 1;
	bool                           m_bKeysSelected       : 1;
	bool                           m_bMoveAxisLocked     : 1;

	SAnimTime                      m_time;
	Vec2                           m_zoom;
	Vec2                           m_translation;
	TRange<SAnimTime>              m_timeRange;
	Range                          m_valueRange;

	int                            m_rulerHeight;
	int                            m_rulerTicksYOffset;
	float                          m_fitMargin;
};

