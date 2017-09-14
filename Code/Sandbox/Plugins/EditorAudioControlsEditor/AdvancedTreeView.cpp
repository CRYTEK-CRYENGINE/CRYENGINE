// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AdvancedTreeView.h"
#include <CryAudio/IAudioSystem.h>

#include <QMenu>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
bool CAdvancedTreeView::IsEditing() const
{
	return state() == QAbstractItemView::EditingState;
}

//////////////////////////////////////////////////////////////////////////
void CAdvancedTreeView::ExpandSelection(QModelIndexList const& indexList)
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
void CAdvancedTreeView::CollapseSelection(QModelIndexList const& indexList)
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
uint32 CAdvancedTreeView::GetItemId(QModelIndex const& index) const
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
void CAdvancedTreeView::BackupExpanded()
{
	m_expandedBackup.clear();

	for (int i = 0; i < model()->rowCount(); ++i)
	{
		BackupExpandedChildren(model()->index(i, 0));
	}
}

//////////////////////////////////////////////////////////////////////////
CAdvancedTreeView::QSetItemId CAdvancedTreeView::BackupExpandedOnReset()
{
	BackupExpanded();
	return m_expandedBackup;
}

//////////////////////////////////////////////////////////////////////////
void CAdvancedTreeView::BackupExpandedChildren(QModelIndex const& index)
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
void CAdvancedTreeView::RestoreExpanded()
{
	if (!m_expandedBackup.isEmpty())
	{
		for (int i = 0; i < model()->rowCount(); ++i)
		{
			RestoreExpandedChildren(model()->index(i, 0));
		}

		m_expandedBackup.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAdvancedTreeView::RestoreExpandedOnReset(QSetItemId const& expandedBackup)
{
	m_expandedBackup = expandedBackup;
	RestoreExpanded();
}

//////////////////////////////////////////////////////////////////////////
void CAdvancedTreeView::RestoreExpandedChildren(QModelIndex const& index)
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
void CAdvancedTreeView::BackupSelection()
{
	m_selectionBackup.clear();
	m_bSelectionChanged = false;

	QModelIndexList const& selectedList = selectionModel()->selectedRows();

	for (QModelIndex const& index : selectedList)
	{
		m_selectionBackup.insert(GetItemId(index));
	}
}

//////////////////////////////////////////////////////////////////////////
CAdvancedTreeView::QSetItemId CAdvancedTreeView::BackupSelectionOnReset()
{
	BackupSelection();
	return m_selectionBackup;
}

//////////////////////////////////////////////////////////////////////////
void CAdvancedTreeView::RestoreSelection()
{
	if (!m_selectionBackup.isEmpty() && !m_bSelectionChanged)
	{
		for (int i = 0; i < model()->rowCount(); ++i)
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
void CAdvancedTreeView::RestoreSelectionOnReset(QSetItemId const& selectedBackup)
{
	m_selectionBackup = selectedBackup;
	RestoreSelection();
}

//////////////////////////////////////////////////////////////////////////
void CAdvancedTreeView::RestoreSelectionChildren(QModelIndex const& index)
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
void CAdvancedTreeView::OnSelectionChanged(QItemSelection const& selected, QItemSelection const& deselected)
{
	for (QModelIndex const& index : selected.indexes())
	{
		if (!m_selectionBackup.contains(GetItemId(index)))
		{
			m_bSelectionChanged = true;
			break;
		}
	}
}
} // namespace ACE
