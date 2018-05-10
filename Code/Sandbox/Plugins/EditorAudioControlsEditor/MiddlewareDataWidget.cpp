// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MiddlewareDataWidget.h"

#include "AudioControlsEditorPlugin.h"
#include "MiddlewareDataModel.h"
#include "MiddlewareFilterProxyModel.h"
#include "ImplementationManager.h"
#include "AssetIcons.h"
#include "ModelUtils.h"
#include "TreeView.h"

#include <IItem.h>
#include <CryString/CryPath.h>
#include <QFilteringPanel.h>
#include <QSearchBox.h>
#include <QtUtil.h>

#include <QHeaderView>
#include <QMenu>
#include <QVBoxLayout>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CMiddlewareDataWidget::CMiddlewareDataWidget(QWidget* const pParent)
	: QWidget(pParent)
	, m_pMiddlewareFilterProxyModel(new CMiddlewareFilterProxyModel(this))
	, m_pMiddlewareDataModel(new CMiddlewareDataModel(this))
	, m_pTreeView(new CTreeView(this))
	, m_nameColumn(static_cast<int>(CMiddlewareDataModel::EColumns::Name))
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	m_pMiddlewareFilterProxyModel->setSourceModel(m_pMiddlewareDataModel);
	m_pMiddlewareFilterProxyModel->setFilterKeyColumn(m_nameColumn);

	m_pTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_pTreeView->setDragEnabled(true);
	m_pTreeView->setDragDropMode(QAbstractItemView::DragOnly);
	m_pTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_pTreeView->setTreePosition(m_nameColumn);
	m_pTreeView->setUniformRowHeights(true);
	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	m_pTreeView->setModel(m_pMiddlewareFilterProxyModel);
	m_pTreeView->sortByColumn(m_nameColumn, Qt::AscendingOrder);
	m_pTreeView->header()->setMinimumSectionSize(25);
	m_pTreeView->header()->setSectionResizeMode(static_cast<int>(CMiddlewareDataModel::EColumns::Notification), QHeaderView::ResizeToContents);
	m_pTreeView->SetNameColumn(m_nameColumn);
	m_pTreeView->SetNameRole(static_cast<int>(ModelUtils::ERoles::Name));
	m_pTreeView->TriggerRefreshHeaderColumns();

	m_pFilteringPanel = new QFilteringPanel("ACEMiddlewareData", m_pMiddlewareFilterProxyModel, this);
	m_pFilteringPanel->SetContent(m_pTreeView);
	m_pFilteringPanel->GetSearchBox()->SetAutoExpandOnSearch(m_pTreeView);

	auto const pMainLayout = new QVBoxLayout(this);
	pMainLayout->setContentsMargins(0, 0, 0, 0);
	pMainLayout->addWidget(m_pFilteringPanel);

	if (g_pIImpl == nullptr)
	{
		setEnabled(false);
	}

	QObject::connect(m_pTreeView, &CTreeView::customContextMenuRequested, this, &CMiddlewareDataWidget::OnContextMenu);

	g_assetsManager.SignalConnectionAdded.Connect([this]()
		{
			if (!g_assetsManager.IsLoading())
			{
			  m_pMiddlewareFilterProxyModel->invalidate();
			}
	  }, reinterpret_cast<uintptr_t>(this));

	g_assetsManager.SignalConnectionRemoved.Connect([this]()
		{
			if (!g_assetsManager.IsLoading())
			{
			  m_pMiddlewareFilterProxyModel->invalidate();
			}
	  }, reinterpret_cast<uintptr_t>(this));

	g_implementationManager.SignalImplementationChanged.Connect([this]()
		{
			setEnabled(g_pIImpl != nullptr);
	  }, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
CMiddlewareDataWidget::~CMiddlewareDataWidget()
{
	g_assetsManager.SignalConnectionAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_assetsManager.SignalConnectionRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_implementationManager.SignalImplementationChanged.DisconnectById(reinterpret_cast<uintptr_t>(this));

	m_pMiddlewareDataModel->DisconnectSignals();
	m_pMiddlewareDataModel->deleteLater();
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::OnContextMenu(QPoint const& pos)
{
	auto const pContextMenu = new QMenu(this);
	auto const& selection = m_pTreeView->selectionModel()->selectedRows(m_nameColumn);

	if (!selection.isEmpty())
	{
		if (selection.count() == 1)
		{
			ControlId const itemId = selection[0].data(static_cast<int>(ModelUtils::ERoles::Id)).toInt();
			Impl::IItem const* const pIItem = g_pIImpl->GetItem(itemId);

			if ((pIItem != nullptr) && ((pIItem->GetFlags() & EItemFlags::IsConnected) != 0))
			{
				auto const pConnectionsMenu = new QMenu(pContextMenu);
				auto const& controls = g_assetsManager.GetControls();
				int count = 0;

				for (auto const pControl : controls)
				{
					if (pControl->GetConnection(pIItem) != nullptr)
					{
						pConnectionsMenu->addAction(GetAssetIcon(pControl->GetType()), tr(pControl->GetName()), [=]()
							{
								SignalSelectConnectedSystemControl(*pControl, pIItem->GetId());
						  });

						++count;
					}
				}

				if (count > 0)
				{
					pConnectionsMenu->setTitle(tr("Connections (" + ToString(count) + ")"));
					pContextMenu->addMenu(pConnectionsMenu);
					pContextMenu->addSeparator();
				}
			}

			if ((pIItem != nullptr) && !pIItem->GetFilePath().IsEmpty())
			{
				pContextMenu->addAction(tr("Open Containing Folder"), [&]()
					{
						QtUtil::OpenInExplorer((PathUtil::GetGameFolder() + "/" + pIItem->GetFilePath()).c_str());
				  });

				pContextMenu->addSeparator();
			}
		}

		pContextMenu->addAction(tr("Expand Selection"), [&]() { m_pTreeView->ExpandSelection(m_pTreeView->GetSelectedIndexes()); });
		pContextMenu->addAction(tr("Collapse Selection"), [&]() { m_pTreeView->CollapseSelection(m_pTreeView->GetSelectedIndexes()); });
		pContextMenu->addSeparator();
	}

	pContextMenu->addAction(tr("Expand All"), [&]() { m_pTreeView->expandAll(); });
	pContextMenu->addAction(tr("Collapse All"), [&]() { m_pTreeView->collapseAll(); });

	pContextMenu->exec(QCursor::pos());
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::SelectConnectedImplItem(ControlId const itemId)
{
	ClearFilters();
	auto const& matches = m_pMiddlewareFilterProxyModel->match(m_pMiddlewareFilterProxyModel->index(0, 0, QModelIndex()), static_cast<int>(ModelUtils::ERoles::Id), itemId, 1, Qt::MatchRecursive);

	if (!matches.isEmpty())
	{
		m_pTreeView->setFocus();
		m_pTreeView->selectionModel()->setCurrentIndex(matches.first(), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::ClearFilters()
{
	m_pFilteringPanel->GetSearchBox()->clear();
	m_pFilteringPanel->Clear();
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::Reset()
{
	ClearFilters();
	m_pMiddlewareDataModel->Reset();
	m_pMiddlewareFilterProxyModel->invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::BackupTreeViewStates()
{
	m_pTreeView->BackupExpanded();
	m_pTreeView->BackupSelection();
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::RestoreTreeViewStates()
{
	m_pTreeView->RestoreExpanded();
	m_pTreeView->RestoreSelection();
}
} // namespace ACE

