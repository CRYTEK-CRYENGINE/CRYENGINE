// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Platform/platform.h>

#include <CrySystem/ISystem.h>
#include <CryThreading/IJobManager.h>

#include "FrameProfiler_Shared.h"

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
// Profiling is enabled in every configuration except Release

//! Public Interface for use of Profile Marker.
namespace CryProfile
{
//////////////////////////////////////////////////////////////////////////
// class to define a profile scope, to represent time events in profile tools
class CScopedProfileMarker
{
	EProfileDescription m_desc;
	const char* m_name;
public:
	inline CScopedProfileMarker(const EProfileDescription desc, const char* pName, ...) : m_desc(desc), m_name(pName)
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
		CryProfile::PushProfilingMarker(desc, markerName);
		va_end(args);
	}
	inline ~CScopedProfileMarker() { CryProfile::PopProfilingMarker(m_desc,m_name); }
};

} // namespace CryProfile

// Util Macro to create scoped profiling markers
	#define CRYPROFILE_CONCAT_(a, b)             a ## b
	#define CRYPROFILE_CONCAT(a, b)              CRYPROFILE_CONCAT_(a, b)
	#define CRYPROFILE_SCOPE_PROFILE_MARKER(...) CryProfile::CScopedProfileMarker CRYPROFILE_CONCAT(__scopedProfileMarker, __LINE__)(EProfileDescription::SECTION, __VA_ARGS__);

#else

	#define CRYPROFILE_SCOPE_PROFILE_MARKER(...)

#endif
