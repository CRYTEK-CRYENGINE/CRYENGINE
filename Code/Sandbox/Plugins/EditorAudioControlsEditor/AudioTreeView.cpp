// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioTreeView.h"
#include <CryAudio/IAudioSystem.h>

#include <QMenu>
#include <QHeaderView>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CAudioTreeView::CAudioTreeView()
	: QAdvancedTreeView(QAdvancedTreeView::Behavior(QAdvancedTreeView::None))
{
	QObject::connect(header(), &QHeaderView::sortIndicatorChanged, [this]() { scrollTo(currentIndex()); });
}

//////////////////////////////////////////////////////////////////////////
bool CAudioTreeView::IsEditing() const
{
	return state() == QAbstractItemView::EditingState;
}

//////////////////////////////////////////////////////////////////////////
void CAudioTreeView::ExpandSelection(QModelIndexList const& indexList)
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

//////////////////////////////////////////////////////////////////////////
void CAudioTreeView::CollapseSelection(QModelIndexList const& indexList)
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

//////////////////////////////////////////////////////////////////////////
uint32 CAudioTreeView::GetItemId(QModelIndex const& index) const
{
	if (index.isValid())
	{
		QString itemName = index.data(Qt::DisplayRole).toString();

		QModelIndex parent = index.parent();

		while (parent.isValid())
		{
			itemName.prepend((parent.data(Qt::DisplayRole).toString()) + "/");
			parent = parent.parent();
		}

		itemName.remove(" *");
		return CryAudio::StringToId(itemName.toStdString().c_str());
	}

	return CryAudio::InvalidCRC32;
}

//////////////////////////////////////////////////////////////////////////
void CAudioTreeView::BackupExpanded()
{
	m_expandedBackup.clear();

	int const rowCount = model()->rowCount();

	for (int i = 0; i < rowCount; ++i)
	{
		BackupExpandedChildren(model()->index(i, 0));
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioTreeView::BackupExpandedChildren(QModelIndex const& index)
{
	if (index.isValid())
	{
		int const rowCount = model()->rowCount(index);

		if (rowCount > 0)
		{
			for (int i = 0; i < rowCount; ++i)
			{
				BackupExpandedChildren(index.child(i, 0));
			}
		}

		if (isExpanded(index))
		{
			m_expandedBackup.insert(GetItemId(index));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioTreeView::RestoreExpanded()
{
	if (!m_expandedBackup.isEmpty())
	{
		int const rowCount = model()->rowCount();

		for (int i = 0; i < rowCount; ++i)
		{
			RestoreExpandedChildren(model()->index(i, 0));
		}

		m_expandedBackup.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioTreeView::RestoreExpandedChildren(QModelIndex const& index)
{
	if (index.isValid())
	{
		int const rowCount = model()->rowCount(index);

		if (rowCount > 0)
		{
			for (int i = 0; i < rowCount; ++i)
			{
				RestoreExpandedChildren(index.child(i, 0));
			}
		}

		if (m_expandedBackup.contains(GetItemId(index)))
		{
			expand(index);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioTreeView::BackupSelection()
{
	m_selectionBackup.clear();
	m_hasSelectionChanged = false;

	QModelIndexList const& selectedList = selectionModel()->selectedRows();

	for (QModelIndex const& index : selectedList)
	{
		m_selectionBackup.insert(GetItemId(index));
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioTreeView::RestoreSelection()
{
	if (!m_selectionBackup.isEmpty() && !m_hasSelectionChanged)
	{
		int const rowCount = model()->rowCount();

		for (int i = 0; i < rowCount; ++i)
		{
			RestoreSelectionChildren(model()->index(i, 0));
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
void CAudioTreeView::RestoreSelectionChildren(QModelIndex const& index)
{
	if (index.isValid())
	{
		int const rowCount = model()->rowCount(index);

		if (rowCount > 0)
		{
			for (int i = 0; i < rowCount; ++i)
			{
				RestoreSelectionChildren(index.child(i, 0));
			}
		}

		if (m_selectionBackup.contains(GetItemId(index)))
		{
			selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
			scrollTo(index);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioTreeView::OnSelectionChanged(QItemSelection const& selected, QItemSelection const& deselected)
{
	for (QModelIndex const& index : selected.indexes())
	{
		if (!m_selectionBackup.contains(GetItemId(index)))
		{
			m_hasSelectionChanged = true;
			break;
		}
	}
}
} // namespace ACE
