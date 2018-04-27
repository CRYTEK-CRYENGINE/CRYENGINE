// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "QtCommon.h"

#include <QAbstractProxyModel>

// See also QAdvancedTreeView::ToOriginal()
QModelIndex GetSourceModelIndex(const QModelIndex& index)
{
	const QAbstractItemModel* model = index.model();
	QModelIndex original = index;

	while (const QAbstractProxyModel* proxy = qobject_cast<const QAbstractProxyModel*>(model))
	{
		original = proxy->mapToSource(original);
		model = proxy->sourceModel();
	}

	return original;
}

