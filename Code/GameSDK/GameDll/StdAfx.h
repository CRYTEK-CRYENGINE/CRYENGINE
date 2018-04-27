// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description:	include file for standard system include files,	or project
								specific include files that are used frequently, but are
								changed infrequently

 -------------------------------------------------------------------------
	History:
	- 20:7:2004   10:51 : Created by Marco Koegler
	- 3:8:2004		15:00 : Taken-over by Márcio Martins

*************************************************************************/
#if !defined(AFX_STDAFX_H__B36C365D_F0EA_4545_B3BC_1E0EAB3B5E43__INCLUDED_)
#define AFX_STDAFX_H__B36C365D_F0EA_4545_B3BC_1E0EAB3B5E43__INCLUDED_


//#define _CRTDBG_MAP_ALLOC

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <CryCore/Project/CryModuleDefs.h> 
#define eCryModule eCryM_LegacyGameDLL
#define RWI_NAME_TAG "RayWorldIntersection(Game)"
#define PWI_NAME_TAG "PrimitiveWorldIntersection(Game)"

// Insert your headers here
#include <CryCore/Platform/platform.h>
#include <algorithm>
#include <vector>
#include <memory>
#include <list>
#include <map>
#include <functional>
#include <limits>


#include <CryCore/smartptr.h>

#include <CryCore/Containers/VectorSet.h>
#include <CryCore/StlUtils.h>

#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Camera.h>
#include <CryCore/Containers/CryListenerSet.h>
#include <CrySystem/ISystem.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryParticleSystem/IParticles.h>
#include <CryInput/IInput.h>
#include <CrySystem/IConsole.h>
#include <CrySystem/ITimer.h>
#include <CrySystem/ILog.h>
#include <IGameplayRecorder.h>
#include <CryNetwork/ISerialize.h>

#include <CryMath/Random.h>

#include "CryMacros.h"

#include <CryGame/GameUtils.h>

#include <CrySystem/Profilers/FrameProfiler/FrameProfiler_JobSystem.h>	// to be removed

#ifndef GAMEDLL_EXPORTS
#define GAMEDLL_EXPORTS
#endif

#ifdef GAMEDLL_EXPORTS
#define GAME_API DLL_EXPORT
#else
#define GAME_API
#endif

# ifdef _DEBUG
#  ifndef DEBUG
#    define DEBUG
#  endif
#endif

#define MAX_PLAYER_LIMIT 16

#include "Game.h"

//////////////////////////////////////////////////////////////////////////
//! Reports a Game Warning to validator with WARNING severity.
inline void GameWarning( const char *format,... ) PRINTF_PARAMS(1, 2);
inline void GameWarning( const char *format,... )
{
	if (!format)
		return;
	va_list args;
	va_start(args, format);
	GetISystem()->WarningV( VALIDATOR_MODULE_GAME,VALIDATOR_WARNING,0,NULL,format,args );
	va_end(args);
}

extern struct SCVars *g_pGameCVars;

//---------------------------------------------------------------------
inline float LinePointDistanceSqr(const Line& line, const Vec3& point, float zScale = 1.0f)
{
	Vec3 x0=point;
	Vec3 x1=line.pointonline;
	Vec3 x2=line.pointonline+line.direction;

	x0.z*=zScale;
	x1.z*=zScale;
	x2.z*=zScale;

	return ((x2-x1).Cross(x1-x0)).GetLengthSquared()/(x2-x1).GetLengthSquared();
}

#if !defined(_RELEASE)
#	define ENABLE_DEBUG_FLASH_PLAYBACK
#endif

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__B36C365D_F0EA_4545_B3BC_1E0EAB3B5E43__INCLUDED_)

