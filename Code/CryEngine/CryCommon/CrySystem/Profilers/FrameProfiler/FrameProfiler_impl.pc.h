// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if defined(CRY_PROFILE_MARKERS_USE_GPA)
	#include <ittnotify.h>

	#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
LINK_THIRD_PARTY_LIBRARY("SDKs/GPA/lib64/libittnotify.lib")
LINK_THIRD_PARTY_LIBRARY("SDKs/GPA/lib64/jitprofiling.lib")
	#elif CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT
LINK_THIRD_PARTY_LIBRARY("SDKs/GPA/lib32/libittnotify.lib")
LINK_THIRD_PARTY_LIBRARY("SDKs/GPA/lib32/jitprofiling.lib")
	#else
		#error Unknown Platform
	#endif
#endif

#include <CryCore/Containers/VectorMap.h>

////////////////////////////////////////////////////////////////////////////
namespace CryProfile {
namespace detail {

#if defined(CRY_PROFILE_MARKERS_USE_GPA)
////////////////////////////////////////////////////////////////////////////
//! Utility function to create a unique domain for GPA in CryEngine.
//! Moved out of function to global scope.
//! Static function variables are not guaranteed to be thread-safe.
static __itt_domain* domain = __itt_domain_create("CryEngine");

__itt_domain* GetDomain()
{
	return domain;
}

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
#endif // #if defined(CRY_PROFILE_MARKERS_USE_GPA)

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//! Set the Thread Name.
void SetThreadName(const char* pName)
{
#if defined(CRY_PROFILE_MARKERS_USE_GPA)
	__itt_thread_set_name(pName);
#endif
}

////////////////////////////////////////////////////////////////////////////
//! Set a one Profiling Event marker.
void SetProfilingEvent(const BYTE colorId, const char* pName)
{
#if defined(CRY_PROFILE_MARKERS_USE_GPA)
	__itt_event_start(GetEventHandle(pName));
#endif
}

////////////////////////////////////////////////////////////////////////////
//! Set the beginning of a profile marker.
void PushProfilingMarker(const BYTE colorId, const char* pName)
{
#if defined(CRY_PROFILE_MARKERS_USE_GPA)
	__itt_task_begin(GetDomain(), __itt_null, __itt_null, GetStringHandle(pName));
#endif
}

////////////////////////////////////////////////////////////////////////////
//! Set the end of a profiling marker.
void PopProfilingMarker(const BYTE colorId, const char* pName)
{
#if defined(CRY_PROFILE_MARKERS_USE_GPA)
	__itt_task_end(GetDomain());
#endif
}

void ProfilerPause()
{
#if defined(CRY_PROFILE_MARKERS_USE_GPA)
	__itt_pause();
#endif
}

void ProfilerResume()
{
#if defined(CRY_PROFILE_MARKERS_USE_GPA)
	__itt_resume();
#endif
}

void ProfilerFrameStart(int nFrameId)
{
#if defined(CRY_PROFILE_MARKERS_USE_GPA)
	__itt_frame_begin_v3(GetDomain(), nullptr);
#endif
}

void ProfilerFrameEnd(int nFrameId)
{
#if defined(CRY_PROFILE_MARKERS_USE_GPA)
	__itt_frame_end_v3(GetDomain(), nullptr);
#endif
}

} // namespace detail
} // namespace CryProfile