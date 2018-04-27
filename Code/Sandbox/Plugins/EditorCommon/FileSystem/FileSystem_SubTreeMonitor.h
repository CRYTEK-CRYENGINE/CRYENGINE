// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "FileSystem_File.h"
#include "FileSystem_FileMonitor.h"
#include "FileSystem_Directory.h"

#include <QMetaType>

#include <memory>

namespace FileSystem
{
namespace Internal
{
class CWorkerApi;
} // namespace Internal

class ISubTreeMonitor;
typedef std::shared_ptr<ISubTreeMonitor> SubTreeMonitorPtr;

struct SSubTreeChange
{
	DirectoryPtr            from;                ///< directory of from snapshot (nullptr for created directories)
	DirectoryPtr            to;                  ///< directory of to snapshot (nullptr for removed directories, all descendants are removed)
	QVector<SSubTreeChange> subDirectoryChanges; ///< all changes to subdirectories
	QVector<SFileChange>    fileChanges;         ///< changes to files in this directory
};

struct SSubTreeMonitorUpdate
{
	SnapshotPtr    from; ///< snapshot these changes apply to
	SnapshotPtr    to;   ///< new snapshot containing all changes
	SSubTreeChange root; ///< changes of sub trees
};

/**
 * \brief interface that receives file and directory changes
 *
 * use the pointer addresses as indices
 *
 * be aware that
 * - remove of a directory invalidates all files and directories inside
 * - you will receive removes for stuff you never got (ignore those)
 * - update of a directory will be followed by updates for all file and directories inside
 */
class ISubTreeMonitor
{
	friend class Internal::CWorkerApi;
protected:
	/// \brief called when the monitor is established
	/// \param the initial snapshot where updates are applied to
	virtual void Activated(const SnapshotPtr& initialSnapshot) = 0;

	/// \brief processes updates filtered for this monitor
	virtual void Update(const SSubTreeMonitorUpdate&) = 0;
};

} // namespace FileSystem

Q_DECLARE_METATYPE(FileSystem::SubTreeMonitorPtr)
Q_DECLARE_METATYPE(FileSystem::SSubTreeMonitorUpdate)

