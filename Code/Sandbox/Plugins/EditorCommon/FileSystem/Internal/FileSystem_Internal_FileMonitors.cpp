// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileSystem_Internal_FileMonitors.h"

namespace FileSystem
{
namespace Internal
{

void CFileMonitors::Add(int handle, const SFileFilter& filter, const FileMonitorPtr& monitor)
{
	auto rootIncluded = FilterUtils::BuildDirectoryIncludedTree(filter.directories);
	SEntry entry;
	entry.handle = handle;
	entry.filter = filter;
	entry.root = rootIncluded;
	entry.monitor = monitor;
	m_fileMonitors[handle] = entry;
}

void CFileMonitors::Remove(int handle)
{
	m_fileMonitors.remove(handle);
}

SFileMonitorUpdate CFileMonitors::FilterUpdate(const SEntry& entry, const SSnapshotUpdate& update)
{
	using namespace FilterUtils;

	struct SRecurseFlags
	{
		bool from;
		bool to;

		inline SRecurseFlags() : from(false), to(false) {}
	};

	struct SUpdateFilter
	{
		const SFileFilter& m_filter;
		SFileMonitorUpdate m_result;

		SUpdateFilter(const SEntry& entry, const SSnapshotUpdate& update)
			: m_filter(entry.filter)
		{
			m_result.from = update.from;
			m_result.to = update.to;
			SRecurseFlags recurseFlags;
			recurseFlags.from = recurseFlags.to = (entry.root.included && m_filter.recursiveSubdirectories);
			FilterDirectoryUpdateEntries(&entry.root, &entry.root, update.root, recurseFlags);
		}

		void FilterDirectoryUpdateEntries(
		  const SDirectoryIncluded* fromFilter,
		  const SDirectoryIncluded* toFilter,
		  const SSnapshotDirectoryUpdate& update,
		  SRecurseFlags recurseFlags)
		{
			auto from = CheckDirectory(m_filter, fromFilter, m_result.from, update.from, recurseFlags.from);
			auto to = CheckDirectory(m_filter, toFilter, m_result.to, update.to, recurseFlags.to, from);
			if (from.isIncluded || to.isIncluded)
			{
				if (!to.isIncluded)
				{
					FilterFileRemoval(update.from);
				}
				else
				{
					FilterFileEntries(update, from.isIncluded, to.isIncluded);
				}
			}
			if (from.isIncluded || to.isIncluded || from.filterDirectory || to.filterDirectory)
			{
				if (!to.isIncluded && !to.filterDirectory)
				{
					FilterDirectoryRemoval(from.filterDirectory, update.from, from.isRecursive);
					return; // all childrens are removed - do not recurse
				}
				SRecurseFlags entryRecurseFlags;
				entryRecurseFlags.from = from.isRecursive;
				entryRecurseFlags.to = to.isRecursive;
				foreach(const SSnapshotDirectoryUpdate &directoryUpdate, update.directoryUpdates)
				{
					FilterDirectoryUpdateEntries(from.filterDirectory, to.filterDirectory, directoryUpdate, entryRecurseFlags);
				}
			}
		}

		void FilterDirectoryRemoval(const SDirectoryIncluded* filter, const DirectoryPtr& directory, bool isRecursive)
		{
			foreach(const DirectoryPtr &directoryEntry, directory->nameDirectory)
			{
				auto from = CheckDirectory(m_filter, filter, m_result.from, directoryEntry, isRecursive);
				if (from.isIncluded)
				{
					FilterFileRemoval(directoryEntry);
				}
				if (from.filterDirectory)
				{
					FilterDirectoryRemoval(from.filterDirectory, directoryEntry, from.isRecursive);
				}
			}
		}

		void FilterFileRemoval(const DirectoryPtr& directory)
		{
			foreach(const FilePtr &fileEntry, directory->nameFile)
			{
				auto file = CheckFile(true, m_result.from, fileEntry);
				if (file)
				{
					SFileChange change = { file, FilePtr() };
					m_result.fileChanges << change;
				}
			}
		}

		const FilePtr& CheckFile(bool directoryIncluded, const SnapshotPtr& snapshot, const FilePtr& file) const
		{
			static const FilePtr noFile;
			if (!directoryIncluded || !file)
			{
				return noFile;
			}
			if (!HasFile(m_filter, snapshot, file))
			{
				return noFile;
			}
			return file;
		}

		void FilterFileEntries(const SSnapshotDirectoryUpdate& directoryUpdate, bool fromIncluded, bool toIncluded)
		{
			foreach(const SSnapshotFileUpdate &fileUpdate, directoryUpdate.fileUpdates)
			{
				auto from = CheckFile(fromIncluded, m_result.from, fileUpdate.from);
				auto to = CheckFile(toIncluded, m_result.to, fileUpdate.to);
				if (from || to)
				{
					SFileChange change = { from, to };
					m_result.fileChanges << change;
				}
			}
		}
	};

	SUpdateFilter filter(entry, update);
	return filter.m_result;
}

} // namespace Internal
} // namespace FileSystem

