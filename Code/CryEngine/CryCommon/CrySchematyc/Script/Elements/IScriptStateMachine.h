// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Script/IScriptElement.h"

namespace Schematyc
{
// #SchematycTODO : Consider making tasks and state machines different element types! Alternatively can we just use a node to stop/start state machines?
enum class EScriptStateMachineLifetime
{
	Persistent,
	Task
};

struct IScriptStateMachine : public IScriptElementBase<EScriptElementType::StateMachine>
{
	virtual ~IScriptStateMachine() {}

	virtual EScriptStateMachineLifetime GetLifetime() const = 0;
	virtual CryGUID                       GetContextGUID() const = 0;
	virtual CryGUID                       GetPartnerGUID() const = 0;
};
} // Schematyc
