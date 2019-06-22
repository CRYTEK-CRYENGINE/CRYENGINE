// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ConnectionsWidget.h"

#include "AudioControlsEditorPlugin.h"
#include "AssetsManager.h"
#include "ImplManager.h"
#include "TreeView.h"
#include "ConnectionsModel.h"
#include "AssetUtils.h"
#include "FileImporterUtils.h"
#include "Common/IConnection.h"
#include "Common/IImpl.h"
#include "Common/IItem.h"
#include "Common/ModelUtils.h"

#include <QtUtil.h>
#include <Controls/QuestionDialog.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/STL.h>
#include <Serialization/QPropertyTreeLegacy/QPropertyTreeLegacy.h>
#include <ProxyModels/AttributeFilterProxyModel.h>

#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QSplitter>
#include <QVBoxLayout>

namespace ACE
{
constexpr int g_connectionsNameColumn = static_cast<int>(CConnectionsModel::EColumns::Name);

//////////////////////////////////////////////////////////////////////////
CConnectionsWidget::CConnectionsWidget(QWidget* const pParent)
	: QWidget(pParent)
	, m_pControl(nullptr)
	, m_pConnectionModel(new CConnectionsModel(this))
	, m_pAttributeFilterProxyModel(new QAttributeFilterProxyModel(QAttributeFilterProxyModel::BaseBehavior, this))
	, m_pConnectionProperties(new QPropertyTreeLegacy(this))
	, m_pTreeView(new CTreeView(this))
{
	m_pAttributeFilterProxyModel->setSourceModel(m_pConnectionModel);
	m_pAttributeFilterProxyModel->setFilterKeyColumn(g_connectionsNameColumn);

	m_pTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_pTreeView->setDragEnabled(false);
	m_pTreeView->setDragDropMode(QAbstractItemView::DropOnly);
	m_pTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_pTreeView->setUniformRowHeights(true);
	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	m_pTreeView->setModel(m_pAttributeFilterProxyModel);
	m_pTreeView->sortByColumn(g_connectionsNameColumn, Qt::AscendingOrder);
	m_pTreeView->setItemsExpandable(false);
	m_pTreeView->setRootIsDecorated(false);
	m_pTreeView->viewport()->installEventFilter(this);
	m_pTreeView->installEventFilter(this);
	m_pTreeView->header()->setMinimumSectionSize(25);
	m_pTreeView->header()->setSectionResizeMode(static_cast<int>(CConnectionsModel::EColumns::Notification), QHeaderView::ResizeToContents);
	m_pTreeView->SetNameColumn(g_connectionsNameColumn);
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

	g_implManager.SignalOnBeforeImplChange.Connect([this]()
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
	g_implManager.SignalOnBeforeImplChange.DisconnectById(reinterpret_cast<uintptr_t>(this));

	m_pConnectionModel->DisconnectSignals();
	m_pConnectionModel->deleteLater();
}

//////////////////////////////////////////////////////////////////////////
bool CConnectionsWidget::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if (pEvent->type() == QEvent::KeyRelease)
	{
		QKeyEvent const* const pKeyEvent = static_cast<QKeyEvent*>(pEvent);

		if (pKeyEvent != nullptr)
		{
			if (pKeyEvent->key() == Qt::Key_Delete)
			{
				RemoveSelectedConnection();
			}
			else if (pKeyEvent->key() == Qt::Key_Space)
			{
				ExecuteConnection();
			}
		}
	}
	else if (pEvent->type() == QEvent::MouseButtonDblClick)
	{
		ExecuteConnection();
	}

	return QWidget::eventFilter(pObject, pEvent);
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::OnContextMenu(QPoint const& pos)
{
	bool canExec = false;
	QModelIndexList const& selection = m_pTreeView->selectionModel()->selectedRows();
	int const numSelections = selection.size();
	auto const pContextMenu = new QMenu(this);

	if (numSelections > 0)
	{
		char const* executeActionName = "Execute Connection";
		char const* removeActionName = "Remove Connection";

		if (numSelections > 1)
		{
			executeActionName = "Execute Connections";
			removeActionName = "Remove Connections";
		}

		if (m_pControl->GetType() == EAssetType::Trigger)
		{
			pContextMenu->addAction(tr(executeActionName), [&]() { ExecuteConnection(); });
		}

		pContextMenu->addAction(tr(removeActionName), [&]() { RemoveSelectedConnection(); });

		if (numSelections == 1)
		{
			ControlId const itemId = static_cast<ControlId>(selection[0].data(static_cast<int>(ModelUtils::ERoles::Id)).toInt());
			Impl::IItem const* const pIItem = g_pIImpl->GetItem(itemId);

			if ((pIItem != nullptr) && ((pIItem->GetFlags() & EItemFlags::IsPlaceHolder) == 0))
			{
				pContextMenu->addSeparator();
				pContextMenu->addAction(tr("Select in Middleware Data"), [=]()
					{
						if (g_pIImpl != nullptr)
						{
						  g_pIImpl->OnSelectConnectedItem(itemId);
						}
					});
			}
		}

		canExec = true;
	}

	if ((g_implInfo.flags & EImplInfoFlags::SupportsFileImport) != 0)
	{
		pContextMenu->addSeparator();
		pContextMenu->addAction(tr("Import Files"), [=]()
			{
				OpenFileSelector(EImportTargetType::Connections, m_pControl);
			});

		canExec = true;
	}

	if (canExec)
	{
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
		QModelIndexList const& selectedIndexes = m_pTreeView->selectionModel()->selectedRows(g_connectionsNameColumn);

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

			messageBox->SetupQuestion(g_szEditorName, text);

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
void CConnectionsWidget::ExecuteConnection()
{
	if ((m_pControl != nullptr) && (m_pControl->GetType() == EAssetType::Trigger))
	{
		CAudioControlsEditorPlugin::ExecuteTriggerEx(m_pControl->GetName(), ConstructTemporaryTriggerConnections(m_pControl));
	}
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CConnectionsWidget::ConstructTemporaryTriggerConnections(CControl const* const pControl)
{
	XmlNodeRef const node = GetISystem()->CreateXmlNode(CryAudio::g_szTriggerTag);

	if (node.isValid())
	{
		node->setAttr(CryAudio::g_szNameAttribute, pControl->GetName());
		QModelIndexList const& selectedIndexes = m_pTreeView->selectionModel()->selectedRows(g_connectionsNameColumn);

		for (auto const& index : selectedIndexes)
		{
			if (index.isValid())
			{
				ControlId const id = static_cast<ControlId>(index.data(static_cast<int>(ModelUtils::ERoles::Id)).toInt());
				IConnection const* const pIConnection = m_pControl->GetConnection(id);

				if (pIConnection != nullptr)
				{
					AssetUtils::TryConstructTriggerConnectionNode(node, pIConnection, pControl->GetContextId());
				}
			}
		}
	}

	return node;
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
					m_pTreeView->setCurrentIndex(m_pTreeView->model()->index(0, g_connectionsNameColumn));
				}
			}
			else
			{
				m_pTreeView->setCurrentIndex(m_pTreeView->model()->index(0, g_connectionsNameColumn));
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
		QModelIndexList const& selectedIndexes = m_pTreeView->selectionModel()->selectedRows(g_connectionsNameColumn);

		for (auto const& index : selectedIndexes)
		{
			if (index.isValid())
			{
				ControlId const id = static_cast<ControlId>(index.data(static_cast<int>(ModelUtils::ERoles::Id)).toInt());
				IConnection const* const pIConnection = m_pControl->GetConnection(id);

				if ((pIConnection != nullptr) && pIConnection->HasProperties())
				{
					serializers.emplace_back(*pIConnection);
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
	QModelIndexList const& selectedIndexes = m_pTreeView->selectionModel()->selectedRows(g_connectionsNameColumn);

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
	m_pTreeView->resizeColumnToContents(g_connectionsNameColumn);
	m_pTreeView->resizeColumnToContents(static_cast<int>(CConnectionsModel::EColumns::Path));
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::OnBeforeReload()
{
	m_pTreeView->BackupSelection();
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::OnAfterReload()
{
	m_pTreeView->RestoreSelection();
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::OnFileImporterOpened()
{
	m_pTreeView->setDragDropMode(QAbstractItemView::NoDragDrop);
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::OnFileImporterClosed()
{
	m_pTreeView->setDragDropMode(QAbstractItemView::DropOnly);
}
} // namespace ACE
