// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/FundamentalTypes.h"
#include "CrySchematyc/Script/IScriptElement.h"

namespace Schematyc
{
struct IScriptInterfaceImpl : public IScriptElementBase<EScriptElementType::InterfaceImpl>
{
	virtual ~IScriptInterfaceImpl() {}

	virtual EDomain GetDomain() const = 0;
	virtual CryGUID   GetRefGUID() const = 0;
};
} // Schematyc
