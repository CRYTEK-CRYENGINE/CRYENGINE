// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Platform/platform.h>

#include "FrameProfiler_Shared.h"

////////////////////////////////////////////////////////////////////////////
// forward declaration for per-platform implementation functions
namespace CryProfile
{
void PushProfilingMarker(const EProfileDescription desc, const char* pName, ...);
void PopProfilingMarker(const EProfileDescription desc, const char* pName);

namespace detail
{
void        SetThreadName(const char* pName);
void        SetProfilingEvent(const BYTE colorId, const char* pName);
void        PushProfilingMarker(const BYTE colorId, const char* pName);
void        PopProfilingMarker(const BYTE colorId, const char* pName);
void        ProfilerFrameStart(int nFrameId);
void        ProfilerFrameEnd(int nFrameId);
void        ProfilerPause();
void        ProfilerResume();
}
inline void ProfilerFrameStart(int nFrameId) { detail::ProfilerFrameStart(nFrameId); }
inline void ProfilerFrameEnd(int nFrameId)   { detail::ProfilerFrameEnd(nFrameId); }
inline void ProfilerPause()                  { detail::ProfilerPause(); }
inline void ProfilerResume()                 { detail::ProfilerResume(); }
}

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
// Profiling is enabled in every configuration except Release

// Platform profilers such as Pix, Razor or GPA
	#if ALLOW_PLATFORM_PROFILER

		#define PLATFORM_PROFILER_THREADNAME(szName) do { ::CryProfile::detail::SetThreadName(szName); } while (0)
		#define PLATFORM_PROFILER_FRAMESTART(szName) /*not implemented*/

		#if defined(PERFORMANCE_BUILD)
			#define PLATFORM_PROFILER_REGION(szName)           /*not implemented*/
			#define PLATFORM_PROFILER_REGION_WAITING(szName)   /*not implemented*/
			#define PLATFORM_PROFILER_FUNCTION(szName)         /*not implemented*/
			#define PLATFORM_PROFILER_FUNCTION_WAITING(szName) /*not implemented*/
			#define PLATFORM_PROFILER_SECTION(szName)          /*not implemented*/
			#define PLATFORM_PROFILER_SECTION_WAITING(szName)  /*not implemented*/
			#define PLATFORM_PROFILER_MARKER(szLabel)          do { ::CryProfile::detail::SetProfilingEvent(0, szLabel); } while (0)
			#define PLATFORM_PROFILER_PUSH(szLabel)            do { ::CryProfile::detail::PushProfilingMarker(0, szLabel); } while (0)
			#define PLATFORM_PROFILER_POP(szLabel)             do { ::CryProfile::detail::PopProfilingMarker(0,szLabel); } while (0)
		#else                                                // profile builds take another path to allow runtime switch
			#define PLATFORM_PROFILER_REGION(szName)           /*do nothing*/
			#define PLATFORM_PROFILER_REGION_WAITING(szName)   /*do nothing*/
			#define PLATFORM_PROFILER_FUNCTION(szName)         /*do nothing*/
			#define PLATFORM_PROFILER_FUNCTION_WAITING(szName) /*do nothing*/
			#define PLATFORM_PROFILER_SECTION(szName)          /*do nothing*/
			#define PLATFORM_PROFILER_SECTION_WAITING(szName)  /*do nothing*/
			#define PLATFORM_PROFILER_MARKER(szName)           /*do nothing*/
			#define PLATFORM_PROFILER_PUSH(szLabel)            /*do nothing*/
			#define PLATFORM_PROFILER_POP(szLabel)             /*do nothing*/
		#endif

	#else

		#define PLATFORM_PROFILER_THREADNAME(szName)       /*do nothing*/
		#define PLATFORM_PROFILER_FRAMESTART(szName)       /*do nothing*/
		#define PLATFORM_PROFILER_REGION(szName)           /*do nothing*/
		#define PLATFORM_PROFILER_REGION_WAITING(szName)   /*do nothing*/
		#define PLATFORM_PROFILER_FUNCTION(szName)         /*do nothing*/
		#define PLATFORM_PROFILER_FUNCTION_WAITING(szName) /*do nothing*/
		#define PLATFORM_PROFILER_SECTION(szName)          /*do nothing*/
		#define PLATFORM_PROFILER_SECTION_WAITING(szName)  /*do nothing*/
		#define PLATFORM_PROFILER_MARKER(szLabel)          /*do nothing*/
		#define PLATFORM_PROFILER_PUSH(szLabel)            /*do nothing*/
		#define PLATFORM_PROFILER_POP(szLabel)             /*do nothing*/

	#endif

//! Deprecated Interface - do not use
namespace CryProfilePlatform
{
//////////////////////////////////////////////////////////////////////////
// class to define a profile scope, to represent time events in profile tools
class CScopedPlatformProfileMarker
{
	const char* m_name = nullptr;
public:
	inline CScopedPlatformProfileMarker(const EProfileDescription desc, const char* pName, ...) : m_name(pName)
	{
		va_list args;
		va_start(args, pName);

		// Format event name
		char markerName[256];
		const int cNumCharsNeeded = vsnprintf(markerName, CRY_ARRAY_COUNT(markerName), pName, args);
		if (cNumCharsNeeded > CRY_ARRAY_COUNT(markerName) - 1 || cNumCharsNeeded < 0)
		{
			markerName[CRY_ARRAY_COUNT(markerName) - 1] = '\0'; // The WinApi only null terminates if strLen < bufSize
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "ProfileEvent: Marker name \"%s\" has been truncated. Max characters allowed: %i. ", pName, CRY_ARRAY_COUNT(markerName) - 1);
		}

		// Set marker
		::CryProfile::detail::PushProfilingMarker(0, markerName);
		va_end(args);
	}
	inline ~CScopedPlatformProfileMarker() { ::CryProfile::detail::PopProfilingMarker(0,m_name); }
};

} // namespace CryProfile

// Util Macro to create scoped profiling markers
	#define CRYPROFILE_CONCAT_(a, b)              a ## b
	#define CRYPROFILE_CONCAT(a, b)               CRYPROFILE_CONCAT_(a, b)
	#define CRYPROFILE_SCOPE_PLATFORM_MARKER(...) CryProfilePlatform::CScopedPlatformProfileMarker CRYPROFILE_CONCAT(__scopedProfileMarker, __LINE__)(EProfileDescription::SECTION, __VA_ARGS__);

#else

	#define PLATFORM_PROFILER_THREADNAME(szName)       /*do nothing*/
	#define PLATFORM_PROFILER_FRAMESTART(szName)       /*do nothing*/
	#define PLATFORM_PROFILER_REGION(szName)           /*do nothing*/
	#define PLATFORM_PROFILER_REGION_WAITING(szName)   /*do nothing*/
	#define PLATFORM_PROFILER_FUNCTION(szName)         /*do nothing*/
	#define PLATFORM_PROFILER_FUNCTION_WAITING(szName) /*do nothing*/
	#define PLATFORM_PROFILER_SECTION(szName)          /*do nothing*/
	#define PLATFORM_PROFILER_SECTION_WAITING(szName)  /*do nothing*/
	#define PLATFORM_PROFILER_MARKER(szLabel)          /*do nothing*/
	#define PLATFORM_PROFILER_PUSH(szLabel)            /*do nothing*/
	#define PLATFORM_PROFILER_POP(szLabel)             /*do nothing*/

	#define CRYPROFILE_SCOPE_PLATFORM_MARKER(...)

#endif
