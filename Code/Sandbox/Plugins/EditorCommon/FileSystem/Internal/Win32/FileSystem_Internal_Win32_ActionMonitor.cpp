// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileSystem_Internal_Win32_ActionMonitor.h"

#include "FileSystem_Internal_Win32_UniqueHandle.h"

#include <QDir>
#include <QDebug>
#include <QHash>

#include <process.h>
#include <CryCore/Platform/CryWindows.h>

#include <atomic>

namespace FileSystem
{
namespace Internal
{
namespace Win32
{

const unsigned long KILOBYTE = 1024;
const unsigned long MONITOR_BUFFER_SIZE = 257 * KILOBYTE;
/*
 * worst case a path is 32k wchar_t => 64kb memory
 * 256 kb can contain 4 of those pathes + some bytes for the changes => 257 kilobytes
 */
const unsigned long MONITOR_BUFFER_ALIGNMENT = 128;

const int64_t UPDATE_DELAY_MS = 33;              // 33 ms ~= 30 updates per second
const int64_t FILETIME_MILLISECONDS = 1000 * 10; // 100ns

struct CActionMonitor::Implementation
{
private:
	typedef unsigned long Key;
	typedef QString       Path;

	struct KeyPath
	{
		Key  key;
		Path fullPath;
	};
	typedef QPair<Implementation*, KeyPath> MonitorKeyPathPair; // used for APC calls
	typedef QPair<Implementation*, Key>     MonitorKeyPair;     // used for APC calls

	struct SMonitor
	{
		CUniqueHandle   handle;
		OVERLAPPED      overlapped;
		bool            hasChanges;
		bool            hasLostTrack;
		SActionSequence changes;
		std::aligned_storage<MONITOR_BUFFER_SIZE, MONITOR_BUFFER_ALIGNMENT>::type buffer;
		QHash<QString, QString> renameFromMap;  // Maps file paths (excluding file name) to files.
	};

private:
	CActionMonitor*               m_api;
	std::atomic<bool>             m_run;
	CUniqueHandle                 m_threadHandle;
	CUniqueHandle                 m_completionPort;
	std::vector<OVERLAPPED_ENTRY> m_overlappedEntries;

	std::vector<KeyPath>          m_addMonitorPathes;
	std::vector<Key>              m_removeMonitorHandles;

	QHash<Key, SMonitor>          m_monitors;

	bool                          m_haveChanges;
	uint64_t                      m_fileTime;

public:
	Implementation(CActionMonitor* api)
		: m_api(api)
		, m_run(false)
		, m_haveChanges(false)
		, m_fileTime(0)
	{
		::qRegisterMetaType<SActionSequence>();
		StartThread();
	}

	~Implementation()
	{
		StopThread();
	}

	void AddPath(Key key, const Path& fullAbsolutePath)
	{
		KeyPath keyPath = { key, fullAbsolutePath };
		QueueUserAPC(&AddPathAPC, m_threadHandle, (ULONG_PTR) new MonitorKeyPathPair(this, keyPath));
	}

	void Remove(Key key)
	{
		QueueUserAPC(&RemoveAPC, m_threadHandle, (ULONG_PTR) new MonitorKeyPair(this, key));
	}

private:
	void StartThread()
	{
		// _beginthreadex allows to use STL methods
		m_threadHandle = (HANDLE)::_beginthreadex(
		  NULL,    // Security
		  0,       // StackSize
		  &RunAPI, // StartAddress
		  this,    // parameter
		  0,       // InitFlag
		  NULL     // ThreadId
		  );
		if (!m_threadHandle.IsValid())
		{
			auto error = ::GetLastError();
			qWarning() << "FileSystem::Internal::Win32::CActionMonitor" << "Error begin thread" << error;
		}
	}

	void StopThread()
	{
		m_run = false;
		// this APC will break the sleep
		QueueUserAPC(&FinishAPC, m_threadHandle, (ULONG_PTR)this);
		WaitForSingleObject(m_threadHandle, INFINITE);
	}

	static void WINAPI AddPathAPC(ULONG_PTR parameter)
	{
		QScopedPointer<MonitorKeyPathPair> pair((MonitorKeyPathPair*)parameter);
		pair->first->m_addMonitorPathes.push_back(pair->second);
	}

	static void WINAPI RemoveAPC(ULONG_PTR parameter)
	{
		QScopedPointer<MonitorKeyPair> pair((MonitorKeyPair*)parameter);
		pair->first->m_removeMonitorHandles.push_back(pair->second);
	}

	static unsigned WINAPI RunAPI(void* parameter)
	{
		auto self = (Implementation*)parameter;
		self->Run();
		_endthreadex(0);
		return 0;
	}

	static void WINAPI FinishAPC(ULONG_PTR parameter)
	{
		auto self = (Implementation*)parameter;
		self->m_run = false;
	}

private:
	void Run()
	{
		m_run = true;
		CreateCompletionPort();

		while (m_run)
		{
			AddMonitorPathes();
			RemoveMonitorKeys();
			if (m_monitors.empty())
			{
				AlertableSleep();
			}
			else
			{
				AwaitAndHandleMonitorCompletions();
			}
		}
	}

	void CreateCompletionPort()
	{
		m_completionPort = ::CreateIoCompletionPort(
		  INVALID_HANDLE_VALUE, // FileHandle
		  NULL,                 // ExistingCompletionPort
		  0,                    // CompletionKey
		  0                     // NumberOfConcurrentThreads
		  );
		if (!m_completionPort.IsValid())
		{
			auto error = ::GetLastError();
			qWarning() << "FileSystem::Internal::Win32::CActionMonitor" << "Error create completion port" << error;
		}
	}

	void AddMonitorPathes()
	{
		for (const auto& keyPath : m_addMonitorPathes)
		{
			AddMonitorHandlePath(keyPath.key, keyPath.fullPath);
		}
		m_addMonitorPathes.clear();
	}

	void RemoveMonitorKeys()
	{
		for (const auto& key : m_removeMonitorHandles)
		{
			m_monitors.remove(key);
		}
		m_removeMonitorHandles.clear();
	}

	void AlertableSleep()
	{
		::SleepEx(
		  GetTimeout(), // Milliseconds
		  TRUE          // Alertable
		  );
	}

	void AwaitAndHandleMonitorCompletions()
	{
		DWORD count = 0;
		m_overlappedEntries.resize(m_monitors.size());
		bool completed = GetQueuedCompletionStatusEx(
		  m_completionPort,                                              // CompletionPort
		  m_overlappedEntries.data(), (ULONG)m_overlappedEntries.size(), // CompletionPortEntries, Count
		  &count,                                                        // NumEntriesRemoved
		  GetTimeout(),                                                  // Milliseconds
		  TRUE                                                           // Altertable
		  );
		if (completed)
		{
			HandleMonitorCompletions(count);
			UpdateTime();
			return; // got completions
		}
		if (0 != count)
		{
			return; // some APCs were triggered
		}
		auto error = ::GetLastError();
		if (error == WAIT_TIMEOUT)
		{
			return; // normal timeout
		}
		// TODO: check for more errors
		//std::wcout << "Error queued completion status " << error << std::endl;
	}

	void HandleMonitorCompletions(DWORD count)
	{
		for (DWORD i = 0; i < count; ++i)
		{
			const OVERLAPPED_ENTRY& entry = m_overlappedEntries[i];
			auto completionKey = (int)entry.lpCompletionKey;
			auto it = m_monitors.find(completionKey);
			if (it == m_monitors.end())
			{
				continue;
			}
			ExtractChanges(*it, entry.dwNumberOfBytesTransferred);
			ListenChanges(*it);
		}
	}

	DWORD GetTimeout()
	{
		if (!m_haveChanges)
		{
			return INFINITE;
		}
		SYSTEMTIME systemTime;
		::GetSystemTime(&systemTime);
		uint64_t fileTime;
		::SystemTimeToFileTime(&systemTime, (FILETIME*)&fileTime);
		int64_t diff = m_fileTime - fileTime;
		if (diff > 0)
		{
			return 1 + (diff / FILETIME_MILLISECONDS);
		}
		SendAllChanges();
		m_haveChanges = false;
		return INFINITE;
	}

	void UpdateTime()
	{
		if (m_haveChanges)
		{
			return;
		}
		m_haveChanges = true;
		SYSTEMTIME systemTime;
		::GetSystemTime(&systemTime);
		uint64_t fileTime;
		::SystemTimeToFileTime(&systemTime, (FILETIME*)&fileTime);
		m_fileTime = fileTime + UPDATE_DELAY_MS * FILETIME_MILLISECONDS;
	}

	void ExtractChanges(SMonitor& monitor, DWORD numberOfBytes)
	{
		if (sizeof(FILE_NOTIFY_INFORMATION) > numberOfBytes)
		{
			monitor.hasLostTrack = true;
			monitor.hasChanges = false;
			monitor.changes.sequence.clear();
			return; // overflow means we lost track
		}
		if (monitor.hasLostTrack)
		{
			return; // do not add changes if we already lost track
		}
		monitor.hasChanges = true;
		auto buffer = reinterpret_cast<BYTE*>(&monitor.buffer);
		while (true)
		{
			auto notify = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
			AddNotifyToChangeSet(notify, &monitor.changes, monitor.renameFromMap);
			auto offset = notify->NextEntryOffset;
			if (0 == offset)
			{
				break; // terminated
			}
			buffer += offset;
		}
	}

	static void AddNotifyToChangeSet(const FILE_NOTIFY_INFORMATION* notify, SActionSequence* actions, QHash<QString, QString>& renameFromMap)
	{
		QString filename = QString::fromWCharArray(notify->FileName, notify->FileNameLength / 2);
		SActionSequence::ActionType actionType;
		switch (notify->Action)
		{
		case FILE_ACTION_ADDED:
			actionType = SActionSequence::Created;
			break;
		case FILE_ACTION_REMOVED:
			actionType = SActionSequence::Removed;
			break;
		case FILE_ACTION_MODIFIED:
			actionType = SActionSequence::Modified;
			break;
		case FILE_ACTION_RENAMED_OLD_NAME:
			{
				const QString path = QFileInfo(filename).path();
				CRY_ASSERT(!renameFromMap.contains(path));
				renameFromMap[path] = filename;
			}
			return;
		case FILE_ACTION_RENAMED_NEW_NAME:
			{
				const QString path = QFileInfo(filename).path();
				auto it = renameFromMap.find(path);
				CRY_ASSERT(it != renameFromMap.end());
				const QString renameFrom = it.value();
				renameFromMap.erase(it);

				// Always send renameFrom-renameTo in pairs.
				const SActionSequence::SAction renameFromAction = { SActionSequence::RenamedFrom, renameFrom };
				const SActionSequence::SAction renameToAction = { SActionSequence::RenamedTo, filename };
				actions->sequence << renameFromAction << renameToAction;
			}
			return;
		default:
			return;
		}
		SActionSequence::SAction action = { actionType, filename };
		actions->sequence << action;
	}

	void SendAllChanges()
	{
		for (auto& monitor : m_monitors)
		{
			if (monitor.hasLostTrack)
			{
				m_api->SendLostTrack(monitor.changes.baseFullAbsolutePath);
				monitor.hasLostTrack = false;
				continue;
			}
			if (monitor.hasChanges)
			{
				m_api->SendChanges(monitor.changes);
				monitor.changes.sequence.clear();
				monitor.hasChanges = false;
			}
		}
	}

	void AddMonitorHandlePath(Key key, const QString& fullAbsolutePath)
	{
		QString nativePath =
		  (2 == fullAbsolutePath.length() && fullAbsolutePath.endsWith(":"))   // is a drive
		  ? (fullAbsolutePath + QStringLiteral("\\"))
		  : QDir::toNativeSeparators(fullAbsolutePath);
		auto linkPath = QStringLiteral("\\\\?\\") + nativePath;
		auto linkPathWStr = linkPath.toStdWString();
		auto fileHandle = ::CreateFileW(
		  linkPathWStr.data(),                                    // FileName
		  FILE_LIST_DIRECTORY,                                    // DesiredAccess
		  FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, // ShareMode
		  NULL,                                                   // SecurityAttributes
		  OPEN_EXISTING,                                          // CreationDisposition
		  FILE_FLAG_OVERLAPPED | FILE_FLAG_BACKUP_SEMANTICS,      // FlagsAndAttributes
		  NULL                                                    // TemplateFile
		  );

		if (fileHandle == INVALID_HANDLE_VALUE)
		{
			auto error = ::GetLastError();
			qWarning() << "FileSystem::Internal::Win32::CActionMonitor" << "Error create file" << error << fullAbsolutePath;
			return;
		}

		auto portHandle = CreateIoCompletionPort(
		  fileHandle,       // FileHandle
		  m_completionPort, // ExistingCompletionPort
		  key,              // CompletionKey
		  0                 // NumberOfConcurrentThreads
		  );
		if (portHandle != m_completionPort)
		{
			auto error = ::GetLastError();
			qWarning() << "FileSystem::Internal::Win32::CActionMonitor" << "Error attach completion port" << error << fullAbsolutePath;
			CloseHandle(fileHandle);
			return;
		}
		auto& folder = m_monitors[key];
		folder.hasChanges = false;
		folder.hasLostTrack = false;
		folder.changes.key = key;
		folder.changes.baseFullAbsolutePath = fullAbsolutePath;
		folder.handle = fileHandle;
		ListenChanges(folder);

		m_api->SendLostTrack(folder.changes.baseFullAbsolutePath); // send intial lost track
	}

	void ListenChanges(SMonitor& folder)
	{
		auto fileHandle = folder.handle.GetHandle();
		auto readSuccess = ReadDirectoryChangesW(
		  fileHandle,                          // Directory
		  &folder.buffer, MONITOR_BUFFER_SIZE, // Buffer, BufferLength
		  TRUE,                                // WatchSubtree
		  FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
		  FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
		  FILE_NOTIFY_CHANGE_LAST_WRITE /* | FILE_NOTIFY_CHANGE_LAST_ACCESS |
					                             FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_SECURITY*/, // NotifyFilter
		  NULL,               // BytesReturned
		  &folder.overlapped, // Overlapped
		  NULL                // CompletionRoutine
		  );
		if (!readSuccess)
		{
			auto error = ::GetLastError();
			qWarning() << "FileSystem::Internal::Win32::CActionMonitor" << "Error with ReadDirectoryChanges" << error << "in" << folder.changes.baseFullAbsolutePath;
			return;
		}
	}
};

CActionMonitor::CActionMonitor()
	: p(new Implementation(this))
{}

CActionMonitor::~CActionMonitor()
{}

void CActionMonitor::AddPath(unsigned long key, const QString& fullAbsolutePath)
{
	p->AddPath(key, fullAbsolutePath);
}

void CActionMonitor::Remove(unsigned long key)
{
	p->Remove(key);
}

} // namespace Win32
} // namespace Internal
} // namespace FileSystem

