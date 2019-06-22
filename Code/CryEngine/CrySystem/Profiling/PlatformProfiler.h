// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CryProfilingSystemSharedImpl.h"

// add all implemented platforms here
#if ALLOW_PLATFORM_PROFILER && (CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS)
#	define USE_PLATFORM_PROFILER 1
#else
#	define USE_PLATFORM_PROFILER 0
#endif

#if USE_PLATFORM_PROFILER

class CPlatformProfiler : public CCryProfilingSystemImpl
{
public:
	CPlatformProfiler();
	~CPlatformProfiler();

	void Stop() final {}
	bool IsStopped() const final { return false; }

	void PauseRecording(bool pause) final;
	bool IsPaused() const final { return m_paused; }

	virtual void StartThread() final;
	virtual void EndThread() final {}

	void DescriptionCreated(SProfilingSectionDescription* pDesc) final { pDesc->color_argb = GenerateColorBasedOnName(pDesc->szEventname); }
	void DescriptionDestroyed(SProfilingSectionDescription*) final {}

	bool StartSection(SProfilingSection*) final;
	void EndSection(SProfilingSection*) final;

	void StartFrame() final;
	void EndFrame() final;

	void RecordMarker(SProfilingMarker*) final;

	void RegisterCVars() final {}

	static bool StartSectionStatic(SProfilingSection* p) { return s_pInstance->StartSection(p);}
	static void EndSectionStatic(SProfilingSection* p) { s_pInstance->EndSection(p); }
	static void RecordMarkerStatic(SProfilingMarker* p) { s_pInstance->RecordMarker(p); }

private:
	static CPlatformProfiler* s_pInstance;

	bool m_paused = false;
};

#endif