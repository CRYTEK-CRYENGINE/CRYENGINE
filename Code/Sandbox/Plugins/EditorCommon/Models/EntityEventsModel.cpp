// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include <CryEntitySystem/IEntityClass.h>
#include "EntityEventsModel.h"

int CEntityEventsModel::rowCount(const QModelIndex& parent) const
{
	if (!parent.isValid())
	{
		return m_class.GetEventCount();
	}
	return 0;
}

int CEntityEventsModel::columnCount(const QModelIndex& parent) const
{
	return 1;
}

QVariant CEntityEventsModel::data(const QModelIndex& index, int role) const
{
	if (index.isValid() && index.row() < m_class.GetEventCount())
	{
		switch (role)
		{
		case Qt::DisplayRole:
		case Qt::EditRole:
			switch (index.column())
			{
			case 0:
				IEntityClass::SEventInfo info = m_class.GetEventInfo(index.row());
				return info.name;
			}
			break;
		case Qt::DecorationRole:
		default:
			break;
		}
	}

	return QVariant();
}

QVariant CEntityEventsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case 0:
			return QObject::tr("Event Name");
		default:
			break;
		}
	}
	return QVariant();
}

QModelIndex CEntityEventsModel::index(int row, int column, const QModelIndex& parent) const
{
	if (row >= 0 && row < m_class.GetEventCount())
	{
		IEntityClass::SEventInfo info = m_class.GetEventInfo(row);
		return QAbstractItemModel::createIndex(row, column, reinterpret_cast<quintptr>(info.name));
	}
	return QModelIndex();
}

QModelIndex CEntityEventsModel::parent(const QModelIndex& index) const
{
	return QModelIndex();
}

