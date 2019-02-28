// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Script/IScriptElement.h"

namespace Schematyc
{
struct IScriptEnum : public IScriptElementBase<EScriptElementType::Enum>
{
	virtual ~IScriptEnum() {}

	virtual uint32      GetConstantCount() const = 0;
	virtual uint32      FindConstant(const char* szConstant) const = 0;
	virtual const char* GetConstant(uint32 constantIdx) const = 0;
};
} // Schematyc
