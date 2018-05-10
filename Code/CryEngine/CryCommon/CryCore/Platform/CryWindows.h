// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CryWindows.h
//  Version:     v1.00
//  Created:     02/05/2012 by James Chilvers.
//  Compilers:   Visual Studio.NET
//  Description: Specific header to handle Windows.h include
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#if CRY_PLATFORM_WINAPI

	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif

// Do not define min/max in windows.h
	#define NOMINMAX

// Prevents <Windows.h> from #including <Winsock.h>
// Manually define your <Winsock2.h> inclusion point elsewhere instead.
	#ifndef _WINSOCKAPI_
		#define _WINSOCKAPI_
	#endif

	#if defined(_WINDOWS_) && !defined(CRY_INCLUDE_WINDOWS_VIA_MFC_OR_ATL_INCLUDES)
		#error "<windows.h> has been included by other means than CryWindows.h"
	#endif

	#include <windows.h>

	#if !defined(CRY_SUPPRESS_CRYENGINE_WINDOWS_FUNCTION_RENAMING)
		#undef min
		#undef max
		#undef GetCommandLine
		#undef GetObject
		#undef PlaySound
		#undef GetClassName
		#undef DrawText
		#undef GetCharWidth
		#undef GetUserName
		#undef LoadLibrary
#if !defined(RESOURCE_COMPILER)
		#undef MessageBox // Disable usage of MessageBox, we want CryMessageBox to be used instead
#endif
	#endif

	#ifdef CRY_PLATFORM_DURANGO
		#include <CryCore/Platform/Durango_Win32Legacy.h>
	#endif

// In RELEASE disable OutputDebugString
	#if defined(_RELEASE) && !CRY_PLATFORM_DESKTOP && !defined(RELEASE_LOGGING)
		#undef OutputDebugString
		#define OutputDebugString(...) (void) 0
	#endif

#endif
