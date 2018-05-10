// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once
#include <CryCore/Platform/platform.h>

#define CRY_USE_ATL
#include <CryCore/Platform/CryAtlMfc.h>

#include <CryMath/Cry_Math.h>
#include <CryString/CryPath.h>

#include <stdio.h>
#include <tchar.h>

#include <vector>
#include <map>

extern HMODULE g_hInst;



#ifndef ReleasePpo
#define ReleasePpo(ppo) \
	if (*(ppo) != NULL) \
		{ \
		(*(ppo))->Release(); \
		*(ppo) = NULL; \
		} \
		else (VOID)0
#endif

// TODO: reference additional headers your program requires here
