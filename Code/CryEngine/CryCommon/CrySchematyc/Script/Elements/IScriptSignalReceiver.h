// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Script/IScriptElement.h"

namespace Schematyc
{
// Forward declare structures.


enum class EScriptSignalReceiverType
{
	Unknown = 0,
	EnvSignal,
	ScriptSignal,
	ScriptTimer,
	Universal // #SchematycTODO : All signal receivers should be universal.
};

struct IScriptSignalReceiver : public IScriptElementBase<EScriptElementType::SignalReceiver>
{
	virtual ~IScriptSignalReceiver() {}

	virtual EScriptSignalReceiverType GetSignalReceiverType() const = 0;
	virtual CryGUID                     GetSignalGUID() const = 0;
};
} // Schematyc
