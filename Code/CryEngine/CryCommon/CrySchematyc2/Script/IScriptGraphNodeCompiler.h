// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/Any.h"
#include "CrySchematyc2/BasicTypes.h"
#include "CrySchematyc2/Runtime/IRuntime.h"

namespace Schematyc2
{
	struct ILibClass;

	struct IScriptGraphNodeCompiler
	{
		virtual ~IScriptGraphNodeCompiler() {}

		virtual const ILibClass& GetLibClass() const = 0;
		virtual bool BindCallback(RuntimeNodeCallbackPtr pCallback) = 0;
		virtual bool BindAnyAttribute(uint32 attributeId, const IAny& value) = 0;
		virtual bool BindAnyInput(uint32 inputIdx, const IAny& value) = 0;
		virtual bool BindAnyOutput(uint32 ouputIdx, const IAny& value) = 0;
		virtual bool BindAnyFunctionInput(uint32 inputIdx, const IAny& value) = 0;
		virtual bool BindAnyFunctionOutput(uint32 ouputIdx, const IAny& value) = 0;

		template <typename TYPE> bool BindAttribute(uint32 attributeId, const TYPE& value)
		{
			return BindAnyAttribute(attributeId, MakeAny(value));
		}

		template <typename TYPE> bool BindInput(uint32 inputIdx, const TYPE& value)
		{
			return BindAnyInput(inputIdx, MakeAny(value));
		}

		template <typename TYPE> bool BindOutput(uint32 ouputIdx, const TYPE& value)
		{
			return BindAnyOutput(ouputIdx, MakeAny(value));
		}

		template <typename TYPE> bool BindFunctionInput(uint32 inputIdx, const TYPE& value)
		{
			return BindAnyFunctionInput(inputIdx, MakeAny(value));
		}

		template <typename TYPE> bool BindFunctionOutput(uint32 ouputIdx, const TYPE& value)
		{
			return BindAnyFunctionOutput(ouputIdx, MakeAny(value));
		}
	};
}
