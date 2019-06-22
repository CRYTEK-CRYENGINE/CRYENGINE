// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PlatformProfiler.h"

#include <CryThreading/IThreadManager.h>

#if CRY_PLATFORM_WINDOWS && ALLOW_PLATFORM_PROFILER

#if defined(CRY_PROFILE_MARKERS_USE_GPA)
	#include <ittnotify.h>

	LINK_THIRD_PARTY_LIBRARY("SDKs/GPA/lib64/libittnotify.lib")
	LINK_THIRD_PARTY_LIBRARY("SDKs/GPA/lib64/jitprofiling.lib")

	#include <CryCore/Containers/VectorMap.h>

	////////////////////////////////////////////////////////////////////////////
	static __itt_domain* domain = nullptr;

	////////////////////////////////////////////////////////////////////////////
	//! Utility function to create a unique string handle for GPA.
	//! Declared in global scope to ensure thread safety.
	static volatile int _lock = 0;
	static VectorMap<string, __itt_string_handle*> _handle_lookup;

	__itt_string_handle* GetStringHandle(const char* pName)
	{
		__itt_string_handle* pHandle = NULL;

		{
			// first try a simple read lock to prevent thread contention
			CryReadLock(&_lock);
			VectorMap<string, __itt_string_handle*>::iterator it = _handle_lookup.find(CONST_TEMP_STRING(pName));
			if (it != _handle_lookup.end())
			{
				pHandle = it->second;
				CryReleaseReadLock(&_lock);
				return pHandle;
			}
			CryReleaseReadLock(&_lock);
		}

		// Nothing found, use write lock to add a new element safely.
		{
			CryWriteLock(&_lock);
			// check again to make sure not two thread want to add the same handle
			VectorMap<string, __itt_string_handle*>::iterator it = _handle_lookup.find(CONST_TEMP_STRING(pName));
			if (it != _handle_lookup.end())
				pHandle = it->second;
			else
			{
				pHandle = __itt_string_handle_create(pName);
				_handle_lookup.insert(std::make_pair(string(pName), pHandle));
			}
			CryReleaseWriteLock(&_lock);
			return pHandle;
		}
	}

	////////////////////////////////////////////////////////////////////////////
	//! Utility function to create a unique event handle for GPA.
	//! Declared in global scope to ensure thread safety.
	static volatile int _event_lock = 0;
	static VectorMap<string, __itt_event> _event_handle_lookup;

	__itt_event& GetEventHandle(const char* pName)
	{
		__itt_event* pHandle = NULL;

		{
			// first try a simple read lock to prevent thread contention
			CryReadLock(&_event_lock);
			VectorMap<string, __itt_event>::iterator it = _event_handle_lookup.find(CONST_TEMP_STRING(pName));
			if (it != _event_handle_lookup.end())
			{
				pHandle = &it->second;
				CryReleaseReadLock(&_event_lock);
				return *pHandle;
			}
			CryReleaseReadLock(&_event_lock);
		}

		// nothing found, use write lock to add a new element safely
		{
			CryWriteLock(&_event_lock);
			// check again to make sure not two thread want to add the same handle
			VectorMap<string, __itt_event>::iterator it = _event_handle_lookup.find(CONST_TEMP_STRING(pName));
			if (it != _event_handle_lookup.end())
				pHandle = &it->second;
			else
			{
				__itt_event handle = __itt_event_create(pName, strlen(pName));
				pHandle = &_event_handle_lookup.insert(std::make_pair(string(pName), handle)).first->second;
			}
			CryReleaseWriteLock(&_event_lock);
			return *pHandle;
		}
	}

	#define IF_USE_GPA(...) __VA_ARGS__
#else 
	#define IF_USE_GPA(...)
#endif // #if defined(CRY_PROFILE_MARKERS_USE_GPA)

CPlatformProfiler* CPlatformProfiler::s_pInstance = nullptr;

CPlatformProfiler::CPlatformProfiler()
{
	s_pInstance = this;
	IF_USE_GPA(assert(domain == nullptr));
	IF_USE_GPA(domain = __itt_domain_create("CryEngine"));
}

CPlatformProfiler::~CPlatformProfiler()
{
	s_pInstance = nullptr;
}

void CPlatformProfiler::PauseRecording(bool pause)
{
	IF_USE_GPA(
		if (pause && !m_paused)
			__itt_pause();
		if (!pause && m_paused)
			__itt_resume();
	);
	
	m_paused = pause;
}

void CPlatformProfiler::StartThread()
{
	IF_USE_GPA(
		const char* szThreadName = gEnv->pThreadManager->GetThreadName(CryGetCurrentThreadId());
		__itt_thread_set_name(szThreadName)
	);
}

void CPlatformProfiler::StartFrame()
{
	IF_USE_GPA( __itt_frame_begin_v3(domain, nullptr) );
}

void CPlatformProfiler::EndFrame()
{
	IF_USE_GPA( __itt_frame_end_v3(domain, nullptr) );
}

bool CPlatformProfiler::StartSection(SProfilingSection* pSection)
{
	IF_USE_GPA( __itt_task_begin(domain, __itt_null, __itt_null, GetStringHandle(pSection->pTracker->pDescription->szEventname)) );
	return false IF_USE_GPA( || true);
}

void CPlatformProfiler::EndSection(SProfilingSection*)
{
	IF_USE_GPA( __itt_task_end(domain) );
}

void CPlatformProfiler::RecordMarker(SProfilingMarker* pMarker)
{
	IF_USE_GPA( __itt_event_start(GetEventHandle(pMarker->pDescription->szMarkername)) );
}

#endif