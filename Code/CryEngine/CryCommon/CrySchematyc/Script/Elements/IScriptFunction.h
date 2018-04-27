// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/FundamentalTypes.h"
#include "CrySchematyc/Script/IScriptElement.h"

namespace Schematyc
{
// Forward declare classes.
class CAnyConstPtr;

struct IScriptFunction : public IScriptElementBase<EScriptElementType::Function>
{
	virtual ~IScriptFunction() {}

	virtual const char*  GetAuthor() const = 0;
	virtual const char*  GetDescription() const = 0;
	virtual uint32       GetInputCount() const = 0;
	virtual CryGUID        GetInputGUID(uint32 inputIdx) const = 0;
	virtual const char*  GetInputName(uint32 inputIdx) const = 0;
	virtual SElementId   GetInputTypeId(uint32 inputIdx) const = 0;
	virtual CAnyConstPtr GetInputData(uint32 inputIdx) const = 0;
	virtual uint32       GetOutputCount() const = 0;
	virtual CryGUID        GetOutputGUID(uint32 outputIdx) const = 0;
	virtual const char*  GetOutputName(uint32 outputIdx) const = 0;
	virtual SElementId   GetOutputTypeId(uint32 outputIdx) const = 0;
	virtual CAnyConstPtr GetOutputData(uint32 outputIdx) const = 0;
};
} // Schematyc
