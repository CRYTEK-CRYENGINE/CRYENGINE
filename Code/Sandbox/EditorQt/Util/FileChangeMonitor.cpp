// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FileChangeMonitor.h"
#include "Util\FileUtil.h"
#include "FilePathUtil.h"
#include <sys/stat.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/IConsole.h>
#include <CryThreading/IThreadManager.h>
#include "CryEdit.h"
#include "QtUtil.h"

//! Message used to communicate with internally created window
#define WM_FILEMONITORCHANGE (WM_APP + 10)

namespace FileChangeMonitor
{
	// the time passed between two identical notifications for same file, used to avoid duplicate notifications
	static const uint32 defaultIgnoreTimeMsec = 500;
	// the monitor tries to wait and combine file change events if the number of waiting to handle events less then value of this constant.
	static const uint32 maxFileChanges = 5;
	// timer ID
	static const uint32 timerId = 1;
	// buffer size used with ReadDirectoryChangesW to get file notifications in OVERLAPPED mode
	static const uint32 fileChangeNotificationBufferSize = 4096;
};

CFileChangeMonitor* CFileChangeMonitor::s_pFileMonitorInstance = NULL;

// Window is used solely to receive notifications from different threads
class CFileChangeMonitorWindow : public CWnd
{
public:
	static const char* GetClassName()
	{
		return "CryFileChangeMonitor";
	}

	CFileChangeMonitorWindow(CFileChangeMonitor* pMonitor)
		: m_pMonitor(pMonitor)
	{
		RECT rect = { 0 };

		RegisterWindowClass();
		VERIFY(CreateEx(0, GetClassName(), "Hidden File Monitor", 0, rect, 0, 0, 0) != FALSE);
	}

	void SetMonitor(CFileChangeMonitor* pMonitor)
	{
		m_pMonitor = pMonitor;
	}

	DECLARE_MESSAGE_MAP()

protected:
	LRESULT OnFileMonitorChange(WPARAM wparam, LPARAM lparam)
	{
		if (m_pMonitor)
		{
			const size_t filesChanged = (size_t)lparam;
			if ((filesChanged < FileChangeMonitor::maxFileChanges) && m_pMonitor->CombineFileChanges())
			{
				// Wait a bit, trying to merge multiple notifications of the same type into one.
				SetTimer(FileChangeMonitor::timerId, FileChangeMonitor::defaultIgnoreTimeMsec, nullptr);
			}
			else
			{
				m_pMonitor->NotifyListeners();
			}
		}

		return 0;
	}

	LRESULT OnTimer(WPARAM wparam, LPARAM lparam)
	{
		if (wparam == FileChangeMonitor::timerId)
		{
			KillTimer(FileChangeMonitor::timerId);

			if (m_pMonitor)
			{
				m_pMonitor->NotifyListeners();
			}
		}

		return 0;
	}

	static bool RegisterWindowClass()
	{
		WNDCLASS windowClass = { 0 };

		windowClass.style = CS_DBLCLKS;
		windowClass.lpfnWndProc = &AfxWndProc;
		windowClass.hInstance = AfxGetInstanceHandle();
		windowClass.hIcon = NULL;
		windowClass.hCursor = NULL;
		windowClass.hbrBackground = NULL;
		windowClass.lpszMenuName = NULL;
		windowClass.lpszClassName = GetClassName();

		return AfxRegisterClass(&windowClass);
	}

	CFileChangeMonitor* m_pMonitor;
};

BEGIN_MESSAGE_MAP(CFileChangeMonitorWindow, CWnd)
	ON_MESSAGE(WM_TIMER, OnTimer)
	ON_MESSAGE(WM_FILEMONITORCHANGE, OnFileMonitorChange)
END_MESSAGE_MAP()

// used to keep track of folder changes
struct SFolderWatchAsyncData
{
	SFolderWatchAsyncData()
		: hFolder(0)
	{
		memset(&overlapped, 0, sizeof(overlapped));
	}

	string           folderPath;
	HANDLE            hFolder;
	std::vector<BYTE> data;
	OVERLAPPED        overlapped;
};

// Directory monitoring thread.
class CFileChangeMonitorThread : public IThread
{
public:
	struct SIgnoreFileMask
	{
		SIgnoreFileMask()
		{
			bRemoveMe = false;
			removeLastTime = 0;
			removeDelayMsec = 0;
		}

		string mask;
		bool    bRemoveMe;
		int     removeLastTime;
		int     removeDelayMsec;
	};

	CFileChangeMonitorThread(HWND listeningWindow)
		: m_listeningWindow(listeningWindow)
	{
		m_bEnabled = true;
		s_killEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		m_handles.push_back(s_killEvent);
		s_updateEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		m_handles.push_back(s_updateEvent);
	}

	virtual ~CFileChangeMonitorThread()
	{
		if (s_killEvent)
		{
			SetEvent(CFileChangeMonitorThread::s_killEvent);
			gEnv->pThreadManager->JoinThread(this, eJM_Join);

			::CloseHandle(s_killEvent);
			s_killEvent = 0;
		}

		if (s_updateEvent)
		{
			::CloseHandle(s_updateEvent);
			s_updateEvent = 0;
		}
	}

	void AddIgnoreFileMask(const char* pMask)
	{
		string maskLower = pMask;

		maskLower.MakeLower();

		CryAutoLock<CryMutex> lock(m_ignoreMasksMutex);
		m_ignoreMasks[pMask] = SIgnoreFileMask();
		m_ignoreMasks[pMask].mask = pMask;
	}

	void RemoveIgnoreFileMask(const char* pMask, int aAfterDelayMsec)
	{
		string maskLower = pMask;

		maskLower.MakeLower();
		CryAutoLock<CryMutex> lock(m_ignoreMasksMutex);

		std::map<string, SIgnoreFileMask>::iterator iter = m_ignoreMasks.find(maskLower);

		if (iter != m_ignoreMasks.end())
		{
			iter->second.bRemoveMe = true;
			iter->second.removeLastTime = GetTickCount();
			iter->second.removeDelayMsec = aAfterDelayMsec;
		}
	}

	//! \return changed file count
	size_t FindChangedFiles(SFolderWatchAsyncData* pFolder)
	{
		DWORD byteCount = 0;
		assert(pFolder);

		if (!pFolder->overlapped.hEvent || !pFolder->hFolder)
			return 0;

		size_t fileCount = 0;

		if (GetOverlappedResult(pFolder->hFolder, &pFolder->overlapped, &byteCount, TRUE))
		{
			FILE_NOTIFY_INFORMATION* lpInfo = (FILE_NOTIFY_INFORMATION*)&pFolder->data[0];

			while (lpInfo)
			{
				const int kMaxFilenameSize = 512;
				WCHAR wcFileName[kMaxFilenameSize + 1] = { 0 };

				if (lpInfo->FileNameLength)
				{
					memcpy(wcFileName, lpInfo->FileName, MIN(kMaxFilenameSize * sizeof(WCHAR), lpInfo->FileNameLength));
					string filename = PathUtil::Make(pFolder->folderPath, string(CryStringUtils::WStrToUTF8(wcFileName)));

					if (AddChangedFile(filename, lpInfo->Action))
					{
						++fileCount;
					}
				}

				if (lpInfo->NextEntryOffset)
				{
					lpInfo = (FILE_NOTIFY_INFORMATION*)(((BYTE*)lpInfo) + lpInfo->NextEntryOffset);
				}
				else
				{
					lpInfo = NULL;
				}
			}

			// arm the watcher's event
			ActivateFileWatcher(pFolder);
		}

		return fileCount;
	}

	bool HasChangedFiles()
	{
		CryAutoLock<CryMutex> lock(m_fileChangesMutex);
		return !m_fileChanges.empty();
	}

	bool PopFileChange(SFileChangeInfo& rOutFileChange)
	{
		CryAutoLock<CryMutex> lock(m_fileChangesMutex);
		if (m_fileChanges.empty())
		{
			return false;
		}
		rOutFileChange = m_fileChanges.front();
		m_fileChanges.pop_front();
		return true;
	}

	static SFileChangeInfo::EChangeType GetChangeTypeFromAction(DWORD action)
	{
		switch (action)
		{
		case FILE_ACTION_ADDED:
			return SFileChangeInfo::eChangeType_Created;
		case FILE_ACTION_REMOVED:
			return SFileChangeInfo::eChangeType_Deleted;
		case FILE_ACTION_MODIFIED:
			return SFileChangeInfo::eChangeType_Modified;
		case FILE_ACTION_RENAMED_OLD_NAME:
			return SFileChangeInfo::eChangeType_RenamedOldName;
		case FILE_ACTION_RENAMED_NEW_NAME:
			return SFileChangeInfo::eChangeType_RenamedNewName;
		default:
			return SFileChangeInfo::eChangeType_Unknown;
		}
	}

	static const char* GetActionName(DWORD action)
	{
		switch (action)
		{
		case FILE_ACTION_ADDED:
			return "created";
		case FILE_ACTION_REMOVED:
			return "deleted";
		case FILE_ACTION_MODIFIED:
			return "modified";
		case FILE_ACTION_RENAMED_OLD_NAME:
			return "old name";
		case FILE_ACTION_RENAMED_NEW_NAME:
			return "new name";
		default:
			return "unknown";
		}
	}

	bool AddChangedFile(const char* pFilename, DWORD action)
	{
		CryAutoLock<CryMutex> lock(m_fileChangesMutex);

		if (!m_bEnabled)
		{
			return false;
		}

		std::vector<char> canonicalPath(strlen(pFilename) + 1);
		char* strCanonicalPath = &canonicalPath[0];

		string pathWithBackslashes = pFilename;
		pathWithBackslashes.Replace('/', '\\');

		if (!PathCanonicalize(strCanonicalPath, pathWithBackslashes.GetString()))
		{
			assert(!"Could not canonize the path, using the incoming path");
			cry_strcpy(strCanonicalPath, canonicalPath.size(), pFilename);
		}

		const string filename = PathUtil::ToUnixPath(const_cast<const char*>(strCanonicalPath)).c_str();
		const SFileChangeInfo::EChangeType changeType = GetChangeTypeFromAction(action);

		// Accumulate multiple changes of the same action in one
		if (!m_fileChanges.empty() && m_fileChanges.back().changeType == changeType && m_fileChanges.back().filename == filename)
		{
			// skipped duplicate file change.
			return false;
		}

		// check if file is ignored
		{
			CryAutoLock<CryMutex> lock(m_ignoreMasksMutex);

			// first lets remove the masks which were scheduled for lazy late removal
			std::map<string, SIgnoreFileMask>::iterator iter2 = m_ignoreMasks.begin();

			while (iter2 != m_ignoreMasks.end())
			{
				if (!iter2->second.bRemoveMe)
				{
					++iter2;
					continue;
				}

				// if timeout, remove!
				if (GetTickCount() - iter2->second.removeLastTime >= iter2->second.removeDelayMsec)
				{
					Log("Removed ignore file mask: '%s'", iter2->first.GetString());
					m_ignoreMasks.erase(iter2++);
				}
				else
				{
					++iter2;
				}
			}

			string strFileLower = filename;
			strFileLower.MakeLower();

			// check if file is ignored
			iter2 = m_ignoreMasks.begin();

			while (iter2 != m_ignoreMasks.end())
			{
				if (PathUtil::MatchWildcard(strFileLower, iter2->first))
				{
					return false;
				}

				++iter2;
			}
		}

		if (CFileChangeMonitor::Instance()->LogFileChanges())
		{
			const char* szActionName = GetActionName(action);
			Log("File changed [%s]: '%s' (%i)", szActionName, filename.c_str(), m_fileChanges.size());
		}

		SFileChangeInfo change;
		change.filename = filename;
		change.changeType = changeType;

		m_fileChanges.push_back(change);

		return true;
	}

	size_t GetNumberOfFileChanges()
	{
		CryAutoLock<CryMutex> lock(m_fileChangesMutex);
		return m_fileChanges.size();
	}

	BOOL ActivateFileWatcher(SFolderWatchAsyncData* pFolder)
	{
		assert(pFolder);
		assert(pFolder->data.size());
		ResetEvent(pFolder->overlapped.hEvent);

		return ReadDirectoryChangesW(
			pFolder->hFolder, &pFolder->data[0], pFolder->data.size(), TRUE,
			FILE_NOTIFY_CHANGE_LAST_WRITE
			| FILE_NOTIFY_CHANGE_CREATION
			| FILE_NOTIFY_CHANGE_SIZE
			| FILE_NOTIFY_CHANGE_FILE_NAME
			| FILE_NOTIFY_CHANGE_DIR_NAME
			| FILE_NOTIFY_CHANGE_ATTRIBUTES
			| FILE_NOTIFY_CHANGE_SECURITY
			, NULL, &pFolder->overlapped, NULL);
	}

	SFolderWatchAsyncData* FindMonitoredItemByEventHandle(HANDLE hnd)
	{
		for (int i = 0, iCount = m_monitorItems.size(); i < iCount; ++i)
		{
			if (m_monitorItems[i]->overlapped.hEvent == hnd)
			{
				return m_monitorItems[i];
			}
		}

		return NULL;
	}

	void SetEnabled(bool bEnabled)
	{
		m_bEnabled = bEnabled;
	}

	bool IsEnabled()
	{
		return m_bEnabled;
	}

	void DeleteAllMonitoredItems()
	{
		for (size_t i = 0, iCount = m_monitorItems.size(); i < iCount; ++i)
		{
			SFolderWatchAsyncData* pItem = m_monitorItems[i];

			::CloseHandle(pItem->hFolder);
			::CloseHandle(pItem->overlapped.hEvent);
			delete pItem;
		}

		m_monitorItems.clear();
		m_handles.clear();

		{
			CryAutoLock<CryMutex> lock(m_fileChangesMutex);
			m_fileChanges.clear();
		}
		m_pendingItems.clear();
	}

	void CheckMonitoredItems()
	{
		if (!m_bEnabled)
		{
			return;
		}

		// check other event handles if they're signalled, skip first two
		for (int i = 0, iCount = m_monitorItems.size(); i < iCount; ++i)
		{
			SFolderWatchAsyncData* pItem = m_monitorItems[i];

			if (pItem && WAIT_OBJECT_0 == WaitForSingleObject(pItem->overlapped.hEvent, 1))
			{
				// gather the changed files
				size_t fileChangeCount = FindChangedFiles(pItem);

				if (fileChangeCount)
				{
					const size_t filesInTheQueue = GetNumberOfFileChanges();

					// notify listening window that something has changed.
					PostMessage(m_listeningWindow, WM_FILEMONITORCHANGE, 0, filesInTheQueue);
				}
			}
		}
	}

	void EnqueuePendingItem(const char* pItem)
	{
		m_pendingItems.push(pItem);
	}

	void AddPendingItem(const char* pItem)
	{
		HANDLE hDir = CreateFile(
			pItem, FILE_LIST_DIRECTORY | GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE,
			NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);

		if (hDir)
		{
			SFolderWatchAsyncData* pFolder = new SFolderWatchAsyncData();

			assert(pFolder);
			pFolder->folderPath = pItem;
			pFolder->data.resize(FileChangeMonitor::fileChangeNotificationBufferSize);
			pFolder->hFolder = hDir;
			pFolder->overlapped.hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);

			if (ActivateFileWatcher(pFolder))
			{
				m_monitorItems.push_back(pFolder);
				m_handles.push_back(pFolder->overlapped.hEvent);
				Log("Adding for file change notifications: '%s'", pItem);
			}
			else
			{
				// abort, delete
				::CloseHandle(pFolder->hFolder);
				::CloseHandle(pFolder->overlapped.hEvent);
				delete pFolder;
			}
		}
	}

	bool MonitoredItemExists(const string& sItem)
	{
		// test for duplicate
		for (std::vector<SFolderWatchAsyncData*>::iterator iter = m_monitorItems.begin();
			iter != m_monitorItems.end(); ++iter)
		{
			// The folder path already exists or is a subfolder of existing.
			if (_strnicmp(sItem.c_str(), (*iter)->folderPath.c_str(), (*iter)->folderPath.length()) == 0)
			{
				return true;
			}
		}

		return false;
	}

	void ThreadEntry()
	{
		while (true)
		{
			UINT handleCount = m_handles.size();
			DWORD dwWaitStatus;
			bool bKill = false;

			dwWaitStatus = WaitForMultipleObjects(
				handleCount, &m_handles[0], FALSE, INFINITE);

			if (m_bEnabled
				&& dwWaitStatus >= WAIT_OBJECT_0
				&& dwWaitStatus < WAIT_OBJECT_0 + handleCount)
			{
				int triggeredHandleIndex = dwWaitStatus - WAIT_OBJECT_0;

				// this is the kill event, so break out
				if (!triggeredHandleIndex)
				{
					bKill = true;
				}

				// this is the update event
				if (triggeredHandleIndex == 1)
				{
					// add pending items to list
					string item;
					while (m_pendingItems.try_pop(item))
					{
						if (!MonitoredItemExists(item))
						{
							AddPendingItem(item);
						}
					}

					// prepare the update event for another triggering
					ResetEvent(s_updateEvent);
				}

				CheckMonitoredItems();

				if (bKill)
				{
					break;
				}
			}
		}

		DeleteAllMonitoredItems();
	}

	static HANDLE s_killEvent;
	static HANDLE s_updateEvent;

protected:
	std::vector<HANDLE>                 m_handles;
	CryMT::queue<string>               m_pendingItems;
	std::vector<SFolderWatchAsyncData*> m_monitorItems;
	std::deque<SFileChangeInfo>       m_fileChanges;
	std::map<string, SIgnoreFileMask>  m_ignoreMasks;
	CryMutex                            m_fileChangesMutex;
	CryMutex                            m_ignoreMasksMutex;
	HWND                                m_listeningWindow;
	volatile bool                       m_bEnabled;
};

HANDLE CFileChangeMonitorThread::s_killEvent = 0;
HANDLE CFileChangeMonitorThread::s_updateEvent = 0;

//////////////////////////////////////////////////////////////////////////
CFileChangeMonitor::CFileChangeMonitor()
{
	ed_logFileChanges = 0;
	ed_combineFileChanges = 1;
	m_pThread = NULL;
}

//////////////////////////////////////////////////////////////////////////
CFileChangeMonitor::~CFileChangeMonitor()
{
	for (TListeners::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
	{
		CFileChangeMonitorListener* pListener = *it;

		if (pListener)
		{
			pListener->SetMonitor(NULL);
		}
	}

	// Send to thread a kill event.
	StopMonitor();
}

CFileChangeMonitor* CFileChangeMonitor::Instance()
{
	if (!s_pFileMonitorInstance)
	{
		s_pFileMonitorInstance = new CFileChangeMonitor();
		s_pFileMonitorInstance->Initialize();
	}

	return s_pFileMonitorInstance;
}

void CFileChangeMonitor::DeleteInstance()
{
	SAFE_DELETE(s_pFileMonitorInstance);
}

void CFileChangeMonitor::Initialize()
{
	REGISTER_CVAR(ed_logFileChanges, 0, VF_NULL, "If its 1, then enable the logging of file monitor file changes");
	REGISTER_CVAR(ed_combineFileChanges, 1, VF_NULL, "With a non-zero value (default), allows the Sandbox file monitor to merge several file changes into one event, if possible.");

	m_window.reset(new CFileChangeMonitorWindow(this));
	m_pThread = new CFileChangeMonitorThread(m_window->GetSafeHwnd());
	m_pThread->AddIgnoreFileMask("*$tmp*");

	// Start thread
	if (!gEnv->pThreadManager->SpawnThread(m_pThread, "FileChangeMonitor"))
	{
		CryFatalError("Error spawning \"FileChangeMonitor\" thread.");
	}
}

void CFileChangeMonitor::AddIgnoreFileMask(const char* pMask)
{
	if (m_pThread)
	{
		Log("Adding '%s' to ignore masks for changed files.", pMask);
		m_pThread->AddIgnoreFileMask(pMask);
	}
}

void CFileChangeMonitor::RemoveIgnoreFileMask(const char* pMask, int aAfterDelayMsec)
{
	if (m_pThread)
	{
		m_pThread->RemoveIgnoreFileMask(pMask, aAfterDelayMsec);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CFileChangeMonitor::MonitorItem(const string& sItem)
{
	if (!m_pThread || !CFileChangeMonitorThread::s_updateEvent)
	{
		return false;
	}

	const QFileInfo fileInfo(QtUtil::ToQString(PathUtil::RemoveSlash(sItem.GetString())));
	if (!fileInfo.exists())
	{
		return false;
	}

	m_pThread->EnqueuePendingItem(fileInfo.isDir() ? sItem : PathUtil::RemoveSlash(PathUtil::GetPathWithoutFilename(sItem)).c_str());
	SetEvent(CFileChangeMonitorThread::s_updateEvent);
	return true;
}

void CFileChangeMonitor::StopMonitor()
{
	if (m_pThread && CFileChangeMonitorThread::s_killEvent != 0)
	{
		SetEvent(CFileChangeMonitorThread::s_killEvent);
	}

	m_window->SetMonitor(NULL);
}

void CFileChangeMonitor::SetEnabled(bool bEnable)
{
	if (m_pThread)
	{
		m_pThread->SetEnabled(bEnable);
	}
}

bool CFileChangeMonitor::IsEnabled()
{
	if (m_pThread)
	{
		return m_pThread->IsEnabled();
	}

	return false;
}

bool CFileChangeMonitor::HaveModifiedFiles() const
{
	return m_pThread->HasChangedFiles();
}

bool CFileChangeMonitor::PopNextFileChange(SFileChangeInfo& rPoppedChange)
{
	if (m_pThread->HasChangedFiles())
	{
		m_pThread->PopFileChange(rPoppedChange);
		return true;
	}

	return false;
}

void CFileChangeMonitor::Subscribe(CFileChangeMonitorListener* pListener)
{
	ASSERT(pListener);
	pListener->SetMonitor(this);
	m_listeners.insert(pListener);
}

void CFileChangeMonitor::Unsubscribe(CFileChangeMonitorListener* pListener)
{
	ASSERT(pListener);
	m_listeners.erase(pListener);
	pListener->SetMonitor(NULL);
}

void CFileChangeMonitor::NotifyListeners()
{
	std::vector<SFileChangeInfo> changes;

	while (HaveModifiedFiles())
	{
		SFileChangeInfo change;
		if (PopNextFileChange(change))
		{
			changes.push_back(change);
		}
	}

	for (size_t i = 0; i < changes.size(); ++i)
	{
		const SFileChangeInfo& change = changes[i];
		for (auto it = m_listeners.begin(); it != m_listeners.end(); ++it)
		{
			CFileChangeMonitorListener* pListener = *it;

			if (pListener)
			{
				pListener->OnFileMonitorChange(change);
			}
		}
	}
}

