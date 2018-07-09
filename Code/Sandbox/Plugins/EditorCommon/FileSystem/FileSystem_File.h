// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "FileType.h"
#include "FileSystem_EnginePath.h"

#include <QString>
#include <QVector>
#include <QSet>
#include <QDateTime>

#include <memory>

namespace FileSystem
{

struct SFile;
typedef std::shared_ptr<const SFile> FilePtr;
typedef std::weak_ptr<const SFile>   WeakFilePtr;
typedef const SFile*                 RawFilePtr;
typedef QVector<FilePtr>             FileVector;
typedef QSet<FilePtr>                FileSet;

struct SProviderData;
typedef QHash<RawFilePtr, SProviderData> ProviderDetails;
typedef ProviderDetails::const_iterator  ProviderIt;

/**
 * \brief details of the different providers
 */
struct SProviderData
{
	bool      isFile;       ///< true if this is a file
	QString   fullName;     ///< full name of the file as provided
	quint64   size;         ///< filesize if known (0 otherwise)
	QDateTime lastModified; ///< last modified date (invalid if unknown)

public:
	inline SProviderData() : isFile(true), size(0) {}
};

/**
 * \brief additional content for archive files
 */
struct SArchive
{
	typedef QSet<QString>    FilePathes;      ///< key archive path
	typedef QVector<QString> DirectoryPathes; ///< key archive path

	FilePathes      filePathes;      ///< internal path
	DirectoryPathes directoryPathes; ///< internal folder pathes

public:
	inline bool IsValid() const { return !filePathes.isEmpty() || !directoryPathes.isEmpty(); }
};

/**
 * \brief information structure about a file
 * \note once an instance exists it's never changed
 * \note we use QString implicit sharing to reduce memory footprint
 */
struct SFile
{
	// fileName & fileExtensions are always indexed
	// note: we rely on QString implicit sharing to lower memory footprint
	QString          keyName;   ///< key used in containing directory
	QString          extension; ///< extension part of the keyName
	ProviderIt       provider;  ///< the current provider for details

	SEnginePath      enginePath;          ///< full path used inside the engine
	SEnginePath      directoryEnginePath; ///< parent directory part of the engine_path

	QSet<QString>    tokens; ///< tokens in the fileName

	const SFileType* type; ///< identified file type (always present)

	ProviderDetails  providerDetails; ///< all providers (nullptr for filesystem)

	SArchive         archive; ///< details if file is an archive

public:
	inline SFile() : provider(nullptr), type(nullptr) {}
	inline bool HasShadow() const              { return providerDetails.size() > 1; } ///< \return true if one file is shadowed
	inline bool IsProvidedByFileSystem() const { return provider.key() == nullptr; }  ///< \return true if file is provided by the filesystem
	inline bool IsArchive() const              { return archive.IsValid(); }          ///< \return true if file is an archive
};

/// \brief qHash implementation for shared file pointers
/// \return hash based on memory address
inline uint qHash(const FilePtr& file, uint seed)
{
	return ::qHash((quintptr)(file.get()), seed);
}

} // namespace FileSystem

