// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PlatformProfiler.h"

#if CRY_PLATFORM_DURANGO && ALLOW_PLATFORM_PROFILER

#define USE_PIX
#include <pix.h>
#pragma comment(lib, "pixEvt")

CPlatformProfiler* CPlatformProfiler::s_pInstance = nullptr;

CPlatformProfiler::CPlatformProfiler()
{
	s_pInstance = this;
}

CPlatformProfiler::~CPlatformProfiler()
{
	s_pInstance = nullptr;
}

bool CPlatformProfiler::StartSection(SProfilingSection* pSection)
{
	PIXBeginEvent(pSection->pDescription->color_argb, L"%hs", pSection->pDescription->szEventname);
	return true;
}

void CPlatformProfiler::EndSection(SProfilingSection*)
{
	PIXEndEvent();
}

void CPlatformProfiler::RecordMarker(SProfilingMarker* pMarker)
{
	PIXSetMarker(pMarker->pDescription->color_argb, L"%hs", pMarker->pDescription->szMarkername);
}

void CPlatformProfiler::PauseRecording(bool pause) {}
void CPlatformProfiler::StartThread() {}
void CPlatformProfiler::StartFrame() {}
void CPlatformProfiler::EndFrame() {}

#endif