// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   stdafx.h
//  Version:     v1.00
//  Created:     30/9/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: Precompiled Header.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __stdafx_h__
#define __stdafx_h__

#if _MSC_VER > 1000
	#pragma once
#endif

//#define DEFINE_MODULE_NAME "CrySystem"

#include <CryCore/Project/CryModuleDefs.h>
#define eCryModule eCryM_System

#define CRYSYSTEM_EXPORTS

#include <CryCore/Platform/platform.h>

//////////////////////////////////////////////////////////////////////////
// CRT
//////////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#if !CRY_PLATFORM_LINUX && !CRY_PLATFORM_ANDROID && !CRY_PLATFORM_APPLE && !CRY_PLATFORM_ORBIS
	#include <memory.h>
	#include <malloc.h>
#endif
#include <fcntl.h>

#if !CRY_PLATFORM_ORBIS && !CRY_PLATFORM_APPLE && !CRY_PLATFORM_ANDROID
	#if CRY_PLATFORM_LINUX
		#include <sys/io.h>
	#else
		#include <io.h>
	#endif
#endif

/////////////////////////////////////////////////////////////////////////////
// CRY Stuff ////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Camera.h>
#include <CryMath/Random.h>
#include <CryCore/smartptr.h>
#include <CryMath/Range.h>
#include <CryMemory/CrySizer.h>
#include <CryCore/StlUtils.h>

static inline int RoundToClosestMB(size_t memSize)
{
	// add half a MB and shift down to get closest MB
	return((int) ((memSize + (1 << 19)) >> 20));
}

//////////////////////////////////////////////////////////////////////////
// For faster compilation
//////////////////////////////////////////////////////////////////////////
#include <CryRenderer/IRenderer.h>
#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <CryScriptSystem/IScriptSystem.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CrySystem/ITimer.h>
#include <CryAudio/IAudioSystem.h>
#include <CryPhysics/IPhysics.h>
#include <CryAISystem/IAISystem.h>
#include <CrySystem/XML/IXml.h>
#include <CrySystem/ICmdLine.h>
#include <CryInput/IInput.h>
#include <CrySystem/IConsole.h>
#include <CrySystem/ILog.h>

#include <CrySystem/Profilers/FrameProfiler/FrameProfiler_JobSystem.h>  // to be removed

/////////////////////////////////////////////////////////////////////////////
//forward declarations for common Interfaces.
/////////////////////////////////////////////////////////////////////////////
class ITexture;
struct IRenderer;
struct ISystem;
struct IScriptSystem;
struct ITimer;
struct IFFont;
struct IInput;
struct IKeyboard;
struct ICVar;
struct IConsole;
struct IEntitySystem;
struct IProcess;
struct ICryPak;
struct ICryFont;
struct I3DEngine;
struct IMovieSystem;
struct IPhysicalWorld;

/////////////////////////////////////////////////////////////////////////////
// HMD libraries
/////////////////////////////////////////////////////////////////////////////
#include <CrySystem/VR/IHMDDevice.h>

#endif // __stdafx_h__
