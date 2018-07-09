// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "PinWidget.h"

#include "NodePinWidgetStyle.h"
#include "NodeGraphView.h"
#include "NodeGraphViewStyle.h"
#include "GraphicsSceneIcon.h"
#include "NodeWidget.h"
#include "NodeWidgetStyle.h"
#include "AbstractNodeContentWidget.h"

#include <QGraphicsLinearLayout>
#include <QLabel>
#include <QGraphicsProxyWidget>
#include <QGraphicsPixmapItem>

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QMargins>
#include <QFontMetrics>

#include <CryIcon.h>

namespace CryGraphEditor {

class CProxyWidget : public QGraphicsProxyWidget
{
public:
	CProxyWidget(QWidget* pWidget)
	{
		setWidget(pWidget);
		setAcceptHoverEvents(false);
	}

	~CProxyWidget()
	{
		return;
	}

	void hoverEnterEvent(QGraphicsSceneHoverEvent* pEvent) override
	{
		pEvent->setAccepted(false);
	}

	void hoverMoveEvent(QGraphicsSceneHoverEvent* pEvent) override
	{
		pEvent->setAccepted(false);
	}

	void hoverLeaveEvent(QGraphicsSceneHoverEvent* pEvent) override
	{
		pEvent->setAccepted(false);
	}

	void mousePressEvent(QGraphicsSceneMouseEvent* pEvent) override
	{
		pEvent->setAccepted(false);
	}

	void mouseReleaseEvent(QGraphicsSceneMouseEvent* pEvent) override
	{
		pEvent->setAccepted(false);
	}

	void mouseMoveEvent(QGraphicsSceneMouseEvent* pEvent) override
	{
		pEvent->setAccepted(false);
	}
};

class CPinName : public QLabel
{
public:
	CPinName(QString name, QWidget* pParent = nullptr)
		: QLabel(name, pParent)
	{

	}

	virtual void paintEvent(QPaintEvent* pPaintEvent) override
	{}
};

CPinWidget::CPinWidget(CAbstractPinItem& item, CNodeWidget& nodeWidget, CNodeGraphView& view, bool displayPinName)
	: CNodeGraphViewGraphicsWidget(view)
	, m_item(item)
	, m_pContent(nullptr)
	, m_pContentProxy(nullptr)
	, m_pActiveIcon(nullptr)
	, m_isConnecting(false)
	, m_nodeWidget(nodeWidget)
	, m_pName(nullptr)
{
	m_isInput = m_item.IsInputPin();

	m_pLayout = new QGraphicsLinearLayout();
	m_pLayout->setOrientation(Qt::Horizontal);
	m_pLayout->setSpacing(0.0f);
	m_pLayout->setContentsMargins(0.0f, 0.0f, 0.0f, 0.0f);

	m_pStyle = GetView().GetStyle().GetPinWidgetStyle(m_item.GetStyleId());
	CRY_ASSERT(m_pStyle);
	if (!m_pStyle)
	{
		m_pStyle = GetView().GetStyle().GetPinWidgetStyle("Pin");
	}

	LoadIcons();

	if (displayPinName)
	{
		CPinName* pName = new CPinName(m_item.GetName());
		pName->setFont(m_pStyle->GetTextFont());
		pName->setStyleSheet(QString("QLabel { font: bold; color: rgb(94, 94, 94); background: rgba(0, 0, 0, 0) }"));
		pName->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

		if (item.IsInputPin())
			pName->setAlignment(Qt::AlignLeft);
		else
			pName->setAlignment(Qt::AlignRight);

		SetNameWidget(pName);
	}

	setLayout(m_pLayout);

	m_nodeWidget.SignalSelectionChanged.Connect(this, &CPinWidget::OnNodeSelectionChanged);

	m_item.SignalInvalidated.Connect(this, &CPinWidget::OnInvalidated);

	GetView().RegisterConnectionEventListener(this);

	SignalHighlightedChanged.Connect(this, &CPinWidget::OnStateChanged);
	SignalSelectionChanged.Connect(this, &CPinWidget::OnStateChanged);
	SignalDeactivatedChanged.Connect(this, &CPinWidget::OnStateChanged);
}

CPinWidget::~CPinWidget()
{
	SignalHighlightedChanged.DisconnectObject(this);
	SignalSelectionChanged.DisconnectObject(this);
	SignalDeactivatedChanged.DisconnectObject(this);

	m_nodeWidget.SignalSelectionChanged.DisconnectObject(this);

	GetView().RemoveConnectionEventListener(this);
}

void CPinWidget::DeleteLater()
{
	m_item.SignalInvalidated.DisconnectObject(this);
	m_nodeWidget.SignalSelectionChanged.DisconnectObject(this);

	GetView().RemoveConnectionEventListener(this);
	CNodeGraphViewGraphicsWidget::DeleteLater();
}

void CPinWidget::SetNameWidget(CPinName* pWidget)
{
	if (m_pContent == pWidget)
		return;

	QGraphicsScene* pScene = GetView().scene();
	if (m_pContent)
	{
		m_pName = nullptr;
		m_pLayout->removeItem(m_pContentProxy);

		m_pContent->deleteLater();
		m_pContentProxy->deleteLater();
	}

	m_pContent = pWidget;
	if (m_pContent)
	{
		m_pName = pWidget;
		m_pContentProxy = new CProxyWidget(pWidget);
		m_pLayout->addItem(m_pContentProxy);
	}

	m_pLayout->activate();
}

void CPinWidget::SetIconOffset(float x, float y)
{
	for (CGraphicsSceneIcon* pIcon : m_pIcons)
	{
		if (pIcon)
		{
			const float iconRadius = pIcon->boundingRect().height() / 2;
			const QPointF styleOffset = m_pStyle->GetIconOffset();
			if (m_item.IsInputPin())
			{
				const QPointF actualOffset(
				  x - iconRadius + styleOffset.x(),
				  y - iconRadius + styleOffset.y()
				  );
				pIcon->setOffset(actualOffset);
			}
			else
			{
				const QPointF actualOffset(
				  x - iconRadius - styleOffset.x(),
				  y - iconRadius + styleOffset.y()
				  );
				pIcon->setOffset(actualOffset);
			}
		}
	}

	UpdateConnectionPoint();

	if (m_pName)
	{
		const QRectF nameRect = m_pName->rect();
		const QPointF namePos = mapFromScene(nameRect.topLeft() + scenePos());
		const qreal textWidth = m_pName->fontMetrics().width(m_pName->text());

		const QPointF iconPos = mapFromScene(m_pActiveIcon->scenePos() + m_pActiveIcon->offset());
		const QRectF iconRect = m_pActiveIcon->boundingRect();

		if (namePos.x() >= iconPos.x())
		{
			const qreal topX = iconPos.x();
			const qreal topY = namePos.y();
			m_rect.setTopLeft(QPointF(topX, topY));

			const qreal botX = namePos.x() + textWidth;
			const qreal botY = namePos.y() + nameRect.height();
			m_rect.setBottomRight(QPointF(botX, botY));
		}
		else
		{
			const qreal topX = namePos.x() + (nameRect.width() - textWidth);
			const qreal topY = namePos.y();
			m_rect.setTopLeft(QPointF(topX, topY));

			const qreal botX = iconPos.x() + iconRect.width();
			const qreal botY = namePos.y() + nameRect.height();
			m_rect.setBottomRight(QPointF(botX, botY));
		}
	}
}

void CPinWidget::paint(QPainter* pPainter, const QStyleOptionGraphicsItem* pOption, QWidget* pWidget)
{
	if (!m_pStyle)
		return;

	pPainter->save();

	const Qt::Alignment alignment = Qt::AlignVCenter | (m_isInput ? Qt::AlignLeft : Qt::AlignRight);

	if (m_pName && GetView().GetZoom() >= 40)
	{
		const QRectF textGeom = m_pName->rect();
		pPainter->setBrush(Qt::NoBrush);
		pPainter->setFont(m_pName->font());

		if (!IsSelected())
		{
			pPainter->setPen(QColor(94, 94, 94));
			pPainter->drawText(textGeom, alignment, m_pName->text());

			if (IsHighlighted())
			{
				static QPainter::CompositionMode mode = QPainter::CompositionMode_Plus;
				pPainter->setCompositionMode(mode);

				pPainter->setPen(m_pStyle->GetHighlightColor());
				pPainter->drawText(textGeom, alignment, m_pName->text());
			}
		}
		else if (IsSelected())
		{
			pPainter->setPen(m_pStyle->GetSelectionColor());
			pPainter->drawText(textGeom, alignment, m_pName->text());
		}
	}
	pPainter->restore();
}

void CPinWidget::updateGeometry()
{
	QGraphicsWidget::updateGeometry();

	if (IsHighlighted())
	{
		m_pActiveIcon->setVisible(false);
		m_pActiveIcon = m_pIcons[StyleState_Highlighted];
		m_pActiveIcon->setVisible(true);
	}
	else if (IsSelected() || m_nodeWidget.IsSelected())
	{
		m_pActiveIcon->setVisible(false);
		m_pActiveIcon = m_pIcons[StyleState_Selected];
		m_pActiveIcon->setVisible(true);
	}
	else
	{
		m_pActiveIcon->setVisible(false);
		m_pActiveIcon = m_pIcons[StyleState_Default];
		m_pActiveIcon->setVisible(true);
	}

	UpdateConnectionPoint();
}

void CPinWidget::OnIconHoverEnterEvent(CGraphicsSceneIcon& sender, QGraphicsSceneHoverEvent* pEvent)
{
	SMouseInputEventArgs mouseArgs = SMouseInputEventArgs(
	  EMouseEventReason::HoverEnter,
	  Qt::MouseButton::NoButton, Qt::MouseButton::NoButton, pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	SPinMouseEventArgs args(mouseArgs);
	GetView().OnPinMouseEvent(this, args);
}

void CPinWidget::OnIconHoverMoveEvent(CGraphicsSceneIcon& sender, QGraphicsSceneHoverEvent* pEvent)
{
	SMouseInputEventArgs mouseArgs = SMouseInputEventArgs(
	  EMouseEventReason::HoverMove,
	  Qt::MouseButton::NoButton, Qt::MouseButton::NoButton, pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	SPinMouseEventArgs args(mouseArgs);
	GetView().OnPinMouseEvent(this, args);
}

void CPinWidget::OnIconHoverLeaveEvent(CGraphicsSceneIcon& sender, QGraphicsSceneHoverEvent* pEvent)
{
	SMouseInputEventArgs mouseArgs = SMouseInputEventArgs(
	  EMouseEventReason::HoverLeave,
	  Qt::MouseButton::NoButton, Qt::MouseButton::NoButton, pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	SPinMouseEventArgs args(mouseArgs);
	GetView().OnPinMouseEvent(this, args);
}

void CPinWidget::OnIconMousePressEvent(CGraphicsSceneIcon& sender, QGraphicsSceneMouseEvent* pEvent)
{
	SMouseInputEventArgs mouseArgs = SMouseInputEventArgs(
	  EMouseEventReason::ButtonPress,
	  pEvent->button(), pEvent->buttons(), pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	SPinMouseEventArgs args(mouseArgs);
	GetView().OnPinMouseEvent(this, args);
}

void CPinWidget::OnIconMouseReleaseEvent(CGraphicsSceneIcon& sender, QGraphicsSceneMouseEvent* pEvent)
{
	SMouseInputEventArgs mouseArgs = SMouseInputEventArgs(
	  EMouseEventReason::ButtonRelease,
	  pEvent->button(), pEvent->buttons(), pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	SPinMouseEventArgs args(mouseArgs);
	GetView().OnPinMouseEvent(this, args);
}

void CPinWidget::OnNodeSelectionChanged(bool isSelected)
{
	if (isSelected)
	{
		m_pActiveIcon->setVisible(false);
		m_pActiveIcon = m_pIcons[StyleState_Selected];
		m_pActiveIcon->setVisible(true);
	}
	else
	{
		m_pActiveIcon->setVisible(false);
		m_pActiveIcon = m_pIcons[StyleState_Default];
		m_pActiveIcon->setVisible(true);

		update();
	}
}

void CPinWidget::OnInvalidated()
{
	m_pName->setText(m_item.GetName());

	GetNodeWidget().UpdateContentWidget();

	ReloadIcons();

	GetNodeWidget().UpdateContentWidget();
}

void CPinWidget::OnBeginCreateConnection(QWidget* pSender, SBeginCreateConnectionEventArgs& args)
{
	if (IsView(pSender) && m_isConnecting == false)
	{
		m_isConnecting = true;

		const bool canConnect = m_item.CanConnect(&args.sourcePin);
		if (!canConnect)
		{
			if (&GetItem() == &args.sourcePin)
			{
				m_pActiveIcon->setVisible(false);
				m_pActiveIcon = m_pIcons[StyleState_Highlighted];
				m_pActiveIcon->setVisible(true);

				// TODO: This shouldn't happen here
				SetHighlighted(true);
				// ~TODO

				m_pActiveIcon->update();
				update();
			}
			else
			{
				m_pActiveIcon->setVisible(false);
				m_pActiveIcon = m_pIcons[StyleState_Disabled];
				m_pActiveIcon->setVisible(true);

				// TODO: This shouldn't happen here
				SetHighlighted(false);
				// ~TODO

				m_pActiveIcon->update();
				update();
			}
		}
		else if (canConnect)
		{
			m_pActiveIcon->setVisible(false);
			m_pActiveIcon = m_pIcons[StyleState_Highlighted];
			m_pActiveIcon->setVisible(true);

			// TODO: This shouldn't happen here
			SetHighlighted(true);
			// ~TODO

			m_pActiveIcon->update();
			update();
		}
	}
}

void CPinWidget::OnEndCreateConnection(QWidget* pSender, SEndCreateConnectionEventArgs& args)
{
	if (IsView(pSender) && m_isConnecting)
	{
		m_isConnecting = false;

		m_pActiveIcon->setVisible(false);
		m_pActiveIcon = m_pIcons[StyleState_Default];
		m_pActiveIcon->setVisible(true);

		// TODO: This shouldn't happen here
		SetHighlighted(false);
		// ~TODO

		m_pActiveIcon->update();
		update();
	}
}

void CPinWidget::LoadIcons()
{
	for (int32 i = 0; i < StyleState_Count; ++i)
	{
		m_pIcons[i] = new CGraphicsSceneIcon(m_pStyle->GetIconPixmap(static_cast<EStyleState>(i)));
		GetView().scene()->addItem(m_pIcons[i]);
		m_pIcons[i]->setParentItem(this);
		m_pIcons[i]->setVisible(false);
		m_pIcons[i]->setAcceptHoverEvents(true);

		QObject::connect(m_pIcons[i], &CGraphicsSceneIcon::SignalHoverEnterEvent, this, &CPinWidget::OnIconHoverEnterEvent);
		QObject::connect(m_pIcons[i], &CGraphicsSceneIcon::SignalHoverMoveEvent, this, &CPinWidget::OnIconHoverMoveEvent);
		QObject::connect(m_pIcons[i], &CGraphicsSceneIcon::SignalHoverLeaveEvent, this, &CPinWidget::OnIconHoverLeaveEvent);

		QObject::connect(m_pIcons[i], &CGraphicsSceneIcon::SignalMousePressEvent, this, &CPinWidget::OnIconMousePressEvent);
		QObject::connect(m_pIcons[i], &CGraphicsSceneIcon::SignalMouseReleaseEvent, this, &CPinWidget::OnIconMouseReleaseEvent);
	}

	m_pActiveIcon = m_pIcons[StyleState_Default];
	m_pActiveIcon->setVisible(true);
}

void CPinWidget::ReloadIcons()
{
	for (int32 i = 0; i < StyleState_Count; ++i)
	{
		if (m_pIcons[i])
		{
			GetView().scene()->removeItem(m_pIcons[i]);
			delete m_pIcons[i];
			m_pIcons[i] = nullptr;
		}
	}

	LoadIcons();
}

void CPinWidget::UpdateConnectionPoint()
{
	if (m_pActiveIcon)
	{
		const float iconSize = m_pActiveIcon->boundingRect().height() / 2.f;
		const QPointF iconPos = m_pActiveIcon->scenePos() + m_pActiveIcon->offset();

		m_connectionPoint.SetPosition(QPointF(iconPos.x() + iconSize, iconPos.y() + iconSize));
	}
}

}

