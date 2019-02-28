// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "MergingProxyModel.h"

#include "Details/MultiProxyColumnMapping.h"
#include "Details/MultiProxyModelHelper.h"
#include "Details/ColumnSort.h"

#include <QSet>
#include <QSize>

#include <algorithm>

// build like Qt foreach macro but iterates backwards
#define reverse_foreach(variable, container)                                       \
  for (QForeachContainer<QT_FOREACH_DECLTYPE(container)> _container_((container)); \
       _container_.control && _container_.i != _container_.e--;                    \
       _container_.control ^= 1)                                                   \
    for (variable = *_container_.e; _container_.control; _container_.control = 0)

struct CMergingProxyModel::Implementation
	: public CAbstractMultiProxyModelHelper
{
	typedef CMergingProxyModel::SourceIndex     SourceIndex;
	typedef CMergingProxyModel::ProxyIndex      ProxyIndex;
	typedef CMultiProxyColumnMapping::MountData MountColumnMapping;

	struct MountPoint
	{
		Connections        connections;
		MountColumnMapping columnMapping;
		int                rootRowFirst; // first row index in the root node
		int                rootRowLast;  // last row index in the root node
	};
	typedef QHash<const SourceModel*, MountPoint>           SourceModelMountHash;
	typedef SourceModelMountHash::iterator                  SourceModelMount;
	typedef SourceModelMountHash::const_iterator            ConstSourceModelMount;
	typedef QVector<SourceModelMount>                       MountPointVector;

	typedef QSet<SourceIndex>                               SourceMapping;
	typedef SourceMapping::const_iterator                   MappingIterator;
	typedef QVector<MappingIterator>                        SourceMappingVector;

	typedef QList<QPair<ProxyIndex, QPersistentModelIndex>> QModelIndexPairList;

public: // data
	CMergingProxyModel*      m_pModel;

	GetHeaderDataCallback    m_getHeaderDataCallback;
	SetHeaderDataCallback    m_setHeaderDataCallback;
	std::function<QMimeData*(const QModelIndexList&)> m_dragCallback;

	SourceModelMountHash     m_sourceModelMounts;
	MountPointVector         m_mountPoints;
	CMultiProxyColumnMapping m_columnMapping;
	CColumnSort              m_sort;

	// cache
	mutable SourceMapping m_sourceMapping;

	// temporary storage
	QModelIndexPairList m_storedPersistentIndexes;
	SourceMappingVector m_storedSourceMappings;

public:
	Implementation(CMergingProxyModel* pModel)
		: m_pModel(pModel)
		, m_getHeaderDataCallback([](int, Qt::Orientation, int){ return QVariant(); })
		, m_setHeaderDataCallback([](int, Qt::Orientation, const QVariant&, int){ return false; })
		, m_dragCallback(nullptr)
	{}

	void Clear()
	{
		foreach(const SourceModelMount &sourceModelMount, m_mountPoints)
		{
			Disconnect(sourceModelMount->connections);
			if (sourceModelMount.key()->QObject::parent() == m_pModel)
			{
				const_cast<SourceModel*>(sourceModelMount.key())->deleteLater();
			}
		}
		m_sourceModelMounts.clear();
		m_mountPoints.clear();
		m_sort = CColumnSort();
		m_sourceMapping.clear();
		m_storedPersistentIndexes.clear();
		m_storedSourceMappings.clear();
	}

	bool IsSourceMapped(const SourceIndex& sourceIndex) const
	{
		CRY_ASSERT(sourceIndex.isValid());
		CRY_ASSERT(0 == sourceIndex.column());
		return m_sourceMapping.contains(sourceIndex);
	}

	MappingIterator CreateMappingFromSource(const SourceIndex& sourceIndex) const
	{
		auto firstIndex = sourceIndex.sibling(sourceIndex.row(), 0);
		auto it = m_sourceMapping.constFind(firstIndex);
		if (it != m_sourceMapping.constEnd())
		{
			return it;
		}
		CRY_ASSERT(sourceIndex.isValid());
		EnsureMappingForSourceParent(firstIndex);
		it = m_sourceMapping.insert(firstIndex);
		CRY_ASSERT(*it == firstIndex);
		return it;
	}

	void EnsureMappingForSourceParent(const SourceIndex& sourceIndex) const
	{
		CRY_ASSERT(sourceIndex.isValid());
		CRY_ASSERT(0 == sourceIndex.column());
		auto sourceParent = sourceIndex.parent();
		if (!sourceParent.isValid())
		{
			return; // invalid parent is always mapped
		}
		CreateMappingFromSource(sourceParent);
	}

	void MapDropCoordinatesToSource(int proxyRow, int proxyColumn, const QModelIndex& proxyParent,
	                                int* sourceRow, int* sourceColumn, QModelIndex* sourceParent, const SourceModel** sourceModel) const
	{
		*sourceRow = -1;
		*sourceColumn = -1;
		if (proxyRow == -1 && proxyColumn == -1)
		{
			*sourceParent = m_pModel->MapToSource(proxyParent);
			if (sourceParent->isValid())
			{
				*sourceModel = sourceParent->model();
			}
			else if (!m_mountPoints.isEmpty())
			{
				*sourceModel = m_mountPoints.last().key();
			}
		}
		else if (proxyRow == m_pModel->rowCount(proxyParent))
		{
			auto mapping = GetMappingFromProxy(proxyParent);
			*sourceParent = *mapping;
			if (sourceParent->isValid())
			{
				*sourceModel = sourceParent->model();
				*sourceRow = (*sourceModel)->rowCount(*sourceParent);
			}
			else
			{
				auto sourceMount = GetMountFromProxyRow(proxyRow - 1);
				*sourceModel = sourceMount.key();
				*sourceRow = 1 + sourceMount->rootRowLast - sourceMount->rootRowFirst; // == rowCount
			}
		}
		else
		{
			QModelIndex proxyIndex = m_pModel->index(proxyRow, proxyColumn, proxyParent);
			QModelIndex sourceIndex = m_pModel->MapToSource(proxyIndex);
			*sourceRow = sourceIndex.row();
			*sourceColumn = sourceIndex.column();
			*sourceParent = sourceIndex.parent();
			*sourceModel = sourceIndex.model();
		}
	}

	void ClearSourceMappingForModel(const SourceModel* sourceModel) const
	{
		DeepRemoveSourceMapping(sourceModel, QModelIndex());
	}

	void DeepRemoveSourceMapping(const SourceModel* sourceModel, const SourceIndex& sourceParent) const
	{
		auto rowCount = sourceModel->rowCount(sourceParent);
		DeepRemoveSourceMappingForChildrenBetween(sourceModel, sourceParent, 0, rowCount - 1);
	}

	void DeepRemoveSourceMappingForChildrenBetween(const SourceModel* sourceModel, const SourceIndex& sourceParent, int first, int last) const
	{
		for (auto row = first; row <= last; ++row)
		{
			auto sourceChild = sourceModel->index(row, 0, sourceParent);
			auto it = m_sourceMapping.find(sourceChild);
			if (it != m_sourceMapping.end())
			{
				DeepRemoveSourceMapping(sourceModel, sourceChild);
				m_sourceMapping.erase(it);
			}
		}
	}

	void RemoveSourceMapping(const SourceModel* sourceModel, const SourceIndex& sourceParent) const
	{
		auto last = sourceModel->rowCount(sourceParent) - 1;
		for (auto row = 0; row <= last; ++row)
		{
			auto sourceChild = sourceModel->index(row, 0, sourceParent);
			auto it = m_sourceMapping.find(sourceChild);
			if (it != m_sourceMapping.end())
			{
				m_sourceMapping.erase(it);
			}
		}
	}

	// This is used for moved children (they will reappear, when they are requested)
	void CollectSourceMappingForChildenFrom(SourceMappingVector& storedSourceMappings, const SourceModel* sourceModel, const SourceIndex& sourceParent, int first) const
	{
		auto last = sourceModel->rowCount(sourceParent) - 1;
		for (auto row = first; row <= last; ++row)
		{
			auto sourceChild = sourceModel->index(row, 0, sourceParent);
			auto it = m_sourceMapping.find(sourceChild);
			if (it != m_sourceMapping.end())
			{
				storedSourceMappings << it;
			}
		}
	}

	void MoveSourceIndexes(const SourceMappingVector& sourceMappings, const SourceModel* sourceModel, const SourceIndex& sourceParent, int rowDrift)
	{
		CRY_ASSERT(0 != rowDrift);
		if (rowDrift > 0)
		{
			reverse_foreach(const MappingIterator &it, sourceMappings)
			{
				auto newIndex = sourceModel->index(it->row() + rowDrift, 0, sourceParent);
				m_sourceMapping.erase(*reinterpret_cast<const SourceMapping::iterator*>(&it));
				m_sourceMapping.insert(newIndex);
			}
		}
		else
		{
			foreach(const MappingIterator &it, sourceMappings)
			{
				auto newIndex = sourceModel->index(it->row() + rowDrift, 0, sourceParent);
				m_sourceMapping.erase(*reinterpret_cast<const SourceMapping::iterator*>(&it));
				m_sourceMapping.insert(newIndex);
			}
		}
	}

	MappingIterator GetMappingFromProxy(const ProxyIndex& proxyIndex) const
	{
		if (!proxyIndex.isValid())
		{
			return CreateMappingFromSource(QModelIndex());
		}
		auto pointer = proxyIndex.internalPointer();
		auto it = *reinterpret_cast<MappingIterator*>(&pointer);
		CRY_ASSERT(m_sourceMapping.contains(*it));
		return it;
	}

	int MapColumnFromProxy(const SourceModel* sourceModel, int proxyColumn) const
	{
		auto mount = GetMountForSourceModel(sourceModel);
		auto sourceColumn = mount->columnMapping.GetColumnFromProxy(proxyColumn);
		return sourceColumn;
	}

	int MapColumnFromSource(const SourceModel* sourceModel, int sourceColumn) const
	{
		auto mount = GetMountForSourceModel(sourceModel);
		auto proxyColumn = mount->columnMapping.GetColumnFromSource(sourceColumn);
		return proxyColumn;
	}

	void UpdateMountColumnMappings()
	{
		auto endIt = m_sourceModelMounts.end();
		for (auto it = m_sourceModelMounts.begin(); it != endIt; ++it)
		{
			auto sourceModel = it.key();
			auto& mount = it.value();
			m_columnMapping.UpdateMountData(sourceModel, mount.columnMapping);
		}
	}

	int MapChildRowFromSource(const SourceModel* sourceModel, const SourceIndex& sourceParent, int row) const
	{
		CRY_ASSERT(sourceModel);
		CRY_ASSERT(m_sourceModelMounts.contains(sourceModel));
		CRY_ASSERT(row >= 0);
		if (sourceParent.isValid())
		{
			return row; // deeper nodes are unmodified
		}
		auto mount = GetMountForSourceModel(sourceModel);
		auto proxyRow = mount->rootRowFirst + row;
		return proxyRow;
	}

	int MapRowFromSource(const SourceIndex& sourceIndex) const
	{
		CRY_ASSERT(sourceIndex.isValid());
		auto sourceModel = sourceIndex.model();
		auto sourceParent = sourceIndex.parent();
		auto row = sourceIndex.row();
		return MapChildRowFromSource(sourceModel, sourceParent, row);
	}

	SourceModelMount GetMountForSourceModel(const SourceModel* sourceModel)
	{
		auto it = m_sourceModelMounts.find(sourceModel);
		CRY_ASSERT(it != m_sourceModelMounts.end());
		return it;
	}

	ConstSourceModelMount GetMountForSourceModel(const SourceModel* sourceModel) const
	{
		auto it = m_sourceModelMounts.find(sourceModel);
		CRY_ASSERT(it != m_sourceModelMounts.end());
		return it;
	}

	SourceModelMount GetMountFromProxyRow(int proxyRow) const
	{
		CRY_ASSERT(m_mountPoints.empty() || (*m_mountPoints.begin())->rootRowFirst == 0);
		CRY_ASSERT(proxyRow >= 0);
		auto it = std::upper_bound(m_mountPoints.begin(), m_mountPoints.end(), proxyRow, [](int row, const SourceModelMount& mount)
		{
			return row < mount->rootRowFirst;
		});
		return *(it - 1);
	}

	SourceModelMount FindMountFromProxyColumn(int proxyColumn) const
	{
		auto it = std::find_if(m_mountPoints.begin(), m_mountPoints.end(), [proxyColumn](const SourceModelMount& mount)
		{
			return (-1 != mount->columnMapping.GetColumnFromProxy(proxyColumn));
		});
		return *it;
	}

	ProxyIndex CreateIndexFromMapping(int row, int column, MappingIterator it) const
	{
		if (row < 0 || column < 0)
		{
			return ProxyIndex();
		}
		auto pointer = *(void**)(&it);
		return m_pModel->createIndex(row, column, pointer);
	}

	void SetMountRowCount(const SourceModelMount& it, int rowCount)
	{
		int oldRowCount = 1 + it->rootRowLast - it->rootRowFirst;
		int rowDifference = rowCount - oldRowCount;
		if (0 == rowDifference)
		{
			return; // not changed
		}
		MoveMountsAfter(it, rowDifference);
	}

	void MoveMountsAfter(const SourceModelMount& it, int rowDifference)
	{
		it->rootRowLast += rowDifference;
		auto index = m_mountPoints.indexOf(it);
		CRY_ASSERT(0 <= index);
		for (auto mit = m_mountPoints.constBegin() + index + 1; mit != m_mountPoints.constEnd(); ++mit)
		{
			SourceModelMount mount = *mit;
			mount->rootRowFirst += rowDifference;
			mount->rootRowLast += rowDifference;
		}
	}

	void CollectPersistedIndices(QModelIndexPairList& store, const SourceModel* sourceModel) const
	{
		auto persistedIndexes = m_pModel->persistentIndexList();
		foreach(ProxyIndex proxyIndex, persistedIndexes)
		{
			auto it = GetMappingFromProxy(proxyIndex);
			auto sourceIndex = *it;
			if (sourceIndex.model() == sourceModel)
			{
				store.append(qMakePair(proxyIndex, QPersistentModelIndex(sourceIndex)));
			}
		}
	}

	void UpdatePersistedIndices(const QModelIndexPairList& store)
	{
		QModelIndexList from, to;
		from.reserve(store.count());
		to.reserve(store.count());
		foreach(auto pair, store)
		{
			auto oldProxyIndex = pair.first;
			auto sourceIndex = QModelIndex(pair.second);
			auto proxyIndex = m_pModel->MapFromSource(sourceIndex);
			from << oldProxyIndex;
			to << proxyIndex.sibling(proxyIndex.row(), oldProxyIndex.column());
		}
		m_pModel->changePersistentIndexList(from, to);
	}

	// CAbstractMultiProxyModelHelper interface
public:
	virtual bool IsSourceIndexMapped(const SourceModel* sourceModel, const QModelIndex& sourceIndex) const override
	{
		CRY_ASSERT(m_sourceModelMounts.contains(sourceModel));
		if (!sourceIndex.isValid())
		{
			return true; // root is always present
		}
		return IsSourceMapped(sourceIndex);
	}

	virtual void onSourceDataChanged(const QModelIndex& sourceTopLeft, const QModelIndex& sourceBottomRight, const QVector<int>& roles) override
	{
		CRY_ASSERT(sourceTopLeft.isValid());
		CRY_ASSERT(sourceBottomRight.isValid());
		CRY_ASSERT(sourceTopLeft.parent() == sourceBottomRight.parent());
		CRY_ASSERT(sourceTopLeft.column() <= sourceBottomRight.column());
		auto sourceParent = sourceTopLeft.parent();
		if (sourceParent.isValid() && !IsSourceMapped(sourceParent))
		{
			return; // parent index is unknown
		}
		auto sourceModel = sourceTopLeft.model();
		auto proxyTopRow = MapRowFromSource(sourceTopLeft);
		auto proxyBottomRow = MapRowFromSource(sourceBottomRight);
		QPair<int, int> minMaxProxyColumn = qMakePair(INT_MAX, -1);
		for (auto sourceColumn = sourceTopLeft.column(); sourceColumn <= sourceBottomRight.column(); ++sourceColumn)
		{
			auto proxyColumn = MapColumnFromSource(sourceModel, sourceColumn);
			minMaxProxyColumn.first = qMin(minMaxProxyColumn.first, proxyColumn);
			minMaxProxyColumn.second = qMax(minMaxProxyColumn.second, proxyColumn);
		}
		auto proxyIndex = m_pModel->MapFromSource(sourceTopLeft);
		auto proxyTopLeft = proxyIndex.sibling(proxyTopRow, minMaxProxyColumn.first);
		auto proxyBottomRight = proxyIndex.sibling(proxyBottomRow, minMaxProxyColumn.second);
		m_pModel->dataChanged(proxyTopLeft, proxyBottomRight, roles);
	}

	virtual void onSourceAboutToBeReset(const SourceModel* sourceModel) override
	{
		SourceModelMount it = m_sourceModelMounts.find(sourceModel);
		CRY_ASSERT(it != m_sourceModelMounts.end());

		auto rowCount = 1 + it->rootRowLast - it->rootRowFirst;
		if (0 < rowCount)
		{
			m_pModel->beginRemoveRows(QModelIndex(), it->rootRowFirst, it->rootRowLast);
			MoveMountsAfter(it, -rowCount);
			ClearSourceMappingForModel(sourceModel);
			m_pModel->endRemoveRows();
		}
	}

	virtual void onSourceReset(const SourceModel* sourceModel) override
	{
		SourceModelMount it = m_sourceModelMounts.find(sourceModel);
		CRY_ASSERT(it != m_sourceModelMounts.end());

		auto rowCount = sourceModel->rowCount();
		if (0 < rowCount)
		{
			auto rowLast = it->rootRowFirst + rowCount - 1;
			m_pModel->beginInsertRows(QModelIndex(), it->rootRowFirst, rowLast);
			MoveMountsAfter(it, rowCount);
			m_pModel->endInsertRows();
		}

		m_columnMapping.UpdateMountData(sourceModel, it->columnMapping);
	}

	virtual void onSourceLayoutAboutToBeChanged(const SourceModel* sourceModel, const QList<QPersistentModelIndex>& sourceParents, LayoutChangeHint hint) override
	{
		QList<QPersistentModelIndex> parents;
		parents.reserve(sourceParents.size());
		foreach(auto & sourceParent, sourceParents)
		{
			if (sourceParent.isValid() && !IsSourceMapped(sourceParent))
			{
				continue;
			}
			parents << m_pModel->MapFromSource(sourceParent);
		}
		m_pModel->layoutAboutToBeChanged(parents, hint);

		CollectPersistedIndices(m_storedPersistentIndexes, sourceModel);

		if (parents.isEmpty())
		{
			ClearSourceMappingForModel(sourceModel);
		}
		else
		{
			foreach(auto & sourceParent, sourceParents)
			{
				RemoveSourceMapping(sourceModel, sourceParent);
			}
		}
	}

	virtual void onSourceLayoutChanged(const SourceModel* sourceModel, const QList<QPersistentModelIndex>& sourceParents, LayoutChangeHint hint) override
	{
		auto it = GetMountForSourceModel(sourceModel);
		SetMountRowCount(it, sourceModel->rowCount());

		UpdatePersistedIndices(m_storedPersistentIndexes);
		m_storedPersistentIndexes.clear();

		QList<QPersistentModelIndex> parents;
		parents.reserve(sourceParents.size());
		foreach(auto & sourceParent, sourceParents)
		{
			if (sourceParent.isValid() && !IsSourceMapped(sourceParent))
			{
				continue;
			}
			parents << m_pModel->MapFromSource(sourceParent);
		}
		m_pModel->layoutChanged(parents, hint);
	}

	virtual void onSourceRowsAboutToBeInserted(const SourceModel* sourceModel, const QModelIndex& sourceParent, int sourceFirst, int sourceLast) override
	{
		CRY_ASSERT(m_sourceModelMounts.contains(sourceModel));
		CRY_ASSERT(sourceFirst >= 0);
		CRY_ASSERT(sourceFirst <= sourceLast);
		auto proxyParent = m_pModel->MapFromSource(sourceParent);
		auto proxyFirst = MapChildRowFromSource(sourceModel, sourceParent, sourceFirst);
		auto proxyLast = MapChildRowFromSource(sourceModel, sourceParent, sourceLast);
		m_pModel->beginInsertRows(proxyParent, proxyFirst, proxyLast);
		CollectSourceMappingForChildenFrom(m_storedSourceMappings, sourceModel, sourceParent, sourceFirst);
	}

	virtual void onSourceRowsInserted(const SourceModel* sourceModel, const QModelIndex& sourceParent, int first, int last) override
	{
		auto rowDrift = 1 + last - first;
		MoveSourceIndexes(m_storedSourceMappings, sourceModel, sourceParent, rowDrift);
		m_storedSourceMappings.clear();
		if (!sourceParent.isValid())
		{
			auto it = GetMountForSourceModel(sourceModel);
			MoveMountsAfter(it, rowDrift);
		}
		m_pModel->endInsertRows();
	}

	virtual void onSourceRowsAboutToBeRemoved(const SourceModel* sourceModel, const QModelIndex& sourceParent, int sourceFirst, int sourceLast) override
	{
		CRY_ASSERT(m_sourceModelMounts.contains(sourceModel));
		CRY_ASSERT(sourceFirst >= 0);
		CRY_ASSERT(sourceFirst <= sourceLast);
		auto proxyParent = m_pModel->MapFromSource(sourceParent);
		auto proxyFirst = MapChildRowFromSource(sourceModel, sourceParent, sourceFirst);
		auto proxyLast = MapChildRowFromSource(sourceModel, sourceParent, sourceLast);
		m_pModel->beginRemoveRows(proxyParent, proxyFirst, proxyLast);
		DeepRemoveSourceMappingForChildrenBetween(sourceModel, sourceParent, sourceFirst, sourceLast);
		CollectSourceMappingForChildenFrom(m_storedSourceMappings, sourceModel, sourceParent, sourceLast + 1);
	}

	virtual void onSourceRowsRemoved(const SourceModel* sourceModel, const QModelIndex& sourceParent, int first, int last) override
	{
		auto rowDrift = 1 + last - first;
		MoveSourceIndexes(m_storedSourceMappings, sourceModel, sourceParent, -rowDrift);
		m_storedSourceMappings.clear();
		if (!sourceParent.isValid())
		{
			auto it = GetMountForSourceModel(sourceModel);
			MoveMountsAfter(it, -rowDrift);
		}
		m_pModel->endRemoveRows();
	}

	virtual void onSourceRowsAboutToBeMoved(
	  const SourceModel* sourceModel,
	  const QModelIndex& sourceParent, int sourceStart, int sourceEnd,
	  const QModelIndex& destinationParent, int destinationRow) override
	{
		CRY_ASSERT(m_sourceModelMounts.contains(sourceModel));
		CRY_ASSERT(sourceStart >= 0);
		CRY_ASSERT(sourceStart <= sourceEnd);
		CRY_ASSERT(destinationRow >= 0);
		Q_UNUSED(sourceStart);
		Q_UNUSED(sourceEnd);
		Q_UNUSED(destinationRow);

		PersistentModelIndexList proxyParents;
		proxyParents << m_pModel->MapFromSource(sourceParent);
		if (sourceParent != destinationParent)
		{
			proxyParents << m_pModel->MapFromSource(destinationParent);
		}
		m_pModel->layoutAboutToBeChanged(proxyParents);

		CollectPersistedIndices(m_storedPersistentIndexes, sourceModel);

		RemoveSourceMapping(sourceModel, sourceParent);
		if (sourceParent != destinationParent)
		{
			RemoveSourceMapping(sourceModel, destinationParent);
		}

		// Note: This version does not work because QHash cannot change indices and QAbstractItemModel insists it can adjust indices without changing the pointer
		//
		//		auto proxySource = m_pModel->MapFromSource(sourceParent);
		//		auto proxyDestination = m_pModel->MapFromSource(destinationParent);
		//		auto proxySourceStart = MapChildRowFromSource(sourceModel, sourceParent, sourceStart);
		//		auto proxySourceEnd = MapChildRowFromSource(sourceModel, sourceParent, sourceEnd);
		//		auto proxyDestinationRow = MapChildRowFromSource(sourceModel, destinationParent, destinationRow);
		//		m_pModel->beginMoveRows(proxySource, proxySourceStart, proxySourceEnd, proxyDestination, proxyDestinationRow);
		//		if (sourceParent == destinationParent)
		//		{
		//			auto first = qMin(sourceStart, destinationRow);
		//			CollectSourceMappingForChildenFrom(m_storedSourceMappings, sourceModel, sourceParent, first);
		//		}
		//		else
		//		{
		//			CollectSourceMappingForChildenFrom(m_storedSourceMappings, sourceModel, sourceParent, sourceStart);
		//		}
	}

	virtual void onSourceRowsMoved(
	  const SourceModel* sourceModel,
	  const QModelIndex& sourceParent, int sourceStart, int sourceEnd,
	  const QModelIndex& destinationParent, int destinationRow) override
	{
		Q_UNUSED(sourceModel);
		Q_UNUSED(sourceStart);
		Q_UNUSED(sourceEnd);
		Q_UNUSED(destinationRow);

		auto it = GetMountForSourceModel(sourceModel);
		SetMountRowCount(it, sourceModel->rowCount());

		UpdatePersistedIndices(m_storedPersistentIndexes);
		m_storedPersistentIndexes.clear();

		PersistentModelIndexList proxyParents;
		proxyParents << m_pModel->MapFromSource(sourceParent);
		if (sourceParent != destinationParent)
		{
			proxyParents << m_pModel->MapFromSource(destinationParent);
		}
		m_pModel->layoutChanged(proxyParents);

		// Note: See comment in onSourceRowsAboutToBeMoved()
		//
		//		if (sourceParent != destinationParent)
		//		{
		//			auto rowDrift = 1 + sourceEnd - sourceStart;
		//			if ( !sourceParent.isValid())
		//			{
		//				MoveSourceIndexes(m_storedSourceMappings, sourceModel, sourceParent, -rowDrift);
		//				auto it = GetMountForSourceModel(sourceModel);
		//				MoveMountsAfter(it, -rowDrift);
		//			}
		//			if ( !destinationParent.isValid())
		//			{
		//				MoveSourceIndexes(m_storedSourceMappings, sourceModel, sourceParent, rowDrift);
		//				auto it = GetMountForSourceModel(sourceModel);
		//				MoveMountsAfter(it, rowDrift);
		//			}
		//		}
		//		m_storedSourceMappings.clear();
		//		m_pModel->endMoveRows();
	}
};

CMergingProxyModel::CMergingProxyModel(QObject* parent)
	: QAbstractItemModel(parent)
	, p(new Implementation(this))
{}

CMergingProxyModel::~CMergingProxyModel()
{}

CMergingProxyModel::SourceIndex CMergingProxyModel::MapToSource(const ProxyIndex& proxyIndex) const
{
	if (!proxyIndex.isValid())
	{
		return QModelIndex();
	}
	auto it = p->GetMappingFromProxy(proxyIndex);
	auto sourceIndex = *it;
	CRY_ASSERT(sourceIndex.isValid());
	auto sourceColumn = p->MapColumnFromProxy(sourceIndex.model(), proxyIndex.column());
	return sourceIndex.sibling(sourceIndex.row(), sourceColumn);
}

CMergingProxyModel::ProxyIndex CMergingProxyModel::MapFromSource(const SourceIndex& sourceIndex) const
{
	if (!sourceIndex.isValid())
	{
		return QModelIndex();
	}
	auto it = p->CreateMappingFromSource(sourceIndex);
	auto proxyColumn = p->MapColumnFromSource(sourceIndex.model(), sourceIndex.column());
	auto proxyRow = p->MapRowFromSource(sourceIndex);
	return p->CreateIndexFromMapping(proxyRow, proxyColumn, it);
}

QVector<const CMergingProxyModel::SourceModel*> CMergingProxyModel::GetSourceModels() const
{
	QVector<const SourceModel*> sourceModels;
	sourceModels.reserve(p->m_mountPoints.count());
	foreach(const auto & sourceModelMount, p->m_mountPoints)
	{
		sourceModels << sourceModelMount.key();
	}
	return sourceModels;
}

int CMergingProxyModel::GetSourceModelCount() const
{
	return p->m_mountPoints.count();
}

const CMergingProxyModel::SourceModel* CMergingProxyModel::GetSourceModel(int index) const
{
	return p->m_mountPoints[index].key();
}

void CMergingProxyModel::MountAppend(SourceModel* sourceModel)
{
	Mount(sourceModel, p->m_mountPoints.size());
}

void CMergingProxyModel::Mount(SourceModel* sourceModel, int index)
{
	if (nullptr == sourceModel->QObject::parent())
	{
		sourceModel->QObject::setParent(this);
	}
	Implementation::MountColumnMapping columnMapping;
	p->m_columnMapping.UpdateMountData(sourceModel, columnMapping);
	if (p->m_sort.isValid())
	{
		auto sourceColumn = columnMapping.GetColumnFromProxy(p->m_sort.column());
		const_cast<SourceModel*>(sourceModel)->sort(sourceColumn, p->m_sort.order());
	}

	auto mountRows = sourceModel->rowCount();
	int firstRow = index <= 0 ? 0 : (index < p->m_mountPoints.size() ? p->m_mountPoints[index]->rootRowFirst : rowCount());
	if (0 < mountRows)
	{
		int lastRow = firstRow + mountRows - 1;
		beginInsertRows(QModelIndex(), firstRow, lastRow);
	}

	Implementation::MountPoint mountPoint;
	mountPoint.rootRowFirst = firstRow;
	mountPoint.rootRowLast = firstRow - 1;
	mountPoint.connections = p->ConnectSourceModel(sourceModel, this);
	mountPoint.columnMapping = columnMapping;
	auto it = p->m_sourceModelMounts.insert(sourceModel, mountPoint);
	p->m_mountPoints.insert(index, it);

	if (0 < mountRows)
	{
		p->MoveMountsAfter(it, mountRows);
		endInsertRows();
	}
}

void CMergingProxyModel::Unmount(SourceModel* submodel)
{
	auto it = p->m_sourceModelMounts.find(submodel);
	if (it == p->m_sourceModelMounts.end())
	{
		return; // not mounted
	}

	auto mountRows = 1 + it->rootRowLast - it->rootRowFirst;
	if (0 < mountRows)
	{
		beginRemoveRows(QModelIndex(), it->rootRowFirst, it->rootRowLast);
	}

	p->MoveMountsAfter(it, -mountRows);
	p->m_sourceModelMounts.erase(it);
	p->m_mountPoints.removeAll(it);
	p->ClearSourceMappingForModel(submodel);

	if (0 < mountRows)
	{
		endRemoveRows();
	}

	if (submodel->QObject::parent() == this)
	{
		submodel->deleteLater();
	}
}

void CMergingProxyModel::UnmountAll()
{
	beginResetModel();

	p->Clear();

	endResetModel();
}

void CMergingProxyModel::SetHeaderDataCallbacks(int columnCount, const GetHeaderDataCallback& getHeaderData, int mergeValueRole, const SetHeaderDataCallback& setHeaderData)
{
	beginResetModel();

	QVector<QVariant> columnValues;
	columnValues.reserve(columnCount);
	for (int i = 0; i < columnCount; ++i)
	{
		columnValues << getHeaderData(i, Qt::Horizontal, mergeValueRole);
	}
	p->m_getHeaderDataCallback = getHeaderData;
	if (setHeaderData)
	{
		p->m_setHeaderDataCallback = setHeaderData;
	}
	else
	{
		p->m_setHeaderDataCallback = [](int, Qt::Orientation, const QVariant&, int){ return false; };
	}
	p->m_columnMapping.Reset(columnValues, mergeValueRole);
	p->UpdateMountColumnMappings();

	endResetModel();
}

void CMergingProxyModel::SetDragCallback(const std::function<QMimeData*(const QModelIndexList&)>& callback)
{
	if (callback)
		p->m_dragCallback = callback;
}

void CMergingProxyModel::ColumnsChanged(int columnCount)
{
	beginResetModel();

	auto mergeValueRole = p->m_columnMapping.GetValueRole();

	QVector<QVariant> columnValues;
	columnValues.reserve(columnCount);
	for (int i = 0; i < columnCount; ++i)
	{
		columnValues << p->m_getHeaderDataCallback(i, Qt::Horizontal, mergeValueRole);
	}
	p->m_columnMapping.Reset(columnValues, mergeValueRole);
	p->UpdateMountColumnMappings();

	endResetModel();
}

QModelIndex CMergingProxyModel::index(int proxyRow, int proxyColumn, const QModelIndex& proxyParent) const
{
	if (proxyRow < 0 || proxyColumn < 0)
	{
		return QModelIndex();
	}
	if (proxyParent.isValid())
	{
		auto it = p->GetMappingFromProxy(proxyParent);
		QModelIndex sourceParent = *it;
		auto sourceIndex = sourceParent.child(proxyRow, 0);
		return MapFromSource(sourceIndex).sibling(proxyRow, proxyColumn);
	}
	auto sourceMount = p->GetMountFromProxyRow(proxyRow);
	auto sourceModel = sourceMount.key();
	auto sourceRow = proxyRow - sourceMount->rootRowFirst;
	auto sourceIndex = sourceModel->index(sourceRow, 0);
	return MapFromSource(sourceIndex).sibling(proxyRow, proxyColumn);
}

QModelIndex CMergingProxyModel::parent(const QModelIndex& proxyChild) const
{
	if (!proxyChild.isValid())
	{
		return QModelIndex();
	}
	auto it = p->GetMappingFromProxy(proxyChild);
	auto sourceIndex = *it;
	auto sourceParent = sourceIndex.parent();
	if (!sourceParent.isValid())
	{
		return QModelIndex();
	}
	return MapFromSource(sourceParent);
}

QModelIndex CMergingProxyModel::sibling(int proxyRow, int proxyColumn, const QModelIndex& proxyIndex) const
{
	if (proxyRow < 0 || proxyColumn < 0 || !proxyIndex.isValid())
	{
		return QModelIndex();
	}
	if (proxyRow == proxyIndex.row())
	{
		return createIndex(proxyRow, proxyColumn, proxyIndex.internalPointer());
	}
	auto it = p->GetMappingFromProxy(proxyIndex);
	auto sourceIndex = *it;
	auto sourceParent = sourceIndex.parent();
	if (proxyRow == proxyIndex.row() || sourceParent.isValid())
	{
		auto sourceColumn = p->MapColumnFromProxy(sourceIndex.model(), proxyColumn);
		auto sourceSibling = sourceIndex.sibling(proxyRow, sourceColumn);
		return MapFromSource(sourceSibling);
	}
	return index(proxyRow, proxyColumn);
}

int CMergingProxyModel::rowCount(const QModelIndex& parent) const
{
	if (parent.isValid())
	{
		if (parent.column() != 0)
		{
			return 0;
		}
		auto sourceParent = MapToSource(parent);
		return sourceParent.model()->rowCount(sourceParent);
	}
	if (p->m_mountPoints.isEmpty())
	{
		return 0;
	}
	auto mount = p->m_mountPoints.back();
	return mount->rootRowLast + 1;
}

int CMergingProxyModel::columnCount(const QModelIndex&) const
{
	return p->m_columnMapping.GetColumnCount();
}

bool CMergingProxyModel::hasChildren(const QModelIndex& proxyParent) const
{
	if (!proxyParent.isValid())
	{
		return true;
	}
	if (proxyParent.column() != 0)
	{
		return false;
	}
	auto sourceParent = MapToSource(proxyParent);
	return sourceParent.model()->hasChildren(sourceParent);
}

Qt::ItemFlags CMergingProxyModel::flags(const QModelIndex& proxyIndex) const
{
	if (!proxyIndex.isValid())
	{
		return 0;
	}
	auto sourceIndex = MapToSource(proxyIndex);
	CRY_ASSERT(sourceIndex.isValid());
	return sourceIndex.flags();
}

QVariant CMergingProxyModel::data(const QModelIndex& proxyIndex, int role) const
{
	if (!proxyIndex.isValid())
	{
		if (p->m_mountPoints.isEmpty())
		{
			return QVariant();
		}
		return p->m_mountPoints.front().key()->data(proxyIndex, role);
	}
	auto sourceIndex = MapToSource(proxyIndex);
	CRY_ASSERT(sourceIndex.isValid());
	return sourceIndex.data(role);
}

bool CMergingProxyModel::setData(const QModelIndex& proxyIndex, const QVariant& value, int role)
{
	if (!proxyIndex.isValid())
	{
		if (p->m_mountPoints.isEmpty())
		{
			return false;
		}
		auto sourceModel = const_cast<QAbstractItemModel*>(p->m_mountPoints.front().key());
		return sourceModel->setData(proxyIndex, value, role);
	}
	auto sourceIndex = MapToSource(proxyIndex);
	CRY_ASSERT(sourceIndex.isValid());
	auto sourceModel = const_cast<QAbstractItemModel*>(sourceIndex.model());
	return sourceModel->setData(sourceIndex, value, role);
}

QVariant CMergingProxyModel::headerData(int proxySection, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal)
	{
		if (role == p->m_columnMapping.GetValueRole())
		{
			return p->m_columnMapping.GetColumnValue(proxySection);
		}
	}
	return p->m_getHeaderDataCallback(proxySection, orientation, role);
}

bool CMergingProxyModel::setHeaderData(int proxySection, Qt::Orientation orientation, const QVariant& value, int role)
{
	if (orientation == Qt::Horizontal)
	{
		if (role == p->m_columnMapping.GetValueRole())
		{
			return false; // this is not implemented
		}
	}
	return p->m_setHeaderDataCallback(proxySection, orientation, value, role);
}

QMap<int, QVariant> CMergingProxyModel::itemData(const QModelIndex& proxyIndex) const
{
	if (!proxyIndex.isValid())
	{
		return QMap<int, QVariant>();
	}
	auto sourceIndex = MapToSource(proxyIndex);
	return sourceIndex.model()->itemData(sourceIndex);
}

bool CMergingProxyModel::setItemData(const QModelIndex& proxyIndex, const QMap<int, QVariant>& roles)
{
	if (!proxyIndex.isValid())
	{
		return false;
	}
	auto sourceIndex = MapToSource(proxyIndex);
	auto sourceModel = const_cast<QAbstractItemModel*>(sourceIndex.model());
	return sourceModel->setItemData(sourceIndex, roles);
}

QStringList CMergingProxyModel::mimeTypes() const
{
	QSet<QString> set;
	for (const Implementation::SourceModelMount& mount : p->m_mountPoints)
	{
		auto sourceModel = mount.key();
		auto mimeTypes = sourceModel->mimeTypes();
		set.unite(mimeTypes.toSet());
	}
	return set.toList();
}

QMimeData* CMergingProxyModel::mimeData(const QModelIndexList& proxyIndexes) const
{
	QModelIndexList sourceIndexList;
	sourceIndexList.reserve(proxyIndexes.size());
	const SourceModel* sourceModel = nullptr;
	foreach(auto & proxyIndex, proxyIndexes)
	{
		auto sourceIndex = MapToSource(proxyIndex);
		sourceIndexList << sourceIndex;
		if (nullptr == sourceModel)
		{
			sourceModel = sourceIndex.model();
		}
		else if (!p->m_dragCallback && sourceIndex.isValid() && sourceIndex.model() != sourceModel)
		{
			return nullptr; // cannot handle indices from multiple models
		}
	}
	if (nullptr == sourceModel)
	{
		return nullptr;
	}

	if (p->m_dragCallback)
		p->m_dragCallback(sourceIndexList);

	return sourceModel->mimeData(sourceIndexList);
}

bool CMergingProxyModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int proxyRow, int proxyColumn, const QModelIndex& proxyParent) const
{
	int sourceRow, sourceColumn;
	QModelIndex sourceParent;
	const SourceModel* sourceModel;
	p->MapDropCoordinatesToSource(proxyRow, proxyColumn, proxyParent, &sourceRow, &sourceColumn, &sourceParent, &sourceModel);
	if (!sourceModel)
	{
		return false;
	}
	return sourceModel->canDropMimeData(data, action, sourceRow, sourceColumn, sourceParent);
}

bool CMergingProxyModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int proxyRow, int proxyColumn, const QModelIndex& proxyParent)
{
	int sourceRow, sourceColumn;
	QModelIndex sourceParent;
	const SourceModel* sourceModel;
	p->MapDropCoordinatesToSource(proxyRow, proxyColumn, proxyParent, &sourceRow, &sourceColumn, &sourceParent, &sourceModel);
	if (!sourceModel)
	{
		return false;
	}
	auto noConstSourceModel = const_cast<SourceModel*>(sourceModel);
	return noConstSourceModel->dropMimeData(data, action, sourceRow, sourceColumn, sourceParent);
}

Qt::DropActions CMergingProxyModel::supportedDropActions() const
{
	Qt::DropActions result = 0;
	foreach(const auto & mount, p->m_mountPoints)
	{
		result |= mount.key()->supportedDropActions();
	}
	return result;
}

Qt::DropActions CMergingProxyModel::supportedDragActions() const
{
	Qt::DropActions result = 0;
	foreach(const auto & mount, p->m_mountPoints)
	{
		result |= mount.key()->supportedDragActions();
	}
	return result;
}

bool CMergingProxyModel::canFetchMore(const QModelIndex& parent) const
{
	if (parent.isValid())
	{
		auto sourceIndex = MapToSource(parent);
		return sourceIndex.model()->canFetchMore(sourceIndex);
	}
	foreach(const auto & mount, p->m_mountPoints)
	{
		if (mount.key()->canFetchMore(QModelIndex()))
		{
			return true;
		}
	}
	return false;
}

void CMergingProxyModel::fetchMore(const QModelIndex& parent)
{
	if (parent.isValid())
	{
		auto sourceIndex = MapToSource(parent);
		auto sourceModel = const_cast<QAbstractItemModel*>(sourceIndex.model());
		sourceModel->fetchMore(sourceIndex);
	}
	else
	{
		foreach(const auto & mount, p->m_mountPoints)
		{
			auto mountModel = const_cast<QAbstractItemModel*>(mount.key());
			mountModel->fetchMore(QModelIndex());
		}
	}
}

void CMergingProxyModel::sort(int proxyColumn, Qt::SortOrder order)
{
	auto newSort = CColumnSort(proxyColumn, order);
	if (p->m_sort == newSort)
	{
		return; // nothing changed
	}
	p->m_sort = newSort;
	// always sort submodels (enables to remove sort)
	foreach(const auto & mount, p->m_mountPoints)
	{
		auto sourceColumn = mount->columnMapping.GetColumnFromProxy(proxyColumn);
		auto mountModel = const_cast<QAbstractItemModel*>(mount.key());
		mountModel->sort(sourceColumn, order);
	}
}

QModelIndex CMergingProxyModel::buddy(const QModelIndex& proxyIndex) const
{
	if (!proxyIndex.isValid())
	{
		return QModelIndex();
	}
	auto sourceIndex = MapToSource(proxyIndex);
	auto sourceBuddy = sourceIndex.model()->buddy(sourceIndex);
	return MapFromSource(sourceBuddy);
}

QSize CMergingProxyModel::span(const QModelIndex& proxyIndex) const
{
	if (!proxyIndex.isValid())
	{
		return QSize(0, 0);
	}
	auto sourceIndex = MapToSource(proxyIndex);
	return sourceIndex.model()->span(sourceIndex);
}

QHash<int, QByteArray> CMergingProxyModel::roleNames() const
{
	QHash<int, QByteArray> result;
	foreach(const auto & mount, p->m_mountPoints)
	{
		auto mountRoles = mount.key()->roleNames();
		foreach(const auto & role, mountRoles.keys())
		{
			if (!result.contains(role))
			{
				result[role] = mountRoles[role];
			}
		}
	}
	return result;
}

#undef reverse_foreach

