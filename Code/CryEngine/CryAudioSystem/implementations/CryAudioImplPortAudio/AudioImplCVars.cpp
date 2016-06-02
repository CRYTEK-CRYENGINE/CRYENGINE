// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioImplCVars.h"
#include <CrySystem/IConsole.h>

using namespace CryAudio::Impl::PortAudio;

//////////////////////////////////////////////////////////////////////////
CAudioImplCVars::CAudioImplCVars()
	: m_primaryMemoryPoolSize(0)
{
}

//////////////////////////////////////////////////////////////////////////
CAudioImplCVars::~CAudioImplCVars()
{
}

//////////////////////////////////////////////////////////////////////////
void CAudioImplCVars::RegisterVariables()
{
#if CRY_PLATFORM_WINDOWS
	m_primaryMemoryPoolSize = 128 << 10;  // 128 MiB
#else
	#error "Undefined platform."
#endif

	REGISTER_CVAR2("s_PortAudioPrimaryPoolSize", &m_primaryMemoryPoolSize, m_primaryMemoryPoolSize, VF_REQUIRE_APP_RESTART,
	               "Specifies the size (in KiB) of the memory pool to be used by the PortAudio implementation.\n"
	               "Usage: s_PortAudioPrimaryPoolSize [0/...]\n"
	               "Default PC: 131072 (128 MiB)\n");
}

//////////////////////////////////////////////////////////////////////////
void CAudioImplCVars::UnregisterVariables()
{
	IConsole* const pConsole = gEnv->pConsole;

	if (pConsole != nullptr)
	{
		pConsole->UnregisterVariable("s_PortAudioPrimaryPoolSize");
	}
}
