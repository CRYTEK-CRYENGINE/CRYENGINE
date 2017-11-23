// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ConnectionsWidget.h"

#include "SystemAssets.h"
#include "AudioControlsEditorPlugin.h"
#include <IEditorImpl.h>
#include <ImplItem.h>
#include "ImplementationManager.h"
#include "MiddlewareDataWidget.h"
#include "MiddlewareDataModel.h"
#include "AudioTreeView.h"
#include "ConnectionsModel.h"

#include <IEditor.h>
#include <QtUtil.h>
#include <IUndoObject.h>
#include <Controls/QuestionDialog.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/STL.h>
#include <Serialization/QPropertyTree/QPropertyTree.h>
#include <ProxyModels/DeepFilterProxyModel.h>

#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QSplitter>
#include <QVBoxLayout>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CConnectionsWidget::CConnectionsWidget(QWidget* pParent)
	: QWidget(pParent)
	, m_pControl(nullptr)
	, m_pConnectionModel(new CConnectionModel())
	, m_pFilterProxyModel(new QDeepFilterProxyModel())
	, m_pConnectionProperties(new QPropertyTree())
	, m_pTreeView(new CAudioTreeView())
{
	m_pFilterProxyModel->setSourceModel(m_pConnectionModel);

	m_pTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_pTreeView->setDragEnabled(false);
	m_pTreeView->setAcceptDrops(true);
	m_pTreeView->setDragDropMode(QAbstractItemView::DropOnly);
	m_pTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	m_pTreeView->setModel(m_pFilterProxyModel);
	m_pTreeView->sortByColumn(0, Qt::AscendingOrder);
	m_pTreeView->setIndentation(0);
	m_pTreeView->installEventFilter(this);
	m_pTreeView->header()->setMinimumSectionSize(50);
	
	QObject::connect(m_pTreeView, &CAudioTreeView::customContextMenuRequested, this, &CConnectionsWidget::OnContextMenu);
	QObject::connect(m_pTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CConnectionsWidget::RefreshConnectionProperties);

	QSplitter* const pSplitter = new QSplitter(Qt::Vertical);
	pSplitter->addWidget(m_pTreeView);
	pSplitter->addWidget(m_pConnectionProperties);
	pSplitter->setCollapsible(0, false);
	pSplitter->setCollapsible(1, false);

	QVBoxLayout* const pMainLayout = new QVBoxLayout();
	pMainLayout->setContentsMargins(0, 0, 0, 0);
	pMainLayout->addWidget(pSplitter);
	setLayout(pMainLayout);

	// First hide all the columns except "Name" and "Path"
	int const count = m_pConnectionModel->columnCount(QModelIndex());

	for (int i = 2; i < count; ++i)
	{
		m_pTreeView->SetColumnVisible(i, false);
	}

	// Then hide the entire widget.
	setHidden(true);

	CAudioControlsEditorPlugin::GetAssetsManager()->signalConnectionRemoved.Connect([&](CSystemControl* pControl)
		{
			if (m_pControl == pControl)
			{
			  // clear the selection if a connection is removed
			  m_pTreeView->selectionModel()->clear();
			  RefreshConnectionProperties();
			}
		}, reinterpret_cast<uintptr_t>(this));

	CAudioControlsEditorPlugin::GetImplementationManger()->signalImplementationAboutToChange.Connect([&]()
		{
			m_pTreeView->selectionModel()->clear();
			RefreshConnectionProperties();
		}, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
CConnectionsWidget::~CConnectionsWidget()
{
	CAudioControlsEditorPlugin::GetAssetsManager()->signalConnectionRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	CAudioControlsEditorPlugin::GetImplementationManger()->signalImplementationAboutToChange.DisconnectById(reinterpret_cast<uintptr_t>(this));
	delete m_pConnectionModel;
}

//////////////////////////////////////////////////////////////////////////
bool CConnectionsWidget::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if (pEvent->type() == QEvent::KeyPress)
	{
		QKeyEvent const* const pKeyEvent = static_cast<QKeyEvent*>(pEvent);

		if ((pKeyEvent != nullptr) && (pKeyEvent->key() == Qt::Key_Delete) && (pObject == m_pTreeView))
		{
			RemoveSelectedConnection();
			return true;
		}
	}

	return QWidget::eventFilter(pObject, pEvent);
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::OnContextMenu(QPoint const& pos)
{
	int const selectionCount = m_pTreeView->selectionModel()->selectedRows().count();

	if (selectionCount > 0)
	{
		QMenu* const pContextMenu = new QMenu();

		char const* actionName = "Remove Connection";

		if (selectionCount > 1)
		{
			actionName = "Remove Connections";
		}

		pContextMenu->addAction(tr(actionName), [&]() { RemoveSelectedConnection(); });
		pContextMenu->exec(QCursor::pos());
	}
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::RemoveSelectedConnection()
{
	if (m_pControl != nullptr)
	{
		CQuestionDialog* const messageBox = new CQuestionDialog();
		QModelIndexList const& selectedIndices = m_pTreeView->selectionModel()->selectedRows();

		if (!selectedIndices.empty())
		{
			int const size = selectedIndices.length();
			QString text;

			if (size == 1)
			{
				text = R"(Are you sure you want to delete the connection between ")" + QtUtil::ToQString(m_pControl->GetName()) + R"(" and ")" + selectedIndices[0].data(Qt::DisplayRole).toString() + R"("?)";
			}
			else
			{
				text = "Are you sure you want to delete the " + QString::number(size) + " selected connections?";
			}

			messageBox->SetupQuestion("Audio Controls Editor", text);

			if (messageBox->Execute() == QDialogButtonBox::Yes)
			{
				CUndo undo("Disconnected Audio Control from Audio System");
				IEditorImpl const* const pEditorImpl = CAudioControlsEditorPlugin::GetImplEditor();

				if (pEditorImpl != nullptr)
				{
					std::vector<CImplItem*> implItems;
					implItems.reserve(selectedIndices.size());

					for (QModelIndex const& index : selectedIndices)
					{
						CID const id = index.data(static_cast<int>(CConnectionModel::EConnectionModelRoles::Id)).toInt();
						implItems.emplace_back(pEditorImpl->GetControl(id));
					}

					for (CImplItem* const pImplItem : implItems)
					{
						if (pImplItem != nullptr)
						{
							m_pControl->RemoveConnection(pImplItem);
						}
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::SetControl(CSystemControl* pControl)
{
	if (m_pControl != pControl)
	{
		m_pControl = pControl;
		Reload();
	}
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::Reload()
{
	m_pConnectionModel->Init(m_pControl);
	m_pTreeView->selectionModel()->clear();
	m_pTreeView->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	RefreshConnectionProperties();
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::RefreshConnectionProperties()
{
	ConnectionPtr pConnection;

	if (m_pControl != nullptr)
	{
		QModelIndexList const& selectedIndices = m_pTreeView->selectionModel()->selectedIndexes();

		if (!selectedIndices.empty())
		{
			QModelIndex const& index = selectedIndices[0];

			if (index.isValid())
			{
				CID const id = index.data(static_cast<int>(CConnectionModel::EConnectionModelRoles::Id)).toInt();
				pConnection = m_pControl->GetConnection(id);
			}
		}
	}

	if ((pConnection != nullptr) && pConnection->HasProperties())
	{
		m_pConnectionProperties->attach(Serialization::SStruct(*pConnection.get()));
		m_pConnectionProperties->setHidden(false);
	}
	else
	{
		m_pConnectionProperties->detach();
		m_pConnectionProperties->setHidden(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::BackupTreeViewStates()
{
	m_pTreeView->BackupSelection();
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsWidget::RestoreTreeViewStates()
{
	m_pTreeView->RestoreSelection();
}
} // namespace ACE
