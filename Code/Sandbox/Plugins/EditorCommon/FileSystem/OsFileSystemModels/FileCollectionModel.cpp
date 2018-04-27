// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileCollectionModel.h"

#include "DragDrop.h"

#include <QFileSystemModel>
#include <QMimeData>

CFileCollectionModel::CFileCollectionModel()
{

}

CFileCollectionModel::~CFileCollectionModel()
{

}

void CFileCollectionModel::AddEntry(const QString& file)
{
	if (!HasEntry(file))
	{
		QFileInfo fileInfo(file);
		if (!fileInfo.exists())
			return;

		beginInsertRows(QModelIndex(), m_entries.count(), m_entries.count());
		Entry entry;
		entry.m_fileInfo = fileInfo;
		entry.m_sourceIndex = GetSourceIndex(entry.m_fileInfo.absoluteFilePath());
		m_entries.append(entry);
		endInsertRows();
	}
}

void CFileCollectionModel::AddEntries(const QStringList& entries)
{
	for (auto& entry : entries)
		AddEntry(entry);
}

void CFileCollectionModel::InsertEntry(const QString& file, uint index)
{
	if (!HasEntry(file) && index >= 0 && index <= m_entries.count())
	{
		QFileInfo fileInfo(file);
		if (!fileInfo.exists())
			return;

		beginInsertRows(QModelIndex(), index, index);
		Entry entry;
		entry.m_fileInfo = fileInfo;
		entry.m_sourceIndex = GetSourceIndex(entry.m_fileInfo.absoluteFilePath());
		m_entries.insert(index, entry);
		endInsertRows();
	}
}

void CFileCollectionModel::InsertEntries(const QStringList& entries, uint index)
{
	for (int i = 0; i < entries.count(); i++)
	{
		auto file = entries[i];
		if (!HasEntry(file) && index >= 0 && index <= m_entries.count())
		{
			QFileInfo fileInfo(file);
			if (!fileInfo.exists())
				return;

			beginInsertRows(QModelIndex(), index, index);
			Entry entry;
			entry.m_fileInfo = fileInfo;
			entry.m_sourceIndex = GetSourceIndex(entry.m_fileInfo.absoluteFilePath());
			m_entries.insert(index, entry);
			endInsertRows();

			index++;
		}
	}
}

void CFileCollectionModel::RemoveEntry(const QString& file)
{
	int index = 0;
	QFileInfo fileInfo(file);
	for (auto& entry : m_entries)
	{
		if (entry.m_fileInfo == fileInfo)
		{
			beginRemoveRows(QModelIndex(), index, index);
			m_entries.remove(index);
			endRemoveRows();
		}

		index++;
	}
}

void CFileCollectionModel::RemoveEntry(const QModelIndex& index)
{
	if(index.isValid())
	{
		CRY_ASSERT(index.model() == this);
		auto row = index.row();
		beginRemoveRows(QModelIndex(), row, row);
		m_entries.remove(row);
		endRemoveRows();
	}
}

bool CFileCollectionModel::HasEntry(const QString& file) const
{
	QFileInfo fileInfo(file);
	for (auto& entry : m_entries)
	{
		if (entry.m_fileInfo == fileInfo)
			return true;
	}

	return false;
}

QString CFileCollectionModel::GetEntry(int index) const
{
	if (index < 0 || index >= m_entries.count())
		return QString();

	return m_entries[index].m_fileInfo.absoluteFilePath();
}

QStringList CFileCollectionModel::GetEntries() const
{
	QStringList entries;
	entries.reserve(m_entries.count());

	for (auto& entry : m_entries)
		entries.append(entry.m_fileInfo.absoluteFilePath());
	
	return entries;
}

QVariant CFileCollectionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	return sourceModel()->headerData(section, orientation, role);
}

void CFileCollectionModel::Clear()
{
	beginResetModel();
	m_entries.clear();
	endResetModel();
}

int CFileCollectionModel::EntriesCount() const
{
	return m_entries.count();
}

QModelIndex CFileCollectionModel::mapToSource(const QModelIndex &proxyIndex) const
{
	if (proxyIndex == QModelIndex())
		return QModelIndex();
	else
	{
		const int row = proxyIndex.row();
		CRY_ASSERT(row >= 0 && row < m_entries.count());

		QModelIndex resolvedIndex(m_entries[row].m_sourceIndex);

		if (proxyIndex.column() == 0)
			return resolvedIndex;
		else
		{
			return resolvedIndex.sibling(resolvedIndex.row(), proxyIndex.column());
		}
	}
}

QModelIndex CFileCollectionModel::mapFromSource(const QModelIndex &sourceIndex) const
{
	int index = 0;
	for (auto& entry : m_entries)
	{
		if (entry.m_sourceIndex == sourceIndex)
			return createIndex(index, sourceIndex.column(), sourceIndex.internalId());

		index++;
	}

	return QModelIndex();
}

bool CFileCollectionModel::hasChildren(const QModelIndex &parent /*= QModelIndex()*/) const
{
	return !parent.isValid();
}

int CFileCollectionModel::columnCount(const QModelIndex &parent /*= QModelIndex()*/) const
{
	return sourceModel()->columnCount(mapToSource(parent));
}

int CFileCollectionModel::rowCount(const QModelIndex &parent /*= QModelIndex()*/) const
{
	if (!parent.isValid())
		return m_entries.count();
		
	return 0;
}

QModelIndex CFileCollectionModel::index(int row, int column, const QModelIndex &parent /*= QModelIndex()*/) const
{
	if(!parent.isValid() && row >= 0 && row < m_entries.count())
		return createIndex(row, column, m_entries[row].m_sourceIndex.internalId());
	
	return QModelIndex();
}

QModelIndex CFileCollectionModel::parent(const QModelIndex &child) const
{
	return QModelIndex();
}

QVariant CFileCollectionModel::data(const QModelIndex &index, int role /*= Qt::DisplayRole*/) const
{
	auto sourceIndex = mapToSource(index);
	if (sourceIndex.isValid())
		return sourceIndex.data(role);
	else
		return QVariant();
}

Qt::ItemFlags CFileCollectionModel::flags(const QModelIndex &index) const
{
	return QAbstractProxyModel::flags(index) | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
}

bool CFileCollectionModel::canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const
{
	const CDragDropData* dragDropData = CDragDropData::FromMimeData(data);
	return dragDropData->HasFilePaths();
}

bool CFileCollectionModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
	int insertRow = -1;
	if (parent.isValid())//dropped on a row
		insertRow = parent.row();
	else
		insertRow = row;

	const CDragDropData* dragDropData = CDragDropData::FromMimeData(data);
	if (dragDropData->HasFilePaths())
	{
		auto paths = dragDropData->GetFilePaths();

		if (paths.count() == 1 && HasEntry(paths[0]))//reordering is only handled for one entry, improve if necessary
			ReorderEntry(paths[0], insertRow);
		else if (insertRow == -1)
			AddEntries(paths);
		else
			InsertEntries(paths, insertRow);

		return true;
	}

	return false;
}

void CFileCollectionModel::ReorderEntry(const QString& entry, uint newIndex)
{
	QFileInfo fileInfo(entry);

	int from = 0;
	for (auto& entry : m_entries)
	{
		if (entry.m_fileInfo == fileInfo)
			break;

		from++;
	}

	int to = newIndex == -1 ? m_entries.count()-1 : newIndex;

	if(to != from)
	{
		beginMoveRows(QModelIndex(), from, from, QModelIndex(), to);
		m_entries.move(from, to);
		endMoveRows();
	}
}

QModelIndex CFileCollectionModel::GetSourceIndex(const QString &path, int column /*= 0*/) const
{
	//Note: this index should not be trusted and should be stored in a QPersistentModelIndex
	//The reason is that Qt has not loaded the full directory description when returning this index and rows are therefore invalid
	//This index will work fine for querying data() but is dangerous to use directly for proxy mappings
	auto source = sourceModel();
	QFileSystemModel* sourceModel = qobject_cast<QFileSystemModel*>(source);
	CRY_ASSERT(sourceModel);
	return sourceModel->index(path, column);
}

