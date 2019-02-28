// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SequenceEventsModel.h"

#include <CryMovie/IMovieSystem.h>

int CSequenceEventsModel::rowCount(const QModelIndex& parent) const
{
	if (!parent.isValid())
	{
		return m_pSequence->GetTrackEventsCount();
	}
	return 0;
}

int CSequenceEventsModel::columnCount(const QModelIndex& parent) const
{
	return 1;
}

QVariant CSequenceEventsModel::data(const QModelIndex& index, int role) const
{
	if (index.isValid() && index.row() < m_pSequence->GetTrackEventsCount())
	{
		switch (role)
		{
		case Qt::DisplayRole:
		case Qt::EditRole:
			switch (index.column())
			{
			case 0:
				return m_pSequence->GetTrackEvent(index.row());
			}
			break;
		case Qt::DecorationRole:
		default:
			break;
		}
	}

	return QVariant();
}

QVariant CSequenceEventsModel::headerData(int section, Qt::Orientation orientation, int role) const
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

QModelIndex CSequenceEventsModel::index(int row, int column, const QModelIndex& parent) const
{
	if (row >= 0 && row < m_pSequence->GetTrackEventsCount())
	{
		return QAbstractItemModel::createIndex(row, column, reinterpret_cast<quintptr>(m_pSequence->GetTrackEvent(row)));
	}
	return QModelIndex();
}

QModelIndex CSequenceEventsModel::parent(const QModelIndex& index) const
{
	return QModelIndex();
}

