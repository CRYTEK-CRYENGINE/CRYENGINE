// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "FileUtil.h"
#include <CryMemory/STLPoolAllocator.h>
#include <CryThreading/IThreadManager.h>

class PLUGIN_API CIndexedFiles
{
	friend class CFileIndexingThread;
public:
	static CIndexedFiles& GetDB()
	{
		if (!s_pIndexedFiles)
		{
			assert(!"CIndexedFiles not created! Make sure you use CIndexedFiles::GetDB() after CIndexedFiles::StartFileIndexing() is called.");
		}
		assert(s_pIndexedFiles);
		return *s_pIndexedFiles;
	}

	static bool HasFileIndexingDone()
	{ return s_bIndexingDone > 0; }

	static void Create()
	{
		assert(!s_pIndexedFiles);
		s_pIndexedFiles = new CIndexedFiles;
	}

	static void Destroy()
	{
		GetFileIndexingThread().SignalStopWork();
		if (gEnv)
		{
			const auto pThreadManager = gEnv->pThreadManager;
			if (pThreadManager)
			{
				pThreadManager->JoinThread(&GetFileIndexingThread(), eJM_Join);
			}
		}
		SAFE_DELETE(s_pIndexedFiles);
	}

	static void StartFileIndexing()
	{
		assert(s_bIndexingDone == 0);
		assert(s_pIndexedFiles);

		if (!s_pIndexedFiles)
		{
			return;
		}

		if (!gEnv->pThreadManager->SpawnThread(&GetFileIndexingThread(), "FileIndexing"))
		{
			CryFatalError("Error spawning \"FileIndexing\" thread.");
		}
	}

public:
	void Initialize(const string& path, CFileUtil::ScanDirectoryUpdateCallBack updateCB = NULL);

	// Adds a new file to the database.
	void AddFile(const CFileUtil::FileDesc& path);
	// Removes a no-longer-existing file from the database.
	void RemoveFile(const string& path);
	// Refreshes this database for the subdirectory.
	void Refresh(const string& path, bool recursive = true);

	void GetFilesWithTags(CFileUtil::FileArray& files, const std::vector<string>& tags) const;

	//! This method returns all the tags which start with a given prefix.
	//! It is useful for the tag auto-completion.
	void   GetTagsOfPrefix(std::vector<string>& tags, const string& prefix) const;

	uint32 GetTotalCount() const
	{ return (uint32)m_files.size(); }

private:
	CFileUtil::FileArray   m_files;
	std::map<string, int> m_pathToIndex;
#ifdef _DEBUG
	// In debug, the validation phase of the pool allocator when destructed takes so much time.
	typedef std::set<int, std::less<int>>                                                                      int_set;
	typedef std::map<string, int_set, std::less<string>>                                                     TagTable;
#else
	typedef std::set<int, std::less<int>, stl::STLPoolAllocator<int>>                                          int_set;
	typedef std::map<string, int_set, std::less<string>, stl::STLPoolAllocator<std::pair<string, int_set>>> TagTable;
#endif
	TagTable m_tags;
	string  m_rootPath;

	void GetTags(std::vector<string>& tags, const string& path) const;
	void PrepareTagTable();

	// A done flag for the background file indexing
	static volatile int s_bIndexingDone;
	// A thread for the background file indexing
	class CFileIndexingThread : public IThread
	{
	public:
		// Start accepting work on thread
		virtual void ThreadEntry()
		{
			CIndexedFiles::GetDB().Initialize(PathUtil::GetGameFolder().c_str(), CallBack);
			CryInterlockedAdd(&CIndexedFiles::s_bIndexingDone, 1);
		}

		CFileIndexingThread() : m_abort(false) {}

		// Signals the thread that it should not accept anymore work and exit
		void SignalStopWork()
		{
			m_abort = true;
		}

		virtual ~CFileIndexingThread()
		{
			SignalStopWork();
			if (gEnv)
			{
				const auto pThreadManager = gEnv->pThreadManager;
				if (pThreadManager)
				{
					pThreadManager->JoinThread(this, eJM_Join);
				}
			}
		}
	private:
		bool m_abort;
		static bool CallBack(const string& msg)
		{
			if (CIndexedFiles::GetFileIndexingThread().m_abort)
				return false;
			return true;
		}
	};

	static CFileIndexingThread& GetFileIndexingThread()
	{
		static CFileIndexingThread s_fileIndexingThread;

		return s_fileIndexingThread;
	}

	// A global database for tagged files
	static CIndexedFiles* s_pIndexedFiles;
};

