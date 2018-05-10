// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Project/CryModuleDefs.h>
#include <CryCore/Platform/platform.h>

#define CRY_USE_MFC
#define CRY_SUPPRESS_CRYENGINE_WINDOWS_FUNCTION_RENAMING
#include <CryCore/Platform/CryAtlMfc.h>

#include <CrySystem/ISystem.h>
#include "IEditor.h"
#include "EditorCommon.h"

#include <CrySerialization/yasli/Archive.h>
#include <CrySerialization/yasli/STL.h>

