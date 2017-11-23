// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

static HINSTANCE g_hInstance = 0;

// Force MFC to use this DllMain.
#ifdef _USRDLL
extern "C" { int __afxForceUSRDLL; }
#endif

// NOTE pavloi 2017.01.24: we have to implement DllMain to obtain HINSTANCE of this DLL.
BOOL WINAPI DllMain(HINSTANCE hinstDLL, ULONG fdwReason, LPVOID lpvReserved)
{
	if(fdwReason == DLL_PROCESS_ATTACH)
	{
		g_hInstance = hinstDLL;
	}
	return TRUE;
}


HINSTANCE GetHInstance()
{
	return g_hInstance;
}
