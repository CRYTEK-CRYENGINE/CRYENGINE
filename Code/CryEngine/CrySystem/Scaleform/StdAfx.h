// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(CRY_IS_SCALEFORM_HELPER)
	// In case Scaleform integration is compiled directly into CrySystem, we need to use CrySystem StdAfx.h
	#include "../StdAfx.h"
#else
	// We are compiling the required Scaleform dependencies into a helper DLL.
	#include <CryCore/Project/CryModuleDefs.h>
	#define eCryModule eCryM_ScaleformHelper
	#include <CryCore/Platform/platform.h>

	// Sanity check, WAF should not have tried to build this.
	#ifndef INCLUDE_SCALEFORM_SDK
		#error Scaleform helper can not be compiled without Scaleform support
	#endif

	// Sanity check, we can't build the helper DLL as part of monolithic build.
	#ifdef _LIB
		#error Scaleform helper can only be built into a shared library
	#endif

	// Video support should not be compiled into the helper DLL in any scenario.
	// If video support is desired, it should be compiled directly into CrySystem instead.
	#define EXCLUDE_SCALEFORM_VIDEO

	#include <CrySystem/ISystem.h>
	#include <CrySystem/Scaleform/ConfigScaleform.h>
	#include <CrySystem/Scaleform/ConfigScaleform_impl.h>
#endif
