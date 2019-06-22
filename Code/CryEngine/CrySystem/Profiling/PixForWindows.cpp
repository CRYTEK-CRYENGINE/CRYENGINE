// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PixForWindows.h"

#include <CryThreading/IThreadManager.h>
#include <CryString/StringUtils.h>

// https://devblogs.microsoft.com/pix/winpixeventruntime/
#ifdef ENABLE_PROFILING_CODE
#	define USE_PIX
#endif
#include <WinPixEventRuntime/pix3.h>

void CPixForWindows::Stop() {}
bool CPixForWindows::IsStopped() const { return false; }

void CPixForWindows::PauseRecording(bool pause) {}
bool CPixForWindows::IsPaused() const { return false; }

void CPixForWindows::StartThread() 
{
	// Proper name is already set in CryThreadUtil::CrySetThreadName()
}
void CPixForWindows::EndThread() {}

void CPixForWindows::DescriptionCreated(SProfilingSectionDescription* pDesc)
{
	pDesc->color_argb = GenerateColorBasedOnName(pDesc->szEventname);
}

void CPixForWindows::DescriptionDestroyed(SProfilingSectionDescription*) {}

bool CPixForWindows::StartSection(SProfilingSection* pSection)
{
	return StartSectionStatic(pSection);
}

void CPixForWindows::EndSection(SProfilingSection* p)
{
	EndSectionStatic(p);
}

void CPixForWindows::RecordMarker(SProfilingMarker* pMarker)
{
	RecordMarkerStatic(pMarker);
}

bool CPixForWindows::StartSectionStatic(SProfilingSection* pSection)
{
	if(pSection->szDynamicName && pSection->szDynamicName[0])
		PIXBeginEvent(pSection->pDescription->color_argb, "%s\n%s", pSection->pDescription->szEventname, pSection->szDynamicName);
	else
		PIXBeginEvent(pSection->pDescription->color_argb, pSection->pDescription->szEventname);
	return true;
}

void CPixForWindows::EndSectionStatic(SProfilingSection*)
{
	PIXEndEvent();
}

void CPixForWindows::RecordMarkerStatic(SProfilingMarker* pMarker)
{
	PIXSetMarker(pMarker->pDescription->color_argb, pMarker->pDescription->szMarkername);
}

void CPixForWindows::StartFrame()
{
	PIXBeginEvent(PIX_COLOR_INDEX(0), "Frame %d", gEnv->nMainFrameID);
}

void CPixForWindows::EndFrame()
{
	PIXEndEvent();
}

void CPixForWindows::RegisterCVars() {}
