// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "FileSystem_File.h"
#include "FileSystem_DirectoryFilter.h"

#include <QMetaType>

struct SFileType;

namespace FileSystem
{

/**
 * \brief find files in the matched folders
 *
 * use the static methods to easily create a filter
 */
struct SFileFilter
	: SDirectoryFilter   // Files are only search in folders that match
{
	typedef std::function<bool (const SnapshotPtr&, const FilePtr&)> FileFilterCallback;

public:
	QSet<QString>               fileExtensions;       ///< text after last <dot> has to equal one of these
	QSet<QString>               fileTokens;           ///< tokens are separated by <space> <dot> <underscore> <minus>
	QVector<FileFilterCallback> fileFilterCallbacks;  ///< all files that match above are filtered
	bool                        skipEmptyDirectories; ///< do not include empty directories

public:
	inline SFileFilter() : skipEmptyDirectories(false) {}

	static SFileFilter ForRootExtension(const QString& extension);
	static SFileFilter ForTreeExtension(const QString& extension);
	static SFileFilter ForDirectoryAndExtension(const QString& directoryEnginePath, const QString& extension);
	static SFileFilter ForDirectoryTreeAndExtension(const QString& directoryEnginePath, const QString& extension);

	static SFileFilter ForFileType(const SFileType*);
	static SFileFilter ForDirectoryAndFileType(const QString& directory, const SFileType*);
	static SFileFilter ForDirectoryTokenAndFileType(const QString& directoryToken, const SFileType*);
	// ... a method for everything that is used at least twice

protected:
	friend class CSnapshot;
	friend class CEnumerator;
	void MakeInputValid(); ///< makes sure all fields are valid
};

} // namespace FileSystem

Q_DECLARE_METATYPE(FileSystem::SFileFilter)

