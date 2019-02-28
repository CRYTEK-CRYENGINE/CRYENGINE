// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QMetaObject>
#include <QObject>
#include <QAbstractItemModel>

struct CAbstractMultiProxyModelHelper
{
	typedef QAbstractItemModel                   SourceModel;
	typedef QAbstractItemModel                   ProxyModel;
	typedef QVector<QMetaObject::Connection>     Connections;
	typedef QAbstractItemModel::LayoutChangeHint LayoutChangeHint;

	typedef QModelIndex                          SourceIndex;
	typedef QModelIndex                          ProxyIndex;
	typedef QList<QPersistentModelIndex>         PersistentModelIndexList;

	CAbstractMultiProxyModelHelper() : m_storedMovedSituation(-1) {}
	virtual ~CAbstractMultiProxyModelHelper() {}

	// return true if the sourceIndex has a mapped proxy index
	// - this is used to split move into a insert/remove or real move on the proxy model
	virtual bool IsSourceIndexMapped(const SourceModel*, const SourceIndex&) const = 0;

	// for data changed you can get the source model of the indices (they are required to be valid)
	virtual void onSourceDataChanged(const SourceIndex& topLeft, const SourceIndex& bottomRight, const QVector<int>& roles) = 0;
	// all other handlers get the source model
	virtual void onSourceAboutToBeReset(const SourceModel*) = 0;
	virtual void onSourceReset(const SourceModel*) = 0;
	virtual void onSourceLayoutAboutToBeChanged(const SourceModel*, const PersistentModelIndexList& sourceParents, LayoutChangeHint hint) = 0;
	virtual void onSourceLayoutChanged(const SourceModel*, const PersistentModelIndexList& sourceParents, LayoutChangeHint hint) = 0;
	virtual void onSourceRowsAboutToBeInserted(const SourceModel*, const SourceIndex& sourceParent, int first, int last) = 0;
	virtual void onSourceRowsInserted(const SourceModel*, const SourceIndex& sourceParent, int first, int last) = 0;
	virtual void onSourceRowsAboutToBeRemoved(const SourceModel*, const SourceIndex& sourceParent, int first, int last) = 0;
	virtual void onSourceRowsRemoved(const SourceModel*, const SourceIndex& sourceParent, int first, int last) = 0;
	virtual void onSourceRowsAboutToBeMoved(const SourceModel*, const SourceIndex& sourceParent, int sourceStart, int sourceEnd, const QModelIndex& destinationParent, int destinationRow) = 0;
	virtual void onSourceRowsMoved(const SourceModel*, const SourceIndex& sourceParent, int sourceStart, int sourceEnd, const QModelIndex& destinationParent, int destinationRow) = 0;

	// TODO: add support for columns!

	Connections ConnectSourceModel(const SourceModel*, QObject*);
	void        Disconnect(const Connections&);

private:
	int m_storedMovedSituation;
};

