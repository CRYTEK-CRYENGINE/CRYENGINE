// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _CONFIG_SCALEFORM_H_
#define _CONFIG_SCALEFORM_H_

#pragma once

#ifdef INCLUDE_SCALEFORM_SDK
#include <CryCore/Platform/CryWindows.h>
#include <GConfig.h>   // Requires <windows.h> because of WIN32 definition in GConfigAddon.h (and potentially other Scaleform files)
#include <CryCore/Project/ProjectDefines.h>


/* *********************************************************************
* ENABLE_GFX_VIDEO disabled by default
* If needed, contact CryENGINE Support to get latest gfxvideo libs
* and uncomment this line
* ******************************************************************* */
//#define ENABLE_GFX_VIDEO

// Enable Scaleform IME implementation
// Note: This requires linking against Scaleform IME plugin library
#define ENABLE_GFX_IME

#if defined(ENABLE_GFX_VIDEO) && !defined(EXCLUDE_SCALEFORM_VIDEO)
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
#define USE_GFX_VIDEO
#elif CRY_PLATFORM_DURANGO
#define USE_GFX_VIDEO
#endif
#endif

#if defined(ENABLE_GFX_IME) && !defined(EXCLUDE_SCALEFORM_IME)
#if CRY_PLATFORM_WINDOWS
#define USE_GFX_IME
#endif
#endif

#if (CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_MAC) && !defined(GFC_BUILD_SHIPPING)
#define USE_GFX_JPG
#define USE_GFX_PNG
#endif

#if !defined(GFC_USE_LIBJPEG) && defined(USE_GFX_JPG)
#undef USE_GFX_JPG
#endif

#if !defined(GFC_USE_LIBPNG) && defined(USE_GFX_PNG)
#undef USE_GFX_PNG
#endif

#if !defined(GFC_USE_IME) && defined(USE_GFX_IME)
#undef USE_GFX_IME
#endif

#endif // #ifdef INCLUDE_SCALEFORM_SDK

#endif // #ifndef _CONFIG_SCALEFORM_H_
