// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Project/CryModuleDefs.h> 
#define eCryModule eCryM_LegacyGameDLL
#define RWI_NAME_TAG "RayWorldIntersection(Game)"
#define PWI_NAME_TAG "PrimitiveWorldIntersection(Game)"

#include <CryCore/Platform/platform.h>
#include "CryMacros.h"

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

//////////////////////////////////////////////////////////////////////////
//! Reports a Game Warning to validator with WARNING severity.
inline void GameWarning(const char *format, ...) PRINTF_PARAMS(1, 2);
inline void GameWarning(const char *format, ...)
{
	void GameWarningImpl(const char* format, va_list args); // implemented in Game.cpp
	if (!format)
		return;
	va_list args;
	va_start(args, format);
	GameWarningImpl(format, args);
	va_end(args);
}

// These inclusions are required everywhere but ideally to be cleaned up
#include <CrySystem/ISystem.h> // gEnv
#include "Game.h" // g_pGame

#include "GameCVars.h"
extern struct SCVars *g_pGameCVars;

#if !defined(_RELEASE)
#	define ENABLE_DEBUG_FLASH_PLAYBACK
#endif

#ifdef USE_PCH
	#include <algorithm>
	#include <atomic>
	#include <functional>
	#include <limits>
	#include <list>
	#include <map>
	#include <memory>
	#include <queue>
	#include <set>
	#include <vector>
#endif // USE_PCH