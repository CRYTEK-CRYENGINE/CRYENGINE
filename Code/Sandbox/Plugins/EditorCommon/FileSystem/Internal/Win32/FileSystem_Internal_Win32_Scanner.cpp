// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileSystem_Internal_Win32_Scanner.h"

#include "FileSystem_Internal_Win32_UniqueFind.h"

#include "FileSystem/Internal/FileSystem_Internal_PathUtils.h"

#include <QThread>
#include <QSet>
#include <QTimerEvent>

#include <deque>

namespace FileSystem
{
namespace Internal
{
namespace Win32
{

const int MAX_WORK_TIME = 25;             // [milliseconds]
const int BACKGROUND_SEND_INTERVAL = 100; // [milliseconds]

struct CScanner::Implementation
{
	CScanner*                 m_api;
	int                       m_workTimerId;
	int                       m_sendTimerId;
	QThread                   m_thread;
	SScanResult               m_results;
	std::deque<SAbsolutePath> m_preferredQueue;
	std::deque<SAbsolutePath> m_backgroundQueue;

public:
	Implementation(CScanner* api)
		: m_api(api)
		, m_workTimerId(0)
		, m_sendTimerId(0)
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

	void EnqueBackground(const SAbsolutePath& absolutePath)
	{
		m_backgroundQueue.push_back(absolutePath);
		ScheduleWork();
	}

	void EnquePreferred(const SAbsolutePath& absolutePath)
	{
		m_preferredQueue.push_back(absolutePath);
		ScheduleWork();
	}

	void HandleTimerEvent(QTimerEvent* event)
	{
		auto timerId = event->timerId();
		if (m_workTimerId == timerId)
		{
			Work();
			return; // done some work
		}
		if (m_sendTimerId == timerId)
		{
			Send();
			return; // sent result
		}
	}

private:
	void ScheduleWork()
	{
		m_thread.setPriority(m_preferredQueue.empty() ? QThread::LowestPriority : QThread::NormalPriority);
		if (0 == m_workTimerId)
		{
			m_workTimerId = m_api->startTimer(0);
			m_api->IsActiveChanged(true);
		}
	}

	void ScheduleBackgroundSend()
	{
		if (0 == m_sendTimerId)
		{
			m_sendTimerId = m_api->startTimer(BACKGROUND_SEND_INTERVAL);
		}
	}

	void Work()
	{
		auto start = QDateTime::currentMSecsSinceEpoch();
		if (!m_preferredQueue.empty())
		{
			do
			{
				auto directoryPath = m_preferredQueue.front();
				m_preferredQueue.pop_front();
				ScanDirectory(directoryPath, [](const SAbsolutePath&){});
				auto current = QDateTime::currentMSecsSinceEpoch();
				if (current - start > MAX_WORK_TIME)
				{
					break; // worked enough
				}
			}
			while (!m_preferredQueue.empty());
			Send(); // transmit results as fast as possible
			return; // done
		}
		ScheduleBackgroundSend();
		while (!m_backgroundQueue.empty())
		{
			auto directoryPath = m_backgroundQueue.front();
			m_backgroundQueue.pop_front();
			ScanDirectory(directoryPath, [&](const SAbsolutePath& subDirectoryPath)
						{
							m_backgroundQueue.push_back(subDirectoryPath);
			      });
			auto current = QDateTime::currentMSecsSinceEpoch();
			if (current - start > MAX_WORK_TIME)
			{
				break; // worked enough
			}
		}
	}

	void Send()
	{
		if (!m_results.directories.empty())
		{
			m_api->ScanResults(m_results);
			m_results.directories.clear();
		}
		if (m_preferredQueue.empty() && m_backgroundQueue.empty())
		{
			m_api->killTimer(m_sendTimerId);
			m_sendTimerId = 0;
			m_api->killTimer(m_workTimerId);
			m_workTimerId = 0;
			m_thread.setPriority(QThread::NormalPriority);
			m_api->IsActiveChanged(false);
		}
		else if (m_preferredQueue.empty())
		{
			m_thread.setPriority(QThread::LowestPriority);
		}
	}

	template<typename SubDirectoryCallback>
	void ScanDirectory(const SAbsolutePath& absolutePath, const SubDirectoryCallback& subDirectoryCallback)
	{
		CUniqueFind find(absolutePath.full);
		if (!find.IsValid())
		{
			return; // path not valid
		}
		SDirectoryScanResult directoryResult;
		directoryResult.absolutePath = absolutePath;
		do
		{
			if (find.IsRelativeDirectory() || (find.IsSystem() && find.IsHidden()))
			{
				continue; // ignore ".", ".." and hidden system files & directories
			}
			if (find.IsDirectory())
			{
				auto info = CreateDirectoryInfo(absolutePath, find);
				directoryResult.directories << info;
				if (info.linkTargetPath.isEmpty()) // do not scan into links - wait until they got mounted and watched
				{
					subDirectoryCallback(info.absolutePath);
				}
			}
			else
			{
				auto info = CreateFileInfo(absolutePath, find);
				directoryResult.files << info;
			}
		}
		while (find.Next());
		m_results.directories << directoryResult;
	}

	static SDirectoryInfoInternal CreateDirectoryInfo(const SAbsolutePath& parentAbsolutePath, const CUniqueFind& find)
	{
		SDirectoryInfoInternal result;
		result.fullName = find.GetFileName();
		result.keyName = result.fullName.toLower();
		result.absolutePath.full = GetCombinedPath(parentAbsolutePath.full, result.fullName);
		result.absolutePath.key = GetCombinedPath(parentAbsolutePath.key, result.keyName);
		if (find.IsLink())
		{
			result.linkTargetPath = find.GetLinkTargetPath();
		}
		return result;
	}

	static SFileInfoInternal CreateFileInfo(const SAbsolutePath& parentAbsolutePath, const CUniqueFind& find)
	{
		SFileInfoInternal result;
		result.fullName = find.GetFileName();
		result.keyName = result.fullName.toLower();
		result.extension = GetFileExtensionFromFilename(result.keyName);
		result.absolutePath.full = GetCombinedPath(parentAbsolutePath.full, result.fullName);
		result.absolutePath.key = GetCombinedPath(parentAbsolutePath.key, result.keyName);
		if (find.IsLink())
		{
			result.linkTargetPath = find.GetLinkTargetPath();
		}
		result.size = find.GetSize();
		result.lastModified = find.GetLastModified();
		return result;
	}
};

CScanner::CScanner()
	: p(new Implementation(this))
{
	p->StartThread();
}

CScanner::~CScanner()
{}

void CScanner::ScanDirectoryRecursiveInBackground(const QString& fullPath)
{
	SAbsolutePath absolutePath(fullPath);
	p->EnqueBackground(absolutePath);
}

void CScanner::ScanDirectoryPreferred(const QString& fullPath)
{
	SAbsolutePath absolutePath(fullPath);
	p->EnquePreferred(absolutePath);
}

void CScanner::timerEvent(QTimerEvent* event)
{
	p->HandleTimerEvent(event);
}

} // namespace Win32
} // namespace Internal
} // namespace FileSystem

