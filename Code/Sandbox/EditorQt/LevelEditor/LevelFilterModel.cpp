// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "LevelFilterModel.h"

#include "LevelFileUtils.h"

#include <QFileSystemModel>
#include <CryIcon.h>

static const QString kLevelFileIconPath = QStringLiteral("icons:FileType/Level.ico");

bool CLevelFilterModel::CPathIsLevelCache::IsLevel(const QString& path)
{
	const auto it = m_cache.find(path);
	if (it == m_cache.end())
	{
		const auto result = LevelFileUtils::IsPathToLevel(path);
		m_cache[path] = result;
		return result;
	}
	return it.value();
}

void CLevelFilterModel::CPathIsLevelCache::Reset()
{
	m_cache.clear();
}

CLevelFilterModel::CLevelFilterModel(QObject* pParent)
	: QSortFilterProxyModel(pParent)
{}

bool CLevelFilterModel::hasChildren(const QModelIndex& parent) const
{
	if (IsIndexLevel(parent))
	{
		return false;
	}
	return QSortFilterProxyModel::hasChildren(parent);
}

QVariant CLevelFilterModel::data(const QModelIndex& index, int role) const
{
	switch (role)
	{
	case Qt::DecorationRole:
		if (IsIndexLevel(index))
		{
			if (index.column() != 0)
			{
				return QSortFilterProxyModel::data(index, role);
			}
			QIcon icon = CryIcon(kLevelFileIconPath);
			return icon;
		}
		return QSortFilterProxyModel::data(index, role);

	default:
		return QSortFilterProxyModel::data(index, role);
	}
}

bool CLevelFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
	QModelIndex childIndex = sourceModel()->index(sourceRow, 0, sourceParent);
	const QString childPath = childIndex.data(QFileSystemModel::FilePathRole).toString();
	
	LevelFileUtils::AbsolutePath basePath = LevelFileUtils::GetUserBasePath();

	// We have to perform this check as filterAcceptsRow will step back before the root path, to the root of the drive (and even other drives if we skip this check)
	if (!childPath.startsWith(basePath) || childPath == basePath)
	{
		return true;
	}

	return IsIndexLevelOrContainsLevels(childIndex, childPath);
}

bool CLevelFilterModel::IsIndexLevel(const QModelIndex& index) const
{
	const QString path = index.data(QFileSystemModel::FilePathRole).toString();
	return !path.isEmpty() && m_cache.IsLevel(path);
}

bool CLevelFilterModel::IsIndexLevelOrContainsLevels(const QModelIndex& index, const QString& path) const
{
	// Check if this folder is a level
	if (!path.isEmpty() && m_cache.IsLevel(path))
	{
		return true;
	}

	sourceModel()->fetchMore(index);

	// Check if any sub-folders contain levels
	for (int i = 0, n = sourceModel()->rowCount(index); i < n; ++i)
	{
		QModelIndex childIndex = sourceModel()->index(i, 0, index);
		const QString childPath = childIndex.data(QFileSystemModel::FilePathRole).toString();

		if (childIndex.isValid() && IsIndexLevelOrContainsLevels(childIndex, childPath))
		{
			return true;
		}
	}

	return false;
}
