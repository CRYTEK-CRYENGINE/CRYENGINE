// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "QSortFilterProxyModel"
#include "EditorCommonAPI.h"

class EDITOR_COMMON_API CFileSortProxyModel
	: public QSortFilterProxyModel
{
	Q_OBJECT

public:
	explicit CFileSortProxyModel(QObject* parent = 0);

	// QSortFilterProxyModel interface
protected:
	virtual bool lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const override;
};

