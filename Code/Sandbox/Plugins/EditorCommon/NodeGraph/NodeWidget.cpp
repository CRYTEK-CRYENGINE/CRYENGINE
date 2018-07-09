// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "NodeWidget.h"

#include "NodeGraphView.h"
#include "NodeInfoWidget.h"
#include "NodeHeaderWidget.h"
#include "NodeWidgetStyle.h"

#include "AbstractNodeContentWidget.h"

#include <QGraphicsLinearLayout>
#include <QGraphicsSceneHoverEvent>

namespace CryGraphEditor {

CNodeWidget::CNodeWidget(CAbstractNodeItem& item, CNodeGraphView& view)
	: CNodeGraphViewGraphicsWidget(view)
	, m_item(item)
	, m_pContent(nullptr)
{
	setAcceptHoverEvents(true);
	setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

	m_pStyle = GetView().GetStyle().GetNodeWidgetStyle(m_item.GetStyleId());
	CRY_ASSERT(m_pStyle);
	if (!m_pStyle)
	{
		m_pStyle = GetView().GetStyle().GetNodeWidgetStyle("Node");
	}

	QGraphicsLinearLayout* pLayout = new QGraphicsLinearLayout();
	pLayout->setOrientation(Qt::Vertical);
	pLayout->setSpacing(0.0f);
	const qreal borderWidth = m_pStyle->GetBorderWidth();
	pLayout->setContentsMargins(borderWidth, borderWidth, borderWidth, borderWidth);
	pLayout->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

	const QMargins contentMargins = m_pStyle->GetMargins();
	m_pContentLayout = new QGraphicsLinearLayout();
	m_pContentLayout->setOrientation(Qt::Vertical);
	m_pContentLayout->setSpacing(0.0f);
	m_pContentLayout->setContentsMargins(contentMargins.left(), contentMargins.top(), contentMargins.right(), contentMargins.bottom());
	m_pContentLayout->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

	m_pInfoBar = new CNodeInfoWidget(*this);
	m_pHeader = new CNodeHeader(*this);

	pLayout->addItem(m_pHeader);
	pLayout->setAlignment(m_pHeader, Qt::AlignVCenter);
	pLayout->addItem(m_pContentLayout);
	pLayout->addItem(m_pInfoBar);
	pLayout->setAlignment(m_pInfoBar, Qt::AlignVCenter);

	setLayout(pLayout);
	setPos(item.GetPosition());

	SetDefaultZValue(1.0f);

	// TODO: Those functions should be called through signals but directly instead.
	item.SignalPositionChanged.Connect(this, &CNodeWidget::OnItemPositionChanged);
	item.SignalDeactivatedChanged.Connect(this, &CNodeWidget::OnItemDeactivatedChanged);
	item.SignalInvalidated.Connect(this, &CNodeWidget::OnItemInvalidated);
	// ~TODO

	item.SignalValidated.Connect(this, &CNodeWidget::OnItemValidated);

	SignalSelectionChanged.Connect(this, &CNodeWidget::OnSelectionChanged);
}

CNodeWidget::~CNodeWidget()
{
	// Don't use the destructor for clean up. Use DeleteLater() instead.
}

void CNodeWidget::DeleteLater()
{
	CNodeGraphViewGraphicsWidget::DeleteLater();

	m_item.SignalPositionChanged.DisconnectObject(this);
	m_item.SignalDeactivatedChanged.DisconnectObject(this);
	m_item.SignalInvalidated.DisconnectObject(this);

	m_item.SignalValidated.DisconnectObject(this);

	// TODO: Do we really need a delete later on the content?
	m_pContent->DeleteLater();
	// ~TODO

	QGraphicsLinearLayout* pLayout = static_cast<QGraphicsLinearLayout*>(layout());
	if (pLayout)
	{
		pLayout->removeItem(m_pHeader);
		delete m_pHeader;
		pLayout->removeItem(m_pInfoBar);
		delete m_pInfoBar;
		pLayout->removeItem(m_pContent);
		delete m_pContent;
	}
}

void CNodeWidget::OnSelectionChanged(bool isSelected)
{
	if (m_pHeader)
		m_pHeader->OnSelectionChanged(isSelected);
}

void CNodeWidget::SetContentWidget(CAbstractNodeContentWidget* pContent)
{
	if (m_pContent == pContent)
		return;

	if (m_pContent)
	{
		m_pContentLayout->removeItem(m_pContent);
		m_pContentLayout->activate();
		delete m_pContent;
	}

	m_pContent = pContent;
	m_pContentLayout->addItem(m_pContent);
	layout()->activate();
}

void CNodeWidget::DetachContentWidget()
{
	if (m_pContent)
	{
		m_pContentLayout->removeItem(m_pContent);
		m_pContentLayout->activate();
		m_pContent = nullptr;
	}
}

CAbstractNodeContentWidget* CNodeWidget::GetContentWidget() const
{
	return m_pContent;
}

void CNodeWidget::UpdateContentWidget()
{
	m_pContent->OnLayoutChanged();
}

QString CNodeWidget::GetName() const
{
	return m_pHeader->GetName();
}

void CNodeWidget::paint(QPainter* pPainter, const QStyleOptionGraphicsItem* pOption, QWidget* pWidget)
{
	if (!m_pStyle)
		return;

	const bool wasAntialisingSet = pPainter->renderHints().testFlag(QPainter::Antialiasing);
	pPainter->setRenderHints(QPainter::Antialiasing, true);
	pPainter->setOpacity(opacity());

	const QSizeF nodeSize = geometry().size();
	const QRectF nodeRect = boundingRect();
	const QColor bgColor = m_pStyle->GetBackgroundColor();

	// Content
	QRectF contentRect = nodeRect;
	pPainter->setBrush(bgColor);
	pPainter->setPen(Qt::PenStyle::NoPen);
	pPainter->drawRect(contentRect);

	// Border
	contentRect = nodeRect;
	const float borderWidth = m_pStyle->GetBorderWidth();
	if (borderWidth > 0.0 && !IsHighlighted() && !IsSelected())
	{
		pPainter->setBrush(Qt::NoBrush);
		pPainter->setPen(QPen(m_pStyle->GetBorderColor(), borderWidth, Qt::SolidLine));
		pPainter->drawRect(contentRect);
	}

	// Selection
	pPainter->setBrush(Qt::BrushStyle::NoBrush);
	if (IsSelected())
	{
		pPainter->setPen(QPen(m_pStyle->GetSelectionColor(), borderWidth, Qt::SolidLine));
		pPainter->drawRect(nodeRect);
	}
	else if (IsHighlighted())
	{
		pPainter->setPen(QPen(m_pStyle->GetHighlightColor(), borderWidth, Qt::SolidLine));
		pPainter->drawRect(nodeRect);
	}

	if (wasAntialisingSet)
		pPainter->setRenderHints(QPainter::Antialiasing, false);
}

void CNodeWidget::hoverEnterEvent(QGraphicsSceneHoverEvent* pEvent)
{
	SMouseInputEventArgs mouseArgs = SMouseInputEventArgs(
	  EMouseEventReason::HoverEnter,
	  Qt::MouseButton::NoButton, Qt::MouseButton::NoButton, pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	mouseArgs.SetAccepted(false);
	m_pContent->OnInputEvent(this, mouseArgs);

	if (m_pContent == nullptr || !mouseArgs.GetAccepted())
	{
		SNodeMouseEventArgs args(mouseArgs);
		GetView().OnNodeMouseEvent(this, args);
	}
}

void CNodeWidget::hoverMoveEvent(QGraphicsSceneHoverEvent* pEvent)
{
	SMouseInputEventArgs mouseArgs = SMouseInputEventArgs(
	  EMouseEventReason::HoverMove,
	  Qt::MouseButton::NoButton, Qt::MouseButton::NoButton, pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	mouseArgs.SetAccepted(false);
	m_pContent->OnInputEvent(this, mouseArgs);

	if (m_pContent == nullptr || !mouseArgs.GetAccepted())
	{
		SNodeMouseEventArgs args(mouseArgs);
		GetView().OnNodeMouseEvent(this, args);
	}
}

void CNodeWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent* pEvent)
{

	SMouseInputEventArgs mouseArgs = SMouseInputEventArgs(
	  EMouseEventReason::HoverLeave,
	  Qt::MouseButton::NoButton, Qt::MouseButton::NoButton, pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	mouseArgs.SetAccepted(false);
	m_pContent->OnInputEvent(this, mouseArgs);

	if (m_pContent == nullptr || !mouseArgs.GetAccepted())
	{
		SNodeMouseEventArgs args(mouseArgs);
		GetView().OnNodeMouseEvent(this, args);
	}
}

void CNodeWidget::mousePressEvent(QGraphicsSceneMouseEvent* pEvent)
{
	SMouseInputEventArgs mouseArgs = SMouseInputEventArgs(
	  EMouseEventReason::ButtonPress,
	  pEvent->button(), pEvent->buttons(), pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	mouseArgs.SetAccepted(false);
	m_pContent->OnInputEvent(this, mouseArgs);

	if (m_pContent == nullptr || !mouseArgs.GetAccepted())
	{
		SNodeMouseEventArgs args(mouseArgs);
		GetView().OnNodeMouseEvent(this, args);
	}
}

void CNodeWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent* pEvent)
{
	SMouseInputEventArgs mouseArgs = SMouseInputEventArgs(
	  EMouseEventReason::ButtonRelease,
	  pEvent->button(), pEvent->buttons(), pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	mouseArgs.SetAccepted(false);
	m_pContent->OnInputEvent(this, mouseArgs);

	if (m_pContent == nullptr || !mouseArgs.GetAccepted())
	{
		SNodeMouseEventArgs args(mouseArgs);
		GetView().OnNodeMouseEvent(this, args);
	}
}

void CNodeWidget::mouseMoveEvent(QGraphicsSceneMouseEvent* pEvent)
{
	SMouseInputEventArgs mouseArgs = SMouseInputEventArgs(
	  EMouseEventReason::ButtonPressAndMove,
	  pEvent->button(), pEvent->buttons(), pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	mouseArgs.SetAccepted(false);
	m_pContent->OnInputEvent(this, mouseArgs);

	if (m_pContent == nullptr || !mouseArgs.GetAccepted())
	{
		SNodeMouseEventArgs args(mouseArgs);
		GetView().OnNodeMouseEvent(this, args);
	}
}

void CNodeWidget::moveEvent(QGraphicsSceneMoveEvent* pEvent)
{
	SignalNodeMoved();
}

void CNodeWidget::OnItemInvalidated()
{
	m_pHeader->SetName(m_item.GetName());
	CNodeGraphViewGraphicsWidget::OnItemInvalidated();

	if (m_pInfoBar)
		m_pInfoBar->Update();

	if (m_pContent)
		m_pContent->OnItemInvalidated();
}

void CNodeWidget::OnItemValidated(CAbstractNodeItem& item)
{
	if (m_pInfoBar)
		m_pInfoBar->Update();
}

void CNodeWidget::EditName()
{
	m_pHeader->EditName();
}

void CNodeWidget::AddHeaderIcon(CNodeHeaderIcon* pHeaderIcon, CNodeHeader::EIconSlot slot)
{
	m_pHeader->AddIcon(pHeaderIcon, slot);
}

void CNodeWidget::SetHeaderNameWidth(int32 width)
{
	m_pHeader->SetNameWidth(width);
}

}

