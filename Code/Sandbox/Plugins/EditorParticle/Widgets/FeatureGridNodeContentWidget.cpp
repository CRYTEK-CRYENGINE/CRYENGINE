// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FeatureGridNodeContentWidget.h"
#include "ViewWidget.h"
#include "Models/ParticleGraphItems.h"

#include <CryParticleSystem/IParticlesPfx2.h>

#include <NodeGraph/NodeGraphView.h>
#include <NodeGraph/AbstractNodeGraphViewModelItem.h>
#include <NodeGraph/NodeWidget.h>
#include <NodeGraph/PinWidget.h>
#include <NodeGraph/NodeGraphViewStyle.h>

#include <QGraphicsLinearLayout>
#include <QString>
#include <QCheckBox>
#include <QGraphicsItemAnimation>
#include <QTimeLine>
#include <QGraphicsSceneHoverEvent>

namespace CryParticleEditor {

CFeatureWidget::CFeatureWidget(CFeatureItem& feature, CFeatureGridNodeContentWidget& contentWidget, CryGraphEditor::CNodeGraphView& view)
	: CNodeGraphViewGraphicsWidget(view)
	, m_feature(feature)
	, m_contentWidget(contentWidget)
{
	setAcceptHoverEvents(true);
	setContentsMargins(0, 0, 0, 0);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum, QSizePolicy::DefaultType);

	m_color = m_feature.GetColor();

	// TODO: Make stylable.
	const QMargins& cm = QMargins(5, 5, 5, 5);
	m_pLayout = new QGraphicsLinearLayout();
	m_pLayout->setContentsMargins(cm.left(), cm.top(), cm.right(), cm.bottom());

	m_pEnabledCheckbox = new QCheckBox();
	m_pEnabledCheckbox->setChecked(!m_feature.IsDeactivated());
	m_pEnabledCheckbox->setFixedSize(QSize(m_pEnabledCheckbox->sizeHint().width(), 23));
	m_pEnabledCheckbox->setStyleSheet(QString("QCheckBox { background: rgba(0, 0, 0, 0) }"));
	m_pEnabledCheckbox->setAutoFillBackground(false);
	if (feature.GetNodeItem().IsDeactivated())
		m_pEnabledCheckbox->setEnabled(false);

	QObject::connect(m_pEnabledCheckbox, &QCheckBox::stateChanged, [&feature](bool isChecked)
		{
			feature.SetDeactivated(!isChecked);
	  });

	QGraphicsWidget* pIsEnabledWidget = CryGraphEditor::AddWidget(view.scene(), m_pEnabledCheckbox);
	m_pLayout->addItem(pIsEnabledWidget);
	m_pLayout->setAlignment(pIsEnabledWidget, Qt::AlignVCenter);

	QString title = feature.GetGroupName() + ": " + feature.GetName();
	m_pTitle = new QLabel(title);
	//m_pTitle->setFont(CryGraphEditor::CNodeStyle::GetContentTextFont());
	m_pTitle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	QGraphicsWidget* pTitleWidget = CryGraphEditor::AddWidget(view.scene(), m_pTitle);
	m_pLayout->addItem(pTitleWidget);
	m_pLayout->setAlignment(pTitleWidget, Qt::AlignVCenter);
	//
	if (CFeaturePinItem* pPinItem = m_feature.GetPinItem())
	{
		m_pPin = new CryGraphEditor::CPinWidget(*pPinItem, contentWidget.GetNode(), view, false);
		m_pPin->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum, QSizePolicy::DefaultType);
		m_pPin->setMaximumHeight(m_pPin->size().height());
		m_pLayout->addItem(m_pPin);
		m_pLayout->setAlignment(m_pPin, Qt::AlignVCenter);
	}
	else
	{
		m_pPin = nullptr;
	}

	m_color = m_feature.GetColor();

	const bool isDeactivated = m_feature.IsDeactivated();
	SetDeactivated(isDeactivated);
	m_pEnabledCheckbox->setChecked(!isDeactivated);

	setLayout(m_pLayout);

	UpdateStyle();

	m_feature.SignalItemDeactivatedChanged.Connect(this, &CFeatureWidget::OnDeactivatedChanged);
	m_feature.SignalInvalidated.Connect(this, &CFeatureWidget::OnItemInvalidated);

	SignalSelectionChanged.Connect(this, &CFeatureWidget::OnSelectionChanged);
	SignalHighlightedChanged.Connect(this, &CFeatureWidget::OnHighlightedChanged);
	SignalDeactivatedChanged.Connect(this, &CFeatureWidget::OnDeactivatedChanged);

	m_contentWidget.GetNode().GetItem().SignalDeactivatedChanged.Connect(this, &CFeatureWidget::OnNodeDeactivatedChanged);
}

CFeatureWidget::~CFeatureWidget()
{

}

void CFeatureWidget::DeleteLater()
{
	m_feature.SignalInvalidated.DisconnectObject(this);
	m_feature.SignalItemDeactivatedChanged.DisconnectObject(this);

	CryGraphEditor::CAbstractNodeItem& nodeItem = m_contentWidget.GetNode().GetItem();
	nodeItem.SignalDeactivatedChanged.DisconnectObject(this);

	CNodeGraphViewGraphicsWidget::DeleteLater();
}

CryGraphEditor::CAbstractNodeGraphViewModelItem* CFeatureWidget::GetAbstractItem() const
{
	return static_cast<CryGraphEditor::CAbstractNodeGraphViewModelItem*>(&m_feature);
}

void CFeatureWidget::OnItemInvalidated()
{
	const bool isDeactivated = m_feature.IsDeactivated();
	m_pEnabledCheckbox->setChecked(!isDeactivated);

	CNodeGraphViewGraphicsWidget::OnItemInvalidated();
}

void CFeatureWidget::OnSelectionChanged(bool isSelected)
{
	UpdateStyle();
}

void CFeatureWidget::OnHighlightedChanged(bool isHighlighted)
{
	UpdateStyle();
}

void CFeatureWidget::OnDeactivatedChanged(bool isDeactiaved)
{
	m_pEnabledCheckbox->setChecked(!isDeactiaved);

	UpdateStyle();
}

void CFeatureWidget::paint(QPainter* pPainter, const QStyleOptionGraphicsItem* pOption, QWidget* pWidget)
{
	const bool wasAntialisingSet = pPainter->renderHints().testFlag(QPainter::Antialiasing);
	pPainter->setRenderHints(QPainter::Antialiasing, true);

	// TODO: Make stylable.
	const QRectF& rect = boundingRect();
	const qreal radius = 4.0;

	pPainter->setPen(Qt::NoPen);
	if (IsSelected())
		pPainter->setBrush(GetView().GetStyle().GetSelectionColor());
	else if (IsHighlighted())
		pPainter->setBrush(GetView().GetStyle().GetHighlightColor());
	else if (IsDeactivated())
		pPainter->setBrush(QColor(58, 58, 58));
	else
		pPainter->setBrush(QColor(78, 78, 78));

	pPainter->drawRoundedRect(rect, radius, radius);

	if (!wasAntialisingSet)
		pPainter->setRenderHints(QPainter::Antialiasing, false);
}

void CFeatureWidget::hoverEnterEvent(QGraphicsSceneHoverEvent* pEvent)
{
	CryGraphEditor::SMouseInputEventArgs mouseArgs = CryGraphEditor::SMouseInputEventArgs(
	  CryGraphEditor::EMouseEventReason::HoverEnter,
	  Qt::MouseButton::NoButton, Qt::MouseButton::NoButton, pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	SFeatureMouseEventArgs args(mouseArgs);
	static_cast<CGraphView&>(GetView()).OnFeatureMouseEvent(this, args);
}

void CFeatureWidget::hoverMoveEvent(QGraphicsSceneHoverEvent* pEvent)
{
	CryGraphEditor::SMouseInputEventArgs mouseArgs = CryGraphEditor::SMouseInputEventArgs(
	  CryGraphEditor::EMouseEventReason::HoverMove,
	  Qt::MouseButton::NoButton, Qt::MouseButton::NoButton, pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	SFeatureMouseEventArgs args(mouseArgs);
	static_cast<CGraphView&>(GetView()).OnFeatureMouseEvent(this, args);
}

void CFeatureWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent* pEvent)
{
	CryGraphEditor::SMouseInputEventArgs mouseArgs = CryGraphEditor::SMouseInputEventArgs(
	  CryGraphEditor::EMouseEventReason::HoverLeave,
	  Qt::MouseButton::NoButton, Qt::MouseButton::NoButton, pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	SFeatureMouseEventArgs args(mouseArgs);
	static_cast<CGraphView&>(GetView()).OnFeatureMouseEvent(this, args);
}

void CFeatureWidget::mousePressEvent(QGraphicsSceneMouseEvent* pEvent)
{
	CryGraphEditor::SMouseInputEventArgs mouseArgs = CryGraphEditor::SMouseInputEventArgs(
	  CryGraphEditor::EMouseEventReason::ButtonPress,
	  pEvent->button(), pEvent->buttons(), pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	SFeatureMouseEventArgs args(mouseArgs);
	static_cast<CGraphView&>(GetView()).OnFeatureMouseEvent(this, args);
}

void CFeatureWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent* pEvent)
{
	CryGraphEditor::SMouseInputEventArgs mouseArgs = CryGraphEditor::SMouseInputEventArgs(
	  CryGraphEditor::EMouseEventReason::ButtonRelease,
	  pEvent->button(), pEvent->buttons(), pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	SFeatureMouseEventArgs args(mouseArgs);
	static_cast<CGraphView&>(GetView()).OnFeatureMouseEvent(this, args);
}

void CFeatureWidget::mouseMoveEvent(QGraphicsSceneMouseEvent* pEvent)
{
	CryGraphEditor::SMouseInputEventArgs mouseArgs = CryGraphEditor::SMouseInputEventArgs(
	  CryGraphEditor::EMouseEventReason::ButtonPressAndMove,
	  pEvent->button(), pEvent->buttons(), pEvent->modifiers(),
	  pEvent->pos(), pEvent->scenePos(), pEvent->screenPos(),
	  pEvent->lastPos(), pEvent->lastScenePos(), pEvent->lastScreenPos());

	SFeatureMouseEventArgs args(mouseArgs);
	static_cast<CGraphView&>(GetView()).OnFeatureMouseEvent(this, args);
}

void CFeatureWidget::moveEvent(QGraphicsSceneMoveEvent* pEvent)
{
	if (m_pPin)
	{
		m_pPin->UpdateConnectionPoint();
	}
}

CryGraphEditor::CNodeWidget& CFeatureWidget::GetNodeWidget() const
{
	return m_contentWidget.GetNode();
}

void CFeatureWidget::UpdateStyle()
{
	if (IsSelected())
	{
		m_pTitle->setStyleSheet(QString("QLabel { font: bold; color: rgb(255, 255, 255); }"));
	}
	else if (m_feature.IsDeactivated() || GetNodeWidget().GetItem().IsDeactivated())
	{
		m_pTitle->setStyleSheet(QString("QLabel { font: bold; color: rgb(94, 94, 94); }"));
	}
	else
	{
		m_pTitle->setStyleSheet(QString("QLabel { font: bold; color: rgb(%1, %2, %3); }").arg(m_color.red()).arg(m_color.green()).arg(m_color.blue()));
	}
}

void CFeatureWidget::OnNodeDeactivatedChanged(bool isDeactivated)
{
	UpdateStyle();

	m_pEnabledCheckbox->setDisabled(isDeactivated);
}

CFeatureSlotWidget::CFeatureSlotWidget()
	: m_index(~0)
	, m_pAnimation(nullptr)
{

}

CFeatureSlotWidget::CFeatureSlotWidget(const CFeatureSlotWidget& other)
{
	m_pFeatureWidget = other.m_pFeatureWidget;

	m_geometry = other.m_geometry;
	m_boundingRect = other.m_boundingRect;
	m_index = other.m_index;
}

CFeatureSlotWidget::~CFeatureSlotWidget()
{

}

void CFeatureSlotWidget::InitFromFeature(CFeatureWidget& featureWidget)
{
	m_pFeatureWidget = &featureWidget;

	m_geometry = featureWidget.geometry();
	m_boundingRect = featureWidget.boundingRect();
	m_index = featureWidget.GetItem().GetIndex();
}

void CFeatureSlotWidget::MoveIntoSlot(CFeatureWidget* pFeatureWidget)
{
	if (m_pFeatureWidget != pFeatureWidget)
	{
		m_pFeatureWidget = pFeatureWidget;
		if (m_pAnimation)
		{
			m_timeLine.stop();
			m_pAnimation->deleteLater();
			m_pAnimation = nullptr;
		}

		if (m_pFeatureWidget)
		{
			m_timeLine.setDuration(100);
			m_timeLine.setUpdateInterval(15);
			m_pAnimation = new QGraphicsItemAnimation();
			m_pAnimation->setItem(m_pFeatureWidget);
			m_pAnimation->setTimeLine(&m_timeLine);
			m_pAnimation->setPosAt(1.0, pos());
			m_timeLine.start();
		}
	}
}

QRectF CFeatureSlotWidget::boundingRect() const
{
	return m_boundingRect;
}

void CFeatureSlotWidget::updateGeometry()
{
	setGeometry(m_geometry);
}

QSizeF CFeatureSlotWidget::sizeHint(Qt::SizeHint which, const QSizeF& constraint) const
{
	return m_geometry.size();
}

CFeatureGridNodeContentWidget::CFeatureGridNodeContentWidget(CryGraphEditor::CNodeWidget& node, CParentPinItem& parentPin, CChildPinItem& childPin, CryGraphEditor::CNodeGraphView& view)
	: CAbstractNodeContentWidget(node, view)
	, m_pGrabbedFeature(nullptr)
	, m_grabbedFeatureIndex(~0)
{
	QGraphicsLinearLayout* pLayout = new QGraphicsLinearLayout();
	pLayout->setOrientation(Qt::Orientation::Vertical);
	pLayout->setContentsMargins(0, 0, 0, 0);
	//pLayout->setSpacing(CFeatureGridStyle::GetParentSpacing());
	pLayout->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

	m_pParentPin = new CryGraphEditor::CPinWidget(parentPin, node, view);
	m_pins.push_back(m_pParentPin);
	pLayout->addItem(m_pParentPin);
	pLayout->setAlignment(m_pParentPin, Qt::AlignVCenter);

	m_pChildPin = new CryGraphEditor::CPinWidget(childPin, node, view);
	m_pins.push_back(m_pChildPin);
	pLayout->addItem(m_pChildPin);
	pLayout->setAlignment(m_pChildPin, Qt::AlignVCenter);

	m_pFeaturesLayout = new QGraphicsLinearLayout();
	m_pFeaturesLayout->setOrientation(Qt::Orientation::Vertical);
	m_pFeaturesLayout->setContentsMargins(0, 0, 0, 0);
	//m_pFeaturesLayout->setSpacing(CFeatureGridStyle::GetFeatureSpacing());
	pLayout->addItem(m_pFeaturesLayout);
	pLayout->setAlignment(m_pFeaturesLayout, Qt::AlignVCenter);

	CRY_ASSERT(node.GetItem().Cast<CNodeItem>());
	CNodeItem& nodeItem = *node.GetItem().Cast<CNodeItem>();
	for (CFeatureItem* pFeatureItem : nodeItem.GetFeatureItems())
	{
		OnFeatureAdded(*pFeatureItem);
	}

	setLayout(pLayout);

	nodeItem.SignalFeatureAdded.Connect(this, &CFeatureGridNodeContentWidget::OnFeatureAdded);
	nodeItem.SignalFeatureRemoved.Connect(this, &CFeatureGridNodeContentWidget::OnFeatureRemoved);
	nodeItem.SignalFeatureMoved.Connect(this, &CFeatureGridNodeContentWidget::OnFeatureMoved);
}

CFeatureGridNodeContentWidget::~CFeatureGridNodeContentWidget()
{

}

void CFeatureGridNodeContentWidget::BeginFeatureMove(CFeatureWidget& featureWidget)
{
	m_pGrabbedFeature = &featureWidget;
	m_grabbedFeatureIndex = featureWidget.GetItem().GetIndex();
	m_pGrabbedFeature->setZValue(2.0);

	m_featureSlots.resize(m_featureWidgetsByItems.size());
	m_floatingFeatures.resize(m_featureWidgetsByItems.size());

	for (auto& pair : m_featureWidgetsByItems)
	{
		CFeatureItem* pFeatureItem = pair.first;
		CFeatureWidget* pFeatureWidget = pair.second;

		const uint32 index = pFeatureItem->GetIndex();
		m_featureSlots[index].InitFromFeature(*pFeatureWidget);
		m_floatingFeatures[index] = pFeatureWidget;

		CFeatureSlotWidget* pFeatureSlotWidget = &m_featureSlots[index];

		const QPointF scenePos = pFeatureWidget->pos();
		m_pFeaturesLayout->removeItem(pFeatureWidget);
		m_pFeaturesLayout->insertItem(index, pFeatureSlotWidget);
		m_pFeaturesLayout->setAlignment(pFeatureSlotWidget, Qt::AlignVCenter);

		pFeatureWidget->setPos(scenePos);
	}

	m_featureSlots[m_grabbedFeatureIndex].MoveIntoSlot(m_pGrabbedFeature);
}

void CFeatureGridNodeContentWidget::MoveFeature(const QPointF& delta)
{
	if (delta.y() != 0.0)
	{
		QRectF featureGeom = m_pGrabbedFeature->geometry();
		const qreal h = featureGeom.height() / 2;
		featureGeom.adjust(0, h / 2, 0, -h / 2);

		const uint32 orgFeatureIndex = m_pGrabbedFeature->GetItem().GetIndex();

		const uint32 numFeatures = m_floatingFeatures.size();
		for (uint32 i = 0; i < numFeatures; ++i)
		{
			CFeatureSlotWidget& targetSlot = m_featureSlots[i];
			const QRectF geom = targetSlot.geometry();
			if (targetSlot.geometry().intersects(featureGeom))
			{
				if (targetSlot.GetIndex() != m_grabbedFeatureIndex)
				{
					const uint32 targetIndex = targetSlot.GetIndex();
					const uint32 numSlots = m_featureSlots.size();
					for (uint32 s = 0, f = 0; s < numSlots; ++s)
					{
						if (s != targetIndex)
						{
							if (f == orgFeatureIndex)
								++f;

							m_featureSlots[s].MoveIntoSlot(m_floatingFeatures[f++]);
						}
					}

					m_grabbedFeatureIndex = targetSlot.GetIndex();
					targetSlot.SetFeature(m_pGrabbedFeature);
				}

				break;
			}
		}

		const QPointF maxPos = m_featureSlots.front().pos();
		const QPointF minPos = m_featureSlots.back().pos();

		const QPointF featurePos = m_pGrabbedFeature->pos();

		const qreal y = featurePos.y() + delta.y();
		const qreal targetY = std::min(minPos.y(), std::max(y, maxPos.y()));

		m_pGrabbedFeature->setPos(featurePos.x(), targetY);
	}
}

uint32 CFeatureGridNodeContentWidget::EndFeatureMove()
{
	for (CFeatureWidget* pFeatureWidget : m_floatingFeatures)
	{
		const uint32 index = pFeatureWidget->GetItem().GetIndex();

		CFeatureSlotWidget* pFeatureSlotWidget = &m_featureSlots[index];
		m_pFeaturesLayout->removeItem(pFeatureSlotWidget);
		m_pFeaturesLayout->insertItem(index, pFeatureWidget);
		m_pFeaturesLayout->setAlignment(pFeatureWidget, Qt::AlignVCenter);
	}

	const uint32 destIndex = m_grabbedFeatureIndex;
	const uint32 itemIndex = m_pGrabbedFeature->GetItem().GetIndex();
	m_pGrabbedFeature->ResetZValue();
	m_pGrabbedFeature = nullptr;

	m_featureSlots.clear();

	return destIndex != itemIndex ? destIndex : ~0;
}

void CFeatureGridNodeContentWidget::OnItemInvalidated()
{
	m_pParentPin->OnItemInvalidated();
	m_pChildPin->OnItemInvalidated();
	for (auto& pair : m_featureWidgetsByItems)
	{
		CFeatureWidget* pFeatureWidget = pair.second;
		pFeatureWidget->OnItemInvalidated();
	}
}

void CFeatureGridNodeContentWidget::OnFeatureAdded(CFeatureItem& feature)
{
	CFeatureWidget* pFeatureWidget = new CFeatureWidget(feature, *this, GetView());
	m_pFeaturesLayout->insertItem(feature.GetIndex(), pFeatureWidget);
	m_pFeaturesLayout->setAlignment(pFeatureWidget, Qt::AlignVCenter);
	m_featureWidgetsByItems.emplace(&feature, pFeatureWidget);

	if (CryGraphEditor::CPinWidget* pPinWidget = pFeatureWidget->GetPinWidget())
		m_pins.push_back(pPinWidget);

	UpdateLayout(m_pFeaturesLayout);
}

void CFeatureGridNodeContentWidget::OnFeatureRemoved(CFeatureItem& feature)
{
	auto result = m_featureWidgetsByItems.find(&feature);
	if (result != m_featureWidgetsByItems.end())
	{
		CFeatureWidget* pFeatureWidget = result->second;

		m_featureWidgetsByItems.erase(result);
		m_pFeaturesLayout->removeItem(pFeatureWidget);
		pFeatureWidget->DeleteLater();

		auto result = std::find(m_pins.begin(), m_pins.end(), pFeatureWidget->GetPinWidget());
		if (result != m_pins.end())
		{
			m_pins.erase(result);
		}

		UpdateLayout(m_pFeaturesLayout);
	}
}

void CFeatureGridNodeContentWidget::OnFeatureMoved(CFeatureItem& feature)
{
	auto result = m_featureWidgetsByItems.find(&feature);
	if (result != m_featureWidgetsByItems.end())
	{
		CFeatureWidget* pFeatureWidget = result->second;
		const uint32 destIndex = feature.GetIndex();

		m_pFeaturesLayout->removeItem(pFeatureWidget);
		m_pFeaturesLayout->insertItem(destIndex, result->second);

		UpdateLayout(m_pFeaturesLayout);
	}
}

}

