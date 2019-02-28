// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SystemFilterProxyModel.h"

#include "Asset.h"
#include "SystemSourceModel.h"

#include <ModelUtils.h>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CSystemFilterProxyModel::CSystemFilterProxyModel(QObject* const pParent)
	: QAttributeFilterProxyModel(QDeepFilterProxyModel::Behavior::AcceptIfChildMatches, pParent)
{
}

//////////////////////////////////////////////////////////////////////////
bool CSystemFilterProxyModel::rowMatchesFilter(int sourceRow, QModelIndex const& sourceParent) const
{
	bool matchesFilter = false;

	if (QAttributeFilterProxyModel::rowMatchesFilter(sourceRow, sourceParent))
	{
		QModelIndex const& index = sourceModel()->index(sourceRow, 0, sourceParent);

		if (index.isValid())
		{
			CAsset const* const pAsset = CSystemSourceModel::GetAssetFromIndex(index, static_cast<int>(CSystemSourceModel::EColumns::Name));
			CRY_ASSERT_MESSAGE(pAsset != nullptr, "Asset is null pointer.");

			// Hide internal controls.
			matchesFilter = (pAsset->GetFlags() & EAssetFlags::IsInternalControl) == 0;
		}
	}

	return matchesFilter;
}

//////////////////////////////////////////////////////////////////////////
bool CSystemFilterProxyModel::lessThan(QModelIndex const& left, QModelIndex const& right) const
{
	bool isLessThan = false;

	// First sort by type, then by name.
	if (left.column() == right.column())
	{
		auto const sortPriorityRole = static_cast<int>(ModelUtils::ERoles::SortPriority);
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
