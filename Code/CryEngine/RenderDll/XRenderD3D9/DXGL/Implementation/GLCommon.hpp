// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GLCommon.hpp
//  Version:     v1.00
//  Created:     05/03/2013 by Valerio Guagliumi.
//  Description: Declares the types and functions that implement the
//               OpenGL rendering functionality of DXGL
// -------------------------------------------------------------------------
//  History:
//   - 27/03/2014: Renamed from CryOpenGLImpl.hpp,
//                 moved CryEngine dependencies to GLCryPlatform.hpp
//
////////////////////////////////////////////////////////////////////////////

#ifndef __GLCOMMON__
#define __GLCOMMON__

#include "GLPlatform.hpp"

#define DXGL_VERSION_32              320 /* DX 10.1 10.2 */
#define DXGL_VERSION_41              410
#define DXGL_VERSION_42              420
#define DXGL_VERSION_43              430 /* DX 11 */
#define DXGL_VERSION_44              440
#define DXGL_VERSION_45              450

#define DXGLES_VERSION_30            300
#define DXGLES_VERSION_31            310

#define DXGL_USE_ADRENO_ES_EMULATOR  0
#define DXGL_USE_POWERVR_ES_EMULATOR 0
#define DXGL_USE_ES_EMULATOR         (DXGL_USE_ADRENO_ES_EMULATOR || DXGL_USE_POWERVR_ES_EMULATOR)

// Set to 1 to enable NVidia's Tegra-specific extensions for Android
#define DXGL_SUPPORT_TEGRA 1

#if CRY_PLATFORM_MAC
	#define DXGL_REQUIRED_VERSION   DXGL_VERSION_41
	#define DXGLES_REQUIRED_VERSION 0
#elif CRY_PLATFORM_ANDROID
	#define DXGL_USE_GLAD
	#if defined(DXGL_ANDROID_GL)
// Android with OpenGL 4
		#define DXGL_REQUIRED_VERSION   DXGL_VERSION_44
		#define DXGLES_REQUIRED_VERSION 0
	#else
		#define DXGL_REQUIRED_VERSION   0
		#define DXGLES_REQUIRED_VERSION DXGLES_VERSION_31
	#endif
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_WINDOWS
	#if CRY_RENDERER_OPENGLES
		#define DXGL_USE_GLAD
//		#define DXGL_ES_SUBSET
		#define DXGLES_REQUIRED_VERSION DXGLES_VERSION_31
		#define DXGL_REQUIRED_VERSION   0
	#else
		#define DXGL_USE_GLAD
		#define DXGL_REQUIRED_VERSION   DXGL_VERSION_44
		#define DXGLES_REQUIRED_VERSION 0
	#endif
#elif CRY_PLATFORM_IOS
	#define DXGL_REQUIRED_VERSION   0
	#define DXGLES_REQUIRED_VERSION DXGLES_VERSION_30
#endif

#define DXGLES DXGLES_REQUIRED_VERSION > 0

#if CRY_PLATFORM_WINDOWS
	#include <WinGDI.h>
#endif

#if CRY_PLATFORM_MOBILE && defined(USE_SDL2_VIDEO)
	#define DXGL_SINGLEWINDOW
#endif

#define DXGL_NSIGHT_VERSION_3_0          30
#define DXGL_NSIGHT_VERSION_3_1          31
#define DXGL_NSIGHT_VERSION_3_2          32
#define DXGL_NSIGHT_VERSION_4_0          40
#define DXGL_NSIGHT_VERSION_4_1          41
#define DXGL_NSIGHT_VERSION_4_5          45

#define DXGL_SUPPORT_NSIGHT_VERSION      0
#define DXGL_SUPPORT_APITRACE            0
#define DXGL_SUPPORT_VOGL                0
#define DXGL_GLSL_FROM_HLSLCROSSCOMPILER 1
// Uncomment to enable temporary fix for AMD drivers with a fixed number of texture units
// #define DXGL_MAX_TEXTURE_UNITS 32

#define DXGL_SUPPORT_NSIGHT(_Version)       (DXGL_SUPPORT_NSIGHT_VERSION == DXGL_NSIGHT_VERSION_ ## _Version)
#define DXGL_SUPPORT_NSIGHT_SINCE(_Version) (DXGL_SUPPORT_NSIGHT_VERSION && DXGL_SUPPORT_NSIGHT_VERSION <= DXGL_NSIGHT_VERSION_ ## _Version)

#define DXGL_TRACE_CALLS       0
#define DXGL_TRACE_CALLS_FLUSH 0
#define DXGL_CHECK_ERRORS      0

#include "GLFeatures.hpp"

namespace NCryOpenGL
{

#if DXGL_USE_ES_EMULATOR
struct SDisplayConnection;
typedef _smart_ptr<SDisplayConnection> TWindowContext;
typedef EGLContext                     TRenderingContext;
#elif defined(USE_SDL2_VIDEO)
typedef SDL_Window*                    TWindowContext;
typedef SDL_GLContext                  TRenderingContext;
#elif CRY_PLATFORM_WINDOWS
typedef HDC                            TWindowContext;
typedef HGLRC                          TRenderingContext;
#else
	#error "Platform specific context not defined"
#endif

}

#endif //__GLCOMMON__
