// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Project/CryModuleDefs.h>
#define eCryModule eCryM_AudioSystem
#include <CryCore/Platform/platform.h>
#include <CryCore/StlUtils.h>
#include <CryCore/Project/ProjectDefines.h>

#if !defined(_RELEASE)
	#define INCLUDE_AUDIO_PRODUCTION_CODE
	#define ENABLE_AUDIO_LOGGING
#endif // _RELEASE

#include <AudioLogger.h>

namespace CryAudio
{
extern CLogger g_logger;
extern CTimeValue g_lastMainThreadFrameStartTime;
} // namespace CryAudio
