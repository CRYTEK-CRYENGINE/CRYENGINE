// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ConnectionWidget.h"

#include "ConnectionWidgetStyle.h"
#include "NodeGraphView.h"
#include "NodeGraphViewStyle.h"
#include "NodeWidget.h"
#include "NodePinWidgetStyle.h"

#include "AbstractPinItem.h"

#include <QPainter>
#include <QBrush>
#include <QLinearGradient>
#include <QPointF>
#include <QtMath>

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>

namespace CryGraphEditor {

CConnectionWidget::CConnectionWidget(CAbstractConnectionItem* pItem, CNodeGraphView& view)
	: CNodeGraphViewGraphicsWidget(view)
	, m_pItem(pItem)
	, m_pSourcePoint(nullptr)
	, m_pTargetPoint(nullptr)
{
	setAcceptHoverEvents(true);

	SetDefaultZValue(0.9);

	if (m_pItem)
	{
		m_pStyle = GetView().GetStyle().GetConnectionWidgetStyle(pItem->GetStyleId());
		CRY_ASSERT(m_pStyle);
		if (m_pStyle)
		{
			m_pStyle = GetView().GetStyle().GetConnectionWidgetStyle("Connection");
		}
	}
	else
	{
		m_pStyle = GetView().GetStyle().GetConnectionWidgetStyle("Connection");
	}
	CRY_ASSERT(m_pStyle);
}

CConnectionWidget::~CConnectionWidget()
{

}

void CConnectionWidget::DeleteLater()
{
	CNodeGraphViewGraphicsWidget::DeleteLater();

	SetSourceConnectionPoint(nullptr);
	SetTargetConnectionPoint(nullptr);
}

void CConnectionWidget::SetSourceConnectionPoint(const CConnectionPoint* pPoint)
{
	if (m_pSourcePoint)
	{
		QObject::disconnect(m_pSourcePoint, 0, this, 0);
	}

	m_pSourcePoint = pPoint;
	if (pPoint)
	{
		QObject::connect(m_pSourcePoint, &CConnectionPoint::SignalPositionChanged, this, &CConnectionWidget::OnPointPositionChanged);
		OnPointPositionChanged();
	}
}

void CConnectionWidget::SetTargetConnectionPoint(const CConnectionPoint* pPoint)
{
	if (m_pTargetPoint)
	{
		QObject::disconnect(m_pTargetPoint, 0, this, 0);
	}

	m_pTargetPoint = pPoint;
	if (pPoint)
	{
		QObject::connect(m_pTargetPoint, &CConnectionPoint::SignalPositionChanged, this, &CConnectionWidget::OnPointPositionChanged);
		OnPointPositionChanged();
	}
}

void CConnectionWidget::Update()
{
	prepareGeometryChange();
	updateGeometry();
	UpdatePath();
}

void CConnectionWidget::paint(QPainter* pPainter, const QStyleOptionGraphicsItem* pOption, QWidget* pWidget)
{
	if (m_path.elementCount() && m_pStyle)
	{
		const QPainterPath& path = m_path;

		const QPointF sourcePos = m_path.elementAt(0);
		const QPointF targetPos = m_path.elementAt(m_path.elementCount() - 1);

		const QColor selectionColor = m_pStyle->GetSelectionColor();

		QBrush brush;
		if (m_pItem)
		{
			if (IsSelected())
			{
				brush = selectionColor;
			}
			else
			{
				if (m_pStyle->GetUsePinColors())
				{
					QLinearGradient gradient(sourcePos, targetPos);
					QColor hColor(0, 0, 0, 0);
					if (IsHighlighted())
					{
						hColor = m_pStyle->GetSelectionColor();
					}

					const CNodePinWidgetStyle* pSourcePinStyle = GetView().GetStyle().GetPinWidgetStyle(m_pItem->GetSourcePinItem().GetStyleId());
					QColor sColor = pSourcePinStyle ? pSourcePinStyle->GetColor() : QColor(255, 100, 150);
					sColor.setRedF(1.0f - (1.0f - sColor.redF()) * (1.0f - hColor.redF()));
					sColor.setGreenF(1.0f - (1.0f - sColor.greenF()) * (1.0f - hColor.greenF()));
					sColor.setBlueF(1.0f - (1.0f - sColor.blueF()) * (1.0f - hColor.blueF()));
					sColor.setAlphaF(1.0f - (1.0f - sColor.alphaF()) * (1.0f - hColor.alphaF()));

					const CNodePinWidgetStyle* pTargetPinStyle = GetView().GetStyle().GetPinWidgetStyle(m_pItem->GetSourcePinItem().GetStyleId());
					QColor tColor = pTargetPinStyle ? pTargetPinStyle->GetColor() : QColor(255, 100, 150);
					tColor.setRedF(1.0f - (1.0f - tColor.redF()) * (1.0f - hColor.redF()));
					tColor.setGreenF(1.0f - (1.0f - tColor.greenF()) * (1.0f - hColor.greenF()));
					tColor.setBlueF(1.0f - (1.0f - tColor.blueF()) * (1.0f - hColor.blueF()));
					tColor.setAlphaF(1.0f - (1.0f - tColor.alphaF()) * (1.0f - hColor.alphaF()));

					gradient.setColorAt(0, sColor);
					gradient.setColorAt(1, tColor);
					brush = QBrush(gradient);
				}
				else
				{
					brush = m_pStyle->GetColor();
				}
			}
		}
		else
		{
			QLinearGradient gradient(sourcePos, targetPos);
			gradient.setColorAt(0, selectionColor);
			gradient.setColorAt(1, selectionColor);
			brush = QBrush(gradient);
		}

		pPainter->setRenderHints(QPainter::Antialiasing, true);
		pPainter->setPen(QPen(brush, m_pStyle->GetWidth()));
		pPainter->setBrush(Qt::NoBrush);
		pPainter->drawPath(path);

		// DEBUG
		/*pPainter->setPen(QPen(brush, 1));
		   pPainter->setBrush(Qt::NoBrush);
		   pPainter->drawPath(m_selectionShape);

		   QVector2D prevPoint = QVector2D(sourcePos);
		   for (uint32 i = 1; i < 16; ++i)
		   {
		   const qreal t = m_path.length() / 16.0 * i;
		   const QVector2D pathPoint = QVector2D(m_path.pointAtPercent(i / 16.0));

		   pPainter->setPen(QColor(0, 0, 255));
		   pPainter->drawEllipse(pathPoint.x(), pathPoint.y(), 2, 2);

		   const QVector2D seg = prevPoint - pathPoint;
		   const QVector2D dir = QVector2D(seg.y(), -seg.x()).normalized() * 9.0;

		   QVector2D pp = QVector2D(pathPoint.x() + dir.x(), pathPoint.y() + dir.y());

		   pPainter->setPen(QColor(255, 0, 0));
		   pPainter->drawLine(pathPoint.x(), pathPoint.y(), pp.x(), pp.y());

		   prevPoint = pathPoint;
		   }*/
	}
}

QRectF CConnectionWidget::boundingRect() const
{
	const QRectF geometryRect = geometry();

	const float bezierSize = m_pStyle->GetBezier();

	const QPointF topLeft = QPointF(-bezierSize, -bezierSize);
	const QPointF bottomRight = QPointF(geometryRect.width() + bezierSize, geometryRect.height() + bezierSize);

	return QRectF(topLeft, bottomRight);
}

void CConnectionWidget::updateGeometry()
{
	// Note:
	//
	// As soon as updateGeometry gets overriden by user the coordinate system changes and therefore
	// the position of the widget. Means 0,0 in local space isn't anymore the center point but the
	// top left point.
	//
	// This is caused by how QGraphicsLayoutItem works.
	//
	// Source: http://doc.qt.io/qt-5/qgraphicslayoutitem.html#setGeometry
	//

	if (m_pSourcePoint && m_pTargetPoint)
	{
		const QPointF sp = m_pSourcePoint->GetPosition();
		const QPointF tp = m_pTargetPoint->GetPosition();

		const qreal x = std::min(sp.x(), tp.x());
		const qreal y = std::min(sp.y(), tp.y());

		const qreal w = std::abs(sp.x() - tp.x());
		const qreal h = std::abs(sp.y() - tp.y());

		const QPointF topLeft = QPointF(x, y);
		const QRectF g = QRectF(topLeft, topLeft + QPointF(w, h));

		setGeometry(g);

		m_path = QPainterPath();
		const QPointF pathStart = mapFromScene(sp);
		const QPointF pathEnd = mapFromScene(tp);

		const float styleBezier = m_pStyle->GetBezier();
		const float bezier = styleBezier * ((pathStart - pathEnd).manhattanLength() / 200);

		const QPointF ctrlPos = QPointF(std::min(bezier, styleBezier), 0.0f);

		m_path.moveTo(pathStart);
		m_path.cubicTo(pathStart + ctrlPos, pathEnd - ctrlPos, pathEnd);
		m_selectionPoint = m_path.pointAtPercent(0.50);
	}
}

QPainterPath CConnectionWidget::shape() const
{
	return m_selectionShape;
}

void CConnectionWidget::hoverEnterEvent(QGraphicsSceneHoverEvent* pEvent)
{
	const bool isInsideShape = shape().contains(pEvent->pos());

	SMouseInputEventArgs mouseArgs = SMouseInputEventArgs(
	  EMouseEventReason::HoverEnter,
	  Qt::MouseButton::NoButton, Qt::MouseButton::NoButton, pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	SConnectionMouseEventArgs args(mouseArgs, isInsideShape);
	GetView().OnConnectionMouseEvent(this, args);
}

void CConnectionWidget::hoverMoveEvent(QGraphicsSceneHoverEvent* pEvent)
{
	const bool isInsideShape = shape().contains(pEvent->pos());

	SMouseInputEventArgs mouseArgs = SMouseInputEventArgs(
	  EMouseEventReason::HoverMove,
	  Qt::MouseButton::NoButton, Qt::MouseButton::NoButton, pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	SConnectionMouseEventArgs args(mouseArgs, isInsideShape);
	GetView().OnConnectionMouseEvent(this, args);
}

void CConnectionWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent* pEvent)
{
	SMouseInputEventArgs mouseArgs = SMouseInputEventArgs(
	  EMouseEventReason::HoverLeave,
	  Qt::MouseButton::NoButton, Qt::MouseButton::NoButton, pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	SConnectionMouseEventArgs args(mouseArgs, false);
	GetView().OnConnectionMouseEvent(this, args);
}

void CConnectionWidget::mousePressEvent(QGraphicsSceneMouseEvent* pEvent)
{
	const bool isInsideShape = shape().contains(pEvent->pos());

	SMouseInputEventArgs mouseArgs = SMouseInputEventArgs(
	  EMouseEventReason::ButtonPress,
	  pEvent->button(), pEvent->buttons(), pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	SConnectionMouseEventArgs args(mouseArgs, isInsideShape);
	GetView().OnConnectionMouseEvent(this, args);
}

void CConnectionWidget::mouseMoveEvent(QGraphicsSceneMouseEvent* pEvent)
{
	const bool isInsideShape = shape().contains(pEvent->pos());

	SMouseInputEventArgs mouseArgs = SMouseInputEventArgs(
	  EMouseEventReason::ButtonPressAndMove,
	  pEvent->button(), pEvent->buttons(), pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	SConnectionMouseEventArgs args(mouseArgs, isInsideShape);
	GetView().OnConnectionMouseEvent(this, args);
}

void CConnectionWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent* pEvent)
{
	const bool isInsideShape = shape().contains(pEvent->pos());

	SMouseInputEventArgs mouseArgs = SMouseInputEventArgs(
	  EMouseEventReason::ButtonRelease,
	  pEvent->button(), pEvent->buttons(), pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	SConnectionMouseEventArgs args(mouseArgs, isInsideShape);
	GetView().OnConnectionMouseEvent(this, args);
}

void CConnectionWidget::OnPointPositionChanged()
{
	Update();
}

void CConnectionWidget::UpdatePath()
{
	// TODO: This happens every frame when moving a node...
	m_selectionShape = QPainterPath();
	if (m_pSourcePoint && m_pTargetPoint)
	{
		QPointF shapeStart = mapFromScene(m_pSourcePoint->GetPosition());
		QPointF shapeEnd = mapFromScene(m_pTargetPoint->GetPosition());

		qreal radius = 18 / 2.0;

		m_selectionShape.moveTo(shapeStart);

		QVector2D prevPoint = QVector2D(shapeStart);
		for (uint32 i = 1; i < 16; ++i)
		{
			const qreal t = m_path.length() / 16.0 * i;
			const QVector2D pathPoint = QVector2D(m_path.pointAtPercent(i / 16.0));

			const QVector2D seg = prevPoint - pathPoint;
			const QVector2D dir = QVector2D(seg.y(), -seg.x()).normalized() * radius;

			m_selectionShape.lineTo(pathPoint.x() + dir.x(), pathPoint.y() + dir.y());

			prevPoint = pathPoint;
		}

		m_selectionShape.lineTo(shapeEnd);
		prevPoint = QVector2D(shapeEnd);

		for (uint32 i = 15; i > 0; --i)
		{
			const qreal t = m_path.length() / 16.0 * i;
			const QVector2D pathPoint = QVector2D(m_path.pointAtPercent(i / 16.0));

			const QVector2D seg = pathPoint - prevPoint;
			const QVector2D dir = QVector2D(-seg.y(), seg.x()).normalized() * radius;

			m_selectionShape.lineTo(pathPoint.x() + dir.x(), pathPoint.y() + dir.y());

			prevPoint = pathPoint;
		}

		m_selectionShape.lineTo(shapeStart);
	}
	// ~TODO
}

}

