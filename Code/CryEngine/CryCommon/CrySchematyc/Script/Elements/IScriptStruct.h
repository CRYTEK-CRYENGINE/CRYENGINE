// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Script/IScriptElement.h"

namespace Schematyc
{
// Forward declare classes.
class CAnyConstPtr;

struct IScriptStruct : public IScriptElementBase<EScriptElementType::Struct>
{
	virtual ~IScriptStruct() {}

	virtual uint32       GetFieldCount() const = 0;
	virtual const char*  GetFieldName(uint32 fieldIdx) const = 0;
	virtual CAnyConstPtr GetFieldValue(uint32 fieldIdx) const = 0;
};
} // Schematyc
