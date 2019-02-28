// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "FileSystem_Internal_IndexedDirectory.h"
#include "FileSystem_Internal_SnapshotUpdate.h"
#include "FileSystem_Internal_Data.h"

#include "FileSystem/FileSystem_Snapshot.h"
#include "FileSystem/FileSystem_SubTreeMonitor.h"
#include "FileSystem/FileTypeStore.h"

#include <QMultiHash>
#include <QMap>

#include <memory>

namespace FileSystem
{
namespace Internal
{

/// \brief apply updates to existing snapshot and create a new snapshot
class CInflightUpdate
{
public:
	CInflightUpdate(const SnapshotPtr&, const CFileTypeStore& = CFileTypeStore());

	/// \brief Create new Snapshot report the difference
	/// after this the update is ready for more changes
	SSnapshotUpdate CreateSnapshot() const;

	/// clear all updates and set base snapshot
	void Reset(const SnapshotPtr&);

	/// \brief registers new file types
	void RegisterFileTypes(const QVector<const SFileType*>& fileTypes);

	// monitor api
	void RemovePath(const QString& keyEnginePath);
	void RenamePath(const QString& keyEnginePath, const SAbsolutePath& toName);
	void AddDirectory(const QString& keyEnginePath, const SDirectoryInfoInternal&);
	void AddFile(const QString& keyEnginePath, const SFileInfoInternal&);
	// engine directories are only pathes => no reason to update them
	void UpdateFile(const QString& keyEnginePath, const SFileInfoInternal&);

	// scanner api
	/// \brief replace the content of a directory @enginePath@ with the scan results
	///
	/// internally this calculates which files/folders are removed, added or updated
	/// (to recognize rename is not possible this way)
	void ApplyDirectoryScanResult(const QString& keyEnginePath, const SDirectoryScanResult&);

	// archive api
	/// \brief add or update contents for an existing archive @enginePath@
	void SetArchiveContent(const QString& keyEnginePath, const SArchiveContentInternal&);

	/// \brief removes archive contents
	void CleanArchiveContent(const QString& keyEnginePath);

private:
	typedef RawFilePtr Provider; // same as in SFile / SDirectory

	// types to track file changes
	typedef std::shared_ptr<SFile> FileMutPtr;
	struct InflightFileChange
	{
		QString       keyEnginePath; ///< the latest full engine path
		QSet<QString> cachedPathes;  ///< all pathes this change can be found with
		FileMutPtr    to;            ///< nullptr if removed
	};
	typedef QMultiHash<FilePtr, InflightFileChange> FileChanges;
	typedef FileChanges::iterator                   FileChangeIt;

	typedef QHash<QString, FileChangeIt>            PathFileChange; // cache to speed up path lookups (path may be any known path for the file)
	typedef PathFileChange::iterator                PathFileChangeIt;

	// types to track directory changes
	typedef std::shared_ptr<SIndexedDirectory>                DirectoryMutPtr;
	struct InflightDirectoryChange;
	typedef QMultiHash<DirectoryPtr, InflightDirectoryChange> DirectoryChanges;
	typedef DirectoryChanges::iterator                        DirectoryChangeIt;
	struct InflightDirectoryChange
	{
		//InflightChangeFlags flags; // OPTIONAL: optimize index generation with flags
		QString                    keyEnginePath; ///< the latest full engine path
		QSet<QString>              cachedPathes;  ///< all pathes this change can be found with
		DirectoryMutPtr            to;            ///< nullptr if removed
		QVector<FileChangeIt>      fileChanges;
		QVector<DirectoryChangeIt> directoryChanges;
	};

	typedef QHash<QString, DirectoryChangeIt> PathDirectoryChange; // cache to speed up path lookups (path may be any known path for the directory)
	typedef PathDirectoryChange::iterator     PathDirectoryChangeIt;

	// types to generate snapshots
	typedef std::shared_ptr<CSnapshot> SnapshotMutPtr;
	struct SnapshotUpdateState
	{
		SnapshotMutPtr toSnapshot;
		QSet<QString>  fromSkipPathes;
	};

private:
	// Snapshot creation
	SSnapshotDirectoryUpdate CreateSnapshotDirectoryUpdateFromChange(SnapshotUpdateState&, const DirectoryChangeIt&, const SEnginePath& parentEnginePath) const;
	SSnapshotDirectoryUpdate CreateSnapshotDirectoryUpdateUnchanged(SnapshotUpdateState&, const DirectoryPtr& from, const DirectoryMutPtr& to) const;
	SSnapshotFileUpdate      CreateSnapshotFileUpdateFromChange(const FileChangeIt&, const SEnginePath& parentEnginePath) const;
	SSnapshotFileUpdate      CreateSnapshotFileUpdateUnchanged(const FilePtr& from, const FileMutPtr& to) const;
	void                     AddSnapshotDirectoryChildrenUpdates(SnapshotUpdateState&, const DirectoryChangeIt&, SSnapshotDirectoryUpdate&) const;
	void                     AddSnapshotDirectoryFilesUpdates(SnapshotUpdateState&, const DirectoryChangeIt&, SSnapshotDirectoryUpdate&) const;
	void                     AddSnapshotDirectoryChildrenUnchanged(SnapshotUpdateState&, const DirectoryMutPtr&, SSnapshotDirectoryUpdate&) const;
	void                     AddSnapshotDirectoryFilesUnchanged(SnapshotUpdateState&, const DirectoryMutPtr&, SSnapshotDirectoryUpdate&) const;
	void                     AddSnapshotDirectoryArchivesUnchanged(const DirectoryChangeIt&) const;

	// Tool Getter
	/// \brief inspect all states the @enginePath@ might have
	template<typename OnUnchangedFile, typename OnFileChange, typename OnUnchangedDirectory, typename OnDirectoryChange, typename OnNotFound>
	void InspectKeyEnginePath(const QString& keyEnginePath, const OnUnchangedFile&, const OnFileChange&, const OnUnchangedDirectory&, const OnDirectoryChange&, const OnNotFound&) const;

	/// \return the lastest engine path after all renamed parent directories are applied
	QString GetLatestKeyEnginePath(const QString& keyEnginePath) const;

	/// \return file change iterator for a given @enginePath@ (points to m_fileChanges.end() if @enginePath@ was not found)
	FileChangeIt      FindFileChangeByKeyEnginePath(const QString& keyEnginePath) const;

	DirectoryChangeIt FindDirectoryChangeByEnginePath(const QString& enginePath) const;

	// generalized updates to providers
	void RemoveProviderPath(const Provider, const QString& keyEnginePath);
	void RenameProviderPath(const Provider, const QString& keyEnginePath, const SAbsolutePath& toName);
	void AddProviderDirectoryWithInfo(const Provider, const QString& keyEnginePath, const SDirectoryInfoInternal&);
	void AddProviderDirectoryWithName(const Provider, const QString& keyEnginePath, const SAbsolutePath& name);
	void AddProviderFile(const Provider, const QString& keyEnginePath, const SFileInfoInternal&);

	// archive contents
	void RemoveArchiveContent(const FileMutPtr& archive, const QString& keyEnginePath);
	void RemoveArchiveContentExcept(const FileMutPtr& archive, const QString& keyEnginePath, const QSet<QString>& filePathes, const QSet<QString>& directoryPathes);
	void UpdateArchiveContent(const DirectoryChangeIt& parent, const RawFilePtr fromArchive, const RawFilePtr toArchive);//Disabled temporarily
	/*void UpdateFileEnginePathProvider(const DirectoryChangeIt& parent, const QString& keyArchivePath, const Provider from, const Provider to);
	void UpdateDirectoryEnginePathProvider(const DirectoryChangeIt& parent, const QString& keyParentPath, const Provider from, const Provider to);*/

	// remove provider implementations
	void RemoveFileProvider(const FilePtr&, const QString& filePath, const Provider);
	void RemoveFileChangeProvider(const FileChangeIt&, const Provider);
	void RemoveDirectoryProvider(const DirectoryPtr&, const QString& keyDirectoryPath, const Provider);
	void RemoveDirectoryChangeProvider(const DirectoryChangeIt&, const Provider);

	// rename provider implementations
	void RenameFileProvider(const FilePtr&, const QString& keyEnginePath, const Provider, const SAbsolutePath& toFilename);
	void RenameFileChangeProvider(const FileChangeIt&, const Provider, const SAbsolutePath& toFilename);
	void RenameDirectoryProvider(const DirectoryPtr&, const QString& keyEnginePath, const Provider, const SAbsolutePath& toName);
	void RenameDirectoryChangeProvider(const DirectoryChangeIt&, const Provider, const SAbsolutePath& toName);

	// add provider directory implementation
	void CreateDirectoryWithName(const QString& keyEnginePath, const Provider, const SAbsolutePath& name, const ProviderDetails& = ProviderDetails());
	void CreateDirectoryWithProvider(const QString& keyEnginePath, const Provider, const SProviderData&, const ProviderDetails& = ProviderDetails());
	void CreateDirectoryWithProviderAndKeyName(const QString& keyEnginePath, const Provider, const SProviderData&, const QString& keyName, const ProviderDetails& = ProviderDetails());
	void AddFileDirectoryProviderData(const FilePtr&, const QString& keyFilePath, const Provider, const SProviderData&);
	void AddFileDirectoryProviderName(const FilePtr&, const QString& keyFilePath, const Provider, const SAbsolutePath& name);
	void AddFileChangeDirectoryProviderData(const FileChangeIt&, const Provider, const SProviderData&);
	void AddFileChangeDirectoryProviderName(const FileChangeIt&, const Provider, const SAbsolutePath& name);
	void AddDirectoryProvider(const DirectoryPtr&, const QString& directoryPath, const Provider, const SAbsolutePath& name);
	void AddDirectoryChangeProviderData(const DirectoryChangeIt&, const Provider, const SProviderData&);
	void AddDirectoryChangeProviderName(const DirectoryChangeIt&, const Provider, const SAbsolutePath& name);

	// add provider file implementation
	void CreateFileWithInfo(const QString& keyEnginePath, const Provider, const SFileInfoInternal&, const ProviderDetails& = ProviderDetails());
	void CreateFileWithProvider(const QString& keyEnginePath, const Provider, const SProviderData&, const ProviderDetails& = ProviderDetails());
	void CreateFileWithProviderKeyNameAndExtension(const QString& keyEnginePath, const Provider, const SProviderData&, const QString& keyName, const QString& extension, const ProviderDetails& = ProviderDetails());
	void AddFileProviderData(const FilePtr&, const QString& keyFilePath, const Provider, const SProviderData&);
	void AddFileProviderInfo(const FilePtr&, const QString& keyFilePath, const Provider, const SFileInfoInternal&);
	void AddFileChangeProviderData(const FileChangeIt&, const Provider, const SProviderData&);
	void AddFileChangeProviderInfo(const FileChangeIt&, const Provider, const SFileInfoInternal&);

	void AddDirectoryFileProviderData(const DirectoryPtr&, const QString& keyDirectoryPath, const Provider, const SProviderData&);
	void AddDirectoryFileProviderInfo(const DirectoryPtr&, const QString& keyDirectoryPath, const Provider, const SFileInfoInternal&);
	void AddDirectoryChangeFileProviderData(const DirectoryChangeIt&, const Provider, const SProviderData&);
	void AddDirectoryChangeFileProviderInfo(const DirectoryChangeIt&, const Provider, const SFileInfoInternal&);

	// change builders
	/// \brief creates directory clones from m_base between root and @enginePath@ and creates missing directories with @provider@
	DirectoryChangeIt BuildDirectoryChangeFromRoot(const QString& keyEnginePath, const Provider);
	DirectoryChangeIt FindDirectoryAndCreateDirectoryChangeIn(const DirectoryChangeIt& parent, const QString& keyParentPath);
	FileChangeIt      FindFileAndCreateFileChangeIn(const DirectoryChangeIt& parent, const QString& keyFileEnginePath);
	DirectoryChangeIt BuildDirectoryChange(const QString& keyEnginePath, const DirectoryMutPtr&, const Provider);
	FileChangeIt      BuildFileChange(const QString& keyEnginePath, const FileMutPtr&, const Provider);
	DirectoryChangeIt CreateDirectoryChange(const QString& keyDirectoryName, const DirectoryChangeIt& parent, const DirectoryPtr& from, const DirectoryMutPtr& to);
	FileChangeIt      CreateFileChange(const QString& keyFileName, const DirectoryChangeIt& parent, const FilePtr& from, const FileMutPtr& to);

	DirectoryChangeIt CloneDirectoryChange(const DirectoryChangeIt& directoryChange, const Provider provider);
	FileChangeIt      CloneFileChange(const FileChangeIt& fileChange, const Provider provider);
	FileChangeIt      MakeFileChangeByEnginePath(const QString& keyEnginePath, const Provider provider);
	DirectoryChangeIt MakeDirectoryCangeByEnginePath(const QString& enginePath, const Provider provider);

	DirectoryChangeIt CreateDirectoryChangeOnParent(const SEnginePath& directoryName, const Provider provider, DirectoryChangeIt& parentDirectoryChange);
	DirectoryChangeIt StoreDirectoryChangeOnParent(const DirectoryPtr& fromDirectory, const InflightDirectoryChange& change, const DirectoryChangeIt& parentDirectoryChange);
	FileChangeIt      StoreFileChangeOnParent(const FilePtr& fromFile, const InflightFileChange& change, const DirectoryChangeIt& parentDirectoryChange);

	// more support
	void UpdateFileChangeProvider(const FileChangeIt&);
	bool UpdateDirectoryChangeProvider(const DirectoryChangeIt&);

	void RemoveFile(const FilePtr&, const QString& keyEnginePath);
	void RemoveFileChange(const FileChangeIt&);
	void RemoveDirectory(const DirectoryPtr&, const QString& keyEnginePath);
	void RemoveDirectoryChange(const DirectoryChangeIt&);

	void RevertFileChange(const FileChangeIt&);
	void RevertDirectoryChange(const DirectoryChangeIt&);

	void RevertFileChangeNested(const FileChangeIt&);
	void RevertDirectoryChangeNested(const DirectoryChangeIt&);
	void DeepRevertChildChanges(const DirectoryChangeIt&);

	void RemoveDirectoryChangeChildProvider(const DirectoryChangeIt&, const Provider);

	// Split directory entries
	void SplitDirectoryEntriesForProvider(const DirectoryChangeIt& from, const DirectoryChangeIt& to, const Provider);
	void SplitFileInDirectoryForProvider(const FilePtr&, const DirectoryChangeIt& from, const DirectoryChangeIt& to, const Provider);
	void SplitFileChangeInDirectoryForProvider(const FileChangeIt&, const DirectoryChangeIt& from, const DirectoryChangeIt& to, const Provider);
	void SplitSubDirectoryInDirectoryForProvider(const DirectoryPtr&, const DirectoryChangeIt& fromParent, const DirectoryChangeIt& toParent, const Provider);
	void SplitSubDirectoryChangeInDirectoryForProvider(const DirectoryChangeIt&, const DirectoryChangeIt& fromParent, const DirectoryChangeIt& toParent, const Provider provider);

	// Merge directory entires
	void MergeDirectoryEntriesForProvider(const DirectoryChangeIt& from, const DirectoryChangeIt& to, const Provider);
	void MergeFileInDirectoryForProvider(const FilePtr&, const DirectoryChangeIt& from, const DirectoryChangeIt& to, const Provider);
	void MergeFileChangeInDirectoryForProvider(const FileChangeIt&, const DirectoryChangeIt& from, const DirectoryChangeIt& to, const Provider);
	void MergeSubDirectoryInDirectoryForProvider(const DirectoryPtr&, const DirectoryChangeIt& fromParent, const DirectoryChangeIt& toParent, const Provider);
	void MergeSubDirectoryChangeInDirectoryForProvider(const DirectoryChangeIt&, const DirectoryChangeIt& fromParent, const DirectoryChangeIt& toParent, const Provider provider);

	void UpdateFileChangeProviderFileName(const FileChangeIt&, const Provider, const SAbsolutePath& fileName);
	void UpdateDirectoryChangeProviderName(const DirectoryChangeIt&, const Provider, const SAbsolutePath& name);

	void MergeFileWithFileChangeProvider(const FilePtr&, const QString& keyFilePath, const FileChangeIt&, const Provider, const SAbsolutePath& fileName);
	void MergeFileChangeProvider(const FileChangeIt& target, const FileChangeIt& other, const Provider, const SAbsolutePath& fileName);
	void MergeDirectoryWithFileChangeProvider(const DirectoryPtr&, const QString& keyDirectoryPath, const FileChangeIt&, const Provider);
	void MergeDirectoryChangeWithFileChangeProvider(const DirectoryChangeIt& target, const FileChangeIt&, const Provider);

	void MergeFileWithDirectoryChangeProvider(const FilePtr&, const QString& keyFilePath, const DirectoryChangeIt&, const Provider);
	void MergeFileChangeWithDirectoryChangeProvider(const FileChangeIt&, const DirectoryChangeIt&, const Provider);
	void MergeDirectoryWithDirectoryChangeProvider(const DirectoryPtr&, const QString& keyDirectoryPath, const DirectoryChangeIt&, const Provider, const SAbsolutePath& name);
	void MergeDirectoryChangeProvider(const DirectoryChangeIt& target, const DirectoryChangeIt& other, const Provider, const SAbsolutePath& name);

	void UpdateDirectoryChangeEntriesForPath(const DirectoryChangeIt&);

	// Setters for Directorychange & FileChange
	void SetFileChangeActiveDirectoryPath(const FileChangeIt&, const QString& keyDirectoryPath);
	void SetDirectoryChangeActiveParentPath(const DirectoryChangeIt&, const QString& keyParentPath);

	void SetFileChangeActiveKeyEnginePath(const FileChangeIt&, const QString& keyEnginePath, const QString& keyName);
	void SetDirectoryChangeActiveKeyEnginePath(const DirectoryChangeIt&, const QString& keyEnginePath, const QString& keyName);

	// Path Caches
	void AddPathFileChangeCacheEntry(const QString& cachePath, const FileChangeIt&);
	void AddPathDirectoryChangeCacheEntry(const QString& cachePath, const DirectoryChangeIt&);
	void MoveFileChangeCachedPathes(const FileChangeIt& from, const FileChangeIt& to);
	void MoveDirectoryChangeCachedPathes(const DirectoryChangeIt& from, const DirectoryChangeIt& to);

	// Generic Tools
	static QSet<QString> CreateTokens(const QString& name);
	static ProviderIt    SelectProvider(const ProviderDetails&);
	static SProviderData ProviderDataFromFileInfo(const SFileInfoInternal&);
	static SProviderData ProviderDataFromDirectoryInfo(const SDirectoryInfoInternal&);
	static SProviderData ProviderDataFromDirectoryName(const SAbsolutePath& directoryName);

	// Factory methods
	static SnapshotMutPtr  CreateSnapshotMut(const CSnapshot&);
	static DirectoryMutPtr CreateDirectoryMut(const DirectoryPtr& directory);
	static DirectoryMutPtr CreateDirectoryMut(const SIndexedDirectory& directory = SIndexedDirectory());
	static FileMutPtr      CreateFileMut(const FilePtr& file);
	static FileMutPtr      CreateFileMut(const SFile& file = SFile());

	// Highlevel setters for DirectoryMut & FileMut
	static void UpdateDirectoryMut(const DirectoryMutPtr&, const SEnginePath& parentEnginePath);
	static void SetDirectoryMutParentPath(const DirectoryMutPtr&, const SEnginePath& parentEnginePath);
	static void UpdateFileMut(const FileMutPtr&, const SEnginePath& parentEnginePath, const CFileTypeStore& fileTypeStore);
	static void SetFileMutDirectoryEnginePath(const FileMutPtr&, const SEnginePath& directoryEnginePath);

	static void AddDirectoryToSnapshotMut(const DirectoryPtr&, SnapshotMutPtr&);
	static void AddDirectoryToSnapshotMutRecursive(const DirectoryPtr&, SnapshotMutPtr&);
	static void AddDirectoryToParentDirectoryMut(const DirectoryPtr&, const DirectoryMutPtr& parent);
	static void AddFileToDirectoryMut(const FilePtr&, const DirectoryMutPtr&);

private:
	CFileTypeStore      m_fileTypeStore;
	SnapshotPtr         m_base;
	DirectoryChangeIt   m_root;

	DirectoryChanges    m_directoryChanges;
	FileChanges         m_fileChanges;
	PathDirectoryChange m_pathDirectoryChange;
	PathFileChange      m_pathFileChange;
};

} // namespace Internal
} // namespace FileSystem

