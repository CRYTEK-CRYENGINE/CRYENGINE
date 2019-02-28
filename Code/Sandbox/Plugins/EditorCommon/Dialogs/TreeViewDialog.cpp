// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "StdAfx.h"
#include "TreeViewDialog.h"

#include "ProxyModels/DeepFilterProxyModel.h"

#include "QSearchBox.h"
#include "QAdvancedTreeView.h"

#include <QAbstractItemModel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSpacerItem>
#include <QString>

void CTreeViewDialog::Initialize(QAbstractItemModel* pModel, int32 filterColumn, const std::vector<int32>& visibleColumns)
{
	QDeepFilterProxyModel::BehaviorFlags behavior = QDeepFilterProxyModel::AcceptIfChildMatches | QDeepFilterProxyModel::AcceptIfParentMatches;
	QDeepFilterProxyModel* pProxy = new QDeepFilterProxyModel(behavior);
	pProxy->setSourceModel(pModel);
	pProxy->setFilterKeyColumn(filterColumn);
	
	m_pSearchBox = new QSearchBox();
	m_pSearchBox->SetModel(pProxy);
	m_pSearchBox->EnableContinuousSearch(true);

	m_pTreeView = new QAdvancedTreeView();
	m_pTreeView->setModel(pProxy);
	m_pTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_pTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
	m_pTreeView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_pTreeView->setDragDropMode(QAbstractItemView::NoDragDrop);

	m_pSearchBox->SetAutoExpandOnSearch(m_pTreeView);

	const int32 count = pProxy->columnCount(QModelIndex());
	for (int32 i = 1; i < count; ++i)
	{
		m_pTreeView->SetColumnVisible(i, false);
	}
	for (int32 i : visibleColumns)
	{
		m_pTreeView->SetColumnVisible(i, true);
	}

	QPushButton* pOkButton = new QPushButton(this);
	pOkButton->setText(tr("Ok"));
	pOkButton->setDefault(true);
	QObject::connect(pOkButton, &QPushButton::clicked, this, &CTreeViewDialog::OnOk);

	QPushButton* pCancelButton = new QPushButton(this);
	pCancelButton->setText(tr("Cancel"));
	QObject::connect(pCancelButton, &QPushButton::clicked, [this]()
	{
		reject();
	});

	QHBoxLayout* pButtonsLayout = new QHBoxLayout();
	pButtonsLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Preferred));
	pButtonsLayout->addWidget(pOkButton);
	pButtonsLayout->addWidget(pCancelButton);

	QVBoxLayout* pMainLayout = new QVBoxLayout();
	pMainLayout->addWidget(m_pSearchBox);
	pMainLayout->addWidget(m_pTreeView);
	pMainLayout->addLayout(pButtonsLayout);

	setLayout(pMainLayout);

	QObject::connect(m_pTreeView, &QAdvancedTreeView::doubleClicked, [this]()
	{
		accept();
	});
	m_pTreeView->sortByColumn(filterColumn, Qt::AscendingOrder);
}

void CTreeViewDialog::SetSelectedValue(const QModelIndex& index, bool bExpand)
{
	if (index.isValid())
	{
		m_pTreeView->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		m_pTreeView->scrollTo(index);

		if (bExpand)
		{
			m_pTreeView->expand(index.parent());
		}
	}
}

QModelIndex CTreeViewDialog::GetSelected()
{
	return m_pTreeView->selectionModel()->currentIndex();
}

void CTreeViewDialog::OnOk()
{
	accept();
}

void CTreeViewDialog::showEvent(QShowEvent* event)
{
	CEditorDialog::showEvent(event);
	m_pSearchBox->setFocus();
}

