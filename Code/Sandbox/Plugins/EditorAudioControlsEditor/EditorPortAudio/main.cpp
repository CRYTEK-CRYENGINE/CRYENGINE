// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioSystemEditor.h"
#include <CryCore/Platform/platform.h>
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/ISystem.h>

using namespace ACE;

CAudioSystemEditor* g_pInterface;

//------------------------------------------------------------------
extern "C" ACE_API IAudioSystemEditor * GetAudioInterface(ISystem * pSystem)
{
	ModuleInitISystem(pSystem, "EditorPortAudio");
	if (!g_pInterface)
	{
		g_pInterface = new CAudioSystemEditor();
	}
	return g_pInterface;
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
		if (g_pInterface)
		{
			delete g_pInterface;
			g_pInterface = nullptr;
		}
		break;
	}
	return TRUE;
}
