// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EditorImpl.h"

#include <CryCore/Platform/platform.h>
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/ISystem.h>

ACE::Wwise::CEditorImpl* g_pWwiseInterface;

//------------------------------------------------------------------
extern "C" ACE_API ACE::IEditorImpl * GetAudioInterface(ISystem * pSystem)
{
	ModuleInitISystem(pSystem, "EditorWwise");

	if (g_pWwiseInterface == nullptr)
	{
		g_pWwiseInterface = new ACE::Wwise::CEditorImpl();
	}

	return g_pWwiseInterface;
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
		if (g_pWwiseInterface != nullptr)
		{
			delete g_pWwiseInterface;
			g_pWwiseInterface = nullptr;
		}
		break;
	}
	return TRUE;
}
