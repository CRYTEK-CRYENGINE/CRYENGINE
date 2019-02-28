// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "NodeHeaderIconWidget.h"

#include "NodeHeaderWidget.h"
#include "NodeGraphViewStyle.h"
#include "NodeWidget.h"

#include <QtUtil.h>

namespace CryGraphEditor {

CNodeHeaderIcon::CNodeHeaderIcon(CNodeWidget& nodeWidget)
	: m_nodeWidget(nodeWidget)
	, m_pPixmap(nullptr)
{
	setAcceptHoverEvents(true);
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	setMaximumSize(QSizeF(16.0, 16.0) /*CNodeStyle::GetHeaderIconSize()*/);
	setMinimumSize(QSizeF(16.0, 16.0) /*CNodeStyle::GetHeaderIconSize()*/);
	setPreferredSize(QSizeF(16.0, 16.0) /*CNodeStyle::GetHeaderIconSize()*/);
}

CAbstractNodeItem& CNodeHeaderIcon::GetNodeItem() const
{
	return m_nodeWidget.GetItem();
}

void CNodeHeaderIcon::paint(QPainter* pPainter, const QStyleOptionGraphicsItem* pOption, QWidget* pWidget)
{
	if (m_pPixmap)
	{
		pPainter->drawPixmap(boundingRect(), *m_pPixmap, m_pPixmap->rect());
	}
}

void CNodeHeaderIcon::hoverEnterEvent(QGraphicsSceneHoverEvent* pEvent)
{
	OnMouseEnter();
}

void CNodeHeaderIcon::hoverLeaveEvent(QGraphicsSceneHoverEvent* pEvent)
{
	OnMouseLeave();
}

void CNodeHeaderIcon::mouseReleaseEvent(QGraphicsSceneMouseEvent* pEvent)
{
	OnClicked();
}

QRectF CNodeHeaderIcon::boundingRect() const
{
	return QRectF(QPointF(0, 0), QSizeF(16.0, 16.0) /*CNodeStyle::GetHeaderIconSize()*/);
}

QSizeF CNodeHeaderIcon::sizeHint(Qt::SizeHint which, const QSizeF& constraint) const
{
	return QSizeF(16.0, 16.0) /*CNodeStyle::GetHeaderIconSize()*/;
}

}

