// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioAdvancedTreeView.h"

namespace ACE
{
bool CAudioAdvancedTreeView::IsEditing() const
{
	return state() == QAbstractItemView::EditingState;
}

void CAudioAdvancedTreeView::ExpandSelection(QModelIndexList const& indexList)
{
	for (auto const& index : indexList)
	{
		if (index.isValid())
		{
			int const childCount = index.model()->rowCount(index);

			for (int i = 0; i < childCount; ++i)
			{
				QModelIndexList childList;
				childList.append(index.child(i, 0));
				ExpandSelection(childList);
			}

			expand(index);
		}
	}
}

void CAudioAdvancedTreeView::CollapseSelection(QModelIndexList const& indexList)
{
	for (auto const& index : indexList)
	{
		if (index.isValid())
		{
			int const childCount = index.model()->rowCount(index);

			for (int i = 0; i < childCount; ++i)
			{
				QModelIndexList childList;
				childList.append(index.child(i, 0));
				CollapseSelection(childList);
			}

			collapse(index);
		}
	}
}
} // namespace ACE
