// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "FileSystem/FileSystem_File.h"
#include "FileSystem/FileSystem_Directory.h"
#include "FileSystem/FileSystem_Snapshot.h"

#include "FileSystem/FileSystem_DirectoryFilter.h"
#include "FileSystem/FileSystem_FileFilter.h"

#include <QSet>

namespace FileSystem
{
namespace Internal
{
namespace FilterUtils
{

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
template<class T>
inline bool QSet_intersects(const QSet<T>& one, const QSet<T>& other)
{
	return one.intersects(other);
}
#else
template<class T>
inline bool QSet_intersects(const QSet<T>& one, const QSet<T>& other)
{
	const bool otherIsBigger = other.size() > one.size();
	const QSet<T>& smallestSet = otherIsBigger ? one : other;
	const QSet<T>& biggestSet = otherIsBigger ? other : one;
	typename QSet<T>::const_iterator i = smallestSet.cbegin();
	typename QSet<T>::const_iterator e = smallestSet.cend();

	while (i != e)
	{
		if (biggestSet.contains(*i))
			return true;
		++i;
	}
	return false;
}
#endif

struct SDirectoryIncluded
{
	QHash<QString, SDirectoryIncluded> entries;
	bool                               included;

	inline SDirectoryIncluded() : included(false) {}
};

inline SDirectoryIncluded BuildDirectoryIncludedTree(const QVector<QString>& includedKeyPathes)
{
	SDirectoryIncluded root;
	foreach(const QString &keyPath, includedKeyPathes)
	{
		const auto keyParts = keyPath.split('/');
		SDirectoryIncluded* pPart = &root;
		foreach(const QString &keyPart, keyParts)
		{
			pPart = &pPart->entries[keyPart];
		}
		pPart->included = true;
	}
	return root;
}

inline bool HasDirectory(const SDirectoryFilter& filter, const SnapshotPtr& snapshot, const DirectoryPtr& directory)
{
	if (!directory)
	{
		return false; // non existing directory is never included
	}
	if (!filter.directoryTokens.isEmpty())
	{
		auto hasIntersects = QSet_intersects(filter.directoryTokens, directory->tokens);
		if (!hasIntersects)
		{
			return false; // none of the search tokens found
		}
	}
	if (!filter.directoryFilterCallbacks.isEmpty())
	{
		auto consesus = std::all_of(filter.directoryFilterCallbacks.begin(), filter.directoryFilterCallbacks.end(), [=](const SDirectoryFilter::DirectoryFilterCallback& callback)
					{
						return callback(snapshot, directory);
		      });
		if (!consesus)
		{
			return false; // at least one file denied
		}
	}
	return true;
}

inline bool HasFile(const SFileFilter& filter, const SnapshotPtr& snapshot, const FilePtr& file)
{
	if (!filter.fileExtensions.isEmpty())
	{
		auto contained = filter.fileExtensions.contains(file->extension);
		if (!contained)
		{
			return false;
		}
	}
	if (!filter.fileTokens.isEmpty())
	{
		auto hasIntersects = QSet_intersects(filter.fileTokens, file->tokens);
		if (!hasIntersects)
		{
			return false;
		}
	}
	if (!filter.fileFilterCallbacks.isEmpty())
	{
		auto consesus = std::all_of(filter.fileFilterCallbacks.begin(), filter.fileFilterCallbacks.end(), [=](const SFileFilter::FileFilterCallback& callback)
					{
						return callback(snapshot, file);
		      });
		if (!consesus)
		{
			return false;
		}
	}
	return true;
}

inline bool HasAnyFile(const SFileFilter& filter, const SnapshotPtr& snapshot, const DirectoryPtr& directory)
{
	auto has_file = std::any_of(directory->nameFile.begin(), directory->nameFile.end(), [&](const FilePtr& file)
				{
					return HasFile(filter, snapshot, file);
	      });
	return has_file || std::any_of(directory->nameDirectory.begin(), directory->nameDirectory.end(), [&](const DirectoryPtr& subDirectory)
				{
					return HasAnyFile(filter, snapshot, subDirectory);
	      });
}

struct SCheckDirectoryResult
{
	const SDirectoryIncluded* filterDirectory;
	bool                      wasConsidered : 1;
	bool                      isIncluded    : 1;
	bool                      isRecursive   : 1;

	inline SCheckDirectoryResult() : filterDirectory(nullptr), wasConsidered(false), isIncluded(false), isRecursive(false) {}
};

inline SCheckDirectoryResult CheckDirectory(
  const SDirectoryFilter& filter,
  const SDirectoryIncluded* parentIncluded,
  const SnapshotPtr& snapshot, const DirectoryPtr& directory,
  bool inRecursion,
  SCheckDirectoryResult result = SCheckDirectoryResult())
{
	const SDirectoryIncluded* directoryIncluded = nullptr;
	if (parentIncluded && directory)
	{
		auto it = parentIncluded->entries.find(directory->keyName);
		if (it != parentIncluded->entries.end())
		{
			directoryIncluded = &*it;
			if (directoryIncluded == result.filterDirectory)
			{
				return result; // shortcut if result exists
			}
		}
	}
	result.filterDirectory = directoryIncluded;
	result.wasConsidered = (inRecursion || (directoryIncluded && directoryIncluded->included));
	result.isIncluded = result.wasConsidered && HasDirectory(filter, snapshot, directory);
	result.isRecursive = inRecursion || (result.wasConsidered && filter.recursiveSubdirectories);
	return result;
}

} // namespace FilterUtils
} // namespace Internal
} // namespace FileSystem

