// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/FundamentalTypes.h"
#include "CrySchematyc/Script/IScriptElement.h"

namespace Schematyc
{
// Forward declare classes.
class CAnyConstPtr;

struct IScriptVariable : public IScriptElementBase<EScriptElementType::Variable>
{
	virtual ~IScriptVariable() {}

	virtual SElementId      GetTypeId() const = 0;
	virtual bool            IsArray() const = 0;
	virtual CAnyConstPtr    GetData() const = 0;
	virtual CryGUID           GetBaseGUID() const = 0;
	virtual EOverridePolicy GetOverridePolicy() const = 0;
};
} // Schematyc
