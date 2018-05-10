// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/GUID.h"
#include "CrySchematyc2/IAny.h"
#include "CrySchematyc2/Env/EnvTypeId.h"

namespace Schematyc2
{
	enum class EEnvFunctionFlags
	{
		None   = 0,
		Member = BIT(0),
		Const  = BIT(1),
		//Pure   = BIT(2)
	};

	DECLARE_ENUM_CLASS_FLAGS(EEnvFunctionFlags)

	struct SEnvFunctionResult {};
	
	struct SEnvFunctionContext {};

	static const uint32 s_maxEnvFunctionParams = 10;

	typedef const IAny* EnvFunctionInputs[s_maxEnvFunctionParams];
	typedef IAny*       EnvFunctionOutputs[s_maxEnvFunctionParams];

	struct IEnvFunctionDescriptor
	{
		virtual ~IEnvFunctionDescriptor() {}

		virtual SGUID GetGUID() const = 0;
		virtual const char* GetName() const = 0;
		virtual const char* GetNamespace() const = 0;
		virtual const char* GetFileName() const = 0;
		virtual const char* GetAuthor() const = 0;
		virtual const char* GetDescription() const = 0;
		virtual EEnvFunctionFlags GetFlags() const = 0;
		virtual EnvTypeId GetContextTypeId() const = 0;
		virtual uint32 GetInputCount() const = 0;
		virtual uint32 GetInputId(uint32 inputIdx) const = 0;
		virtual const char* GetInputName(uint32 inputIdx) const = 0;
		virtual const char* GetInputDescription(uint32 inputIdx) const = 0;
		virtual IAnyConstPtr GetInputValue(uint32 inputIdx) const = 0;
		virtual uint32 GetOutputCount() const = 0;
		virtual uint32 GetOutputId(uint32 outputIdx) const = 0;
		virtual const char* GetOutputName(uint32 outputIdx) const = 0;
		virtual const char* GetOutputDescription(uint32 outputIdx) const = 0;
		virtual IAnyConstPtr GetOutputValue(uint32 outputIdx) const = 0;
		virtual SEnvFunctionResult Execute(const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs) const = 0;
	};
}
