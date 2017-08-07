// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Env/IEnvElement.h"

namespace Schematyc
{

// Forward declare classes.
class CCommonTypeDesc;

struct IEnvDataType : public IEnvElementBase<EEnvElementType::DataType>
{
	virtual ~IEnvDataType() {}

	virtual const CCommonTypeDesc& GetDesc() const = 0;
};

} // Schematyc
