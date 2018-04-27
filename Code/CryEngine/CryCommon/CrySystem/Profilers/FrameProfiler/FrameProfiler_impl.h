// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FrameProfiler_impl.pc.h
//  Version:     v1.00
//  Created:     11/12/2002 by Christopher Bolte.
//  Compilers:   Visual Studio.NET
//  Description: Platform Profiling Marker Implementation, dispatches to the correct header file
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#if !defined(CRY_PROFILE_MARKER_IMPL_H_)
#define CRY_PROFILE_MARKER_IMPL_H_

#include <CrySystem/Profilers/FrameProfiler/FrameProfiler.h>
#include <CryThreading/IJobManager.h>

//! A list of colors which are used to distinguish between different profile label types.
ColorF profile_colors[] =
{
	// When modifying this array, always adjust EProfileDescription

	Col_Red,            //!< 0: Undefined

#if ALLOW_BROFILER
	Col_Transparent,    //!< 1: FunctionEntry - do not show in hierarchical view
	Col_Transparent,    //!< 2: Section - do not show in hierarchical view
#else
	Col_Orchid,         //!< 1: FunctionEntry
	Col_Coral,          //!< 2: Section
#endif
	Col_SkyBlue,        //!< 3: Region

	Col_Transparent,    //!< 4: Undefined

#if ALLOW_BROFILER
	Col_Transparent,    //!< 5: FunctionEntry - Waiting - do not show in hierarchical view
	Col_Transparent,    //!< 6: Section - Waiting - do not show in hierarchical view
#else
	Col_Yellow,         //!< 5: FunctionEntry - Waiting
	Col_Yellow,         //!< 6: Section - Waiting
#endif
	Col_Yellow,         //!< 7: Region - Waiting

	// Do not add any other labels - color lookup is defined as byte
};

#if (!defined(_LIB) || defined(_LAUNCHER)) && defined(ENABLE_PROFILING_CODE) && !((defined(SANDBOX_EXPORTS) || defined(PLUGIN_EXPORTS)))
	#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
		#include <CrySystem/Profilers/FrameProfiler/FrameProfiler_impl.pc.h>
	#elif CRY_PLATFORM_DURANGO
		#include <CrySystem/Profilers/FrameProfiler/FrameProfiler_impl.durango.h>
	#elif CRY_PLATFORM_ORBIS
		#include <CrySystem/Profilers/FrameProfiler/FrameProfiler_impl.orbis.h>
	#else
		#error No Platform support for Profile Marker
	#endif
#else
/* inline */ void CryProfile::detail::SetThreadName(const char* pName)                           {}
/* inline */ void CryProfile::detail::SetProfilingEvent(const BYTE colorId, const char* Name)    {}
/* inline */ void CryProfile::detail::PushProfilingMarker(const BYTE colorId, const char* pName) {}
/* inline */ void CryProfile::detail::PopProfilingMarker(const BYTE colorId, const char* pName)  {}
/* inline */ void CryProfile::detail::ProfilerPause()                                            {}
/* inline */ void CryProfile::detail::ProfilerResume()                                           {}
/* inline */ void CryProfile::detail::ProfilerFrameStart(int nFrameId)                           {}
/* inline */ void CryProfile::detail::ProfilerFrameEnd(int nFrameId)                             {}
#endif

//////////////////////////////////////////////////////////////////////////
//! Direct push/pop for profiling markers.
void CryProfile::PushProfilingMarker(const EProfileDescription desc, const char* pName, ...)
{
	va_list args;
	va_start(args, pName);

	// Format marker name
	char markerName[256];
	if (strlen(pName) == 0)
		__debugbreak();
	const int cNumCharsNeeded = vsnprintf(markerName, CRY_ARRAY_COUNT(markerName), pName, args);
	if (cNumCharsNeeded > CRY_ARRAY_COUNT(markerName) - 1 || cNumCharsNeeded < 0)
	{
		markerName[CRY_ARRAY_COUNT(markerName) - 1] = '\0'; // The WinApi only null terminates if strLen < bufSize
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "ProfileEvent: Marker name \"%s\" has been truncated. Max characters allowed: %i. ", pName, CRY_ARRAY_COUNT(markerName) - 1);
	}

	// Set marker
	gEnv->pJobManager->PushProfilingMarker(markerName);
	va_end(args);
}

//////////////////////////////////////////////////////////////////////////
void CryProfile::PopProfilingMarker(const EProfileDescription desc, const char* pName)
{
	gEnv->pJobManager->PopProfilingMarker();
}

#endif // CRY_PROFILE_MARKER_IMPL_H_
