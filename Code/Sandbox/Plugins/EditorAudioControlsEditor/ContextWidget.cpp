// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ContextWidget.h"

#include "ContextModel.h"
#include "Context.h"
#include "ContextManager.h"
#include "AssetsManager.h"
#include "ImplManager.h"
#include "SystemControlsWidget.h"
#include "TreeView.h"

#include <QtUtil.h>
#include <QSearchBox.h>
#include <Controls/QuestionDialog.h>
#include <ProxyModels/AttributeFilterProxyModel.h>

#include <QAction>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

namespace ACE
{
constexpr int g_contextNameColumn = static_cast<int>(CContextModel::EColumns::Name);

//////////////////////////////////////////////////////////////////////////
CContextWidget::CContextWidget(QWidget* const pParent)
	: QWidget(pParent)
	, m_pContextModel(new CContextModel(this))
	, m_pAttributeFilterProxyModel(new QAttributeFilterProxyModel(QAttributeFilterProxyModel::BaseBehavior, this))
	, m_pTreeView(new CTreeView(this))
	, m_pSearchBox(new QSearchBox(this))
{
	m_pAttributeFilterProxyModel->setSourceModel(m_pContextModel);
	m_pAttributeFilterProxyModel->setFilterKeyColumn(g_contextNameColumn);

	m_pTreeView->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
	m_pTreeView->setDragEnabled(false);
	m_pTreeView->setDragDropMode(QAbstractItemView::NoDragDrop);
	m_pTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_pTreeView->setUniformRowHeights(true);
	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	m_pTreeView->setModel(m_pAttributeFilterProxyModel);
	m_pTreeView->sortByColumn(g_contextNameColumn, Qt::AscendingOrder);
	m_pTreeView->viewport()->installEventFilter(this);
	m_pTreeView->installEventFilter(this);
	m_pTreeView->setItemsExpandable(false);
	m_pTreeView->setRootIsDecorated(false);
	m_pTreeView->header()->setMinimumSectionSize(25);
	m_pTreeView->header()->setSectionResizeMode(static_cast<int>(CContextModel::EColumns::Notification), QHeaderView::ResizeToContents);
	m_pTreeView->SetNameColumn(g_contextNameColumn);
	m_pTreeView->TriggerRefreshHeaderColumns();

	QObject::connect(m_pContextModel, &CContextModel::rowsInserted, this, &CContextWidget::SelectNewContext);
	QObject::connect(m_pTreeView, &CTreeView::customContextMenuRequested, this, &CContextWidget::OnContextMenu);
	QObject::connect(m_pTreeView, &CTreeView::doubleClicked, this, &CContextWidget::OnItemDoubleClicked);

	m_pSearchBox->SetModel(m_pAttributeFilterProxyModel);
	m_pSearchBox->EnableContinuousSearch(true);

	QPushButton* const pAddButton = new QPushButton(tr("Add"), this);
	pAddButton->setToolTip(tr("Add new context"));
	QObject::connect(pAddButton, &QPushButton::clicked, [&]() { AddContext(); });

	auto const pMainLayout = new QVBoxLayout(this);
	setLayout(pMainLayout);
	pMainLayout->setContentsMargins(0, 0, 0, 0);
	pMainLayout->addWidget(pAddButton);
	pMainLayout->addWidget(m_pSearchBox);
	pMainLayout->addWidget(m_pTreeView);

	if (g_pIImpl == nullptr)
	{
		setEnabled(false);
	}

	g_contextManager.SignalOnAfterContextRemoved.Connect([this]()
		{
			if (!g_assetsManager.IsLoading())
			{
			  m_pTreeView->selectionModel()->clear();
			}
		}, reinterpret_cast<uintptr_t>(this));

	g_implManager.SignalOnAfterImplChange.Connect([this]()
		{
			setEnabled(g_pIImpl != nullptr);
		}, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
CContextWidget::~CContextWidget()
{
	g_contextManager.SignalOnAfterContextRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_implManager.SignalOnAfterImplChange.DisconnectById(reinterpret_cast<uintptr_t>(this));

	m_pContextModel->DisconnectSignals();
	m_pContextModel->deleteLater();
}

//////////////////////////////////////////////////////////////////////////
void CContextWidget::OnContextMenu(QPoint const& pos)
{
	QModelIndexList const& selection = m_pTreeView->selectionModel()->selectedRows();
	auto const pContextMenu = new QMenu(this);
	int const numSelections = selection.size();

	if (numSelections == 1)
	{
		QModelIndex const& index = m_pTreeView->currentIndex();

		if (index.isValid())
		{
			CContext const* const pContext = CContextModel::GetContextFromIndex(index);

			if ((pContext != nullptr) && (pContext->GetId() != CryAudio::GlobalContextId))
			{
				if (pContext->IsActive())
				{
					pContextMenu->addAction(tr("Deactivate"), [&]() { DeactivateContext(pContext->GetId()); });
				}
				else
				{
					QAction* const pAction = pContextMenu->addAction(tr("Activate"), [&]() { ActivateContext(pContext->GetId()); });

					if (!pContext->IsRegistered())
					{
						pAction->setEnabled(false);
					}
				}

				pContextMenu->addSeparator();
				pContextMenu->addAction(tr("Rename"), [&]() { RenameContext(); });
				pContextMenu->addAction(tr("Delete"), [&]() { DeleteContext(pContext); });
			}
		}
	}
	else if (numSelections > 1)
	{
		pContextMenu->addAction(tr("Activate"), [&]() { ActivateMultipleContexts(); });
		pContextMenu->addAction(tr("Deactivate"), [&]() { DeactivateMultipleContexts(); });
		pContextMenu->addSeparator();
		pContextMenu->addAction(tr("Delete"), [&]() { DeleteMultipleContexts(); });
	}
	else
	{
		pContextMenu->addAction(tr("Add Context"), [&]() { AddContext(); });
	}

	pContextMenu->exec(QCursor::pos());
}

//////////////////////////////////////////////////////////////////////////
void CContextWidget::OnItemDoubleClicked(QModelIndex const& index)
{
	CContext const* const pContext = CContextModel::GetContextFromIndex(index);

	if ((pContext != nullptr) && (pContext->GetId() != CryAudio::GlobalContextId))
	{
		if (pContext->IsActive())
		{
			gEnv->pAudioSystem->DeactivateContext(pContext->GetId());
		}
		else if (pContext->IsRegistered())
		{
			gEnv->pAudioSystem->ActivateContext(pContext->GetId());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CContextWidget::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if (pEvent->type() == QEvent::KeyRelease)
	{
		QKeyEvent const* const pKeyEvent = static_cast<QKeyEvent*>(pEvent);

		if (pKeyEvent != nullptr)
		{
			if (pKeyEvent->key() == Qt::Key_Delete)
			{
				QModelIndexList const& selection = m_pTreeView->selectionModel()->selectedRows();
				int const numSelections = selection.size();

				if (numSelections == 1)
				{
					QModelIndex const& index = m_pTreeView->currentIndex();

					if (index.isValid())
					{
						CContext const* const pContext = CContextModel::GetContextFromIndex(index);

						if ((pContext != nullptr) && (pContext->GetId() != CryAudio::GlobalContextId))
						{
							DeleteContext(pContext);
						}
					}
				}
				else if (numSelections > 1)
				{
					DeleteMultipleContexts();
				}
			}
		}
	}

	return QWidget::eventFilter(pObject, pEvent);
}

//////////////////////////////////////////////////////////////////////////
void CContextWidget::ActivateContext(CryAudio::ContextId const contextId)
{
	gEnv->pAudioSystem->ActivateContext(contextId);
}

//////////////////////////////////////////////////////////////////////////
void CContextWidget::DeactivateContext(CryAudio::ContextId const contextId)
{
	gEnv->pAudioSystem->DeactivateContext(contextId);
}

//////////////////////////////////////////////////////////////////////////
void CContextWidget::ActivateMultipleContexts()
{
	QModelIndexList const& selection = m_pTreeView->selectionModel()->selectedRows();

	for (auto const& index : selection)
	{
		if (index.isValid())
		{
			CContext const* const pContext = CContextModel::GetContextFromIndex(index);

			if ((pContext != nullptr) &&
			    (pContext->GetId() != CryAudio::GlobalContextId) &&
			    pContext->IsRegistered() &&
			    !pContext->IsActive())
			{

				gEnv->pAudioSystem->ActivateContext(pContext->GetId());
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CContextWidget::DeactivateMultipleContexts()
{
	QModelIndexList const& selection = m_pTreeView->selectionModel()->selectedRows();

	for (auto const& index : selection)
	{
		if (index.isValid())
		{
			CContext const* const pContext = CContextModel::GetContextFromIndex(index);

			if ((pContext != nullptr) &&
			    (pContext->GetId() != CryAudio::GlobalContextId) &&
			    pContext->IsActive())
			{
				gEnv->pAudioSystem->DeactivateContext(pContext->GetId());
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CContextWidget::AddContext()
{
	g_contextManager.TryCreateContext(g_contextManager.GenerateUniqueContextName("new_context"), false);
}

//////////////////////////////////////////////////////////////////////////
void CContextWidget::RenameContext()
{
	QModelIndex const& nameColumnIndex = m_pTreeView->currentIndex().sibling(m_pTreeView->currentIndex().row(), g_contextNameColumn);
	m_pTreeView->edit(nameColumnIndex);
}

//////////////////////////////////////////////////////////////////////////
void CContextWidget::DeleteContext(CContext const* const pContext)
{
	QString const text = R"(Are you sure you want to delete the context ")" +
	                     QtUtil::ToQString(pContext->GetName()) +
	                     R"("?)" +
	                     "\nAll controls in this context will be moved to the global context!";

	auto const messageBox = new CQuestionDialog();
	messageBox->SetupQuestion(g_szEditorName, text);

	if (messageBox->Execute() == QDialogButtonBox::Yes)
	{
		CryAudio::ContextId const contextId = pContext->GetId();
		g_contextManager.DeleteContext(pContext);
		g_assetsManager.ChangeContext(contextId, CryAudio::GlobalContextId);
	}
}

//////////////////////////////////////////////////////////////////////////
void CContextWidget::DeleteMultipleContexts()
{
	QString const text = "Are you sure you want to delete the selected contexts?\nAll controls in these contexts will be moved to the global context!";

	auto const messageBox = new CQuestionDialog();
	messageBox->SetupQuestion(g_szEditorName, text);

	if (messageBox->Execute() == QDialogButtonBox::Yes)
	{
		QModelIndexList const& selection = m_pTreeView->selectionModel()->selectedRows();
		std::vector<CContext const*> contexts;
		contexts.reserve(static_cast<size_t>(selection.size()));

		for (auto const& index : selection)
		{
			if (index.isValid())
			{
				CContext const* const pContext = CContextModel::GetContextFromIndex(index);

				if ((pContext != nullptr) && (pContext->GetId() != CryAudio::GlobalContextId))
				{
					contexts.push_back(pContext);
				}
			}
		}

		for (auto const pContext : contexts)
		{
			g_assetsManager.ChangeContext(pContext->GetId(), CryAudio::GlobalContextId);
			g_contextManager.DeleteContext(pContext);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CContextWidget::SelectNewContext(QModelIndex const& parent, int const row)
{
	if (!g_assetsManager.IsLoading())
	{
		m_pSearchBox->clear();
		QModelIndex const& index = m_pAttributeFilterProxyModel->mapFromSource(m_pContextModel->index(row, g_contextNameColumn, parent));

		m_pTreeView->setCurrentIndex(index);
		m_pTreeView->edit(index);
	}
}

//////////////////////////////////////////////////////////////////////////
void CContextWidget::Reset()
{
	m_pTreeView->BackupSelection();
	m_pContextModel->Reset();
	m_pTreeView->RestoreSelection();
}

//////////////////////////////////////////////////////////////////////////
void CContextWidget::OnBeforeReload()
{
	m_pTreeView->BackupSelection();
	m_pContextModel->Reset();
}

//////////////////////////////////////////////////////////////////////////
void CContextWidget::OnAfterReload()
{
	m_pContextModel->Reset();
	m_pTreeView->RestoreSelection();
}
} // namespace ACE
