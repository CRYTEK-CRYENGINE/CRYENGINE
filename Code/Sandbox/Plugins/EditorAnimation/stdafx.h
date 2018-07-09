// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

/////////////////////////////////////////////////////////////////////////////
// CRY Stuff ////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
#include <CryCore/Project/CryModuleDefs.h>
#include <CryCore/Platform/platform.h>

#define CRY_USE_MFC
#include <CryCore/Platform/CryAtlMfc.h>

#include <stdlib.h>
#define INCLUDE_SAVECGF

/////////////////////////////////////////////////////////////////////////////
// STL
/////////////////////////////////////////////////////////////////////////////
#include <vector>
#include <list>
#include <map>
#include <set>
#include <algorithm>
#include <memory>

namespace physics_editor {
using std::vector;
using std::pair;
using std::auto_ptr;
};

/////////////////////////////////////////////////////////////////////////////
// CRY Stuff ////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

#define TSmartPtr _smart_ptr
#define SMARTPTR_TYPEDEF(Class) typedef _smart_ptr<Class> Class ## Ptr

#include <CrySystem/ISystem.h>
#include "Util/EditorUtils.h"
#include "IEditor.h"
#include "EditorCommon.h"

IEditor* GetIEditor();

void     Log(const char* format, ...);

// all these are needed just for CGFLoader.cpp
#include <CryMath/Cry_Math.h>
#include <CryCore/Platform/platform.h>
#include <CryMath/Cry_Geo.h>
#include <CryMemory/CrySizer.h>
#include <CryRenderer/VertexFormats.h>

#include <qt_windows.h>

