// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ListenerSelectorModel.h"
#include "Common.h"
#include "ListenerManager.h"
#include "AssetIcons.h"

#include <QtUtil.h>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
int CListenerSelectorModel::rowCount(QModelIndex const& parent) const
{
	int rowCount = 0;

	if ((parent.row() < 0) || (parent.column() < 0))
	{
		rowCount = static_cast<int>(g_listenerManager.GetNumListeners());
	}

	return rowCount;
}

//////////////////////////////////////////////////////////////////////////
int CListenerSelectorModel::columnCount(QModelIndex const& parent) const
{
	return 1;
}

//////////////////////////////////////////////////////////////////////////
QVariant CListenerSelectorModel::data(QModelIndex const& index, int role) const
{
	QVariant variant;

	if (index.isValid() && (index.row() < static_cast<int>(g_listenerManager.GetNumListeners())))
	{
		switch (role)
		{
		case Qt::DisplayRole: // Intentional fall-through.
		case Qt::ToolTipRole:
			{
				variant = QtUtil::ToQString(g_listenerManager.GetListenerName(static_cast<size_t>(index.row())));
				break;
			}
		case Qt::DecorationRole:
			{
				variant = s_listenerIcon;
				break;
			}
		default:
			{
				break;
			}
		}
	}

	return variant;
}

//////////////////////////////////////////////////////////////////////////
bool CListenerSelectorModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
QVariant CListenerSelectorModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	QVariant variant;

	if ((orientation == Qt::Horizontal) && ((role == Qt::DisplayRole) || (role == Qt::ToolTipRole)))
	{
		variant = "Name";
	}

	return variant;
}

//////////////////////////////////////////////////////////////////////////
Qt::ItemFlags CListenerSelectorModel::flags(QModelIndex const& index) const
{
	return index.isValid() ? (QAbstractItemModel::flags(index) | Qt::ItemIsSelectable) : Qt::NoItemFlags;
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CListenerSelectorModel::index(int row, int column, QModelIndex const& parent /*= QModelIndex()*/) const
{
	QModelIndex modelIndex = QModelIndex();

	if ((row >= 0) && (column >= 0) && (row < static_cast<int>(g_listenerManager.GetNumListeners())))
	{
		modelIndex = createIndex(row, column);
	}

	return modelIndex;
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CListenerSelectorModel::parent(QModelIndex const& index) const
{
	return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////
bool CListenerSelectorModel::canDropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent) const
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CListenerSelectorModel::dropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
Qt::DropActions CListenerSelectorModel::supportedDropActions() const
{
	return Qt::IgnoreAction;
}

//////////////////////////////////////////////////////////////////////////
string CListenerSelectorModel::GetListenerNameFromIndex(QModelIndex const& index)
{
	return QtUtil::ToString(index.sibling(index.row(), 0).data(Qt::DisplayRole).toString());
}
} // namespace ACE
