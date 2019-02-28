// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "NodeInfoWidget.h"

#include "NodeWidget.h"
#include "NodeWidgetStyle.h"
#include "NodeInfoWidgetStyle.h"
#include "NodeGraphView.h"
#include "AbstractNodeItem.h"

namespace CryGraphEditor {

CNodeInfoWidget::CNodeInfoWidget(CNodeWidget& nodeWidget)
	: m_nodeWidget(nodeWidget)
	, m_view(nodeWidget.GetView())
{
	m_pStyle = &(nodeWidget.GetStyle().GetInfoWidgetStyle());

	const qreal height = m_pStyle->GetHeight();
	setMaximumHeight(height);
	setMinimumHeight(height);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	Update();
}

void CNodeInfoWidget::Update()
{
	CAbstractNodeItem& item = m_nodeWidget.GetItem();
	m_showError = item.HasErrors();
	m_showWarning = item.HasWarnings();
	setVisible(m_showError || m_showWarning);
}

void CNodeInfoWidget::paint(QPainter* pPainter, const QStyleOptionGraphicsItem* pOption, QWidget* pWidget)
{
	const bool wasAntialisingSet = pPainter->renderHints().testFlag(QPainter::Antialiasing);
	pPainter->setRenderHints(QPainter::Antialiasing, true);

	const QRectF geom = boundingRect().adjusted(0, 0, 1, 1);

	QFont font = pPainter->font();
	font.setBold(true);
	pPainter->setFont(font);

	pPainter->setPen(Qt::NoPen);
	if (m_showError)
	{
		pPainter->setBrush(m_pStyle->GetErrorBackgroundColor());
		pPainter->drawRect(geom);

		if (m_view.GetZoom() >= 70)
		{
			pPainter->setBrush(Qt::NoBrush);
			pPainter->setPen(m_pStyle->GetErrorTextColor());
			pPainter->drawText(geom, Qt::AlignCenter, m_pStyle->GetErrorText());
		}
	}
	else
	{
		pPainter->setBrush(m_pStyle->GetWarningBackgroundColor());
		pPainter->drawRect(geom);

		if (m_view.GetZoom() >= 70)
		{
			pPainter->setBrush(Qt::NoBrush);
			pPainter->setPen(m_pStyle->GetWarningTextColor());
			pPainter->drawText(geom, Qt::AlignCenter, m_pStyle->GetWarningText());
		}
	}

	if (wasAntialisingSet)
		pPainter->setRenderHints(QPainter::Antialiasing, false);
}

}

