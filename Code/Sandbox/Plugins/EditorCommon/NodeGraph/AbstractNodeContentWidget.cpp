// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AbstractNodeContentWidget.h"

#include "NodeWidget.h"
#include "PinWidget.h"

#include <QGraphicsSceneMoveEvent>
#include <QGraphicsLayout>

namespace CryGraphEditor {

CAbstractNodeContentWidget::CAbstractNodeContentWidget(CNodeWidget& node, CNodeGraphView& view)
	: m_node(node)
	, m_view(view)
	, m_isCollapsible(false)
{
	node.SetContentWidget(this);

	QObject::connect(&node, &CNodeWidget::SignalNodeMoved, this, &CAbstractNodeContentWidget::OnNodeMoved);
}

CAbstractNodeContentWidget::~CAbstractNodeContentWidget()
{

}

CPinWidget* CAbstractNodeContentWidget::GetPinWidget(const CAbstractPinItem& pin) const
{
	for (CPinWidget* pPinWidget : m_pins)
	{
		if (pPinWidget->GetItem().IsSame(pin))
			return pPinWidget;
	}

	return nullptr;
}

CConnectionPoint* CAbstractNodeContentWidget::GetConnectionPoint(const CAbstractPinItem& pin) const
{
	for (CPinWidget* pPinWidget : m_pins)
	{
		if (pPinWidget->GetItem().IsSame(pin))
			return &pPinWidget->GetConnectionPoint();
	}

	return nullptr;
}

void CAbstractNodeContentWidget::DeleteLater()
{

}

void CAbstractNodeContentWidget::OnNodeMoved()
{
	for (CPinWidget* pPinWidget : m_pins)
	{
		pPinWidget->UpdateConnectionPoint();
	}
}

void CAbstractNodeContentWidget::UpdateLayout(QGraphicsLayout* pLayout)
{
	m_node.DetachContentWidget();
	if (pLayout)
	{
		pLayout->activate();
		m_node.SetContentWidget(this);
		updateGeometry();
	}

	for (CPinWidget* pPinWidget : m_pins)
		pPinWidget->UpdateConnectionPoint();
}

void CAbstractNodeContentWidget::updateGeometry()
{
	QGraphicsWidget::updateGeometry();

	const QRectF nodeRect = GetNode().geometry() /*+ CNodeStyle::GetContentRectAdjustment()*/;

	for (CPinWidget* pPinWidget : m_pins)
	{
		const QPointF nodePos = nodeRect.topLeft();
		const QPointF pinPos = pPinWidget->scenePos();

		const float y = pPinWidget->geometry().height() / 2;
		if (pPinWidget->GetItem().IsInputPin())
		{
			const float x = nodePos.x() - pinPos.x();
			pPinWidget->SetIconOffset(x, y);
		}
		else
		{
			const float x = nodePos.x() + nodeRect.width() - pinPos.x();
			pPinWidget->SetIconOffset(x, y);
		}
	}
}

}

