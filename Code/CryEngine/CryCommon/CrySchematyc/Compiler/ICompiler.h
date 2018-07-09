// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Utils/GUID.h"
#include "CrySchematyc/Utils/Signal.h"

namespace Schematyc
{
// Forward declare interfaces.
struct IRuntimeClass;

typedef CSignal<void (const IRuntimeClass&)> ClassCompilationSignal;

struct ICompiler
{
	virtual ~ICompiler() {}

	virtual void                           CompileAll() = 0;
	virtual void                           CompileDependencies(const CryGUID& guid) = 0;
	virtual ClassCompilationSignal::Slots& GetClassCompilationSignalSlots() = 0;
};
} // Schematyc
