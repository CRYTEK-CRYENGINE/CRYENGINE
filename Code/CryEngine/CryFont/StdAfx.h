// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Project/CryModuleDefs.h>
#define eCryModule eCryM_Font

#define CRYFONT_EXPORTS

#include <CryCore/Platform/platform.h>

#include <CryFont/IFont.h>

#include <CrySystem/ILog.h>
#include <CrySystem/IConsole.h>
#include <CryRenderer/IRenderer.h>
#include <CryMemory/CrySizer.h>

// USE_NULLFONT should be defined for all platforms running as a pure dedicated server
#if CRY_PLATFORM_DESKTOP
	#ifndef USE_NULLFONT
		#define USE_NULLFONT
	#endif
#endif
