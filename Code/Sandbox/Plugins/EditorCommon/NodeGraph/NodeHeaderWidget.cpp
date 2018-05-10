// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "NodeHeaderWidget.h"

#include "NodeHeaderWidgetStyle.h"
#include "NodeWidget.h"
#include "NodeWidgetStyle.h"
#include "NodeHeaderIconWidget.h"
#include "NodeNameWidget.h"
#include "NodeGraphView.h"
#include "AbstractNodeItem.h"

#include <QGraphicsLinearLayout>
#include <QGraphicsWidget>
#include <QGraphicsScene>

namespace CryGraphEditor {

class CNodeTypeIcon : public CNodeHeaderIcon
{
public:
	CNodeTypeIcon(CNodeWidget& nodeWidget)
		: CNodeHeaderIcon(nodeWidget)
		, m_style(nodeWidget.GetStyle().GetHeaderWidgetStyle())
	{
		SetDisplayIcon(&m_style.GetNodeIconViewPixmap(StyleState_Default));
	}

	CNodeTypeIcon::~CNodeTypeIcon()
	{
	}

private:
	const CNodeHeaderWidgetStyle& m_style;
};

CNodeHeader::CNodeHeader(CNodeWidget& nodeWidget)
	: m_nodeWidget(nodeWidget)
	, m_view(nodeWidget.GetView())
{
	CAbstractNodeItem& item = nodeWidget.GetItem();

	m_pStyle = &(nodeWidget.GetStyle().GetHeaderWidgetStyle());

	const qreal height = m_pStyle->GetHeight();
	setMaximumHeight(height);
	setMinimumHeight(height);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	m_pName = new CNodeNameWidget(nodeWidget);
	m_pName->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	m_pName->setFixedHeight(height - 2);

	QGraphicsScene* pScene = nodeWidget.GetView().scene();
	QGraphicsWidget* pName = AddWidget(pScene, m_pName);

	const QMargins hm = m_pStyle->GetMargins();
	m_pLayout = new QGraphicsLinearLayout();
	m_pLayout->setOrientation(Qt::Horizontal);
	m_pLayout->setContentsMargins(hm.left(), hm.top(), hm.right(), hm.bottom());
	m_pLayout->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

	m_pLayout->addItem(pName);
	m_pLayout->setAlignment(pName, Qt::AlignVCenter);
	m_pLayout->addStretch(hm.left());

	CNodeTypeIcon* pHeaderIcon = new CNodeTypeIcon(nodeWidget);
	AddIcon(pHeaderIcon, EIconSlot::Left);

	setLayout(m_pLayout);

	// Note: We need to set the name after the layout was set because otherwise the width
	// requirement calculation is wrong.
	m_pName->SetName(m_nodeWidget.GetItem().GetName());
}

QString CNodeHeader::GetName() const
{
	return m_pName->text();
}

void CNodeHeader::SetName(QString name)
{
	m_pName->SetName(name);
}

void CNodeHeader::EditName()
{
	m_pName->TriggerEdit();
}

void CNodeHeader::paint(QPainter* pPainter, const QStyleOptionGraphicsItem* pOption, QWidget* pWidget)
{
	const bool wasAntialisingSet = pPainter->renderHints().testFlag(QPainter::Antialiasing);
	pPainter->setRenderHints(QPainter::Antialiasing, true);

	const QRectF geom = boundingRect().adjusted(0, 0, 1, 1);
	if (m_nodeWidget.IsSelected())
	{
		const QColor selectionColor = m_pStyle->GetSelectionColor();
		pPainter->setBrush(QBrush(selectionColor));
	}
	else if (m_nodeWidget.IsDeactivated())
	{
		// TODO: Deactivation color!
		pPainter->setBrush(QBrush(QColor(94, 94, 94)));
		// ~TODO
	}
	else
	{
		CAbstractNodeItem& nodeItem = m_nodeWidget.GetItem();

		const float hc = m_pStyle->GetHeight() / 2;
		QLinearGradient headerGradient(QPointF(0, hc), QPointF(m_nodeWidget.size().width(), hc));
		headerGradient.setColorAt(0, m_pStyle->GetLeftColor());
		headerGradient.setColorAt(1, m_pStyle->GetRightColor());

		pPainter->setBrush(QBrush(headerGradient));
	}

	pPainter->setPen(Qt::PenStyle::NoPen);
	pPainter->drawRect(geom);

	if (wasAntialisingSet)
		pPainter->setRenderHints(QPainter::Antialiasing, false);
}

void CNodeHeader::AddIcon(CNodeHeaderIcon* pHeaderIcon, EIconSlot slot)
{
	const QMargins hm = m_pStyle->GetMargins();

	if (slot == EIconSlot::Left)
	{
		if (hm.left() > 0)
			m_pLayout->insertStretch(0, hm.left());

		m_pLayout->insertItem(0, pHeaderIcon);
		m_pLayout->setAlignment(pHeaderIcon, Qt::AlignVCenter | Qt::AlignLeft);
	}
	else
	{
		m_pLayout->addItem(pHeaderIcon);
		m_pLayout->setAlignment(pHeaderIcon, Qt::AlignVCenter | Qt::AlignRight);

		if (hm.right() > 0)
			m_pLayout->addStretch(hm.right());
	}

	m_pLayout->activate();
}

void CNodeHeader::SetNameWidth(int32 width)
{
	m_pName->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	m_pName->setFixedWidth(width);
}

void CNodeHeader::SetNameColor(QColor color)
{
	m_pName->SetTextColor(color);
}

void CNodeHeader::OnSelectionChanged(bool isSelected)
{
	m_pName->SetSelectionStyle(isSelected);
}

}

