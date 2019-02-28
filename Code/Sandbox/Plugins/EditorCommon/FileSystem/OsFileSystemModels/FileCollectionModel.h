// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include <QAbstractProxyModel>
#include <QFileInfo>

//! This will display a flat lists of selected entries. Despite the name the entries can be directories or files indiscriminately
//! Assumes the parent model is a CAdvancedFileSystemModel (or QFileSystemModel)
//! Drag&dropped 
class EDITOR_COMMON_API CFileCollectionModel : public QAbstractProxyModel
{
public:
	CFileCollectionModel();
	~CFileCollectionModel();

	void AddEntry(const QString& file);
	void AddEntries(const QStringList& entries);
	void InsertEntry(const QString& file, uint index);
	void InsertEntries(const QStringList& entries, uint index);
	void RemoveEntry(const QString& file);
	void RemoveEntry(const QModelIndex& index);
	void Clear();

	int EntriesCount() const;

	bool HasEntry(const QString& file) const;

	QString GetEntry(int index) const;
	QStringList GetEntries() const;

	//QAbstractItemModel interface
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;
	virtual QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override;
	virtual bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
	virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
	virtual QModelIndex parent(const QModelIndex &child) const override;

	virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

	virtual bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const override;
	virtual bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;

private:

	void ReorderEntry(const QString& entry, uint newIndex);
	QModelIndex GetSourceIndex(const QString &path, int column = 0) const;

	struct Entry
	{
		QFileInfo m_fileInfo;
		QPersistentModelIndex m_sourceIndex;
	};

	QVector<Entry> m_entries;
};
