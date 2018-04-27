// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "FileSystem_Directory.h"
#include "FileSystem_Snapshot.h"

#include <QString>
#include <QVector>
#include <QMetaType>

#include <functional>

namespace FileSystem
{

class CSnapshot;
class CEnumerator;

/**
 * \brief filter definition for directory searches
 *
 * use the static methods to easily create a filter
 */
struct SDirectoryFilter
{
	/// \note filterCallback is executed in filesystem thread
	typedef std::function<bool (const FileSystem::SnapshotPtr&, const FileSystem::DirectoryPtr&)> DirectoryFilterCallback;

public:
	QVector<QString>                 directories;              ///< engine pathes of directories to scan (empty means scan in engine root path)
	bool                             recursiveSubdirectories;  ///< true means scan subfolders of the given directories
	QSet<QString>                    directoryTokens;          ///< directory name tokens (separated by " ._-") (if tokens are given one has to match)
	QVector<DirectoryFilterCallback> directoryFilterCallbacks; ///< all directories that match the above are filtered

public:
	inline SDirectoryFilter() : recursiveSubdirectories(true) {}

	static SDirectoryFilter ForDirectory(const QString& directory);
	static SDirectoryFilter ForDirectoryTree(const QString& directory);

protected:
	friend class CSnapshot;
	friend class CEnumerator;
	void MakeInputValid(); ///< makes sure all fields are valid
};

} // namespace FileSystem

Q_DECLARE_METATYPE(FileSystem::SDirectoryFilter)

