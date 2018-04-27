// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MANNEQUIN_DEBUG__H__
#define __MANNEQUIN_DEBUG__H__

#ifndef _RELEASE
	#define MANNEQUIN_DEBUGGING_ENABLED (1)
#else
	#define MANNEQUIN_DEBUGGING_ENABLED (0)
#endif

class CActionController;

namespace mannequin
{
namespace debug
{
void RegisterCommands();

#if MANNEQUIN_DEBUGGING_ENABLED
void        Log(const IActionController& actionControllerI, const char* format, ...);
void        DrawDebug();
inline void WebDebugState(const CActionController& actionController)                 {}
#else
inline void Log(const IActionController& actionControllerI, const char* format, ...) {}
inline void DrawDebug()                                                              {}
inline void WebDebugState(const CActionController& actionController)                 {}
#endif

}
}

//--- Macro ensures that complex expressions within a log are still removed from the build completely
#if MANNEQUIN_DEBUGGING_ENABLED
	#define MannLog(ac, format, ...) mannequin::debug::Log(ac, format, __VA_ARGS__)
#else //MANNEQUIN_DEBUGGING_ENABLED
	#define MannLog(...)
#endif //MANNEQUIN_DEBUGGING_ENABLED

#endif
