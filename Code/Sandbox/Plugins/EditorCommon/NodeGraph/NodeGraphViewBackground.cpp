// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "NodeGraphViewBackground.h"

#include "NodeGraphViewStyle.h"

#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace CryGraphEditor {

CNodeGraphViewBackground::CNodeGraphViewBackground(QGraphicsItem* pParent)
	: QGraphicsWidget(pParent)
	, m_pStyle(nullptr)
{
	setZValue(-2.0f);
}

void CNodeGraphViewBackground::paint(QPainter* pPainter, const QStyleOptionGraphicsItem* pOption, QWidget* pWidget)
{
	if (!pOption || !m_pStyle || m_pStyle->GetGridSegmentSize() <= 0.0f || m_pStyle->GetGridSubSegmentCount() <= 0.0f)
		return;

	const QRectF clientRect = pOption->exposedRect;

	const float segmentSize = m_pStyle->GetGridSegmentSize();
	const float subSegmentSize = segmentSize / m_pStyle->GetGridSubSegmentCount();

	pPainter->fillRect(clientRect, m_pStyle->GetGridBackgroundColor());

	pPainter->setPen(QPen(m_pStyle->GetGridSubSegmentLineColor(), m_pStyle->GetGridSubSegmentLineWidth(), Qt::SolidLine));
	DrawGrid(pPainter, clientRect, subSegmentSize);

	pPainter->setPen(QPen(m_pStyle->GetGridSegmentLineColor(), m_pStyle->GetGridSegementLineWidth(), Qt::SolidLine));
	DrawGrid(pPainter, clientRect, segmentSize);
}

void CNodeGraphViewBackground::DrawGrid(QPainter* painter, const QRectF rect, const float gridSize) const
{
	const float startX = rect.left();
	const float startY = rect.top();
	const float endX = rect.right();
	const float endY = rect.bottom();

	const float firstX = std::floor(startX / gridSize) * gridSize;
	const float firstY = std::floor(startY / gridSize) * gridSize;

	const size_t numLines = (endX - firstX) / gridSize + (endY - firstY) / gridSize;

	QVector<QLineF> lines;
	lines.reserve(numLines);

	for (float x = firstX; x <= endX; x += gridSize)
		lines.append(QLine(x, startY, x, endY));
	for (float y = firstY; y <= endY; y += gridSize)
		lines.append(QLine(startX, y, endX, y));

	painter->drawLines(lines);
}

void CNodeGraphViewBackground::mousePressEvent(QGraphicsSceneMouseEvent* pEvent)
{
	if (pEvent->button() != Qt::RightButton)
		QGraphicsWidget::mousePressEvent(pEvent);
}

void CNodeGraphViewBackground::mouseReleaseEvent(QGraphicsSceneMouseEvent* pEvent)
{
	if (pEvent->button() == Qt::RightButton)
	{
		OnMouseRightReleasedEvent(/**this, pEvent*/);
	}
}

}

