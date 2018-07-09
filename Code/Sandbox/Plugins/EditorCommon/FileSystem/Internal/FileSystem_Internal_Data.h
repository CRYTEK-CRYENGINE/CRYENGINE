// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "FileSystem_Internal_PathUtils.h"

#include <QString>
#include <QVector>
#include <QSet>
#include <QDateTime>
#include <QMetaType>

namespace FileSystem
{
namespace Internal
{

struct SAbsolutePath
{
	QString key;  ///< key version of the path (all lowercase if filesystem is case insensitive)
	QString full; ///< original version of the path

	inline SAbsolutePath() {}
	explicit inline SAbsolutePath(const QString& fullPath) : key(fullPath.toLower()), full(fullPath) {}
	inline SAbsolutePath(const SAbsolutePath& other) : key(other.key), full(other.full) {}
};

/// \brief internal information about a file
struct SFileInfoInternal
{
	SAbsolutePath absolutePath;   ///< path of the file
	QString       keyName;        ///< file name part (used to ensure deduplication)
	QString       fullName;       ///< file name part (used to ensure deduplication)
	QString       extension;      ///< file extension part (used to ensure deduplication)
	QString       linkTargetPath; ///< empty if no link present
	quint64       size;           ///< filesize if known (0 otherwise)
	QDateTime     lastModified;   ///< last modified date (invalid if unknown)

public:
	inline SFileInfoInternal() : size(0) {}
	inline void SetAbsolutePath(const SAbsolutePath& _absolutePath)
	{
		absolutePath = _absolutePath;
		keyName = GetFilenameFromPath(absolutePath.key);
		fullName = GetFilenameFromPath(absolutePath.full);
		extension = GetFileExtensionFromFilename(keyName);
	}
};

/// \brief internal information about a directory
struct SDirectoryInfoInternal
{
	SAbsolutePath absolutePath;   ///< path of the file
	QString       keyName;        ///< directory name part (used to ensure deduplication)
	QString       fullName;       ///< directory name part (used to ensure deduplication)
	QString       linkTargetPath; ///< empty if no link present (link is known before)

public:
	inline void SetAbsolutePath(const SAbsolutePath& _absolutePath)
	{
		absolutePath = _absolutePath;
		keyName = GetFilenameFromPath(absolutePath.key);
		fullName = GetFilenameFromPath(absolutePath.full);
	}
};

/// \brief internal result of a directory scan
struct SDirectoryScanResult
{
	SAbsolutePath                   absolutePath; ///< the key path of the scanned directory
	QVector<SDirectoryInfoInternal> directories;  ///< found directories
	QVector<SFileInfoInternal>      files;        ///< found files
};

/// \brief internal result of multiple directory scans
struct SScanResult
{
	QVector<SDirectoryScanResult> directories;
};

/// \brief all contents of an archive
/// \note the structure is similar to SDirectoryScanResult but it contains the entire archive (not just one folder)
struct SArchiveContentInternal
{
	QString                         keyArchivePath; ///< key version of the path to the archive
	QVector<SDirectoryInfoInternal> directories;    ///< all pathes are lokal to pakFile
	QVector<SFileInfoInternal>      files;          ///< all pathes are lokal to pakFile
};

} // namespace Internal
} // namespace FileSystem

Q_DECLARE_METATYPE(FileSystem::Internal::SScanResult)

