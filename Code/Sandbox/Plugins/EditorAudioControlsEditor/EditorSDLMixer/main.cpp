// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EditorImpl.h"

#include <CryCore/Platform/platform.h>
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/ISystem.h>

ACE::SDLMixer::CEditorImpl* g_pSdlMixerInterface;

//------------------------------------------------------------------
extern "C" ACE_API ACE::IEditorImpl * GetAudioInterface(ISystem * pSystem)
{
	ModuleInitISystem(pSystem, "EditorSDLMixer");

	if (g_pSdlMixerInterface == nullptr)
	{
		g_pSdlMixerInterface = new ACE::SDLMixer::CEditorImpl();
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
