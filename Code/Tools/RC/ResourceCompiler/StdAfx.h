// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once
#include <CryCore/Platform/platform.h>

#include <CrySystem/XML/IXml.h>

// Standard C headers.
#include <direct.h>

// STL headers.
#include <vector>
#include <list>
#include <algorithm>
#include <functional>
#include <map>
#include <set>

//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////


#include <stdio.h>
#include <tchar.h>

#define CRY_SUPPRESS_CRYENGINE_WINDOWS_FUNCTION_RENAMING
#include <CryCore/Platform/CryWindows.h>

//#include <StlDbgAlloc.h>
// to make smoother transition back from cry to std namespace...
#define cry std
#define CRY_AS_STD

#include <CryCore/smartptr.h>

//////////////////////////////////////////////////////////////////////////
#include "ConvertContext.h"

#include <CryMath/Cry_Math.h>
#include <CryPhysics/primitives.h>
#include <Cry3DEngine/CGF/CryHeaders.h>
#include <CrySystem/CryVersion.h>

#include <CryCore/StlUtils.h>
#include <CryCore/Containers/VectorMap.h>
