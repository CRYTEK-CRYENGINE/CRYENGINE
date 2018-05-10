// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Script/IScriptElement.h"

namespace Schematyc
{

// Forward declare classes.
class CClassProperties;

struct IScriptActionInstance : public IScriptElementBase<EScriptElementType::ActionInstance>
{
	virtual ~IScriptActionInstance() {}

	virtual CryGUID                   GetActionTypeGUID() const = 0;
	virtual CryGUID                   GetComponentInstanceGUID() const = 0;
	virtual const CClassProperties& GetProperties() const = 0;
};

} // Schematyc
