// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "GroupWidget.h"

#include "NodeGraphView.h"

#include <QPainter>
#include <QBrush>
#include <QPointF>

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>

namespace CryGraphEditor {

CGroupWidget::CGroupWidget(CNodeGraphView& view)
	: CNodeGraphViewGraphicsWidget(view)
{
	SetDefaultZValue(0.8);
}

CGroupWidget::~CGroupWidget()
{

}

void CGroupWidget::DeleteLater()
{
	CNodeGraphViewGraphicsWidget::DeleteLater();
}

void CGroupWidget::SetAA(const QPointF& positon)
{
	m_aa = positon;
	prepareGeometryChange();
	updateGeometry();
}

void CGroupWidget::SetBB(const QPointF& positon)
{
	m_bb = positon;
	prepareGeometryChange();
	updateGeometry();
}

void CGroupWidget::paint(QPainter* pPainter, const QStyleOptionGraphicsItem* pOption, QWidget* pWidget)
{
	if (isVisible())
	{
		pPainter->save();

		pPainter->setBrush(QColor(m_color.red(), m_color.green(), m_color.blue(), 90));
		pPainter->setPen(m_color);
		pPainter->drawRect(boundingRect());

		pPainter->setBrush(Qt::NoBrush);
		pPainter->setPen(QColor(255, 0, 0));

		pPainter->restore();
	}
}

QRectF CGroupWidget::boundingRect() const
{
	const QRectF geometryRect = geometry();
	return QRectF(0, 0, geometryRect.width(), geometryRect.height());
}

void CGroupWidget::updateGeometry()
{
	const QPointF& AA = m_aa;
	const QPointF& BB = m_bb;

	const qreal x = std::min(AA.x(), BB.x());
	const qreal y = std::min(AA.y(), BB.y());

	const qreal w = std::abs(AA.x() - BB.x());
	const qreal h = std::abs(AA.y() - BB.y());

	const QPointF topLeft = QPointF(x, y);
	const QRectF g = QRectF(topLeft, topLeft + QPointF(w, h)).adjusted(-1.0, -1.0, 1.0, 1.0);

	setGeometry(g);
}

void CGroupWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent* pEvent)
{
	SMouseInputEventArgs mouseArgs = SMouseInputEventArgs(
	  EMouseEventReason::ButtonRelease,
	  pEvent->button(), pEvent->buttons(), pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	SGroupMouseEventArgs args(mouseArgs);
	GetView().OnGroupMouseEvent(this, args);
}

void CGroupWidget::mouseMoveEvent(QGraphicsSceneMouseEvent* pEvent)
{
	m_bb = pEvent->scenePos();

	SMouseInputEventArgs mouseArgs = SMouseInputEventArgs(
	  EMouseEventReason::ButtonPressAndMove,
	  pEvent->button(), pEvent->buttons(), pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	SGroupMouseEventArgs args(mouseArgs);
	GetView().OnGroupMouseEvent(this, args);

	prepareGeometryChange();
	updateGeometry();
}

void CGroupWidget::moveEvent(QGraphicsSceneMoveEvent* pEvent)
{
	// TODO: Signal to grouped items.
}

void CGroupWidget::UpdateContainingSceneItems()
{
	m_items.clear();

	QList<QGraphicsItem*> items = GetView().scene()->items(geometry());
	for (QGraphicsItem* pItem : items)
	{
		CNodeGraphViewGraphicsWidget* pGraphWidget = qgraphicsitem_cast<CNodeGraphViewGraphicsWidget*>(pItem);
		const int32 widgetType = pGraphWidget ? pGraphWidget->GetType() : eGraphViewWidgetType_Unset;
		if (widgetType == eGraphViewWidgetType_NodeWidget || widgetType == eGraphViewWidgetType_ConnectionWidget)
		{
			m_items.insert(pGraphWidget);
		}
	}
}

}

