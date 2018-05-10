// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once
#include <CryCore/Platform/platform.h>

#include <functional>
#include <vector>
#include <map>

#include <stdio.h>
#include <tchar.h>

#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Geo.h>

#include <CrySystem/File/CryFile.h>

#include <CrySystem/XML/IXml.h>

#include "IRCLog.h"

#include <CryRenderer/VertexFormats.h>

#define CRY_SUPPRESS_CRYENGINE_WINDOWS_FUNCTION_RENAMING
#include <CryCore/Platform/CryWindows.h>

#include <Alembic/Abc/All.h>
#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreFactory/All.h>
#include <Alembic/AbcCoreHDF5/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/Util/All.h>