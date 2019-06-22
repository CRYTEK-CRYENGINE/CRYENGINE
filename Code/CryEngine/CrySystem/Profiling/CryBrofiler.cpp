// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryBrofiler.h"

#if ALLOW_BROFILER
#include "Cry_Brofiler.h"
#include <CryThreading/IThreadManager.h>

thread_local Profiler::ThreadDescription* tls_pDescription = nullptr;

CBrofiler* CBrofiler::s_pInstance = nullptr;

CBrofiler::CBrofiler()
{
	CRY_ASSERT_MESSAGE(s_pInstance == nullptr, "Multiple instantiation!");
	s_pInstance = this;
}

CBrofiler::~CBrofiler()
{
	s_pInstance = nullptr;
}

void CBrofiler::StartThread()
{
	CRY_ASSERT(tls_pDescription == nullptr);
	tls_pDescription = new Profiler::ThreadDescription(gEnv->pThreadManager->GetThreadName(CryGetCurrentThreadId()));
}

void CBrofiler::EndThread()
{
	CRY_ASSERT(tls_pDescription != nullptr);
	delete tls_pDescription;
}

void CBrofiler::DescriptionCreated(SProfilingSectionDescription* pDesc)
{
	pDesc->customData = (uintptr_t) Profiler::EventDescription::Create(pDesc->szEventname, pDesc->szFilename, pDesc->line, GenerateColorBasedOnName(pDesc->szEventname));
}

void CBrofiler::DescriptionDestroyed(SProfilingSectionDescription* pDesc)
{
	Profiler::EventDescription::DestroyEventDescription((Profiler::EventDescription*) pDesc->customData);
}

bool CBrofiler::StartSection(SProfilingSection* pSection)
{
	pSection->customData = (uintptr_t) Profiler::Event::Start(*(Profiler::EventDescription*) pSection->pDescription->customData);
	return true;
}

void CBrofiler::EndSection(SProfilingSection* pSection)
{
	if (pSection->customData)
		Profiler::Event::Stop(*(Profiler::EventData*) pSection->customData);
}

bool CBrofiler::StartSectionStatic(SProfilingSection* p)
{
	return s_pInstance->StartSection(p);
}

void CBrofiler::EndSectionStatic(SProfilingSection* p)
{
	s_pInstance->EndSection(p);
}

#endif