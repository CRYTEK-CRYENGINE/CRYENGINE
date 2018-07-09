// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "FileSystem/FileSystem_Snapshot.h"
#include "FileSystem/FileSystem_File.h"

#include <QMetaType>

#include <memory>

namespace FileSystem
{
namespace Internal
{
class CWorkerApi;
} // namespace Internal

class IFileMonitor;
typedef std::shared_ptr<IFileMonitor> FileMonitorPtr;

struct SFileChange
{
	FilePtr from; // nullptr if file was created
	FilePtr to;   // nullptr if file was deleted
};

struct SFileMonitorUpdate
{
	SnapshotPtr          from; ///< snapshot these changes apply to
	SnapshotPtr          to;   ///< new snapshot containing all changes

	QVector<SFileChange> fileChanges; ///< all file changes that match the filter (random order)
};

/**
 * \brief interface that receives file changes
 *
 * use the pointer addresses as indices
 *
 * prefer this interface if you do no bother about the directory structure of the results
 *
 * note: be aware that you class has to be created for a shared_ptr
 */
class IFileMonitor
{
	friend class Internal::CWorkerApi;
protected:
	/// \brief called when the monitor is established
	/// \param the initial snapshot where updates are applied to
	virtual void Activated(const SnapshotPtr& initialSnapshot) = 0;

	/// \brief processes updates filtered for this monitor
	virtual void Update(const SFileMonitorUpdate&) = 0;
};

} // FileSystem

Q_DECLARE_METATYPE(FileSystem::SFileMonitorUpdate)
Q_DECLARE_METATYPE(FileSystem::FileMonitorPtr)

