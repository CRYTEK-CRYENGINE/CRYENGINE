// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include "EditorCommonAPI.h"

#include "FileSystem_Directory.h"
#include "FileSystem_File.h"

#include "FileSystem_FileMonitor.h"
#include "FileSystem_SubTreeMonitor.h"

#include "FileSystem_Snapshot.h"

#include <QObject>

namespace FileSystem
{

/**
 * \brief Index accelerated enumeration of the engine file system
 *
 * once instanciated will start to scan the filesystem
 */
class EDITOR_COMMON_API CEnumerator
	: public QObject
{
	Q_OBJECT

public:
	typedef int MonitorHandle; ///< use this monitor handle to stop the monitoring withous freeing the update handler

public:
	CEnumerator(const QString& absoluteProjectPath);
	~CEnumerator();

	/// \return the global filesystem path of the engine root
	const QString& GetProjectRootPath() const;

	/// \return the latest snapshot - use it to consistently query for files and directories without updates
	const SnapshotPtr& GetCurrentSnapshot() const;

	/// \returns true if scanning thread is still active
	bool IsScannerActive() const;

signals:
	void IsScannerActiveChanged(bool);

public:
	/// \brief initiate a monitoring for files
	/// \note the update handler is stored a std::weak_ptr - freeing it will stop it automatically
	MonitorHandle StartFileMonitor(const SFileFilter& filter, const FileMonitorPtr&);
	void StopFileMonitor(MonitorHandle);

	/// \brief initiate a monitoring for directories and files
	/// \note the update handler is stored a std::weak_ptr - freeing it will stop it automatically
	MonitorHandle StartSubTreeMonitor(const SFileFilter& filter, const SubTreeMonitorPtr&);
	void StopSubTreeMonitor(MonitorHandle);

	/// \brief register a set of file types to be assigned to files
	void RegisterFileTypes(const QVector<const SFileType*>& fileTypes);

private:
	struct Implementation;
	std::unique_ptr<Implementation> p;
};

} // namespace FileSystem

