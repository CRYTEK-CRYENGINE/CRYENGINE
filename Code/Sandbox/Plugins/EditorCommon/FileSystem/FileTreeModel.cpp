// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileTreeModel.h"

#include "FileSystem/FileSystem_Enumerator.h"

#include "CryIcon.h"
#include "DragDrop.h"

#include "CryIcon.h"
#include <QLocale>

#include <cassert>
#include <queue>

struct CFileTreeModel::SImplementation
{
	class CFileMonitor : public FileSystem::ISubTreeMonitor
	{
	public:
		CFileMonitor(SImplementation* pImpl)
			: m_pImpl(pImpl)
		{}

		void ClearImpl()
		{
			// Clear the impl so we don't attempt to call it after it's dead
			m_pImpl = nullptr;
		}

		// ISubTreeMonitor interface
	protected:
		virtual void Activated(const FileSystem::SnapshotPtr& snapshot) override
		{
			if (!m_pImpl || m_pImpl->m_pMonitor.get() != this)
			{
				return;
			}
			m_pImpl->SetSnapshot(snapshot);
		}

		virtual void Update(const FileSystem::SSubTreeMonitorUpdate& update) override
		{
			if (!m_pImpl || m_pImpl->m_pMonitor.get() != this)
			{
				return;
			}
			m_pImpl->ApplyUpdate(update);
		}

	private:
		SImplementation* m_pImpl;
	};

	struct SItem;
	typedef QHash<FileSystem::DirectoryPtr, SItem*> DirectoryItemHash;
	typedef QHash<FileSystem::FilePtr, SItem*>      FileItemHash;

	struct SItem
	{
		SItem()
			: parent(nullptr),
			row(0) {}

		FileSystem::DirectoryPtr            directory;
		FileSystem::FilePtr                 file;
		SItem*                              parent;
		int                                 row;
		std::vector<std::unique_ptr<SItem>> directories;
		std::vector<std::unique_ptr<SItem>> files;

		static SItem*                       makeFile(const FileSystem::FilePtr& file)
		{
			auto item = new SItem();
			item->file = file;
			return item;
		}

		static SItem* makeDirectory(const FileSystem::DirectoryPtr& directory)
		{
			auto item = new SItem();
			item->directory = directory;
			return item;
		}

		bool isDirectory() const { return (bool)directory; }
		bool isFile() const      { return (bool)file; }

		int  rowCount() const
		{
			return directories.size() + files.size();
		}

		void fixRemovedChildRow(size_t removedRow)
		{
			for (; removedRow < directories.size(); ++removedRow)
			{
				directories[removedRow]->row--;
			}
			removedRow -= directories.size();
			for (; removedRow < files.size(); ++removedRow)
			{
				files[removedRow]->row--;
			}
		}

		void fixInsertedChildRow(size_t row)
		{
			++row;
			for (; row < directories.size(); ++row)
			{
				directories[row]->row++;
			}
			row -= directories.size();
			for (; row < files.size(); ++row)
			{
				files[row]->row++;
			}
		}

		SItem* getChild(size_t row)
		{
			if (row < directories.size())
			{
				return directories[row].get();
			}
			row -= directories.size();
			if (row < files.size())
			{
				return files[row].get();
			}
			return nullptr;
		}
	};

	SImplementation(CFileTreeModel* pModel, FileSystem::CEnumerator& enumerator)
		: m_pModel(pModel)
		, m_enumerator(enumerator)
		, m_filterChanged(false)
		, m_monitorHandle(-1)
	{
	}

	~SImplementation()
	{
		// Make sure we remove this monitor. Receiving file system events after we're dead is not a sane thing to attempt
		m_enumerator.StopSubTreeMonitor(m_monitorHandle);
		m_pMonitor->ClearImpl();
	}

	void SetFilter(const FileSystem::SFileFilter& filter)
	{
		m_fileFilter = filter;
		m_pMonitor = std::make_shared<CFileMonitor>(this);

		// If we've already installed a file system monitor, remove it before adding another one
		if (m_monitorHandle > -1)
			m_enumerator.StopSubTreeMonitor(m_monitorHandle);

		m_monitorHandle = m_enumerator.StartSubTreeMonitor(m_fileFilter, m_pMonitor);
		m_filterChanged = true;
		SetSnapshot(m_enumerator.GetCurrentSnapshot());
		m_filterChanged = false;
	}

	void Clear()
	{
		if (!m_rootItem)
		{
			return; // nothing to clear
		}
		m_pModel->beginResetModel();
		ClearWithoutSignals();
		m_pModel->endResetModel();
	}

	void ClearWithoutSignals()
	{
		m_rootItem.reset();
		m_directoryItems.clear();
		m_fileItems.clear();
	}

	void SetSnapshot(const FileSystem::SnapshotPtr& snapshot)
	{
		if (snapshot == m_snapshot && !m_filterChanged)
		{
			return; // already there
		}
		m_snapshot = snapshot;
		if (!snapshot)
		{
			Clear();
			return; // no snapshot
		}
		const auto subTree = snapshot->FindSubTree(m_fileFilter);
		if (!subTree.directory)
		{
			Clear();
			return; // snapshot has no root
		}
		SetSubTreeRoot(subTree);
	}

	void ApplyUpdate(const FileSystem::SSubTreeMonitorUpdate& update)
	{
		CRY_ASSERT(!m_snapshot || update.from == m_snapshot);
		ApplySubTreeChangeRoot(update.root);
		m_snapshot = update.to;
	}

	void SetSubTreeRoot(const FileSystem::SSubTree& subTree)
	{
		auto rootDir = subTree.directory;
		CRY_ASSERT(rootDir);
		if (!m_rootItem)
		{
			m_rootItem.reset(new SItem());
		}
		else
		{
			CRY_ASSERT(m_directoryItems.contains(m_rootItem->directory));
			m_directoryItems.remove(m_rootItem->directory);
		}
		m_rootItem->directory = rootDir;
		CRY_ASSERT(!m_directoryItems.contains(rootDir));
		m_directoryItems.insert(rootDir, m_rootItem.get());
		SetSubTreeChildren(subTree, m_rootItem.get());
	}

	void ApplySubTreeChangeRoot(const FileSystem::SSubTreeChange& rootChange)
	{
		if (!rootChange.to || rootChange.from == rootChange.to)
		{
			return; // no changes to apply
		}
		if (!m_rootItem)
		{
			m_rootItem.reset(new SItem());
		}
		else
		{
			CRY_ASSERT(m_directoryItems.contains(m_rootItem->directory));
			m_directoryItems.remove(m_rootItem->directory);
		}
		m_rootItem->directory = rootChange.to;
		CRY_ASSERT(!m_directoryItems.contains(rootChange.to));
		m_directoryItems.insert(rootChange.to, m_rootItem.get());
		ApplySubTreeChangeChildren(rootChange, m_rootItem.get());
	}

	void SetSubTreeChildren(const FileSystem::SSubTree& subtree, SItem* item)
	{
		QSet<FileSystem::DirectoryPtr> oldDirectories;
		QSet<FileSystem::FilePtr> oldFiles;
		for (auto& subDirectoryItem : item->directories)
		{
			CRY_ASSERT(m_directoryItems.contains(subDirectoryItem->directory));
			oldDirectories << subDirectoryItem->directory;
		}
		for (auto& subFileItem : item->files)
		{
			oldFiles << subFileItem->file;
		}

		for (auto& subDirectoryTree : subtree.subDirectories)
		{
			oldDirectories.remove(subDirectoryTree.directory);
		}
		for (auto& file : subtree.files)
		{
			oldFiles.remove(file);
		}

		RemoveDirectories(item, oldDirectories);
		RemoveFiles(item, oldFiles);

		for (auto& subDirectoryTree : subtree.subDirectories)
		{
			SetSubTree(subDirectoryTree, item);
		}
		for (auto& file : subtree.files)
		{
			SetFile(file, item);
		}
	}

	void ApplySubTreeChangeChildren(const FileSystem::SSubTreeChange& subtreeChange, SItem* item)
	{
		for (auto& subDirectoryChange : subtreeChange.subDirectoryChanges)
		{
			ApplySubTreeChange(subDirectoryChange, item);
		}
		for (auto& fileChange : subtreeChange.fileChanges)
		{
			ApplyFileChange(fileChange, item);
		}
	}

	void SetSubTree(const FileSystem::SSubTree& subtree, SItem* parent)
	{
		auto it = m_directoryItems.find(subtree.directory);
		if (it != m_directoryItems.end())
		{
			auto item = it.value();
			SetSubTreeChildren(subtree, item);
			return; // directory existed
		}
		CreateDirectoryFromTree(subtree, parent);
	}

	void ApplySubTreeChange(const FileSystem::SSubTreeChange& subtreeChange, SItem* parent)
	{
		if (subtreeChange.from)
		{
			auto it = m_directoryItems.find(subtreeChange.from);
			if (it != m_directoryItems.end())
			{
				auto item = it.value();
				if (subtreeChange.to)
				{
					UpdateDirectory(item, subtreeChange);
				}
				else
				{
					RemoveDirectory(item);
				}
				return; // handled existing from
			}
		}
		if (subtreeChange.to)
		{
			CreateDirectoryFromChange(subtreeChange, parent);
		}
	}

	void SetFile(const FileSystem::FilePtr& file, SItem* parent)
	{
		auto it = m_fileItems.find(file);
		if (it != m_fileItems.end())
		{
			return; // file existed
		}
		CreateFile(file, parent);
	}

	void ApplyFileChange(const FileSystem::SFileChange& fileChange, SItem* parent)
	{
		if (fileChange.from)
		{
			auto it = m_fileItems.find(fileChange.from);
			if (it != m_fileItems.end())
			{
				auto item = it.value();
				if (fileChange.to)
				{
					UpdateFile(item, fileChange);
				}
				else
				{
					RemoveFile(item);
				}
				return; // handles existing
			}
		}
		if (fileChange.to)
		{
			CreateFile(fileChange.to, parent);
		}
	}

	// ----- regular updates -----
	void UpdateDirectory(SItem* directoryItem, const FileSystem::SSubTreeChange& subtreeChange)
	{
		CRY_ASSERT(m_directoryItems.contains(subtreeChange.from));
		CRY_ASSERT(!m_directoryItems.contains(subtreeChange.to));

		directoryItem->directory = subtreeChange.to;
		m_directoryItems.remove(subtreeChange.from);
		m_directoryItems[subtreeChange.to] = directoryItem;

		if (subtreeChange.from->provider->fullName != subtreeChange.to->provider->fullName)
		{
			auto index = GetIndex(directoryItem);
			m_pModel->dataChanged(index, index, QVector<int>() << Qt::DisplayRole);
		}
		ApplySubTreeChangeChildren(subtreeChange, directoryItem);
	}

	void UpdateFile(SItem* fileItem, const FileSystem::SFileChange& fileChange)
	{
		CRY_ASSERT(m_fileItems.contains(fileChange.from));
		CRY_ASSERT(!m_fileItems.contains(fileChange.to));

		fileItem->file = fileChange.to;
		m_fileItems.remove(fileChange.from);
		m_fileItems[fileChange.to] = fileItem;

		auto topLeft = GetIndex(fileItem);
		auto bottomRight = topLeft.sibling(topLeft.row(), eColumn_Count - 1);
		m_pModel->dataChanged(topLeft, bottomRight);
	}

	// ---- removal -----
	void RemoveDirectories(SItem* parent, const QSet<FileSystem::DirectoryPtr>& directories)
	{
		if (directories.isEmpty())
		{
			return;
		}
		auto parentIndex = GetIndex(parent);
		int i = parent->directories.size() - 1;
		while (i >= 0)
		{
			while (i >= 0 && !directories.contains(parent->directories[i]->directory)) --i;
			if (i < 0) break;
			auto last = i;
			--i;
			while (i >= 0 && directories.contains(parent->directories[i]->directory)) --i;
			auto first = i + 1;

			m_pModel->beginRemoveRows(parentIndex, first, last);
			for (int j = last; j >= first; --j)
			{
				RemoveDirectoryWithoutSignals(parent->directories[j].get());
			}
			m_pModel->endRemoveRows();
			--i;
		}
	}

	void RemoveFiles(SItem* parent, const QSet<FileSystem::FilePtr>& files)
	{
		if (files.isEmpty())
		{
			return;
		}
		auto parentIndex = GetIndex(parent);
		int i = parent->files.size() - 1;
		while (i >= 0)
		{
			while (i >= 0 && !files.contains(parent->files[i]->file)) --i;
			if (i < 0) break;
			auto last = i;
			--i;
			while (i >= 0 && files.contains(parent->files[i]->file)) --i;
			auto first = i + 1;

			m_pModel->beginRemoveRows(parentIndex, first + parent->directories.size(), last + parent->directories.size());
			for (int j = last; j >= first; --j)
			{
				RemoveFileWithoutSignals(parent->files[j].get());
			}
			m_pModel->endRemoveRows();
			--i;
		}
	}

	void RemoveDirectory(SItem* directoryItem)
	{
		SItem* parent = directoryItem->parent;
		CRY_ASSERT(parent);
		int row = directoryItem->row;
		m_pModel->beginRemoveRows(GetIndex(parent), row, row);
		RemoveDirectoryWithoutSignals(directoryItem);
		m_pModel->endRemoveRows();
	}

	void RemoveDirectoryWithoutSignals(SItem* directoryItem)
	{
		DropChildren(directoryItem);
		CRY_ASSERT(m_directoryItems.contains(directoryItem->directory));
		m_directoryItems.remove(directoryItem->directory);
		SItem* parent = directoryItem->parent;
		CRY_ASSERT(parent);
		int row = directoryItem->row;
		parent->directories.erase(parent->directories.begin() + row);
		parent->fixRemovedChildRow(row);
	}

	void DropChildren(SItem* directoryItem)
	{
		for (const auto& subDirectoryItem : directoryItem->directories)
		{
			CRY_ASSERT(m_directoryItems.contains(subDirectoryItem->directory));
			m_directoryItems.remove(subDirectoryItem->directory);
			DropChildren(subDirectoryItem.get());
		}
		for (const auto& fileItem : directoryItem->files)
		{
			CRY_ASSERT(m_fileItems.contains(fileItem->file));
			m_fileItems.remove(fileItem->file);
		}
	}

	void RemoveFile(SItem* fileItem)
	{
		SItem* parent = fileItem->parent;
		CRY_ASSERT(parent);
		int row = fileItem->row;
		m_pModel->beginRemoveRows(GetIndex(parent), row, row);
		RemoveFileWithoutSignals(fileItem);
		m_pModel->endRemoveRows();
	}

	void RemoveFileWithoutSignals(SItem* fileItem)
	{
		CRY_ASSERT(m_fileItems.contains(fileItem->file));
		m_fileItems.remove(fileItem->file);
		SItem* parent = fileItem->parent;
		CRY_ASSERT(parent);
		int row = fileItem->row;
		parent->files.erase(parent->files.begin() + (row - parent->directories.size()));
		parent->fixRemovedChildRow(row);
	}

	// ----- creation -----
	void CreateChildrenFromTree(const FileSystem::SSubTree& subtree, SItem* itemIt)
	{
		for (auto& subDirectoryTree : subtree.subDirectories)
		{
			CreateSubDirectoryFromTree(subDirectoryTree, itemIt);
		}
		for (auto& file : subtree.files)
		{
			CreateSubFile(file, itemIt);
		}
	}

	void CreateChildrenFromChange(const FileSystem::SSubTreeChange& subtreeChange, SItem* itemIt)
	{
		foreach(const auto & subDirectoryChange, subtreeChange.subDirectoryChanges)
		{
			if (subDirectoryChange.to)
			{
				CreateSubDirectoryFromChange(subDirectoryChange, itemIt);
			}
		}
		foreach(const auto & fileChange, subtreeChange.fileChanges)
		{
			if (fileChange.to)
			{
				CreateSubFile(fileChange.to, itemIt);
			}
		}
	}

	void CreateDirectoryFromChange(const FileSystem::SSubTreeChange& subtreeChange, SItem* parent)
	{
		auto directoryItem = SItem::makeDirectory(subtreeChange.to);
		CreateChildrenFromChange(subtreeChange, directoryItem);
		AppendDirectoryToParent(directoryItem, parent);
	}

	void CreateDirectoryFromTree(const FileSystem::SSubTree& subtree, SItem* parent)
	{
		auto directoryItem = SItem::makeDirectory(subtree.directory);
		CreateChildrenFromTree(subtree, directoryItem);
		AppendDirectoryToParent(directoryItem, parent);
	}

	void CreateSubDirectoryFromChange(const FileSystem::SSubTreeChange& subtreeChange, SItem* parent)
	{
		auto directoryItem = SItem::makeDirectory(subtreeChange.to);
		AppendDirectoryToParentWithoutSignals(directoryItem, parent);
		CreateChildrenFromChange(subtreeChange, directoryItem);
	}

	void CreateSubDirectoryFromTree(const FileSystem::SSubTree& subtree, SItem* parent)
	{
		auto directoryItem = SItem::makeDirectory(subtree.directory);
		AppendDirectoryToParentWithoutSignals(directoryItem, parent);
		CreateChildrenFromTree(subtree, directoryItem);
	}

	void AppendDirectoryToParent(SItem* directoryItem, SItem* parent)
	{
		int row = parent->directories.size();
		m_pModel->beginInsertRows(GetIndex(parent), row, row);
		AppendDirectoryToParentWithoutSignals(directoryItem, parent);
		m_pModel->endInsertRows();
	}

	void AppendDirectoryToParentWithoutSignals(SItem* directoryItem, SItem* parent)
	{
		CRY_ASSERT(directoryItem->directory);
		CRY_ASSERT(!m_directoryItems.contains(directoryItem->directory));
		m_directoryItems[directoryItem->directory] = directoryItem;
		int row = parent->directories.size();
		directoryItem->parent = parent;
		directoryItem->row = row;
		parent->directories.push_back(std::unique_ptr<SItem>(directoryItem));
		parent->fixInsertedChildRow(row);
	}

	void CreateFile(const FileSystem::FilePtr& file, SItem* parent)
	{
		auto fileItem = SItem::makeFile(file);
		AppendFileToParent(fileItem, parent);
	}

	void CreateSubFile(const FileSystem::FilePtr& file, SItem* parent)
	{
		auto fileItem = SItem::makeFile(file);
		AppendFileToParentWithoutSignals(fileItem, parent);
	}

	void AppendFileToParent(SItem* fileItem, SItem* parent)
	{
		int row = parent->rowCount();
		m_pModel->beginInsertRows(GetIndex(parent), row, row);
		AppendFileToParentWithoutSignals(fileItem, parent);
		m_pModel->endInsertRows();
	}

	void AppendFileToParentWithoutSignals(SItem* fileItem, SItem* parent)
	{
		CRY_ASSERT(fileItem->file);
		CRY_ASSERT(!m_fileItems.contains(fileItem->file));
		m_fileItems[fileItem->file] = fileItem;
		fileItem->parent = parent;
		fileItem->row = parent->rowCount();
		parent->files.push_back(std::unique_ptr<SItem>(fileItem));
	}

	QVariant GetDirectoryData(const FileSystem::DirectoryPtr& pDirectory, int role, int column) const
	{
		if (!pDirectory)
		{
			return QVariant();
		}

		switch (role)
		{
		case Qt::DisplayRole:
			switch (column)
			{
			case eColumn_Name:
				return pDirectory->provider->fullName;

			case eColumn_Type:
				return GetFileTypeString(SFileType::DirectoryType());

			case eColumn_Archive:
				return GetArchiveString(pDirectory->provider, pDirectory->providerDetails);

			default:
				return QVariant();
			}

		case Qt::DecorationRole:
			{
				if (column != eColumn_Name)
				{
					return QVariant();
				}
				// Return the pixmap so we never end up tinting the icon to it's 'active' state & color
				return GetFileTypeIcon(SFileType::DirectoryType()).pixmap(16, 16);
			}

		case eRole_FileName:
			return pDirectory->provider->fullName;

		case eRole_EnginePath:
			return pDirectory->enginePath.full;

		default:
			return QVariant();
		}
	}

	QVariant GetFileData(const FileSystem::FilePtr& pFile, int role, int column) const
	{
		if (!pFile)
		{
			return QVariant();
		}

		switch (role)
		{
		case Qt::DisplayRole:
			switch (column)
			{
			case eColumn_Name:
				return pFile->provider->fullName;

			case eColumn_LastModified:
				return pFile->provider->lastModified.toTimeSpec(Qt::LocalTime).toString(Qt::SystemLocaleShortDate);

			case eColumn_Type:
				return GetFileTypeString(pFile->type);

			case eColumn_Size:
				return GetSizeString(pFile->provider->size);

			case eColumn_Archive:
				return GetArchiveString(pFile->provider, pFile->providerDetails);

			default:
				return QVariant();
			}

		case Qt::DecorationRole:
			{
				if (column != eColumn_Name)
				{
					return QVariant();
				}
				// Return the pixmap so we never end up tinting the icon to it's 'active' state & color
				return GetFileTypeIcon(pFile->type).pixmap(16, 16);
			}

		case eRole_FileName:
			return pFile->provider->fullName;

		case eRole_EnginePath:
			return pFile->enginePath.full;

		case eRole_FileSize:
			return QVariant::fromValue(pFile->provider->size);

		case eRole_LastModified:
			return QVariant::fromValue(pFile->provider->lastModified);

		default:
			return QVariant();
		}
	}

	static QString GetFileTypeString(const SFileType* fileType)
	{
		CRY_ASSERT(fileType); // a filetype should always be present
		if (!fileType)
		{
			return QString();
		}
		return fileType->name();
	}

	static CryIcon GetFileTypeIcon(const SFileType* fileType)
	{
		CRY_ASSERT(fileType); // a filetype should always be present
		if (!fileType)
		{
			return CryIcon();
		}
		return CryIcon(fileType->iconPath);
	}

	// from QFileSystemModel code
	static QString GetSizeString(quint64 bytes)
	{
		const quint64 kb = 1024;
		const quint64 mb = 1024 * kb;
		const quint64 gb = 1024 * mb;
		const quint64 tb = 1024 * gb;
		if (bytes >= tb)
		{
			return tr("%1 TB").arg(QLocale().toString(static_cast<qreal>(bytes) / tb, 'f', 3));
		}
		if (bytes >= gb)
		{
			return tr("%1 GB").arg(QLocale().toString(static_cast<qreal>(bytes) / gb, 'f', 2));
		}
		if (bytes >= mb)
		{
			return tr("%1 MB").arg(QLocale().toString(static_cast<qreal>(bytes) / mb, 'f', 1));
		}
		if (bytes >= kb)
		{
			return tr("%1 KB").arg(QLocale().toString(bytes / kb));
		}
		return tr("%1 bytes").arg(QLocale().toString(bytes));
	}

	static QString GetArchiveString(const FileSystem::ProviderIt& provider, const FileSystem::ProviderDetails& providerDetails)
	{
		QStringList parts;
		if (provider.key())
		{
			parts << provider.key()->provider->fullName;
		}
		if (providerDetails.size() > 1)
		{
			QStringList providers;
			auto itEnd = providerDetails.end();
			for (auto it = providerDetails.begin(); it != itEnd; ++it)
			{
				if (it == provider)
				{
					continue;
				}
				if (it.key())
				{
					providers << it.key()->provider->fullName;
				}
				else
				{
					providers.prepend(tr("FS"));
				}
			}
			parts << QStringLiteral("(%1)").arg(providers.join(QStringLiteral(", ")));
		}
		return parts.join(QChar::Space);
	}

	QModelIndex GetIndex(SItem* item) const
	{
		if (item == m_rootItem.get())
		{
			return QModelIndex();
		}
		auto internalPtr = *reinterpret_cast<void**>(&item);
		return m_pModel->createIndex(item->row, 0, internalPtr);
	}

	QModelIndex GetIndexByPath(const QString& enginePath) const
	{
		if (!m_snapshot)
		{
			return QModelIndex(); // no snapshot data
		}
		auto pDirectory = m_snapshot->GetDirectoryByEnginePath(enginePath);
		if (pDirectory)
		{
			auto directoryIt = m_directoryItems.find(pDirectory);
			if (directoryIt != m_directoryItems.end())
			{
				return GetIndex(directoryIt.value());
			}
			return QModelIndex(); // directory not found (probably filtered out)
		}
		auto pFile = m_snapshot->GetFileByEnginePath(enginePath);
		if (pFile)
		{
			auto fileIt = m_fileItems.find(pFile);
			if (fileIt != m_fileItems.end())
			{
				return GetIndex(fileIt.value());
			}
			return QModelIndex(); // file not found (probably filtered out)
		}
		return QModelIndex(); // path not found
	}

	FileSystem::CEnumerator::MonitorHandle m_monitorHandle;

	CFileTreeModel*                        m_pModel;
	FileSystem::CEnumerator&               m_enumerator;
	bool m_filterChanged;
	FileSystem::SFileFilter                m_fileFilter;
	std::shared_ptr<CFileMonitor>          m_pMonitor;
	FileSystem::SnapshotPtr                m_snapshot; // needed for creating indexes

	std::unique_ptr<SItem>                 m_rootItem;
	DirectoryItemHash                      m_directoryItems;
	FileItemHash                           m_fileItems;
};

CFileTreeModel::CFileTreeModel(FileSystem::CEnumerator& enumerator, const FileSystem::SFileFilter& filter, QObject* pParent)
	: QAbstractItemModel(pParent)
	, p(new SImplementation(this, enumerator))
{
	p->SetFilter(filter);
}

CFileTreeModel::~CFileTreeModel()
{}

void CFileTreeModel::SetFilter(const FileSystem::SFileFilter& filter)
{
	p->SetFilter(filter);
}

QModelIndex CFileTreeModel::GetIndexByPath(const QString& enginePath) const
{
	return p->GetIndexByPath(enginePath);
}

QString CFileTreeModel::GetPathFromIndex(const QModelIndex& index) const
{
	if (!index.isValid())
	{
		return QString();
	}
	CRY_ASSERT(index.model() == this);
	auto item = static_cast<SImplementation::SItem*>(index.internalPointer());
	if (item->isFile())
	{
		return item->file->enginePath.full;
	}
	if (item->isDirectory())
	{
		return item->directory->enginePath.full;
	}
	return QString();
}

QString CFileTreeModel::GetFileNameFromIndex(const QModelIndex& index) const
{
	if (!index.isValid())
	{
		return QString();
	}
	CRY_ASSERT(index.model() == this);
	auto item = static_cast<SImplementation::SItem*>(index.internalPointer());
	if (item->isFile())
	{
		return item->file->provider->fullName;
	}
	if (item->isDirectory())
	{
		return item->directory->provider->fullName;
	}
	return QString();
}

bool CFileTreeModel::IsDirectory(const QModelIndex& index) const
{
	if (!index.isValid())
	{
		return true;
	}
	CRY_ASSERT(index.model() == this);
	auto item = static_cast<SImplementation::SItem*>(index.internalPointer());
	return item->isDirectory();
}

const SFileType* CFileTreeModel::GetFileType(const QModelIndex& index) const
{
	if (!index.isValid())
	{
		return SFileType::DirectoryType();
	}
	CRY_ASSERT(index.model() == this);
	auto item = static_cast<SImplementation::SItem*>(index.internalPointer());
	if (item->isFile())
	{
		return item->file->type;
	}
	return SFileType::DirectoryType();
}

quint64 CFileTreeModel::GetFileSize(const QModelIndex& index) const
{
	if (!index.isValid())
	{
		return 0;
	}
	CRY_ASSERT(index.model() == this);
	auto item = static_cast<SImplementation::SItem*>(index.internalPointer());
	if (item->isFile())
	{
		return item->file->provider->size;
	}
	return 0;
}

QDateTime CFileTreeModel::GetLastModified(const QModelIndex& index) const
{
	if (!index.isValid())
	{
		return QDateTime();
	}
	CRY_ASSERT(index.model() == this);
	auto item = static_cast<SImplementation::SItem*>(index.internalPointer());
	if (item->isFile())
	{
		return item->file->provider->lastModified;
	}
	return QDateTime();
}

QModelIndex CFileTreeModel::index(int row, int column, const QModelIndex& parent) const
{
	if (row < 0 || column < 0)
	{
		return QModelIndex();
	}
	auto parentItem = parent.isValid() ? static_cast<SImplementation::SItem*>(parent.internalPointer()) : p->m_rootItem.get();
	if (!parentItem || parentItem->isFile())
	{
		return QModelIndex(); // has no children
	}
	auto childItem = parentItem->getChild(row);
	if (childItem == nullptr)
	{
		return QModelIndex(); // wrong row
	}
	auto internalPtr = static_cast<void*>(childItem);
	return createIndex(row, column, internalPtr);
}

QModelIndex CFileTreeModel::parent(const QModelIndex& child) const
{
	if (!child.isValid())
	{
		return QModelIndex(); // child is root
	}

	auto childItem = static_cast<SImplementation::SItem*>(child.internalPointer());
	auto item = childItem->parent;
	if (item == p->m_rootItem.get())
	{
		return QModelIndex(); // parent is root
	}
	return p->GetIndex(item);
}

bool CFileTreeModel::hasChildren(const QModelIndex& parent) const
{
	if (!parent.isValid())
	{
		return true; // root always has children
	}
	auto item = static_cast<SImplementation::SItem*>(parent.internalPointer());
	return item->isDirectory();
}

int CFileTreeModel::rowCount(const QModelIndex& parent) const
{
	// root is invalid index
	if (!parent.isValid())
	{
		if (!p->m_rootItem)
		{
			return 0; // no valid root item
		}
		return p->m_rootItem->rowCount();
	}
	auto parentItem = static_cast<SImplementation::SItem*>(parent.internalPointer());
	if (parentItem->isFile())
	{
		return 0;
	}
	return parentItem->rowCount();
}

int CFileTreeModel::columnCount(const QModelIndex& /*parent*/) const
{
	return eColumn_Count;
}

QVariant CFileTreeModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
	{
		return QVariant();
	}

	if ((role == Qt::TextAlignmentRole) && (index.column() == 3))
	{
		return (int)(Qt::AlignRight | Qt::AlignVCenter);
	}

	auto item = static_cast<SImplementation::SItem*>(index.internalPointer());
	if (item->isFile())
	{
		return p->GetFileData(item->file, role, index.column());
	}
	return p->GetDirectoryData(item->directory, role, index.column());
}

QVariant CFileTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	// only horizontal header, only show column names
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
	{
		return QVariant();
	}

	switch (section)
	{
	case eColumn_Name:
		return tr("Name");

	case eColumn_LastModified:
		return tr("Last modified");

	case eColumn_Type:
		return tr("Type");

	case eColumn_Size:
		return tr("Size");

	case eColumn_Archive:
		return tr("Archive");

	default:
		return QVariant();
	}
}

Qt::ItemFlags CFileTreeModel::flags(const QModelIndex& index) const
{
	auto defaultFlags = QAbstractItemModel::flags(index);
	return defaultFlags | Qt::ItemIsDragEnabled;
}

QStringList CFileTreeModel::mimeTypes() const
{
	auto typeList = QAbstractItemModel::mimeTypes();
	// TODO: refactor hard coded mime type which must be known by each
	// widget using this model with drag and drop
	typeList << CDragDropData::GetMimeFormatForType("EngineFilePaths");
	return typeList;
}

QMimeData* CFileTreeModel::mimeData(const QModelIndexList& indexes) const
{
	auto pDragDropData = new CDragDropData();

	QStringList enginePaths;
	QSet<qint64> idsUsed;

	// when rows with multiple columns are selected the same
	// path would be added for every column index of a selected
	// row leading to multiple paths. Therefore a check is required
	for (const auto& index : indexes)
	{
		if (!idsUsed.contains(index.internalId()))
		{
			// remove leading "/" if present
			auto filePath = GetPathFromIndex(index);
			if (!filePath.isEmpty() && filePath[0] == QChar('/'))
			{
				filePath.remove(0, 1);
			}

			enginePaths << filePath;
			idsUsed.insert(index.internalId());
		}
	}

	QByteArray byteArray;
	QDataStream stream(&byteArray, QIODevice::ReadWrite);
	stream << enginePaths;
	// TODO: refactor hard coded mime type
	pDragDropData->SetCustomData("EngineFilePaths", byteArray);
	return pDragDropData;
}

