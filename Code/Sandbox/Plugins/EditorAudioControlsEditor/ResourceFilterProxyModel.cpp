// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ResourceFilterProxyModel.h"

#include "SystemSourceModel.h"
#include "Control.h"
#include "Common/ModelUtils.h"

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
bool CResourceFilterProxyModel::rowMatchesFilter(int sourceRow, QModelIndex const& sourceParent) const
{
	bool matchesFilter = false;

	if (QDeepFilterProxyModel::rowMatchesFilter(sourceRow, sourceParent))
	{
		QModelIndex const& index = sourceModel()->index(sourceRow, 0, sourceParent);

		if (index.isValid())
		{
			auto const pControl = static_cast<CControl const*>(CSystemSourceModel::GetAssetFromIndex(index, 0));

			if ((pControl != nullptr) && ((pControl->GetFlags() & EAssetFlags::IsHiddenInResourceSelector) == 0))
			{
				CryAudio::ContextId const contextId = pControl->GetContextId();
				matchesFilter =
					(pControl->GetType() == m_type) &&
					((contextId == CryAudio::GlobalContextId) ||
					 (std::find(g_activeUserDefinedContexts.begin(), g_activeUserDefinedContexts.end(), contextId) != g_activeUserDefinedContexts.end()));
			}
		}
	}

	return matchesFilter;
}

//////////////////////////////////////////////////////////////////////////
bool CResourceFilterProxyModel::lessThan(QModelIndex const& left, QModelIndex const& right) const
{
	bool isLessThan = false;

	// First sort by type, then by name.
	if (left.column() == right.column())
	{
		int const sortPriorityRole = static_cast<int>(ModelUtils::ERoles::SortPriority);
		EAssetType const leftType = static_cast<EAssetType>(sourceModel()->data(left, sortPriorityRole).toInt());
		EAssetType const rightType = static_cast<EAssetType>(sourceModel()->data(right, sortPriorityRole).toInt());

		if (leftType != rightType)
		{
			isLessThan = rightType < EAssetType::Folder;
		}
		else
		{
			QVariant const& valueLeft = sourceModel()->data(left, Qt::DisplayRole);
			QVariant const& valueRight = sourceModel()->data(right, Qt::DisplayRole);
			isLessThan = valueLeft < valueRight;
		}
	}
	else
	{
		isLessThan = QSortFilterProxyModel::lessThan(left, right);
	}

	return isLessThan;
}
} // namespace ACE
