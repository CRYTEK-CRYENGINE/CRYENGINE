// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TerrainLayerView.h"

#include "IEditorImpl.h"
#include "Terrain/TerrainLayerModel.h"
#include "Terrain/TerrainManager.h"
#include "Terrain/Layer.h"
#include "RecursionLoopGuard.h"

#include <Commands/QCommandAction.h>
#include <EditorFramework/Events.h>
#include <QAdvancedItemDelegate.h>

#include <QEvent>
#include <QMenu>
#include <QMouseEvent>

QTerrainLayerView::QTerrainLayerView(QWidget* pParent, CTerrainManager* pTerrainManager)
	: QAdvancedTreeView(BehaviorFlags(PreserveExpandedAfterReset | PreserveSelectionAfterReset), pParent)
	, m_pModel(nullptr)
	, m_pTerrainManager(pTerrainManager)
	, m_selectionProcessing(false)
{
	CRY_ASSERT(m_pTerrainManager);

	setSelectionMode(QAbstractItemView::SingleSelection);
	setDragDropMode(QAbstractItemView::InternalMove);
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setSortingEnabled(false);
	setIconSize(QSize(CTerrainLayerModel::s_layerPreviewSize, CTerrainLayerModel::s_layerPreviewSize + 2));

	pTerrainManager->signalSelectedLayerChanged.Connect(this, &QTerrainLayerView::SelectedLayerChanged);
	pTerrainManager->signalLayersChanged.Connect(this, &QTerrainLayerView::LayersChanged);
	m_pModel = new CTerrainLayerModel(this, pTerrainManager);

	setModel(m_pModel);
	setItemDelegate(new QAdvancedItemDelegate(this));

	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &QTerrainLayerView::customContextMenuRequested, this, &QTerrainLayerView::OnContextMenu);
}

QTerrainLayerView::~QTerrainLayerView()
{
	m_pTerrainManager->signalSelectedLayerChanged.DisconnectObject(this);
	m_pTerrainManager->signalLayersChanged.DisconnectObject(this);
}

void QTerrainLayerView::SelectRow(int row)
{
	if (selectionModel() && model())
	{
		selectionModel()->select(model()->index(row, 0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		update();
	}
}

void QTerrainLayerView::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
	RECURSION_GUARD(m_selectionProcessing);

	if (0 != selected.size())
	{
		int selectedIndex = selected.front().top();
		m_pTerrainManager->SelectLayer(selectedIndex);
	}

	__super::selectionChanged(selected, deselected);
}

void QTerrainLayerView::SelectedLayerChanged(CLayer* pLayer)
{
	RECURSION_GUARD(m_selectionProcessing);

	const int layerCount = m_pTerrainManager->GetLayerCount();
	if (layerCount == 0)
	{
		update();
		return;
	}

	for (int i = 0; i < layerCount; ++i)
	{
		if (pLayer == m_pTerrainManager->GetLayer(i))
		{
			SelectRow(i);

			const auto currSelection = selectedIndexes();
			if (!currSelection.empty())
			{
				scrollTo(currSelection.front());
			}

			return;
		}
	}
}

void QTerrainLayerView::customEvent(QEvent* event)
{
	if (event->type() == SandboxEvent::Command)
	{
		CommandEvent* commandEvent = static_cast<CommandEvent*>(event);
		const string& command = commandEvent->GetCommand();
		if (command == "general.delete")
		{
			if (!selectionModel())
			{
				return;
			}

			auto selectedRows = selectionModel()->selectedRows();
			if (!selectedRows.empty())
			{
				GetIEditorImpl()->ExecuteCommand("terrain.delete_layer");
				commandEvent->setAccepted(true);
			}
		}
	}
	else
	{
		__super::customEvent(event);
	}
}

void QTerrainLayerView::LayersChanged()
{
	RECURSION_GUARD(m_selectionProcessing);

	if (0 == selectedIndexes().size())
	{
		int layerCount = m_pTerrainManager->GetLayerCount();
		for (int i = 0; i < layerCount; ++i)
		{
			if (m_pTerrainManager->GetLayer(i)->IsSelected())
			{
				SelectRow(i);
				return;
			}
		}
	}
}

void QTerrainLayerView::OnContextMenu(const QPoint& pos)
{
	ICommandManager* pManager = GetIEditorImpl()->GetICommandManager();
	QMenu menu;

	const QModelIndex indexUnderMousePress = indexAt(pos);
	if (indexUnderMousePress.isValid())
	{
		string name = "Create Layer (below)";
		string command;
		command.Format("terrain.create_layer_at '%d'", indexUnderMousePress.row() + 1);
		menu.addAction(new QCommandAction(name.c_str(), command.c_str(), &menu));

		menu.addAction(pManager->GetAction("terrain.delete_layer"));
		menu.addAction(pManager->GetAction("terrain.duplicate_layer"));
		menu.addSeparator();

		menu.addAction(pManager->GetAction("terrain.move_layer_to_top"));
		menu.addAction(pManager->GetAction("terrain.move_layer_up"));
		menu.addAction(pManager->GetAction("terrain.move_layer_down"));
		menu.addAction(pManager->GetAction("terrain.move_layer_to_bottom"));
		menu.addSeparator();

		menu.addAction(pManager->GetAction("terrain.flood_layer"));
	}
	else
	{
		menu.addAction(pManager->GetAction("terrain.create_layer"));
	}

	menu.exec(QCursor::pos());
}
