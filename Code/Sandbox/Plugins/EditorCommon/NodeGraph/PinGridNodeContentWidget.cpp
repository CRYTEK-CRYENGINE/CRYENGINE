// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "PinGridNodeContentWidget.h"

#include "NodeGraphView.h"
#include "AbstractNodeGraphViewModelItem.h"
#include "NodeWidget.h"
#include "PinWidget.h"

#include <QGraphicsGridLayout>

namespace CryGraphEditor {

CPinGridNodeContentWidget::CPinGridNodeContentWidget(CNodeWidget& node, CNodeGraphView& view)
	: CAbstractNodeContentWidget(node, view)
	, m_numLeftPins(0)
	, m_numRightPins(0)
	, m_pLastEnteredPin(nullptr)
{
	node.SetContentWidget(this);

	m_pLayout = new QGraphicsGridLayout(this);
	m_pLayout->setColumnAlignment(0, Qt::AlignLeft);
	m_pLayout->setColumnAlignment(1, Qt::AlignRight);
	m_pLayout->setVerticalSpacing(2.f);
	m_pLayout->setHorizontalSpacing(35.f);
	m_pLayout->setContentsMargins(5.0f, 5.0f, 5.0f, 5.0f);

	CAbstractNodeItem& nodeItem = node.GetItem();
	nodeItem.SignalPinAdded.Connect(this, &CPinGridNodeContentWidget::OnPinAdded);
	nodeItem.SignalPinRemoved.Connect(this, &CPinGridNodeContentWidget::OnPinRemoved);
	nodeItem.SignalInvalidated.Connect(this, &CPinGridNodeContentWidget::OnItemInvalidated);

	for (CAbstractPinItem* pPinItem : node.GetItem().GetPinItems())
	{
		CPinWidget* pPinWidget = pPinItem->CreateWidget(node, GetView());
		AddPin(*pPinWidget);
	}

	setLayout(m_pLayout);
}

CPinGridNodeContentWidget::~CPinGridNodeContentWidget()
{

}

void CPinGridNodeContentWidget::OnInputEvent(CNodeWidget* pSender, SMouseInputEventArgs& args)
{
	// TODO: In here we should map all positions we forward to the pins.

	const EMouseEventReason orgReason = args.GetReason();

	CPinWidget* pHitPinWidget = nullptr;
	if (orgReason != EMouseEventReason::HoverLeave)
	{
		for (CPinWidget* pPinWidget : m_pins)
		{
			const QPointF localPoint = pPinWidget->mapFromScene(args.GetScenePos());
			const bool containsPoint = pPinWidget->GetRect().contains(localPoint);
			if (containsPoint)
			{
				pHitPinWidget = pPinWidget;
				break;
			}
		}
	}

	if (m_pLastEnteredPin != nullptr && (m_pLastEnteredPin != pHitPinWidget || pHitPinWidget == nullptr))
	{
		const EMouseEventReason originalReason = args.GetReason();
		if (originalReason == EMouseEventReason::HoverLeave || originalReason == EMouseEventReason::HoverMove)
		{
			// TODO: Should we send the events through the actual pin item instead?
			SMouseInputEventArgs mouseArgs = SMouseInputEventArgs(
			  EMouseEventReason::HoverLeave,
			  Qt::MouseButton::NoButton, Qt::MouseButton::NoButton, args.GetModifiers(),
			  args.GetLocalPos(), args.GetScenePos(), args.GetScreenPos(),
			  args.GetLastLocalPos(), args.GetLastScenePos(), args.GetLastScreenPos());

			GetView().OnPinMouseEvent(m_pLastEnteredPin, SPinMouseEventArgs(mouseArgs));
			// ~TODO

			m_pLastEnteredPin = nullptr;

			args.SetAccepted(true);
			return;
		}
	}

	if (pHitPinWidget)
	{
		EMouseEventReason reason = orgReason;
		if (m_pLastEnteredPin == nullptr)
		{
			m_pLastEnteredPin = pHitPinWidget;
			if (reason == EMouseEventReason::HoverMove)
			{
				reason = EMouseEventReason::HoverEnter;
			}
		}

		// TODO: Should we send the events through the actual pin item instead?
		SMouseInputEventArgs mouseArgs = SMouseInputEventArgs(
		  reason,
		  args.GetButton(), args.GetButtons(), args.GetModifiers(),
		  args.GetLocalPos(), args.GetScenePos(), args.GetScreenPos(),
		  args.GetLastLocalPos(), args.GetLastScenePos(), args.GetLastScreenPos());

		GetView().OnPinMouseEvent(pHitPinWidget, SPinMouseEventArgs(mouseArgs));
		// ~TODO

		args.SetAccepted(true);
		return;
	}

	args.SetAccepted(false);
	return;
}

void CPinGridNodeContentWidget::DeleteLater()
{
	CAbstractNodeItem& nodeItem = GetNode().GetItem();
	nodeItem.SignalPinAdded.DisconnectObject(this);
	nodeItem.SignalPinRemoved.DisconnectObject(this);

	for (CPinWidget* pPinWidget : m_pins)
		pPinWidget->DeleteLater();

	CAbstractNodeContentWidget::DeleteLater();
}

void CPinGridNodeContentWidget::OnItemInvalidated()
{
	CryGraphEditor::PinItemArray pinItems;
	pinItems.reserve(pinItems.size());

	size_t numKeptPins = 0;

	PinWidgetArray pinWidgets;
	pinWidgets.swap(m_pins);
	for (CPinWidget* pPinWidget : pinWidgets)
	{
		m_pLayout->removeItem(pPinWidget);
	}

	m_numLeftPins = m_numRightPins = 0;
	for (CAbstractPinItem* pPinItem : GetNode().GetItem().GetPinItems())
	{
		QVariant pinId = pPinItem->GetId();
		auto predicate = [pinId](CPinWidget* pPinWidget) -> bool
		{
			return (pPinWidget && pPinWidget->GetItem().HasId(pinId));
		};

		auto result = std::find_if(pinWidgets.begin(), pinWidgets.end(), predicate);
		if (result == m_pins.end())
		{
			CPinWidget* pPinWidget = new CPinWidget(*pPinItem, GetNode(), GetView(), true);
			AddPin(*pPinWidget);
		}
		else
		{
			CPinWidget* pPinWidget = *result;
			AddPin(*pPinWidget);

			*result = nullptr;
			++numKeptPins;
		}
	}

	for (CPinWidget* pPinWidget : pinWidgets)
	{
		if (pPinWidget != nullptr)
		{
			RemovePin(*pPinWidget);
			pPinWidget->GetItem().SignalInvalidated.DisconnectObject(this);
			pPinWidget->DeleteLater();
		}
	}

	UpdateLayout(m_pLayout);
}

void CPinGridNodeContentWidget::OnLayoutChanged()
{
	UpdateLayout(m_pLayout);
}

void CPinGridNodeContentWidget::AddPin(CPinWidget& pinWidget)
{
	CAbstractPinItem& pinItem = pinWidget.GetItem();
	if (pinItem.IsInputPin())
	{
		m_pLayout->addItem(&pinWidget, m_numLeftPins++, 0);
	}
	else
	{
		m_pLayout->addItem(&pinWidget, m_numRightPins++, 1);
	}

	m_pins.push_back(&pinWidget);
	UpdateLayout(m_pLayout);
}

void CPinGridNodeContentWidget::RemovePin(CPinWidget& pinWidget)
{
	CAbstractPinItem& pinItem = pinWidget.GetItem();
	if (pinItem.IsInputPin())
		--m_numLeftPins;
	else
		--m_numRightPins;

	m_pLayout->removeItem(&pinWidget);
	pinWidget.DeleteLater();

	auto result = std::find(m_pins.begin(), m_pins.end(), &pinWidget);
	CRY_ASSERT(result != m_pins.end());
	if (result != m_pins.end())
		m_pins.erase(result);
}

void CPinGridNodeContentWidget::OnPinAdded(CAbstractPinItem& item)
{
	CPinWidget* pPinWidget = item.CreateWidget(GetNode(), GetView());
	AddPin(*pPinWidget);

	UpdateLayout(m_pLayout);
}

void CPinGridNodeContentWidget::OnPinRemoved(CAbstractPinItem& item)
{
	QVariant pinId = item.GetId();

	auto condition = [pinId](CPinWidget* pPinWidget) -> bool
	{
		return (pPinWidget && pPinWidget->GetItem().HasId(pinId));
	};

	const auto result = std::find_if(m_pins.begin(), m_pins.end(), condition);
	if (result != m_pins.end())
	{
		CPinWidget* pPinWidget = *result;
		m_pLayout->removeItem(pPinWidget);
		pPinWidget->DeleteLater();

		if (item.IsInputPin())
			--m_numLeftPins;
		else
			--m_numRightPins;

		m_pins.erase(result);
	}

	UpdateLayout(m_pLayout);
}

}

