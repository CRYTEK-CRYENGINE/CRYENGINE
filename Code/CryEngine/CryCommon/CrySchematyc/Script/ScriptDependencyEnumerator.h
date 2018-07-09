// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Utils/Delegate.h"
#include "CrySchematyc/Utils/GUID.h"

namespace Schematyc
{
// Forward declare structures.
typedef std::function<void (const CryGUID&)> ScriptDependencyEnumerator;

enum class EScriptDependencyType
{
	Load,
	Compile,
	Event,
	Reference
};
} // Schematyc
