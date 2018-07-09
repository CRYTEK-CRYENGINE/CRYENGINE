// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Impl.h"

#include <CryCore/Platform/platform.h>
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/ISystem.h>

ACE::Impl::SDLMixer::CImpl* g_pSdlMixerInterface;

//------------------------------------------------------------------
extern "C" ACE_API ACE::Impl::IImpl * GetAudioInterface(ISystem * pSystem)
{
	ModuleInitISystem(pSystem, "EditorSDLMixer");

	if (g_pSdlMixerInterface == nullptr)
	{
		g_pSdlMixerInterface = new ACE::Impl::SDLMixer::CImpl();
	}

	return g_pSdlMixerInterface;
}

//------------------------------------------------------------------
HINSTANCE g_hInstance = 0;
BOOL __stdcall DllMain(HINSTANCE hinstDLL, ULONG fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		g_hInstance = hinstDLL;
		break;
	case DLL_PROCESS_DETACH:
		if (g_pSdlMixerInterface != nullptr)
		{
			delete g_pSdlMixerInterface;
			g_pSdlMixerInterface = nullptr;
		}
		break;
	}
	return TRUE;
}

