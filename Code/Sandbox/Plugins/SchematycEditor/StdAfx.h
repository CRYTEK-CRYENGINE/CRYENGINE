// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

// CryEngine headers.

#include <CryCore/Project/CryModuleDefs.h>
#include <CryCore/Platform/platform.h>

// MFC & XTToolkit Pro headers.
#define CRY_USE_MFC
#define CRY_USE_ATL
#define CRY_USE_XT
#include <CryCore/Platform/CryAtlMfc.h>

// STL headers.

#include <vector>
#include <list>
#include <map>
#include <set>
#include <algorithm>

// Additional CryEngine headers.
#include <CrySystem/ISystem.h>
#include <CryCore/functor.h>
#include <CryCore/SmartPtr.h>
#include <CrySystem/File/ICryPak.h>

#include "IEditor.h"
#include "Util/EditorUtils.h"
#include "Util/Variable.h"

#include <QMetaType>

// Schematyc headers and declarations.

#if defined(SCHEMATYC_PLUGIN_EXPORTS)
	#define SCHEMATYC_PLUGIN_API __declspec(dllexport)
#else
	#define SCHEMATYC_PLUGIN_API __declspec(dllimport)
#endif

#include <CrySchematyc/CoreAPI.h>

namespace Cry {
namespace SchematycEd {

using namespace Schematyc2;

} // namespace Cry
} // namespace SchematycEd

Q_DECLARE_METATYPE(CryGUID);
