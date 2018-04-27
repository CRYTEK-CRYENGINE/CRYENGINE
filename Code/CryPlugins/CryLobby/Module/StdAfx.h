// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(AFX_STDAFX_H__D3D35062_283E_4DF2_A9C4_9AE0A1B082A8__INCLUDED_)
#define AFX_STDAFX_H__D3D35062_283E_4DF2_A9C4_9AE0A1B082A8__INCLUDED_

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

//#define NOT_USE_CRY_MEMORY_MANAGER

#include <CryCore/Project/CryModuleDefs.h>
#define eCryModule eCryM_EnginePlugin
#define CRYLOBBY_EXPORTS
#include <CryCore/Platform/platform.h>

#if !defined(_DEBUG)
	#define CRYNETWORK_RELEASEBUILD 1
#else
	#define CRYNETWORK_RELEASEBUILD 0
#endif // #if !defined(_DEBUG)

#include <stdio.h>
#include <stdarg.h>

#if CRY_PLATFORM_DURANGO
	#define USE_DURANGOLIVE 0 // not supported - requires to be rewritten with xsapi 2.0
#else
	#define USE_DURANGOLIVE 0
#endif

#include <map>
#include <algorithm>

#include <CryMath/Cry_Math.h>
#include <CryMemory/CrySizer.h>
#include <CryCore/StlUtils.h>
#include <CryCore/CryVariant.h>

#include <CryRenderer/IRenderer.h>


#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	#include <unistd.h>
	#include <fcntl.h>
#endif

#if CRY_PLATFORM_WINAPI
	#define S_ADDR_IP4(ADDR) ((ADDR).sin_addr.S_un.S_addr)
#else
	#define S_ADDR_IP4(ADDR) ((ADDR).sin_addr.s_addr)
#endif

#include <memory>
#include <vector>

#define NET_ASSERT(x) CRY_ASSERT_MESSAGE(x, # x)

#if _MSC_VER > 1000
	#pragma intrinsic(memcpy)
#endif

#include "INetworkPrivate.h"

#if defined(_MSC_VER)
extern "C" void* _ReturnAddress(void);
	#pragma intrinsic(_ReturnAddress)
	#define UP_STACK_PTR _ReturnAddress()
#else
	#define UP_STACK_PTR 0
#endif

#if !defined(EXCLUDE_NORMAL_LOG)
	#define NetLog       CryLog
	#define NetLogAlways CryLogAlways
#else
	#define NetLog(...)
	#define NetLogAlways(...)
#endif

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__D3D35062_283E_4DF2_A9C4_9AE0A1B082A8__INCLUDED_)
