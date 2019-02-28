// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Qt>

#include <QAbstractItemModel>

enum EItemDataRole
{
	// Returns variant of type void* which points to a Serialization::SStruct.
	// The caller of data() is responsible for deletion.
	eItemDataRole_YasliSStruct = Qt::UserRole,

	eItemDataRole_MAX
};

QModelIndex GetSourceModelIndex(const QModelIndex& index);

