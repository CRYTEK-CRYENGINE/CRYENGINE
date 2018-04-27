// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SubstanceNodeContentWidget.h"

#include <NodeGraph\NodeGraphView.h>
#include <NodeGraph\AbstractNodeGraphViewModelItem.h>
#include <NodeGraph\NodeWidget.h>

#include <QGraphicsGridLayout>
#include <QComboBox>

#include "SubstanceCommon.h"

namespace EditorSubstance
{
	namespace OutputEditor
	{
		class OutputPreviewImage : public QGraphicsLayoutItem, public QGraphicsPixmapItem
		{
		public:
			OutputPreviewImage(QGraphicsItem *parent = 0)
				: QGraphicsLayoutItem(), QGraphicsPixmapItem(parent)
			{

			}

			void setGeometry(const QRectF &geom) override
			{
				prepareGeometryChange();
				QGraphicsLayoutItem::setGeometry(geom);
				setPos(geom.topLeft());
			}

			QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override
			{
				return constraint;
			}

			void SetPreviewImage(const QPixmap& pixmap)
			{
				setPixmap(pixmap);
			}
		};

		CSubstanceNodeContentWidget::CSubstanceNodeContentWidget(CryGraphEditor::CNodeWidget& node, CryGraphEditor::CNodeGraphView& view, const CSubstanceNodeContentWidget::EOutputType outputType, SSubstanceOutput* output, bool showPreview)
			: CAbstractNodeContentWidget(node, view)
			, m_numLeftPins(0)
			, m_numRightPins(0)
			, m_pLastEnteredPin(nullptr)
			, m_pOutput(output)
			, m_outputType(outputType)
			, m_showPreview(showPreview)
		{
			node.SetContentWidget(this);

			m_pLayout = new QGraphicsGridLayout(this);
			m_pLayout->setColumnAlignment(0, Qt::AlignLeft);
			m_pLayout->setColumnAlignment(1, Qt::AlignRight);
			m_pLayout->setVerticalSpacing(2.f);
			m_pLayout->setHorizontalSpacing(15.f);
			m_pLayout->setContentsMargins(5.0f, 5.0f, 5.0f, 5.0f);
			if (m_outputType == Virtual)
			{
				m_pLayout->setColumnMaximumWidth(0, 15);

			}
			setLayout(m_pLayout);

			if (m_outputType == Virtual)
			{
				m_pPreset = new QLabel();
				m_pResolution = new QLabel();
				Update();
				QGraphicsProxyWidget* presetsProxy = new QGraphicsProxyWidget(this);
				QGraphicsProxyWidget* resolutionProxy = new QGraphicsProxyWidget(this);
				presetsProxy->setWidget(m_pPreset);
				resolutionProxy->setWidget(m_pResolution);
				m_pLayout->addItem(presetsProxy, 0, 0, 1, 2);
				m_pLayout->addItem(resolutionProxy, 1, 0, 1, 2);

			}
			UpdateLayout(m_pLayout);

			CryGraphEditor::CAbstractNodeItem& nodeItem = node.GetItem();
			nodeItem.SignalPinAdded.Connect(this, &CSubstanceNodeContentWidget::OnPinAdded);
			nodeItem.SignalPinRemoved.Connect(this, &CSubstanceNodeContentWidget::OnPinRemoved);
			nodeItem.SignalInvalidated.Connect(this, &CSubstanceNodeContentWidget::OnItemInvalidated);

			for (CryGraphEditor::CAbstractPinItem* pPinItem : node.GetItem().GetPinItems())
			{
				CryGraphEditor::CPinWidget* pPinWidget = pPinItem->CreateWidget(node, GetView());
				AddPin(*pPinWidget);
			}

			if (showPreview)
			{
				m_pPreviewImage = new OutputPreviewImage(this);
				m_pLayout->addItem(m_pPreviewImage, m_outputType == Virtual ? 2 : 0, m_outputType == Virtual ? 1 : 0, m_outputType != Virtual ? m_numRightPins : m_numLeftPins, 1, m_outputType == Virtual ? Qt::AlignLeft : Qt::AlignLeft);
			}


		}

		CSubstanceNodeContentWidget::~CSubstanceNodeContentWidget()
		{

		}

		void CSubstanceNodeContentWidget::OnInputEvent(CryGraphEditor::CNodeWidget* pSender, CryGraphEditor::SMouseInputEventArgs& args)
		{
			// TODO: In here we should map all positions we forward to the pins.

			const CryGraphEditor::EMouseEventReason orgReason = args.GetReason();

			CryGraphEditor::CPinWidget* pHitPinWidget = nullptr;
			if (orgReason != CryGraphEditor::EMouseEventReason::HoverLeave)
			{
				for (CryGraphEditor::CPinWidget* pPinWidget : m_pins)
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
				const CryGraphEditor::EMouseEventReason originalReason = args.GetReason();
				if (originalReason == CryGraphEditor::EMouseEventReason::HoverLeave || originalReason == CryGraphEditor::EMouseEventReason::HoverMove)
				{
					// TODO: Should we send the events through the actual pin item instead?
					CryGraphEditor::SMouseInputEventArgs mouseArgs = CryGraphEditor::SMouseInputEventArgs(
						CryGraphEditor::EMouseEventReason::HoverLeave,
						Qt::MouseButton::NoButton, Qt::MouseButton::NoButton, args.GetModifiers(),
						args.GetLocalPos(), args.GetScenePos(), args.GetScreenPos(),
						args.GetLastLocalPos(), args.GetLastScenePos(), args.GetLastScreenPos());

					GetView().OnPinMouseEvent(m_pLastEnteredPin, CryGraphEditor::SPinMouseEventArgs(mouseArgs));
					// ~TODO

					m_pLastEnteredPin = nullptr;

					args.SetAccepted(true);
					return;
				}
			}

			if (pHitPinWidget)
			{
				CryGraphEditor::EMouseEventReason reason = orgReason;
				if (m_pLastEnteredPin == nullptr)
				{
					m_pLastEnteredPin = pHitPinWidget;
					if (reason == CryGraphEditor::EMouseEventReason::HoverMove)
					{
						reason = CryGraphEditor::EMouseEventReason::HoverEnter;
					}
				}

				// TODO: Should we send the events through the actual pin item instead?
				CryGraphEditor::SMouseInputEventArgs mouseArgs = CryGraphEditor::SMouseInputEventArgs(
					reason,
					args.GetButton(), args.GetButtons(), args.GetModifiers(),
					args.GetLocalPos(), args.GetScenePos(), args.GetScreenPos(),
					args.GetLastLocalPos(), args.GetLastScenePos(), args.GetLastScreenPos());

				GetView().OnPinMouseEvent(pHitPinWidget, CryGraphEditor::SPinMouseEventArgs(mouseArgs));
				// ~TODO

				args.SetAccepted(true);
				return;
			}

			args.SetAccepted(false);
			return;
		}

		void CSubstanceNodeContentWidget::DeleteLater()
		{
			CryGraphEditor::CAbstractNodeItem& nodeItem = GetNode().GetItem();
			nodeItem.SignalPinAdded.DisconnectObject(this);
			nodeItem.SignalPinRemoved.DisconnectObject(this);

			for (CryGraphEditor::CPinWidget* pPinWidget : m_pins)
				pPinWidget->DeleteLater();

			CAbstractNodeContentWidget::DeleteLater();
		}

		void CSubstanceNodeContentWidget::OnItemInvalidated()
		{
			CryGraphEditor::PinItemArray pinItems;
			pinItems.reserve(pinItems.size());

			size_t numKeptPins = 0;

			CryGraphEditor::PinWidgetArray pinWidgets;
			pinWidgets.swap(m_pins);
			for (CryGraphEditor::CPinWidget* pPinWidget : pinWidgets)
			{
				m_pLayout->removeItem(pPinWidget);
			}

			m_numLeftPins = m_numRightPins = 0;
			for (CryGraphEditor::CAbstractPinItem* pPinItem : GetNode().GetItem().GetPinItems())
			{
				QVariant pinId = pPinItem->GetId();
				auto predicate = [pinId](CryGraphEditor::CPinWidget* pPinWidget) -> bool
				{
					return (pPinWidget && pPinWidget->GetItem().HasId(pinId));
				};

				auto result = std::find_if(pinWidgets.begin(), pinWidgets.end(), predicate);
				if (result == m_pins.end())
				{
					CryGraphEditor::CPinWidget* pPinWidget = new CryGraphEditor::CPinWidget(*pPinItem, GetNode(), GetView(), true);
					AddPin(*pPinWidget);
				}
				else
				{
					CryGraphEditor::CPinWidget* pPinWidget = *result;
					AddPin(*pPinWidget);

					*result = nullptr;
					++numKeptPins;
				}
			}

			for (CryGraphEditor::CPinWidget* pPinWidget : pinWidgets)
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

		void CSubstanceNodeContentWidget::OnLayoutChanged()
		{
			UpdateLayout(m_pLayout);
		}

		void CSubstanceNodeContentWidget::AddPin(CryGraphEditor::CPinWidget& pinWidget)
		{
			int addOffset = 0;
			if (m_outputType == Virtual)
			{
				addOffset = 2;
			}
			CryGraphEditor::CAbstractPinItem& pinItem = pinWidget.GetItem();
			if (pinItem.IsInputPin())
			{
				m_pLayout->addItem(&pinWidget, addOffset + m_numLeftPins++, 0);
			}
			else
			{
				m_pLayout->addItem(&pinWidget, addOffset + m_numRightPins++, 1);
			}

			m_pins.push_back(&pinWidget);
		}

		void CSubstanceNodeContentWidget::RemovePin(CryGraphEditor::CPinWidget& pinWidget)
		{
			CryGraphEditor::CAbstractPinItem& pinItem = pinWidget.GetItem();
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

		void CSubstanceNodeContentWidget::OnPinAdded(CryGraphEditor::CAbstractPinItem& item)
		{
			CryGraphEditor::CPinWidget* pPinWidget = item.CreateWidget(GetNode(), GetView());
			AddPin(*pPinWidget);

			UpdateLayout(m_pLayout);
		}

		void CSubstanceNodeContentWidget::OnPinRemoved(CryGraphEditor::CAbstractPinItem& item)
		{
			QVariant pinId = item.GetId();

			auto condition = [pinId](CryGraphEditor::CPinWidget* pPinWidget) -> bool
			{
				return (pPinWidget && pPinWidget->GetItem().HasId(pinId));
			};

			const auto result = std::find_if(m_pins.begin(), m_pins.end(), condition);
			if (result != m_pins.end())
			{
				CryGraphEditor::CPinWidget* pPinWidget = *result;
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

		void CSubstanceNodeContentWidget::Update()
		{
			if (m_outputType == Virtual)
			{
				m_pPreset->setText(m_pOutput->preset.c_str());
				for each (auto& var in resolutionNamesMap)
				{
					if (var.second == m_pOutput->resolution)
					{
						m_pResolution->setText(var.first.c_str());
						break;
					}
				}

			}

		}

		void CSubstanceNodeContentWidget::SetPreviewImage(const QPixmap& pixmap)
		{
			if (m_showPreview)
			{
				m_pPreviewImage->SetPreviewImage(pixmap);
			}
		}

	}
}
