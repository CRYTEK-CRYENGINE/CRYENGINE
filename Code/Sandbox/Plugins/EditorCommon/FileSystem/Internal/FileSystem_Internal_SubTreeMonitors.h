// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "FileSystem_Internal_FilterUtils.h"
#include "FileSystem_Internal_SnapshotUpdate.h"

#include "FileSystem/FileSystem_SubTreeMonitor.h"
#include "FileSystem/FileSystem_FileFilter.h"

namespace FileSystem
{
namespace Internal
{

/// \brief contains all active sub tree monitors
class CSubTreeMonitors
{
public:
	void Add(int handle, const SFileFilter& filter, const SubTreeMonitorPtr& monitor);
	void Remove(int handle);

	/// filter update for all monitors and run callback
	template<typename Callback>
	void UpdateAll(const SSnapshotUpdate& update, Callback callback)
	{
		foreach(const auto & entry, m_fileMonitors)
		{
			auto monitor = entry.monitor.lock();
			if (!monitor)
			{
				Remove(entry.handle);
				continue; // unused monitor
			}
			auto filteredUpdate = FilterUpdate(entry, update);
			callback(monitor, filteredUpdate);
		}
	}

private:
	typedef std::weak_ptr<ISubTreeMonitor> MonitorWeakPtr;

	struct SEntry
	{
		int                             handle;
		SFileFilter                     filter;
		FilterUtils::SDirectoryIncluded root;
		MonitorWeakPtr                  monitor;
	};

private:
	static SDirectory     BuildDirectoryTree(const QVector<QString>& includedKeyPathes);
	SSubTreeMonitorUpdate FilterUpdate(const SEntry& entry, const SSnapshotUpdate& update);

private:
	QHash<int, SEntry> m_fileMonitors;
};

} // namespace Internal
} // namespace FileSystem

