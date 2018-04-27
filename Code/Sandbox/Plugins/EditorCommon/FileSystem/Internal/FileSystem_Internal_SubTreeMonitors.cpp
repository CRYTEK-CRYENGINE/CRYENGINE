// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileSystem_Internal_SubTreeMonitors.h"
#include "FileSystem_Internal_FilterUtils.h"

namespace FileSystem
{
namespace Internal
{

void CSubTreeMonitors::Add(int handle, const SFileFilter& filter, const SubTreeMonitorPtr& monitor)
{
	auto root = FilterUtils::BuildDirectoryIncludedTree(filter.directories);
	SEntry entry = { handle, filter, root, monitor };
	m_fileMonitors[handle] = entry;
}

void CSubTreeMonitors::Remove(int handle)
{
	m_fileMonitors.remove(handle);
}

SSubTreeMonitorUpdate CSubTreeMonitors::FilterUpdate(const SEntry& entry, const SSnapshotUpdate& update)
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
		const SFileFilter&    m_filter;
		SSubTreeMonitorUpdate m_result;

		bool                  IsFileFilter() const
		{
			return !m_filter.fileExtensions.empty() || !m_filter.fileTokens.isEmpty() || !m_filter.fileFilterCallbacks.isEmpty();
		}

		SUpdateFilter(const SEntry& entry, const SSnapshotUpdate& update)
			: m_filter(entry.filter)
		{
			m_result.from = update.from;
			m_result.to = update.to;
			SRecurseFlags recurseFlags;
			recurseFlags.from = recurseFlags.to = (entry.root.included && m_filter.recursiveSubdirectories);
			m_result.root = FilterDirectoryUpdateEntries(&entry.root, &entry.root, update.root, recurseFlags);
		}

		SSubTreeChange FilterDirectoryUpdateEntries(
		  const SDirectoryIncluded* fromFilter,
		  const SDirectoryIncluded* toFilter,
		  const SSnapshotDirectoryUpdate& update,
		  SRecurseFlags recurseFlags)
		{
			SSubTreeChange result;
			auto from = CheckDirectory(m_filter, fromFilter, m_result.from, update.from, recurseFlags.from);
			auto to = CheckDirectory(m_filter, toFilter, m_result.to, update.to, recurseFlags.to, from);
			auto isFromSkipped = m_filter.skipEmptyDirectories;
			auto isToSkipped = m_filter.skipEmptyDirectories;
			if (from.isIncluded || to.isIncluded)
			{
				if (!to.isIncluded)
				{
					result.from = update.from;
					return result; // directory is removed (do not try to filter further)
				}

				foreach(const SSnapshotFileUpdate &fileUpdate, update.fileUpdates)
				{
					auto fromFile = CheckFile(from.isIncluded, m_result.from, fileUpdate.from);
					auto toFile = CheckFile(to.isIncluded, m_result.to, fileUpdate.to);
					if (fromFile || toFile)
					{
						isFromSkipped = isFromSkipped && (!fromFile);
						isToSkipped = isToSkipped && (!toFile);

						SFileChange change = { fromFile, toFile };
						result.fileChanges << change;
					}
				}
			}
			if (from.isIncluded || to.isIncluded || from.filterDirectory || to.filterDirectory)
			{
				SRecurseFlags entryRecurseFlags;
				entryRecurseFlags.from = from.isRecursive;
				entryRecurseFlags.to = to.isRecursive;
				foreach(const SSnapshotDirectoryUpdate &directoryUpdate, update.directoryUpdates)
				{
					auto subResult = FilterDirectoryUpdateEntries(from.filterDirectory, to.filterDirectory, directoryUpdate, /*fileFilterMatch,*/ entryRecurseFlags);
					if (subResult.from || subResult.to)
					{
						if (!to.isIncluded && !to.filterDirectory)
						{
							result.from = update.from;
							return result; // from.filterDirectory had a match => now removed, do not scan further
						}
						isFromSkipped = isFromSkipped && (!subResult.from);
						isToSkipped = isToSkipped && (!subResult.to);

						result.subDirectoryChanges << subResult;
					}
				}
				if (!result.subDirectoryChanges.isEmpty())
				{
					to.isIncluded = to.isIncluded || to.filterDirectory;
					from.isIncluded = from.isIncluded || from.filterDirectory;
				}
			}
			if (from.isIncluded)
			{
				if (!isFromSkipped || HasAnyFile(m_filter, m_result.from, update.from))
				{
					result.from = update.from;
				}
			}
			if (to.isIncluded)
			{
				if (!isToSkipped || HasAnyFile(m_filter, m_result.to, update.to))
				{
					result.to = update.to;
				}
			}
			return result;
		}

		const FilePtr& CheckFile(bool directoryIncluded, const SnapshotPtr& snapshot, const FilePtr& file) const
		{
			static const FilePtr noFile;
			if (!directoryIncluded || !file)
			{
				return noFile;
			}
			if (!FilterUtils::HasFile(m_filter, snapshot, file))
			{
				return noFile;
			}
			return file;
		}
	};

	SUpdateFilter filter(entry, update);
	return filter.m_result;
}

} // namespace Internal
} // namespace FileSystem

