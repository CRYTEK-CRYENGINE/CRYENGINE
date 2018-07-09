// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Runtime/RuntimeGraph.h"
#include "CrySchematyc/Utils/Any.h"

namespace Schematyc
{
typedef SRuntimeResult (* RuntimeGraphNodeCallbackPtr)(SRuntimeContext& context, const SRuntimeActivationParams& activationParams);

struct IGraphNodeCompiler
{
	virtual ~IGraphNodeCompiler() {}

	virtual uint32 GetGraphIdx() const = 0;
	virtual uint32 GetGraphNodeIdx() const = 0;

	virtual void   BindCallback(RuntimeGraphNodeCallbackPtr pCallback) = 0;
	virtual void   BindData(const CAnyConstRef& value) = 0;
};
} // Schematyc
