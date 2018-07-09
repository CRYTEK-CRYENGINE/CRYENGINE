// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   stdafx.h
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_STDAFX_H__8B93AD4E_EE86_4127_9BED_37AC6D0F978B__INCLUDED_3DENGINE)
#define AFX_STDAFX_H__8B93AD4E_EE86_4127_9BED_37AC6D0F978B__INCLUDED_3DENGINE

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

#include <CryCore/Project/CryModuleDefs.h>
#define eCryModule   eCryM_3DEngine
#define RWI_NAME_TAG "RayWorldIntersection(3dEngine)"
#define PWI_NAME_TAG "PrimitiveWorldIntersection(3dEngine)"
#include <CryCore/Platform/platform.h>

#define CRY3DENGINE_EXPORTS

const int nThreadsNum = 3;

//#define DEFINE_MODULE_NAME "Cry3DEngine"

//////////////////////////////////////////////////////////////////////////////////////////////
// Highlevel defines

// deferred cull queue handling - currently disabled
// #define USE_CULL_QUEUE

// Crysis3 as it's dx11 only can simply use the zbufferculler everywhere
// Older CCoverageBuffer currently does not compile
#if 1
	#define OCCLUSIONCULLER CZBufferCuller
#endif

// Compilation (Export to Engine) not needed on consoles
#if CRY_PLATFORM_DESKTOP
	#define ENGINE_ENABLE_COMPILATION 1
#else
	#define ENGINE_ENABLE_COMPILATION 0
#endif

#if !defined(_RELEASE) && CRY_PLATFORM_WINDOWS
	#define ENABLE_CONSOLE_MTL_VIZ
#endif

#pragma warning( error: 4018 )

#include <stdio.h>

#define MAX_PATH_LENGTH 512

#include <CrySystem/ITimer.h>
#include <CrySystem/IProcess.h>
#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Camera.h>
#include <CryMath/Cry_XOptimise.h>
#include <CryMath/Cry_Geo.h>
#include <CrySystem/ILog.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/IConsole.h>
#include <CryPhysics/IPhysics.h>
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <Cry3DEngine/IRenderNode.h>
#include <CryCore/Containers/StackContainer.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CrySystem/File/ICryPak.h>
#include <CrySystem/File/CryFile.h>
#include <CryCore/smartptr.h>
#include <CryCore/Containers/CryArray.h>
#include <Cry3DEngine/CGF/CryHeaders.h>
#include "Cry3DEngineBase.h"
#include <float.h>
#include <CryCore/Containers/CryArray.h>
#include "cvars.h"
#include <CryMemory/CrySizer.h>
#include <CryCore/StlUtils.h>
#include "Array2d.h"
#include "Material.h"
#include "3dEngine.h"
#include "ObjMan.h"
#include "Vegetation.h"
#include "terrain.h"
#include "ObjectsTree.h"

#include <CrySystem/Profilers/FrameProfiler/FrameProfiler_JobSystem.h>  // to be removed

template<class T>
void AddToPtr(byte*& pPtr, T& rObj, EEndian eEndian)
{
	PREFAST_SUPPRESS_WARNING(6326) static_assert((sizeof(T) % 4) == 0, "Invalid type size!");
	assert(!((INT_PTR)pPtr & 3));
	memcpy(pPtr, &rObj, sizeof(rObj));
	SwapEndian(*(T*)pPtr, eEndian);
	pPtr += sizeof(rObj);
	assert(!((INT_PTR)pPtr & 3));
}

template<class T>
void AddToPtr(byte*& pPtr, int& nDataSize, T& rObj, EEndian eEndian)
{
	PREFAST_SUPPRESS_WARNING(6326) static_assert((sizeof(T) % 4) == 0, "Invalid type size!");
	assert(!((INT_PTR)pPtr & 3));
	memcpy(pPtr, &rObj, sizeof(rObj));
	SwapEndian(*(T*)pPtr, eEndian);
	pPtr += sizeof(rObj);
	nDataSize -= sizeof(rObj);
	assert(nDataSize >= 0);
	assert(!((INT_PTR)pPtr & 3));
}

inline void FixAlignment(byte*& pPtr, int& nDataSize)
{
	while ((UINT_PTR)pPtr & 3)
	{
		*pPtr = 222;
		pPtr++;
		nDataSize--;
	}
}

inline void FixAlignment(byte*& pPtr)
{
	while ((UINT_PTR)pPtr & 3)
	{
		*pPtr = 222;
		pPtr++;
	}
}

template<class T>
void AddToPtr(byte*& pPtr, int& nDataSize, const T* pArray, int nElemNum, EEndian eEndian, bool bFixAlignment = false)
{
	assert(!((INT_PTR)pPtr & 3));
	memcpy(pPtr, pArray, nElemNum * sizeof(T));
	SwapEndian((T*)pPtr, nElemNum, eEndian);
	pPtr += nElemNum * sizeof(T);
	nDataSize -= nElemNum * sizeof(T);
	assert(nDataSize >= 0);

	if (bFixAlignment)
		FixAlignment(pPtr, nDataSize);
	else
		assert(!((INT_PTR)pPtr & 3));
}

template<class T>
void AddToPtr(byte*& pPtr, const T* pArray, int nElemNum, EEndian eEndian, bool bFixAlignment = false)
{
	assert(!((INT_PTR)pPtr & 3));
	memcpy(pPtr, pArray, nElemNum * sizeof(T));
	SwapEndian((T*)pPtr, nElemNum, eEndian);
	pPtr += nElemNum * sizeof(T);

	if (bFixAlignment)
		FixAlignment(pPtr);
	else
		assert(!((INT_PTR)pPtr & 3));
}

struct TriangleIndex
{
	TriangleIndex() { ZeroStruct(*this); }
	uint16&       operator[](const int& n)       { assert(n >= 0 && n < 3); return idx[n]; }
	const uint16& operator[](const int& n) const { assert(n >= 0 && n < 3); return idx[n]; }
	uint16        idx[3];
	uint16        nCull;
};

#define FUNCTION_PROFILER_3DENGINE CRY_PROFILE_FUNCTION(PROFILE_3DENGINE)

#if CRY_PLATFORM_DESKTOP
	#define INCLUDE_SAVECGF
#endif

#define ENABLE_TYPE_INFO_NAMES 1

#endif // !defined(AFX_STDAFX_H__8B93AD4E_EE86_4127_9BED_37AC6D0F978B__INCLUDED_3DENGINE)
