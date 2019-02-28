// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "GraphicsSceneIcon.h"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

namespace CryGraphEditor {

CGraphicsSceneIcon::CGraphicsSceneIcon(const QPixmap& pixmap)
	: QGraphicsPixmapItem(pixmap, nullptr)
{
	setAcceptHoverEvents(true);
}

void CGraphicsSceneIcon::paint(QPainter* pPainter, const QStyleOptionGraphicsItem* pOption, QWidget* pWidget)
{
	// DEBUG
	/*QRectF rect = boundingRect();
	   pPainter->setPen(Qt::PenStyle::NoPen);
	   pPainter->drawRect(rect);*/

	QGraphicsPixmapItem::paint(pPainter, pOption, pWidget);
}

void CGraphicsSceneIcon::hoverEnterEvent(QGraphicsSceneHoverEvent* pEvent)
{
	SignalHoverEnterEvent(*this, pEvent);
	pEvent->accept();
}

void CGraphicsSceneIcon::hoverMoveEvent(QGraphicsSceneHoverEvent* pEvent)
{
	SignalHoverMoveEvent(*this, pEvent);
	pEvent->accept();
}

void CGraphicsSceneIcon::hoverLeaveEvent(QGraphicsSceneHoverEvent* pEvent)
{
	SignalHoverLeaveEvent(*this, pEvent);
	pEvent->accept();
}

void CGraphicsSceneIcon::mousePressEvent(QGraphicsSceneMouseEvent* pEvent)
{
	SignalMousePressEvent(*this, pEvent);
	pEvent->accept();
}

void CGraphicsSceneIcon::mouseReleaseEvent(QGraphicsSceneMouseEvent* pEvent)
{
	SignalMouseReleaseEvent(*this, pEvent);
	pEvent->accept();
}

}

