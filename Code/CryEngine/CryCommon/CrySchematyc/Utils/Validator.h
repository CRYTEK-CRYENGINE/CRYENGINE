// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Utils/Delegate.h"

namespace Schematyc
{
enum class EValidatorMessageType
{
	Warning = 0,
	Error
};

typedef std::function<void(EValidatorMessageType, const char*)> Validator;
} // Schematyc
