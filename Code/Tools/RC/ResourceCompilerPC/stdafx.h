// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once
#include <CryCore/Platform/platform.h>

typedef string tstring;

#include <malloc.h>			// malloc()

#include <float.h>

#include <memory>
#include <set>
#include <map>
#include <vector>
#include <algorithm>

#include <stdio.h>
#include <tchar.h>

#include <CrySystem/XML/IXml.h>

#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Geo.h>
#include <Cry3DEngine/CGF/CryHeaders.h>
#include <CryPhysics/primitives.h>
#include <CryCore/smartptr.h>
#include <CryPhysics/physinterface.h>
#include <CryMemory/CrySizer.h>

#include "ResourceCompilerPC.h"

#include <CrySystem/File/CryFile.h>

#include <CryRenderer/VertexFormats.h>

#include "..\ResourceCompiler\IRCLog.h"
#include "..\ResourceCompiler\SwapEndianness.h"
