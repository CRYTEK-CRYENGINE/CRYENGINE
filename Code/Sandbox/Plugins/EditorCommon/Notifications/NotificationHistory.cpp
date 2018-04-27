// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

#include "NotificationHistory.h"
#include "Internal/Notifications_Internal.h"
#include "Internal/NotificationModel_Internal.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <QHeaderView>

// EditorCommon
#include "QAdvancedTreeView.h"
#include "CryIcon.h"
#include "EditorFramework/PersonalizationManager.h"

CNotificationHistory::CNotificationHistory(QWidget* pParent /* = nullptr*/)
	: QWidget(pParent)
{
	QVBoxLayout* pMainLayout = new QVBoxLayout();
	pMainLayout->setMargin(0);
	pMainLayout->setSpacing(0);

	QHBoxLayout* pButtonLayout = new QHBoxLayout();
	pButtonLayout->setMargin(0);
	pButtonLayout->setSpacing(0);
	pMainLayout->addLayout(pButtonLayout);

	QToolButton* pClearAll = new QToolButton();
	pClearAll->setIcon(CryIcon("icons:General/Element_Clear.ico"));
	pClearAll->setToolTip("Clear all");
	pButtonLayout->addWidget(pClearAll, Qt::AlignLeft);

	QToolButton* pShowCompleteHistory = new QToolButton();
	pShowCompleteHistory->setIcon(CryIcon("icons:Dialogs/show_log.ico"));
	pShowCompleteHistory->setToolTip("Show complete history");
	pButtonLayout->addWidget(pShowCompleteHistory, Qt::AlignLeft);

	QToolButton* pCopyHistory = new QToolButton();
	pCopyHistory->setIcon(CryIcon("icons:General/Copy.ico"));
	pCopyHistory->setToolTip("Copy history to clipboard");
	pButtonLayout->addWidget(pCopyHistory, Qt::AlignLeft);

	pButtonLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

	m_pShowInfo = new QToolButton();
	m_pShowInfo->setIcon(CryIcon("icons:Dialogs/dialog-question.ico"));
	m_pShowInfo->setToolTip("Show info");
	m_pShowInfo->setCheckable(true);
	m_pShowInfo->setChecked(true);
	pButtonLayout->addWidget(m_pShowInfo, Qt::AlignRight);

	m_pShowWarnings = new QToolButton();
	m_pShowWarnings->setIcon(CryIcon("icons:Dialogs/dialog-warning.ico"));
	m_pShowWarnings->setToolTip("Show warnings");
	m_pShowWarnings->setCheckable(true);
	m_pShowWarnings->setChecked(true);
	pButtonLayout->addWidget(m_pShowWarnings, Qt::AlignRight);

	m_pShowErrors = new QToolButton();
	m_pShowErrors->setIcon(CryIcon("icons:Dialogs/dialog-error.ico"));
	m_pShowErrors->setToolTip("Show errors");
	m_pShowErrors->setCheckable(true);
	m_pShowErrors->setChecked(true);
	pButtonLayout->addWidget(m_pShowErrors, Qt::AlignRight);

	m_pTreeView = new QAdvancedTreeView();
	m_pTreeView->setSortingEnabled(true);
	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	m_pTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pNotificationModel = new CNotificationModel();
	m_pFilterModel = new CNotificationFilterModel();
	m_pFilterModel->setSourceModel(m_pNotificationModel);
	m_pFilterModel->setSortRole(static_cast<int>(CNotificationModel::Roles::SortRole));
	m_pFilterModel->SetFilterTypeState(Internal::CNotification::Progress, false);
	m_pTreeView->setModel(m_pFilterModel);
	pMainLayout->addWidget(m_pTreeView);

	int columnCount = m_pNotificationModel->columnCount();
	for (int i = 0; i < columnCount; ++i)
	{
		QVariant columnVariant = m_pNotificationModel->headerData(i, Qt::Horizontal, Qt::DisplayRole);
		if (columnVariant.toString() == "Title")
			m_pTreeView->SetColumnVisible(i, false);
	}

	m_pTreeView->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	m_pTreeView->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

	connect(m_pShowInfo, &QToolButton::clicked, [this](bool bChecked)
	{
		m_pFilterModel->SetFilterTypeState(Internal::CNotification::Info, bChecked);
		SaveState();
	});

	connect(m_pShowWarnings, &QToolButton::clicked, [this](bool bChecked)
	{
		m_pFilterModel->SetFilterTypeState(Internal::CNotification::Warning, bChecked);
		SaveState();
	});

	connect(m_pShowErrors, &QToolButton::clicked, [this](bool bChecked)
	{
		m_pFilterModel->SetFilterTypeState(Internal::CNotification::Critical, bChecked);
		SaveState();
	});

	connect(pClearAll, &QToolButton::clicked, m_pNotificationModel, &CNotificationModel::ClearAll);
	connect(pShowCompleteHistory, &QToolButton::clicked, m_pNotificationModel, &CNotificationModel::ShowCompleteHistory);
	connect(pCopyHistory, &QToolButton::clicked, m_pNotificationModel, static_cast<void (CNotificationModel::*)()const>(&CNotificationModel::CopyHistory));
	connect(m_pTreeView, &QTreeView::customContextMenuRequested, this, &CNotificationHistory::OnContextMenu);
	connect(m_pTreeView, &QTreeView::doubleClicked, this, &CNotificationHistory::OnDoubleClicked);
	connect(m_pNotificationModel, &QAbstractItemModel::rowsInserted, this, &CNotificationHistory::OnNotificationsAdded);
	connect(m_pTreeView->header(), &QHeaderView::sortIndicatorChanged, this, &CNotificationHistory::SaveState);

	LoadState();

	setLayout(pMainLayout);
}

void CNotificationHistory::OnNotificationsAdded()
{
	QModelIndex currentIndex =  m_pTreeView->selectionModel()->currentIndex();
	if (currentIndex.isValid())
	{
		m_pTreeView->scrollTo(currentIndex);
	}
}

void CNotificationHistory::OnContextMenu(const QPoint& pos) const
{
	QMenu* menu = new QMenu();
	QAction* action = menu->addAction(CryIcon("icons:General/Copy.ico"), tr("Copy"));
	connect(action, &QAction::triggered, [this]()
	{
		QModelIndexList sourceRows;
		QModelIndexList selection = m_pTreeView->selectionModel()->selectedRows();
		for (auto selectedIdx : selection)
		{
			sourceRows.push_back(m_pFilterModel->mapToSource(selectedIdx));
		}

		m_pNotificationModel->CopyHistory(sourceRows);
	});
	
	menu->exec(m_pTreeView->mapToGlobal(pos));
}

void CNotificationHistory::OnDoubleClicked(const QModelIndex &index)
{
	Internal::CNotification* pNotification = m_pNotificationModel->NotificationFromIndex(m_pFilterModel->mapToSource(index));

	if (pNotification)
	{
		pNotification->Execute();
	}
}

void CNotificationHistory::LoadState()
{
	QVariant filterTypeVar = GET_PERSONALIZATION_PROPERTY(CNotificationHistory, "ShowInfo");
	if (filterTypeVar.isValid())
	{
		m_pShowInfo->setChecked(filterTypeVar.toBool());
		m_pFilterModel->SetFilterTypeState(Internal::CNotification::Info, m_pShowInfo->isChecked());
	}

	filterTypeVar = GET_PERSONALIZATION_PROPERTY(CNotificationHistory, "ShowWarnings");
	if (filterTypeVar.isValid())
	{
		m_pShowWarnings->setChecked(filterTypeVar.toBool());
		m_pFilterModel->SetFilterTypeState(Internal::CNotification::Warning, m_pShowWarnings->isChecked());
	}

	filterTypeVar = GET_PERSONALIZATION_PROPERTY(CNotificationHistory, "ShowErrors");
	if (filterTypeVar.isValid())
	{
		m_pShowErrors->setChecked(filterTypeVar.toBool());
		m_pFilterModel->SetFilterTypeState(Internal::CNotification::Critical, m_pShowErrors->isChecked());
	}

	QVariant treeViewVar = GET_PERSONALIZATION_PROPERTY(CNotificationHistory, "TreeView");
	if (treeViewVar.isValid() && treeViewVar.type() == QVariant::Map)
		m_pTreeView->SetState(treeViewVar.toMap());
}

void CNotificationHistory::SaveState()
{
	SET_PERSONALIZATION_PROPERTY(CNotificationHistory, "ShowInfo", m_pShowInfo->isChecked());
	SET_PERSONALIZATION_PROPERTY(CNotificationHistory, "ShowWarnings", m_pShowWarnings->isChecked());
	SET_PERSONALIZATION_PROPERTY(CNotificationHistory, "ShowErrors", m_pShowErrors->isChecked());
	SET_PERSONALIZATION_PROPERTY(CNotificationHistory, "TreeView", m_pTreeView->GetState());
}

