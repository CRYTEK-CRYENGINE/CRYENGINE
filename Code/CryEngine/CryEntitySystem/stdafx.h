// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#if !defined(AFX_STDAFX_H__2E966576_9327_4B66_9CFD_329F604BE709__INCLUDED_)
#define AFX_STDAFX_H__2E966576_9327_4B66_9CFD_329F604BE709__INCLUDED_

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

#include <CryCore/Project/CryModuleDefs.h>
#define eCryModule   eCryM_EntitySystem
#define RWI_NAME_TAG "RayWorldIntersection(EntitySys)"
#define PWI_NAME_TAG "PrimitiveWorldIntersection(EntitySys)"

#define CRYENTITYDLL_EXPORTS

#include <CryCore/Platform/platform.h>

#pragma warning (error : 4018) //Cannot align catch objects to greater than 16 bytes

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

#pragma warning (disable : 4768) //Cannot align catch objects to greater than 16 bytes
#include <vector>
#include <list>
#include <iterator>
#include <algorithm>
#include <map>

//////////////////////////////////////////////////////////////////////////

#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Camera.h>
#include <CryMath/Cry_XOptimise.h>
#include <CryMath/Cry_GeoIntersect.h>
#include <CryMemory/CrySizer.h>
#include <CryCore/smartptr.h>
#include <CryCore/StlUtils.h>

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

//#include "EntityDesc.h"
//#include <IEntitySystem.h>

#include <CrySystem/ISystem.h>
#include <CrySystem/IConsole.h>
#include <CryPhysics/IPhysics.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CrySystem/TimeValue.h>
#include <CryMemory/PoolAllocator.h>
#include <CryEntitySystem/IEntitySystem.h>

#include "EntityCVars.h"

#include <CrySystem/Profilers/FrameProfiler/FrameProfiler_JobSystem.h>  // to be removed

#if !defined(_RELEASE)
	#define INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE
#endif // !_RELEASE

//////////////////////////////////////////////////////////////////////////
class CEntitySystem;
extern CEntitySystem* g_pIEntitySystem;
ILINE CEntitySystem* GetIEntitySystem() { return g_pIEntitySystem; }
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//! Reports a Game Warning to validator with WARNING severity.
inline void EntityWarning(const char* format, ...) PRINTF_PARAMS(1, 2);
inline void EntityWarning(const char* format, ...)
{
	if (!format)
		return;

	va_list args;
	va_start(args, format);
	gEnv->pSystem->WarningV(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, 0, NULL, format, args);
	va_end(args);
}

inline void EntityFileWarning(const char* file, const char* format, ...) PRINTF_PARAMS(2, 3);
inline void EntityFileWarning(const char* file, const char* format, ...)
{
	if (!format)
		return;

	va_list args;
	va_start(args, format);
	gEnv->pSystem->WarningV(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, 0, file, format, args);
	va_end(args);
}

#define ENTITY_PROFILER FUNCTION_PROFILER(GetISystem(), PROFILE_ENTITY);
#define ENTITY_PROFILER_NAME(str) FRAME_PROFILER(str, GetISystem(), PROFILE_ENTITY);

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__2E966576_9327_4B66_9CFD_329F604BE709__INCLUDED_)
