// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include "EditorCommonAPI.h"

#include <QAbstractItemModel>

#include <memory>
#include <functional>

/**
 * Allows multiple sourceModels.
 * Merges Attributes of all sourceModels
 *
 * Note: 
 *  * Does not support column inserted/removed/moved. We assume those will not change at runtime.
 *  * Only supports models that are isomorphic in columns
 *
 * Example:
 * /       - invalid proxy index (main root) - mountpoint for model A, B (in that order)
 *  a1     - root children of first mounted model A
 *  a...   - more items of model A
 *  b1     - root children of second mounted model B
 *  b...   - more items of model B
 */
class EDITOR_COMMON_API CMergingProxyModel
	: public QAbstractItemModel // is not a QAbstractProxyModel because it has multiple sourceModels
{
	Q_OBJECT
public:
	typedef QModelIndex                                                                   SourceIndex;
	typedef QModelIndex                                                                   ProxyIndex;
	typedef QAbstractItemModel                                                            SourceModel;
	typedef QAbstractItemModel                                                            SubModel;

	typedef std::function<QVariant(int section, Qt::Orientation, int role)>               GetHeaderDataCallback;
	typedef std::function<bool (int section, Qt::Orientation, const QVariant&, int role)> SetHeaderDataCallback;

public:
	CMergingProxyModel(QObject* parent = nullptr);
	~CMergingProxyModel();

	SourceIndex                 MapToSource(const ProxyIndex& proxyIndex) const;
	ProxyIndex                  MapFromSource(const SourceIndex& sourceIndex) const;

	QVector<const SourceModel*> GetSourceModels() const;
	int                         GetSourceModelCount() const;
	const SourceModel*          GetSourceModel(int index) const;

	//! Add source model to end of mounts
	void MountAppend(SourceModel* sourceModel);

	//! Add a source model as index-ed sourceModel
	void Mount(SourceModel* sourceModel, int index);

	//! remove a source model from an index
	void Unmount(SourceModel* sourceModel);

	//! remove all source models
	void UnmountAll();

	/// \brief set the header data callbacks and the column count for the final model
	/// \note mergeValueRole is used to map all mounted models to the columns
	void SetHeaderDataCallbacks(int columnCount, const GetHeaderDataCallback&, int mergeValueRole = Qt::DisplayRole, const SetHeaderDataCallback& = SetHeaderDataCallback());

	// Set the drag callback override. Specially useful if we want to support dragging items from multiple source models
	void SetDragCallback(const std::function<QMimeData*(const QModelIndexList&)>& callback);

public slots:
	// these are moc slots to enable signal passing through threads
	void ColumnsChanged(int columnCount);

	// QAbstractItemModel interface
public:
	virtual QModelIndex            index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	virtual QModelIndex            parent(const QModelIndex& child) const override;
	virtual QModelIndex            sibling(int row, int column, const QModelIndex&) const override;
	virtual int                    rowCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int                    columnCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual bool                   hasChildren(const QModelIndex& parent = QModelIndex()) const override;
	virtual Qt::ItemFlags          flags(const QModelIndex& index) const override;
	virtual QVariant               data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	virtual bool                   setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
	virtual QVariant               headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	virtual bool                   setHeaderData(int section, Qt::Orientation orientation, const QVariant& value, int role = Qt::EditRole) override;
	virtual QMap<int, QVariant>    itemData(const QModelIndex& index) const override;
	virtual bool                   setItemData(const QModelIndex& index, const QMap<int, QVariant>& roles) override;
	virtual QStringList            mimeTypes() const override;
	virtual QMimeData*             mimeData(const QModelIndexList& indexes) const override;
	virtual bool                   canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const override;
	virtual bool                   dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;
	virtual Qt::DropActions        supportedDropActions() const override;
	virtual Qt::DropActions        supportedDragActions() const override;
	virtual bool                   canFetchMore(const QModelIndex& parent = QModelIndex()) const override;
	virtual void                   fetchMore(const QModelIndex& parent = QModelIndex()) override;
	virtual void                   sort(int column, Qt::SortOrder order) override;
	virtual QModelIndex            buddy(const QModelIndex& index) const override;
	virtual QSize                  span(const QModelIndex& index) const override;
	virtual QHash<int, QByteArray> roleNames() const override;

private:
	struct Implementation;
	std::unique_ptr<Implementation> p;
};

