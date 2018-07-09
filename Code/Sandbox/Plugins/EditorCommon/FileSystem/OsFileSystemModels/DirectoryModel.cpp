// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "DirectoryModel.h"

#include "AdvancedFileSystemModel.h"

CDirectoriesOnlyProxyModel::CDirectoriesOnlyProxyModel(QObject* parent)
	: QSortFilterProxyModel(parent)
{}

bool CDirectoriesOnlyProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
	auto fileSystem = static_cast<CAdvancedFileSystemModel*>(sourceModel());
	auto sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
	return fileSystem->isDir(sourceIndex);
}

void CDirectoriesOnlyProxyModel::setSourceModel(CAdvancedFileSystemModel* sourceFileSystem)
{
	QSortFilterProxyModel::setSourceModel(sourceFileSystem);
}

