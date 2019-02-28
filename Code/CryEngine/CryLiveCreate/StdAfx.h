// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Project/CryModuleDefs.h>

#define eCryModule eCryM_LiveCreate
#define CRYLIVECREATE_EXPORTS

#include <CryCore/Platform/platform.h>

#if defined(_DEBUG) && CRY_PLATFORM_WINAPI
	#include <crtdbg.h>
#endif

#include <CryMath/Cry_Math.h>
#include <CrySystem/XML/IXml.h>
#include <CrySystem/ISystem.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryMovie/IMovieSystem.h>
#include <CryCore/smartptr.h>
#include <CryMemory/CrySizer.h>

#include <vector>
#include <set>
#include <map>

#if (defined(DEDICATED_SERVER) || defined(_RELEASE)) && !defined(NO_LIVECREATE)
	#define NO_LIVECREATE
#endif

#include <CryLiveCreate/ILiveCreateCommon.h>
#include <CryLiveCreate/ILiveCreatePlatform.h>
#include <CryLiveCreate/ILiveCreateManager.h>
#include <CryLiveCreate/ILiveCreateHost.h>
