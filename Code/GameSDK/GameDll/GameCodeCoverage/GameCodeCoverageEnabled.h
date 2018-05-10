// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GameCodeCoverageEnabled.h
//  Created:     11/11/2009 by Tim Furnish
//  Description: Defines ENABLE_GAME_CODE_COVERAGE
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __GAME_CODE_COVERAGE_ENABLED_H_
#define __GAME_CODE_COVERAGE_ENABLED_H_

#include <CrySystem/ICodeCheckpointMgr.h>

//===============================================================================================

// 1 = Disable the game DLL's own code coverage system and use the engine's ICodeCoverageMgr instance IF IT'S ENABLED
// 0 = Use the game's own code coverage system if available (fall back to using engine's system if ENABLE_GAME_CODE_COVERAGE gets set to 0 by something else)
#define GAME_SHOULD_PRIORITISE_ENGINE_CODE_COVERAGE_SYSTEM					1

//===============================================================================================

#if defined(CODECHECKPOINT_ENABLED) && (GAME_SHOULD_PRIORITISE_ENGINE_CODE_COVERAGE_SYSTEM)
	#define ENABLE_GAME_CODE_COVERAGE     0
#elif defined(_RELEASE)
	#define ENABLE_GAME_CODE_COVERAGE			0		// Final release - never enable!
#elif !defined(_DEBUG)
	#if CRY_PLATFORM_WINDOWS
		#define ENABLE_GAME_CODE_COVERAGE		1		// Profile builds on PC
	#else
		#define ENABLE_GAME_CODE_COVERAGE		0		// Profile builds on consoles
	#endif
#else
	#if CRY_PLATFORM_WINDOWS
		#define ENABLE_GAME_CODE_COVERAGE		1		// Debug builds on PC
	#else
		#define ENABLE_GAME_CODE_COVERAGE		1		// Debug builds on consoles
	#endif
#endif

#if defined(CODECHECKPOINT_ENABLED) && !ENABLE_GAME_CODE_COVERAGE
	#define ENABLE_SHARED_CODE_COVERAGE 1
#endif

#endif // __GAME_CODE_COVERAGE_ENABLED_H_