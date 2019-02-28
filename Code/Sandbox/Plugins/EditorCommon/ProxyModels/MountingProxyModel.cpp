// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "MountingProxyModel.h"

#include "Details/MultiProxyColumnMapping.h"
#include "Details/MultiProxyModelHelper.h"
#include "Details/ColumnSort.h"

#include <QSize>
#include <QSet>

// build like Qt foreach macro but iterates backwards
#define reverse_foreach(variable, container)                                       \
  for (QForeachContainer<QT_FOREACH_DECLTYPE(container)> _container_((container)); \
       _container_.control && _container_.i != _container_.e--;                    \
       _container_.control ^= 1)                                                   \
    for (variable = *_container_.e; _container_.control; _container_.control = 0)

struct CMountingProxyModel::Implementation
	: public CAbstractMultiProxyModelHelper
{
	typedef CMountingProxyModel::ProxyIndex         ProxyIndex;
	typedef CMountingProxyModel::SourceModel        SourceModel;
	typedef CMountingProxyModel::SourceModelFactory SourceModelFactory;
	typedef CMultiProxyColumnMapping::MountData     MountColumnMapping;

	struct MountPoint
	{
		QPersistentModelIndex sourceIndex; // sourceIndex where this is mounted to
		Connections           connections;
		MountColumnMapping    columnMapping;
	};
	typedef QHash<const SourceModel*, MountPoint> SourceModelMountHash;
	typedef SourceModelMountHash::const_iterator  SourceModelMount;
	typedef SourceModelMountHash::iterator        ModifiableSourceModelMount;
	typedef QList<SourceModelMount>               MountPointList;

	struct Mapping
	{
		SourceModelMount mount; // nullptr if no submodel mounted here
	};
	typedef QHash<SourceIndex, Mapping>                     SourceMappingHash;
	typedef SourceMappingHash::const_iterator               SourceMapping;
	typedef SourceMappingHash::iterator                     ModifiableSourceMapping;
	typedef QVector<SourceMapping>                          SourceMappingVector;

	typedef QVector<QPair<SourceIndex, Mapping>>            SourceIndexMappingPairVector;

	typedef QList<QPair<ProxyIndex, QPersistentModelIndex>> QProxyIndexPersistentPairList;

	SourceModelFactory       m_sourceModelFactory;
	CMountingProxyModel*     m_pModel;

	GetHeaderDataCallback    m_getHeaderDataCallback;
	SetHeaderDataCallback    m_setHeaderDataCallback;
	std::function<QMimeData*(const QModelIndexList&)> m_dragCallback;

	CMultiProxyColumnMapping m_columnMapping;
	CColumnSort              m_sort;

	// caches
	mutable SourceMappingHash    m_sourceMapping;     // submodel mapping for all source indices
	mutable SourceModelMountHash m_sourceModelMounts; // source indices only all the mounting points

	// temporary storage
	QProxyIndexPersistentPairList m_storedPersistentIndexes;
	MountPointList                m_storedMountPoints;
	SourceMappingVector           m_storedSourceMappings;
	SourceMappingVector           m_storedDestinationMappings;

public:
	Implementation(const SourceModelFactory& factory, CMountingProxyModel* model)
		: m_sourceModelFactory(factory)
		, m_pModel(model)
		, m_getHeaderDataCallback([](int, Qt::Orientation, int){ return QVariant(); })
		, m_setHeaderDataCallback([](int, Qt::Orientation, const QVariant&, int){ return false; })
		, m_dragCallback(nullptr)
		, m_bAllowRemove(false)
	{}

	static SourceIndex CreateSourceChildIndex(
	  int sourceRow, int sourceColumn,
	  SourceModelMount mount,
	  const QModelIndex& parent,
	  const QAbstractItemModel* mainModel)
	{
		// TODO: remap column?
		CRY_ASSERT(sourceRow >= 0);
		CRY_ASSERT(sourceColumn >= 0);
		if (SourceModelMount() != mount)
		{
			return mount.key()->index(sourceRow, sourceColumn);
		}
		if (parent.isValid())
		{
			return parent.child(sourceRow, sourceColumn);
		}
		return mainModel->index(sourceRow, sourceColumn);
	}

	bool IsMountedSourceMapped(const SourceModel* sourceModel, const SourceIndex& sourceIndex) const
	{
		if (sourceIndex.isValid())
		{
			CRY_ASSERT(0 == sourceIndex.column());
			CRY_ASSERT(sourceIndex.model() == sourceModel);
			CRY_ASSERT(m_sourceModelMounts.contains(sourceModel));
			return m_sourceMapping.contains(sourceIndex);
		}
		return m_sourceModelMounts.contains(sourceModel);
	}

	int MapColumnFromProxy(const SourceModel* sourceModel, int proxyColumn) const
	{
		auto mount = GetMountForSourceModel(sourceModel);
		if (mount == m_sourceModelMounts.end())
		{
			return -1;
		}
		auto sourceColumn = mount->columnMapping.GetColumnFromProxy(proxyColumn);
		return sourceColumn;
	}

	int MapColumnFromSource(const SourceModel* sourceModel, int sourceColumn) const
	{
		auto mount = GetMountForSourceModel(sourceModel);
		if (mount == m_sourceModelMounts.end())
		{
			return -1;
		}
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

	void MapDropCoordinatesToSource(int proxyRow, int proxyColumn, const QModelIndex& proxyParent,
	                                int* sourceRow, int* sourceColumn, QModelIndex* sourceParent, const SourceModel** sourceModel) const
	{
		*sourceRow = -1;
		*sourceColumn = -1;
		if (proxyRow == -1 && proxyColumn == -1)
		{
			*sourceParent = m_pModel->MapToSource(proxyParent);
			*sourceModel = sourceParent->isValid() ? sourceParent->model() : m_pModel->GetSourceModel();
		}
		else if (proxyRow == m_pModel->rowCount(proxyParent))
		{
			if (!proxyParent.isValid())
			{
				*sourceParent = proxyParent;
				*sourceModel = m_pModel->GetSourceModel();
			}
			else
			{
				auto mapping = GetMappingFromProxy(proxyParent);
				*sourceParent = mapping.key();
				*sourceModel = sourceParent->model();
			}
			*sourceRow = (*sourceModel)->rowCount(*sourceParent);
		}
		else
		{
			QModelIndex proxyIndex = m_pModel->index(proxyRow, 0, proxyParent);
			CRY_ASSERT(proxyIndex.isValid());
			auto mapping = GetMappingFromProxy(proxyIndex);
			QModelIndex sourceIndex = mapping.key();
			*sourceModel = sourceIndex.model();
			*sourceRow = sourceIndex.row();
			*sourceColumn = MapColumnFromProxy(*sourceModel, proxyColumn);
			*sourceParent = sourceIndex.parent();
		}
	}

	SourceModelMount GetMountForSourceModel(const SourceModel* sourceModel) const
	{
		auto it = m_sourceModelMounts.find(sourceModel);
		CRY_ASSERT(it != m_sourceModelMounts.end());
		return it;
	}

	SourceMapping GetMappingFromSource(const SourceIndex& sourceIndex) const
	{
		auto it = m_sourceMapping.constFind(sourceIndex);
		CRY_ASSERT(it != m_sourceMapping.constEnd());
		return it;
	}

	SourceMapping CreateMappingFromSource(const SourceIndex& sourceIndex) const
	{
		auto firstIndex = sourceIndex.sibling(sourceIndex.row(), 0);
		auto it = m_sourceMapping.constFind(firstIndex);
		if (it != m_sourceMapping.constEnd())
		{
			return it;
		}
		CRY_ASSERT(sourceIndex.isValid());
		EnsureMappingForSourceParent(firstIndex);
		Mapping mapping;
		mapping.mount = CreateMountFromFactory(firstIndex);
		it = m_sourceMapping.insert(firstIndex, mapping);
		CRY_ASSERT(it.key() == firstIndex);
		return it;
	}

	void EnsureMappingForSourceParent(const SourceIndex& sourceIndex) const
	{
		auto parentIndex = sourceIndex.parent();
		if (!parentIndex.isValid())
		{
			auto mit = m_sourceModelMounts.constFind(sourceIndex.model());
			CRY_ASSERT(mit != m_sourceModelMounts.constEnd());
			parentIndex = mit.value().sourceIndex;
		}
		if (parentIndex.isValid())
		{
			CreateMappingFromSource(parentIndex);
		}
	}

	SourceMapping GetMappingFromProxy(const ProxyIndex& proxyIndex) const
	{
		if (!proxyIndex.isValid())
		{
			return CreateMappingFromSource(QModelIndex());
		}
		auto pointer = proxyIndex.internalPointer();
		auto it = static_cast<SourceMapping>(pointer);
		return it;
	}

	ProxyIndex CreateIndexFromMapping(int proxyRow, int proxyColumn, const SourceMapping& it) const
	{
		if (proxyRow < 0 || proxyColumn < 0)
		{
			return QModelIndex();
		}
		// use the iterator itself as a pointer
		auto pointer = *(void**)(&it);
		return m_pModel->createIndex(proxyRow, proxyColumn, pointer);
	}

	SourceIndex GetMountedSourceIndex(const SourceModel* sourceModel, const SourceIndex& sourceIndex) const
	{
		if (sourceIndex.isValid())
		{
			CRY_ASSERT(sourceModel == sourceIndex.model());
			return sourceIndex;
		}
		CRY_ASSERT(nullptr != sourceModel);
		CRY_ASSERT(m_sourceModelMounts.contains(sourceModel));
		return m_sourceModelMounts[sourceModel].sourceIndex;
	}

	ProxyIndex GetMountedProxyIndex(const SourceModel* sourceModel, const SourceIndex& sourceIndex) const
	{
		auto mountedSourceIndex = GetMountedSourceIndex(sourceModel, sourceIndex);
		CRY_ASSERT(mountedSourceIndex.isValid() || sourceModel == m_pModel->GetSourceModel());
		if (!mountedSourceIndex.isValid())
		{
			return QModelIndex();
		}
		auto proxyRow = mountedSourceIndex.row();
		auto proxyColumn = MapColumnFromSource(mountedSourceIndex.model(), mountedSourceIndex.column());
		auto it = GetMappingFromSource(mountedSourceIndex);
		return CreateIndexFromMapping(proxyRow, proxyColumn, it);
	}

	SourceModelMount FindMountFromProxyColumn(int proxyColumn) const
	{
		CRY_ASSERT(!m_sourceModelMounts.isEmpty());  // DO not call this unless you have a model mounted!

		// prefer the main model
		auto mainIt = m_sourceMapping[QModelIndex()].mount;
		if (-1 != mainIt->columnMapping.GetColumnFromProxy(proxyColumn))
		{
			return mainIt;
		}

		// search all other source models
		for (auto it = m_sourceModelMounts.constBegin(); it != m_sourceModelMounts.constEnd(); it++)
		{
			if (-1 != it->columnMapping.GetColumnFromProxy(proxyColumn))
			{
				return it;
			}
		}
		return SourceModelMount();
	}
	SourceModelMount CreateMountFromFactory(const SourceIndex& sourceIndex) const
	{
		CRY_ASSERT(sourceIndex.isValid());
		CRY_ASSERT(0 == sourceIndex.column());
		auto sourceModel = m_sourceModelFactory(sourceIndex);
		return const_cast<Implementation*>(this)->CreateMountPoint(sourceIndex, sourceModel);
	}

	SourceModelMount CreateMountPoint(const SourceIndex& sourceIndex, SourceModel* sourceModel)
	{
		if (nullptr == sourceModel)
		{
			return SourceModelMount(); // nothing mounted
		}
		CRY_ASSERT(!m_sourceModelMounts.contains(sourceModel)); // each submodel can only be mounted once!
		if (nullptr == sourceModel->QObject::parent())
		{
			sourceModel->QObject::setParent(m_pModel);
		}
		MountColumnMapping mountColumnMapping;
		m_columnMapping.UpdateMountData(sourceModel, mountColumnMapping);

		// sort sourceModel now, so we do not have to handle the remapping
		if (m_sort.isValid())
		{
			auto sourceColumn = mountColumnMapping.GetColumnFromProxy(m_sort.column());
			const_cast<SourceModel*>(sourceModel)->sort(sourceColumn, m_sort.order());
		}
		auto mount = CreateMountPointInternal(sourceIndex, sourceModel, mountColumnMapping);
		return mount;
	}

	SourceModelMount CreateMountPointInternal(const SourceIndex& sourceIndex, SourceModel* sourceModel, const MountColumnMapping& mountColumnMapping)
	{
		MountPoint mountPoint;
		mountPoint.sourceIndex = sourceIndex;
		mountPoint.connections = const_cast<Implementation*>(this)->ConnectSourceModel(sourceModel, m_pModel);
		mountPoint.columnMapping = mountColumnMapping;
		auto mount = m_sourceModelMounts.insert(sourceModel, mountPoint);
		return mount;
	}

	void DestroyMountPoint(SourceModelMount mount)
	{
		CRY_ASSERT(mount != m_sourceModelMounts.end());
		Disconnect(mount->connections);

		if (mount.key()->QObject::parent() == m_pModel)
		{
			const_cast<SourceModel*>(mount.key())->deleteLater();
		}
	}

	void ClearPersistedIndices()
	{
		auto persistedIndexList = m_pModel->persistentIndexList();
		QVector<QModelIndex> clean(persistedIndexList.size());
		m_pModel->changePersistentIndexList(persistedIndexList, clean.toList());
	}

	void ClearMappings()
	{
		for (auto it = m_sourceModelMounts.begin(); it != m_sourceModelMounts.end(); ++it)
		{
			DestroyMountPoint(it);
		}
		m_sourceModelMounts.clear();
		m_sourceMapping.clear();
	}

	void CollectPersistedIndices(QProxyIndexPersistentPairList& store, const SourceModel* sourceModel) const
	{
		auto persistedIndexes = m_pModel->persistentIndexList();
		foreach(ProxyIndex proxyIndex, persistedIndexes)
		{
			auto it = GetMappingFromProxy(proxyIndex);
			auto sourceIndex = it.key();
			if (sourceIndex.model() == sourceModel)
			{
				store.append(qMakePair(proxyIndex, QPersistentModelIndex(sourceIndex)));
			}
		}
	}

	void UpdatePersistedIndices(const QProxyIndexPersistentPairList& store)
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

	void CollectSourceParents(PersistentModelIndexList& list, const SourceModel* sourceModel, const SourceIndex& parentIndex) const
	{
		if (!IsMountedSourceMapped(sourceModel, parentIndex))
		{
			return;
		}
		auto rowCount = sourceModel->rowCount(parentIndex);
		if (0 < rowCount)
		{
			list << QPersistentModelIndex(parentIndex);
		}
		for (auto row = 0; row < rowCount; ++row)
		{
			auto sourceIndex = sourceModel->index(row, 0, parentIndex);
			CollectSourceParents(list, sourceModel, sourceIndex);
		}
	}

	void CollectSourceMappings(SourceMappingVector& sourceMappings, const SourceModel* sourceModel, const SourceIndex& sourceParent, int first, int end) const
	{
		for (int row = first; row < end; ++row)
		{
			auto sourceIndex = sourceModel->index(row, 0, sourceParent);
			auto it = m_sourceMapping.constFind(sourceIndex);
			if (it == m_sourceMapping.constEnd())
			{
				continue; // index is not mapped
			}
			sourceMappings << it;
		}
	}

	void MoveSourceMapping(const SourceMapping& it, const SourceIndex& newSourceIndex)
	{
		auto mapping = it.value();
		m_sourceMapping.erase(*reinterpret_cast<const ModifiableSourceMapping*>(&it));
		m_sourceMapping.insert(newSourceIndex, mapping);
		if (SourceModelMount() != mapping.mount)
		{
			CRY_ASSERT(mapping.mount->sourceIndex == newSourceIndex);
		}
	}

	QPair<SourceIndex, Mapping> GetUpdateAndRemoveSourceMapping(const SourceMapping& it, const SourceIndex& newSourceIndex)
	{
		auto mapping = it.value();
		m_sourceMapping.erase(*reinterpret_cast<const ModifiableSourceMapping*>(&it));
		auto update = qMakePair(newSourceIndex, mapping);
		if (SourceModelMount() != mapping.mount)
		{
			CRY_ASSERT(mapping.mount->sourceIndex == newSourceIndex);
		}
		return update;
	}

	template<typename F>
	void MoveSourceIndexes(const SourceMappingVector& sourceMappings, int rowDrift, F&& f)
	{
		CRY_ASSERT(0 != rowDrift);
		if (rowDrift > 0)
		{
			reverse_foreach(const SourceMapping &it, sourceMappings)
			{
				MoveSourceMapping(it, f(it));
			}
		}
		else
		{
			foreach(const SourceMapping &it, sourceMappings)
			{
				MoveSourceMapping(it, f(it));
			}
		}
	}

	void MoveSourceIndexes(const SourceMappingVector& sourceMappings, const SourceModel* sourceModel, const SourceIndex& sourceParent, int rowDrift)
	{
		MoveSourceIndexes(sourceMappings, rowDrift, [&](SourceMapping it)
		{
			return sourceModel->index(it.key().row() + rowDrift, 0, sourceParent);
		});
	}

	void DeepStoreMountPoints(MountPointList& mountPoints, const SourceModel* sourceModel, const SourceIndex& sourceParent) const
	{
		auto rowCount = sourceModel->rowCount(sourceParent);
		for (auto row = 0; row < rowCount; ++row)
		{
			auto sourceIndex = sourceModel->index(row, 0, sourceParent);
			auto it = m_sourceMapping.constFind(sourceIndex);
			if (it == m_sourceMapping.constEnd())
			{
				continue; // no mapping to remove
			}
			StoreMountPoint(mountPoints, sourceIndex);
			DeepStoreMountPoints(mountPoints, sourceModel, sourceIndex);
		}
	}

	void StoreMountPoints(MountPointList& mountPoints, const SourceModel* sourceModel, const PersistentModelIndexList& sourceParents) const
	{
		if (sourceParents.isEmpty())
		{
			DeepStoreMountPoints(mountPoints, sourceModel, QModelIndex());
			return; // stored all
		}
		foreach(auto & sourceParent, sourceParents)
		{
			auto rowCount = sourceModel->rowCount(sourceParent);
			for (auto row = 0; row < rowCount; ++row)
			{
				auto sourceIndex = sourceModel->index(row, 0, sourceParent);
				StoreMountPoint(mountPoints, sourceIndex);
			}
		}
	}

	void StoreMountPoint(MountPointList& mountPoints, const SourceIndex& sourceIndex) const
	{
		auto mapping = m_sourceMapping.find(sourceIndex);
		if (mapping == m_sourceMapping.end())
		{
			return; // index was not exposed
		}
		auto mount = mapping->mount;
		if (SourceModelMount() != mount)
		{
			mountPoints << mount;
			mapping->mount = SourceModelMount();
		}
	}

	void UpdateMountPoints(const MountPointList& mountPoints)
	{
		foreach(auto & mount, mountPoints)
		{
			auto sourceIndex = QModelIndex(mount.value().sourceIndex);
			if (sourceIndex.isValid())
			{
				Mapping mapping;
				mapping.mount = mount;
				m_sourceMapping.insert(sourceIndex, mapping);
			}
			else
			{
				RemoveMount(mount);
			}
		}
		foreach(auto & mount, mountPoints)
		{
			auto sourceIndex = QModelIndex(mount.value().sourceIndex);
			if (sourceIndex.isValid())
			{
				EnsureMappingForSourceParent(sourceIndex);
			}
		}
	}

	void DeepRemoveSourceMappings(const SourceModel* sourceModel, const SourceIndex& sourceParent, int first, int end)
	{
		for (int row = first; row < end; ++row)
		{
			auto sourceIndex = sourceModel->index(row, 0, sourceParent);
			auto it = m_sourceMapping.constFind(sourceIndex);
			if (it == m_sourceMapping.constEnd())
			{
				continue; // no mapping to remove
			}
			DeepRemoveMapping(it);
		}
	}

	void DeepRemoveMapping(const SourceMapping& it)
	{
		auto sourceIndex = it.key();
		auto mount = it->mount;
		if (SourceModelMount() != mount)
		{
			DeepRemoveSourceMappings(mount.key(), QModelIndex(), 0, mount.key()->rowCount());
			RemoveMount(mount);
		}
		else
		{
			auto sourceModel = sourceIndex.model();
			DeepRemoveSourceMappings(sourceModel, sourceIndex, 0, sourceModel->rowCount(sourceIndex));
		}
		m_sourceMapping.remove(sourceIndex);
	}

	void RemoveChildSourceMappings(const SourceModel* sourceModel, const PersistentModelIndexList& sourceParents)
	{
		if (sourceParents.empty())
		{
			DeepRemoveSourceMappings(sourceModel, QModelIndex(), 0, sourceModel->rowCount());
			return; // removed all
		}
		foreach(auto & sourceParent, sourceParents)
		{
			auto rowCount = sourceModel->rowCount(sourceParent);
			for (auto row = 0; row < rowCount; ++row)
			{
				auto sourceIndex = sourceModel->index(row, 0, sourceParent);
				m_sourceMapping.remove(sourceIndex);
			}
		}
	}

	void RemoveMount(SourceModelMount mount)
	{
		CRY_ASSERT(SourceModelMount() != mount);
		DestroyMountPoint(mount);
		m_sourceModelMounts.erase(*reinterpret_cast<ModifiableSourceModelMount*>(&mount));
	}

	// CAbstractMultiProxyModelHelper interface
public:
	virtual bool IsSourceIndexMapped(const SourceModel* sourceModel, const QModelIndex& sourceIndex) const override
	{
		return IsMountedSourceMapped(sourceModel, sourceIndex);
	}

	virtual void onSourceDataChanged(const QModelIndex& sourceTopLeft, const QModelIndex& sourceBottomRight, const QVector<int>& roles) override
	{
		auto proxyTopLeft = m_pModel->MapFromSource(sourceTopLeft);
		auto proxyBottomRight = m_pModel->MapFromSource(sourceBottomRight);
		m_pModel->dataChanged(proxyTopLeft, proxyBottomRight, roles);

		for (auto row = sourceTopLeft.row(); row <= sourceBottomRight.row(); ++row)
		{
			auto sourceIndex = sourceTopLeft.sibling(row, 0);
			auto it = m_sourceMapping.constFind(sourceIndex);
			if (it == m_sourceMapping.constEnd())
			{
				continue; // index not mapped
			}
			SourceModelMount mount = it->mount;
			if (SourceModelMount() != mount)
			{
				m_pModel->MountPointDataChanged(sourceIndex, mount.key());
			}
			else
			{
				mount = CreateMountFromFactory(sourceIndex);
				if (SourceModelMount() != mount)
				{
					auto rowCount = mount.key()->rowCount();
					if (0 != rowCount)
					{
						auto proxyIndex = m_pModel->MapFromSource(sourceIndex);
						SetBusy(true);
						m_pModel->beginInsertRows(proxyIndex, 0, rowCount - 1);
					}
					m_sourceMapping[sourceIndex].mount = mount;
					if (0 != rowCount)
					{
						SetBusy(false);
						m_pModel->endInsertRows();
					}
				}
			}
		}
	}

	virtual void onSourceAboutToBeReset(const SourceModel* sourceModel) override
	{
		auto mit = m_sourceModelMounts.constFind(sourceModel);
		if (mit == m_sourceModelMounts.constEnd())
		{
			return; // this may happen if signals are queued
		}
		auto sourceIndex = mit.value().sourceIndex;
		if (!sourceIndex.isValid())
		{
			SetBusy(true);
			m_pModel->beginResetModel();
			return; // main model resets
		}

		// remove children at mountpoint
		auto rowCount = sourceModel->rowCount();
		if (0 != rowCount)
		{
			auto proxyIndex = m_pModel->MapFromSource(sourceIndex);
			SetBusy(true);
			m_pModel->beginRemoveRows(proxyIndex, 0, rowCount - 1);
		}
		DeepRemoveSourceMappings(sourceModel, QModelIndex(), 0, rowCount);
		m_sourceMapping[sourceIndex].mount = SourceModelMount();
		if (0 != rowCount)
		{
			SetBusy(false);
			m_pModel->endRemoveRows();
		}
	}

	virtual void onSourceReset(const SourceModel* sourceModel) override
	{
		auto mit = m_sourceModelMounts.constFind(sourceModel);
		if (mit == m_sourceModelMounts.constEnd())
		{
			return;
		}
		auto sourceIndex = mit.value().sourceIndex;
		if (!sourceIndex.isValid())
		{
			ClearPersistedIndices();

			m_sourceMapping.clear();
			for (auto it = m_sourceModelMounts.begin(); it != m_sourceModelMounts.end(); )
			{
				if (it.key() != sourceModel)
				{
					DestroyMountPoint(it);
					it = m_sourceModelMounts.erase(it);
				}
				else
				{
					Mapping mapping;
					mapping.mount = it;
					m_sourceMapping.insert(sourceIndex, mapping);
					++it;
				}
			}

			SetBusy(false);
			m_pModel->endResetModel();
			return; // main model reseted
		}

		// insert children at mountpoint
		auto rowCount = sourceModel->rowCount();
		if (0 != rowCount)
		{
			auto proxyIndex = m_pModel->MapFromSource(sourceIndex);
			SetBusy(true);
			m_pModel->beginInsertRows(proxyIndex, 0, rowCount - 1);
		}
		m_sourceMapping[sourceIndex].mount = mit;
		if (0 != rowCount)
		{
			SetBusy(false);
			m_pModel->endInsertRows();
		}
	}

	virtual void onSourceLayoutAboutToBeChanged(const SourceModel* sourceModel, const PersistentModelIndexList& sourceParents, LayoutChangeHint hint) override
	{
		PersistentModelIndexList allSourceParents = sourceParents;
		if (sourceParents.isEmpty() && sourceModel != m_pModel->GetSourceModel())
		{
			CollectSourceParents(allSourceParents, sourceModel, QModelIndex());
		}
		QList<QPersistentModelIndex> proxyParents;
		proxyParents.reserve(allSourceParents.size());
		foreach(auto & sourceParent, allSourceParents)
		{
			auto mountIndex = GetMountedSourceIndex(sourceModel, sourceParent);
			proxyParents << m_pModel->MapFromSource(mountIndex);
		}
		m_pModel->layoutAboutToBeChanged(proxyParents, hint);

		StoreMountPoints(m_storedMountPoints, sourceModel, allSourceParents);
		CollectPersistedIndices(m_storedPersistentIndexes, sourceModel);

		RemoveChildSourceMappings(sourceModel, allSourceParents);
	}

	virtual void onSourceLayoutChanged(const SourceModel* sourceModel, const PersistentModelIndexList& sourceParents, LayoutChangeHint hint) override
	{
		UpdateMountPoints(m_storedMountPoints);
		m_storedMountPoints.clear();

		UpdatePersistedIndices(m_storedPersistentIndexes);
		m_storedPersistentIndexes.clear();

		auto allSourceParents = sourceParents;
		if (sourceParents.isEmpty() && sourceModel != m_pModel->GetSourceModel())
		{
			CollectSourceParents(allSourceParents, sourceModel, QModelIndex());
		}
		QList<QPersistentModelIndex> proxyParents;
		proxyParents.reserve(allSourceParents.size());
		foreach(auto & sourceParent, allSourceParents)
		{
			auto mountIndex = GetMountedSourceIndex(sourceModel, sourceParent);
			proxyParents << m_pModel->MapFromSource(mountIndex);
		}
		m_pModel->layoutChanged(proxyParents, hint);
	}

	virtual void onSourceRowsAboutToBeInserted(const SourceModel* sourceModel, const QModelIndex& sourceParent, int first, int last) override
	{
		auto proxyParent = GetMountedProxyIndex(sourceModel, sourceParent);
		SetBusy(true);
		m_pModel->beginInsertRows(proxyParent, first, last);

		auto rowCount = sourceModel->rowCount(sourceParent);
		CollectSourceMappings(m_storedSourceMappings, sourceModel, sourceParent, first, rowCount);
	}

	virtual void onSourceRowsInserted(const SourceModel* sourceModel, const QModelIndex& sourceParent, int first, int last) override
	{
		auto rowDrift = last - first + 1;
		MoveSourceIndexes(m_storedSourceMappings, sourceModel, sourceParent, rowDrift);
		m_storedSourceMappings.clear();

		SetBusy(false);
		m_pModel->endInsertRows();
	}

	virtual void onSourceRowsAboutToBeRemoved(const SourceModel* sourceModel, const QModelIndex& sourceParent, int first, int last) override
	{
		auto proxyParent = GetMountedProxyIndex(sourceModel, sourceParent);
		SetBusy(true);
		m_pModel->beginRemoveRows(proxyParent, first, last);

		auto rowCount = sourceModel->rowCount(sourceParent);
		CollectSourceMappings(m_storedSourceMappings, sourceModel, sourceParent, last + 1, rowCount);
		DeepRemoveSourceMappings(sourceModel, sourceParent, first, last + 1);
		m_bAllowRemove = true;
	}

	virtual void onSourceRowsRemoved(const SourceModel* sourceModel, const QModelIndex& sourceParent, int first, int last) override
	{
		if (!m_bAllowRemove)
			return;

		auto rowDrift = last - first + 1;
		MoveSourceIndexes(m_storedSourceMappings, sourceModel, sourceParent, -rowDrift);
		m_storedSourceMappings.clear();
		SetBusy(false);
		m_pModel->endRemoveRows();
		m_bAllowRemove = false;
	}

	virtual void onSourceRowsAboutToBeMoved(
	  const SourceModel* sourceModel,
	  const QModelIndex& sourceParent, int sourceStart, int sourceEnd,
	  const QModelIndex& destinationParent, int destinationRow) override
	{
		Q_UNUSED(sourceStart);
		Q_UNUSED(sourceEnd);
		Q_UNUSED(destinationRow);

		PersistentModelIndexList sourceParents;
		PersistentModelIndexList proxyParents;
		sourceParents << sourceParent;
		proxyParents << GetMountedProxyIndex(sourceModel, sourceParent);
		if (sourceParent != destinationParent)
		{
			sourceParents << destinationParent;
			proxyParents << GetMountedProxyIndex(sourceModel, destinationParent);
		}
		m_pModel->layoutAboutToBeChanged(proxyParents);

		StoreMountPoints(m_storedMountPoints, sourceModel, sourceParents);
		CollectPersistedIndices(m_storedPersistentIndexes, sourceModel);

		RemoveChildSourceMappings(sourceModel, sourceParents);

		// Note: This version does not work because QHash cannot change indices and QAbstractItemModel insists it can adjust indices without changing the pointer
		//
		//		auto proxySource = GetMountedProxyIndex(sourceModel, sourceParent);
		//		auto proxyDestination = GetMountedProxyIndex(sourceModel, destinationParent);
		//		m_pModel->beginMoveRows(proxySource, sourceStart, sourceEnd, proxyDestination, destinationRow);
		//		if (sourceParent == destinationParent)
		//		{
		//			auto moveCount = 1 + sourceEnd - sourceStart;
		//			auto min = qMin(sourceStart, destinationRow);
		//			auto max = qMax(sourceEnd, destinationRow - moveCount);
		//			CollectSourceMappings(m_storedSourceMappings, sourceModel, sourceParent, min, max + 1);
		//		}
		//		else
		//		{
		//			// all indices after start have to be changed
		//			auto sourceRowCount = sourceModel->rowCount(sourceParent);
		//			CollectSourceMappings(m_storedSourceMappings, sourceModel, sourceParent, sourceStart, sourceRowCount);
		//			// all indices after row have to be changed
		//			auto destinationRowCount = sourceModel->rowCount(destinationParent);
		//			CollectSourceMappings(m_storedDestinationMappings, sourceModel, destinationParent, destinationRow, destinationRowCount);
		//		}
	}

	virtual void onSourceRowsMoved(
	  const SourceModel* sourceModel,
	  const QModelIndex& sourceParent, int sourceStart, int sourceEnd,
	  const QModelIndex& destinationParent, int destinationRow) override
	{
		Q_UNUSED(sourceStart);
		Q_UNUSED(sourceEnd);
		Q_UNUSED(destinationRow);

		UpdateMountPoints(m_storedMountPoints);
		m_storedMountPoints.clear();

		UpdatePersistedIndices(m_storedPersistentIndexes);
		m_storedPersistentIndexes.clear();

		PersistentModelIndexList proxyParents;
		proxyParents << GetMountedProxyIndex(sourceModel, sourceParent);
		if (sourceParent != destinationParent)
		{
			proxyParents << GetMountedProxyIndex(sourceModel, destinationParent);
		}
		m_pModel->layoutChanged(proxyParents);

		// Note: See comment in onSourceRowsAboutToBeMoved()
		//
		//		if (sourceParent == destinationParent)
		//		{
		//			auto moveCount = 1 + sourceEnd - sourceStart; // rows that are moved
		//			auto moveDrift = destinationRow - sourceStart;
		//			auto displaceDrift = moveCount;
		//			if (moveDrift > 0)
		//			{
		//				displaceDrift *= -1;
		//			}
		//			SourceIndexMappingPairVector updates;
		//			updates.reserve(m_storedSourceMappings.size());
		//			for (SourceMapping it : m_storedSourceMappings)
		//			{
		//				auto oldRow = it.key().row();
		//				auto newRow = (oldRow < sourceStart || oldRow > sourceEnd)
		//						? oldRow + displaceDrift
		//						: oldRow + moveDrift;
		//				auto newSourceIndex = sourceModel->index(newRow, 0, sourceParent);
		//				updates << GetUpdateAndRemoveSourceMapping(it, newSourceIndex);
		//			}
		//			foreach (auto& pair, updates)
		//			{
		//				m_sourceMapping.insert(pair.first, pair.second);
		//			}
		//		}
		//		else
		//		{
		//			auto rowDrift = sourceEnd - sourceStart + 1;
		//			MoveSourceIndexes(m_storedDestinationMappings, sourceModel, destinationParent, rowDrift); // make space in destination
		//			auto destinationDrift = destinationRow - sourceStart;
		//			MoveSourceIndexes(m_storedSourceMappings, -rowDrift, [&](SourceMapping it)
		//			{
		//				auto oldRow = it.key().row();
		//				return oldRow <= sourceEnd
		//						? sourceModel->index(oldRow + destinationDrift, 0, destinationParent)
		//						: sourceModel->index(oldRow - rowDrift, 0, sourceParent);
		//			});
		//			m_storedDestinationMappings.clear();
		//		}
		//		m_storedSourceMappings.clear();
		//		m_pModel->endMoveRows();
	}

private:
	void SetBusy(bool busy)
	{
		CRY_ASSERT(m_isBusy != busy);
		m_isBusy = busy;
	}

	bool m_bAllowRemove;
	bool m_isBusy{ false };
};

CMountingProxyModel::CMountingProxyModel(const SourceModelFactory& factory, QObject* parent)
	: QAbstractItemModel(parent)
	, p(new Implementation(factory, this))
{}

CMountingProxyModel::~CMountingProxyModel()
{}

void CMountingProxyModel::SetSourceModel(SourceModel* newSourceModel)
{
	beginResetModel();

	p->ClearPersistedIndices();
	p->ClearMappings();

	if (newSourceModel)
	{
		// add the initial source mapping and mount point
		auto sourceIndex = QModelIndex();
		auto mount = p->CreateMountPoint(sourceIndex, newSourceModel);

		Implementation::Mapping mapping;
		mapping.mount = mount;

		p->m_sourceMapping.insert(sourceIndex, mapping);
	}

	endResetModel();
}

void CMountingProxyModel::SetHeaderDataCallbacks(int columnCount, const GetHeaderDataCallback& getHeaderData, int mergeValueRole, const SetHeaderDataCallback& setHeaderData)
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

void CMountingProxyModel::SetDragCallback(const std::function<QMimeData*(const QModelIndexList&)>& callback)
{
	if (callback)
		p->m_dragCallback = callback;
}

const QAbstractItemModel* CMountingProxyModel::GetSourceModel() const
{
	auto it = p->m_sourceMapping.constFind(QModelIndex());
	if (it == p->m_sourceMapping.constEnd())
	{
		CRY_ASSERT(p->m_sourceMapping.isEmpty());
		return nullptr;
	}
	if (Implementation::SourceModelMount() == it.value().mount)
	{
		return nullptr;
	}
	return it.value().mount.key();
}

const QAbstractItemModel* CMountingProxyModel::GetMountSourceModel(const ProxyIndex& proxyIndex) const
{
	auto it = p->GetMappingFromProxy(proxyIndex);
	if (Implementation::SourceModelMount() == it->mount)
	{
		return nullptr;
	}
	return it->mount.key();
}

CMountingProxyModel::SourceIndex CMountingProxyModel::MapToSource(const ProxyIndex& proxyIndex) const
{
	if (!proxyIndex.isValid())
	{
		return QModelIndex();
	}
	auto it = p->GetMappingFromProxy(proxyIndex);
	auto sourceIndex = it.key();
	CRY_ASSERT(sourceIndex.row() == proxyIndex.row());
	CRY_ASSERT(sourceIndex.column() == 0);
	auto sourceColumn = p->MapColumnFromProxy(sourceIndex.model(), proxyIndex.column());
	return sourceIndex.sibling(sourceIndex.row(), sourceColumn);
}

CMountingProxyModel::ProxyIndex CMountingProxyModel::MapFromSource(const SourceIndex& sourceIndex) const
{
	if (!sourceIndex.isValid())
	{
		return QModelIndex();
	}
	auto it = p->CreateMappingFromSource(sourceIndex);
	auto proxyRow = sourceIndex.row();
	CRY_ASSERT(proxyRow == it.key().row());
	auto proxyColumn = p->MapColumnFromSource(sourceIndex.model(), sourceIndex.column());
	return p->CreateIndexFromMapping(proxyRow, proxyColumn, it);
}

CMountingProxyModel::ProxyIndex CMountingProxyModel::MapFromSourceModel(const SourceModel* sourceModel) const
{
	CRY_ASSERT(sourceModel);
	auto it = p->m_sourceModelMounts.constFind(sourceModel);
	if (it != p->m_sourceModelMounts.constEnd())
	{
		return MapFromSource(it.value().sourceIndex);
	}
	return ProxyIndex();
}

QModelIndex CMountingProxyModel::index(int proxyRow, int proxyColumn, const QModelIndex& proxyParent) const
{
	if (proxyRow < 0 || proxyColumn < 0)
	{
		return QModelIndex();
	}
	auto it = p->GetMappingFromProxy(proxyParent);
	auto sourceParent = it.key();
	auto sourceIndex = Implementation::CreateSourceChildIndex(proxyRow, 0, it->mount, sourceParent, GetSourceModel());
	return MapFromSource(sourceIndex).sibling(proxyRow, proxyColumn);
}

QModelIndex CMountingProxyModel::parent(const QModelIndex& proxyChild) const
{
	auto it = p->GetMappingFromProxy(proxyChild);
	auto sourceIndex = it.key();
	auto sourceParent = sourceIndex.parent();
	if (!sourceParent.isValid())
	{
		auto mit = p->m_sourceModelMounts.constFind(sourceIndex.model());
		if (mit != p->m_sourceModelMounts.constEnd())
		{
			sourceParent = mit.value().sourceIndex;
		}
	}
	return MapFromSource(sourceParent);
}

QModelIndex CMountingProxyModel::sibling(int proxyRow, int proxyColumn, const QModelIndex& proxyIndex) const
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
	auto sourceRow = proxyRow;
	auto sourceColumn = p->MapColumnFromProxy(it.key().model(), proxyColumn);
	auto sourceIndex = it.key().sibling(sourceRow, sourceColumn);
	return MapFromSource(sourceIndex);
}

int CMountingProxyModel::rowCount(const QModelIndex& parent) const
{
	if (p->m_sourceModelMounts.isEmpty())
	{
		return 0;
	}
	auto it = p->GetMappingFromProxy(parent);
	auto mount = it->mount;
	auto sourceParent = it.key();
	if (Implementation::SourceModelMount() != mount)
	{
		return mount.key()->rowCount();
	}
	CRY_ASSERT(sourceParent.isValid());
	return sourceParent.model()->rowCount(sourceParent);
}

int CMountingProxyModel::columnCount(const QModelIndex&) const
{
	return p->m_columnMapping.GetColumnCount();
}

bool CMountingProxyModel::hasChildren(const QModelIndex& proxyParent) const
{
	if (!proxyParent.isValid())
	{
		return true;
	}
	if (proxyParent.column() != 0)
	{
		return false;
	}
	auto it = p->GetMappingFromProxy(proxyParent);
	auto mount = it->mount;
	if (Implementation::SourceModelMount() != mount)
	{
		return true; // required for the lazy QTreeView
	}
	auto sourceParent = it.key();
	if (!sourceParent.isValid())
	{
		return false;
	}
	return sourceParent.model()->hasChildren(sourceParent);
}

Qt::ItemFlags CMountingProxyModel::flags(const QModelIndex& proxyIndex) const
{
	if (!proxyIndex.isValid())
	{
		return GetSourceModel()->flags(proxyIndex);
	}
	auto sourceIndex = MapToSource(proxyIndex);
	if (!sourceIndex.isValid())
	{
		return 0;
	}
	return sourceIndex.model()->flags(sourceIndex);
}

QVariant CMountingProxyModel::data(const QModelIndex& proxyIndex, int role) const
{
	if (!proxyIndex.isValid())
	{
		return GetSourceModel()->data(proxyIndex, role);
	}
	auto sourceIndex = MapToSource(proxyIndex);
	if (!sourceIndex.isValid())
	{
		return QVariant();
	}
	return sourceIndex.model()->data(sourceIndex, role);
}

bool CMountingProxyModel::setData(const QModelIndex& proxyIndex, const QVariant& value, int role)
{
	if (!proxyIndex.isValid())
	{
		return const_cast<Implementation::SourceModel*>(GetSourceModel())->setData(proxyIndex, role);
	}
	auto sourceIndex = MapToSource(proxyIndex);
	if (!sourceIndex.isValid())
	{
		return false;
	}
	return const_cast<Implementation::SourceModel*>(sourceIndex.model())->setData(sourceIndex, value, role);
}

QVariant CMountingProxyModel::headerData(int proxySection, Qt::Orientation orientation, int role) const
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

bool CMountingProxyModel::setHeaderData(int proxySection, Qt::Orientation orientation, const QVariant& value, int role)
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

QMap<int, QVariant> CMountingProxyModel::itemData(const QModelIndex& proxyIndex) const
{
	if (!proxyIndex.isValid())
	{
		return QMap<int, QVariant>();
	}
	auto sourceIndex = MapToSource(proxyIndex);
	if (!sourceIndex.isValid())
	{
		return QMap<int, QVariant>();
	}
	return sourceIndex.model()->itemData(sourceIndex);
}

bool CMountingProxyModel::setItemData(const QModelIndex& proxyIndex, const QMap<int, QVariant>& roles)
{
	if (!proxyIndex.isValid())
	{
		return false;
	}
	auto sourceIndex = MapToSource(proxyIndex);
	if (!sourceIndex.isValid())
	{
		return false;
	}
	return const_cast<Implementation::SourceModel*>(sourceIndex.model())->setItemData(sourceIndex, roles);
}

QStringList CMountingProxyModel::mimeTypes() const
{
	QSet<QString> set;
	for (auto it = p->m_sourceModelMounts.constBegin(); it != p->m_sourceModelMounts.constEnd(); it++)
	{
		auto mimeTypes = it.key()->mimeTypes();
		set.unite(mimeTypes.toSet());
	}
	return set.toList();
}

QMimeData* CMountingProxyModel::mimeData(const QModelIndexList& proxyIndexes) const
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

	// If there's a drag callback then just call that
	if (p->m_dragCallback)
		return p->m_dragCallback(sourceIndexList);

	return sourceModel->mimeData(sourceIndexList);
}

bool CMountingProxyModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int proxyRow, int proxyColumn, const QModelIndex& proxyParent) const
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

bool CMountingProxyModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int proxyRow, int proxyColumn, const QModelIndex& proxyParent)
{
	int sourceRow, sourceColumn;
	QModelIndex sourceParent;
	const SourceModel* sourceModel;
	p->MapDropCoordinatesToSource(proxyRow, proxyColumn, proxyParent, &sourceRow, &sourceColumn, &sourceParent, &sourceModel);
	auto noConstSourceModel = const_cast<SourceModel*>(sourceModel);
	return noConstSourceModel->dropMimeData(data, action, sourceRow, sourceColumn, sourceParent);
}

Qt::DropActions CMountingProxyModel::supportedDropActions() const
{
	return GetSourceModel()->supportedDropActions();
}

Qt::DropActions CMountingProxyModel::supportedDragActions() const
{
	return GetSourceModel()->supportedDragActions();
}

bool CMountingProxyModel::canFetchMore(const QModelIndex& proxyParent) const
{
	auto it = p->GetMappingFromProxy(proxyParent);
	auto mount = it->mount;
	auto sourceParent = it.key();
	if (Implementation::SourceModelMount() != mount)
	{
		return mount.key()->canFetchMore(QModelIndex());
	}
	if (!sourceParent.isValid())
	{
		return false;
	}
	return sourceParent.model()->canFetchMore(sourceParent);
}

void CMountingProxyModel::fetchMore(const QModelIndex& proxyParent)
{
	auto it = p->GetMappingFromProxy(proxyParent);
	auto mount = it->mount;
	auto sourceParent = it.key();
	if (Implementation::SourceModelMount() != mount)
	{
		const_cast<SourceModel*>(mount.key())->fetchMore(QModelIndex());
		return;
	}
	if (!sourceParent.isValid())
	{
		return;
	}
	const_cast<SourceModel*>(sourceParent.model())->fetchMore(sourceParent);
}

void CMountingProxyModel::sort(int proxyColumn, Qt::SortOrder order)
{
	auto newSort = CColumnSort(proxyColumn, order);
	if (p->m_sort == newSort)
	{
		return; // nothing changed
	}
	p->m_sort = newSort;
	// always sort submodels (enables to remove sort)
	for (auto it = p->m_sourceModelMounts.constBegin(); it != p->m_sourceModelMounts.constEnd(); it++)
	{
		auto sourceColumn = p->MapColumnFromProxy(it.key(), proxyColumn);
		auto sourceModel = const_cast<SourceModel*>(it.key());
		sourceModel->sort(sourceColumn, order);
	}
}

QModelIndex CMountingProxyModel::buddy(const QModelIndex& proxyIndex) const
{
	if (!proxyIndex.isValid())
	{
		return QModelIndex();
	}
	auto sourceIndex = MapToSource(proxyIndex);
	if (!sourceIndex.isValid())
	{
		return QModelIndex();
	}
	auto sourceBuddy = sourceIndex.model()->buddy(sourceIndex);
	return MapFromSource(sourceBuddy);
}

QSize CMountingProxyModel::span(const QModelIndex& proxyIndex) const
{
	if (!proxyIndex.isValid())
	{
		return GetSourceModel()->span(proxyIndex);
	}
	auto sourceIndex = MapToSource(proxyIndex);
	if (!sourceIndex.isValid())
	{
		return QSize();
	}
	return sourceIndex.model()->span(sourceIndex);
}

QHash<int, QByteArray> CMountingProxyModel::roleNames() const
{
	QHash<int, QByteArray> result;
	for (auto it = p->m_sourceModelMounts.constBegin(); it != p->m_sourceModelMounts.constEnd(); it++)
	{
		auto mountRoles = it.key()->roleNames();
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

void CMountingProxyModel::ColumnsChanged(int columnCount)
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

void CMountingProxyModel::MountToSource(const QPersistentModelIndex& sourceIndex, QAbstractItemModel* sourceModel)
{
	if (!sourceIndex.isValid())
	{
		return;
	}
	auto proxyIndex = MapFromSource(sourceIndex);
	Mount(proxyIndex, sourceModel);
}

void CMountingProxyModel::Mount(const QModelIndex& proxyIndex, QAbstractItemModel* sourceModel)
{
	if (!proxyIndex.isValid())
	{
		return; // cannot mount to root
	}
	auto it = p->GetMappingFromProxy(proxyIndex);
	if (!p->m_sourceMapping.contains(it.key()))
	{
		return; // key seems outdated
	}
	auto oldMount = it.value().mount;
	if (Implementation::SourceModelMount() != oldMount)
	{
		return; // already mounted
	}
	if (hasChildren(proxyIndex))
	{
		return; // do not shadow children
	}
	if (nullptr == sourceModel->QObject::parent())
	{
		sourceModel->QObject::setParent(this);
	}

	auto mount = p->CreateMountPoint(it.key(), sourceModel);

	auto mountRows = sourceModel->rowCount();
	if (0 != mountRows)
	{
		beginInsertRows(proxyIndex, 0, mountRows - 1);
	}
	p->m_sourceMapping[it.key()].mount = mount;
	if (0 != mountRows)
	{
		endInsertRows();
	}
	dataChanged(proxyIndex, proxyIndex);
}

void CMountingProxyModel::Unmount(const QModelIndex& proxyIndex)
{
	if (!proxyIndex.isValid())
	{
		return; // cannot unmount to root
	}
	auto it = p->GetMappingFromProxy(proxyIndex);
	if (!p->m_sourceMapping.contains(it.key()))
	{
		return; // key seems outdated
	}
	auto mount = it->mount;
	if (Implementation::SourceModelMount() == mount)
	{
		return; // nothing mounted
	}

	auto mountRows = mount.key()->rowCount();
	if (0 != mountRows)
	{
		beginRemoveRows(proxyIndex, 0, mountRows - 1);
	}

	p->DeepRemoveSourceMappings(mount.key(), QModelIndex(), 0, mountRows);
	p->m_sourceMapping[it.key()].mount = Implementation::SourceModelMount();

	if (0 != mountRows)
	{
		endRemoveRows();
	}

	p->RemoveMount(mount);

	dataChanged(proxyIndex, proxyIndex);
}

#undef reverse_foreach

