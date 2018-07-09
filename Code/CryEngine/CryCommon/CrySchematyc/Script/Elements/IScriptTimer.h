// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Script/IScriptElement.h"

namespace Schematyc
{
// Forward declare structures.
struct STimerParams;

struct IScriptTimer : public IScriptElementBase<EScriptElementType::Timer>
{
	virtual ~IScriptTimer() {}

	virtual const char*  GetAuthor() const = 0;
	virtual const char*  GetDescription() const = 0;
	virtual STimerParams GetParams() const = 0;
};
} // Schematyc
