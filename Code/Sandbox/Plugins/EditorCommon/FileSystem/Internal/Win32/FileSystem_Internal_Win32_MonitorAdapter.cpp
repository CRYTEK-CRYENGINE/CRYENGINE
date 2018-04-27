// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileSystem_Internal_Win32_MonitorAdapter.h"
#include "FileSystem_Internal_Win32_Utils.h"

#include "FileSystem/Internal/FileSystem_Internal_PathUtils.h"

#include <QThread>
#include <QDir>

#ifdef FILESYSTEM_INTERNAL_WIN32_ENABLE_DEBUG_MONITOR_ADAPTER
	#include <QDebug>
	#define DEBUG_MONITOR_ADAPTER(s) qDebug() << s
#else
	#define DEBUG_MONITOR_ADAPTER(s)
#endif

#include <CryCore/Platform/CryWindows.h>

namespace FileSystem
{
namespace Internal
{
namespace Win32
{

struct CMonitorAdapter::Implementation
{
	Implementation(CMonitorAdapter* api)
		: m_api(api)
	{}

	~Implementation()
	{
		m_thread.quit();
		m_thread.wait();
	}

	void StartThread()
	{
		m_api->moveToThread(&m_thread);
		m_thread.start();
	}

	void Enqueue(const SActionSequence& actions)
	{
		CUpdateSequence updateSequence;
		auto baseDir = QDir(actions.baseFullAbsolutePath);
		QSet<QString> modifiedPathes;
		QSet<QString> failedPathes;
		auto itEnd = actions.sequence.end();
		for (auto it = actions.sequence.begin(); it != itEnd; ++it)
		{
			const SActionSequence::SAction& action = *it;
			const auto absolutePath = SAbsolutePath(baseDir.filePath(QDir::fromNativeSeparators(action.fullPath)));
			switch (action.type)
			{
			case SActionSequence::Created:
				DEBUG_MONITOR_ADAPTER("Created" << absolutePath.full);
				HandleCreate(updateSequence, modifiedPathes, failedPathes, absolutePath);
				break;
			case SActionSequence::Removed:
				DEBUG_MONITOR_ADAPTER("Removed" << absolutePath.full);
				HandleRemove(updateSequence, modifiedPathes, failedPathes, absolutePath);
				break;
			case SActionSequence::Modified:
				DEBUG_MONITOR_ADAPTER("Modified" << absolutePath.full);
				HandleModified(updateSequence, modifiedPathes, failedPathes, absolutePath);
				break;
			case SActionSequence::RenamedFrom:
				{
					CRY_ASSERT(it + 1 != itEnd);
					const SActionSequence::SAction& renameTo = *(++it);
					CRY_ASSERT(renameTo.type == SActionSequence::RenamedTo);
					const auto toName = SAbsolutePath(GetFilenameFromPath(QDir::fromNativeSeparators(renameTo.fullPath)));
					DEBUG_MONITOR_ADAPTER("Renamed" << absolutePath.full << "To" << toName.full);
					HandleRename(updateSequence, modifiedPathes, failedPathes, absolutePath, toName);
					break;
				}
			case SActionSequence::RenamedTo:
				CRY_ASSERT(!"RenameFrom missing");
				break;
			default:
				CRY_ASSERT(!"Unknown action");
			}
		}
		m_api->SendUpdates(updateSequence);
	}

private:
	static void HandleCreate(CUpdateSequence& updates, QSet<QString>& modifiedPathes, QSet<QString>& failedPathes, const SAbsolutePath& absolutePath)
	{
		if (modifiedPathes.contains(absolutePath.key) || failedPathes.contains(absolutePath.key))
		{
			return; // was already handled or was handled with error
		}
		auto attributes = GetAttributes(absolutePath.full);
		if (IsFailed(attributes))
		{
			failedPathes.insert(absolutePath.key);
			return; // file not found (probably deleted or renamed)
		}
		if (IsDirectory(attributes))
		{
			updates.AddCreateDirectory(CreateDirectoryInfo(absolutePath, attributes));
		}
		else
		{
			updates.AddCreateFile(CreateFileInfo(absolutePath, attributes));
		}
		modifiedPathes.insert(absolutePath.key);
	}

	static void HandleRemove(CUpdateSequence& updates, QSet<QString>& modifiedPathes, QSet<QString>& failedPathes, const SAbsolutePath& absolutePath)
	{
		updates.AddRemovePath(absolutePath.key);
		failedPathes.remove(absolutePath.key);   // it failed because it was deleted
		modifiedPathes.remove(absolutePath.key); // allow to recreate it
	}

	static void HandleModified(CUpdateSequence& updates, QSet<QString>& modifiedPathes, QSet<QString>& failedPathes, const SAbsolutePath& absolutePath)
	{
		if (modifiedPathes.contains(absolutePath.key) || failedPathes.contains(absolutePath.key))
		{
			return; // was already handled or was handled with error
		}
		auto attributes = GetAttributes(absolutePath.full);
		if (IsFailed(attributes))
		{
			failedPathes.insert(absolutePath.key);
			return; // file not found
		}
		if (IsDirectory(attributes))
		{
			updates.AddModifyDirectory(CreateDirectoryInfo(absolutePath, attributes));
		}
		else
		{
			updates.AddModifyFile(CreateFileInfo(absolutePath, attributes));
		}
		modifiedPathes.insert(absolutePath.key);
	}

	static void HandleRename(CUpdateSequence& updates, QSet<QString>& modifiedPathes, QSet<QString>& failedPathes, const SAbsolutePath& absolutePath, const SAbsolutePath& toName)
	{
		updates.AddRename(absolutePath.key, toName);
		modifiedPathes.remove(absolutePath.key); // allow to recreate it
		if (failedPathes.remove(absolutePath.key))
		{
			SAbsolutePath toAbsolutePath;
			toAbsolutePath.key = GetCombinedPath(GetDirectoryPath(absolutePath.key), toName.key);
			toAbsolutePath.full = GetCombinedPath(GetDirectoryPath(absolutePath.full), toName.full);
			HandleModified(updates, modifiedPathes, failedPathes, toAbsolutePath);
		}
	}

	static WIN32_FILE_ATTRIBUTE_DATA GetAttributes(const QString& fullAbsolutePath)
	{
		WIN32_FILE_ATTRIBUTE_DATA result;
		auto fullPathWStr = fullAbsolutePath.toStdWString();
		auto success = GetFileAttributesExW(fullPathWStr.c_str(), GetFileExInfoStandard, &result);
		if (!success)
		{
			result.dwFileAttributes = MAXDWORD;
			// auto error = GetLastError();
			// TODO handle error
		}
		return result;
	}

	static bool IsFailed(const WIN32_FILE_ATTRIBUTE_DATA& attributes)
	{
		return MAXDWORD == attributes.dwFileAttributes;
	}

	static bool IsDirectory(const WIN32_FILE_ATTRIBUTE_DATA& attributes)
	{
		return (FILE_ATTRIBUTE_DIRECTORY & attributes.dwFileAttributes) != 0;
	}

	static SDirectoryInfoInternal CreateDirectoryInfo(const SAbsolutePath& absolutePath, const WIN32_FILE_ATTRIBUTE_DATA& attributes)
	{
		SDirectoryInfoInternal result;
		result.SetAbsolutePath(absolutePath);
		if ((FILE_ATTRIBUTE_REPARSE_POINT & attributes.dwFileAttributes) != 0)
		{
			result.linkTargetPath = GetLinkTargetPath(absolutePath.full);
		}
		return result;
	}

	static SFileInfoInternal CreateFileInfo(const SAbsolutePath& absolutePath, const WIN32_FILE_ATTRIBUTE_DATA& attributes)
	{
		SFileInfoInternal result;
		result.SetAbsolutePath(absolutePath);
		result.size = ((uint64_t)attributes.nFileSizeHigh << 32) + attributes.nFileSizeLow;
		SYSTEMTIME systemTime;
		FileTimeToSystemTime(&attributes.ftLastWriteTime, &systemTime);
		result.lastModified = QDateTime(
		  QDate(systemTime.wYear, systemTime.wMonth, systemTime.wDay),
		  QTime(systemTime.wHour, systemTime.wMinute, systemTime.wSecond, systemTime.wMilliseconds),
		  Qt::UTC);
		if ((FILE_ATTRIBUTE_REPARSE_POINT & attributes.dwFileAttributes) != 0)
		{
			result.linkTargetPath = GetLinkTargetPath(absolutePath.full);
		}
		return result;
	}

private:
	CMonitorAdapter* m_api;
	QThread          m_thread;
};

CMonitorAdapter::CMonitorAdapter()
	: p(new Implementation(this))
{
	p->StartThread();
}

CMonitorAdapter::~CMonitorAdapter()
{}

void CMonitorAdapter::Enqueue(const SActionSequence& changes)
{
	p->Enqueue(changes);
}

} // namespace Win32
} // namespace Internal
} // namespace FileSystem

#undef DEBUG_MONITOR_ADAPTER

