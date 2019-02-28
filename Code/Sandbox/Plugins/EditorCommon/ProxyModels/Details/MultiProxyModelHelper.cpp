// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "MultiProxyModelHelper.h"

namespace CAbstractMultiProxyModelHelper_Private
{
namespace
{
typedef CAbstractMultiProxyModelHelper::PersistentModelIndexList PersistentModelIndexList;
typedef CAbstractMultiProxyModelHelper::SourceModel              SourceModel;

PersistentModelIndexList FilterMountedIndices(const CAbstractMultiProxyModelHelper& helper, const SourceModel* sourceModel, const PersistentModelIndexList& sourceParents)
{
	PersistentModelIndexList result;
	result.reserve(sourceParents.size());
	foreach(auto & sourceParent, sourceParents)
	{
		if (helper.IsSourceIndexMapped(sourceModel, sourceParent))
		{
			result << sourceParent;
		}
	}
	return result;
}

} // namespace
} // namespace CAbstractMultiProxyModelHelper_Private

CAbstractMultiProxyModelHelper::Connections CAbstractMultiProxyModelHelper::ConnectSourceModel(const SourceModel* pSourceModel, QObject* pParent)
{
	Connections connections;
	connections << QObject::connect(pSourceModel, &QAbstractItemModel::dataChanged, pParent, [this](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles)
	{
		onSourceDataChanged(topLeft, bottomRight, roles);
	});
	connections << QObject::connect(pSourceModel, &QAbstractItemModel::modelAboutToBeReset, pParent, [this, pSourceModel]()
	{
		onSourceAboutToBeReset(pSourceModel);
	});
	connections << QObject::connect(pSourceModel, &QAbstractItemModel::modelReset, pParent, [this, pSourceModel]()
	{
		onSourceReset(pSourceModel);
	});
	connections << QObject::connect(pSourceModel, &QAbstractItemModel::layoutAboutToBeChanged, pParent, [this, pSourceModel](const QList<QPersistentModelIndex>& sourceParents, LayoutChangeHint hint)
	{
		auto filteredSourceParents = CAbstractMultiProxyModelHelper_Private::FilterMountedIndices(*this, pSourceModel, sourceParents);
		if (sourceParents.isEmpty() || !filteredSourceParents.isEmpty())
		{
		  onSourceLayoutAboutToBeChanged(pSourceModel, filteredSourceParents, hint);
		}
	});
	connections << QObject::connect(pSourceModel, &QAbstractItemModel::layoutChanged, pParent, [this, pSourceModel](const QList<QPersistentModelIndex>& sourceParents, LayoutChangeHint hint)
	{
		auto filteredSourceParents = CAbstractMultiProxyModelHelper_Private::FilterMountedIndices(*this, pSourceModel, sourceParents);
		if (sourceParents.isEmpty() || !filteredSourceParents.isEmpty())
		{
		  onSourceLayoutChanged(pSourceModel, filteredSourceParents, hint);
		}
	});
	connections << QObject::connect(pSourceModel, &QAbstractItemModel::rowsAboutToBeInserted, pParent, [this, pSourceModel](const QModelIndex& sourceParent, int first, int last)
	{
		if (IsSourceIndexMapped(pSourceModel, sourceParent))
		{
		  onSourceRowsAboutToBeInserted(pSourceModel, sourceParent, first, last);
		}
	});
	connections << QObject::connect(pSourceModel, &QAbstractItemModel::rowsInserted, pParent, [this, pSourceModel](const QModelIndex& sourceParent, int first, int last)
	{
		if (IsSourceIndexMapped(pSourceModel, sourceParent))
		{
		  onSourceRowsInserted(pSourceModel, sourceParent, first, last);
		}
	});
	connections << QObject::connect(pSourceModel, &QAbstractItemModel::rowsAboutToBeRemoved, pParent, [this, pSourceModel](const QModelIndex& sourceParent, int first, int last)
	{
		if (IsSourceIndexMapped(pSourceModel, sourceParent))
		{
		  onSourceRowsAboutToBeRemoved(pSourceModel, sourceParent, first, last);
		}
	});
	connections << QObject::connect(pSourceModel, &QAbstractItemModel::rowsRemoved, pParent, [this, pSourceModel](const QModelIndex& sourceParent, int first, int last)
	{
		if (IsSourceIndexMapped(pSourceModel, sourceParent))
		{
		  onSourceRowsRemoved(pSourceModel, sourceParent, first, last);
		}
	});

	enum MoveSituations
	{
		eMoveAllUnknown       = 0,
		eMoveSourceValid      = 1,
		eMoveDestinationValid = 2,
		eMoveNormal           = 3
	};
	auto getMoveSituation = [this](const SourceModel* pSourceModel, const QModelIndex& source, const QModelIndex& destination)
	{
		return (IsSourceIndexMapped(pSourceModel, source) ? eMoveSourceValid : 0)
		       + (IsSourceIndexMapped(pSourceModel, destination) ? eMoveDestinationValid : 0);
	};
	connections << QObject::connect(pSourceModel, &QAbstractItemModel::rowsAboutToBeMoved, pParent, [this, pSourceModel, getMoveSituation](const QModelIndex& sourceParent, int sourceStart, int sourceEnd, const QModelIndex& destinationParent, int destinationRow)
	{
		m_storedMovedSituation = getMoveSituation(pSourceModel, sourceParent, destinationParent);
		switch (m_storedMovedSituation)
		{
		case eMoveSourceValid:
			onSourceRowsAboutToBeRemoved(pSourceModel, sourceParent, sourceStart, sourceEnd);
			break;
		case eMoveDestinationValid:
			onSourceRowsAboutToBeInserted(pSourceModel, destinationParent, destinationRow, destinationRow + (sourceEnd - sourceStart));
			break;
		case eMoveNormal:
			onSourceRowsAboutToBeMoved(pSourceModel, sourceParent, sourceStart, sourceEnd, destinationParent, destinationRow);
			break;
		case eMoveAllUnknown:
		default:
			return; // both parents are unknown to the outside
		}
	});
	connections << QObject::connect(pSourceModel, &QAbstractItemModel::rowsMoved, pParent, [this, pSourceModel, getMoveSituation](const QModelIndex& sourceParent, int sourceStart, int sourceEnd, const QModelIndex& destinationParent, int destinationRow)
	{
		switch (m_storedMovedSituation)
		{
		case eMoveSourceValid:
			onSourceRowsRemoved(pSourceModel, sourceParent, sourceStart, sourceEnd);
			break;
		case eMoveDestinationValid:
			onSourceRowsInserted(pSourceModel, destinationParent, destinationRow, destinationRow + (sourceEnd - sourceStart));
			break;
		case eMoveNormal:
			onSourceRowsMoved(pSourceModel, sourceParent, sourceStart, sourceEnd, destinationParent, destinationRow);
			break;
		case eMoveAllUnknown:
		default:
			return; // both parents are unknown to the outside
		}
	});

	connections << QObject::connect(pSourceModel, &QAbstractItemModel::columnsAboutToBeInserted, pParent, []()
	{
		CRY_ASSERT("TODO"); // setup your columns before connecting the models!
	});
	connections << QObject::connect(pSourceModel, &QAbstractItemModel::columnsAboutToBeRemoved, pParent, []()
	{
		CRY_ASSERT("TODO"); // setup your columns before connecting the models!
	});
	connections << QObject::connect(pSourceModel, &QAbstractItemModel::columnsAboutToBeMoved, pParent, []()
	{
		CRY_ASSERT("TODO"); // setup your columns before connecting the models!
	});
	return connections;
}

void CAbstractMultiProxyModelHelper::Disconnect(const CAbstractMultiProxyModelHelper::Connections& connections)
{
	foreach(auto & connection, connections)
	{
		QObject::disconnect(connection);
	}
}

