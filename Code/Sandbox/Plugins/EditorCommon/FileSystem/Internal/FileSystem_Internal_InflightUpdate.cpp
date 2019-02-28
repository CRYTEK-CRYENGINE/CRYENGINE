// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileSystem_Internal_InflightUpdate.h"

#include "FileSystem_Internal_PathUtils.h"

#include "FileSystem/FileSystem_FileFilter.h"

#include <QRegularExpression>

namespace FileSystem
{
namespace Internal
{

template<typename Container>
static void QHash_RemoveExcept(Container& hash, const typename Container::key_type& exceptKey)
{
	auto endIt = hash.end();
	for (auto it = hash.begin(); it != endIt; )
	{
		if (it.key() == exceptKey)
		{
			++it;
		}
		else
		{
			it = hash.erase(it);
		}
	}
}

template<typename Container>
static typename Container::const_iterator QHash_ConstFind(const Container& hash, const typename Container::key_type& key)
{
	return hash.find(key);
}

template<typename Container>
static typename Container::iterator QHash_EraseConstIt(Container& hash, const typename Container::const_iterator& cit)
{
	return hash.erase(*reinterpret_cast<const typename Container::iterator*>(&cit));
}

CInflightUpdate::CInflightUpdate(const SnapshotPtr& snapshot, const CFileTypeStore& fileTypeStore)
	: m_fileTypeStore(fileTypeStore)
{
	Reset(snapshot);
}

SSnapshotUpdate CInflightUpdate::CreateSnapshot() const
{
	SSnapshotUpdate result;
	result.from = m_base;
	auto toSnapshot = CreateSnapshotMut(*m_base);
	result.to = toSnapshot;
	if (!m_root->directoryChanges.isEmpty() || !m_root->fileChanges.isEmpty())
	{
		SnapshotUpdateState state;
		state.toSnapshot = toSnapshot;
		result.root = CreateSnapshotDirectoryUpdateFromChange(state, m_root, SEnginePath());
	}
	else
	{
		result.to = result.from;
	}
	return result;
}

void CInflightUpdate::Reset(const SnapshotPtr& snapshot)
{
	m_pathDirectoryChange.clear();
	m_pathFileChange.clear();
	m_directoryChanges.clear();
	m_fileChanges.clear();
	m_base = snapshot;

	auto rootDirectory = m_base->GetEngineRootDirectory();
	InflightDirectoryChange change;
	change.to = CreateDirectoryMut(rootDirectory);
	if (!rootDirectory)
	{
		SProviderData providerData;
		providerData.isFile = false;
		auto rootProviderIt = change.to->providerDetails.insert(nullptr, providerData);
		change.to->provider = rootProviderIt;
	}
	change.cachedPathes << QString();
	auto rootUpdate = m_directoryChanges.insert(rootDirectory, change);
	m_root = rootUpdate;
	m_pathDirectoryChange[QString()] = m_root;
}

void CInflightUpdate::RegisterFileTypes(const QVector<const SFileType*>& fileTypes)
{
	m_fileTypeStore.RegisterFileTypes(fileTypes);

	SFileFilter extensionFilter;
	for (auto pFileType : fileTypes)
	{
		assert(pFileType);
		if (!pFileType)
		{
			continue;
		}
		extensionFilter.fileExtensions.insert(pFileType->primaryExtension);
		for (const auto& extension : pFileType->extraExtensions)
		{
			extensionFilter.fileExtensions.insert(extension);
		}
	}
	const auto files = m_base->FindFiles(extensionFilter);
	for (const auto& pFile : files)
	{
		auto pNewType = m_fileTypeStore.GetType(pFile->enginePath.key, pFile->extension);
		const auto bFileTypeChanged = (pNewType && pFile->type && pNewType != pFile->type);
		const auto bFileTypeAdded = !pFile->type && pNewType;
		if ((bFileTypeChanged || bFileTypeAdded) && !m_fileChanges.contains(pFile))
		{
			auto latestEnginePath = GetLatestKeyEnginePath(pFile->enginePath.key);
			FindFileAndCreateFileChangeIn(m_root, latestEnginePath);
		}
	}
}

//
//  -------------------------------- Monitor API ----------------------------------------------
//
void CInflightUpdate::RemovePath(const QString& keyEnginePath)
{
	RemoveProviderPath(nullptr, keyEnginePath);
}

void CInflightUpdate::RenamePath(const QString& keyEnginePath, const SAbsolutePath& toName)
{
	RenameProviderPath(nullptr, keyEnginePath, toName);
}

void CInflightUpdate::AddDirectory(const QString& keyEnginePath, const SDirectoryInfoInternal& info)
{
	AddProviderDirectoryWithInfo(nullptr, keyEnginePath, info);
}

void CInflightUpdate::AddFile(const QString& keyEnginePath, const SFileInfoInternal& info)
{
	AddProviderFile(nullptr, keyEnginePath, info);
}

void CInflightUpdate::UpdateFile(const QString& keyEnginePath, const SFileInfoInternal& info)
{
	// as we do not catch errors here, add and update are similar
	AddFile(keyEnginePath, info);
}

//
//  -------------------------------- Scanner API ----------------------------------------------
//
void CInflightUpdate::ApplyDirectoryScanResult(const QString& keyEnginePath, const SDirectoryScanResult& scanResult)
{
	if (!keyEnginePath.isEmpty())
	{
		BuildDirectoryChangeFromRoot(keyEnginePath, nullptr); // ensure the directory is present
	}
	// Collect all names
	QSet<QString> fileNames;
	foreach(const SFileInfoInternal &fileInfo, scanResult.files)
	{
		fileNames << fileInfo.keyName;
	}
	QSet<QString> directoryNames;
	foreach(const SDirectoryInfoInternal &directoryInfo, scanResult.directories)
	{
		directoryNames << directoryInfo.keyName;
	}

	// Find all pathes to remove
	QSet<QString> removePathes;
	QString keyLatestDirectoryPath;
	DirectoryPtr directory;
	auto pathDirectoryChangeIt = m_pathDirectoryChange.find(keyEnginePath);
	if (pathDirectoryChangeIt == m_pathDirectoryChange.end())
	{
		directory = m_base->GetDirectoryByEnginePath(keyEnginePath);
		keyLatestDirectoryPath = GetLatestKeyEnginePath(keyEnginePath);
	}
	else
	{
		const DirectoryChangeIt& directoryChange = pathDirectoryChangeIt.value();
		directory = directoryChange.key(); // use fromDirectory - toDirectory has not child infos
		keyLatestDirectoryPath = directoryChange->keyEnginePath;
		foreach(const FileChangeIt &fileChange, directoryChange->fileChanges)
		{
			auto fileName = GetFilenameFromPath(fileChange->keyEnginePath);
			if (!fileNames.contains(fileName))
			{
				removePathes << fileChange->keyEnginePath;
			}
		}
		foreach(const DirectoryChangeIt &subDirectoryChange, directoryChange->directoryChanges)
		{
			auto subDirectoryName = GetFilenameFromPath(subDirectoryChange->keyEnginePath);
			if (!directoryNames.contains(subDirectoryName))
			{
				removePathes << subDirectoryChange->keyEnginePath;
			}
		}
	}
	if (directory)
	{
		auto nameFileItEnd = directory->nameFile.end();
		for (auto nameFileIt = directory->nameFile.begin(); nameFileIt != nameFileItEnd; ++nameFileIt)
		{
			if (!fileNames.contains(nameFileIt.key()))
			{
				removePathes << GetCombinedPath(keyLatestDirectoryPath, nameFileIt.key());
			}
		}
		auto nameDirectoryItEnd = directory->nameDirectory.end();
		for (auto nameDirectoryIt = directory->nameDirectory.begin(); nameDirectoryIt != nameDirectoryItEnd; ++nameDirectoryIt)
		{
			if (!directoryNames.contains(nameDirectoryIt.key()))
			{
				removePathes << GetCombinedPath(keyLatestDirectoryPath, nameDirectoryIt.key());
			}
		}
	}

	// Run Actions
	foreach(const QString &removePath, removePathes)
	{
		RemoveProviderPath(nullptr, removePath);
	}
	foreach(const SFileInfoInternal &fileInfo, scanResult.files)
	{
		auto keyFileEnginePath = GetCombinedPath(keyLatestDirectoryPath, fileInfo.keyName);
		AddFile(keyFileEnginePath, fileInfo);
	}
	foreach(const SDirectoryInfoInternal &directoryInfo, scanResult.directories)
	{
		auto keyDirectoryEnginePath = GetCombinedPath(keyLatestDirectoryPath, directoryInfo.keyName);
		AddDirectory(keyDirectoryEnginePath, directoryInfo);
	}
}

//
//  -------------------------------- Archive API ----------------------------------------------
//
void CInflightUpdate::SetArchiveContent(const QString& keyEnginePath, const SArchiveContentInternal& archiveContent)
{
	const FileChangeIt& fileChange = MakeFileChangeByEnginePath(keyEnginePath, nullptr);
	if (fileChange == m_fileChanges.end())
	{
		return; // archive not found
	}
	const FileMutPtr& toArchive = fileChange->to;
	Provider archiveProvider = toArchive.get();
	if (!toArchive)
	{
		return; // archive was removed
	}
	auto keyArchivePath = fileChange->keyEnginePath;

	// remove unused pathes
	QSet<QString> directoryPathes;
	foreach(const SDirectoryInfoInternal &directoryInfo, archiveContent.directories)
	{
		directoryPathes << directoryInfo.absolutePath.key;
	}
	QSet<QString> filePathes;
	foreach(const SFileInfoInternal &fileInfo, archiveContent.files)
	{
		filePathes << fileInfo.absolutePath.key;
	}
	RemoveArchiveContentExcept(toArchive, keyArchivePath, filePathes, directoryPathes);

	auto keyMountDirectoryPath = GetDirectoryPath(keyArchivePath);

	// add new stuff
	toArchive->archive.directoryPathes.clear();
	foreach(const SDirectoryInfoInternal &directoryInfo, archiveContent.directories)
	{
		QString keyDirectoryEnginePath = GetCombinedPath(keyMountDirectoryPath, directoryInfo.absolutePath.key);
		AddProviderDirectoryWithInfo(archiveProvider, keyDirectoryEnginePath, directoryInfo);

		toArchive->archive.directoryPathes << directoryInfo.absolutePath.key;
	}

	foreach(const SFileInfoInternal &fileInfo, archiveContent.files)
	{
		auto keyMountedFilePath = GetCombinedPath(keyMountDirectoryPath, fileInfo.absolutePath.key);
		AddProviderFile(archiveProvider, keyMountedFilePath, fileInfo);

		toArchive->archive.filePathes << fileInfo.absolutePath.key;
	}
}

void CInflightUpdate::CleanArchiveContent(const QString& keyEnginePath)
{
	const FileChangeIt& fileChange = MakeFileChangeByEnginePath(keyEnginePath, nullptr);
	if (fileChange == m_fileChanges.end())
	{
		return; // archive not found
	}
	FileMutPtr& toArchive = fileChange->to;
	if (!toArchive)
	{
		return; // archive was removed
	}
	auto archivePath = fileChange->keyEnginePath;
	RemoveArchiveContent(toArchive, archivePath);
}

//
//  -------------------------------- Snapshot creation ----------------------------------------------
//
SSnapshotDirectoryUpdate CInflightUpdate::CreateSnapshotDirectoryUpdateFromChange(SnapshotUpdateState& state, const DirectoryChangeIt& directoryChange, const SEnginePath& parentEnginePath) const
{
	const auto& fromDirectory = directoryChange.key();
	const auto& inflight = directoryChange.value();
	const auto& toDirectory = inflight.to;

	SSnapshotDirectoryUpdate result;
	result.from = fromDirectory;
	result.to = toDirectory;
	if (toDirectory) // directory was not removed
	{
		if (directoryChange != m_root)
		{
			UpdateDirectoryMut(toDirectory, parentEnginePath);
			AddSnapshotDirectoryArchivesUnchanged(directoryChange);
		}
		AddSnapshotDirectoryFilesUpdates(state, directoryChange, result);
		AddSnapshotDirectoryChildrenUpdates(state, directoryChange, result);
		if (fromDirectory)
		{
			AddSnapshotDirectoryFilesUnchanged(state, toDirectory, result);
			AddSnapshotDirectoryChildrenUnchanged(state, toDirectory, result);
		}
		AddDirectoryToSnapshotMut(toDirectory, state.toSnapshot);
	}
	return result;
}

SSnapshotDirectoryUpdate CInflightUpdate::CreateSnapshotDirectoryUpdateUnchanged(SnapshotUpdateState& state, const DirectoryPtr& fromDirectory, const DirectoryMutPtr& toDirectory) const
{
	SSnapshotDirectoryUpdate result;
	result.from = fromDirectory;
	result.to = toDirectory;
	AddSnapshotDirectoryFilesUnchanged(state, toDirectory, result);
	AddSnapshotDirectoryChildrenUnchanged(state, toDirectory, result);
	AddDirectoryToSnapshotMut(toDirectory, state.toSnapshot);
	return result;
}

SSnapshotFileUpdate CInflightUpdate::CreateSnapshotFileUpdateFromChange(const FileChangeIt& fileChange, const SEnginePath& parentEnginePath) const
{
	const auto& fromFile = fileChange.key();
	const auto& inflight = fileChange.value();
	const auto& toFile = inflight.to;

	SSnapshotFileUpdate result;
	result.from = fromFile;
	result.to = toFile;
	if (toFile)
	{
		UpdateFileMut(toFile, parentEnginePath, m_fileTypeStore);
	}
	return result;
}

SSnapshotFileUpdate CInflightUpdate::CreateSnapshotFileUpdateUnchanged(const FilePtr& fromFile, const CInflightUpdate::FileMutPtr& toFile) const
{
	SSnapshotFileUpdate result;
	result.from = fromFile;
	result.to = toFile;
	return result;
}

void CInflightUpdate::AddSnapshotDirectoryChildrenUpdates(SnapshotUpdateState& state, const DirectoryChangeIt& directoryChange, SSnapshotDirectoryUpdate& directoryUpdate) const
{
	const InflightDirectoryChange& directoryInflight = directoryChange.value();
	const auto& directory = directoryInflight.to;
	const auto& directoryEnginePath = directory->enginePath;
	foreach(const auto & subDirectoryChange, directoryInflight.directoryChanges)
	{
		const auto& subFromDirectory = subDirectoryChange.key();
		const auto& subInflight = subDirectoryChange.value();
		const auto& subToDirectory = subInflight.to;
		if (subFromDirectory)
		{
			state.fromSkipPathes.insert(subFromDirectory->enginePath.key); // marker that it should not be taken from base
		}
		auto subDirectoryUpdate = CreateSnapshotDirectoryUpdateFromChange(state, subDirectoryChange, directoryEnginePath);
		directoryUpdate.directoryUpdates << subDirectoryUpdate;
		if (subToDirectory)
		{
			AddDirectoryToParentDirectoryMut(subToDirectory, directory);
		}
	}
}

void CInflightUpdate::AddSnapshotDirectoryFilesUpdates(SnapshotUpdateState& state, const DirectoryChangeIt& directoryChange, SSnapshotDirectoryUpdate& directoryUpdate) const
{
	const InflightDirectoryChange& directoryInflight = directoryChange.value();
	const auto& directory = directoryInflight.to;
	const auto& directoryEnginePath = directory->enginePath;
	foreach(const auto & fileChange, directoryInflight.fileChanges)
	{
		const auto& fromFile = fileChange.key();
		const auto& fileInflight = fileChange.value();
		const auto& toFile = fileInflight.to;
		if (fromFile)
		{
			state.fromSkipPathes.insert(fromFile->enginePath.key); // marker not to take the file from base
		}
		auto fileUpdate = CreateSnapshotFileUpdateFromChange(fileChange, directoryEnginePath);
		directoryUpdate.fileUpdates << fileUpdate;
		if (toFile)
		{
			// AddFileToSnapshot(toFile, state.toSnapshot);
			AddFileToDirectoryMut(toFile, directory);
		}
	}
}

void CInflightUpdate::AddSnapshotDirectoryChildrenUnchanged(SnapshotUpdateState& state, const DirectoryMutPtr& toDirectory, SSnapshotDirectoryUpdate& directoryUpdate) const
{
	const DirectoryPtr& fromDirectory = directoryUpdate.from;
	CRY_ASSERT(fromDirectory);
	CRY_ASSERT(toDirectory);
	foreach(const auto & subFromDirectory, fromDirectory->nameDirectory)
	{
		if (state.fromSkipPathes.contains(subFromDirectory->enginePath.key))
		{
			continue; // skip changed directory
		}
		if (subFromDirectory->parentEnginePath.key == toDirectory->enginePath.key)
		{
			// path is preserved - no update necessary (all children are taken as well)
			const auto& subToDirectory = subFromDirectory;
			AddDirectoryToSnapshotMutRecursive(subToDirectory, state.toSnapshot);
			AddDirectoryToParentDirectoryMut(subToDirectory, toDirectory);
		}
		else
		{
			// path is changed - requires update
			auto subToDirectory = CreateDirectoryMut(subFromDirectory);
			CRY_ASSERT(!subToDirectory->keyName.isEmpty());
			SetDirectoryMutParentPath(subToDirectory, toDirectory->enginePath);
			auto subDirectoryUpdate = CreateSnapshotDirectoryUpdateUnchanged(state, subFromDirectory, subToDirectory);
			directoryUpdate.directoryUpdates << subDirectoryUpdate;
			AddDirectoryToParentDirectoryMut(subToDirectory, toDirectory);
		}
	}
}

void CInflightUpdate::AddSnapshotDirectoryFilesUnchanged(SnapshotUpdateState& state, const DirectoryMutPtr& toDirectory, SSnapshotDirectoryUpdate& directoryUpdate) const
{
	const DirectoryPtr& fromDirectory = directoryUpdate.from;
	CRY_ASSERT(fromDirectory);
	CRY_ASSERT(toDirectory);
	foreach(const auto & fromFile, fromDirectory->nameFile)
	{
		if (state.fromSkipPathes.contains(fromFile->enginePath.key))
		{
			continue; // skip changed file
		}
		if (fromFile->directoryEnginePath.key == toDirectory->enginePath.key)
		{
			// path is preserved - no update necessary
			const auto& toFile = fromFile;
			// AddFileToSnapshot(toFile, state.toSnapshot);
			AddFileToDirectoryMut(toFile, toDirectory);
		}
		else
		{
			// path is changed - create update anyway
			auto updatedFile = CreateFileMut(fromFile);
			SetFileMutDirectoryEnginePath(updatedFile, toDirectory->enginePath);
			auto fileUpdate = CreateSnapshotFileUpdateUnchanged(fromFile, updatedFile);
			directoryUpdate.fileUpdates << fileUpdate;
			// AddFileToSnapshot(updatedFile, state.toSnapshot);
			AddFileToDirectoryMut(updatedFile, toDirectory);
		}
	}
}

void CInflightUpdate::AddSnapshotDirectoryArchivesUnchanged(const DirectoryChangeIt& directoryChange) const
{
	// all files link to the archive, so we have to ensure the archive changed if the path to it changed
	const auto& fromDirectory = directoryChange.key();
	const auto& inflight = directoryChange.value();
	const auto& toDirectory = inflight.to;
	if (!fromDirectory || !toDirectory || fromDirectory->enginePath.key == toDirectory->enginePath.key)
	{
		return; // nothing to do
	}
	foreach(const FilePtr &fromFile, fromDirectory->nameFile)
	{
		if (!fromFile->IsArchive() || m_fileChanges.contains(fromFile))
		{
			continue; // not an archive or already modified
		}
		const_cast<CInflightUpdate*>(this)->CreateFileChange(fromFile->keyName, directoryChange, fromFile, CreateFileMut(fromFile));
	}
}

//
//  -------------------------------- Getter ----------------------------------------------
//
template<typename OnUnchangedFile, typename OnFileChange, typename OnUnchangedDirectory, typename OnDirectoryChange, typename OnNotFound>
void CInflightUpdate::InspectKeyEnginePath(const QString& keyEnginePath, const OnUnchangedFile& onUnchangedFile, const OnFileChange& onFileChange, const OnUnchangedDirectory& onUnchangedDirectory, const OnDirectoryChange& onDirectoryChange, const OnNotFound& onNotFound) const
{
	auto pathFileChangeIt = m_pathFileChange.find(keyEnginePath);
	if (pathFileChangeIt != m_pathFileChange.end())
	{
		auto fileChange = pathFileChangeIt.value();
		onFileChange(fileChange);
		return; // found a file change
	}
	auto pathDirectoryChangeIt = m_pathDirectoryChange.find(keyEnginePath);
	if (pathDirectoryChangeIt != m_pathDirectoryChange.end())
	{
		auto directoryChange = pathDirectoryChangeIt.value();
		onDirectoryChange(directoryChange);
		return; // found a directory change
	}
	// no update found
	auto directory = m_base->GetDirectoryByEnginePath(keyEnginePath);
	if (directory)
	{
		auto currentPath = GetLatestKeyEnginePath(keyEnginePath);
		if (currentPath.isEmpty())
		{
			onNotFound();
			return; // directory was there, but is removed
		}
		onUnchangedDirectory(directory, currentPath);
		return; // found an unchanged directory
	}
	auto file = m_base->GetFileByEnginePath(keyEnginePath);
	if (file)
	{
		auto currentPath = GetLatestKeyEnginePath(keyEnginePath);
		if (currentPath.isEmpty())
		{
			onNotFound();
			return; // file was there, but is removed
		}
		onUnchangedFile(file, currentPath);
		return; // found an unchanged file
	}
	onNotFound();
}

QString CInflightUpdate::GetLatestKeyEnginePath(const QString& keyEnginePath) const
{
	QString result;
	const auto keyParts = keyEnginePath.split('/', QString::SkipEmptyParts);
	auto keyEnd = keyParts.end();
	bool hasChanged = false;
	for (auto keyIt = keyParts.begin(); keyIt != keyEnd; ++keyIt)
	{
		const auto& keyPart = *keyIt;
		result = GetCombinedPath(result, keyPart);
		auto changeIt = m_pathDirectoryChange.find(result);
		if (changeIt != m_pathDirectoryChange.end())
		{
			auto directoryChange = changeIt.value();
			const InflightDirectoryChange& inflightChange = directoryChange.value();
			if (!inflightChange.to)
			{
				return QString(); // a parent directory was removed
			}
			result = inflightChange.keyEnginePath;
			hasChanged = true;
		}
	}
	if (!hasChanged)
	{
		return keyEnginePath;
	}
	return result;
}

CInflightUpdate::FileChangeIt CInflightUpdate::FindFileChangeByKeyEnginePath(const QString& keyEnginePath) const
{
	auto pathFileChangeIt = m_pathFileChange.find(keyEnginePath);
	if (pathFileChangeIt != m_pathFileChange.end())
	{
		auto fileChange = pathFileChangeIt.value();
		return fileChange; // found a file change
	}
	return const_cast<CInflightUpdate*>(this)->m_fileChanges.end();
}

CInflightUpdate::DirectoryChangeIt CInflightUpdate::FindDirectoryChangeByEnginePath(const QString& enginePath) const
{
	auto pathDirectoryChangeIt = m_pathDirectoryChange.find(enginePath);
	if (pathDirectoryChangeIt != m_pathDirectoryChange.end())
	{
		auto directoryChange = pathDirectoryChangeIt.value();
		return directoryChange; // found a directory change
	}
	return const_cast<CInflightUpdate*>(this)->m_directoryChanges.end();
}

//
//  -------------------------------- generalized updates to providers ----------------------------------------------
//
void CInflightUpdate::RemoveProviderPath(const Provider provider, const QString& keyEnginePath)
{
	CRY_ASSERT(!keyEnginePath.isEmpty());

	auto onUnchangedFile = [&](const FilePtr& file, const QString& filePath)
	{
		RemoveFileProvider(file, filePath, provider);
	};
	auto onFileChange = [&](const FileChangeIt& fileChange)
	{
		RemoveFileChangeProvider(fileChange, provider);
	};
	auto onUnchangedDirectory = [&](const DirectoryPtr& directory, const QString& directoryPath)
	{
		RemoveDirectoryProvider(directory, directoryPath, provider);
	};
	auto onDirectoryChange = [&](const DirectoryChangeIt& directoryChange)
	{
		RemoveDirectoryChangeProvider(directoryChange, provider);
	};
	auto onNotFound = [](){}; // do nothing

	InspectKeyEnginePath(
	  keyEnginePath,
	  onUnchangedFile, onFileChange,
	  onUnchangedDirectory, onDirectoryChange,
	  onNotFound);
}

void CInflightUpdate::RenameProviderPath(const Provider provider, const QString& keyEnginePath, const SAbsolutePath& toName)
{
	CRY_ASSERT(!keyEnginePath.isEmpty());
	CRY_ASSERT(!toName.key.isEmpty());
	CRY_ASSERT(!toName.key.contains('/'));

	auto onUnchangedFile = [&](const FilePtr& file, const QString& keyFilePath)
	{
		RenameFileProvider(file, keyFilePath, provider, toName);
	};
	auto onFileChange = [&](const FileChangeIt& fileChange)
	{
		RenameFileChangeProvider(fileChange, provider, toName);
	};
	auto onUnchangedDirectory = [&](const DirectoryPtr& directory, const QString& keyDirectoryPath)
	{
		RenameDirectoryProvider(directory, keyDirectoryPath, provider, toName);
	};
	auto onDirectoryChange = [&](const DirectoryChangeIt& directoryChange)
	{
		RenameDirectoryChangeProvider(directoryChange, provider, toName);
	};
	auto onNotFound = [](){}; // we don't know directory or file => do nothing

	InspectKeyEnginePath(
	  keyEnginePath,
	  onUnchangedFile, onFileChange,
	  onUnchangedDirectory, onDirectoryChange,
	  onNotFound);
}

void CInflightUpdate::AddProviderDirectoryWithInfo(const Provider provider, const QString& keyEnginePath, const SDirectoryInfoInternal& info)
{
	SAbsolutePath name;
	name.full = info.fullName;
	name.key = info.keyName;
	AddProviderDirectoryWithName(provider, keyEnginePath, name);
}

void CInflightUpdate::AddProviderDirectoryWithName(const Provider provider, const QString& keyEnginePath, const SAbsolutePath& name)
{
	CRY_ASSERT(!keyEnginePath.isEmpty());

	auto onNotFound = [&]()
	{
		CreateDirectoryWithName(keyEnginePath, provider, name);
	};
	auto onUnchangedFile = [&](const FilePtr& file, const QString& keyFilePath)
	{
		if (keyFilePath == keyEnginePath) // the place is really occupied by a file
		{
			AddFileDirectoryProviderName(file, keyFilePath, provider, name);
			return; // file blocked
		}
		CreateDirectoryWithName(keyEnginePath, provider, name);
	};
	auto onFileChange = [&](const FileChangeIt& fileChange)
	{
		if (fileChange->to && fileChange->keyEnginePath == keyEnginePath) // changed file occupies the path
		{
			AddFileChangeDirectoryProviderName(fileChange, provider, name);
			return; // file blocked
		}
		CreateDirectoryWithName(keyEnginePath, provider, name);
	};
	auto onUnchangedDirectory = [&](const DirectoryPtr& directory, const QString& keyDirectoryPath)
	{
		if (keyDirectoryPath == keyEnginePath)
		{
			AddDirectoryProvider(directory, keyDirectoryPath, provider, name);
			return; // directory exists
		}
		CreateDirectoryWithName(keyEnginePath, provider, name);
	};
	auto onDirectoryChange = [&](const DirectoryChangeIt& directoryChange)
	{
		if (directoryChange->to && directoryChange->keyEnginePath == keyEnginePath)
		{
			AddDirectoryChangeProviderName(directoryChange, provider, name);
			return; // directory exists
		}
		CreateDirectoryWithName(keyEnginePath, provider, name);
	};

	InspectKeyEnginePath(
	  keyEnginePath,
	  onUnchangedFile, onFileChange,
	  onUnchangedDirectory, onDirectoryChange,
	  onNotFound);
}

void CInflightUpdate::AddProviderFile(const Provider provider, const QString& keyEnginePath, const SFileInfoInternal& info)
{
	CRY_ASSERT(!keyEnginePath.isEmpty());

	auto onNotFound = [&]()
	{
		CreateFileWithInfo(keyEnginePath, provider, info);
	};
	auto onUnchangedFile = [&](const FilePtr& file, const QString& keyFilePath)
	{
		if (keyFilePath == keyEnginePath)
		{
			AddFileProviderInfo(file, keyFilePath, provider, info);
			return; // file exists
		}
		CreateFileWithInfo(keyEnginePath, provider, info);
	};
	auto onFileChange = [&](const FileChangeIt& fileChange)
	{
		if (fileChange->to && fileChange->keyEnginePath == keyEnginePath)
		{
			AddFileChangeProviderInfo(fileChange, provider, info);
			return; // file exists
		}
		CreateFileWithInfo(keyEnginePath, provider, info);
	};
	auto onUnchangedDirectory = [&](const DirectoryPtr& directory, const QString& keyDirectoryPath)
	{
		if (keyDirectoryPath == keyEnginePath)
		{
			AddDirectoryFileProviderInfo(directory, keyDirectoryPath, provider, info);
			return; // directory blocks
		}
		CreateFileWithInfo(keyEnginePath, provider, info);
	};
	auto onDirectoryChange = [&](const DirectoryChangeIt& directoryChange)
	{
		if (directoryChange->to && directoryChange->keyEnginePath == keyEnginePath)
		{
			AddDirectoryChangeFileProviderInfo(directoryChange, provider, info);
			return; // directory blocks
		}
		CreateFileWithInfo(keyEnginePath, provider, info);
	};

	InspectKeyEnginePath(
	  keyEnginePath,
	  onUnchangedFile, onFileChange,
	  onUnchangedDirectory, onDirectoryChange,
	  onNotFound);
}

//
//  -------------------------------- archive contents ----------------------------------------------
//
void CInflightUpdate::RemoveArchiveContent(const FileMutPtr& archiveFile, const QString& keyEnginePath)
{
	CRY_ASSERT(archiveFile);
	CRY_ASSERT(!keyEnginePath.isEmpty());
	auto archiveProvider = archiveFile.get();
	auto keyBasePath = GetDirectoryPath(keyEnginePath);

	foreach(const auto & keyArchiveFilePath, archiveFile->archive.filePathes)
	{
		auto keyFileEnginePath = GetCombinedPath(keyBasePath, keyArchiveFilePath);
		RemoveProviderPath(archiveProvider, keyFileEnginePath);
	}
	archiveFile->archive.filePathes.clear();

	foreach(const auto & keyArchiveDirectoryPath, archiveFile->archive.directoryPathes)
	{
		auto keyFileEnginePath = GetCombinedPath(keyBasePath, keyArchiveDirectoryPath);
		RemoveProviderPath(archiveProvider, keyFileEnginePath);
	}
	archiveFile->archive.directoryPathes.clear();
}

void CInflightUpdate::RemoveArchiveContentExcept(const FileMutPtr& archiveFile, const QString& keyEnginePath, const QSet<QString>& filePathes, const QSet<QString>& directoryPathes)
{
	CRY_ASSERT(archiveFile);
	CRY_ASSERT(!keyEnginePath.isEmpty());
	Provider archiveProvider = archiveFile.get();
	auto keyBasePath = GetDirectoryPath(keyEnginePath);

	auto fileItEnd = archiveFile->archive.filePathes.end();
	for (auto fileIt = archiveFile->archive.filePathes.begin(); fileIt != fileItEnd; )
	{
		if (!filePathes.contains(*fileIt))
		{
			auto keyFileEnginePath = GetCombinedPath(keyBasePath, *fileIt);
			RemoveProviderPath(archiveProvider, keyFileEnginePath);
			fileIt = archiveFile->archive.filePathes.erase(fileIt);
		}
		else
		{
			++fileIt;
		}
	}

	auto directoryItEnd = archiveFile->archive.directoryPathes.end();
	for (auto directoryIt = archiveFile->archive.directoryPathes.begin(); directoryIt != directoryItEnd; )
	{
		if (!directoryPathes.contains(*directoryIt))
		{
			auto keyDirectoryEnginePath = GetCombinedPath(keyBasePath, *directoryIt);
			RemoveProviderPath(archiveProvider, keyDirectoryEnginePath);
			directoryIt = archiveFile->archive.directoryPathes.erase(directoryIt);
			directoryItEnd = archiveFile->archive.directoryPathes.end();
		}
		else
		{
			++directoryIt;
		}
	}
}

void CInflightUpdate::UpdateArchiveContent(const DirectoryChangeIt& parentChange, const RawFilePtr fromArchive, const RawFilePtr toArchive)
{
	//Temporarily disabled this functionnality as it was causing crashes and we do not want to handle live update of pak files anyway
	/*if (!fromArchive || !toArchive || !fromArchive->IsArchive() || !toArchive->IsArchive())
	{
		return; // something is missing
	}
	auto directoryItEnd = toArchive->archive.directoryPathes.end();
	for (auto directoryIt = toArchive->archive.directoryPathes.begin(); directoryIt != directoryItEnd; ++directoryIt)
	{
		UpdateDirectoryEnginePathProvider(parentChange, *directoryIt, fromArchive, toArchive);
	}
	auto fileItEnd = toArchive->archive.filePathes.end();
	for (auto fileIt = toArchive->archive.filePathes.begin(); fileIt != fileItEnd; ++fileIt)
	{
		UpdateFileEnginePathProvider(parentChange, *fileIt, fromArchive, toArchive);
	}*/
}
/*
void CInflightUpdate::UpdateFileEnginePathProvider(const DirectoryChangeIt& parentChange, const QString& keyArchivePath, const Provider fromProvider, const Provider toProvider)
{
	auto fileChange = FindFileAndCreateFileChangeIn(parentChange, keyArchivePath);
	if (fileChange == m_fileChanges.end())
	{
		return; // file not found
	}
	FileMutPtr toFile = fileChange->to;
	if (toFile)
	{
		auto fromIt = QHash_ConstFind(toFile->providerDetails, fromProvider);
		if (fromIt == toFile->providerDetails.cend())
		{
			CRY_ASSERT(false);
			return; // from provider not found
		}
		auto providerData = fromIt.value();
		QHash_EraseConstIt(toFile->providerDetails, fromIt);
		toFile->providerDetails[toProvider] = providerData;
	}
	else
	{
		// file was removed recreate
		auto fromFile = fileChange.key();
		auto fromIt = fromFile->providerDetails.find(fromProvider);
		if (fromIt == fromFile->providerDetails.cend())
		{
			CRY_ASSERT(false);
			return; // from provider not found
		}
		auto providerData = fromIt.value();
		fileChange->to = CreateFileMut(fromFile);
		toFile = fileChange->to;
		toFile->providerDetails.clear();
		toFile->providerDetails[toProvider] = providerData;
	}
	UpdateFileChangeProvider(fileChange);
}

void CInflightUpdate::UpdateDirectoryEnginePathProvider(const DirectoryChangeIt& parentChange, const QString& keyParentPath, const Provider fromProvider, const Provider toProvider)
{
	auto directoryChange = FindDirectoryAndCreateDirectoryChangeIn(parentChange, keyParentPath);
	if (directoryChange == m_directoryChanges.end())
	{
		return; // directory not found
	}
	auto toDirectory = directoryChange->to;
	if (toDirectory)
	{
		auto fromIt = QHash_ConstFind(toDirectory->providerDetails, fromProvider);//here providerDetails doesn't contain fromProvider
		if (fromIt == toDirectory->providerDetails.cend())
		{
			CRY_ASSERT(false);
			return; // from provider not found
		}
		auto providerData = fromIt.value();
		QHash_EraseConstIt(toDirectory->providerDetails, fromIt);
		toDirectory->providerDetails[toProvider] = providerData;
	}
	else
	{
		// Directory was removed => recreate
		auto fromDirectory = directoryChange.key();
		auto fromIt = fromDirectory->providerDetails.find(fromProvider);
		if (fromIt == fromDirectory->providerDetails.cend())
		{
			CRY_ASSERT(false);
			return; // from provider not found
		}
		auto providerData = fromIt.value();
		directoryChange->to = CreateDirectoryMut(fromDirectory);
		toDirectory = directoryChange->to;
		toDirectory->providerDetails.clear();
		toDirectory->providerDetails[toProvider] = providerData;
	}
	UpdateDirectoryChangeProvider(directoryChange);
}
*/
//
//  -------------------------------- remove provider implementations ----------------------------------------------
//
void CInflightUpdate::RemoveFileProvider(const FilePtr& file, const QString& filePath, const Provider provider)
{
	CRY_ASSERT(file);
	if (filePath.isEmpty())
	{
		CRY_ASSERT(false); // should not happen
		return;
	}
	if (!file->providerDetails.contains(provider))
	{
		return; // not provided
	}
	auto fileChange = BuildFileChange(filePath, CreateFileMut(file), provider);
	RemoveFileChangeProvider(fileChange, provider);
}

void CInflightUpdate::RemoveFileChangeProvider(const FileChangeIt& fileChange, const Provider provider)
{
	CRY_ASSERT(fileChange != m_fileChanges.end());
	const auto& toFile = fileChange->to;
	if (!toFile)
	{
		return; // no target
	}
	if (!toFile->providerDetails.contains(provider))
	{
		return; // not provided
	}
	toFile->providerDetails.remove(provider);
	UpdateFileChangeProvider(fileChange);
}

void CInflightUpdate::RemoveDirectoryProvider(const DirectoryPtr& directory, const QString& keyDirectoryPath, const Provider provider)
{
	CRY_ASSERT(directory);
	if (keyDirectoryPath.isEmpty())
	{
		return; // no target
	}
	if (!directory->providerDetails.contains(provider))
	{
		return; // not provided
	}
	auto directoryChange = BuildDirectoryChange(keyDirectoryPath, CreateDirectoryMut(directory), provider);
	RemoveDirectoryChangeProvider(directoryChange, provider);
}

void CInflightUpdate::RemoveDirectoryChangeProvider(const DirectoryChangeIt& directoryChange, const Provider provider)
{
	CRY_ASSERT(directoryChange != m_directoryChanges.end());
	const auto& toDirectory = directoryChange->to;
	if (!toDirectory)
	{
		return; // no target
	}
	if (!toDirectory->providerDetails.contains(provider))
	{
		return; // not provided
	}
	toDirectory->providerDetails.remove(provider);
	bool isValid = UpdateDirectoryChangeProvider(directoryChange);
	if (isValid && directoryChange->to)
	{
		RemoveDirectoryChangeChildProvider(directoryChange, provider);
	}
}

//
//  -------------------------------- rename provider implementations ----------------------------------------------
//
void CInflightUpdate::RenameFileProvider(const FilePtr& file, const QString& keyEnginePath, const Provider provider, const SAbsolutePath& toFilename)
{
	CRY_ASSERT(file);
	if (keyEnginePath.isEmpty())
	{
		return; // no target
	}
	if (!file->providerDetails.contains(provider))
	{
		// we lack info to create the file here
		return; // not provided
	}
	auto fileChange = BuildFileChange(keyEnginePath, CreateFileMut(file), provider);
	RenameFileChangeProvider(fileChange, provider, toFilename);
}

void CInflightUpdate::RenameFileChangeProvider(const FileChangeIt& fileChange, const Provider provider, const SAbsolutePath& toFilename)
{
	CRY_ASSERT(fileChange != m_fileChanges.end());
	const auto& toFile = fileChange->to;
	if (!toFile)
	{
		return; // no target
	}
	if (!toFile->providerDetails.contains(provider))
	{
		CRY_ASSERT(false); // this should be prevented
		return;            // not provided
	}
	if (toFile->provider.key() == provider)
	{
		if (toFile->providerDetails.size() > 1) // multiple providers => create a clone for all other providers
		{
			auto cloneFileChange = CloneFileChange(fileChange, nullptr); // provider should not be used
			const auto& cloneToFile = cloneFileChange->to;
			cloneToFile->providerDetails.remove(provider);
			UpdateFileChangeProvider(cloneFileChange);

			QHash_RemoveExcept(toFile->providerDetails, provider);
		}
		UpdateFileChangeProviderFileName(fileChange, provider, toFilename);
		return;
	}
	if (Q_UNLIKELY(toFile->providerDetails.size() == 1))
	{
		CRY_ASSERT(false); // only one provider + not the active one => wrong state
		return;
	}
	auto cloneFileChange = CloneFileChange(fileChange, provider); // provider should not be used
	const FileMutPtr& cloneToFile = cloneFileChange->to;
	QHash_RemoveExcept(cloneToFile->providerDetails, provider);
	UpdateFileChangeProviderFileName(cloneFileChange, provider, toFilename);

	toFile->providerDetails.remove(provider);
	UpdateFileChangeProvider(fileChange);
}

void CInflightUpdate::RenameDirectoryProvider(const DirectoryPtr& directory, const QString& keyEnginePath, const Provider provider, const SAbsolutePath& toName)
{
	CRY_ASSERT(directory);
	if (keyEnginePath.isEmpty())
	{
		return; // no target
	}
	if (!directory->providerDetails.contains(provider))
	{
		auto keyNewPath = GetCombinedPath(GetDirectoryPath(keyEnginePath), toName.key);
		AddProviderDirectoryWithName(provider, keyNewPath, toName);
		return; // not provided => created
	}
	auto directoryChange = BuildDirectoryChange(keyEnginePath, CreateDirectoryMut(directory), nullptr);
	RenameDirectoryChangeProvider(directoryChange, provider, toName);
}

void CInflightUpdate::RenameDirectoryChangeProvider(const DirectoryChangeIt& directoryChange, const Provider provider, const SAbsolutePath& toName)
{
	CRY_ASSERT(directoryChange != m_directoryChanges.end());
	DirectoryMutPtr& toDirectory = directoryChange->to;
	if (!toDirectory)
	{
		toDirectory = directoryChange->to = CreateDirectoryMut();
		SProviderData providerData;
		providerData.isFile = false;
		providerData.fullName = toName.full;
		toDirectory->providerDetails[provider] = providerData;
		UpdateDirectoryChangeProviderName(directoryChange, provider, toName);
		return; // was removed recreated
	}
	if (toDirectory->providerDetails.size() > 1)
	{
		auto cloneDirectoryChange = CloneDirectoryChange(directoryChange, nullptr);

		toDirectory->providerDetails.remove(provider);
		UpdateDirectoryChangeProvider(directoryChange);

		const auto& cloneToDirectory = cloneDirectoryChange->to;
		QHash_RemoveExcept(cloneToDirectory->providerDetails, provider);
		UpdateDirectoryChangeProviderName(cloneDirectoryChange, provider, toName);

		if (cloneDirectoryChange->to) // directory might be merged behind a file
		{
			SplitDirectoryEntriesForProvider(directoryChange, cloneDirectoryChange, provider);
		}
		return; // splitted
	}
	UpdateDirectoryChangeProviderName(directoryChange, provider, toName);
}

//
//  -------------------------------- add provider dictionary implementation ----------------------------------------------
//
void CInflightUpdate::CreateDirectoryWithName(const QString& keyEnginePath, const Provider provider, const SAbsolutePath& name, const ProviderDetails& providerDetails)
{
	SProviderData providerData;
	providerData.isFile = false;
	providerData.fullName = name.full;
	CreateDirectoryWithProviderAndKeyName(keyEnginePath, provider, providerData, name.key, providerDetails);
}

void CInflightUpdate::CreateDirectoryWithProvider(const QString& keyEnginePath, const Provider provider, const SProviderData& providerData, const ProviderDetails& providerDetails)
{
	auto keyName = GetFilenameFromPath(keyEnginePath);
	CreateDirectoryWithProviderAndKeyName(keyEnginePath, provider, providerData, keyName, providerDetails);
}

void CInflightUpdate::CreateDirectoryWithProviderAndKeyName(const QString& keyEnginePath, const Provider provider, const SProviderData& providerData, const QString& keyName, const ProviderDetails& providerDetails)
{
	CRY_ASSERT(!keyEnginePath.isEmpty());
	auto directoryChange = BuildDirectoryChange(keyEnginePath, CreateDirectoryMut(), provider);
	const DirectoryMutPtr& toDirectory = directoryChange->to;
	CRY_ASSERT(toDirectory);
	toDirectory->keyName = keyName;
	toDirectory->providerDetails = providerDetails;
	toDirectory->providerDetails[provider] = providerData;
	toDirectory->provider = SelectProvider(toDirectory->providerDetails);
	CRY_ASSERT(!toDirectory->provider->isFile);
}

void CInflightUpdate::AddFileDirectoryProviderData(const FilePtr& file, const QString& keyFilePath, const Provider provider, const SProviderData& providerData)
{
	CRY_ASSERT(file);
	ProviderDetails providerDetails = file->providerDetails;
	providerDetails[provider] = providerData;
	auto activeProvider = SelectProvider(providerDetails);
	if (activeProvider.key() == provider)
	{
		RemoveFile(file, keyFilePath);
		CreateDirectoryWithProvider(keyFilePath, provider, providerData, providerDetails);
		return; // mutated file to directory
	}
	auto fileChange = BuildFileChange(keyFilePath, CreateFileMut(file), provider);
	auto toFile = fileChange->to;
	toFile->providerDetails = providerDetails;
	toFile->provider = activeProvider;
	CRY_ASSERT(toFile->provider->isFile);
}

void CInflightUpdate::AddFileDirectoryProviderName(const FilePtr& file, const QString& keyFilePath, const Provider provider, const SAbsolutePath& name)
{
	auto providerData = ProviderDataFromDirectoryName(name);
	AddFileDirectoryProviderData(file, keyFilePath, provider, providerData);
}

void CInflightUpdate::AddFileChangeDirectoryProviderData(const FileChangeIt& fileChange, const Provider provider, const SProviderData& providerData)
{
	CRY_ASSERT(fileChange != m_fileChanges.end());
	const FileMutPtr& toFile = fileChange->to;
	CRY_ASSERT(toFile);
	ProviderDetails providerDetails = toFile->providerDetails;
	providerDetails[provider] = providerData;
	auto activeProvider = SelectProvider(providerDetails);
	if (activeProvider.key() == provider)
	{
		auto keyFilePath = fileChange->keyEnginePath;
		RemoveFileChange(fileChange);
		CreateDirectoryWithProvider(keyFilePath, provider, providerData, providerDetails);
		return; // mutated file to directory
	}
	toFile->providerDetails = providerDetails;
	toFile->provider = activeProvider;
	CRY_ASSERT(toFile->provider->isFile);
}

void CInflightUpdate::AddFileChangeDirectoryProviderName(const FileChangeIt& fileChange, const Provider provider, const SAbsolutePath& name)
{
	auto providerData = ProviderDataFromDirectoryName(name);
	AddFileChangeDirectoryProviderData(fileChange, provider, providerData);
}

void CInflightUpdate::AddDirectoryProvider(const DirectoryPtr& directory, const QString& directoryPath, const Provider provider, const SAbsolutePath& name)
{
	CRY_ASSERT(directory);
	if (directoryPath.isEmpty())
	{
		CRY_ASSERT(false); // should not happen
		return;
	}
	auto directoryChange = BuildDirectoryChange(directoryPath, CreateDirectoryMut(directory), provider);
	AddDirectoryChangeProviderName(directoryChange, provider, name);
}

void CInflightUpdate::AddDirectoryChangeProviderData(const DirectoryChangeIt& directoryChange, const Provider provider, const SProviderData& providerData)
{
	CRY_ASSERT(directoryChange != m_directoryChanges.end());
	InflightDirectoryChange& inflight = directoryChange.value();
	DirectoryMutPtr& toDirectory = inflight.to;
	if (!toDirectory)
	{
		toDirectory = inflight.to = CreateDirectoryMut();
		toDirectory->keyName = GetFilenameFromPath(directoryChange->keyEnginePath);
	}
	toDirectory->providerDetails[provider] = providerData;
	toDirectory->provider = SelectProvider(toDirectory->providerDetails);
	CRY_ASSERT(!toDirectory->provider->isFile);
}

void CInflightUpdate::AddDirectoryChangeProviderName(const DirectoryChangeIt& directoryChange, const Provider provider, const SAbsolutePath& name)
{
	CRY_ASSERT(directoryChange != m_directoryChanges.end());
	InflightDirectoryChange& inflight = directoryChange.value();
	DirectoryMutPtr& toDirectory = inflight.to;
	if (!toDirectory)
	{
		toDirectory = inflight.to = CreateDirectoryMut();
		toDirectory->keyName = name.key;
	}
	else if (provider)
	{
		auto endIte = toDirectory->providerDetails.end();
		for (auto ite = toDirectory->providerDetails.begin(); ite != endIte; )
		{
			if (ite.key() && ite.key()->keyName == provider->keyName)
			{
				ite = toDirectory->providerDetails.erase(ite);
				endIte = toDirectory->providerDetails.end();
			}
			else
			{
				++ite;
			}
		}
	}
	toDirectory->providerDetails[provider] = ProviderDataFromDirectoryName(name);
	toDirectory->provider = SelectProvider(toDirectory->providerDetails);
	CRY_ASSERT(!toDirectory->provider->isFile);
}

//
//  -------------------------------- add provider file implementation ----------------------------------------------
//
void CInflightUpdate::CreateFileWithInfo(const QString& keyEnginePath, const Provider provider, const SFileInfoInternal& info, const ProviderDetails& providerDetails)
{
	CRY_ASSERT(!keyEnginePath.isEmpty());
	auto providerData = ProviderDataFromFileInfo(info);
	CreateFileWithProviderKeyNameAndExtension(keyEnginePath, provider, providerData, info.keyName, info.extension, providerDetails);
}

void CInflightUpdate::CreateFileWithProvider(const QString& keyEnginePath, const Provider provider, const SProviderData& providerData, const ProviderDetails& providerDetails)
{
	auto keyName = GetFilenameFromPath(keyEnginePath);
	auto extension = GetFileExtensionFromFilename(keyName);
	CreateFileWithProviderKeyNameAndExtension(keyEnginePath, provider, providerData, keyName, extension, providerDetails);
}

void CInflightUpdate::CreateFileWithProviderKeyNameAndExtension(const QString& keyEnginePath, const Provider provider, const SProviderData& providerData, const QString& keyName, const QString& extension, const ProviderDetails& providerDetails)
{
	CRY_ASSERT(!keyEnginePath.isEmpty());
	auto fileChange = BuildFileChange(keyEnginePath, CreateFileMut(), provider);
	const FileMutPtr& toFile = fileChange->to;
	CRY_ASSERT(toFile);
	toFile->keyName = keyName;
	toFile->extension = extension;
	toFile->providerDetails = providerDetails;
	toFile->providerDetails[provider] = providerData;
	toFile->provider = SelectProvider(toFile->providerDetails);
	CRY_ASSERT(toFile->provider->isFile);
}

void CInflightUpdate::AddFileProviderData(const FilePtr& file, const QString& keyFilePath, const Provider provider, const SProviderData& providerData)
{
	CRY_ASSERT(file);
	if (keyFilePath.isEmpty())
	{
		return; // no target
	}
	auto fileChange = BuildFileChange(keyFilePath, CreateFileMut(file), provider);
	AddFileChangeProviderData(fileChange, provider, providerData);
}

void CInflightUpdate::AddFileProviderInfo(const FilePtr& file, const QString& keyFilePath, const Provider provider, const SFileInfoInternal& info)
{
	CRY_ASSERT(file);
	if (keyFilePath.isEmpty())
	{
		return; // no target
	}
	auto fileChange = BuildFileChange(keyFilePath, CreateFileMut(file), provider);
	AddFileChangeProviderInfo(fileChange, provider, info);
}

void CInflightUpdate::AddFileChangeProviderData(const FileChangeIt& fileChange, const Provider provider, const SProviderData& providerData)
{
	CRY_ASSERT(fileChange != m_fileChanges.end());
	InflightFileChange& inflight = fileChange.value();
	FileMutPtr& toFile = inflight.to;
	if (!toFile)
	{
		toFile = inflight.to = CreateFileMut();
		toFile->keyName = GetFilenameFromPath(fileChange->keyEnginePath);
		toFile->extension = GetFileExtensionFromFilename(toFile->keyName);
	}
	toFile->providerDetails[provider] = providerData;
	toFile->provider = SelectProvider(toFile->providerDetails);
	CRY_ASSERT(toFile->provider->isFile);
}

void CInflightUpdate::AddFileChangeProviderInfo(const FileChangeIt& fileChange, const Provider provider, const SFileInfoInternal& info)
{
	CRY_ASSERT(fileChange != m_fileChanges.end());
	InflightFileChange& inflight = fileChange.value();
	FileMutPtr& toFile = inflight.to;
	if (!toFile)
	{
		toFile = inflight.to = CreateFileMut();
		toFile->keyName = info.keyName;
		toFile->extension = info.extension;
	}
	else if (provider)
	{
		auto endIte = toFile->providerDetails.end();
		for (auto ite = toFile->providerDetails.begin(); ite != endIte; )
		{
			if (ite.key() && ite.key()->keyName == provider->keyName)
			{
				ite = toFile->providerDetails.erase(ite);
				endIte = toFile->providerDetails.end();
			}
			else
			{
				++ite;
			}
		}
	}

	toFile->providerDetails[provider] = ProviderDataFromFileInfo(info);
	toFile->provider = SelectProvider(toFile->providerDetails);
	CRY_ASSERT(toFile->provider->isFile);
}

void CInflightUpdate::AddDirectoryFileProviderData(const DirectoryPtr& directory, const QString& keyDirectoryPath, const Provider provider, const SProviderData& providerData)
{
	CRY_ASSERT(directory);
	ProviderDetails providerDetails = directory->providerDetails;
	providerDetails[provider] = providerData;
	auto activeProvider = SelectProvider(providerDetails);
	if (activeProvider.key() == provider)
	{
		RemoveDirectory(directory, keyDirectoryPath);
		CreateFileWithProvider(keyDirectoryPath, provider, providerData, providerDetails);
		return; // mutated directory to file
	}
	auto directoryChange = BuildDirectoryChange(keyDirectoryPath, CreateDirectoryMut(directory), provider);
	auto toDirectory = directoryChange->to;
	toDirectory->providerDetails = providerDetails;
	toDirectory->provider = activeProvider;
	CRY_ASSERT(!toDirectory->provider->isFile);
}

void CInflightUpdate::AddDirectoryFileProviderInfo(const DirectoryPtr& directory, const QString& keyDirectoryPath, const Provider provider, const SFileInfoInternal& info)
{
	CRY_ASSERT(directory);
	ProviderDetails providerDetails = directory->providerDetails;
	providerDetails[provider] = ProviderDataFromFileInfo(info);
	auto activeProvider = SelectProvider(providerDetails);
	if (activeProvider.key() == provider)
	{
		RemoveDirectory(directory, keyDirectoryPath);
		CreateFileWithInfo(keyDirectoryPath, provider, info, providerDetails);
		return; // mutated directory to file
	}
	auto directoryChange = BuildDirectoryChange(keyDirectoryPath, CreateDirectoryMut(directory), provider);
	auto toDirectory = directoryChange->to;
	toDirectory->providerDetails = providerDetails;
	toDirectory->provider = activeProvider;
	CRY_ASSERT(!toDirectory->provider->isFile);
}

void CInflightUpdate::AddDirectoryChangeFileProviderData(const DirectoryChangeIt& directoryChange, const Provider provider, const SProviderData& providerData)
{
	CRY_ASSERT(directoryChange != m_directoryChanges.end());
	const DirectoryMutPtr& toDirectory = directoryChange->to;
	CRY_ASSERT(toDirectory);
	ProviderDetails providerDetails = toDirectory->providerDetails;
	providerDetails[provider] = providerData;
	auto activeProvider = SelectProvider(providerDetails);
	if (activeProvider.key() == provider)
	{
		auto keyDirectoryPath = directoryChange->keyEnginePath;
		RemoveDirectoryChange(directoryChange);
		CreateFileWithProvider(keyDirectoryPath, provider, providerData, providerDetails);
		return; // mutated directory to file
	}
	toDirectory->providerDetails = providerDetails;
	toDirectory->provider = activeProvider;
	CRY_ASSERT(!toDirectory->provider->isFile);
}

void CInflightUpdate::AddDirectoryChangeFileProviderInfo(const DirectoryChangeIt& directoryChange, const Provider provider, const SFileInfoInternal& info)
{
	CRY_ASSERT(directoryChange != m_directoryChanges.end());
	const DirectoryMutPtr& toDirectory = directoryChange->to;
	CRY_ASSERT(toDirectory);
	ProviderDetails providerDetails = toDirectory->providerDetails;
	providerDetails[provider] = ProviderDataFromFileInfo(info);
	auto activeProvider = SelectProvider(providerDetails);
	if (activeProvider.key() == provider)
	{
		auto keyDirectoryPath = directoryChange->keyEnginePath;
		RemoveDirectoryChange(directoryChange);
		CreateFileWithInfo(keyDirectoryPath, provider, info, providerDetails);
		return; // mutated directory to file
	}
	toDirectory->providerDetails = providerDetails;
	toDirectory->provider = activeProvider;
	CRY_ASSERT(!toDirectory->provider->isFile);
}

//
//  -------------------------------- change builders ----------------------------------------------
//
CInflightUpdate::DirectoryChangeIt CInflightUpdate::BuildDirectoryChangeFromRoot(const QString& keyEnginePath, const Provider provider)
{
	// try to find path directly
	{
		auto it = m_pathDirectoryChange.find(keyEnginePath);
		if (it != m_pathDirectoryChange.end())
		{
			return *it; // success
		}
	}
	// walk from the root
	DirectoryChangeIt result = m_root;
	const auto keyPathParts = keyEnginePath.split('/', QString::SkipEmptyParts);
	QString currentKeyPath;
	auto itEnd = keyPathParts.end();
	auto it = keyPathParts.begin();
	for (; it != itEnd; ++it)
	{
		auto keyPathPart = *it;
		auto searchKeyPath = GetCombinedPath(currentKeyPath, keyPathPart);
		auto it = m_pathDirectoryChange.find(searchKeyPath);
		if (it == m_pathDirectoryChange.end())
		{
			break; // missing change
		}
		auto directoryChange = it.value();
		const InflightDirectoryChange& inflightChange = directoryChange.value();
		if (!inflightChange.to)
		{
			break; // current change removes
		}
		currentKeyPath = inflightChange.keyEnginePath;
		result = directoryChange;
	}
	for (; it != itEnd; ++it)
	{
		auto keyPathPart = *it;
		SEnginePath directoryName;
		directoryName.key = keyPathPart;
		directoryName.full = keyPathPart; // if directory is not created properly it only has key name
		result = CreateDirectoryChangeOnParent(directoryName, provider, result);
	}
	return result;
}

CInflightUpdate::DirectoryChangeIt CInflightUpdate::FindDirectoryAndCreateDirectoryChangeIn(const DirectoryChangeIt& parentChange, const QString& keyParentPath)
{
	const auto keyPathParts = keyParentPath.split('/', QString::SkipEmptyParts);
	DirectoryChangeIt currentChange = parentChange;
	auto itEnd = keyPathParts.end();
	auto it = keyPathParts.begin();
	for (; it != itEnd; ++it)
	{
		auto keyPathPart = *it;
		InflightDirectoryChange& inflight = currentChange.value();
		auto searchPath = GetCombinedPath(inflight.keyEnginePath, keyPathPart);
		auto nextIt = std::find_if(inflight.directoryChanges.cbegin(), inflight.directoryChanges.cend(), [&](const DirectoryChangeIt& candidate)
				{
					return candidate->keyEnginePath == searchPath;
		    });
		if (nextIt == inflight.directoryChanges.cend())
		{
			break;
		}
		currentChange = *nextIt;
	}
	for (; it != itEnd; ++it)
	{
		auto keyPathPart = *it;
		const DirectoryPtr& currentFrom = currentChange.key();
		if (!currentFrom)
		{
			return m_directoryChanges.end(); // nothing to search in
		}
		auto nextIt = currentFrom->nameDirectory.find(keyPathPart);
		if (nextIt == currentFrom->nameDirectory.end())
		{
			return m_directoryChanges.end(); // not found
		}
		auto nextFrom = *nextIt;
		currentChange = CreateDirectoryChange(keyPathPart, currentChange, nextFrom, CreateDirectoryMut(nextFrom));
	}
	return currentChange;
}

CInflightUpdate::FileChangeIt CInflightUpdate::FindFileAndCreateFileChangeIn(const DirectoryChangeIt& parentChange, const QString& keyFileEnginePath)
{
	const auto keyDirectoryPath = GetDirectoryPath(keyFileEnginePath);
	const auto keyFileName = GetFilenameFromPath(keyFileEnginePath);
	DirectoryChangeIt directoryChange = FindDirectoryAndCreateDirectoryChangeIn(parentChange, keyDirectoryPath);
	if (directoryChange == m_directoryChanges.end())
	{
		return m_fileChanges.end(); // directory not found
	}
	InflightDirectoryChange& directoryInflight = directoryChange.value();

	auto searchPath = GetCombinedPath(directoryInflight.keyEnginePath, keyFileName);
	auto fileChangeIt = std::find_if(directoryInflight.fileChanges.cbegin(), directoryInflight.fileChanges.cend(), [&](const FileChangeIt& candidate)
			{
				return candidate->keyEnginePath == searchPath;
	    });
	if (fileChangeIt != directoryInflight.fileChanges.cend())
	{
		return *fileChangeIt; // found it
	}
	const DirectoryPtr& directoryFrom = directoryChange.key();
	if (!directoryFrom)
	{
		return m_fileChanges.end(); // nothing to search in
	}
	auto fileFromIt = directoryFrom->nameFile.find(keyFileName);
	if (fileFromIt == directoryFrom->nameFile.end())
	{
		return m_fileChanges.end(); // not found
	}
	auto fileFrom = *fileFromIt;
	return CreateFileChange(keyFileName, directoryChange, fileFrom, CreateFileMut(fileFrom));
}

CInflightUpdate::DirectoryChangeIt CInflightUpdate::BuildDirectoryChange(const QString& keyEnginePath, const DirectoryMutPtr& directory, const Provider provider)
{
	auto keyParentPath = GetDirectoryPath(keyEnginePath);
	auto keyDirectoryName = GetFilenameFromPath(keyEnginePath);
	auto parentChange = BuildDirectoryChangeFromRoot(keyParentPath, provider);
	const DirectoryPtr& parentFrom = parentChange.key();
	const DirectoryPtr& from = parentFrom ? parentFrom->nameDirectory[keyDirectoryName] : DirectoryPtr();
	return CreateDirectoryChange(keyDirectoryName, parentChange, from, directory);
}

CInflightUpdate::FileChangeIt CInflightUpdate::BuildFileChange(const QString& keyEnginePath, const FileMutPtr& toFile, const Provider provider)
{
	auto keyDirectoryPath = GetDirectoryPath(keyEnginePath);
	auto keyFileName = GetFilenameFromPath(keyEnginePath);
	auto directoryChange = BuildDirectoryChangeFromRoot(keyDirectoryPath, provider);
	const DirectoryPtr& directoryFrom = directoryChange.key();
	const FilePtr& fromFile = directoryFrom ? directoryFrom->nameFile[keyFileName] : FilePtr();
	return CreateFileChange(keyFileName, directoryChange, fromFile, toFile);
}

CInflightUpdate::DirectoryChangeIt CInflightUpdate::CreateDirectoryChange(const QString& keyDirectoryName, const DirectoryChangeIt& parentChange, const DirectoryPtr& fromDirectory, const DirectoryMutPtr& toDirectory)
{
	InflightDirectoryChange change;
	change.keyEnginePath = GetCombinedPath(parentChange->keyEnginePath, keyDirectoryName);
	change.to = toDirectory;
	return StoreDirectoryChangeOnParent(fromDirectory, change, parentChange);
}

CInflightUpdate::FileChangeIt CInflightUpdate::CreateFileChange(const QString& keyFileName, const DirectoryChangeIt& parentChange, const FilePtr& fromFile, const FileMutPtr& toFile)
{
	InflightFileChange change;
	change.keyEnginePath = GetCombinedPath(parentChange->keyEnginePath, keyFileName);
	change.to = toFile;
	if (toFile)
	{
		toFile->enginePath.key = change.keyEnginePath; // required to use the file as provider
	}
	auto result = StoreFileChangeOnParent(fromFile, change, parentChange);
	UpdateArchiveContent(parentChange, fromFile.get(), toFile.get());
	return result;
}

CInflightUpdate::DirectoryChangeIt CInflightUpdate::CloneDirectoryChange(const DirectoryChangeIt& directoryChange, const Provider provider)
{
	auto keyParentDirectoryPath = GetDirectoryPath(directoryChange->keyEnginePath);
	auto parentChange = BuildDirectoryChangeFromRoot(keyParentDirectoryPath, provider);
	InflightDirectoryChange change;
	change.keyEnginePath = directoryChange->keyEnginePath;
	change.to = CreateDirectoryMut(directoryChange->to);
	auto from = directoryChange.key(); // clone uses the same from node
	// do not store pathes
	auto result = m_directoryChanges.insert(from, change);
	parentChange->directoryChanges << result;
	return result;
}

CInflightUpdate::FileChangeIt CInflightUpdate::CloneFileChange(const FileChangeIt& fileChange, const Provider provider)
{
	auto keyDirectoryPath = GetDirectoryPath(fileChange->keyEnginePath);
	auto parentChange = BuildDirectoryChangeFromRoot(keyDirectoryPath, provider);
	InflightFileChange change;
	change.keyEnginePath = fileChange->keyEnginePath;
	change.to = CreateFileMut(fileChange->to);
	auto from = fileChange.key(); // clone uses the same from node
	// do not store pathes
	auto result = m_fileChanges.insert(from, change);
	parentChange->fileChanges << result;
	return result;
}

CInflightUpdate::FileChangeIt CInflightUpdate::MakeFileChangeByEnginePath(const QString& keyEnginePath, const Provider provider)
{
	auto result = FindFileChangeByKeyEnginePath(keyEnginePath);
	if (result != m_fileChanges.end())
	{
		return result; // already existed
	}
	auto fromFile = m_base->GetFileByEnginePath(keyEnginePath);
	if (!fromFile)
	{
		return result; // could not find it
	}
	result = BuildFileChange(keyEnginePath, CreateFileMut(fromFile), provider);
	return result;
}

CInflightUpdate::DirectoryChangeIt CInflightUpdate::MakeDirectoryCangeByEnginePath(const QString& enginePath, const Provider provider)
{
	auto result = FindDirectoryChangeByEnginePath(enginePath);
	if (result != m_directoryChanges.end())
	{
		return result;
	}
	auto fromDirectory = m_base->GetDirectoryByEnginePath(enginePath);
	if (!fromDirectory)
	{
		return result; // could not find it
	}
	result = BuildDirectoryChange(enginePath, CreateDirectoryMut(fromDirectory), provider);
	return result;
}

CInflightUpdate::DirectoryChangeIt CInflightUpdate::CreateDirectoryChangeOnParent(const SEnginePath& directoryName, const Provider provider, DirectoryChangeIt& parentDirectoryChange)
{
	const DirectoryPtr& parentFrom = parentDirectoryChange.key();
	const DirectoryPtr& fromDirectory = parentFrom ? parentFrom->nameDirectory[directoryName.key] : DirectoryPtr();
	DirectoryMutPtr toDirectory = CreateDirectoryMut(fromDirectory);
	if (!fromDirectory)
	{
		toDirectory->keyName = directoryName.key;
		SProviderData providerData;
		providerData.isFile = false;
		providerData.fullName = directoryName.full;
		auto providerIt = toDirectory->providerDetails.insert(provider, providerData);
		toDirectory->provider = providerIt;
	}
	return CreateDirectoryChange(directoryName.key, parentDirectoryChange, fromDirectory, toDirectory);
}

CInflightUpdate::DirectoryChangeIt CInflightUpdate::StoreDirectoryChangeOnParent(const DirectoryPtr& fromDirectory, const InflightDirectoryChange& change, const DirectoryChangeIt& parentDirectoryChange)
{
	auto result = m_directoryChanges.insert(fromDirectory, change);
	parentDirectoryChange->directoryChanges << result;
	auto keyDirectoryName = GetFilenameFromPath(change.keyEnginePath);
	foreach(const QString &parentCachedPath, parentDirectoryChange->cachedPathes)
	{
		auto cachePath = GetCombinedPath(parentCachedPath, keyDirectoryName);
		AddPathDirectoryChangeCacheEntry(cachePath, result);
	}
	return result;
}

CInflightUpdate::FileChangeIt CInflightUpdate::StoreFileChangeOnParent(const FilePtr& fromFile, const InflightFileChange& change, const DirectoryChangeIt& parentDirectoryChange)
{
	auto result = m_fileChanges.insert(fromFile, change);
	parentDirectoryChange->fileChanges << result;
	auto keyFileName = GetFilenameFromPath(change.keyEnginePath);
	foreach(const QString &parentPath, parentDirectoryChange->cachedPathes)
	{
		auto cachePath = GetCombinedPath(parentPath, keyFileName);
		AddPathFileChangeCacheEntry(cachePath, result);
	}
	return result;
}

//
//  -------------------------------- more support ----------------------------------------------
//
void CInflightUpdate::UpdateFileChangeProvider(const FileChangeIt& fileChange)
{
	FileMutPtr& toFile = fileChange->to;
	CRY_ASSERT(toFile);

	auto newProvider = SelectProvider(toFile->providerDetails);
	if (newProvider == toFile->provider)
	{
		return; // nothing changed
	}
	toFile->provider = newProvider;
	if (newProvider == toFile->providerDetails.cend())
	{
		RemoveFileChange(fileChange); // we lack details to recover otherwise
		return;
	}
	if (!newProvider->isFile)
	{
		// mutate file to directory
		auto keyEnginePath = fileChange->keyEnginePath;
		auto providerDetails = toFile->providerDetails;
		RemoveFileChange(fileChange);

		auto directoryChange = BuildDirectoryChangeFromRoot(keyEnginePath, newProvider.key());

		DirectoryMutPtr& toDirectory = directoryChange->to;
		toDirectory->provider = newProvider;
		toDirectory->providerDetails = providerDetails;
		// TODO: ?? Add directory contents from the providers
	}
}

bool CInflightUpdate::UpdateDirectoryChangeProvider(const DirectoryChangeIt& directoryChange)
{
	DirectoryMutPtr& toDirectory = directoryChange->to;
	CRY_ASSERT(toDirectory);

	auto newProvider = SelectProvider(toDirectory->providerDetails);
	if (newProvider == toDirectory->provider)
	{
		return true; // nothing more to tune
	}
	toDirectory->provider = newProvider;
	if (newProvider == toDirectory->providerDetails.cend())
	{
		RemoveDirectoryChange(directoryChange);
		return false; // directoryChange is removed
	}
	if (newProvider->isFile)
	{
		// mutate directory to file
		auto keyEnginePath = directoryChange->keyEnginePath;
		auto providerDetails = toDirectory->providerDetails;
		RemoveDirectoryChange(directoryChange);

		CreateFileWithProvider(keyEnginePath, newProvider.key(), newProvider.value(), providerDetails);
		return false; // directoryChange is removed
	}
	return true;
}

void CInflightUpdate::RemoveFile(const FilePtr& file, const QString& keyEnginePath)
{
	if (!file)
	{
		return; // no file
	}
	auto fileChange = MakeFileChangeByEnginePath(keyEnginePath, nullptr);
	if (fileChange == m_fileChanges.end())
	{
		return; // not known
	}
	RemoveFileChange(fileChange);
}

void CInflightUpdate::RemoveFileChange(const FileChangeIt& fileChange)
{
	const auto& toFile = fileChange->to;
	if (!toFile)
	{
		return; // already removed
	}
	if (toFile->IsArchive())
	{
		RemoveArchiveContent(toFile, fileChange->keyEnginePath);
	}
	const auto& fromFile = fileChange.key();
	if (!fromFile || 1 < m_fileChanges.count(fromFile))
	{
		RevertFileChange(fileChange);
		return; // reverted
	}
	fileChange->to = FileMutPtr();
}

void CInflightUpdate::RemoveDirectory(const DirectoryPtr& directory, const QString& keyEnginePath)
{
	if (!directory)
	{
		return; // no directory
	}
	auto directoryChange = MakeDirectoryCangeByEnginePath(keyEnginePath, nullptr);
	if (directoryChange == m_directoryChanges.end())
	{
		return; // not known
	}
	RemoveDirectoryChange(directoryChange);
}

void CInflightUpdate::RemoveDirectoryChange(const DirectoryChangeIt& directoryChange)
{
	const auto& toDirectory = directoryChange->to;
	if (!toDirectory)
	{
		return; // already removed
	}
	const auto& fromDirectory = directoryChange.key();
	if (!fromDirectory || 1 < m_directoryChanges.count(fromDirectory))
	{
		RevertDirectoryChange(directoryChange);
		return; // reverted
	}
	else
	{
		DeepRevertChildChanges(directoryChange);
	}
	directoryChange->to = DirectoryMutPtr();
}

void CInflightUpdate::RevertFileChange(const FileChangeIt& fileChange)
{
	InflightFileChange& inflight = fileChange.value();
	foreach(auto & path, inflight.cachedPathes)
	{
		m_pathFileChange.remove(path);
	}
	auto keyDirectoryPath = GetDirectoryPath(inflight.keyEnginePath);
	auto pathDirectoryChangeIt = m_pathDirectoryChange.constFind(keyDirectoryPath);
	if (pathDirectoryChangeIt != m_pathDirectoryChange.constEnd())
	{
		const DirectoryChangeIt& directoryChange = pathDirectoryChangeIt.value();
		InflightDirectoryChange& directoryInflight = directoryChange.value();
		directoryInflight.fileChanges.removeOne(fileChange);
	}
	m_fileChanges.erase(fileChange);
}

void CInflightUpdate::RevertDirectoryChange(const DirectoryChangeIt& directoryChange)
{
	DeepRevertChildChanges(directoryChange);
	InflightDirectoryChange& inflight = directoryChange.value();
	foreach(auto & path, inflight.cachedPathes)
	{
		m_pathDirectoryChange.remove(path);
	}
	auto keyParentDirectoryPath = GetDirectoryPath(inflight.keyEnginePath);
	auto parentPathDirectoryChangeIt = m_pathDirectoryChange.constFind(keyParentDirectoryPath);
	if (parentPathDirectoryChangeIt != m_pathDirectoryChange.constEnd())
	{
		const DirectoryChangeIt& parentDirectoryChange = parentPathDirectoryChangeIt.value();
		InflightDirectoryChange& parentDirectoryInflight = parentDirectoryChange.value();
		parentDirectoryInflight.directoryChanges.removeOne(directoryChange);
	}
	m_directoryChanges.erase(directoryChange);
}

void CInflightUpdate::RevertFileChangeNested(const FileChangeIt& fileChange)
{
	InflightFileChange& inflight = fileChange.value();
	foreach(auto & path, inflight.cachedPathes)
	{
		m_pathFileChange.remove(path);
	}
	m_fileChanges.erase(fileChange);
}

void CInflightUpdate::RevertDirectoryChangeNested(const DirectoryChangeIt& directoryChange)
{
	DeepRevertChildChanges(directoryChange);
	InflightDirectoryChange& inflight = directoryChange.value();
	foreach(auto & path, inflight.cachedPathes)
	{
		m_pathDirectoryChange.remove(path);
	}
	m_directoryChanges.erase(directoryChange);
}

void CInflightUpdate::DeepRevertChildChanges(const DirectoryChangeIt& directoryChange)
{
	InflightDirectoryChange& inflight = directoryChange.value();
	foreach(auto & fileChange, inflight.fileChanges)
	{
		RevertFileChangeNested(fileChange);
	}
	inflight.fileChanges.clear();
	foreach(auto & directoryChange, inflight.directoryChanges)
	{
		RevertDirectoryChangeNested(directoryChange);
	}
	inflight.directoryChanges.clear();
}

void CInflightUpdate::RemoveDirectoryChangeChildProvider(const DirectoryChangeIt& directoryChange, const Provider provider)
{
	InflightDirectoryChange& inflight = directoryChange.value();
	foreach(auto & fileChange, inflight.fileChanges)
	{
		RemoveFileChangeProvider(fileChange, provider);
	}
	foreach(auto & subDirectoryChange, inflight.directoryChanges)
	{
		RemoveDirectoryChangeProvider(subDirectoryChange, provider);
	}
	// remove provider from non-changed children
	const DirectoryPtr& fromDirectory = directoryChange.key();
	if (!fromDirectory)
	{
		return; // nothing to inspect
	}
	const DirectoryPtr& toDirectory = directoryChange->to;
	if (!toDirectory)
	{
		CRY_ASSERT(false);
		return; // do not call this method
	}
	auto keyDirectoryPath = inflight.keyEnginePath;
	foreach(const auto & file, fromDirectory->nameFile)
	{
		if (m_fileChanges.contains(file))
		{
			continue; // was handled above
		}
		auto keyFilePath = GetCombinedPath(keyDirectoryPath, file->keyName);
		RemoveFileProvider(file, keyFilePath, provider);
	}
	foreach(const auto & subDirectory, fromDirectory->nameDirectory)
	{
		if (m_directoryChanges.contains(subDirectory))
		{
			continue; // was handled above
		}
		auto keySubDirectoryPath = GetCombinedPath(keyDirectoryPath, subDirectory->keyName);
		RemoveDirectoryProvider(subDirectory, keySubDirectoryPath, provider);
	}
}

//
//  -------------------------------- Split directory entries ----------------------------------------------
//
void CInflightUpdate::SplitDirectoryEntriesForProvider(const DirectoryChangeIt& fromDirectoryChange, const DirectoryChangeIt& toDirectoryChange, const Provider provider)
{
	InflightDirectoryChange& fromInflight = fromDirectoryChange.value();
	foreach(auto & fileChange, fromInflight.fileChanges)
	{
		SplitFileChangeInDirectoryForProvider(fileChange, fromDirectoryChange, toDirectoryChange, provider);
	}
	foreach(auto & subDirectoryChange, fromInflight.directoryChanges)
	{
		SplitSubDirectoryChangeInDirectoryForProvider(subDirectoryChange, fromDirectoryChange, toDirectoryChange, provider);
	}
	const DirectoryPtr& fromDirectory = fromDirectoryChange.key();
	if (fromDirectory)
	{
		foreach(const auto & file, fromDirectory->nameFile)
		{
			if (m_fileChanges.contains(file))
			{
				continue; // was included above
			}
			SplitFileInDirectoryForProvider(file, fromDirectoryChange, toDirectoryChange, provider);
		}
		foreach(const auto & subDirectory, fromDirectory->nameDirectory)
		{
			if (m_directoryChanges.contains(subDirectory))
			{
				continue; // was included above
			}
			SplitSubDirectoryInDirectoryForProvider(subDirectory, fromDirectoryChange, toDirectoryChange, provider);
		}
	}
}

void CInflightUpdate::SplitFileInDirectoryForProvider(const FilePtr& file, const DirectoryChangeIt& fromDirectoryChange, const DirectoryChangeIt& toDirectoryChange, const Provider provider)
{
	CRY_ASSERT(file);
	if (!file->providerDetails.contains(provider))
	{
		return; // not affected
	}
	auto fileChange = CreateFileChange(file->keyName, fromDirectoryChange, file, CreateFileMut(file));
	return SplitFileChangeInDirectoryForProvider(fileChange, fromDirectoryChange, toDirectoryChange, provider);
}

void CInflightUpdate::SplitFileChangeInDirectoryForProvider(const FileChangeIt& fileChange, const DirectoryChangeIt& fromDirectoryChange, const DirectoryChangeIt& toDirectoryChange, const Provider provider)
{
	const FileMutPtr& toFile = fileChange->to;
	if (!toFile || !toFile->providerDetails.contains(provider))
	{
		return; // not affected
	}
	CRY_ASSERT(toFile->provider.key() == provider || toFile->providerDetails.size() > 1); // we should have been the provider then
	if (toFile->provider.key() == provider || toFile->providerDetails.size() == 1)
	{
		// create a clone for all other providers
		auto cloneFileChange = CloneFileChange(fileChange, nullptr); // provider should not be used
		MoveFileChangeCachedPathes(fileChange, cloneFileChange);
		const auto& cloneToFile = cloneFileChange->to;
		cloneToFile->providerDetails.remove(provider);
		if (cloneToFile->providerDetails.isEmpty())
		{
			cloneFileChange->to = FileMutPtr(); // we need a marker that the path was removed
		}
		else
		{
			UpdateFileChangeProvider(cloneFileChange);
		}

		// move fileChange to toDirectoryChange
		QHash_RemoveExcept(toFile->providerDetails, provider);
		toFile->provider = toFile->providerDetails.begin();
		fromDirectoryChange->fileChanges.removeOne(fileChange);
		toDirectoryChange->fileChanges << fileChange;
		SetFileChangeActiveDirectoryPath(fileChange, toDirectoryChange->keyEnginePath);
		return;
	}
	// clone for provider itself
	auto cloneFileChange = CloneFileChange(fileChange, provider); // provider should not be used
	const FileMutPtr& cloneToFile = cloneFileChange->to;
	QHash_RemoveExcept(cloneToFile->providerDetails, provider);
	cloneToFile->provider = cloneToFile->providerDetails.begin();
	fromDirectoryChange->fileChanges.removeOne(cloneFileChange);
	toDirectoryChange->fileChanges << cloneFileChange;
	SetFileChangeActiveDirectoryPath(cloneFileChange, toDirectoryChange->keyEnginePath);

	toFile->providerDetails.remove(provider);
	UpdateFileChangeProvider(fileChange);
}

void CInflightUpdate::SplitSubDirectoryInDirectoryForProvider(const DirectoryPtr& directory, const DirectoryChangeIt& fromParentDirectoryChange, const DirectoryChangeIt& toParentDirectoryChange, const Provider provider)
{
	CRY_ASSERT(directory);
	if (!directory->providerDetails.contains(provider))
	{
		return; // not affected
	}
	auto directoryChange = CreateDirectoryChange(directory->keyName, fromParentDirectoryChange, directory, CreateDirectoryMut(directory));
	SplitSubDirectoryChangeInDirectoryForProvider(directoryChange, fromParentDirectoryChange, toParentDirectoryChange, provider);
}

void CInflightUpdate::SplitSubDirectoryChangeInDirectoryForProvider(const DirectoryChangeIt& directoryChange, const DirectoryChangeIt& fromParentDirectoryChange, const DirectoryChangeIt& toParentDirectoryChange, const Provider provider)
{
	const auto& toDirectory = directoryChange->to;
	if (!toDirectory || !toDirectory->providerDetails.contains(provider))
	{
		return; // not affected
	}
	CRY_ASSERT(toDirectory->provider.key() == provider || toDirectory->providerDetails.size() > 1); // we should have been the provider then
	if (toDirectory->provider.key() == provider || toDirectory->providerDetails.size() == 1)
	{
		// create clone for all other providers
		auto cloneDirectoryChange = CloneDirectoryChange(directoryChange, nullptr);
		MoveDirectoryChangeCachedPathes(directoryChange, cloneDirectoryChange);
		const auto& cloneToDirectory = cloneDirectoryChange->to;
		cloneToDirectory->providerDetails.remove(provider);
		if (cloneToDirectory->providerDetails.isEmpty())
		{
			cloneDirectoryChange->to = DirectoryMutPtr();
		}
		else
		{
			UpdateDirectoryChangeProvider(cloneDirectoryChange);
		}

		// move directoryChange into toParentDirectoryChange
		QHash_RemoveExcept(toDirectory->providerDetails, provider);
		toDirectory->provider = toDirectory->providerDetails.begin();
		fromParentDirectoryChange->directoryChanges.removeOne(directoryChange);
		toParentDirectoryChange->directoryChanges << directoryChange;
		SetDirectoryChangeActiveParentPath(directoryChange, toParentDirectoryChange->keyEnginePath);

		SplitDirectoryEntriesForProvider(cloneDirectoryChange, directoryChange, provider);
		return; // splitted directory
	}
	// clone for provider itself
	auto cloneDirectoryChange = CloneDirectoryChange(directoryChange, provider);
	const DirectoryMutPtr& cloneToDirectory = cloneDirectoryChange->to;
	QHash_RemoveExcept(cloneToDirectory->providerDetails, provider);
	cloneToDirectory->provider = cloneToDirectory->providerDetails.begin();
	fromParentDirectoryChange->directoryChanges.removeOne(cloneDirectoryChange);
	toParentDirectoryChange->directoryChanges << cloneDirectoryChange;
	SetDirectoryChangeActiveParentPath(cloneDirectoryChange, toParentDirectoryChange->keyEnginePath);

	SplitDirectoryEntriesForProvider(directoryChange, cloneDirectoryChange, provider);

	toDirectory->providerDetails.remove(provider);
	UpdateDirectoryChangeProvider(directoryChange);
}

//
//  -------------------------------- Merge directory entires ----------------------------------------------
//
void CInflightUpdate::MergeDirectoryEntriesForProvider(const DirectoryChangeIt& fromDirectoryChange, const DirectoryChangeIt& toDirectoryChange, const Provider provider)
{
	foreach(auto & fromFileChange, fromDirectoryChange->fileChanges)
	{
		MergeFileChangeInDirectoryForProvider(fromFileChange, fromDirectoryChange, toDirectoryChange, provider);
	}
	foreach(auto & fromSubDirectoryChange, fromDirectoryChange->directoryChanges)
	{
		MergeSubDirectoryChangeInDirectoryForProvider(fromSubDirectoryChange, fromDirectoryChange, toDirectoryChange, provider);
	}
	const auto& fromDirectory = fromDirectoryChange.key();
	if (fromDirectory)
	{
		foreach(auto & file, fromDirectory->nameFile)
		{
			if (m_fileChanges.contains(file))
			{
				continue;
			}
			MergeFileInDirectoryForProvider(file, fromDirectoryChange, toDirectoryChange, provider);
		}
		foreach(auto & subDirectory, fromDirectory->nameDirectory)
		{
			if (m_directoryChanges.contains(subDirectory))
			{
				continue;
			}
			MergeSubDirectoryInDirectoryForProvider(subDirectory, fromDirectoryChange, toDirectoryChange, provider);
		}
	}
}

void CInflightUpdate::MergeFileInDirectoryForProvider(const FilePtr& fromFile, const DirectoryChangeIt& fromDirectoryChange, const DirectoryChangeIt& toDirectoryChange, const Provider provider)
{
	CRY_ASSERT(fromFile);
	if (!fromFile->providerDetails.contains(provider))
	{
		return; // not affected
	}
	auto fileChange = CreateFileChange(fromFile->keyName, fromDirectoryChange, fromFile, CreateFileMut(fromFile));
	return MergeFileChangeInDirectoryForProvider(fileChange, fromDirectoryChange, toDirectoryChange, provider);
}

void CInflightUpdate::MergeFileChangeInDirectoryForProvider(const FileChangeIt& fileChange, const DirectoryChangeIt& fromDirectoryChange, const DirectoryChangeIt& toDirectoryChange, const Provider provider)
{
	const FileMutPtr& toFile = fileChange->to;
	if (!toFile || !toFile->providerDetails.contains(provider))
	{
		return; // not affected
	}

	const auto keyFileName = toFile->keyName;
	const auto providerData = toFile->providerDetails[provider];
	auto keyEnginePath = GetCombinedPath(toDirectoryChange->keyEnginePath, keyFileName);

	auto onNotFound = [&]()
	{
		SplitFileChangeInDirectoryForProvider(fileChange, fromDirectoryChange, toDirectoryChange, provider);
	};
	auto onUnchangedFile = [&](const FilePtr& file, const QString& keyFilePath)
	{
		if (keyFilePath == keyEnginePath)
		{
			AddFileProviderData(file, keyFilePath, provider, providerData);
			RemoveFileChangeProvider(fileChange, provider);
			return; // file exists
		}
		SplitFileChangeInDirectoryForProvider(fileChange, fromDirectoryChange, toDirectoryChange, provider);
	};
	auto onFileChange = [&](const FileChangeIt& existingFileChange)
	{
		if (existingFileChange->keyEnginePath == keyEnginePath)
		{
			AddFileChangeProviderData(existingFileChange, provider, providerData);
			RemoveFileChangeProvider(fileChange, provider);
			return; // file exists
		}
		SplitFileChangeInDirectoryForProvider(fileChange, fromDirectoryChange, toDirectoryChange, provider);
	};
	auto onUnchangedDirectory = [&](const DirectoryPtr& directory, const QString& keyDirectoryPath)
	{
		if (keyDirectoryPath == keyEnginePath)
		{
			AddDirectoryFileProviderData(directory, keyDirectoryPath, provider, providerData);
			RemoveFileChangeProvider(fileChange, provider);
			return; // directory blocks
		}
		SplitFileChangeInDirectoryForProvider(fileChange, fromDirectoryChange, toDirectoryChange, provider);
	};
	auto onDirectoryChange = [&](const DirectoryChangeIt& directoryChange)
	{
		if (directoryChange->to && directoryChange->keyEnginePath == keyEnginePath)
		{
			AddDirectoryChangeFileProviderData(directoryChange, provider, providerData);
			RemoveFileChangeProvider(fileChange, provider);
			return; // directory blocks
		}
		SplitFileChangeInDirectoryForProvider(fileChange, fromDirectoryChange, toDirectoryChange, provider);
	};

	InspectKeyEnginePath(
	  keyEnginePath,
	  onUnchangedFile, onFileChange,
	  onUnchangedDirectory, onDirectoryChange,
	  onNotFound);
}

void CInflightUpdate::MergeSubDirectoryInDirectoryForProvider(const DirectoryPtr& directory, const DirectoryChangeIt& fromParentDirectoryChange, const DirectoryChangeIt& toParentDirectoryChange, const Provider provider)
{
	CRY_ASSERT(directory);
	if (!directory->providerDetails.contains(provider))
	{
		return; // not affected
	}
	auto directoryChange = CreateDirectoryChange(directory->keyName, fromParentDirectoryChange, directory, CreateDirectoryMut(directory));
	MergeSubDirectoryChangeInDirectoryForProvider(directoryChange, fromParentDirectoryChange, toParentDirectoryChange, provider);
}

void CInflightUpdate::MergeSubDirectoryChangeInDirectoryForProvider(const DirectoryChangeIt& subDirectoryChange, const DirectoryChangeIt& fromParentDirectoryChange, const DirectoryChangeIt& toParentDirectoryChange, const Provider provider)
{
	const auto& toDirectory = subDirectoryChange->to;
	if (!toDirectory || !toDirectory->providerDetails.contains(provider))
	{
		return; // not affected
	}

	const auto keyName = GetFilenameFromPath(subDirectoryChange->keyEnginePath);
	const auto providerData = toDirectory->providerDetails[provider];
	auto keyEnginePath = GetCombinedPath(toParentDirectoryChange->keyEnginePath, keyName);

	auto onNotFound = [&]()
	{
		SplitSubDirectoryChangeInDirectoryForProvider(subDirectoryChange, fromParentDirectoryChange, toParentDirectoryChange, provider);
	};
	auto onUnchangedFile = [&](const FilePtr& file, const QString& keyFilePath)
	{
		if (keyFilePath == keyEnginePath)
		{
			AddFileDirectoryProviderData(file, keyFilePath, provider, providerData);
			RemoveDirectoryChangeProvider(subDirectoryChange, provider);
			return; // file exists
		}
		SplitSubDirectoryChangeInDirectoryForProvider(subDirectoryChange, fromParentDirectoryChange, toParentDirectoryChange, provider);
	};
	auto onFileChange = [&](const FileChangeIt& existingFileChange)
	{
		if (existingFileChange->keyEnginePath == keyEnginePath)
		{
			AddFileChangeDirectoryProviderData(existingFileChange, provider, providerData);
			RemoveDirectoryChangeProvider(subDirectoryChange, provider);
			return; // file exists
		}
		SplitSubDirectoryChangeInDirectoryForProvider(subDirectoryChange, fromParentDirectoryChange, toParentDirectoryChange, provider);
	};
	auto onUnchangedDirectory = [&](const DirectoryPtr& directory, const QString& keyDirectoryPath)
	{
		if (keyDirectoryPath == keyEnginePath)
		{
			auto directoryChange = BuildDirectoryChange(keyDirectoryPath, CreateDirectoryMut(directory), directory->provider.key());
			AddDirectoryChangeProviderData(directoryChange, provider, providerData);
			MergeDirectoryEntriesForProvider(subDirectoryChange, directoryChange, provider);
			RemoveDirectoryChangeProvider(subDirectoryChange, provider);
			return; // directory blocks
		}
		SplitSubDirectoryChangeInDirectoryForProvider(subDirectoryChange, fromParentDirectoryChange, toParentDirectoryChange, provider);
	};
	auto onDirectoryChange = [&](const DirectoryChangeIt& directoryChange)
	{
		if (directoryChange->to && directoryChange->keyEnginePath == keyEnginePath)
		{
			AddDirectoryChangeProviderData(directoryChange, provider, providerData);
			MergeDirectoryEntriesForProvider(subDirectoryChange, directoryChange, provider);
			RemoveDirectoryChangeProvider(subDirectoryChange, provider);
			return; // directory blocks
		}
		SplitSubDirectoryChangeInDirectoryForProvider(subDirectoryChange, fromParentDirectoryChange, toParentDirectoryChange, provider);
	};

	InspectKeyEnginePath(
	  keyEnginePath,
	  onUnchangedFile, onFileChange,
	  onUnchangedDirectory, onDirectoryChange,
	  onNotFound);
}

void CInflightUpdate::UpdateFileChangeProviderFileName(const FileChangeIt& fileChange, const Provider provider, const SAbsolutePath& fileName)
{
	const auto& toFile = fileChange->to;
	if (Q_UNLIKELY(!toFile))
	{
		CRY_ASSERT(false); // was checked by caller
		return;            // nothing to rename
	}
	if (Q_UNLIKELY(!toFile->providerDetails.contains(provider)))
	{
		CRY_ASSERT(false); // was checked by caller
		return;            // not provided
	}
	toFile->providerDetails[provider].fullName = fileName.full;
	UpdateFileChangeProvider(fileChange);

	if (Q_UNLIKELY(!fileChange->to))
	{
		CRY_ASSERT(false); // we have at least one provider + it was not added => so it should not remove
		return;
	}
	if (toFile->provider.key() != provider)
	{
		return; // not current provider
	}
	const auto& keyName = fileName.key;
	if (toFile->keyName == keyName)
	{
		return; // key name did not change
	}

	auto keyDirectoryPath = GetDirectoryPath(fileChange->keyEnginePath);
	auto keyEnginePath = GetCombinedPath(keyDirectoryPath, keyName);

	auto onNotFound = [&]()
	{
		SetFileChangeActiveKeyEnginePath(fileChange, keyEnginePath, keyName);
	};
	auto onUnchangedFile = [&](const FilePtr& file, const QString& keyFilePath)
	{
		if (keyFilePath == keyEnginePath)
		{
			MergeFileWithFileChangeProvider(file, keyFilePath, fileChange, provider, fileName);
			return; // file exists
		}
		SetFileChangeActiveKeyEnginePath(fileChange, keyEnginePath, keyName);
	};
	auto onFileChange = [&](const FileChangeIt& existingFileChange)
	{
		if (existingFileChange->keyEnginePath == keyEnginePath)
		{
			MergeFileChangeProvider(existingFileChange, fileChange, provider, fileName);
			return; // file exists
		}
		SetFileChangeActiveKeyEnginePath(fileChange, keyEnginePath, keyName);
	};
	auto onUnchangedDirectory = [&](const DirectoryPtr& directory, const QString& keyDirectoryPath)
	{
		if (keyDirectoryPath == keyEnginePath)
		{
			MergeDirectoryWithFileChangeProvider(directory, keyDirectoryPath, fileChange, provider);
			return; // directory blocks
		}
		SetFileChangeActiveKeyEnginePath(fileChange, keyEnginePath, keyName);
	};
	auto onDirectoryChange = [&](const DirectoryChangeIt& directoryChange)
	{
		if (directoryChange->to && directoryChange->keyEnginePath == keyEnginePath)
		{
			MergeDirectoryChangeWithFileChangeProvider(directoryChange, fileChange, provider);
			return; // directory blocks
		}
		SetFileChangeActiveKeyEnginePath(fileChange, keyEnginePath, keyName);
	};

	InspectKeyEnginePath(
	  keyEnginePath,
	  onUnchangedFile, onFileChange,
	  onUnchangedDirectory, onDirectoryChange,
	  onNotFound);
}

void CInflightUpdate::UpdateDirectoryChangeProviderName(const DirectoryChangeIt& directoryChange, const Provider provider, const SAbsolutePath& name)
{
	const auto& toDirectory = directoryChange->to;
	if (Q_UNLIKELY(!toDirectory))
	{
		CRY_ASSERT(false); // was checked by caller
		return;            // nothing to rename
	}
	if (Q_UNLIKELY(!toDirectory->providerDetails.contains(provider)))
	{
		CRY_ASSERT(false); // was checked by caller
		return;            // not provided
	}
	toDirectory->providerDetails[provider].fullName = name.full;
	UpdateDirectoryChangeProvider(directoryChange);

	if (Q_UNLIKELY(!directoryChange->to))
	{
		CRY_ASSERT(false); // we have at least one provider + it was not added => so it should not remove
		return;
	}
	if (toDirectory->provider.key() != provider)
	{
		return; // not current provider
	}
	auto keyName = name.key;
	if (toDirectory->keyName == keyName)
	{
		return; // key name did not change
	}

	auto keyParentDirectoryPath = GetDirectoryPath(directoryChange->keyEnginePath);
	auto keyEnginePath = GetCombinedPath(keyParentDirectoryPath, keyName);

	auto onNotFound = [&]()
	{
		SetDirectoryChangeActiveKeyEnginePath(directoryChange, keyEnginePath, keyName);
		UpdateDirectoryChangeEntriesForPath(directoryChange);
	};
	auto onUnchangedFile = [&](const FilePtr& file, const QString& keyFilePath)
	{
		if (keyFilePath == keyEnginePath)
		{
			MergeFileWithDirectoryChangeProvider(file, keyFilePath, directoryChange, provider);
			return; // file exists
		}
		SetDirectoryChangeActiveKeyEnginePath(directoryChange, keyEnginePath, keyName);
		UpdateDirectoryChangeEntriesForPath(directoryChange);
	};
	auto onFileChange = [&](const FileChangeIt& fileChange)
	{
		if (fileChange->to && fileChange->keyEnginePath == keyEnginePath)
		{
			MergeFileChangeWithDirectoryChangeProvider(fileChange, directoryChange, provider);
			return; // file exists
		}
		SetDirectoryChangeActiveKeyEnginePath(directoryChange, keyEnginePath, keyName);
		UpdateDirectoryChangeEntriesForPath(directoryChange);
	};
	auto onUnchangedDirectory = [&](const DirectoryPtr& directory, const QString& keyDirectoryPath)
	{
		if (keyDirectoryPath == keyEnginePath)
		{
			MergeDirectoryWithDirectoryChangeProvider(directory, keyDirectoryPath, directoryChange, provider, name);
			return; // directory blocks
		}
		SetDirectoryChangeActiveKeyEnginePath(directoryChange, keyEnginePath, keyName);
		UpdateDirectoryChangeEntriesForPath(directoryChange);
	};
	auto onDirectoryChange = [&](const DirectoryChangeIt& existingDirectoryChange)
	{
		if (existingDirectoryChange->keyEnginePath == keyEnginePath)
		{
			MergeDirectoryChangeProvider(existingDirectoryChange, directoryChange, provider, name);
			return; // directory blocks
		}
		SetDirectoryChangeActiveKeyEnginePath(existingDirectoryChange, keyEnginePath, keyName);
		UpdateDirectoryChangeEntriesForPath(directoryChange);
	};

	InspectKeyEnginePath(
	  keyEnginePath,
	  onUnchangedFile, onFileChange,
	  onUnchangedDirectory, onDirectoryChange,
	  onNotFound);
}

void CInflightUpdate::MergeFileWithFileChangeProvider(const FilePtr& file, const QString& keyFilePath, const FileChangeIt& otherFileChange, const Provider otherProvider, const SAbsolutePath& fileName)
{
	CRY_ASSERT(file);
	auto fileChange = BuildFileChange(keyFilePath, CreateFileMut(file), nullptr);
	MergeFileChangeProvider(fileChange, otherFileChange, otherProvider, fileName);
}

void CInflightUpdate::MergeFileChangeProvider(const FileChangeIt& fileChange, const FileChangeIt& otherFileChange, const Provider otherProvider, const SAbsolutePath& fileName)
{
	auto otherToFile = otherFileChange->to;
	CRY_ASSERT(otherToFile);

	auto toFile = fileChange->to;
	if (!toFile)
	{
		fileChange->to = otherToFile; // file was removed revive it
		otherToFile->keyName = fileName.key;
		otherToFile->extension = GetFileExtensionFromFilename(fileName.key);
		otherToFile->enginePath.key = fileChange->keyEnginePath;
		if (!otherFileChange.key())
		{
			MoveFileChangeCachedPathes(otherFileChange, fileChange);
		}
	}
	else
	{
		toFile->providerDetails[otherProvider] = otherToFile->providerDetails[otherProvider];
	}
	RemoveFileChange(otherFileChange);

	UpdateFileChangeProvider(fileChange);
}

void CInflightUpdate::MergeDirectoryWithFileChangeProvider(const DirectoryPtr& directory, const QString& keyDirectoryPath, const FileChangeIt& otherFileChange, const Provider otherProvider)
{
	CRY_ASSERT(directory);
	auto directoryChange = BuildDirectoryChange(keyDirectoryPath, CreateDirectoryMut(directory), nullptr);
	MergeDirectoryChangeWithFileChangeProvider(directoryChange, otherFileChange, otherProvider);
}

void CInflightUpdate::MergeDirectoryChangeWithFileChangeProvider(const DirectoryChangeIt& directoryChange, const FileChangeIt& otherFileChange, const Provider otherProvider)
{
	auto otherToFile = otherFileChange->to;
	CRY_ASSERT(otherToFile);

	auto toDirectory = directoryChange->to;
	CRY_ASSERT(toDirectory);

	auto providerDetails = toDirectory->providerDetails;
	providerDetails[otherProvider] = otherToFile->providerDetails[otherProvider];
	auto provider = SelectProvider(providerDetails);

	if (provider.key() == otherProvider)
	{
		otherFileChange->keyEnginePath = directoryChange->keyEnginePath;
		otherToFile->enginePath.key = directoryChange->keyEnginePath;
		otherToFile->providerDetails = providerDetails;
		otherToFile->provider = provider;
		RemoveDirectoryChange(directoryChange);
		return; // directory is turned into file
	}

	toDirectory->providerDetails = providerDetails;
	toDirectory->provider = provider;

	RemoveFileChange(otherFileChange);
}

void CInflightUpdate::MergeFileWithDirectoryChangeProvider(const FilePtr& file, const QString& keyFilePath, const DirectoryChangeIt& otherDirectoryChange, const Provider otherProvider)
{
	CRY_ASSERT(file);
	auto fileChange = BuildFileChange(keyFilePath, CreateFileMut(file), nullptr);
	MergeFileChangeWithDirectoryChangeProvider(fileChange, otherDirectoryChange, otherProvider);
}

void CInflightUpdate::MergeFileChangeWithDirectoryChangeProvider(const FileChangeIt& fileChange, const DirectoryChangeIt& otherDirectoryChange, const Provider otherProvider)
{
	auto otherToDirectory = otherDirectoryChange->to;
	CRY_ASSERT(otherToDirectory);

	auto toFile = fileChange->to;
	CRY_ASSERT(toFile);

	auto providerDetails = toFile->providerDetails;
	providerDetails[otherProvider] = otherToDirectory->providerDetails[otherProvider];
	auto provider = SelectProvider(providerDetails);

	if (provider.key() == otherProvider)
	{
		otherDirectoryChange->keyEnginePath = fileChange->keyEnginePath;
		otherToDirectory->providerDetails = providerDetails;
		otherToDirectory->provider = provider;
		RemoveFileChange(fileChange);

		UpdateDirectoryChangeEntriesForPath(otherDirectoryChange);
		return; // file is turned into directory
	}

	toFile->providerDetails = providerDetails;
	toFile->provider = provider;

	RemoveDirectoryChange(otherDirectoryChange);
}

void CInflightUpdate::MergeDirectoryWithDirectoryChangeProvider(const DirectoryPtr& directory, const QString& keyDirectoryPath, const DirectoryChangeIt& otherDirectoryChange, const Provider otherProvider, const SAbsolutePath& name)
{
	CRY_ASSERT(directory);
	auto directoryChange = BuildDirectoryChange(keyDirectoryPath, CreateDirectoryMut(directory), nullptr);
	MergeDirectoryChangeProvider(directoryChange, otherDirectoryChange, otherProvider, name);
}

void CInflightUpdate::MergeDirectoryChangeProvider(const DirectoryChangeIt& directoryChange, const DirectoryChangeIt& otherDirectoryChange, const Provider otherProvider, const SAbsolutePath& name)
{
	auto otherToDirectory = otherDirectoryChange->to;
	CRY_ASSERT(otherToDirectory);

	auto toDirectory = directoryChange->to;
	if (!toDirectory)
	{
		directoryChange->to = otherToDirectory; // directory was removed - revive it
		otherToDirectory->keyName = name.key;
	}
	else
	{
		toDirectory->providerDetails[otherProvider] = otherToDirectory->providerDetails[otherProvider];
		UpdateDirectoryChangeProvider(directoryChange);
		CRY_ASSERT(directoryChange->to);
	}
	MergeDirectoryEntriesForProvider(otherDirectoryChange, directoryChange, otherProvider);
	RemoveDirectoryChange(otherDirectoryChange);
}

void CInflightUpdate::UpdateDirectoryChangeEntriesForPath(const DirectoryChangeIt& directoryChange)
{
	InflightDirectoryChange& inflight = directoryChange.value();
	auto keyEnginePath = inflight.keyEnginePath;
	foreach(auto & fileChange, inflight.fileChanges)
	{
		SetFileChangeActiveDirectoryPath(fileChange, keyEnginePath);
	}
	foreach(auto & subDirectoryChange, inflight.directoryChanges)
	{
		SetDirectoryChangeActiveParentPath(subDirectoryChange, keyEnginePath);
		UpdateDirectoryChangeEntriesForPath(subDirectoryChange);
	}
	// All non changed files & directories are updated on Snapshot
}

//
//  -------------------------------- Setters for Directorychange & FileChange ----------------------------------------------
//
void CInflightUpdate::SetFileChangeActiveDirectoryPath(const FileChangeIt& fileChange, const QString& keyDirectoryPath)
{
	auto keyFileName = GetFilenameFromPath(fileChange->keyEnginePath);
	auto keyEnginePath = GetCombinedPath(keyDirectoryPath, keyFileName);
	SetFileChangeActiveKeyEnginePath(fileChange, keyEnginePath, keyFileName);
}

void CInflightUpdate::SetDirectoryChangeActiveParentPath(const DirectoryChangeIt& directoryChange, const QString& keyParentPath)
{
	auto keyDirectoryName = GetFilenameFromPath(directoryChange->keyEnginePath);
	auto keyEnginePath = GetCombinedPath(keyParentPath, keyDirectoryName);
	SetDirectoryChangeActiveKeyEnginePath(directoryChange, keyEnginePath, keyDirectoryName);
}

void CInflightUpdate::SetFileChangeActiveKeyEnginePath(const FileChangeIt& fileChange, const QString& keyEnginePath, const QString& keyName)
{
	auto toFile = fileChange->to;
	toFile->keyName = keyName;
	toFile->extension = GetFileExtensionFromFilename(keyName);
	toFile->enginePath.key = keyEnginePath;
	fileChange->keyEnginePath = keyEnginePath;
	AddPathFileChangeCacheEntry(keyEnginePath, fileChange);
}

void CInflightUpdate::SetDirectoryChangeActiveKeyEnginePath(const DirectoryChangeIt& directoryChange, const QString& keyEnginePath, const QString& keyName)
{
	auto toDirectory = directoryChange->to;
	toDirectory->keyName = keyName;
	directoryChange->keyEnginePath = keyEnginePath;
	AddPathDirectoryChangeCacheEntry(keyEnginePath, directoryChange);
}

//
//  -------------------------------- Path Caches ----------------------------------------------
//
void CInflightUpdate::AddPathFileChangeCacheEntry(const QString& cachePath, const FileChangeIt& fileChange)
{
	auto it = m_pathFileChange.find(cachePath);
	if (it != m_pathFileChange.end())
	{
		auto oldFileChange = it.value();
		oldFileChange->cachedPathes.remove(cachePath);
	}
	fileChange->cachedPathes << cachePath;
	m_pathFileChange[cachePath] = fileChange;
}

void CInflightUpdate::AddPathDirectoryChangeCacheEntry(const QString& cachePath, const DirectoryChangeIt& directoryChange)
{
	auto it = m_pathDirectoryChange.find(cachePath);
	if (it != m_pathDirectoryChange.end())
	{
		auto oldFileChange = it.value();
		oldFileChange->cachedPathes.remove(cachePath);
	}
	directoryChange->cachedPathes << cachePath;
	m_pathDirectoryChange[cachePath] = directoryChange;
}

void CInflightUpdate::MoveFileChangeCachedPathes(const FileChangeIt& fromFileChange, const FileChangeIt& toFileChange)
{
	foreach(const auto & cachePath, fromFileChange->cachedPathes)
	{
		m_pathFileChange[cachePath] = toFileChange;
	}
	toFileChange->cachedPathes |= fromFileChange->cachedPathes;
	fromFileChange->cachedPathes.clear();
}

void CInflightUpdate::MoveDirectoryChangeCachedPathes(const DirectoryChangeIt& fromDirectoryChange, const DirectoryChangeIt& toDirectoryChange)
{
	foreach(const auto & cachePath, fromDirectoryChange->cachedPathes)
	{
		m_pathDirectoryChange[cachePath] = toDirectoryChange;
	}
	toDirectoryChange->cachedPathes |= fromDirectoryChange->cachedPathes;
	fromDirectoryChange->cachedPathes.clear();
}

//
//  -------------------------------- Generic Tools ----------------------------------------------
//
QSet<QString> CInflightUpdate::CreateTokens(const QString& name)
{
	static auto regexp = QRegularExpression("[\\s\\._-]+");
	auto splits = name.split(regexp, QString::SkipEmptyParts);
	return splits.toSet();
}

ProviderIt CInflightUpdate::SelectProvider(const ProviderDetails& providerDetails)
{
	if (providerDetails.isEmpty())
	{
		return providerDetails.cend();
	}
	if (providerDetails.size() == 1)
	{
		return providerDetails.cbegin(); // only one option
	}
	auto it = providerDetails.find(nullptr);
	if (it != providerDetails.cend())
	{
		return it; // filesystem always wins for editor
	}
	// TODO: do more sophisticated stuff (mods are prefered to game paks)
	// see ICryPak implementation
	return providerDetails.cbegin();
}

SProviderData CInflightUpdate::ProviderDataFromFileInfo(const SFileInfoInternal& info)
{
	SProviderData providerData;
	providerData.isFile = true;
	providerData.fullName = info.fullName;
	providerData.size = info.size;
	providerData.lastModified = info.lastModified;
	return providerData;
}

SProviderData CInflightUpdate::ProviderDataFromDirectoryInfo(const SDirectoryInfoInternal& info)
{
	SProviderData providerData;
	providerData.isFile = false;
	providerData.fullName = info.fullName;
	return providerData;
}

SProviderData CInflightUpdate::ProviderDataFromDirectoryName(const SAbsolutePath& directoryName)
{
	SProviderData providerData;
	providerData.isFile = false;
	providerData.fullName = directoryName.full;
	return providerData;
}

//
//  -------------------------------- Factory methods ----------------------------------------------
//
CInflightUpdate::SnapshotMutPtr CInflightUpdate::CreateSnapshotMut(const CSnapshot&)
{
	// there is nothing we can reuse
	return std::make_shared<CSnapshot>();
}

CInflightUpdate::DirectoryMutPtr CInflightUpdate::CreateDirectoryMut(const DirectoryPtr& directory)
{
	if (directory)
	{
		return CreateDirectoryMut(*std::static_pointer_cast<const SIndexedDirectory>(directory)); // copy
	}
	return CreateDirectoryMut(); // create new object
}

CInflightUpdate::DirectoryMutPtr CInflightUpdate::CreateDirectoryMut(const SIndexedDirectory& directory)
{
	DirectoryMutPtr result = std::make_shared<SIndexedDirectory>(directory);
	// hierarchy hashes are rebuild when a snapshot is created
	result->nameDirectory.clear();
	result->nameFile.clear();
	result->tokenDirectories.clear();
	result->tokenFiles.clear();
	result->extensionFiles.clear();
	return result;
}

CInflightUpdate::FileMutPtr CInflightUpdate::CreateFileMut(const FilePtr& file)
{
	if (file)
	{
		return CreateFileMut(*file); // copy
	}
	return CreateFileMut(); // create new object
}

CInflightUpdate::FileMutPtr CInflightUpdate::CreateFileMut(const SFile& file)
{
	FileMutPtr result = std::make_shared<SFile>(file);
	return result;
}

//
//  -------------------------------- Highlevel setters for DirectoryMut & FileMut ----------------------------------------
//
void CInflightUpdate::UpdateDirectoryMut(const DirectoryMutPtr& directory, const SEnginePath& parentEnginePath)
{
	SetDirectoryMutParentPath(directory, parentEnginePath);
	directory->tokens = CreateTokens(directory->keyName);
}

void CInflightUpdate::SetDirectoryMutParentPath(const DirectoryMutPtr& directory, const SEnginePath& parentEnginePath)
{
	directory->parentEnginePath = parentEnginePath;
	directory->enginePath.key = GetCombinedPath(parentEnginePath.key, directory->keyName);
	directory->enginePath.full = GetCombinedPath(parentEnginePath.full, directory->provider->fullName);
}

void CInflightUpdate::UpdateFileMut(const FileMutPtr& file, const SEnginePath& parentEnginePath, const CFileTypeStore& fileTypeStore)
{
	SetFileMutDirectoryEnginePath(file, parentEnginePath);
	auto basename = GetBasenameFromFilename(file->keyName);
	file->tokens = CreateTokens(basename);
	file->type = fileTypeStore.GetType(file->directoryEnginePath.key, file->extension);
}

void CInflightUpdate::SetFileMutDirectoryEnginePath(const FileMutPtr& file, const SEnginePath& directoryEnginePath)
{
	file->directoryEnginePath = directoryEnginePath;
	file->enginePath.key = GetCombinedPath(directoryEnginePath.key, file->keyName);
	file->enginePath.full = GetCombinedPath(directoryEnginePath.full, file->provider->fullName);
}

void CInflightUpdate::AddDirectoryToSnapshotMut(const DirectoryPtr& directory, SnapshotMutPtr& snapshot)
{
	snapshot->m_pathToDirectory[directory->enginePath.key] = directory;
	foreach(auto & token, directory->tokens)
	{
		snapshot->m_tokenToDirectories.insert(token, directory);
	}
}

void CInflightUpdate::AddDirectoryToSnapshotMutRecursive(const DirectoryPtr& directory, SnapshotMutPtr& snapshot)
{
	AddDirectoryToSnapshotMut(directory, snapshot);
	foreach(const auto & subDirectory, directory->nameDirectory)
	{
		AddDirectoryToSnapshotMutRecursive(subDirectory, snapshot);
	}
}

void CInflightUpdate::AddDirectoryToParentDirectoryMut(const DirectoryPtr& directory, const DirectoryMutPtr& parent)
{
	parent->nameDirectory[directory->keyName] = directory;
	foreach(auto & token, directory->tokens)
	{
		parent->tokenDirectories.insert(token, directory);
	}
}

void CInflightUpdate::AddFileToDirectoryMut(const FilePtr& file, const DirectoryMutPtr& directory)
{
	directory->nameFile[file->keyName] = file;
	directory->extensionFiles.insert(file->extension, file);
	foreach(auto & token, file->tokens)
	{
		directory->tokenFiles.insert(token, file);
	}
}

} // namespace Internal
} // namespace FileSystem

