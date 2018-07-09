// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FilterProxyModel.h"

#include <ModelUtils.h>

namespace ACE
{
namespace Impl
{
namespace SDLMixer
{
//////////////////////////////////////////////////////////////////////////
CFilterProxyModel::CFilterProxyModel(QObject* const pParent)
	: QAttributeFilterProxyModel(QDeepFilterProxyModel::Behavior::AcceptIfChildMatches, pParent)
{
}

//////////////////////////////////////////////////////////////////////////
bool CFilterProxyModel::rowMatchesFilter(int sourceRow, QModelIndex const& sourceParent) const
{
	bool matchesFilter = QAttributeFilterProxyModel::rowMatchesFilter(sourceRow, sourceParent);

	if (matchesFilter)
	{
		QModelIndex const& index = sourceModel()->index(sourceRow, filterKeyColumn(), sourceParent);

		if (index.isValid())
		{
			// Hide placeholder.
			matchesFilter = !sourceModel()->data(index, static_cast<int>(ModelUtils::ERoles::IsPlaceholder)).toBool();
		}
	}

	return matchesFilter;
}

//////////////////////////////////////////////////////////////////////////
bool CFilterProxyModel::lessThan(QModelIndex const& left, QModelIndex const& right) const
{
	bool isLessThan = false;

	// First sort by type, then by name.
	if (left.column() == right.column())
	{
		auto const sortPriorityRole = static_cast<int>(ModelUtils::ERoles::SortPriority);
		int const leftPriority = sourceModel()->data(left, sortPriorityRole).toInt();
		int const rightPriority = sourceModel()->data(right, sortPriorityRole).toInt();

		if (leftPriority != rightPriority)
		{
			isLessThan = leftPriority > rightPriority;
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
} // namespace SDLMixer
} // namespace Impl
} // namespace ACE
