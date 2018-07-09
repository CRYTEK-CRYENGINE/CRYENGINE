// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "FileSystem_File.h"
#include "FileSystem_EnginePath.h"

#include <QString>
#include <QHash>

#include <memory>

namespace FileSystem
{

struct SDirectory;
typedef std::shared_ptr<const SDirectory> DirectoryPtr;
typedef QVector<DirectoryPtr>             DirectoryVector;

/**
 * \brief information structure about a directory
 * \note once an instance exists it's never changed
 * \note we use QString implicit sharing to reduce memory footprint
 */
struct SDirectory
{
	typedef QHash<QString, DirectoryPtr>     NameDirectory; ///< map from key name to sub directory
	typedef QHash<QString, FilePtr>          NameFile;      ///< map from key name to file
	typedef QHash<RawFilePtr, SProviderData> ProviderDetails;
	typedef ProviderDetails::const_iterator  Provider;

public:
	QString         keyName;  ///< key used in containing directory (empty for engineRoot)
	Provider        provider; ///< the current provider for details

	SEnginePath     enginePath;       ///< full engine path of the directory (empty for engineRoot)
	SEnginePath     parentEnginePath; ///< engine path of the parent directory (empty for engineRoot)
	QSet<QString>   tokens;           ///< directoryName splitted by <space>_-.

	NameDirectory   nameDirectory; ///< all contained sub directories
	NameFile        nameFile;      ///< all contained files

	ProviderDetails providerDetails; ///< archives that contain this directory (contains nullptr for filesystem)

public:
	inline bool HasShadow() const              { return providerDetails.size() > 1; } ///< \return true if directory is provided by multiple providers
	inline bool IsProvidedByFileSystem() const { return provider.key() == nullptr; }  ///< \return true if directory is provided by the filesystem
};

/// \brief qHash implementation for shared file pointers
/// \return hash based on memory address
inline uint qHash(const DirectoryPtr& directory, uint seed)
{
	return ::qHash((quintptr)(directory.get()), seed);
}

} // namespace FileSystem

