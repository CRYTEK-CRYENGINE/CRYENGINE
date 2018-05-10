// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ViewWidget.h"

#include "Models/ParticleGraphModel.h"
#include "Models/ParticleGraphItems.h"

#include "FeatureGridNodeContentWidget.h"
#include <QtUtil.h>
#include "Undo.h"

#include <ICommandManager.h>
#include <NodeGraph/NodeGraphUndo.h>

#include <QVBoxLayout>
#include <QlineEdit>
#include <QLabel>
#include <QSpacerItem>
#include <QAdvancedPropertyTree.h>
#include <QControls.h>

namespace CryParticleEditor {

void CItemProperties::SFeatureSerializer::Serialize(Serialization::IArchive& archive)
{
	if (pItem)
	{
		archive.openBlock(name, name);
		pItem->Serialize(archive);
		archive.closeBlock();
	}
}

CItemProperties::CItemProperties(CryGraphEditor::GraphItemSet& selectedItems)
	: m_pPropertyTree(nullptr)
	, m_pNodeItem(nullptr)
	, m_isPushingUndo(true)
{
	struct SNode
	{
		CNodeItem*                 pNodeItem;
		std::vector<CFeatureItem*> features;

		SNode()
			: pNodeItem(nullptr)
		{}
	};

	std::map<uint32 /* name crc */, SNode> nodesByNameCrc;
	for (CryGraphEditor::CAbstractNodeGraphViewModelItem* pAbstractItem : selectedItems)
	{
		if (CNodeItem* pNodeItem = pAbstractItem->Cast<CNodeItem>())
		{
			const uint32 nameCrc = CCrc32::Compute(pNodeItem->GetComponentInterface().GetName());

			auto result = nodesByNameCrc.find(nameCrc);
			if (result == nodesByNameCrc.end())
			{
				SNode node;
				node.pNodeItem = pNodeItem;
				nodesByNameCrc.emplace(nameCrc, node);
			}
		}
		else if (CFeatureItem* pFeatureItem = pAbstractItem->Cast<CFeatureItem>())
		{
			CNodeItem& nodeItem = pFeatureItem->GetNodeItem();
			const uint32 nameCrc = CCrc32::Compute(nodeItem.GetComponentInterface().GetName());

			auto result = nodesByNameCrc.find(nameCrc);
			if (result != nodesByNameCrc.end())
			{
				result->second.features.push_back(pFeatureItem);
			}
			else
			{
				SNode node;
				node.pNodeItem = &nodeItem;
				auto result = nodesByNameCrc.emplace(nameCrc, node);
				if (result.second)
					result.first->second.features.push_back(pFeatureItem);
			}
		}
	}

	if (nodesByNameCrc.size() == 1)
	{
		QWidget* pContainer = new QWidget();
		QVBoxLayout* pLayout = new QVBoxLayout();

		SNode& node = nodesByNameCrc.begin()->second;
		m_pNodeItem = node.pNodeItem;

		const size_t numFeature = (!node.features.empty() ? node.features : node.pNodeItem->GetFeatureItems()).size();
		m_features.resize(numFeature);

		uint32 i = 0;
		for (CFeatureItem* pFeatureItem : (!node.features.empty() ? node.features : node.pNodeItem->GetFeatureItems()))
		{
			SFeatureSerializer& feature = m_features[i++];
			feature.pItem = pFeatureItem;
			feature.name = QtUtil::ToString(QString("%1: %2").arg(pFeatureItem->GetGroupName()).arg(pFeatureItem->GetName()));
		}

		m_pPropertyTree = new QAdvancedPropertyTree(node.pNodeItem->GetName());
		// WORKAROUND: Serialization of features doesn't work with the default style.
		//						 We either need to fix serialization or property tree. As soon as it's
		//						 done use the commented out code below.
		PropertyTreeStyle treeStyle(m_pPropertyTree->treeStyle());
		treeStyle.propertySplitter = false;
		m_pPropertyTree->setTreeStyle(treeStyle);
		m_pPropertyTree->setSizeToContent(true);
		// ~WORKAROUND
		m_pPropertyTree->setExpandLevels(4);
		m_pPropertyTree->setValueColumnWidth(0.6f);
		m_pPropertyTree->setAutoRevert(false);
		m_pPropertyTree->setAggregateMouseEvents(false);
		m_pPropertyTree->setFullRowContainers(true);

		m_structs.push_back(Serialization::SStruct(*this));
		m_pPropertyTree->attach(m_structs);

		m_pNodeItem->SignalInvalidated.Connect(this, &CItemProperties::OnItemsChanged);
		m_pNodeItem->SignalDeletion.Connect(this, &CItemProperties::OnItemsDeletion);
		QObject::connect(m_pPropertyTree, &QPropertyTree::signalPushUndo, this, &CItemProperties::OnPushUndo);
		QObject::connect(m_pPropertyTree, &QPropertyTree::signalChanged, this, &CItemProperties::OnItemsChanged);

		QVBoxLayout* pMainLayout = new QVBoxLayout(this);
		pMainLayout->addWidget(m_pPropertyTree);
	}
}

CItemProperties::~CItemProperties()
{
	if (m_pNodeItem)
	{
		m_pNodeItem->SignalDeletion.DisconnectObject(this);
		m_pNodeItem->SignalInvalidated.DisconnectObject(this);
	}
}

void CItemProperties::Serialize(Serialization::IArchive& archive)
{
	for (SFeatureSerializer& feature : m_features)
	{
		feature.Serialize(archive);
	}
}

void CItemProperties::showEvent(QShowEvent* pEvent)
{
	QWidget::showEvent(pEvent);

	if (m_pPropertyTree)
		m_pPropertyTree->setSizeToContent(true);
}

void CItemProperties::OnPushUndo()
{
	m_isPushingUndo = true;
	CryGraphEditor::CUndoNodePropertiesChange* pUndoObject = new CryGraphEditor::CUndoNodePropertiesChange(*m_pNodeItem);
	CUndo undo(pUndoObject->GetDescription());
	CUndo::Record(pUndoObject);
	m_isPushingUndo = false;
}

void CItemProperties::OnItemsChanged()
{
	if (m_pPropertyTree && !m_isPushingUndo)
		m_pPropertyTree->revertNoninterrupting();
}

void CItemProperties::OnItemsDeletion()
{
	m_pNodeItem = nullptr;
}

CGraphView::CGraphView()
	: CNodeGraphView()
	, m_pMovingFeatureWidget(nullptr)
{

}

CGraphView::~CGraphView()
{

}

void CGraphView::OnFeatureMouseEvent(QGraphicsItem* pSender, SFeatureMouseEventArgs& args)
{
	CFeatureWidget* pFeatureWidget = CryGraphEditor::CNodeGraphViewGraphicsWidget::Cast<CFeatureWidget>(pSender);
	if (pFeatureWidget == nullptr)
		return;

	switch (args.GetReason())
	{
	case CryGraphEditor::EMouseEventReason::ButtonPress:
		{
			if (args.GetButton() == Qt::MouseButton::LeftButton)
			{
				AbortAction();
				const bool shiftPressed = (args.GetModifiers() & Qt::ShiftModifier) != 0;
				if (pFeatureWidget->IsSelected())
				{
					if (shiftPressed)
						DeselectWidget(*pFeatureWidget);
				}
				else
				{
					if (!shiftPressed)
						DeselectAllItems();
					SelectWidget(*pFeatureWidget);
				}
			}
			else if (args.GetButton() == Qt::MouseButton::RightButton)
			{

			}
		}
		break;
	case CryGraphEditor::EMouseEventReason::ButtonRelease:
		{
			uint32 action = GetAction();
			if (args.GetButton() == Qt::MouseButton::LeftButton)
			{
				if (action == eAction_ItemMovement)
					SetAction(eAction_None);

				if (action == eAction_ConnectionCreation)
					AbortAction();

				if (action == eCustomAction_MoveFeature)
				{
					CRY_ASSERT(m_pMovingFeatureWidget);
					if (m_pMovingFeatureWidget)
					{

						const int32 destIndex = pFeatureWidget->GetContentWidget().EndFeatureMove();
						MoveFeatureToIndex(*m_pMovingFeatureWidget, destIndex);
						m_pMovingFeatureWidget = nullptr;
					}

					SetAction(eAction_None);
				}
			}
			else if (args.GetButton() == Qt::MouseButton::RightButton)
			{
				AbortAction();

				if (!pFeatureWidget->IsSelected() || GetSelectedItems().size() <= 1)
				{
					DeselectAllItems();
					SelectWidget(*pFeatureWidget);

					ShowFeatureContextMenu(pFeatureWidget, args.GetScreenPos());
				}
			}
		}
		break;
	case CryGraphEditor::EMouseEventReason::ButtonPressAndMove:
		{
			if (args.GetButtons() == Qt::MouseButton::LeftButton)
			{
				uint32 action = GetAction();
				if (action == eAction_None || action == eCustomAction_MoveFeature)
				{
					SetAction(eCustomAction_MoveFeature);
					if (m_pMovingFeatureWidget == nullptr)
					{
						GetIEditor()->GetIUndoManager()->Begin();
						pFeatureWidget->GetContentWidget().BeginFeatureMove(*pFeatureWidget);
						m_pMovingFeatureWidget = pFeatureWidget;
					}
					else
					{
						pFeatureWidget->GetContentWidget().MoveFeature(args.GetScenePos() - args.GetLastScenePos());
					}
				}
			}
		}
		break;
	case CryGraphEditor::EMouseEventReason::HoverMove:
		{
			pFeatureWidget->SetHighlighted(true);
		}
		break;
	case CryGraphEditor::EMouseEventReason::HoverLeave:
		{
			pFeatureWidget->SetHighlighted(false);
		}
		break;
	default:
		break;
	}
}

bool CGraphView::MoveFeatureToIndex(CFeatureWidget& featureWidget, uint32 destIndex)
{
	CFeatureItem& moveFeatureItem = featureWidget.GetItem();
	CNodeItem& nodeItem = moveFeatureItem.GetNodeItem();

	const uint32 itemIndex = moveFeatureItem.GetIndex();
	if (destIndex < nodeItem.GetNumFeatures())
	{
		const bool swapSucceeded = nodeItem.MoveFeatureAtIndex(itemIndex, destIndex);
		if (GetIEditor()->GetIUndoManager()->IsUndoRecording())
		{
			if (swapSucceeded)
			{
				stack_string desc;
				desc.Format("Undo feature movement from index '%d' to index '%d'", itemIndex, destIndex);
				GetIEditor()->GetIUndoManager()->Accept(desc.c_str());
			}
			else
				GetIEditor()->GetIUndoManager()->Cancel();
		}

		return swapSucceeded;
	}

	return false;
}

void CGraphView::ShowFeatureContextMenu(CFeatureWidget* pFeatureWidget, QPointF screenPos)
{
	ICommandManager* pCommandManager = GetIEditor()->GetICommandManager();
	CFeatureItem* pItem = static_cast<CFeatureItem*>(pFeatureWidget->GetAbstractItem());
	CRY_ASSERT(pItem);

	QMenu menu;
	menu.addAction(pCommandManager->GetAction("general.copy"));
	menu.addAction(pCommandManager->GetAction("general.paste"));
	// TODO: Not yet implemented
	//menu.addAction(pCommandManager->GetAction("general.cut"));
	menu.addAction(pCommandManager->GetAction("general.delete"));

	if (CFeatureItem* pFeatureItem = pFeatureWidget->GetAbstractItem()->Cast<CFeatureItem>())
	{
		const uint32 index = pFeatureItem->GetIndex();
		menu.addAction(new QMenuLabelSeparator("Add feature"));
		PopulateMenuWithFeatures("Before", pFeatureItem->GetNodeItem(), menu, index);
		PopulateMenuWithFeatures("After", pFeatureItem->GetNodeItem(), menu, index + 1);
	}

	menu.addSeparator();
	if (pItem->GetAcceptsDeactivation())
	{
		QAction* pAction = menu.addAction(QObject::tr("Active"));
		pAction->setCheckable(true);
		pAction->setChecked(!pItem->IsDeactivated());
		QObject::connect(pAction, &QAction::triggered, this, [pItem](bool isChecked)
			{
				pItem->SetDeactivated(!isChecked);
		  });
	}

	const QPoint parent = mapFromGlobal(QPoint(screenPos.x(), screenPos.y()));
	const QPointF scenePos = mapToScene(parent);
	SetContextMenuPosition(scenePos);

	menu.exec(QPoint(screenPos.x(), screenPos.y()));
}

QWidget* CGraphView::CreatePropertiesWidget(CryGraphEditor::GraphItemSet& selectedItems)
{
	return new CItemProperties(selectedItems);
}

bool CGraphView::PopulateNodeContextMenu(CryGraphEditor::CAbstractNodeItem& node, QMenu& menu)
{
	return PopulateMenuWithFeatures("Add Feature", node, menu, -1);
}

bool CGraphView::PopulateMenuWithFeatures(const char* szTitle, CryGraphEditor::CAbstractNodeItem& node, QMenu& menu, uint32 index)
{
	CNodeItem& nodeItem = *node.Cast<CNodeItem>(&node);

	struct SSubMenu
	{
		std::map<QString, QAction*> m_subActions;
		QMenu*                      m_pMenu;
	};
	std::map<QString, SSubMenu> subMenus;

	const uint32 numParams = GetParticleSystem()->GetNumFeatureParams();
	for (uint32 i = 0; i < numParams; ++i)
	{
		const pfx2::SParticleFeatureParams& featureParams = GetParticleSystem()->GetFeatureParam(i);

		const QString subMenuName = QString(featureParams.m_groupName);
		auto result = subMenus.find(subMenuName);
		if (result == subMenus.end())
		{
			SSubMenu subMenu;
			subMenu.m_pMenu = new QMenu(subMenuName);
			result = subMenus.emplace(subMenuName, subMenu).first;
		}

		const QString actionName = QString(featureParams.m_featureName);
		QAction* pAction = new QAction(actionName, 0);
		QObject::connect(pAction, &QAction::triggered, this, [&featureParams, &nodeItem, index]()
			{
				GetIEditor()->GetIUndoManager()->Begin();
				CFeatureItem* pFeatureItem;
				if (index != ~0)
					pFeatureItem = nodeItem.AddFeature(index, featureParams);
				else
					pFeatureItem = nodeItem.AddFeature(featureParams);

				if (pFeatureItem)
					GetIEditor()->GetIUndoManager()->Accept("Undo feature add");
				else
					GetIEditor()->GetIUndoManager()->Cancel();

		  });

		result->second.m_subActions[actionName] = pAction;
	}

	QMenu* pAddFeatureMenu = new QMenu(szTitle);
	menu.addMenu(pAddFeatureMenu);

	for (auto& subMenu : subMenus)
	{
		pAddFeatureMenu->addMenu(subMenu.second.m_pMenu);
		for (auto& action : subMenu.second.m_subActions)
		{
			subMenu.second.m_pMenu->addAction(action.second);
		}
	}

	return true;
}

bool CGraphView::DeleteCustomItem(CryGraphEditor::CAbstractNodeGraphViewModelItem& item)
{
	const int32 type = item.GetType();
	switch (type)
	{
	case eCustomItemType_Feature:
		{
			if (CFeatureItem* pFeatureItem = item.Cast<CFeatureItem>())
			{
				pFeatureItem->GetNodeItem().RemoveFeature(pFeatureItem->GetIndex());
			}
		}
		break;
	default:
		break;
	}

	return true;
}

void CGraphView::OnRemoveCustomItem(CryGraphEditor::CAbstractNodeGraphViewModelItem& item)
{
	DeselectItem(item);
}

}

