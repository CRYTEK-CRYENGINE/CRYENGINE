// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if CRY_PLATFORM_ORBIS && ALLOW_PLATFORM_PROFILER

#include "StdAfx.h"
#include "PlatformProfiler.h"

#include <CrySystem/ISystem.h>
#include <perf.h>

uint32 COLOR_TO_SCE_RAZOR_COLOR(uint32 color_argb)
{
	return 0x80000000 // turn into 0x80'BB'GG'RR
		|  (color_argb & 0x0000FF00)
		| ((color_argb & 0x00FF0000) >> 16)
		| ((color_argb & 0x000000FF) << 16);
}

CPlatformProfiler* CPlatformProfiler::s_pInstance = nullptr;

CPlatformProfiler::CPlatformProfiler()
{
	CRY_ASSERT_MESSAGE(gEnv->bPS4DevKit, "Orbis platform profiler only supported on dev kit!");
	s_pInstance = this;
}

CPlatformProfiler::~CPlatformProfiler()
{
	s_pInstance = nullptr;
}

bool CPlatformProfiler::StartSection(SProfilingSection* pSection)
{
	sceRazorCpuPushMarker(pSection->pDescription->szEventname, COLOR_TO_SCE_RAZOR_COLOR(pSection->pDescription->color_argb), 0);
	return true;
}

void CPlatformProfiler::EndSection(SProfilingSection*)
{
	sceRazorCpuPopMarker();
}

void CPlatformProfiler::RecordMarker(SProfilingMarker* pMarker)
{
	sceRazorCpuPushMarker(pMarker->pDescription->szMarkername, COLOR_TO_SCE_RAZOR_COLOR(pMarker->pDescription->color_argb), 0);
	sceRazorCpuPopMarker();
}

void CPlatformProfiler::PauseRecording(bool) {}
void CPlatformProfiler::StartThread() {}
void CPlatformProfiler::StartFrame() {}
void CPlatformProfiler::EndFrame() {}

#endif