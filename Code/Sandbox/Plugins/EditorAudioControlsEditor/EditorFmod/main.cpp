// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EditorImpl.h"

#include <CryCore/Platform/platform.h>
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/ISystem.h>

ACE::Fmod::CEditorImpl* g_pFmodInterface;

//------------------------------------------------------------------
extern "C" ACE_API ACE::IEditorImpl * GetAudioInterface(ISystem * pSystem)
{
	ModuleInitISystem(pSystem, "EditorFmod");

	if (g_pFmodInterface == nullptr)
	{
		g_pFmodInterface = new ACE::Fmod::CEditorImpl();
	}

	return g_pFmodInterface;
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
		if (g_pFmodInterface != nullptr)
		{
			delete g_pFmodInterface;
			g_pFmodInterface = nullptr;
		}
		break;
	}
	return TRUE;
}
