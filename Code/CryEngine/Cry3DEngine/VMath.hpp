// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Version:     v1.00
//  Created:     Michael Kopietz
//  Description: unified vector math lib
// -------------------------------------------------------------------------
//  History:		- created 1999  for Katmai and K3
//							-	...
//							-	integrated into cryengine
//
////////////////////////////////////////////////////////////////////////////
#ifndef __VMATH__
#define __VMATH__

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

#endif
