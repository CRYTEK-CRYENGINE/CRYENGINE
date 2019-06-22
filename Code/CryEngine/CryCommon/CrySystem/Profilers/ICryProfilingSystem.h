// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Cry_Math.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/ITimer.h>

struct SProfilingSectionDescription;
struct SProfilingSection;
struct SProfilingMarker;
typedef int32 TProfilingCount;
typedef int64 TProfilingValue;

enum EProfiledSubsystem : uint8
{
	PROFILE_RENDERER,
	PROFILE_3DENGINE,
	PROFILE_PARTICLE,
	PROFILE_AI,
	PROFILE_ANIMATION,
	PROFILE_MOVIE,
	PROFILE_ENTITY,
	PROFILE_FONT,
	PROFILE_NETWORK,
	PROFILE_PHYSICS,
	PROFILE_SCRIPT,
	PROFILE_SCRIPT_CFUNC,
	PROFILE_AUDIO,
	PROFILE_EDITOR,
	PROFILE_SYSTEM,
	PROFILE_ACTION,
	PROFILE_GAME,
	PROFILE_INPUT,
	PROFILE_LOADING_ONLY, //!< Deprecated

	PROFILE_LAST_SUBSYSTEM //!< Must always be last.
};

struct ICryProfilingSystem
{
	//! end the profiling session
	virtual void Stop() = 0;
	virtual bool IsStopped() const = 0;

	virtual void PauseRecording(bool pause) = 0;
	//! Are we collecting profiling data?
	virtual bool IsPaused() const = 0;

	bool IsRunning() const { return !(IsStopped() || IsPaused()); };

	virtual void StartThread() = 0;
	virtual void EndThread() = 0;

	virtual void DescriptionCreated(SProfilingSectionDescription*) = 0;
	virtual void DescriptionDestroyed(SProfilingSectionDescription*) = 0;

	virtual bool StartSection(SProfilingSection*) = 0;
	virtual void EndSection(SProfilingSection*) = 0;

	virtual void RecordMarker(SProfilingMarker*) = 0;

	virtual void StartFrame() = 0;
	virtual void EndFrame() = 0;
	//! Must be called when something quacks like the end of the frame.
	virtual void OnSliceAndSleep() = 0;

	virtual const char* GetModuleName(EProfiledSubsystem) const = 0;
	virtual const char* GetModuleName(const SProfilingSection*) const = 0;

	//! get time lost to profiling in milliseconds, may return 0, if not measured
	virtual float GetProfilingTimeCost() const = 0;
	virtual float GetAverageProfilingTimeCost() const = 0;

protected:
	~ICryProfilingSystem() = default;
};

//! Description of a code section we're profiling.
struct SProfilingSectionDescription
{
	SProfilingSectionDescription(const char* szFilename, const char* szEventname, uint16 line, bool isWaiting, EProfiledSubsystem subsystem)
		: szFilename(szFilename), szEventname(szEventname), line(line), isWaiting(isWaiting), subsystem(subsystem)
		, color_argb(0), customData(0)
	{
		if(gEnv && gEnv->pSystem)
			gEnv->pSystem->GetProfilingSystem()->DescriptionCreated(this);
	}

	~SProfilingSectionDescription()
	{
		if (gEnv && gEnv->pSystem)
			gEnv->pSystem->GetProfilingSystem()->DescriptionDestroyed(this);
	}

	const char* szFilename;
	const char* szEventname;
	uint16 line;
	bool isWaiting;
	EProfiledSubsystem subsystem;
	//! color stored with 1B per channel
	uint32 color_argb;

	//! can be used by a profiling system to attach its own information
	mutable uintptr_t customData;
};

struct SProfilingSection
{
	SProfilingSection(const SProfilingSectionDescription* pDescription, const char* szDynamicName)
		: pDescription(pDescription), szDynamicName(szDynamicName), childExcludeValue(0), customData(0) // startValue is set by the profiling system
	{
		wasStarted = gEnv->startProfilingSection(this);
	}

	~SProfilingSection()
	{
		if(wasStarted)
			gEnv->endProfilingSection(this);
	}

	const SProfilingSectionDescription* pDescription;
	//! optional description of the specific instance, e.g. parameters of called function
	const char* szDynamicName;

	TProfilingValue startValue;
	TProfilingValue childExcludeValue;

	//! can be used by a profiling system to attach its own information
	uintptr_t customData;
	bool wasStarted;
};

struct SProfilingMarkerDescription
{
	const char* szFilename;
	const char* szMarkername;
	uint32 line;
	EProfiledSubsystem subsystem;
	//! color stored with 1B per channel
	uint32 color_argb;
};

struct SProfilingMarker
{
	SProfilingMarker(SProfilingMarkerDescription* pDescription) :
		pDescription(pDescription), threadId(CryGetCurrentThreadId())
	{
		gEnv->recordProfilingMarker(this);
	}

	SProfilingMarkerDescription* pDescription;
	threadID threadId;
};

#ifdef ENABLE_PROFILING_CODE

#define CRYPROF_CAT_(a, b) a ## b
#define CRYPROF_CAT(a, b)  CRYPROF_CAT_(a, b)

#define CRY_PROFILE_THREADSTART if(gEnv->pSystem) gEnv->pSystem->GetProfilingSystem()->StartThread()
#define CRY_PROFILE_THREADEND   if(gEnv->pSystem) gEnv->pSystem->GetProfilingSystem()->EndThread()

#define CRY_PROFILE_SECTION_FULL(subsystem, szName, szArg, isWaiting) \
	static_assert(szName != nullptr, "Only use string literals as the name of a profiling section!"); \
	static_assert(subsystem >= EProfiledSubsystem(0) || true, "The subsystem cannot be set dynamically!"); \
	static_assert(isWaiting || true, "The waiting status cannot be set dynamically!"); \
	const static SProfilingSectionDescription CRYPROF_CAT(profEventDesc, __LINE__) (__FILE__, szName, __LINE__, isWaiting, subsystem); \
	SProfilingSection CRYPROF_CAT(profSection, __LINE__)(&CRYPROF_CAT(profEventDesc, __LINE__), szArg);

#define CRY_PROFILE_FUNCTION(subsystem)               CRY_PROFILE_SECTION_FULL(subsystem, __FUNC__, nullptr, false)
#define CRY_PROFILE_FUNCTION_ARG(subsystem, argument) CRY_PROFILE_SECTION_FULL(subsystem, __FUNC__, argument, false)
#define CRY_PROFILE_FUNCTION_WAITING(subsystem)       CRY_PROFILE_SECTION_FULL(subsystem, __FUNC__, nullptr, true)

#define CRY_PROFILE_SECTION(subsystem, szName)            CRY_PROFILE_SECTION_FULL(subsystem, szName, nullptr, false)
#define CRY_PROFILE_SECTION_ARG(subsystem, szName, szArg) CRY_PROFILE_SECTION_FULL(subsystem, szName, szArg, false)
#define CRY_PROFILE_SECTION_WAITING(subsystem, szName)    CRY_PROFILE_SECTION_FULL(subsystem, szName, nullptr, true)

#define CRY_PROFILE_MARKER(SYS, PNAME) \
	static SProfilingMarkerDescription CRYPROF_CAT(profMarkerDesc, __LINE__) = {__FILE__, PNAME, __LINE__, SYS, 0}; \
	SProfilingMarker CRYPROF_CAT(profMarker, __LINE__)(&CRYPROF_CAT(profMarkerDesc, __LINE__));

#else

#define CRY_PROFILE_THREADSTART
#define CRY_PROFILE_THREADEND

#define CRY_PROFILE_FUNCTION(subsystem)
#define CRY_PROFILE_FUNCTION_ARG(subsystem, argument)
#define CRY_PROFILE_FUNCTION_WAITING(subsystem)

#define CRY_PROFILE_SECTION(subsystem, szName)
#define CRY_PROFILE_SECTION_ARG(subsystem, szName, argument)
#define CRY_PROFILE_SECTION_WAITING(subsystem, szName)

#define CRY_PROFILE_MARKER(SYS, PNAME)

#endif

#ifdef ENABLE_LOADING_PROFILER
struct SBootprofilerAutoSession
{
	SBootprofilerAutoSession(const char* szName) { if (gEnv->pSystem) gEnv->pSystem->StartBootProfilerSession(szName); }
	~SBootprofilerAutoSession() { if (gEnv->pSystem) gEnv->pSystem->EndBootProfilerSession(); }
};
#	define LOADING_TIME_PROFILE_AUTO_SESSION(NAME) SBootprofilerAutoSession __bootProfSession(NAME)
#else
#	define LOADING_TIME_PROFILE_AUTO_SESSION(NAME)
#endif
