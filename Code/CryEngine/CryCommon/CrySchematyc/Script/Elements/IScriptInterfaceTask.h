// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Script/IScriptElement.h"

namespace Schematyc
{
struct IScriptInterfaceTask : public IScriptElementBase<EScriptElementType::InterfaceTask>
{
	virtual ~IScriptInterfaceTask() {}

	virtual const char* GetAuthor() const = 0;
	virtual const char* GetDescription() const = 0;
};
} // Schematyc
