// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileSystem_Internal_Worker.h"

#include <QTimerEvent>

#ifdef FILESYSTEM_INTERNAL_ENABLE_DEBUG_MONITOR_UPDATES
	#define DEBUG_MONITOR_UPDATES(s) qDebug() << s
	#include <QDebug>
#else
	#define DEBUG_MONITOR_UPDATES(s)
#endif

namespace FileSystem
{
namespace Internal
{

const int SNAPSHOT_UPDATE_TIME = 33; // [milliseconds]

CWorker::CWorker(const QString& absoluteEnginePath, const CFileTypeStore& fileTypeStore)
	: m_currentSnapshot(std::make_shared<const CSnapshot>())
	, m_inflight(m_currentSnapshot, fileTypeStore)
	, m_directoryLinkMounts(absoluteEnginePath)
	, m_updateTimerId(0)
	, m_isScannerActive(false)
	, m_markScannerFinished(false)
{
	qRegisterMetaType<SScanResult>();
	qRegisterMetaType<SFileFilter>();
	qRegisterMetaType<SDirectoryFilter>();
	qRegisterMetaType<FileMonitorPtr>();
	qRegisterMetaType<SubTreeMonitorPtr>();
	qRegisterMetaType<SnapshotPtr>();
	qRegisterMetaType<SFileMonitorUpdate>();
	qRegisterMetaType<SSubTreeMonitorUpdate>();
	qRegisterMetaType<QVector<const SFileType*>>();

	m_systemScanner.reset(new System::CScanner);
	m_systemMonitor.reset(new System::CMonitor);

	connect(m_systemScanner.data(), &System::CScanner::ScanResults, this, &CWorker::AddScanResults);
	connect(m_systemScanner.data(), &System::CScanner::IsActiveChanged, this, &CWorker::SetIsScannerActive);
	connect(m_systemMonitor.data(), &System::CMonitor::Updates, this, &CWorker::AddMonitorUpdates);
	connect(m_systemMonitor.data(), &System::CMonitor::LostTrack, this, &CWorker::AddUpdateLostTrack);

	connect(this, &CWorker::TriggerScanDirectoryRecursiveInBackground, m_systemScanner.data(), &System::CScanner::ScanDirectoryRecursiveInBackground, Qt::QueuedConnection);
	connect(this, &CWorker::TriggerScanDirectoryPreferred, m_systemScanner.data(), &System::CScanner::ScanDirectoryPreferred, Qt::QueuedConnection);
}

void CWorker::Start()
{
	auto absoluteEnginePath = m_directoryLinkMounts.GetAbsoluteEnginePath();
	m_systemMonitor->AddPath(absoluteEnginePath);
}

void CWorker::StartFileMonitor(int handle, const SFileFilter& filter, const FileMonitorPtr& monitor)
{
	m_fileMonitors.Add(handle, filter, monitor);
	TriggerFileMonitorActivated(monitor, m_currentSnapshot);
}

void CWorker::StopFileMonitor(int handle)
{
	m_fileMonitors.Remove(handle);
}

void CWorker::StartSubTreeMonitor(int handle, const SFileFilter& filter, const SubTreeMonitorPtr& monitor)
{
	m_subTreeMonitors.Add(handle, filter, monitor);
	TriggerSubTreeMonitorActivated(monitor, m_currentSnapshot);
}

void CWorker::StopSubTreeMonitor(int handle)
{
	m_subTreeMonitors.Remove(handle);
}

void CWorker::RegisterFileTypes(const QVector<const SFileType*>& fileTypes)
{
	m_inflight.RegisterFileTypes(fileTypes);
	ScheduleSnapshotUpdate();
}

void CWorker::AddScanResults(const SScanResult& result)
{
	ScheduleSnapshotUpdate();
	foreach(const SDirectoryScanResult &directoryResult, result.directories)
	{
		m_directoryLinkMounts.ForEachMountPoint(directoryResult.absolutePath, [&](const SEnginePath& enginePath)
				{
					m_inflight.ApplyDirectoryScanResult(enginePath.key, directoryResult);

					foreach(const SDirectoryInfoInternal &directoryInfo, directoryResult.directories)
					{
					  AddDirectoryLink(enginePath, directoryInfo);
					}
					foreach(const SFileInfoInternal &fileInfo, directoryResult.files)
					{
					  AddPakFileInDirectory(enginePath.key, fileInfo);
					}
		    });
	}
}

void CWorker::SetIsScannerActive(bool active)
{
	if (active)
	{
		m_markScannerFinished = false;
		if (!m_isScannerActive)
		{
			m_isScannerActive = true;
			TriggerSetIsScannerActive(true);
		}
	}
	else
	{
		m_markScannerFinished = true;
	}
}

void CWorker::AddMonitorUpdates(const CUpdateSequence& updates)
{
	ScheduleSnapshotUpdate();

	auto removePathCallback = [&](const QString& keyAbsolutePath)
	{
		m_directoryLinkMounts.ForEachKeyMountPoint(keyAbsolutePath, [&](const QString& keyEnginePath)
				{
					DEBUG_MONITOR_UPDATES("Remove" << keyEnginePath);
					m_inflight.RemovePath(keyEnginePath);
					// removing mounts is defered until the snapshot is created
		    });
	};
	auto renamePathCallback = [&](const QString& keyAbsolutePath, const SAbsolutePath& toName)
	{
		m_directoryLinkMounts.ForEachKeyMountPoint(keyAbsolutePath, [&](const QString& keyEnginePath)
				{
					DEBUG_MONITOR_UPDATES("Rename" << keyEnginePath << "To" << toName.full);
					m_inflight.RenamePath(keyEnginePath, toName);
					m_directoryLinkMounts.RenameMount(keyEnginePath, toName);
					RenamePakFile(keyEnginePath, toName);
		    });
	};
	auto createDirectoryCallback = [&](const SDirectoryInfoInternal& info)
	{
		m_directoryLinkMounts.ForEachMountPoint(info.absolutePath, [&](const SEnginePath& enginePath)
				{
					DEBUG_MONITOR_UPDATES("Create Directory" << enginePath.full);
					m_inflight.AddDirectory(enginePath.key, info);
					AddDirectoryLink(enginePath, info);
		    });
	};
	auto createFileCallback = [&](const SFileInfoInternal& info)
	{
		m_directoryLinkMounts.ForEachKeyMountPoint(info.absolutePath.key, [&](const QString& keyEnginePath)
				{
					DEBUG_MONITOR_UPDATES("Create File" << keyEnginePath);
					m_inflight.AddFile(keyEnginePath, info);
					AddPakFile(keyEnginePath);
		    });
	};
	auto modifyDirectoryCallback = [&](const SDirectoryInfoInternal&)
	{
		// It seems you cannot update a mountoint like this
	};
	auto modifyFileCallback = [&](const SFileInfoInternal& info)
	{
		m_directoryLinkMounts.ForEachKeyMountPoint(info.absolutePath.key, [&](const QString& keyEnginePath)
				{
					DEBUG_MONITOR_UPDATES("Modify File" << keyEnginePath);
					m_inflight.UpdateFile(keyEnginePath, info);
					AddPakFile(keyEnginePath);
		    });
	};

	updates.visitAll(
	  removePathCallback, renamePathCallback,
	  createDirectoryCallback, createFileCallback,
	  modifyDirectoryCallback, modifyFileCallback);
}

void CWorker::AddUpdateLostTrack(const QString& fullPath)
{
	SetIsScannerActive(true);
	TriggerScanDirectoryRecursiveInBackground(fullPath);
}

void CWorker::timerEvent(QTimerEvent* event)
{
	if (event->timerId() == m_updateTimerId)
	{
		killTimer(m_updateTimerId);
		m_updateTimerId = 0;
		UpdateSnapshot();
	}
}

void CWorker::ScheduleSnapshotUpdate()
{
	if (0 == m_updateTimerId)
	{
		m_updateTimerId = startTimer(SNAPSHOT_UPDATE_TIME);
	}
}

void CWorker::UpdateSnapshot()
{
	auto diff = m_inflight.CreateSnapshot();
	if (diff.from == diff.to)
	{
		return; // no changes
	}
	m_currentSnapshot = diff.to;
	TriggerSetSnapshot(m_currentSnapshot);
	UpdateMountPoints(diff.root);
	m_fileMonitors.UpdateAll(diff, [this](const FileMonitorPtr& monitor, const SFileMonitorUpdate& update)
			{
				TriggerFileMonitorUpdate(monitor, update);
	    });
	m_subTreeMonitors.UpdateAll(diff, [this](const SubTreeMonitorPtr& monitor, const SSubTreeMonitorUpdate& update)
			{
				TriggerSubTreeMonitorUpdate(monitor, update);
	    });
	m_inflight.Reset(m_currentSnapshot);

	if (m_markScannerFinished)
	{
		m_markScannerFinished = false;
		if (m_isScannerActive)
		{
			m_isScannerActive = false;
			TriggerSetIsScannerActive(false);
		}
	}
}

void CWorker::UpdateMountPoints(const SSnapshotDirectoryUpdate& directoryUpdate)
{
	if (!directoryUpdate.to)
	{
		m_directoryLinkMounts.RemoveMountsIn(directoryUpdate.from->enginePath.key, [&](const SAbsolutePath& absolutePath)
				{
					bool success = m_systemMonitor->RemovePath(absolutePath.key);
					CRY_ASSERT(success);
		    });
		return; // all mounts removed
	}
	foreach(const auto & subDirectoryUpdate, directoryUpdate.directoryUpdates)
	{
		UpdateMountPoints(subDirectoryUpdate);
	}
}

void CWorker::AddDirectoryLink(const SEnginePath& baseEnginePath, const SDirectoryInfoInternal& directoryInfo)
{
	if (directoryInfo.linkTargetPath.isEmpty())
	{
		return;
	}
	auto linkTargetPath = SAbsolutePath(directoryInfo.linkTargetPath);
	SEnginePath directoryEnginePath;
	directoryEnginePath.key = GetCombinedPath(baseEnginePath.key, directoryInfo.keyName);
	directoryEnginePath.full = GetCombinedPath(baseEnginePath.full, directoryInfo.fullName);
	auto otherEnginePath = m_directoryLinkMounts.GetEnginePath(linkTargetPath.key);
	auto success = m_directoryLinkMounts.AddMountPoint(directoryEnginePath, linkTargetPath);
	if (!success)
	{
		return; // mount was rejected
	}
	bool isAdded = m_systemMonitor->AddPath(linkTargetPath);
	if (!isAdded)
	{
		AddMountContents(baseEnginePath, otherEnginePath);
	}
}

void CWorker::AddMountContents(const SEnginePath& enginePath, const SEnginePath& existingEnginePath)
{
	m_directoryLinkMounts.ForEachMountBelow(existingEnginePath.key, [&](const SEnginePath& mountEnginePath, const SAbsolutePath& mountAbsolutePath)
			{
				SEnginePath subEnginePath;
				subEnginePath.key = GetMountPath(mountEnginePath.key, existingEnginePath.key, mountEnginePath.key, enginePath.key);
				subEnginePath.full = GetMountPath(mountEnginePath.key, existingEnginePath.key, mountEnginePath.full, enginePath.full);
				m_directoryLinkMounts.AddMountPoint(subEnginePath, mountAbsolutePath);
	    });
	auto directory = m_currentSnapshot->GetDirectoryByEnginePath(existingEnginePath.key);
	if (!directory)
	{
		return; // some how existing path does not exist
	}
	AddMountDirectory(directory, enginePath, existingEnginePath);
}

void CWorker::AddMountDirectory(const DirectoryPtr& directory, const SEnginePath& enginePath, const SEnginePath& existingEnginePath)
{
	foreach(const auto & file, directory->nameFile)
	{
		QString keyFileEnginePath = GetMountPath(file->enginePath.key, existingEnginePath.key, file->enginePath.key, enginePath.key);
		SFileInfoInternal fileInfo;
		fileInfo.keyName = file->keyName;
		fileInfo.fullName = file->provider->fullName;
		fileInfo.extension = file->extension;
		fileInfo.size = file->provider->size;
		fileInfo.lastModified = file->provider->lastModified;
		m_inflight.AddFile(keyFileEnginePath, fileInfo);
	}
	foreach(const auto & subDirectory, directory->nameDirectory)
	{
		QString keySubDirectoryEnginePath = GetMountPath(subDirectory->enginePath.key, existingEnginePath.key, subDirectory->enginePath.key, enginePath.key);
		SDirectoryInfoInternal directoryInfo;
		directoryInfo.keyName = subDirectory->keyName;
		directoryInfo.fullName = subDirectory->provider->fullName;
		m_inflight.AddDirectory(keySubDirectoryEnginePath, directoryInfo);
		AddMountDirectory(subDirectory, enginePath, existingEnginePath);
	}
}

void CWorker::AddPakFileInDirectory(const QString& keyDirectoryEnginePath, const SFileInfoInternal& info)
{
	if (info.extension != QStringLiteral("pak"))
	{
		return; // not a pak file
	}
	QString keyEnginePath = GetCombinedPath(keyDirectoryEnginePath, info.keyName);
	AddPakFile(keyEnginePath);
}

void CWorker::AddPakFile(const QString& keyEnginePath)
{
	if (!keyEnginePath.endsWith(".pak"))
	{
		return; // not a pak file
	}
	auto archiveContent = m_pakFiles.GetContents(keyEnginePath);
	m_inflight.SetArchiveContent(keyEnginePath, archiveContent);
}

void CWorker::RenamePakFile(const QString& keyEnginePath, const SAbsolutePath& toName)
{
	bool fromPak = keyEnginePath.endsWith(".pak");
	bool toPak = toName.key.endsWith(".pak");
	if (!(fromPak ^ toPak))
	{
		return; // still a pak or no pak file
	}
	if (fromPak)
	{
		m_inflight.CleanArchiveContent(keyEnginePath);
	}
	if (toPak)
	{
		auto toKeyEnginePath = GetCombinedPath(GetDirectoryPath(keyEnginePath), toName.key);
		AddPakFile(toKeyEnginePath);
	}
}

} // namespace Internal
} // namespace FileSystem

#undef DEBUG_MONITOR_UPDATES

