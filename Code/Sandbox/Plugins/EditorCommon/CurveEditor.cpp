// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "QtUtil.h"
#include "Util/Math.h"
#include "CurveEditor.h"
#include "DrawingPrimitives/TimeSlider.h"
#include "DrawingPrimitives/Ruler.h"
#include <CryMath/Bezier_impl.h>

#include <QPainter>
#include <QMouseEvent>
#include <QColor>
#include <QToolBar>
#include "CryIcon.h"
#include "EditorStyleHelper.h"

#include <EditorFramework/Events.h>

#pragma warning (push)
#pragma warning (disable : 4554)

#define INDEX_NOT_OUT_OF_RANGE PREFAST_SUPPRESS_WARNING(6201)
#define NO_BUFFER_OVERRUN      PREFAST_SUPPRESS_WARNING(6385 6386)
#include <CryPhysics/polynomial.h>

#pragma warning (pop)


namespace Private_CurveEditor
{
const int kDefaultRulerHeight = 16;
const int kRulerShadowHeight = 6;
const int kRulerMarkHeight = 8;
const float kHitDistance = 5.0f;
const float kSnapDistance = 10.0f;
const float kMinZoom = 0.00001f;
const float kMaxZoom = 1000.0f;
const float kDefaultFitMargin = 30.0f;

const QPointF kPointRectExtent = QPointF(2.5f, 2.5f);

Vec2 TransformPointToScreen(const Vec2 zoom, const Vec2 translation, QRect curveArea, Vec2 point)
{
	Vec2 transformedPoint = Vec2(point.x * zoom.x, point.y * -zoom.y) + translation;
	transformedPoint.x *= curveArea.width();
	transformedPoint.y *= curveArea.height();
	return Vec2(transformedPoint.x + curveArea.left(), transformedPoint.y + curveArea.top());
}

Vec2 TransformPointFromScreen(const Vec2 zoom, const Vec2 translation, QRect curveArea, Vec2 point)
{
	Vec2 transformedPoint = Vec2((point.x - curveArea.left()) / curveArea.width(), (point.y - curveArea.top()) / curveArea.height()) - translation;
	transformedPoint.x /= zoom.x;
	transformedPoint.y /= -zoom.y;
	return transformedPoint;
}

QPointF Vec2ToPoint(Vec2 point)
{
	return QPointF(point.x, point.y);
}

Vec2 PointToVec2(QPointF point)
{
	return Vec2(point.x(), point.y());
}

QPainterPath CreatePathFromCurve(const SCurveEditorCurve& curve, std::function<Vec2(Vec2)> transformFunc)
{
	QPainterPath path;

	const Vec2 startPoint(curve.m_keys[0].m_time.ToFloat(), curve.m_keys[0].m_controlPoint.m_value);
	const Vec2 startTransformed = transformFunc(startPoint);
	path.moveTo(startTransformed.x, startTransformed.y);

	const auto endIter = curve.m_keys.end() - 1;
	for (auto iter = curve.m_keys.begin(); iter != endIter; ++iter)
	{
		const SCurveEditorKey segmentStartKey = curve.ApplyOutTangent(*iter, true);
		const SCurveEditorKey segmentEndKey = curve.ApplyInTangent(*(iter + 1), true);

		const Vec2 p0 = Vec2(segmentStartKey.m_time.ToFloat(), segmentStartKey.m_controlPoint.m_value);
		const Vec2 p3 = Vec2(segmentEndKey.m_time.ToFloat(), segmentEndKey.m_controlPoint.m_value);
		const Vec2 p1 = p0 + segmentStartKey.m_controlPoint.m_outTangent;
		const Vec2 p2 = p3 + segmentEndKey.m_controlPoint.m_inTangent;

		const QPointF p0Transformed = Vec2ToPoint(transformFunc(p0));
		const QPointF p1Transformed = Vec2ToPoint(transformFunc(p1));
		const QPointF p2Transformed = Vec2ToPoint(transformFunc(p2));
		const QPointF p3Transformed = Vec2ToPoint(transformFunc(p3));
		path.moveTo(p0Transformed);
		path.cubicTo(p1Transformed, p2Transformed, p3Transformed);
	}

	return path;
}

QPainterPath CreateExtrapolatedPathFromCurve(const SCurveEditorCurve& curve, std::function<Vec2(Vec2)> transformFunc, float windowWidth)
{
	QPainterPath path;

	if (curve.m_keys.size() > 0)
	{
		const Vec2 startPoint = Vec2(curve.m_keys[0].m_time.ToFloat(), curve.m_keys[0].m_controlPoint.m_value);
		const Vec2 startTransformed = transformFunc(startPoint);
		if (startTransformed.x > 0.0f)
		{
			path.moveTo(std::min(startTransformed.x, windowWidth), startTransformed.y);
			path.lineTo(0.0f, startTransformed.y);
		}

		const Vec2 endPoint(curve.m_keys.back().m_time.ToFloat(), curve.m_keys.back().m_controlPoint.m_value);
		const Vec2 endTransformed = transformFunc(endPoint);
		if (endTransformed.x < windowWidth)
		{
			path.moveTo(std::max(endTransformed.x, 0.0f), endTransformed.y);
			path.lineTo(windowWidth, endTransformed.y);
		}
	}
	else
	{
		const Vec2 pointOnCurve = Vec2(0.0f, curve.m_defaultValue);
		const Vec2 pointOnTransformed = transformFunc(pointOnCurve);
		path.moveTo(0.0, pointOnTransformed.y);
		path.lineTo(windowWidth, pointOnTransformed.y);
	}

	QVector<qreal> dashPattern;
	dashPattern << 16 << 8;

	QPainterPathStroker stroker;
	stroker.setCapStyle(Qt::RoundCap);
	stroker.setDashPattern(dashPattern);
	stroker.setWidth(0.5);

	return stroker.createStroke(path);
}

QPainterPath CreateDiscontinuityPathFromCurve(const SCurveEditorCurve& curve, std::function<Vec2(Vec2)> transformFunc)
{
	QPainterPath path;

	if (curve.m_keys.size() > 0)
	{
		const auto endIter = curve.m_keys.end() - 1;

		for (auto iter = curve.m_keys.begin(); iter != endIter; ++iter)
		{
			const SCurveEditorKey segmentStartKey = curve.ApplyOutTangent(*iter, true);
			const SCurveEditorKey segmentEndKey = curve.ApplyInTangent(*(iter + 1), true);

			if (segmentStartKey.m_controlPoint.m_value != iter->m_controlPoint.m_value)
			{
				const Vec2 start = Vec2(segmentStartKey.m_time.ToFloat(), segmentStartKey.m_controlPoint.m_value);
				const Vec2 end = Vec2(iter->m_time.ToFloat(), iter->m_controlPoint.m_value);

				const QPointF startTransformed = Vec2ToPoint(transformFunc(start));
				const QPointF endTransformed = Vec2ToPoint(transformFunc(end));

				path.moveTo(startTransformed);
				path.lineTo(endTransformed);
			}

			if (segmentEndKey.m_controlPoint.m_value != (iter + 1)->m_controlPoint.m_value)
			{
				const Vec2 start = Vec2(segmentEndKey.m_time.ToFloat(), segmentEndKey.m_controlPoint.m_value);
				const Vec2 end = Vec2((iter + 1)->m_time.ToFloat(), (iter + 1)->m_controlPoint.m_value);

				const QPointF startTransformed = Vec2ToPoint(transformFunc(start));
				const QPointF endTransformed = Vec2ToPoint(transformFunc(end));

				path.moveTo(startTransformed);
				path.lineTo(endTransformed);
			}
		}
	}

	QVector<qreal> dashPattern;
	dashPattern << 2 << 10;

	QPainterPathStroker stroker;
	stroker.setCapStyle(Qt::RoundCap);
	stroker.setDashPattern(dashPattern);
	stroker.setWidth(0.5);

	return stroker.createStroke(path);
}

void DrawPointRect(QPainter& painter, QPointF point, const QColor& color)
{
	painter.setBrush(QBrush(color));
	painter.setPen(QColor(0, 0, 0));
	painter.drawRect(QRectF(point - kPointRectExtent, point + kPointRectExtent));
}

void DrawArrow(QPainter& painter, QPointF point, QPointF dir, const QColor& color)
{
	painter.setBrush(QBrush(color));
	painter.setPen(QColor(0, 0, 0));
	QPointF trans(-dir.y(), dir.x());
	QPointF points[3] =
	{
		point + dir * (kHitDistance * 0.5f),
		point + (-dir + trans) * (kHitDistance * 0.5f),
		point + (-dir - trans) * (kHitDistance * 0.5f),
	};
	painter.drawPolygon(points, 3);
}

void DrawKeys(QPainter& painter, const SCurveEditorCurve& curve, std::function<Vec2(Vec2)> transformFunc, const bool bDrawHandles)
{
	const QColor tangentColor = QtUtil::InterpolateColor(QColor(), QColor(curve.m_color.r, curve.m_color.g, curve.m_color.b, curve.m_color.a), 0.3f);
	const QPen tangentPen = QPen(tangentColor, 2.5);

	for (auto iter = curve.m_keys.begin(); iter != curve.m_keys.end(); ++iter)
	{
		SCurveEditorKey key = *iter;

		const Vec2 keyPoint = Vec2(key.m_time.ToFloat(), key.m_controlPoint.m_value);
		const QPointF transformedKeyPoint = Vec2ToPoint(transformFunc(keyPoint));

		const bool bIsFirstKey = (iter == curve.m_keys.begin());
		const bool bIsLastKey = (iter == (curve.m_keys.end() - 1));
		if (!bIsFirstKey)
			key = curve.ApplyInTangent(key, false, &*iter);
		if (!bIsLastKey)
			key = curve.ApplyOutTangent(key, false, &*iter);

		if (key.m_bSelected && (key.m_controlPoint.m_inTangentType != SBezierControlPoint::ETangentType::Step) && !bIsFirstKey && bDrawHandles)
		{
			// Draw incoming tangent
			const Vec2 tangentHandlePoint = keyPoint + key.m_controlPoint.m_inTangent;
			const QPointF transformedTangentHandlePoint = Vec2ToPoint(transformFunc(tangentHandlePoint));
			painter.setPen(tangentPen);
			painter.drawLine(transformedKeyPoint, transformedTangentHandlePoint);
			DrawPointRect(painter, transformedTangentHandlePoint, GetStyleHelper()->curveTangent());
		}

		if (key.m_bSelected && (key.m_controlPoint.m_outTangentType != SBezierControlPoint::ETangentType::Step) && !bIsLastKey && bDrawHandles)
		{
			// Draw outgoing tangent
			const Vec2 tangentHandlePoint = keyPoint + key.m_controlPoint.m_outTangent;
			const QPointF transformedTangentHandlePoint = Vec2ToPoint(transformFunc(tangentHandlePoint));
			painter.setPen(tangentPen);
			painter.drawLine(transformedKeyPoint, transformedTangentHandlePoint);
			DrawPointRect(painter, transformedTangentHandlePoint, GetStyleHelper()->curveTangent());
		}

		const QColor pointColor = key.m_bSelected ? GetStyleHelper()->curveSelectedPoint() : GetStyleHelper()->curvePoint();
		DrawPointRect(painter, transformedKeyPoint, pointColor);
	}
}

void ForEachKey(SCurveEditorContent& content, std::function<void (SCurveEditorCurve& curve, SCurveEditorKey& key)> fun)
{
	for (auto& curve : content.m_curves)
	{
		for (auto& key : curve.m_keys)
		{
			fun(curve, key);
		}
	}
}

#pragma warning (push)
#pragma warning (disable : 4554)

Vec2 ClosestPointOnBezierSegment(const Vec2 point, const float t0, const float t1, const float p0, const float p1, const float p2, const float p3)
{
	// If values are too close the distance function is too flat to be useful. We just assume the curve is flat then
	if ((p0 * p0 + p1 * p1 + p2 * p2 + p3 * p3) < 1e-10f)
	{
		return Vec2(point.x, p0);
	}

	const float deltaTime = (t1 - t0);
	const float deltaTimeSq = deltaTime * deltaTime;

	// Those are just the normal cubic Bezier formulas B(t) and B'(t) in collected polynomial form
	const P3f cubicBezierPoly = P3f(-p0 + 3.0f * p1 - 3.0f * p2 + p3) + P2f(3.0f * p0 - 6.0f * p1 + 3.0f * p2) + P1f(3.0f * p1 - 3.0f * p0) + p0;
	const P2f cubicBezierDerivativePoly = P2f(-3.0f * p0 + 9.0f * p1 - 6.0f * p2 + 3.0f * (p3 - p2)) + P1f(6.0f * p0 - 12.0f * p1 + 6.0f * p2) - 3.0f * p0 + 3.0f * p1;

	// lerp(t, t0, t1) in polynomial form
	const P1f timePoly = P1f(deltaTime) + t0;

	// Derivative of the distance function (cubicBezierPoly - point.y) ^ 2 + (timePoly - point.x) ^ 2
	const auto distanceDerivativePoly = (cubicBezierDerivativePoly * (cubicBezierPoly - point.y) + (timePoly - point.x) * deltaTime) * 2.0f;

	// The point of minimum distance must be at one of the roots of the distance derivative or at the start/end of the segment
	float checkPoints[7];
	const uint numRoots = distanceDerivativePoly.findroots(0.0f, 1.0f, checkPoints + 2);

	// Start and end of segment
	checkPoints[0] = 0.0f;
	checkPoints[1] = 1.0f;

	// Find the closest point among all the candidates
	Vec2 closestPoint;
	float minDistanceSq = std::numeric_limits<float>::max();
	for (uint i = 0; i < numRoots + 2; ++i)
	{
		const Vec2 rootPoint(Lerp(t0, t1, checkPoints[i]), Bezier::Evaluate(checkPoints[i], p0, p1, p2, p3));
		const float deltaX = rootPoint.x - point.x;
		const float deltaY = rootPoint.y - point.y;
		const float distSq = deltaX * deltaX + deltaY * deltaY;
		if (distSq < minDistanceSq)
		{
			closestPoint = rootPoint;
			minDistanceSq = distSq;
		}
	}

	return closestPoint;
}

Vec2 ClosestPointOn2DBezierSegment(const Vec2 point, const Vec2 p0, const Vec2 p1, const Vec2 p2, const Vec2 p3)
{
	// If values are too close the distance function is too flat to be useful. We just assume the curve is flat then
	if ((p0.y * p0.y + p1.y * p1.y + p2.y * p2.y + p3.y * p3.y) < 1e-10f)
	{
		return Vec2(point.x, p0.y);
	}

	// Those are just the normal cubic Bezier formulas B(t) and B'(t) in collected polynomial form
	const P3f xCubicBezierPoly = P3f(-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) + P2f(3.0f * p0.x - 6.0f * p1.x + 3.0f * p2.x) + P1f(3.0f * p1.x - 3.0f * p0.x) + p0.x;
	const P2f xCubicBezierDerivativePoly = P2f(-3.0f * p0.x + 9.0f * p1.x - 6.0f * p2.x + 3.0f * (p3.x - p2.x)) + P1f(6.0f * p0.x - 12.0f * p1.x + 6.0f * p2.x) - 3.0f * p0.x + 3.0f * p1.x;
	const P3f yCubicBezierPoly = P3f(-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) + P2f(3.0f * p0.y - 6.0f * p1.y + 3.0f * p2.y) + P1f(3.0f * p1.y - 3.0f * p0.y) + p0.y;
	const P2f yCubicBezierDerivativePoly = P2f(-3.0f * p0.y + 9.0f * p1.y - 6.0f * p2.y + 3.0f * (p3.y - p2.y)) + P1f(6.0f * p0.y - 12.0f * p1.y + 6.0f * p2.y) - 3.0f * p0.y + 3.0f * p1.y;

	// Derivative of the distance function (yCubicBezierPoly - point.y) ^ 2 + (xCubicBezierPoly - point.x) ^ 2
	const auto distanceDerivativePoly = yCubicBezierDerivativePoly * (yCubicBezierPoly - point.y) + xCubicBezierDerivativePoly * (xCubicBezierPoly - point.x);

	// The point of minimum distance must be at one of the roots of the distance derivative or at the start/end of the segment
	float checkPoints[7];
	const uint numRoots = distanceDerivativePoly.findroots(0.0f, 1.0f, checkPoints + 2);

	// Start and end of segment
	checkPoints[0] = 0.0f;
	checkPoints[1] = 1.0f;

	// Find the closest point among all the candidates
	Vec2 closestPoint;
	float minDistanceSq = std::numeric_limits<float>::max();
	for (uint i = 0; i < numRoots + 2; ++i)
	{
		const Vec2 rootPoint(Bezier::Evaluate(checkPoints[i], p0.x, p1.x, p2.x, p3.x), Bezier::Evaluate(checkPoints[i], p0.y, p1.y, p2.y, p3.y));
		const float deltaX = rootPoint.x - point.x;
		const float deltaY = rootPoint.y - point.y;
		const float distSq = deltaX * deltaX + deltaY * deltaY;
		if (distSq < minDistanceSq)
		{
			closestPoint = rootPoint;
			minDistanceSq = distSq;
		}
	}
	
	return closestPoint;
}

// This works for 1D and 2D bezier because the y range of values is not affected by the x bezier in the 2D case.
Range GetBezierSegmentValueRange(const SCurveEditorKey& startKey, const SCurveEditorKey& endKey)
{
	const float p0 = startKey.m_controlPoint.m_value;
	const float p1 = p0 + startKey.m_controlPoint.m_outTangent.y;
	const float p3 = endKey.m_controlPoint.m_value;
	const float p2 = p3 + endKey.m_controlPoint.m_inTangent.y;

	Range valueRange(Range::EMPTY);
	valueRange |= p0;
	valueRange |= p3;

	const P2f cubicBezierDerivativePoly = P2f(-3.0f * p0 + 9.0f * p1 - 6.0f * p2 + 3.0f * (p3 - p2)) + P1f(6.0f * p0 - 12.0f * p1 + 6.0f * p2) - 3.0f * p0 + 3.0f * p1;

	float roots[2];
	const uint numRoots = cubicBezierDerivativePoly.findroots(0.0f, 1.0f, roots);
	for (uint i = 0; i < numRoots; ++i)
	{
		const float rootValue = Bezier::Evaluate(roots[i], p0, p1, p2, p3);
		valueRange |= rootValue;
	}

	return valueRange;
}

std::pair<float, Vec2> ClosestPointOnSimpleSegment(const Vec2& point, const SCurveEditorKey& segmentStartKey, const SCurveEditorKey& segmentEndKey, std::function<Vec2(Vec2)> transformFunc)
{
	const Vec2 a = transformFunc(Vec2(segmentStartKey.m_time.ToFloat(), segmentStartKey.m_controlPoint.m_value));
	const Vec2 b = transformFunc(Vec2(segmentEndKey.m_time.ToFloat(), segmentEndKey.m_controlPoint.m_value));

	const Vec2 ab = b - a;
	const Vec2 ap = point - a;

	const float lenghtSqrAB = ab.GetLengthSquared();
	const float dot = ap.Dot(ab);
	const float dist = dot / lenghtSqrAB;

	Vec2 closestOnSegment;
	if (dist < 0.0f)
	{
		closestOnSegment = a;
	}
	else if (dist > 1.0f)
	{
		closestOnSegment = b;
	}
	else
	{
		closestOnSegment = a + ab * dist;
	}

	const float distanceToSegment = (closestOnSegment - point).GetLengthSquared();
	return{ distanceToSegment, closestOnSegment };
}

void SaveMinIfLess(float newDistance, const Vec2& newPoint, float& minDistance, Vec2& closestPoint)
{
	if (newDistance < minDistance)
	{
		closestPoint = newPoint;
		minDistance = newDistance;
	}
}

bool ArePointsAligned(const Vec2& p0, const Vec2& p1, const Vec2& lineDir, float tolerance)
{
	auto tmpVec = p1 - p0;
	auto dot = lineDir.Dot(tmpVec);
	Vec2 closest = tmpVec - lineDir * dot;
	return closest.GetLength() < tolerance;
}

bool ArePointsAligned(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3, float tolerance = 0.1)
{
	Vec2 lineDir = p3 - p0;
	lineDir.Normalize();
	return ArePointsAligned(p0, p1, lineDir, tolerance) && ArePointsAligned(p0, p2, lineDir, tolerance);
}

}

#pragma warning (pop)

struct CCurveEditor::SMouseHandler
{
	virtual void mousePressEvent(QMouseEvent* pEvent)       {}
	virtual void mouseDoubleClickEvent(QMouseEvent* pEvent) {}
	virtual void mouseMoveEvent(QMouseEvent* pEvent)        {}
	virtual void mouseReleaseEvent(QMouseEvent* pEvent)     {}
	virtual void focusOutEvent(QFocusEvent* pEvent)         {}
	virtual void paintOver(QPainter& painter)               {}
};

struct CCurveEditor::SSelectionHandler : public CCurveEditor::SMouseHandler
{
	CCurveEditor* m_pCurveEditor;
	QPoint        m_startPoint;
	QRect         m_rect;
	bool          m_bAdd;

	SSelectionHandler(CCurveEditor* pCurveEditor, bool bAdd) : m_pCurveEditor(pCurveEditor), m_bAdd(bAdd) {}

	void mousePressEvent(QMouseEvent* pEvent) override
	{
		m_startPoint = pEvent->pos();
		m_rect = QRect(m_startPoint, m_startPoint + QPoint(1, 1));
	}

	void mouseMoveEvent(QMouseEvent* pEvent) override
	{
		m_rect = QRect(m_startPoint, pEvent->pos() + QPoint(1, 1));
	}

	void mouseReleaseEvent(QMouseEvent* pEvent) override
	{
		const bool bToggleSelected = (pEvent->modifiers() & Qt::CTRL) != 0;
		const bool bDeselect = (pEvent->modifiers() & Qt::ALT) != 0;

		m_pCurveEditor->SelectInRect(m_rect, bToggleSelected, bDeselect);
	}

	void paintOver(QPainter& painter) override
	{
		painter.save();
		QColor highlightColor = GetStyleHelper()->highlightColor();
		QColor highlightColorA = QColor(highlightColor.red(), highlightColor.green(), highlightColor.blue(), 128);
		painter.setPen(QPen(highlightColor));
		painter.setBrush(QBrush(highlightColorA));
		painter.drawRect(QRectF(m_rect));
		painter.restore();
	}
};

struct CCurveEditor::SPanHandler : public CCurveEditor::SMouseHandler
{
	CCurveEditor* m_pCurveEditor;
	QPoint        m_startPoint;
	Vec2          m_startTranslation;

	SPanHandler(CCurveEditor* pCurveEditor) : m_pCurveEditor(pCurveEditor)
	{
	}

	void mousePressEvent(QMouseEvent* pEvent) override
	{
		m_startPoint = QPoint(int(pEvent->x()), int(pEvent->y()));
		m_startTranslation = m_pCurveEditor->m_translation;
	}

	void mouseMoveEvent(QMouseEvent* pEvent) override
	{
		const Vec2 windowSize((float)m_pCurveEditor->size().width(), (float)m_pCurveEditor->size().height());

		const int pixelDeltaX = pEvent->x() - m_startPoint.x();
		const int pixelDeltaY = pEvent->y() - m_startPoint.y();

		const float deltaX = float(pixelDeltaX) / (windowSize.x);
		const float deltaY = float(pixelDeltaY) / (windowSize.y);

		const Vec2 delta(deltaX, deltaY);
		m_pCurveEditor->m_translation = m_startTranslation + delta;
		m_pCurveEditor->update();

		m_pCurveEditor->SignalPan();
	}
};

struct CCurveEditor::SZoomHandler : public CCurveEditor::SMouseHandler
{
	CCurveEditor* m_pCurveEditor;
	Vec2          m_pivot;
	QPoint        m_lastPoint;

	SZoomHandler(CCurveEditor* pCurveEditor) : m_pCurveEditor(pCurveEditor)
	{
	}

	void mousePressEvent(QMouseEvent* pEvent) override
	{
		m_lastPoint = QPoint(int(pEvent->x()), int(pEvent->y()));

		const QRect curveArea = m_pCurveEditor->GetCurveArea();
		const float pivotXNormalized = (float)(m_lastPoint.x() - curveArea.left()) / (float)curveArea.width();
		const float pivotYNormalized = (float)(m_lastPoint.y() - curveArea.top()) / (float)curveArea.height();
		m_pivot = Vec2(pivotXNormalized, pivotYNormalized);
	}

	void mouseMoveEvent(QMouseEvent* pEvent) override
	{
		using namespace Private_CurveEditor;
		const Vec2 windowSize((float)m_pCurveEditor->size().width(), (float)m_pCurveEditor->size().height());

		const int pixelDeltaX = pEvent->x() - m_lastPoint.x();
		const int pixelDeltaY = -(pEvent->y() - m_lastPoint.y());
		m_lastPoint = QPoint(int(pEvent->x()), int(pEvent->y()));

		Vec2& translation = m_pCurveEditor->m_translation;
		Vec2& zoom = m_pCurveEditor->m_zoom;

		const float pivotX = (m_pivot.x - translation.x) / zoom.x;
		const float pivotY = (m_pivot.y - translation.y) / zoom.y;

		zoom.x *= pow(1.2f, (float)pixelDeltaX * 0.03f);
		zoom.y *= pow(1.2f, (float)pixelDeltaY * 0.03f);

		zoom.x = clamp_tpl(zoom.x, kMinZoom, kMaxZoom);
		zoom.y = clamp_tpl(zoom.y, kMinZoom, kMaxZoom);

		// Adjust translation so pivot point stays at same x and y position on screen
		translation.x += ((m_pivot.x - translation.x) / zoom.x - pivotX) * zoom.x;
		translation.y += ((m_pivot.y - translation.y) / zoom.y - pivotY) * zoom.y;

		m_pCurveEditor->update();
	}
};

struct CCurveEditor::SScrubHandler : SMouseHandler
{
	CCurveEditor* m_pCurveEditor;
	SAnimTime     m_startThumbPosition;
	QPoint        m_startPoint;

	SScrubHandler(CCurveEditor* pCurveEditor) : m_pCurveEditor(pCurveEditor)
	{
	}

	void mousePressEvent(QMouseEvent* ev) override
	{
		using namespace Private_CurveEditor;
		QPoint point = QPoint(ev->pos().x(), ev->pos().y());

		const Vec2 pointInCurveSpace = TransformPointFromScreen(m_pCurveEditor->m_zoom, m_pCurveEditor->m_translation, m_pCurveEditor->GetCurveArea(), PointToVec2(point));

		m_pCurveEditor->m_time = clamp_tpl(SAnimTime(pointInCurveSpace.x), m_pCurveEditor->m_timeRange.start, m_pCurveEditor->m_timeRange.end);
		m_startThumbPosition = m_pCurveEditor->m_time;
		m_startPoint = point;

		m_pCurveEditor->SignalScrub();
	}

	void Apply(QMouseEvent* ev, bool continuous)
	{
		QPoint point = QPoint(ev->pos().x(), ev->pos().y());

		bool shift = ev->modifiers().testFlag(Qt::ShiftModifier);
		bool control = ev->modifiers().testFlag(Qt::ControlModifier);

		const float deltaX = (float)(point.x() - m_startPoint.x());
		const float width = (float)m_pCurveEditor->size().width();
		float delta = float(deltaX) / (width * m_pCurveEditor->m_zoom.x);

		if (shift)
		{
			delta *= 0.01f;
		}

		if (control)
		{
			delta *= 0.1f;
		}

		m_pCurveEditor->m_time = clamp_tpl(m_startThumbPosition + SAnimTime(delta), m_pCurveEditor->m_timeRange.start, m_pCurveEditor->m_timeRange.end);
		m_pCurveEditor->SignalScrub();
	}

	void mouseMoveEvent(QMouseEvent* ev) override
	{
		Apply(ev, true);
	}

	void mouseReleaseEvent(QMouseEvent* ev) override
	{
		Apply(ev, false);
	}
};

struct CCurveEditor::SMoveHandler : public CCurveEditor::SMouseHandler
{
	Vec2                   m_startPoint;
	Vec2                   m_prevPoint;
	SAnimTime              m_minSelectedTime;
	std::vector<SAnimTime> m_keyTimes;
	std::vector<float>     m_keyValues;
	CCurveEditor*          m_pCurveEditor;

	SMoveHandler(CCurveEditor* pCurveEditor, bool bCycleSelection)
		: m_startPoint(0.0f, 0.0f)
		, m_prevPoint(0.0f, 0.0f)
		, m_pCurveEditor(pCurveEditor)
	{}

	void mousePressEvent(QMouseEvent* pEvent) override
	{
		using namespace Private_CurveEditor;
		const QPoint currentPos = pEvent->pos();
		m_startPoint = TransformPointFromScreen(m_pCurveEditor->m_zoom, m_pCurveEditor->m_translation, m_pCurveEditor->GetCurveArea(), PointToVec2(currentPos));
		m_prevPoint = m_startPoint;
		StoreKeyPositions();
		m_pCurveEditor->PreContentUpdate();
	}

	void mouseMoveEvent(QMouseEvent* pEvent) override
	{
		using namespace Private_CurveEditor;
		const QPoint currentPos = pEvent->pos();
		const QPointF startScreenPos = Vec2ToPoint(TransformPointToScreen(m_pCurveEditor->m_zoom, m_pCurveEditor->m_translation, m_pCurveEditor->GetCurveArea(), m_startPoint));
		const QPointF screenOffset = currentPos - startScreenPos;
		const bool lockVecticaly = abs(screenOffset.x()) < abs(screenOffset.y());
		const Vec2 transformedPos = TransformPointFromScreen(m_pCurveEditor->m_zoom, m_pCurveEditor->m_translation, m_pCurveEditor->GetCurveArea(), PointToVec2(currentPos));
		const Vec2 offset = transformedPos - m_startPoint;
		const Vec2 delta = transformedPos - m_prevPoint;
		const Vec2 snapDist = TransformPointFromScreen(m_pCurveEditor->m_zoom, Vec2(0), m_pCurveEditor->GetCurveArea(), Vec2(kSnapDistance));

		int k = 0;
		for (auto& curve : m_pCurveEditor->m_pContent->m_curves)
		{
			for (auto& key : curve.m_keys)
			{
				if (key.m_bSelected)
				{
					if (m_pCurveEditor->m_bMoveAxisLocked && lockVecticaly)
					{
						key.m_time = m_keyTimes[k];
					}
					else
					{
						// Adjust time
						SAnimTime deltaTime = SAnimTime(offset.x);
						key.m_time = m_keyTimes[k] + deltaTime;
						LimitRange(key.m_time, m_pCurveEditor->m_timeRange, m_pCurveEditor->m_timeLimiting, snapDist.x, delta.x);
					}

					if (m_pCurveEditor->m_bMoveAxisLocked && !lockVecticaly)
					{
						key.m_controlPoint.m_value = m_keyValues[k];
					}
					else
					{
						// Adjust values
						key.m_controlPoint.m_value = m_keyValues[k] + offset.y;
						LimitRange(key.m_controlPoint.m_value, m_pCurveEditor->m_valueRange, m_pCurveEditor->m_valueLimiting, -snapDist.y, offset.y);
					}

					key.m_bModified = true;
					k++;
				}
			}
			m_pCurveEditor->SortKeys(curve);
		}

		m_pCurveEditor->ContentUpdate();

		m_prevPoint = transformedPos;
	}

	void focusOutEvent(QFocusEvent* pEvent) override
	{
		RestoreKeyPositions();
	}

	void mouseReleaseEvent(QMouseEvent* pEvent) override
	{
		m_pCurveEditor->PostContentUpdate();
	}

	void StoreKeyPositions()
	{
		m_minSelectedTime = SAnimTime::Max();

		for (auto& curve : m_pCurveEditor->m_pContent->m_curves)
		{
			for (auto& key : curve.m_keys)
			{
				if (key.m_bSelected)
				{
					m_keyTimes.push_back(key.m_time);
					m_keyValues.push_back(key.m_controlPoint.m_value);
					m_minSelectedTime = min(m_minSelectedTime, key.m_time);
				}
			}
		}
	}

	void RestoreKeyPositions()
	{
		SCurveEditorContent* pContent = m_pCurveEditor->m_pContent;

		auto timeIter = m_keyTimes.begin();
		auto valueIter = m_keyValues.begin();

		for (auto& curve : m_pCurveEditor->m_pContent->m_curves)
		{
			for (auto& key : curve.m_keys)
			{
				if (key.m_bSelected)
				{
					key.m_time = *(timeIter++);
					key.m_controlPoint.m_value = *(valueIter++);
					key.m_bModified = false;
				}
			}
		}
	}

	template<class T>
	void LimitRange(T& val, TRange<T> range, ELimit limiting, float snap, float delta)
	{
		if (limiting == ELimit::Clamp)
		{
			range.ClipValue(val);
		}
		else if (limiting == ELimit::Snap)
		{
			if (delta < 0.0f && val < range.start && val >= range.start - T(snap))
				val = range.start;
			else if (delta > 0.0f && val > range.end && val <= range.end + T(snap))
				val = range.end;
		}
	}
};

struct CCurveEditor::SHandleMoveHandler : public CCurveEditor::SMouseHandler
{
	CCurveEditor*                     m_pCurveEditor;
	SCurveEditorKey                   m_appliedHandlesKey;
	SCurveEditorKey*                  m_pKey;
	CCurveEditor::ETangent            m_tangent;
	bool                              m_bBezier2D;
	Vec2                              m_startPoint;
	Vec2                              m_inTangentStartPosition;
	Vec2                              m_outTangentStartPosition;
	SBezierControlPoint::ETangentType m_inTangentStartType;
	SBezierControlPoint::ETangentType m_outTangentStartType;

	SHandleMoveHandler(CCurveEditor* pCurveEditor, const SCurveEditorCurve& curve, const SCurveEditorKey& appliedHandlesKey, SCurveEditorKey* pKey, CCurveEditor::ETangent tangent)
		: m_pCurveEditor(pCurveEditor)
		, m_appliedHandlesKey(appliedHandlesKey)
		, m_pKey(pKey)
		, m_tangent(tangent)
		, m_bBezier2D(curve.m_bBezier2D)
		, m_inTangentStartPosition(ZERO)
		// , m_inTangentStartType(SBezierControlPoint::ETangentType::Auto)
		, m_inTangentStartType(SBezierControlPoint::ETangentType::Smooth)
		, m_outTangentStartPosition(ZERO)
		// , m_outTangentStartType(SBezierControlPoint::ETangentType::Auto)
		, m_outTangentStartType(SBezierControlPoint::ETangentType::Smooth)
	{
	}

	void mousePressEvent(QMouseEvent* pEvent) override
	{
		using namespace Private_CurveEditor;
		const Vec2 currentPos = PointToVec2(pEvent->pos());
		m_startPoint = TransformPointFromScreen(
		  m_pCurveEditor->m_zoom, m_pCurveEditor->m_translation,
		  m_pCurveEditor->GetCurveArea(), currentPos);

		m_inTangentStartPosition = m_appliedHandlesKey.m_controlPoint.m_inTangent;
		m_inTangentStartType = m_appliedHandlesKey.m_controlPoint.m_inTangentType;
		m_outTangentStartPosition = m_appliedHandlesKey.m_controlPoint.m_outTangent;
		m_outTangentStartType = m_appliedHandlesKey.m_controlPoint.m_outTangentType;
	}

	void mouseMoveEvent(QMouseEvent* pEvent) override
	{
		using namespace Private_CurveEditor;
		const Vec2 currentPos = PointToVec2(pEvent->pos());
		const Vec2 transformedPos = TransformPointFromScreen(
		  m_pCurveEditor->m_zoom, m_pCurveEditor->m_translation,
		  m_pCurveEditor->GetCurveArea(), currentPos);

		if (m_tangent == CCurveEditor::ETangent::In)
		{
			Vec2 newPos = m_inTangentStartPosition + (transformedPos - m_startPoint);
			m_pKey->m_controlPoint.m_inTangent = newPos;
			m_pKey->m_controlPoint.m_inTangentType = SBezierControlPoint::ETangentType::Custom;

			if (!m_pKey->m_controlPoint.m_bBreakTangents)
			{
				m_pKey->m_controlPoint.m_outTangent.y = m_pKey->m_controlPoint.m_outTangent.x ? newPos.y / newPos.x * m_pKey->m_controlPoint.m_outTangent.x : 0.0f;
				m_pKey->m_controlPoint.m_outTangentType = SBezierControlPoint::ETangentType::Custom;
			}
		}
		else
		{
			Vec2 newPos = m_outTangentStartPosition + (transformedPos - m_startPoint);
			m_pKey->m_controlPoint.m_outTangent = newPos;
			m_pKey->m_controlPoint.m_outTangentType = SBezierControlPoint::ETangentType::Custom;

			if (!m_pKey->m_controlPoint.m_bBreakTangents)
			{
				m_pKey->m_controlPoint.m_inTangent.y = m_pKey->m_controlPoint.m_inTangent.x ? newPos.y / newPos.x * m_pKey->m_controlPoint.m_inTangent.x : 0.0f;
				m_pKey->m_controlPoint.m_inTangentType = SBezierControlPoint::ETangentType::Custom;
			}
		}

		m_pKey->m_bModified = true;
	}

	void focusOutEvent(QFocusEvent* pEvent) override
	{
		m_pKey->m_controlPoint.m_inTangent = m_inTangentStartPosition;
		m_pKey->m_controlPoint.m_inTangentType = m_inTangentStartType;
		m_pKey->m_controlPoint.m_outTangent = m_outTangentStartPosition;
		m_pKey->m_controlPoint.m_outTangentType = m_outTangentStartType;
		m_pKey->m_bModified = false;
	}

	void mouseReleaseEvent(QMouseEvent* pEvent) override
	{
		m_pCurveEditor->PostContentUpdate();
	}
};

CCurveEditor::CCurveEditor(QWidget* parent)
	: QWidget(parent)
	, m_pContent(nullptr)
	, m_pMouseHandler(nullptr)
	, m_frameRate(SAnimTime::eFrameRate_30fps)
	, m_bHandlesVisible(true)
	, m_bRulerVisible(true)
	, m_bTimeSliderVisible(true)
	, m_bGridVisible(false)
	, m_timeLimiting(ELimit::None)
	, m_valueLimiting(ELimit::None)
	, m_bAllowDiscontinuous(true)
	, m_bKeysSelected(false)
	, m_bMoveAxisLocked(false)
	, m_time(SAnimTime(0))
	, m_zoom(0.5f, 0.5f)
	, m_translation(0.5f, 0.5f)
	, m_timeRange(SAnimTime::Min(), SAnimTime::Max())
	, m_valueRange(-1.0f, 1.0f)
	, m_rulerHeight(Private_CurveEditor::kDefaultRulerHeight)
	, m_rulerTicksYOffset(0)
	, m_fitMargin(Private_CurveEditor::kDefaultFitMargin)
	, m_curveFocusIndex(0)
{
	setMouseTracking(true);
}

CCurveEditor::~CCurveEditor()
{
}

void CCurveEditor::SetContent(SCurveEditorContent* pContent)
{
	if (m_pContent != pContent)
	{
		if (m_pContent)
		{
			QObject::disconnect(m_pContent, 0, this, 0);
		}
		m_pContent = pContent;
		QObject::connect(m_pContent, &SCurveEditorContent::destroyed, this, &CCurveEditor::OnContentDestroyed);

		update();
	}
}

void CCurveEditor::SetTime(const SAnimTime time)
{
	m_timeRange.ClipValue(m_time = time);
	update();
}

void CCurveEditor::SetTimeRange(const SAnimTime start, const SAnimTime end, ELimit limit)
{
	if (start <= end)
	{
		m_timeRange = TRange<SAnimTime>(start, end);
		m_timeLimiting = limit;
		update();
	}
}

void CCurveEditor::SetValueRange(const float min, const float max, ELimit limit)
{
	if (min <= max)
	{
		m_valueRange = Range(min, max);
		m_valueLimiting = limit;
		update();
	}
}

void CCurveEditor::ZoomToTimeRange(const float start, const float end)
{
	const float delta = (end - start);

	if (delta > 1e-10f)
	{
		m_zoom.x = 1.0f / (end - start);
		m_translation.x = start / (start - end);
	}
	else
	{
		// Just center around value with zoom = 1.0f
		m_zoom.x = 1.0f;
		m_translation.x = 0.5f - start;
	}

	if (int width = GetCurveArea().width())
	{
		// Adjust zoom and translation depending on m_fitMargin;
		const float pivot = (0.5f - m_translation.x) / m_zoom.x;
		m_zoom.x /= 1.0f + 2.0f * m_fitMargin / width;
		m_translation.x += ((0.5f - m_translation.x) / m_zoom.x - pivot) * m_zoom.x;
	}
}

void CCurveEditor::ZoomToValueRange(const float min, const float max)
{
	const float delta = (max - min);

	if (delta > 1e-10f)
	{
		m_zoom.y = 1.0f / (max - min);
		m_translation.y = max / (max - min);
	}
	else
	{
		// Just center around value with zoom = 1.0f
		m_zoom.y = 1.0f;
		m_translation.y = 0.5f + min;
	}

	if (int height = GetCurveArea().height())
	{
		// Adjust zoom and translation depending on m_fitMargin
		const float pivot = (0.5f - m_translation.y) / m_zoom.y;
		m_zoom.y /= 1.0f + 2.0f * m_fitMargin / height;
		m_translation.y += ((0.5f - m_translation.y) / m_zoom.y - pivot) * m_zoom.y;
	}
}

Range CCurveEditor::GetVisibleTimeRange() const
{
	return Range(-m_translation.x / m_zoom.x, (1.0f - m_translation.x) / m_zoom.x);
}

void CCurveEditor::paintEvent(QPaintEvent* pEvent)
{
	using namespace Private_CurveEditor;
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.translate(0.5f, 0.5f);

	auto transformFunc = [&](Vec2 point)
	{
		return TransformPointToScreen(m_zoom, m_translation, GetCurveArea(), point);
	};

	const QColor rangeHighlightColor = GetStyleHelper()->curveRangeHighlight();
	const QRectF rangesRect(Vec2ToPoint(transformFunc(Vec2(m_timeRange.start.ToFloat(), m_valueRange.start))), Vec2ToPoint(transformFunc(Vec2(m_timeRange.end.ToFloat(), m_valueRange.end))));
	painter.setPen(QPen(Qt::NoPen));
	painter.setBrush(rangeHighlightColor);
	painter.drawRect(rangesRect);

	if (m_bGridVisible)
	{
		DrawGrid(painter);
	}

	if (m_pContent)
	{
		const QPen extrapolatedCurvePen = QPen(GetStyleHelper()->curveExtrapolated());
		for (auto& curve : GetPrioritizedCurves(true))
		{
			painter.setBrush(QBrush(Qt::NoBrush));
			ColorB color = curve.m_color;
			if (!IsPriorityCurve(curve))
			{
				ColorB bgcolor(rangeHighlightColor.red(), rangeHighlightColor.green(), rangeHighlightColor.blue(), 0);
				for (int i = 0; i < 4; ++i)
					color[i] = (color[i] + bgcolor[i]) / 2;
			}
			QColor qcolor(color.r, color.g, color.b, color.a);
			QPen curvePen(qcolor, 2);
			QPen narrowCurvePen(qcolor);

			const QPainterPath extrapolatedPath = CreateExtrapolatedPathFromCurve(curve, transformFunc, width());
			painter.setPen(narrowCurvePen);
			painter.drawPath(extrapolatedPath);

			const QPainterPath discontinuityPath = CreateDiscontinuityPathFromCurve(curve, transformFunc);
			painter.setPen(narrowCurvePen);
			painter.drawPath(discontinuityPath);

			if (curve.m_keys.size() > 0)
			{
				const QPainterPath path = CreatePathFromCurve(curve, transformFunc);
				painter.setPen(curvePen);
				painter.drawPath(path);

				DrawKeys(painter, curve, transformFunc, m_bHandlesVisible);
			}
		}
	}

	if (m_pMouseHandler)
	{
		m_pMouseHandler->paintOver(painter);
	}

	if (m_bRulerVisible)
	{
		DrawingPrimitives::SRulerOptions rulerOptions;
		rulerOptions.m_rect = QRect(0, -1, size().width(), m_rulerHeight + 2);
		rulerOptions.m_visibleRange = Range(-m_translation.x / m_zoom.x, (1.0f - m_translation.x) / m_zoom.x);
		rulerOptions.m_rulerRange = rulerOptions.m_visibleRange;
		rulerOptions.m_markHeight = kRulerMarkHeight;
		rulerOptions.m_shadowSize = kRulerShadowHeight;
		rulerOptions.m_ticksYOffset = m_rulerTicksYOffset;
		rulerOptions.m_drawBackgroundCallback = [this, &painter, &rulerOptions]()
		{
			SignalDrawRulerBackground(painter, rulerOptions.m_rect, rulerOptions.m_visibleRange);
		};

		int rulerPrecision;
		DrawingPrimitives::DrawRuler(painter, rulerOptions, &rulerPrecision);

		if (m_bTimeSliderVisible && m_pContent && isEnabled())
		{
			DrawingPrimitives::STimeSliderOptions timeSliderOptions;
			timeSliderOptions.m_rect = rect();
			timeSliderOptions.m_precision = rulerPrecision;
			timeSliderOptions.m_position = transformFunc(Vec2(m_time.ToFloat(), 0.0f)).x;
			timeSliderOptions.m_time = m_time.ToFloat();
			timeSliderOptions.m_bHasFocus = hasFocus();
			DrawingPrimitives::DrawTimeSlider(painter, timeSliderOptions);
		}
	}
}

void CCurveEditor::mousePressEvent(QMouseEvent* pEvent)
{
	setFocus();

	if (pEvent->button() == Qt::LeftButton)
	{
		LeftButtonMousePressEvent(pEvent);
	}
	else if (pEvent->button() == Qt::RightButton)
	{
		RightButtonMousePressEvent(pEvent);
	}
	else if (pEvent->button() == Qt::MiddleButton)
	{
		MiddleButtonMousePressEvent(pEvent);
	}
}

void CCurveEditor::mouseDoubleClickEvent(QMouseEvent* pEvent)
{
	if (pEvent->button() == Qt::LeftButton)
	{
		auto curveHitPair = HitDetectCurve(pEvent->pos());
		if (curveHitPair.first)
		{
			AddPointToCurve(curveHitPair.second, curveHitPair.first);
			setCursor(QCursor(Qt::SizeAllCursor));
		}
	}
}

void CCurveEditor::DrawGrid(QPainter& painter)
{
	using namespace DrawingPrimitives;

	QColor gridColor = GetStyleHelper()->curveGrid();
	gridColor.setAlpha(128);
	const QColor textColor = GetStyleHelper()->curveText();

	const Range horizontalVisibleRange = Range(-m_translation.x / m_zoom.x, (1.0f - m_translation.x) / m_zoom.x);
	const Range verticalVisibleRange = Range((m_translation.y - 1.0f) / m_zoom.y, m_translation.y / m_zoom.y);

	QRect curveRect = GetCurveArea();
	const int height = curveRect.height();
	const int width = curveRect.width();

	int verticalRulerPrecision;

	std::vector<STick> horizontalTicks;
	CalculateTicks(max(0, width), horizontalVisibleRange, horizontalVisibleRange, nullptr, nullptr, horizontalTicks);
	std::vector<STick> verticalTicks;
	CalculateTicks(max(0, height), verticalVisibleRange, verticalVisibleRange, &verticalRulerPrecision, nullptr, verticalTicks);

	char format[16];
	cry_sprintf(format, "%%.%df", verticalRulerPrecision);

	const QPen gridPen(gridColor, 1.0);
	painter.setPen(gridPen);

	for (const STick& tick : horizontalTicks)
	{
		if (!tick.m_bTenth)
		{
			const int x = tick.m_position;
			painter.drawLine(x, curveRect.top(), x, height);
		}
	}

	for (const STick& tick : verticalTicks)
	{
		if (!tick.m_bTenth)
		{
			const int y = height - tick.m_position + curveRect.top();
			painter.drawLine(0, y, width, y);
		}
	}

	const QPen textPen(textColor);
	painter.setPen(textPen);

	QString str;
	for (const STick& tick : verticalTicks)
	{
		if (!tick.m_bTenth)
		{
			const int y = height - tick.m_position + curveRect.top();
			str.sprintf(format, tick.m_value);
			painter.drawText(5, y - 4, str);
		}
	}
}

void CCurveEditor::LeftButtonMousePressEvent(QMouseEvent* pEvent)
{
	using namespace Private_CurveEditor;
	if (m_bRulerVisible && pEvent->y() < m_rulerHeight)
	{
		m_pMouseHandler.reset(new SScrubHandler(this));
	}
	else
	{
		const bool bToggleSelection = (pEvent->modifiers() & Qt::CTRL) != 0;
		const bool bSelect = (pEvent->modifiers() & Qt::SHIFT) != 0;
		const bool bDeselect = (pEvent->modifiers() & Qt::ALT) != 0;

		auto curveKeyPair = HitDetectKey(pEvent->pos());
		auto handleKeyTuple = HitDetectHandle(pEvent->pos());

		if (curveKeyPair.first)
		{
			bool useExistingSelection = curveKeyPair.second->m_bSelected;

			if (bToggleSelection)
			{
				curveKeyPair.second->m_bSelected = !curveKeyPair.second->m_bSelected;
			}
			else if (bSelect)
			{
				curveKeyPair.second->m_bSelected = true;
			}
			else if (bDeselect)
			{
				curveKeyPair.second->m_bSelected = false;
			}
			else
			{
				if (!useExistingSelection)
				{
					ForEachKey(*m_pContent, [](SCurveEditorCurve& curve, SCurveEditorKey& key)
					{
						key.m_bSelected = false;
					});
				}

				curveKeyPair.second->m_bSelected = true;
			}

			if (!useExistingSelection || bToggleSelection || bSelect)
			{
				m_bKeysSelected = true;
				SetPriorityCurve(*curveKeyPair.first);
				SignalSelectionChanged();
			}

			m_pMouseHandler.reset(new SMoveHandler(this, false));
		}
		else if (std::get<0>(handleKeyTuple))
		{
			m_pMouseHandler.reset(new SHandleMoveHandler(this, *std::get<0>(handleKeyTuple), std::get<1>(handleKeyTuple), std::get<2>(handleKeyTuple), std::get<3>(handleKeyTuple)));
		}
		else
		{
			m_pMouseHandler.reset(new SSelectionHandler(this, false));
			CyclePriorityCurve();
		}
	}

	m_pMouseHandler->mousePressEvent(pEvent);
	update();
}

void CCurveEditor::RightButtonMousePressEvent(QMouseEvent* pEvent)
{
	m_pMouseHandler.reset(new SPanHandler(this));
	m_pMouseHandler->mousePressEvent(pEvent);
	update();
}

void CCurveEditor::MiddleButtonMousePressEvent(QMouseEvent* pEvent)
{
	const bool bShiftPressed = (pEvent->modifiers() & Qt::SHIFT) != 0;

	if (bShiftPressed)
	{
		m_pMouseHandler.reset(new SZoomHandler(this));
	}
	else
	{
		m_pMouseHandler.reset(new SPanHandler(this));
	}

	m_pMouseHandler->mousePressEvent(pEvent);
	update();
}

void CCurveEditor::customEvent(QEvent* pEvent)
{
	if (pEvent->type() == SandboxEvent::Command)
	{
		CommandEvent* commandEvent = static_cast<CommandEvent*>(pEvent);
		const string& command = commandEvent->GetCommand();

		if (command == "general.delete")
		{
			OnDeleteSelectedKeys();
			pEvent->accept();
			update();
		}
		else
		{
			pEvent->ignore();
		}
	}
	else
	{
		QWidget::customEvent(pEvent);
	}
}

void CCurveEditor::mouseMoveEvent(QMouseEvent* pEvent)
{
	if (m_pMouseHandler)
	{
		m_pMouseHandler->mouseMoveEvent(pEvent);
	}
	else
	{
		if (HitDetectKey(pEvent->pos()).first || std::get<0>(HitDetectHandle(pEvent->pos())))
		{
			setCursor(QCursor(Qt::SizeAllCursor));
		}
		else
		{
			setCursor(QCursor());
		}
	}

	update();
}

void CCurveEditor::mouseReleaseEvent(QMouseEvent* pEvent)
{
	if (m_pMouseHandler)
	{
		m_pMouseHandler->mouseReleaseEvent(pEvent);
		m_pMouseHandler.reset();
		update();
	}
}

void CCurveEditor::focusOutEvent(QFocusEvent* pEvent)
{
	if (m_pMouseHandler)
	{
		m_pMouseHandler->focusOutEvent(pEvent);
		m_pMouseHandler.reset();
		update();
	}
}

void CCurveEditor::wheelEvent(QWheelEvent* pEvent)
{
	using namespace Private_CurveEditor;
	Vec2 windowSize((float)size().width(), (float)size().height());
	windowSize.y = (windowSize.y > 0.0f) ? windowSize.y : 1.0f;

	const QRect curveArea = GetCurveArea();
	const float mouseXNormalized = (float)(pEvent->x() - curveArea.left()) / (float)curveArea.width();
	const float mouseYNormalized = (float)(pEvent->y() - curveArea.top()) / (float)curveArea.height();

	const float pivotX = (mouseXNormalized - m_translation.x) / m_zoom.x;
	const float pivotY = (mouseYNormalized - m_translation.y) / m_zoom.y;

	m_zoom *= pow(1.2f, (float)pEvent->delta() * 0.01f);
	m_zoom.x = clamp_tpl(m_zoom.x, kMinZoom, kMaxZoom);
	m_zoom.y = clamp_tpl(m_zoom.y, kMinZoom, kMaxZoom);

	// Adjust translation so pivot point stays at same x and y position on screen
	m_translation.x += ((mouseXNormalized - m_translation.x) / m_zoom.x - pivotX) * m_zoom.x;
	m_translation.y += ((mouseYNormalized - m_translation.y) / m_zoom.y - pivotY) * m_zoom.y;

	SignalZoom();
	update();
}

void CCurveEditor::keyPressEvent(QKeyEvent* pEvent)
{
	pEvent->ignore();
	if (!m_pContent)
	{
		return;
	}

	QKeySequence key(pEvent->key());

	if (key == QKeySequence(Qt::Key_Delete))
	{
		OnDeleteSelectedKeys();
		pEvent->accept();
	}
	else if (key == QKeySequence(Qt::Key_Shift))
	{
		m_bMoveAxisLocked = true;
	}

	update();
}

void CCurveEditor::keyReleaseEvent(QKeyEvent* pEvent)
{
	QKeySequence key(pEvent->key());

	if (key == QKeySequence(Qt::Key_Shift))
	{
		m_bMoveAxisLocked = false;
	}
}

void CCurveEditor::SetHandlesVisible(bool bVisible)
{
	m_bHandlesVisible = bVisible;
	update();
}

void CCurveEditor::SetRulerVisible(bool bVisible)
{
	m_bRulerVisible = bVisible;
	update();
}

void CCurveEditor::SetRulerHeight(int height)
{
	m_rulerHeight = height;
	update();
}

void CCurveEditor::SetRulerTicksYOffset(int offset)
{
	m_rulerTicksYOffset = offset;
	update();
}

void CCurveEditor::SetTimeSliderVisible(bool bVisible)
{
	m_bTimeSliderVisible = bVisible;
	update();
}

void CCurveEditor::SetGridVisible(bool bVisible)
{
	m_bGridVisible = bVisible;
	update();
}

void CCurveEditor::SetFitMargin(float margin)
{
	m_fitMargin = margin;
}

void CCurveEditor::SelectInRect(const QRect& rect, bool bToggleSelected, bool bDeselect)
{
	using namespace Private_CurveEditor;
	if (!m_pContent)
	{
		return;
	}

	bool hasSelectedKeys = false;
	ForEachKey(*m_pContent, [&](SCurveEditorCurve& curve, SCurveEditorKey& key)
	{
		const Vec2 screenPoint = TransformPointToScreen(m_zoom, m_translation, GetCurveArea(), Vec2(key.m_time.ToFloat(), key.m_controlPoint.m_value));
		bool bKeyFoundInRect = rect.contains((int)screenPoint.x, (int)screenPoint.y);

		if (!bToggleSelected && !bDeselect)
		{
		  key.m_bSelected = bKeyFoundInRect;
		}
		else
		{
		  if (bKeyFoundInRect)
		  {
		    if (bToggleSelected)
		    {
		      key.m_bSelected = !key.m_bSelected;
		    }
		    else if (bDeselect)
		    {
		      key.m_bSelected = false;
		    }
		  }
		}

		if (key.m_bSelected)
		{
		  hasSelectedKeys = true;
		}
	});

	m_bKeysSelected = hasSelectedKeys;
	update();
	PostContentUpdate();
}

void CCurveEditor::CyclePriorityCurve()
{
	if (m_pContent && m_pContent->m_curves.size() > 1)
		m_curveFocusIndex = (m_curveFocusIndex + 1) % m_pContent->m_curves.size();
}

void CCurveEditor::SetPriorityCurve(SCurveEditorCurve const& curve)
{
	for (int i = 0; i < m_pContent->m_curves.size(); ++i)
		if (&curve == &m_pContent->m_curves[i])
		{
			m_curveFocusIndex = i;
			break;
		}
}

bool CCurveEditor::IsPriorityCurve(SCurveEditorCurve const& curve) const
{
	return m_curveFocusIndex < m_pContent->m_curves.size() && &m_pContent->m_curves[m_curveFocusIndex] == &curve;
}

CCurveEditor::TCurveReferences CCurveEditor::GetPrioritizedCurves(bool backwards)
{
	size_t count = m_pContent->m_curves.size();
	TCurveReferences priCurves;
	for (int i = 0; i < count; ++i)
	{
		int s = (m_curveFocusIndex + (backwards ? count - 1 - i : i)) % count;
		priCurves.push_back(&m_pContent->m_curves[s]);
	}
	return priCurves;
}

std::pair<SCurveEditorCurve*, Vec2> CCurveEditor::HitDetectCurve(const QPoint point)
{
	using namespace Private_CurveEditor;
	if (!m_pContent)
	{
		return std::make_pair(nullptr, Vec2(ZERO));
	}

	SCurveEditorCurve* pNearestCurve = nullptr;
	Vec2 closestPoint = Vec2(ZERO);

	float nearestDistance = std::numeric_limits<float>::max();
	for (auto& curve : GetPrioritizedCurves())
	{
		const Vec2 closestPointOnCurve = ClosestPointOnCurve(PointToVec2(point), curve);

		const float distance = (PointToVec2(point) - closestPointOnCurve).GetLength();
		if (distance < nearestDistance)
		{
			nearestDistance = distance;
			pNearestCurve = &curve;
			closestPoint = closestPointOnCurve;
		}
	}

	if (nearestDistance <= kHitDistance)
	{
		return std::make_pair(pNearestCurve, TransformPointFromScreen(m_zoom, m_translation, GetCurveArea(), closestPoint));
	}

	return std::make_pair(nullptr, Vec2(ZERO));
}

std::pair<SCurveEditorCurve*, SCurveEditorKey*> CCurveEditor::HitDetectKey(const QPoint point)
{
	using namespace Private_CurveEditor;
	if (!m_pContent)
	{
		return std::make_pair(nullptr, nullptr);
	}

	for (auto& curve : GetPrioritizedCurves())
	{
		for (auto& key : curve.m_keys)
		{
			const Vec2 keyPoint = Vec2(key.m_time.ToFloat(), key.m_controlPoint.m_value);
			const Vec2 transformedPoint = TransformPointToScreen(m_zoom, m_translation, GetCurveArea(), keyPoint);
			if ((transformedPoint - PointToVec2(point)).GetLength() <= kHitDistance)
			{
				return std::make_pair(&curve, &key);
			}
		}
	}

	return std::make_pair(nullptr, nullptr);
}

std::tuple<SCurveEditorCurve*, SCurveEditorKey, SCurveEditorKey*, CCurveEditor::ETangent> CCurveEditor::HitDetectHandle(const QPoint point)
{
	using namespace Private_CurveEditor;
	if (!m_pContent || !m_bHandlesVisible)
	{
		return std::make_tuple(nullptr, SCurveEditorKey(), nullptr, ETangent::In);
	}

	for (auto& curve : GetPrioritizedCurves())
	{
		for (auto iter = curve.m_keys.begin(); iter != curve.m_keys.end(); ++iter)
		{
			SCurveEditorKey key = *iter;

			const Vec2 keyPoint = Vec2(key.m_time.ToFloat(), key.m_controlPoint.m_value);
			const bool bIsFirstKey = (iter == curve.m_keys.begin());
			const bool bIsLastKey = (iter == (curve.m_keys.end() - 1));
			if (!bIsFirstKey)
				key = curve.ApplyInTangent(key, false, &*iter);
			if (!bIsLastKey)
				key = curve.ApplyOutTangent(key, false, &*iter);

			if (!bIsFirstKey && (key.m_controlPoint.m_inTangentType != SBezierControlPoint::ETangentType::Step))
			{
				const Vec2 tangentHandlePoint = keyPoint + key.m_controlPoint.m_inTangent;
				const Vec2 transformedTangentHandlePoint = TransformPointToScreen(m_zoom, m_translation, GetCurveArea(), tangentHandlePoint);
				if ((transformedTangentHandlePoint - PointToVec2(point)).GetLength() <= kHitDistance)
				{
					SetPriorityCurve(curve);
					return std::make_tuple(&curve, key, &(*iter), ETangent::In);
				}
			}

			if (!bIsLastKey && (key.m_controlPoint.m_outTangentType != SBezierControlPoint::ETangentType::Step))
			{
				const Vec2 tangentHandlePoint = keyPoint + key.m_controlPoint.m_outTangent;
				const Vec2 transformedTangentHandlePoint = TransformPointToScreen(m_zoom, m_translation, GetCurveArea(), tangentHandlePoint);
				if ((transformedTangentHandlePoint - PointToVec2(point)).GetLength() <= kHitDistance)
				{
					SetPriorityCurve(curve);
					return std::make_tuple(&curve, key, &(*iter), ETangent::Out);
				}
			}
		}
	}

	return std::make_tuple(nullptr, SCurveEditorKey(), nullptr, ETangent::In);
}

// Input and output are in screen space
Vec2 CCurveEditor::ClosestPointOnCurve(const Vec2 point, const SCurveEditorCurve& curve)
{
	using namespace Private_CurveEditor;
	auto transformFunc = [&](Vec2 point)
	{
		return TransformPointToScreen(m_zoom, m_translation, GetCurveArea(), point);
	};

	if (curve.m_keys.size() == 0)
	{
		const Vec2 pointOnCurve = transformFunc(Vec2(0.0f, curve.m_defaultValue));
		return Vec2(point.x, pointOnCurve.y);
	}

	Vec2 closestPoint;
	float minDistance = std::numeric_limits<float>::max();

	const Vec2 startKeyTransformed = transformFunc(Vec2(curve.m_keys.front().m_time.ToFloat(), curve.m_keys.front().m_controlPoint.m_value));
	if (point.x < startKeyTransformed.x)
	{
		const float distanceToCurve = std::abs(point.y - startKeyTransformed.y);
		if (distanceToCurve < minDistance)
		{
			closestPoint = Vec2(point.x, startKeyTransformed.y);
			minDistance = distanceToCurve;
		}
	}

	const Vec2 endKeyTransformed = transformFunc(Vec2(curve.m_keys.back().m_time.ToFloat(), curve.m_keys.back().m_controlPoint.m_value));
	if (point.x > endKeyTransformed.x)
	{
		const float distanceToCurve = std::abs(point.y - endKeyTransformed.y);
		if (distanceToCurve < minDistance)
		{
			closestPoint = Vec2(point.x, endKeyTransformed.y);
			minDistance = distanceToCurve;
		}
	}

	const auto endIter = curve.m_keys.end() - 1;
	for (auto iter = curve.m_keys.begin(); iter != endIter; ++iter)
	{
		const SCurveEditorKey segmentStartKey = curve.ApplyOutTangent(*iter, true);
		const SCurveEditorKey segmentEndKey = curve.ApplyInTangent(*(iter + 1), true);

		if ((segmentStartKey.m_controlPoint.m_outTangentType == SBezierControlPoint::ETangentType::Linear
		    && segmentEndKey.m_controlPoint.m_inTangentType == SBezierControlPoint::ETangentType::Linear)
		    || (segmentStartKey.m_controlPoint.m_outTangentType == SBezierControlPoint::ETangentType::Smooth
		        && segmentEndKey.m_controlPoint.m_inTangentType == SBezierControlPoint::ETangentType::Smooth
		        && (segmentStartKey.m_controlPoint.m_outTangent + segmentEndKey.m_controlPoint.m_inTangent).GetLengthSquared() < 0.0001f))
		{
			auto newClosestPointData = ClosestPointOnSimpleSegment(point, segmentStartKey, segmentEndKey, transformFunc);
			SaveMinIfLess(newClosestPointData.first, newClosestPointData.second, minDistance, closestPoint);
		}
		else
		{
			const Vec2 p0 = transformFunc(Vec2(segmentStartKey.m_time.ToFloat(), segmentStartKey.m_controlPoint.m_value));
			const Vec2 p3 = transformFunc(Vec2(segmentEndKey.m_time.ToFloat(), segmentEndKey.m_controlPoint.m_value));
			const Vec2 p1 = transformFunc(Vec2(segmentStartKey.m_time.ToFloat() + segmentStartKey.m_controlPoint.m_outTangent.x,
			                                   segmentStartKey.m_controlPoint.m_value + segmentStartKey.m_controlPoint.m_outTangent.y));
			const Vec2 p2 = transformFunc(Vec2(segmentEndKey.m_time.ToFloat() + segmentEndKey.m_controlPoint.m_inTangent.x,
			                                   segmentEndKey.m_controlPoint.m_value + segmentEndKey.m_controlPoint.m_inTangent.y));
			
			if (ArePointsAligned(p0, p1, p2, p3))
			{
				auto newClosestPointData = ClosestPointOnSimpleSegment(point, segmentStartKey, segmentEndKey, transformFunc);
				SaveMinIfLess(newClosestPointData.first, newClosestPointData.second, minDistance, closestPoint);
			}
			else
			{
				const Vec2 closestOnSegment = (!curve.m_bBezier2D) ? ClosestPointOnBezierSegment(point, p0.x, p3.x, p0.y, p1.y, p2.y, p3.y) : ClosestPointOn2DBezierSegment(point, p0, p1, p2, p3);
				const float distanceToSegment = (closestOnSegment - point).GetLength();
				SaveMinIfLess(distanceToSegment, closestOnSegment, minDistance, closestPoint);
			}
		}
	}

	return closestPoint;
}

void CCurveEditor::PreContentUpdate()
{
	CRY_ASSERT(m_pContent);

	SignalContentAboutToBeChanged();
	m_pContent->SignalAboutToBeChanged(*this);
}

void CCurveEditor::ContentUpdate()
{
	CRY_ASSERT(m_pContent);

	SignalContentChanging();
	m_pContent->SignalChanging(*this);
}

void CCurveEditor::PostContentUpdate()
{
	using namespace Private_CurveEditor;
	CRY_ASSERT(m_pContent);

	DeleteMarkedKeys();
	ForEachKey(*m_pContent, [](SCurveEditorCurve& curve, SCurveEditorKey& key)
	{
		key.m_bModified = false;
	});

	update();

	SignalContentChanged();
	m_pContent->SignalChanged(*this);
}

void CCurveEditor::DeleteMarkedKeys()
{
	if (m_pContent)
	{
		for (auto& curve : m_pContent->m_curves)
		{
			for (auto keyIter = curve.m_keys.begin(); keyIter != curve.m_keys.end(); )
			{
				if (keyIter->m_bDeleted)
				{
					keyIter = curve.m_keys.erase(keyIter);
				}
				else
				{
					++keyIter;
				}
			}
		}
	}
}

void CCurveEditor::AddPointToCurve(const Vec2 point, SCurveEditorCurve* pCurve)
{
	using namespace Private_CurveEditor;
	PreContentUpdate();
	SCurveEditorKey key;
	key.m_time = SAnimTime(point.x);
	key.m_controlPoint.m_value = point.y;
	key.m_bAdded = true;
	pCurve->m_keys.push_back(key);

	SortKeys(*pCurve);
	PostContentUpdate();

	ForEachKey(*m_pContent, [](SCurveEditorCurve& curve, SCurveEditorKey& key)
	{
		if (key.m_bAdded)
		{
		  key.m_controlPoint.m_inTangent.x = -1.0f;
		  key.m_controlPoint.m_outTangent.x = 1.0f;
		  key = curve.ApplyTangents(key, true);
		}
	});
	ForEachKey(*m_pContent, [](SCurveEditorCurve& curve, SCurveEditorKey& key)
	{
		key.m_bAdded = false;
	});
}

void CCurveEditor::SortKeys(SCurveEditorCurve& curve)
{
	std::stable_sort(curve.m_keys.begin(), curve.m_keys.end(), [](const SCurveEditorKey& a, const SCurveEditorKey& b)
	{
		return a.m_time < b.m_time;
	});
}

void CCurveEditor::OnDeleteSelectedKeys()
{
	using namespace Private_CurveEditor;
	if (m_bKeysSelected)
	{
		PreContentUpdate();

		ForEachKey(*m_pContent, [](SCurveEditorCurve& curve, SCurveEditorKey& key)
		{
			key.m_bDeleted = key.m_bDeleted || key.m_bSelected;
		});

		PostContentUpdate();
	}
}

void CCurveEditor::OnSetSelectedKeysTangentAuto()
{
	// SetSelectedKeysTangentType(ETangent::In, SBezierControlPoint::ETangentType::Auto);
	// SetSelectedKeysTangentType(ETangent::Out, SBezierControlPoint::ETangentType::Auto);
	SetSelectedKeysTangentType(ETangent::In, SBezierControlPoint::ETangentType::Smooth);
	SetSelectedKeysTangentType(ETangent::Out, SBezierControlPoint::ETangentType::Smooth);
}

void CCurveEditor::OnSetSelectedKeysInTangentZero()
{
	SetSelectedKeysTangentType(ETangent::In, SBezierControlPoint::ETangentType::Zero);
}

void CCurveEditor::OnSetSelectedKeysInTangentStep()
{
	SetSelectedKeysTangentType(ETangent::In, SBezierControlPoint::ETangentType::Step);
}

void CCurveEditor::OnSetSelectedKeysInTangentLinear()
{
	SetSelectedKeysTangentType(ETangent::In, SBezierControlPoint::ETangentType::Linear);
}

void CCurveEditor::OnSetSelectedKeysOutTangentZero()
{
	SetSelectedKeysTangentType(ETangent::Out, SBezierControlPoint::ETangentType::Zero);
}

void CCurveEditor::OnSetSelectedKeysOutTangentStep()
{
	SetSelectedKeysTangentType(ETangent::Out, SBezierControlPoint::ETangentType::Step);
}

void CCurveEditor::OnSetSelectedKeysOutTangentLinear()
{
	SetSelectedKeysTangentType(ETangent::Out, SBezierControlPoint::ETangentType::Linear);
}

void CCurveEditor::OnFitCurvesHorizontally()
{
	if (m_pContent)
	{
		TRange<SAnimTime> range(SAnimTime::Max(), SAnimTime::Min());

		for (const auto& curve : m_pContent->m_curves)
		{
			if (curve.m_keys.size() > 0)
			{
				range |= TRange<SAnimTime>(curve.m_keys.front().m_time, curve.m_keys.back().m_time);
			}
		}

		if (range.IsEmpty())
			range = m_timeRange;

		ZoomToTimeRange(range.start.ToFloat(), range.end.ToFloat());
	}

	update();
}

void CCurveEditor::OnFitCurvesVertically()
{
	using namespace Private_CurveEditor;
	if (m_pContent)
	{
		Range range(Range::EMPTY);

		for (const auto& curve : m_pContent->m_curves)
		{
			if (curve.m_keys.size() > 1)
			{
				const auto endIter = curve.m_keys.end() - 1;

				for (auto iter = curve.m_keys.begin(); iter != endIter; ++iter)
				{
					const SCurveEditorKey segmentStartKey = curve.ApplyOutTangent(*iter, false);
					const SCurveEditorKey segmentEndKey = curve.ApplyInTangent(*(iter + 1), false);

					range |= GetBezierSegmentValueRange(segmentStartKey, segmentEndKey);
					range |= segmentStartKey.m_controlPoint.m_value + segmentStartKey.m_controlPoint.m_outTangent.y;
					range |= segmentEndKey.m_controlPoint.m_value + segmentEndKey.m_controlPoint.m_inTangent.y;
				}
			}
			else if (curve.m_keys.size() == 1)
			{
				range |= curve.m_keys[0].m_controlPoint.m_value;
			}
		}

		if (fabs(range.start - range.end) < FLOAT_EPSILON)
		{
			range = range + m_valueRange;
		}
		else if (range.IsEmpty())
		{
			range = m_valueRange;
		}

		ZoomToValueRange(range.start, range.end);

		// Adjust zoom and translation depending on kFitMargin
		const float pivot = (0.5f - m_translation.y) / m_zoom.y;
		const int height = GetCurveArea().height();
		if (height > 0)
		{
			m_zoom.y /= 1.0f + 2.0f * (m_fitMargin / height);
		}
		m_translation.y += ((0.5f - m_translation.y) / m_zoom.y - pivot) * m_zoom.y;
	}

	update();
}

void CCurveEditor::OnBreakTangents()
{
	using namespace Private_CurveEditor;
	if (m_pContent && m_bKeysSelected)
	{
		PreContentUpdate();
		ForEachKey(*m_pContent, [&](SCurveEditorCurve& curve, SCurveEditorKey& key)
		{
			if (key.m_bSelected)
			{
			  key.m_controlPoint.m_bBreakTangents = true;
			}
		});
		update();
		PostContentUpdate();
	}
}

void CCurveEditor::OnUnifyTangents()
{
	using namespace Private_CurveEditor;
	if (m_pContent && m_bKeysSelected)
	{
		PreContentUpdate();
		ForEachKey(*m_pContent, [&](SCurveEditorCurve& curve, SCurveEditorKey& key)
		{
			if (key.m_bSelected && key.m_controlPoint.m_bBreakTangents)
			{
			  key.m_controlPoint.m_bBreakTangents = false;
			  // key.m_controlPoint.m_inTangentType = key.m_controlPoint.m_outTangentType = SBezierControlPoint::ETangentType::Auto;
			  key.m_controlPoint.m_inTangentType = key.m_controlPoint.m_outTangentType = SBezierControlPoint::ETangentType::Smooth;
			}
		});
		update();
		PostContentUpdate();
	}
}

void CCurveEditor::OnContentDestroyed()
{
	QObject::disconnect(m_pContent, 0, this, 0);
}

void CCurveEditor::SetSelectedKeysTangentType(const ETangent tangent, const SBezierControlPoint::ETangentType type)
{
	using namespace Private_CurveEditor;
	if (m_pContent && m_bKeysSelected)
	{
		PreContentUpdate();
		ForEachKey(*m_pContent, [&](SCurveEditorCurve& curve, SCurveEditorKey& key)
		{
			if (key.m_bSelected)
			{
			  if (tangent == ETangent::In)
			  {
			    key.m_controlPoint.m_inTangentType = type;
			  }
			  else
			  {
			    key.m_controlPoint.m_outTangentType = type;
			  }
			  // if (type != SBezierControlPoint::ETangentType::Custom && type != SBezierControlPoint::ETangentType::Auto)
			  if (type != SBezierControlPoint::ETangentType::Custom && type != SBezierControlPoint::ETangentType::Smooth)
					key.m_controlPoint.m_bBreakTangents = true;
			}
		});

		update();
		PostContentUpdate();
	}
}

QRect CCurveEditor::GetCurveArea()
{
	const uint rulerAreaHeight = m_bRulerVisible ? m_rulerHeight : 0;
	return QRect(0, rulerAreaHeight, width(), height() - rulerAreaHeight);
}

void CCurveEditor::FillWithCurveToolsAndConnect(QToolBar* pToolBar)
{
	pToolBar->addAction(CryIcon("icons:CurveEditor/auto.ico"), "Set in and out tangents to auto", this, SLOT(OnSetSelectedKeysTangentAuto()));
	pToolBar->addSeparator();
	pToolBar->addAction(CryIcon("icons:CurveEditor/zero_in.ico"), "Set in tangent to zero", this, SLOT(OnSetSelectedKeysInTangentZero()));
	if (m_bAllowDiscontinuous)
		pToolBar->addAction(CryIcon("icons:CurveEditor/step_in.ico"), "Set in tangent to step", this, SLOT(OnSetSelectedKeysInTangentStep()));
	pToolBar->addAction(CryIcon("icons:CurveEditor/linear_in.ico"), "Set in tangent to linear", this, SLOT(OnSetSelectedKeysInTangentLinear()));
	pToolBar->addSeparator();
	pToolBar->addAction(CryIcon("icons:CurveEditor/zero_out.ico"), "Set out tangent to zero", this, SLOT(OnSetSelectedKeysOutTangentZero()));
	if (m_bAllowDiscontinuous)
		pToolBar->addAction(CryIcon("icons:CurveEditor/step_out.ico"), "Set out tangent to step", this, SLOT(OnSetSelectedKeysOutTangentStep()));
	pToolBar->addAction(CryIcon("icons:CurveEditor/linear_out.ico"), "Set out tangent to linear", this, SLOT(OnSetSelectedKeysOutTangentLinear()));
	pToolBar->addSeparator();
	pToolBar->addAction(CryIcon("icons:CurveEditor/fit_horizontal.ico"), "Fit curves horizontally", this, SLOT(OnFitCurvesHorizontally()));
	pToolBar->addAction(CryIcon("icons:CurveEditor/fit_vertical.ico"), "Fit curves vertically", this, SLOT(OnFitCurvesVertically()));
	pToolBar->addSeparator();
	pToolBar->addAction(CryIcon("icons:CurveEditor/break.ico"), "Break tangents", this, SLOT(OnBreakTangents()));
	pToolBar->addAction(CryIcon("icons:CurveEditor/unify.ico"), "Unify tangents", this, SLOT(OnUnifyTangents()));
}

