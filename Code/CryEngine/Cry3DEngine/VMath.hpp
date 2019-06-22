// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#define VEC4_SSE
#if CRY_PLATFORM_DURANGO
	#define VEC4_SSE4
#endif

#include <CryMath/Cry_Math.h>

#if CRY_PLATFORM_NEON
	#include "VMath_NEON.hpp"
#elif CRY_PLATFORM_SSE2 || CRY_PLATFORM_SSE4
	#include "VMath_SSE.hpp"
#else
	#include "VMath_C.hpp"
#endif
