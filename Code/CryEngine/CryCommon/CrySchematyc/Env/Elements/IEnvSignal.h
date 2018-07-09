// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Env/IEnvElement.h"

namespace Schematyc
{

// Forward declare classes.
class CClassDesc;

struct IEnvSignal : public IEnvElementBase<EEnvElementType::Signal>
{
	virtual ~IEnvSignal() {}

	virtual const CClassDesc& GetDesc() const = 0;
};

} // Schematyc
