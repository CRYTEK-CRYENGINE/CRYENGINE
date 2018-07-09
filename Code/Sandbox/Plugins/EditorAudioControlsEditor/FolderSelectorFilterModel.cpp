// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FolderSelectorFilterModel.h"

#include <FileSystem/OsFileSystemModels/AdvancedFileSystemModel.h>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CFolderSelectorFilterModel::CFolderSelectorFilterModel(QString const& assetpath, QObject* const pParent)
	: QDeepFilterProxyModel(QDeepFilterProxyModel::Behavior::AcceptIfChildMatches, pParent)
	, m_assetPath(assetpath)
{
}

//////////////////////////////////////////////////////////////////////////
bool CFolderSelectorFilterModel::rowMatchesFilter(int sourceRow, QModelIndex const& sourceParent) const
{
	bool matchesFilter = false;

	if (QDeepFilterProxyModel::rowMatchesFilter(sourceRow, sourceParent))
	{
		QModelIndex const& index = sourceModel()->index(sourceRow, 0, sourceParent);

		if (index.isValid())
		{
			QString const& filePath = index.data(QFileSystemModel::Roles::FilePathRole).toString();

			if (filePath.startsWith(m_assetPath + "/", Qt::CaseInsensitive) || (filePath.compare(m_assetPath, Qt::CaseInsensitive) == 0))
			{
				matchesFilter = true;
			}
		}
	}

	return matchesFilter;
}
} // namespace ACE
