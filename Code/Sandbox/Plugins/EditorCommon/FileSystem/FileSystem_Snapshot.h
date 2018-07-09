// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include "EditorCommonAPI.h"

#include "FileSystem_Directory.h"
#include "FileSystem_File.h"
#include "FileSystem_SubTree.h"

#include <QMetaType>

#include "memory"

namespace FileSystem
{
namespace Internal
{
class CInflightUpdate;
} // namespace Internal

struct SDirectoryFilter;
struct SFileFilter;

class CSnapshot;
typedef std::shared_ptr<const CSnapshot> SnapshotPtr;

/**
 * \brief snapshotted view on the filesystem
 * \note a snapshot stays consistent as long as you keep a shared_ptr instance around
 *
 * if you need to be informed about updates consider using a monitor
 */
class EDITOR_COMMON_API CSnapshot
	: public std::enable_shared_from_this<CSnapshot>
{
public:
	/// \return engine root directory
	DirectoryPtr GetEngineRootDirectory() const;

	/// \return nullptr if directory does not exist
	DirectoryPtr GetDirectoryByEnginePath(const QString& enginePath) const;

	/// \return nullptr if file does not exist
	FilePtr GetFileByEnginePath(const QString& enginePath) const;

	/// \return all directories that match the given filter
	DirectoryVector FindDirectories(const SDirectoryFilter&) const;

	/// \return all files that match the given filter
	FileVector FindFiles(const SFileFilter&) const;

	/// \return all roots of matching sub trees
	SSubTree FindSubTree(const SFileFilter&) const;

private:
	friend class FileSystem::Internal::CInflightUpdate;
	struct Implementation;
	friend struct Implementation;

	QHash<QString, DirectoryPtr>      m_pathToDirectory; ///< key path => Directory
	// indices
	QMultiHash<QString, DirectoryPtr> m_tokenToDirectories;
};

} // namespace FileSystem

Q_DECLARE_METATYPE(FileSystem::SnapshotPtr)

