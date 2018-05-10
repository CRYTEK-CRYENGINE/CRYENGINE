// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(AFX_STDAFX_H__B36C365D_F0EA_4545_B3BC_1E0EAB3B5E43__INCLUDED_)
#define AFX_STDAFX_H__B36C365D_F0EA_4545_B3BC_1E0EAB3B5E43__INCLUDED_


//#define _CRTDBG_MAP_ALLOC

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define _LAUNCHER

#include <CryCore/Project/CryModuleDefs.h>
#define eCryModule eCryM_Launcher

// Insert your headers here
#include <CryCore/Platform/platform.h>
#include <assert.h>
#include <algorithm>
#include <vector>

// enable memory pool usage
#define USE_NEWPOOL
#include <CryMemory/CryMemoryManager.h>

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__B36C365D_F0EA_4545_B3BC_1E0EAB3B5E43__INCLUDED_)
