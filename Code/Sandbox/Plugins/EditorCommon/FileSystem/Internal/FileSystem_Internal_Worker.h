// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "FileSystem/FileSystem_SubTreeMonitor.h"
#include "FileSystem/FileSystem_FileMonitor.h"
#include "FileSystem/FileSystem_FileFilter.h"

#include "FileSystem_Internal_Data.h"
#include "FileSystem_Internal_UpdateSequence.h"
#include "FileSystem_Internal_InflightUpdate.h"
#include "FileSystem_Internal_Mounts.h"
#include "FileSystem_Internal_FileMonitors.h"
#include "FileSystem_Internal_SubTreeMonitors.h"
#include "FileSystem_Internal_PakFiles.h"

#ifdef _WIN32
	#include "FileSystem/Internal/Win32/FileSystem_Internal_Win32_Scanner.h"
	#include "FileSystem/Internal/Win32/FileSystem_Internal_Win32_Monitor.h"
#endif

#include <QObject>
#include <QSharedPointer>

namespace FileSystem
{
namespace Internal
{
namespace System
{
#ifdef _WIN32
using namespace Win32;
#endif
}   // namespace System

/**
 * \brief the worker is inside the monitor thread
 *
 * It receives data and updates with slots and transmits data to the ui thread with signals
 */
class CWorker
	: public QObject
{
	Q_OBJECT

public:
	CWorker(const QString& absoluteEnginePath, const CFileTypeStore& = CFileTypeStore());

	void Start();

public slots:
	void StartFileMonitor(int handle, const SFileFilter& filter, const FileMonitorPtr&);
	void StopFileMonitor(int handle);

	void StartSubTreeMonitor(int handle, const SFileFilter& filter, const SubTreeMonitorPtr&);
	void StopSubTreeMonitor(int handle);

	void RegisterFileTypes(const QVector<const SFileType*>& fileTypes);

	void AddScanResults(const SScanResult& result);
	void SetIsScannerActive(bool active);
	void AddMonitorUpdates(const CUpdateSequence&);
	void AddUpdateLostTrack(const QString& fullPath);

signals:
	void TriggerSetSnapshot(const FileSystem::SnapshotPtr&);
	void TriggerSetIsScannerActive(bool);

	void TriggerFileMonitorActivated(const FileSystem::FileMonitorPtr&, const FileSystem::SnapshotPtr&);
	void TriggerFileMonitorUpdate(const FileSystem::FileMonitorPtr&, const FileSystem::SFileMonitorUpdate&);

	void TriggerSubTreeMonitorActivated(const FileSystem::SubTreeMonitorPtr&, const FileSystem::SnapshotPtr&);
	void TriggerSubTreeMonitorUpdate(const FileSystem::SubTreeMonitorPtr&, const FileSystem::SSubTreeMonitorUpdate&);

	void TriggerScanDirectoryRecursiveInBackground(const QString& fullPath);
	void TriggerScanDirectoryPreferred(const QString& fullPath);

protected:
	void timerEvent(QTimerEvent*);

private:
	int  GetGamePathIndex() const;

	void ScheduleSnapshotUpdate();

	void UpdateSnapshot();

	void UpdateMountPoints(const SSnapshotDirectoryUpdate&);
	void AddDirectoryLink(const SEnginePath& baseEnginePath, const SDirectoryInfoInternal&);
	void AddMountContents(const SEnginePath& enginePath, const SEnginePath& existingEnginePath);
	void AddMountDirectory(const DirectoryPtr& directory, const SEnginePath& enginePath, const SEnginePath& existingEnginePath);

	void AddPakFileInDirectory(const QString& keyDirectoryEnginePath, const SFileInfoInternal&);
	void AddPakFile(const QString& keyEnginePath);
	void RenamePakFile(const QString& keyEnginePath, const SAbsolutePath& toName);

private:
	struct DirectoryAndFileMonitorEntry
	{
		SFileFilter       filter;
		SubTreeMonitorPtr monitor;
	};

private:
	SnapshotPtr                      m_currentSnapshot;
	QSharedPointer<System::CScanner> m_systemScanner;
	QSharedPointer<System::CMonitor> m_systemMonitor;
	CPakFiles                        m_pakFiles;

	CInflightUpdate                  m_inflight;
	CMounts                          m_directoryLinkMounts;

	CFileMonitors                    m_fileMonitors;
	CSubTreeMonitors                 m_subTreeMonitors;

	int                              m_updateTimerId;
	bool                             m_isScannerActive;
	bool                             m_markScannerFinished;
};

} // namespace Internal
} // namespace FileSystem

