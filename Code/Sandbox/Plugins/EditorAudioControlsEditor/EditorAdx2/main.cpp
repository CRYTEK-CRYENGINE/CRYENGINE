// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Impl.h"

#include <CryCore/Platform/platform.h>
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/ISystem.h>

ACE::Impl::Adx2::CImpl* g_pAdx2Interface;

//------------------------------------------------------------------
extern "C" ACE_API ACE::Impl::IImpl* GetAudioInterface(ISystem * const pSystem)
{
	ModuleInitISystem(pSystem, "EditorAdx2");

	if (g_pAdx2Interface == nullptr)
	{
		g_pAdx2Interface = new ACE::Impl::Adx2::CImpl();
	}

	return g_pAdx2Interface;
}

//------------------------------------------------------------------
HINSTANCE g_hInstance = 0;
BOOL __stdcall DllMain(HINSTANCE hinstDLL, ULONG fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		{
			g_hInstance = hinstDLL;
			break;
		}

	case DLL_PROCESS_DETACH:
		{
			if (g_pAdx2Interface != nullptr)
			{
				delete g_pAdx2Interface;
				g_pAdx2Interface = nullptr;
			}

			break;
		}
	default:
		{
			break;
		}
	}

	return TRUE;
}
