// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "VegetationTreeView.h"

#include <QItemSelectionModel>
#include <QDrag>

CVegetationTreeView::CVegetationTreeView(QWidget* pParent)
	: QAdvancedTreeView(QAdvancedTreeView::BehaviorFlags(QAdvancedTreeView::PreserveExpandedAfterReset | QAdvancedTreeView::PreserveSelectionAfterReset), pParent)
{
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setSelectionMode(QAbstractItemView::ExtendedSelection);
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	setContextMenuPolicy(Qt::CustomContextMenu);
	setDragEnabled(true);
	setAcceptDrops(true);
}

void CVegetationTreeView::startDrag(Qt::DropActions supportedActions)
{
	const auto pSelectionModel = selectionModel();
	const auto rowIndexes = pSelectionModel->selectedRows();
	if (rowIndexes.empty())
	{
		return;
	}

	auto pDrag = new QDrag(this);
	const auto pModel = model();
	QMimeData* pMimeData = pModel->mimeData(rowIndexes);
	if (!pMimeData)
	{
		return;
	}

	dragStarted(rowIndexes);
	pDrag->setMimeData(pMimeData);
	pDrag->exec(supportedActions);
}

