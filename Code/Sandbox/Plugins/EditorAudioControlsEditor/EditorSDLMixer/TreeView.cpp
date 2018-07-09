// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TreeView.h"

#include <ModelUtils.h>
#include <CryAudio/IAudioSystem.h>

#include <QHeaderView>

namespace ACE
{
namespace Impl
{
namespace SDLMixer
{
//////////////////////////////////////////////////////////////////////////
CTreeView::CTreeView(QWidget* const pParent, QAdvancedTreeView::BehaviorFlags const flags /*= QAdvancedTreeView::BehaviorFlags(UseItemModelAttribute)*/)
	: QAdvancedTreeView(QAdvancedTreeView::BehaviorFlags(flags), pParent)
{
	QObject::connect(header(), &QHeaderView::sortIndicatorChanged, [this]() { scrollTo(currentIndex()); });
}

//////////////////////////////////////////////////////////////////////////
void CTreeView::ExpandSelection()
{
	ExpandSelectionRecursively(selectedIndexes());
}

//////////////////////////////////////////////////////////////////////////
void CTreeView::ExpandSelectionRecursively(QModelIndexList const& indexList)
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
				ExpandSelectionRecursively(childList);
			}

			expand(index);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTreeView::CollapseSelection()
{
	CollapseSelectionRecursively(selectedIndexes());
}

//////////////////////////////////////////////////////////////////////////
void CTreeView::CollapseSelectionRecursively(QModelIndexList const& indexList)
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
				CollapseSelectionRecursively(childList);
			}

			collapse(index);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
ControlId CTreeView::GetItemId(QModelIndex const& index) const
{
	auto itemId = static_cast<ControlId>(CryAudio::InvalidCRC32);

	if (index.isValid())
	{
		QModelIndex const itemIndex = index.sibling(index.row(), m_nameColumn);

		if (itemIndex.isValid())
		{
			itemId = static_cast<ControlId>(itemIndex.data(static_cast<int>(ModelUtils::ERoles::Id)).toInt());
		}
	}

	return itemId;
}

//////////////////////////////////////////////////////////////////////////
void CTreeView::BackupExpanded()
{
	m_expandedBackup.clear();

	int const rowCount = model()->rowCount();

	for (int i = 0; i < rowCount; ++i)
	{
		BackupExpandedRecursively(model()->index(i, 0));
	}
}

//////////////////////////////////////////////////////////////////////////
void CTreeView::BackupExpandedRecursively(QModelIndex const& index)
{
	if (index.isValid())
	{
		int const rowCount = model()->rowCount(index);

		if (rowCount > 0)
		{
			for (int i = 0; i < rowCount; ++i)
			{
				BackupExpandedRecursively(index.child(i, 0));
			}
		}

		if (isExpanded(index))
		{
			m_expandedBackup.insert(GetItemId(index));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTreeView::RestoreExpanded()
{
	if (!m_expandedBackup.isEmpty())
	{
		int const rowCount = model()->rowCount();

		for (int i = 0; i < rowCount; ++i)
		{
			RestoreExpandedRecursively(model()->index(i, 0));
		}

		m_expandedBackup.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
void CTreeView::RestoreExpandedRecursively(QModelIndex const& index)
{
	if (index.isValid())
	{
		int const rowCount = model()->rowCount(index);

		if (rowCount > 0)
		{
			for (int i = 0; i < rowCount; ++i)
			{
				RestoreExpandedRecursively(index.child(i, 0));
			}
		}

		if (m_expandedBackup.contains(GetItemId(index)))
		{
			expand(index);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTreeView::BackupSelection()
{
	m_selectionBackup.clear();

	QModelIndexList const& selectedList = selectionModel()->selectedRows();

	for (QModelIndex const& index : selectedList)
	{
		m_selectionBackup.insert(GetItemId(index));
	}
}

//////////////////////////////////////////////////////////////////////////
void CTreeView::RestoreSelection()
{
	if (!m_selectionBackup.isEmpty())
	{
		int const rowCount = model()->rowCount();

		for (int i = 0; i < rowCount; ++i)
		{
			RestoreSelectionRecursively(model()->index(i, 0));
		}
	}
	else
	{
		for (QModelIndex const& index : selectionModel()->selectedIndexes())
		{
			scrollTo(index);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTreeView::RestoreSelectionRecursively(QModelIndex const& index)
{
	if (index.isValid())
	{
		int const rowCount = model()->rowCount(index);

		if (rowCount > 0)
		{
			for (int i = 0; i < rowCount; ++i)
			{
				RestoreSelectionRecursively(index.child(i, 0));
			}
		}

		if (m_selectionBackup.contains(GetItemId(index)))
		{
			selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
			scrollTo(index);
		}
	}
}
} // namespace SDLMixer
} // namespace Impl
} // namespace ACE
