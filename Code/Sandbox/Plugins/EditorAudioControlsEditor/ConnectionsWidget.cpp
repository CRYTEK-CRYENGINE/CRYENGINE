// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ConnectionsWidget.h"

#include "AudioControlsEditorPlugin.h"
#include "ImplementationManager.h"
#include "TreeView.h"
#include "ConnectionsModel.h"

#include <ModelUtils.h>
#include <IItem.h>
#include <QtUtil.h>
#include <Controls/QuestionDialog.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/STL.h>
#include <Serialization/QPropertyTree/QPropertyTree.h>
#include <ProxyModels/AttributeFilterProxyModel.h>

#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QSplitter>
#include <QVBoxLayout>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CConnectionsWidget::CConnectionsWidget(QWidget* const pParent)
	: QWidget(pParent)
	, m_pControl(nullptr)
	, m_pConnectionModel(new CConnectionsModel(this))
	, m_pAttributeFilterProxyModel(new QAttributeFilterProxyModel(QAttributeFilterProxyModel::BaseBehavior, this))
	, m_pConnectionProperties(new QPropertyTree(this))
	, m_pTreeView(new CTreeView(this))
	, m_nameColumn(static_cast<int>(CConnectionsModel::EColumns::Name))
{
	m_pAttributeFilterProxyModel->setSourceModel(m_pConnectionModel);
	m_pAttributeFilterProxyModel->setFilterKeyColumn(m_nameColumn);

	m_pTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_pTreeView->setDragEnabled(false);
	m_pTreeView->setDragDropMode(QAbstractItemView::DropOnly);
	m_pTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_pTreeView->setUniformRowHeights(true);
	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	m_pTreeView->setModel(m_pAttributeFilterProxyModel);
	m_pTreeView->sortByColumn(m_nameColumn, Qt::AscendingOrder);
	m_pTreeView->setItemsExpandable(false);
	m_pTreeView->setRootIsDecorated(false);
	m_pTreeView->installEventFilter(this);
	m_pTreeView->header()->setMinimumSectionSize(25);
	m_pTreeView->header()->setSectionResizeMode(static_cast<int>(CConnectionsModel::EColumns::Notification), QHeaderView::ResizeToContents);
	m_pTreeView->SetNameColumn(m_nameColumn);
	m_pTreeView->SetNameRole(static_cast<int>(ModelUtils::ERoles::Name));
	m_pTreeView->TriggerRefreshHeaderColumns();

	QObject::connect(m_pTreeView, &CTreeView::customContextMenuRequested, this, &CConnectionsWidget::OnContextMenu);
	QObject::connect(m_pTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, [this]()
		{
			RefreshConnectionProperties();
			UpdateSelectedConnections();
	  });

	auto const pSplitter = new QSplitter(Qt::Vertical, this);
	pSplitter->addWidget(m_pTreeView);
	pSplitter->addWidget(m_pConnectionProperties);
	pSplitter->setCollapsible(0, false);
	pSplitter->setCollapsible(1, false);
	pSplitter->setStretchFactor(0, 0);
	pSplitter->setStretchFactor(1, 1);

	auto const pMainLayout = new QVBoxLayout(this);
	pMainLayout->setContentsMargins(0, 0, 0, 0);
	pMainLayout->addWidget(pSplitter);
	setLayout(pMainLayout);

	setHidden(true);

	g_assetsManager.SignalConnectionRemoved.Connect([this](CControl* pControl)
		{
			if (!g_assetsManager.IsLoading() && (m_pControl == pControl))
			{
			  m_pTreeView->selectionModel()->clear();
			  RefreshConnectionProperties();
			}
	  }, reinterpret_cast<uintptr_t>(this));

	g_implementationManager.SignalImplementationAboutToChange.Connect([this]()
		{
			m_pTreeView->selectionModel()->clear();
			RefreshConnectionProperties();
	  }, reinterpret_cast<uintptr_t>(this));

	QObject::connect(m_pConnectionModel, &CConnectionsModel::SignalConnectionAdded, this, &CConnectionsWidget::OnConnectionAdded);
}

//////////////////////////////////////////////////////////////////////////
CConnectionsWidget::~CConnectionsWidget()
{
	g_assetsManager.SignalConnectionRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_implementationManager.SignalImplementationAboutToChange.DisconnectById(reinterpret_cast<uintptr_t>(this));

	m_pConnectionModel->DisconnectSignals();
	m_pConnectionModel->deleteLater();
}

//////////////////////////////////////////////////////////////////////////
bool CConnectionsWidget::eventFilter(QObject* pObject, QEvent* pEvent)
{
	bool isEvent = false;

	if (pEvent->type() == QEvent::KeyPress)
	{
		QKeyEvent const* const pKeyEvent = static_cast<QKeyEvent*>(pEvent);

		if ((pKeyEvent != nullptr) && (pKeyEvent->key() == Qt::Key_Delete) && (pObject == m_pTreeView))
		{
			RemoveSelectedConnection();
			isEvent = true;
		}
	}

	if (!isEvent)
	{
		isEvent = QWidget::eventFilter(pObject, pEvent);
	}

	return isEvent;
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::OnContextMenu(QPoint const& pos)
{
	auto const selection = m_pTreeView->selectionModel()->selectedRows();
	int const selectionCount = selection.count();

	if (selectionCount > 0)
	{
		auto const pContextMenu = new QMenu(this);

		char const* actionName = "Remove Connection";

		if (selectionCount > 1)
		{
			actionName = "Remove Connections";
		}

		pContextMenu->addAction(tr(actionName), [&]() { RemoveSelectedConnection(); });

		if (selectionCount == 1)
		{
			ControlId const itemId = static_cast<ControlId>(selection[0].data(static_cast<int>(ModelUtils::ERoles::Id)).toInt());
			Impl::IItem const* const pIItem = g_pIImpl->GetItem(itemId);

			if ((pIItem != nullptr) && ((pIItem->GetFlags() & EItemFlags::IsPlaceHolder) == 0))
			{
				pContextMenu->addSeparator();
				pContextMenu->addAction(tr("Select in Middleware Data"), [=]()
					{
						SignalSelectConnectedImplItem(itemId);
				  });
			}
		}

		pContextMenu->exec(QCursor::pos());
	}
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::OnConnectionAdded(ControlId const id)
{
	if (m_pControl != nullptr)
	{
		auto const& matches = m_pAttributeFilterProxyModel->match(m_pAttributeFilterProxyModel->index(0, 0, QModelIndex()), static_cast<int>(ModelUtils::ERoles::Id), id, 1, Qt::MatchRecursive);

		if (!matches.isEmpty())
		{
			m_pTreeView->selectionModel()->select(matches.first(), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		}

		ResizeColumns();
	}
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::RemoveSelectedConnection()
{
	if (m_pControl != nullptr)
	{
		auto const messageBox = new CQuestionDialog();
		QModelIndexList const& selectedIndexes = m_pTreeView->selectionModel()->selectedRows(m_nameColumn);

		if (!selectedIndexes.empty())
		{
			int const numSelected = selectedIndexes.length();
			QString text;

			if (numSelected == 1)
			{
				text = R"(Are you sure you want to delete the connection between ")" + QtUtil::ToQString(m_pControl->GetName()) + R"(" and ")" + selectedIndexes[0].data(Qt::DisplayRole).toString() + R"("?)";
			}
			else
			{
				text = "Are you sure you want to delete the " + QString::number(numSelected) + " selected connections?";
			}

			messageBox->SetupQuestion("Audio Controls Editor", text);

			if (messageBox->Execute() == QDialogButtonBox::Yes)
			{
				std::vector<Impl::IItem*> implItems;
				implItems.reserve(selectedIndexes.size());

				for (QModelIndex const& index : selectedIndexes)
				{
					ControlId const id = static_cast<ControlId>(index.data(static_cast<int>(ModelUtils::ERoles::Id)).toInt());
					implItems.push_back(g_pIImpl->GetItem(id));
				}

				for (Impl::IItem* const pIItem : implItems)
				{
					if (pIItem != nullptr)
					{
						m_pControl->RemoveConnection(pIItem);
					}
				}

				ResizeColumns();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::SetControl(CControl* const pControl, bool const restoreSelection)
{
	if (m_pControl != pControl)
	{
		m_pControl = pControl;
		Reset();

		if ((m_pControl != nullptr) && restoreSelection)
		{
			auto const& selectedConnections = m_pControl->GetSelectedConnections();

			if (!selectedConnections.empty())
			{
				int matchCount = 0;

				for (auto const itemId : selectedConnections)
				{
					auto const& matches = m_pAttributeFilterProxyModel->match(m_pAttributeFilterProxyModel->index(0, 0, QModelIndex()), static_cast<int>(ModelUtils::ERoles::Id), itemId, 1, Qt::MatchRecursive);

					if (!matches.isEmpty())
					{
						m_pTreeView->selectionModel()->select(matches.first(), QItemSelectionModel::Select | QItemSelectionModel::Rows);
						++matchCount;
					}
				}

				if (matchCount == 0)
				{
					m_pTreeView->setCurrentIndex(m_pTreeView->model()->index(0, m_nameColumn));
				}
			}
			else
			{
				m_pTreeView->setCurrentIndex(m_pTreeView->model()->index(0, m_nameColumn));
			}
		}

		ResizeColumns();
	}
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::Reset()
{
	m_pConnectionModel->Init(m_pControl);
	m_pTreeView->selectionModel()->clear();
	RefreshConnectionProperties();
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::RefreshConnectionProperties()
{
	m_pConnectionProperties->detach();

	Serialization::SStructs serializers;
	bool showProperties = false;

	if (m_pControl != nullptr)
	{
		QModelIndexList const& selectedIndexes = m_pTreeView->selectionModel()->selectedRows(m_nameColumn);

		for (auto const& index : selectedIndexes)
		{
			if (index.isValid())
			{
				ControlId const id = static_cast<ControlId>(index.data(static_cast<int>(ModelUtils::ERoles::Id)).toInt());
				ConnectionPtr const pConnection = m_pControl->GetConnection(id);

				if ((pConnection != nullptr) && pConnection->HasProperties())
				{
					serializers.emplace_back(*pConnection);
					showProperties = true;
				}
			}
		}
	}

	m_pConnectionProperties->attach(serializers);
	m_pConnectionProperties->setHidden(!showProperties);
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::UpdateSelectedConnections()
{
	ControlIds selectedIds;
	QModelIndexList const& selectedIndexes = m_pTreeView->selectionModel()->selectedRows(m_nameColumn);

	for (auto const& index : selectedIndexes)
	{
		if (index.isValid())
		{
			ControlId const id = static_cast<ControlId>(index.data(static_cast<int>(ModelUtils::ERoles::Id)).toInt());
			selectedIds.push_back(id);
		}
	}

	m_pControl->SetSelectedConnections(selectedIds);
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::ResizeColumns()
{
	m_pTreeView->resizeColumnToContents(m_nameColumn);
	m_pTreeView->resizeColumnToContents(static_cast<int>(CConnectionsModel::EColumns::Path));
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::OnAboutToReload()
{
	m_pTreeView->BackupSelection();
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::OnReloaded()
{
	m_pTreeView->RestoreSelection();
}
} // namespace ACE

