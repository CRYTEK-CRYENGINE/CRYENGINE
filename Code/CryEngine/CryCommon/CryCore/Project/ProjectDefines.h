// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef PROJECTDEFINES_H
#define PROJECTDEFINES_H

#include <CryCore/BaseTypes.h>

#if defined(_RELEASE) && !defined(RELEASE)
	#define RELEASE
#endif

// This was chewing up a lot of CPU time just waiting for a connection
#define NO_LIVECREATE

// Scaleform base configuration
#if defined(DEDICATED_SERVER)
	#undef INCLUDE_SCALEFORM_SDK   // Not used in dedicated server
	#undef CRY_FEATURE_SCALEFORM_HELPER
#endif
#if CRY_PLATFORM_MOBILE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	#undef INCLUDE_SCALEFORM_VIDEO // Not currently supported on these platforms
#endif

#if CRY_PLATFORM_DURANGO
	#if !defined(_RELEASE)
		#define ENABLE_STATS_AGENT
	#endif
#elif CRY_PLATFORM_WINDOWS
	#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
		#define ENABLE_STATS_AGENT
	#endif
#endif

// Durango SDK and Orbis SDK are 64-bit only
#if !(CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT)
	#undef TOOLS_SUPPORT_DURANGO
	#undef TOOLS_SUPPORT_ORBIS
#endif

// Type used for vertex indices
#if defined(RESOURCE_COMPILER)
typedef uint32 vtx_idx;
#elif CRY_PLATFORM_MOBILE
typedef uint16 vtx_idx;
#else
// Uncomment one of the two following typedefs:
typedef uint32 vtx_idx;
//typedef uint16 vtx_idx;
#endif

//! 0=off, 1=on
//! \see http://wiki/bin/view/CryEngine/TerrainTexCompression
#define TERRAIN_USE_CIE_COLORSPACE 0

// For mobiles and consoles every bit of memory is important so files for documentation purpose are excluded
// they are part of regular compiling to verify the interface.
#if !CRY_PLATFORM_DESKTOP
	#define EXCLUDE_DOCUMENTATION_PURPOSE
#endif

//! When non-zero, const cvar accesses (by name) are logged in release-mode on consoles.
//! This can be used to find non-optimal usage scenario's, where the constant should be used directly instead.
//! Since read accesses tend to be used in flow-control logic, constants allow for better optimization by the compiler.
#define LOG_CONST_CVAR_ACCESS 0

#if CRY_PLATFORM_WINDOWS || LOG_CONST_CVAR_ACCESS
	#define RELEASE_LOGGING
	#if defined(_RELEASE)
		#define CVARS_WHITELIST
	#endif // defined(_RELEASE)
#endif

#if defined(_RELEASE) && !defined(RELEASE_LOGGING) && !defined(DEDICATED_SERVER)
	#define EXCLUDE_NORMAL_LOG
#endif

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	#if defined(DEDICATED_SERVER)
//! Enable/disable map load slicing functionality from the build.
		#define MAP_LOADING_SLICING
	#endif
#endif

#if defined(MAP_LOADING_SLICING)
	#if defined(__cplusplus)
		#define SLICE_AND_SLEEP()    do { GetISystemScheduler()->SliceAndSleep(__FUNC__, __LINE__); } while (0)
		#define SLICE_SCOPE_DEFINE() CSliceLoadingMonitor sliceScope
	#else
extern void SliceAndSleep(const char* pFunc, int line);
		#define SLICE_AND_SLEEP() SliceAndSleep(__FILE__, __LINE__)
	#endif
#else
	#define SLICE_AND_SLEEP()
	#define SLICE_SCOPE_DEFINE()
#endif

// Compile with unit testing enabled when not in RELEASE
#if !defined(_RELEASE)
	#define CRY_UNIT_TESTING

// configure the unit testing framework, if we have exceptions or not
	#if !(CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_ORBIS)
		#define CRY_UNIT_TESTING_USE_EXCEPTIONS
	#endif

#endif

#if (!defined(_RELEASE) || defined(PERFORMANCE_BUILD))
	#define USE_HTTP_WEBSOCKETS 0
#endif

#if (CRY_PLATFORM_WINDOWS || CRY_PLATFORM_ORBIS || CRY_PLATFORM_DURANGO) && !defined(RESOURCE_COMPILER)
	#define CAPTURE_REPLAY_LOG 1
#endif

#if defined(RESOURCE_COMPILER) || defined(_RELEASE)
	#undef CAPTURE_REPLAY_LOG
#endif

#ifndef CAPTURE_REPLAY_LOG
	#define CAPTURE_REPLAY_LOG 0
#endif

#define USE_GLOBAL_BUCKET_ALLOCATOR

#define OLD_VOICE_SYSTEM_DEPRECATED

#if !defined(PHYSICS_STACK_SIZE)
	#define PHYSICS_STACK_SIZE (128U << 10)
#endif

#if (!defined(_RELEASE) || defined(PERFORMANCE_BUILD)) && !defined(RESOURCE_COMPILER)
	#ifndef ENABLE_PROFILING_CODE
		#define ENABLE_PROFILING_CODE
	#endif
// Lightweight profilers, disable for submissions, disables displayinfo inside 3dengine as well.
	#ifndef ENABLE_LW_PROFILERS
		#define ENABLE_LW_PROFILERS
	#endif
#endif

#if !defined(_RELEASE)
	#define USE_FRAME_PROFILER // comment this define to remove most profiler related code in the engine
	#define CRY_TRACE_HEAP
#endif

//---------------------------------------------------------------------
// Profiling
//---------------------------------------------------------------------
#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
// Define here if you want to enable any external profiling tools
	#if defined(PERFORMANCE_BUILD)
		#if defined(USE_FRAME_PROFILER)
			#define ALLOW_FRAME_PROFILER 0 // currently not support in performance
		#else
			#define ALLOW_FRAME_PROFILER 0 // do not change this value
		#endif
		#if defined(USE_BROFILER) && CRY_PLATFORM_WINDOWS
			#define ALLOW_BROFILER        1 // decide if you want to enable Brofiler
		#else
			#define ALLOW_BROFILER        0 // do not change this value
		#endif
		#define ALLOW_PLATFORM_PROFILER 1 // decide if you want to enable PIX/Razor/GPA
		#define ALLOW_DEEP_PROFILING    1 // show more than just Regions
	#else
		#if defined(USE_FRAME_PROFILER)
			#define ALLOW_FRAME_PROFILER 1 // decide if you want to enable internal profiler
		#else
			#define ALLOW_FRAME_PROFILER 0 // do not change this value
		#endif
		#if defined(USE_BROFILER) && CRY_PLATFORM_WINDOWS
			#define ALLOW_BROFILER        1 // decide if you want to enable Brofiler
		#else
			#define ALLOW_BROFILER        0 // do not change this value
		#endif
		#define ALLOW_PLATFORM_PROFILER 1 // decide if you want to enable PIX/Razor/GPA
		#define ALLOW_DEEP_PROFILING    1 // show more than just Regions
	#endif

#endif
//---------------------------------------------------------------------

#undef ENABLE_STATOSCOPE
#if defined(ENABLE_PROFILING_CODE)
	#define ENABLE_STATOSCOPE 1
#endif

#if defined(ENABLE_PROFILING_CODE)
	#define USE_PERFHUD
#endif

#if defined(ENABLE_PROFILING_CODE)
	#define ENABLE_ART_RT_TIME_ESTIMATE
#endif

#if !defined(_RELEASE) || defined(ENABLE_STATOSCOPE_RELEASE)
#define ENABLE_FLASH_INFO
#endif

#if !defined(ENABLE_LW_PROFILERS) && !defined(ENABLE_DEVELOPER_CONSOLE_IN_RELEASE)
	#ifndef USE_NULLFONT
		#define USE_NULLFONT      1
	#endif
	#define USE_NULLFONT_ALWAYS 1
#endif

#if CRY_PLATFORM_WINDOWS
	#define FLARES_SUPPORT_EDITING
#endif

// Reflect texture slot information - only used in the editor.
#if CRY_PLATFORM_WINDOWS
	#define SHADER_REFLECT_TEXTURE_SLOTS 1
#else
	#define SHADER_REFLECT_TEXTURE_SLOTS 0
#endif

#if CRY_PLATFORM_WINDOWS && (!defined(_RELEASE) || defined(RESOURCE_COMPILER))
	#define CRY_ENABLE_RC_HELPER 1
#endif

#if defined(CRY_IS_MONOLITHIC_BUILD)
	#define SYS_ENV_AS_STRUCT
#endif

//! These enable and disable certain net features to give compatibility between PCs and consoles / profile and performance builds.
#define PC_CONSOLE_NET_COMPATIBLE          0
#define PROFILE_PERFORMANCE_NET_COMPATIBLE 0

#if (!defined(_RELEASE) || defined(PERFORMANCE_BUILD)) && !PROFILE_PERFORMANCE_NET_COMPATIBLE
	#define USE_LAGOMETER (1)
#else
	#define USE_LAGOMETER (0)
#endif

//! A special ticker thread to run during load and unload of levels.
#define USE_NETWORK_STALL_TICKER_THREAD

#if !CRY_PLATFORM_MOBILE
//---------------------------------------------------------------------
// Enable Tessellation Features
// (displacement mapping, subdivision, water tessellation)
//---------------------------------------------------------------------
// Modules   : 3DEngine, Renderer
// Depends on: DX11

// Global tessellation feature flag
	#define TESSELLATION
	#ifdef TESSELLATION
// Specific features flags
		#define WATER_TESSELLATION
		#define PARTICLES_TESSELLATION

		#if !CRY_PLATFORM_ORBIS   // Causes memory wastage in RenderMesh.cpp
// Mesh tessellation (displacement, smoothing, subd)
			#define MESH_TESSELLATION
// Mesh tessellation also in motion blur passes
			#define MOTIONBLUR_TESSELLATION
		#endif

// Dependencies
		#ifdef MESH_TESSELLATION
			#define MESH_TESSELLATION_ENGINE
		#endif

		#ifdef WATER_TESSELLATION
			#define WATER_TESSELLATION_RENDERER
		#endif
		#ifdef PARTICLES_TESSELLATION
			#define PARTICLES_TESSELLATION_RENDERER
		#endif
		#ifdef MESH_TESSELLATION_ENGINE
			#define MESH_TESSELLATION_RENDERER
		#endif

		#if defined(WATER_TESSELLATION_RENDERER) || defined(PARTICLES_TESSELLATION_RENDERER) || defined(MESH_TESSELLATION_RENDERER)
//! Common tessellation flag enabling tessellation stages in renderer.
			#define TESSELLATION_RENDERER
		#endif

	#endif // TESSELLATION
#endif   // !CRY_PLATFORM_MOBILE

#define USE_GEOM_CACHES

//------------------------------------------------------
// SVO GI
//------------------------------------------------------
// Modules   : Renderer, Engine
// Platform  : DX11
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	#define FEATURE_SVO_GI
	#if CRY_PLATFORM_WINDOWS
		#define FEATURE_SVO_GI_ALLOW_HQ
	#endif
#endif

#if defined(RELEASE) && CRY_PLATFORM_WINDOWS && defined(DEDICATED_SERVER) && 0
	#define RELEASE_SERVER_SECURITY
#endif

#if !defined(_RELEASE)
	#define ENABLE_DYNTEXSRC_PROFILING
#endif

#if defined(ENABLE_PROFILING_CODE)
	#define USE_DISK_PROFILER
	#define ENABLE_LOADING_PROFILER
#endif

#if !defined(_DEBUG) && CRY_PLATFORM_WINDOWS
//# define CRY_PROFILE_MARKERS_USE_GPA
//# define CRY_PROFILE_MARKERS_USE_NVTOOLSEXT
#endif

#ifdef SEG_WORLD
	#define SW_STRIP_LOADING_MSG
	#define SW_ENTITY_ID_USE_GUID
	#define SW_NAVMESH_USE_GUID
#endif

#include "ProjectDefinesInclude.h"

#ifdef RELEASE
// Forces the .cryproject file to be read from a .pak file instead of directly from disk.
#define CRY_FORCE_CRYPROJECT_IN_PAK
#endif

//Encryption & security defines

//! Defines for various encryption methodologies that we support (or did support at some stage).
#define SUPPORT_UNENCRYPTED_PAKS             //Enable during dev and on consoles to support paks that aren't encrypted in any way

// #define SUPPORT_XTEA_PAK_ENCRYPTION                             //! C2 Style. Compromised - do not use.
// #define SUPPORT_STREAMCIPHER_PAK_ENCRYPTION                     //! C2 DLC Style - by Mark Tully.
#if !CRY_PLATFORM_DURANGO
	#define SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION //C3/Warface Style - By Timur Davidenko and integrated by Rob Jessop
#endif
#if (!defined(_RELEASE) || defined(PERFORMANCE_BUILD)) && !defined(SUPPORT_UNSIGNED_PAKS)
	#define SUPPORT_UNSIGNED_PAKS //Enable to load paks that aren't RSA signed
#endif                          //!_RELEASE || PERFORMANCE_BUILD
#if !CRY_PLATFORM_DURANGO
	#define SUPPORT_RSA_PAK_SIGNING //RSA signature verification
#endif

#if CRY_PLATFORM_DURANGO
//#define SUPPORT_SMARTGLASS // Disabled - needs fixing with April XDK
#endif

#if defined(SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION) || defined(SUPPORT_RSA_PAK_SIGNING)
//! Use LibTomMath and LibTomCrypt for cryptography.
	#define INCLUDE_LIBTOMCRYPT
#endif

//! This enables checking of CRCs on archived files when they are loaded fully and synchronously in CryPak.
//! Computes a CRC of the decompressed data and compares it to the CRC stored in the archive CDR for that file.
//! Files with CRC mismatches will return Z_ERROR_CORRUPT and invoke the global handler in the PlatformOS.
#define VERIFY_PAK_ENTRY_CRC

//#define CHECK_CRC_ONLY_ONCE	//Do NOT enable this if using SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION - it will break subsequent decryption attempts for a file as it nulls the stored CRC32

#if 0 // Enable when clear on which platforms we want this check
//! On consoles we can trust files that have been loaded from optical drives.
	#define SKIP_CHECKSUM_FROM_OPTICAL_MEDIA
#endif // 0

//End of encryption & security defines

// loading patch1.pak during startup
//#define USE_PATCH_PAK

#define EXPOSE_D3DDEVICE

//! The maximum number of joints in an animation.
#define MAX_JOINT_AMOUNT 1024

// #define this if you still need the old C1-style AI-character system, but which will get removed soon
//#define USE_DEPRECATED_AI_CHARACTER_SYSTEM

#endif // PROJECTDEFINES_H
