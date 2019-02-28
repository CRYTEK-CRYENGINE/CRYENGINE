// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileListModel.h"

#include "CryIcon.h"
#include "DragDrop.h"

CFileListModel::CFileListModel(FileSystem::CEnumerator& enumerator, const FileSystem::SFileFilter& filter, QObject* pParent)
	: QAbstractListModel(pParent)
	, m_enumerator(enumerator)
{}

CFileListModel::~CFileListModel()
{}

void CFileListModel::AddEntry(const QString& enginePath)
{
	if (!HasEntry(enginePath))
	{
		auto entry = CreateEntry(enginePath);
		if (!entry.IsValid())
			return;
	
		beginInsertRows(QModelIndex(), m_entries.count(), m_entries.count());
		m_entries.append(entry);
		endInsertRows();
	}
}

void CFileListModel::AddEntries(const QStringList& enginePaths)
{
	for (auto& path : enginePaths)
		AddEntry(path);
}

void CFileListModel::InsertEntry(const QString& enginePath, uint index)
{
	if (!HasEntry(enginePath) && index >= 0 && index <= m_entries.count())
	{
		auto entry = CreateEntry(enginePath);
		if (!entry.IsValid())
			return;

		beginInsertRows(QModelIndex(), index, index);
		m_entries.insert(index, entry);
		endInsertRows();
	}
}

void CFileListModel::InsertEntries(const QStringList& enginePaths, uint index)
{
	for (int i = 0; i < enginePaths.count(); i++)
	{
		auto file = enginePaths[i];
		if (!HasEntry(file) && index >= 0 && index <= m_entries.count())
		{
			auto entry = CreateEntry(file);
			if (!entry.IsValid())
				return;

			beginInsertRows(QModelIndex(), index, index);
			m_entries.insert(index, entry);
			endInsertRows();

			index++;
		}
	}
}


void CFileListModel::ReorderEntry(const QString& entry, uint newIndex)
{
	int from = FindEntry(entry);
	CRY_ASSERT(from >= 0);

	int to = newIndex == -1 ? m_entries.count() - 1 : newIndex;

	if (to != from)
	{
		beginMoveRows(QModelIndex(), from, from, QModelIndex(), to);
		m_entries.move(from, to);
		endMoveRows();
	}
}

int CFileListModel::FindEntry(const QString& enginePath) const
{
	auto compare = CreateEntry(enginePath);
	if (!compare.IsValid())
		return -1;

	int index = 0;
	for (auto& entry : m_entries)
	{
		if (entry.enginePath == compare.enginePath)
		{
			return index;
		}

		index++;
	}

	return -1;
}

CFileListModel::Entry CFileListModel::CreateEntry(const QString& enginePath) const
{
	const FileSystem::SnapshotPtr& snapshot = m_enumerator.GetCurrentSnapshot();
	if (!snapshot)
	{
		return Entry();
	}

	FileSystem::FilePtr ptr = snapshot->GetFileByEnginePath(enginePath);//does not handle directories ...
	if (ptr)
	{
		Entry entry;
		entry.enginePath = ptr->enginePath.full;
		entry.filePtr = ptr;
		return entry;
	}
	
	FileSystem::DirectoryPtr dir = snapshot->GetDirectoryByEnginePath(enginePath);//does not handle directories ...
	if (dir)
	{
		Entry entry;
		entry.enginePath = dir->enginePath.full;
		entry.directoryPtr = dir;
		return entry;
	}

	return Entry();
}

void CFileListModel::RemoveEntry(const QString& enginePath)
{
	auto index = FindEntry(enginePath);
	if (index >= 0)
	{
		beginRemoveRows(QModelIndex(), index, index);
		m_entries.remove(index);
		endRemoveRows();
	}
}

void CFileListModel::RemoveEntry(const QModelIndex& index)
{
	if (index.isValid())
	{
		CRY_ASSERT(index.model() == this);
		auto row = index.row();
		beginRemoveRows(QModelIndex(), row, row);
		m_entries.remove(row);
		endRemoveRows();
	}
}

void CFileListModel::Clear()
{
	beginResetModel();
	m_entries.clear();
	endResetModel();
}

int CFileListModel::EntriesCount() const
{
	return m_entries.count();
}

bool CFileListModel::HasEntry(const QString& enginePath) const
{
	auto compare = CreateEntry(enginePath);
	if (!compare.IsValid())
		return false;

	for (auto& entry : m_entries)
	{
		if (entry.enginePath == compare.enginePath)
			return true;
	}
	
	return false;
}

QString CFileListModel::GetEntry(int index) const
{
	if (index < 0 || index >= m_entries.count())
		return QString();

	return m_entries[index].enginePath;
}

QStringList CFileListModel::GetEntries() const
{
	QStringList entries;
	entries.reserve(m_entries.count());

	for (auto& entry : m_entries)
		entries.append(entry.enginePath);

	return entries;
}

QModelIndex CFileListModel::GetIndexByPath(const QString& enginePath) const
{
	auto index = FindEntry(enginePath);
	if (index >= 0)
	{
		return QAbstractListModel::index(index);
	}

	return QModelIndex();
}

QString CFileListModel::GetPathFromIndex(const QModelIndex& index) const
{
	CRY_ASSERT(index.model() == this);

	return GetEntry(index.row());
}

int CFileListModel::rowCount(const QModelIndex& /*parent*/) const
{
	return m_entries.size();
}

QVariant CFileListModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
	{
		return QVariant();
	}

	const auto& entry = m_entries[index.row()];
	switch (role)
	{
	case Qt::DisplayRole:
		if(entry.IsFile())
			return entry.filePtr->provider->fullName;
		else
			return entry.directoryPtr->provider->fullName;

	case Qt::DecorationRole:
			if(entry.IsFile())
			{
				auto pType = entry.filePtr->type;
				// a filetype should always be present
				assert(pType);
				if (!pType)
				{
					return QVariant();
				}
				return CryIcon(pType->iconPath);
			}
			else
			{
				return CryIcon(SFileType::DirectoryType()->iconPath);
			}

	case eRole_EnginePath:
		return entry.enginePath;

	default:
		return QVariant();
	}
}

QVariant CFileListModel::headerData(int /*section*/, Qt::Orientation orientation, int role) const
{
	// Only horizontal header
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
	{
		return QVariant();
	}

	// section is always 0 for list
	return tr("Name");
}

QStringList CFileListModel::mimeTypes() const
{
	auto typeList = QAbstractItemModel::mimeTypes();
	typeList << CDragDropData::GetMimeFormatForType("EngineFilePaths");
	return typeList;
}

QMimeData* CFileListModel::mimeData(const QModelIndexList& indexes) const
{
	auto pDragDropData = new CDragDropData();

	QStringList enginePaths;
	QSet<int> rowsUsed;

	// when rows with multiple columns are selected the same
	// path would be added for every column index of a selected
	// row leading to multiple paths. Therefore a check is required
	for (const auto& index : indexes)
	{
		if (!rowsUsed.contains(index.row()))
		{
			// remove leading "/" if present
			auto filePath = GetPathFromIndex(index);
			if (!filePath.isEmpty() && filePath[0] == QChar('/'))
			{
				filePath.remove(0, 1);
			}

			enginePaths << filePath;
			rowsUsed.insert(index.row());
		}
	}

	QByteArray byteArray;
	QDataStream stream(&byteArray, QIODevice::ReadWrite);
	stream << enginePaths;
	// TODO: refactor hard coded mime type
	pDragDropData->SetCustomData("EngineFilePaths", byteArray);
	return pDragDropData;
}

bool CFileListModel::canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const
{
	const CDragDropData* dragDropData = CDragDropData::FromMimeData(data);
	auto test = dragDropData->HasCustomData("EngineFilePaths");
	return test;
}

bool CFileListModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
	int insertRow = -1;
	if (parent.isValid())//dropped on a row
		insertRow = parent.row();
	else
		insertRow = row;

	const CDragDropData* dragDropData = CDragDropData::FromMimeData(data);
	if (dragDropData->HasCustomData("EngineFilePaths"))
	{

		QByteArray byteArray = dragDropData->GetCustomData("EngineFilePaths");
		QDataStream stream(byteArray);
		QStringList paths;
		stream >> paths;

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

Qt::ItemFlags CFileListModel::flags(const QModelIndex &index) const
{
	return QAbstractListModel::flags(index) | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
}

