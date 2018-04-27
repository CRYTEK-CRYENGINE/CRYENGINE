// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

// CryEngine headers.

#include <CryCore/Project/CryModuleDefs.h>
#define eCryModule eCryM_Legacy

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

// Schematyc headers and declarations.

#if defined(SCHEMATYC_PLUGIN_EXPORTS)
#define SCHEMATYC_PLUGIN_API __declspec(dllexport)
#else
#define SCHEMATYC_PLUGIN_API __declspec(dllimport)
#endif

#ifdef CRY_USE_SCHEMATYC2_BRIDGE

namespace Bridge { struct IFramework; };
namespace Bridge { struct IScriptFile; };

Bridge::IFramework* GetBridge();

#define GetSchematyc() GetBridge()
#define TScriptFile Bridge::IScriptFile
#define TScriptRegistry Bridge::IScriptRegistry
#define TDomainContextPtr Bridge::IDomainContextPtr
#define TDomainContextScope Bridge::SDomainContextScope
#define TScriptFileConstVisitor Bridge::ScriptFileConstVisitor

#include <Bridge_LibUtils.h>
#include <Bridge_ICompiler.h>
#include <Bridge_IFramework.h>
#include <Bridge_IScriptFile.h>
#include <Bridge_DocUtils.h>

#else

#define GetSchematyc() gEnv->pSchematyc2
#define TScriptFile Schematyc2::IScriptFile
#define TScriptRegistry Schematyc2::IScriptRegistry
#define TDomainContextPtr Schematyc2::IDomainContextPtr
#define TDomainContextScope Schematyc2::SDomainContextScope
#define TScriptFileConstVisitor Schematyc2::ScriptFileConstVisitor

#include <CrySchematyc2/ICompiler.h>
#include <CrySchematyc2/IFramework.h>
#include <CrySchematyc2/Script/IScriptFile.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>

#endif

#include <CrySchematyc2/TemplateUtils/TemplateUtils_Delegate.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_Signal.h>
#include <CrySchematyc2/LibUtils.h>
#include <CrySchematyc2/GUID.h>
#include <CrySchematyc2/ILibRegistry.h>
#include <CrySchematyc2/Services/ILog.h>

IEditor* GetIEditor();
HINSTANCE GetHInstance();

