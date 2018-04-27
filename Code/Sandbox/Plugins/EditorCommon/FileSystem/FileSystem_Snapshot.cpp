// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileSystem_Snapshot.h"

#include "Internal/FileSystem_Internal_PathUtils.h"
#include "Internal/FileSystem_Internal_IndexedDirectory.h"
#include "Internal/FileSystem_Internal_FilterUtils.h"

#include "FileSystem_DirectoryFilter.h"
#include "FileSystem_FileFilter.h"

#include <QDir>

namespace FileSystem
{

struct CSnapshot::Implementation
{
	template<typename Callback>
	static void FindInDirectories(const SnapshotPtr& snapshot, const SDirectoryFilter& filter, Callback&& callback)
	{
		for (const auto& directoryPath : filter.directories)
		{
			auto directory = snapshot->m_pathToDirectory[directoryPath];
			if (!directory)
			{
				continue;
			}
			FindInDirectory(snapshot, directory, filter, callback);
		}
	}

	template<typename Callback>
	static void FindInDirectory(const SnapshotPtr& snapshot, const DirectoryPtr& directory, const SDirectoryFilter& filter, Callback&& callback)
	{
		if (filter.directoryTokens.isEmpty())
		{
			if (filter.recursiveSubdirectories)
			{
				FindAllInDirectoryAndRecurse(snapshot, directory, filter, callback);
			}
			else
			{
				FindAllInDirectory(snapshot, directory, filter, callback);
			}
		}
		else if (1 == filter.directoryTokens.count())
		{
			if (filter.recursiveSubdirectories)
			{
				FindOneTokenInDirectoryAndRecurse(snapshot, directory, filter, callback);
			}
			else
			{
				FindOneTokenInDirectory(snapshot, directory, filter, callback);
			}
		}
		else
		{
			if (filter.recursiveSubdirectories)
			{
				FindAnyTokenInDirectoryAndRecurse(snapshot, directory, filter, callback);
			}
			else
			{
				FindAnyTokenInDirectory(snapshot, directory, filter, callback);
			}
		}
	}

	template<typename Callback>
	static void FindAllInDirectoryAndRecurse(const SnapshotPtr& snapshot, const DirectoryPtr& directory, const SDirectoryFilter& filter, Callback&& callback)
	{
		FindAllInDirectory(snapshot, directory, filter, callback);
		for (const auto& subDirectory : directory->nameDirectory)
		{
			FindAllInDirectoryAndRecurse(snapshot, subDirectory, filter, callback);
		}
	}

	template<typename Callback>
	static void FindAllInDirectory(const SnapshotPtr& snapshot, const DirectoryPtr& directory, const SDirectoryFilter& filter, Callback&& callback)
	{
		CRY_ASSERT(filter.directoryTokens.isEmpty());
		// search only in current directory, sub directories are handled in Recurse function
		FilterDirectory(snapshot, directory, filter, callback);
	}

	template<typename Callback>
	static void FindOneTokenInDirectoryAndRecurse(const SnapshotPtr& snapshot, const DirectoryPtr& directory, const SDirectoryFilter& filter, Callback&& callback)
	{
		FindOneTokenInDirectory(snapshot, directory, filter, callback);
		for (const auto& subDirectory : directory->nameDirectory)
		{
			FindOneTokenInDirectoryAndRecurse(snapshot, subDirectory, filter, callback);
		}
	}

	template<typename Callback>
	static void FindOneTokenInDirectory(const SnapshotPtr& snapshot, const DirectoryPtr& directory, const SDirectoryFilter& filter, Callback&& callback)
	{
		CRY_ASSERT(1 == filter.directoryTokens.count());
		const auto& token = *filter.directoryTokens.begin();
		auto indexedDirectory = reinterpret_cast<const Internal::SIndexedDirectory*>(directory.get());
		auto results = indexedDirectory->tokenDirectories.values(token);
		for (const auto& subDirectory : results)
		{
			FilterDirectory(snapshot, subDirectory, filter, callback);
		}
	}

	template<typename Callback>
	static void FindAnyTokenInDirectoryAndRecurse(const SnapshotPtr& snapshot, const DirectoryPtr& directory, const SDirectoryFilter& filter, Callback&& callback)
	{
		FindAnyTokenInDirectory(snapshot, directory, filter, callback);
		for (const auto& subDirectory : directory->nameDirectory)
		{
			FindAnyTokenInDirectoryAndRecurse(snapshot, subDirectory, filter, callback);
		}
	}

	template<typename Callback>
	static void FindAnyTokenInDirectory(const SnapshotPtr& snapshot, const DirectoryPtr& directory, const SDirectoryFilter& filter, Callback&& callback)
	{
		CRY_ASSERT(1 < filter.directoryTokens.count());
		QSet<const void*> found;
		auto indexedDirectory = reinterpret_cast<const Internal::SIndexedDirectory*>(directory.get());
		for (const auto& token : filter.directoryTokens)
		{
			auto results = indexedDirectory->tokenDirectories.values(token);
			for (const auto& subDirectory : results)
			{
				if (!found.contains(subDirectory.get()))
				{
					FilterDirectory(snapshot, subDirectory, filter, callback);
					found.insert(subDirectory.get());
				}
			}
		}
	}

	template<typename Callback>
	static void FilterDirectory(const SnapshotPtr& snapshot, const DirectoryPtr& directory, const SDirectoryFilter& filter, Callback&& callback)
	{
		for (const auto& directoryFilter : filter.directoryFilterCallbacks)
		{
			if (!directoryFilter(snapshot, directory))
			{
				return; // filter denied
			}
		}
		callback(directory);
	}

	template<typename Callback>
	static void FindFilesInDirectory(const SnapshotPtr& snapshot, const DirectoryPtr& directory, const SFileFilter& filter, Callback&& callback)
	{
		if (filter.fileExtensions.isEmpty() && filter.fileTokens.isEmpty())
		{
			FindAllFilesInDirectory(snapshot, directory, filter, callback);
		}
		else if (filter.fileExtensions.count() > filter.fileTokens.count())
		{
			FindFileExtensionsInDirectory(directory, filter, [&](const FilePtr& fileInfo)
				{
					FilterFileTokens(snapshot, fileInfo, filter, callback);
			  });
		}
		else
		{
			FindFileTokensInDirectory(directory, filter, [&](const FilePtr& fileInfo)
				{
					FilterFileExtensions(snapshot, fileInfo, filter, callback);
			  });
		}
	}

	template<typename Callback>
	static void FindAllFilesInDirectory(const SnapshotPtr& snapshot, const DirectoryPtr& directory, const SFileFilter& filter, Callback&& callback)
	{
		CRY_ASSERT(filter.fileExtensions.isEmpty() && filter.fileTokens.isEmpty());
		for (const auto& fileInfo : directory->nameFile)
		{
			FilterFile(snapshot, fileInfo, filter, callback);
		}
	}

	template<typename Callback>
	static void FindFileExtensionsInDirectory(const DirectoryPtr& directory, const SFileFilter& filter, Callback&& callback)
	{
		CRY_ASSERT(!filter.fileExtensions.isEmpty());
		QSet<const void*> found;
		auto indexedDirectory = reinterpret_cast<const Internal::SIndexedDirectory*>(directory.get());
		for (const auto& token : filter.fileExtensions)
		{
			auto results = indexedDirectory->extensionFiles.values(token);
			for (const auto& file : results)
			{
				if (!found.contains(file.get()))
				{
					callback(file);
					found.insert(file.get());
				}
			}
		}
	}

	template<typename Callback>
	static void FindFileTokensInDirectory(const DirectoryPtr& directory, const SFileFilter& filter, Callback&& callback)
	{
		CRY_ASSERT(!filter.fileTokens.isEmpty());
		QSet<const void*> found;
		auto indexedDirectory = reinterpret_cast<const Internal::SIndexedDirectory*>(directory.get());
		for (const auto& token : filter.fileTokens)
		{
			auto results = indexedDirectory->tokenFiles.values(token);
			for (const auto& file : results)
			{
				if (!found.contains(file.get()))
				{
					callback(file);
					found.insert(file.get());
				}
			}
		}
	}

	template<typename Callback>
	static void FilterFileTokens(const SnapshotPtr& snapshot, const FilePtr& file, const SFileFilter& filter, Callback&& callback)
	{
		if (filter.fileTokens.isEmpty())
		{
			FilterFile(snapshot, file, filter, callback);
			return; // nothing to match
		}
		auto fileTokens = file->tokens;
		for (const auto& filterToken : filter.fileTokens)
		{
			if (fileTokens.contains(filterToken))
			{
				FilterFile(snapshot, file, filter, callback);
				return; // match found
			}
		}
	}

	template<typename Callback>
	static void FilterFileExtensions(const SnapshotPtr& snapshot, const FilePtr& file, const SFileFilter& filter, Callback&& callback)
	{
		auto fileExtension = file->extension;
		if (filter.fileExtensions.isEmpty() || filter.fileExtensions.contains(fileExtension))
		{
			FilterFile(snapshot, file, filter, callback);
			return; // match found
		}
	}

	template<typename Callback>
	static void FilterFile(const SnapshotPtr& snapshot, const FilePtr& file, const SFileFilter& filter, Callback&& callback)
	{
		for (const auto& fileFilter : filter.fileFilterCallbacks)
		{
			if (!fileFilter(snapshot, file))
			{
				return; // filter denied
			}
		}
		callback(file);
	}

	static SSubTree CollectSubTreeMatches(const SnapshotPtr& snapshot, const DirectoryPtr& directory, const SFileFilter& filter, const Internal::FilterUtils::SDirectoryIncluded* directoryIncluded, bool isRecursion)
	{
		using namespace Internal::FilterUtils;

		SSubTree result;
		auto directoryCheck = CheckDirectory(filter, directoryIncluded, snapshot, directory, isRecursion);
		result.wasConsidered = directoryCheck.wasConsidered;
		if (directoryCheck.isIncluded)
		{
			FindFilesInDirectory(snapshot, directory, filter, [&](const FilePtr& file)
				{
					result.files << file;
			  });
		}
		if (directoryCheck.isRecursive || (directoryCheck.filterDirectory && !directoryCheck.filterDirectory->entries.isEmpty()))
		{
			foreach(const DirectoryPtr &subDirectory, directory->nameDirectory)
			{
				SSubTree subResult = CollectSubTreeMatches(snapshot, subDirectory, filter, directoryCheck.filterDirectory, directoryCheck.isRecursive);
				if (subResult.directory)
				{
					result.subDirectories << subResult;
				}
			}
		}
		if (!result.subDirectories.isEmpty() || !result.files.isEmpty() || (!filter.skipEmptyDirectories && directoryCheck.isIncluded))
		{
			result.directory = directory;
		}
		return result;
	}
};

DirectoryPtr CSnapshot::GetEngineRootDirectory() const
{
	return GetDirectoryByEnginePath(QString());
}

DirectoryPtr CSnapshot::GetDirectoryByEnginePath(const QString& enginePath) const
{
	auto keyEnginePath = SEnginePath::ConvertUserToKeyPath(enginePath);
	return m_pathToDirectory[keyEnginePath];
}

FilePtr CSnapshot::GetFileByEnginePath(const QString& enginePath) const
{
	auto keyEnginePath = SEnginePath::ConvertUserToKeyPath(enginePath);
	auto keyDirectoryPath = Internal::GetDirectoryPath(keyEnginePath);
	auto directory = m_pathToDirectory[keyDirectoryPath];
	if (!directory)
	{
		return FilePtr(); // containing directory not found
	}
	auto keyFileName = Internal::GetFilenameFromPath(keyEnginePath);
	return directory->nameFile[keyFileName];
}

DirectoryVector CSnapshot::FindDirectories(const SDirectoryFilter& filter) const
{
	DirectoryVector result;
	auto keyFilter = filter;
	keyFilter.MakeInputValid();
	auto snapshot = shared_from_this();
	Implementation::FindInDirectories(snapshot, keyFilter, [&](const DirectoryPtr& directory)
		{
			result.push_back(directory);
	  });
	return result;
}

FileVector CSnapshot::FindFiles(const SFileFilter& filter) const
{
	FileVector result;
	auto keyFilter = filter;
	keyFilter.MakeInputValid();
	auto snapshot = shared_from_this();
	Implementation::FindInDirectories(snapshot, keyFilter, [&](const DirectoryPtr& directory)
		{
			Implementation::FindFilesInDirectory(snapshot, directory, keyFilter, [&](const FilePtr& file)
			{
				result.push_back(file);
			});
	  });
	return result;
}

SSubTree CSnapshot::FindSubTree(const SFileFilter& filter) const
{
	auto keyFilter = filter;
	keyFilter.MakeInputValid();
	auto snapshot = shared_from_this();
	auto rootDirectory = GetEngineRootDirectory();
	if (!rootDirectory)
	{
		return SSubTree();
	}
	auto rootIncluded = Internal::FilterUtils::BuildDirectoryIncludedTree(keyFilter.directories);
	return Implementation::CollectSubTreeMatches(snapshot, rootDirectory, keyFilter, &rootIncluded, rootIncluded.included && keyFilter.recursiveSubdirectories);
}

} // namespace FileSystem

