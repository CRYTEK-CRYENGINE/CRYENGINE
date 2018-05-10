// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if _MSC_VER == 1900
// Disabling warnings related to member variables of classes that are exported with SANDBOX_API not being exported as well.
	#pragma warning(disable: 4251)
// Disabling warnings related to base classes of classes that are exported with SANDBOX_API not being exported as well.
	#pragma warning(disable: 4275)
#endif

#include <CryCore/Project/CryModuleDefs.h>
#define eCryModule eCryM_Editor
#undef  RWI_NAME_TAG
#define RWI_NAME_TAG "RayWorldIntersection(Editor)"
#undef  PWI_NAME_TAG
#define PWI_NAME_TAG "PrimitiveWorldIntersection(Editor)"
#include <CryCore/Platform/platform.h>

#define CRY_USE_XT
#define CRY_USE_ATL
#define CRY_SUPPRESS_CRYENGINE_WINDOWS_FUNCTION_RENAMING // Because we depend on having wrappers for GetObjectA/W and LoadLibraryA/W variants.
#include <CryCore/Platform/CryAtlMfc.h>

#include <CryCore/Project/ProjectDefines.h>
// Shell Extensions.
#include <Shlwapi.h>

// Resource includes
#include "Resource.h"

//////////////////////////////////////////////////////////////////////////
// Main Editor include.
//////////////////////////////////////////////////////////////////////////
#include "IPlugin.h"
#include "EditorDefs.h"

//#include "CryEdit.h"
#include "EditTool.h"
#include "PluginManager.h"

#include "Util/Variable.h"

#include <CryMath/ISplines.h>
#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Geo.h>
#include <CryCore/Containers/CryListenerSet.h>

#include "EditorCommon.h"
#include "IEditorClassFactory.h"

