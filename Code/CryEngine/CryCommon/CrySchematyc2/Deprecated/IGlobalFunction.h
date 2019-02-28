// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/Any.h"
#include "CrySchematyc2/Deprecated/Variant.h"

namespace Schematyc2
{
	struct IGlobalFunction
	{
		virtual ~IGlobalFunction() {}

		virtual SGUID GetGUID() const = 0;
		virtual void SetName(const char* szName) = 0;
		virtual const char* GetName() const = 0;
		virtual void SetNamespace(const char* szNamespace) = 0;
		virtual const char* GetNamespace() const = 0;
		virtual void SetFileName(const char* szFileName, const char* szProjectDir) = 0;
		virtual const char* GetFileName() const = 0;
		virtual void SetAuthor(const char* szAuthor) = 0;
		virtual const char* GetAuthor() const = 0;
		virtual void SetDescription(const char* szDescription) = 0;
		virtual const char* GetDescription() const = 0;
		virtual void SetWikiLink(const char* szWikiLink) = 0;
		virtual const char* GetWikiLink() const = 0;
		virtual bool BindInput(size_t paramIdx, const char* szName, const char* szDescription) = 0;
		virtual size_t GetInputCount() const = 0;
		virtual IAnyConstPtr GetInputValue(size_t inputIdx) const = 0;
		virtual const char* GetInputName(size_t inputIdx) const = 0;
		virtual const char* GetInputDescription(size_t inputIdx) const = 0;
		virtual TVariantConstArray GetVariantInputs() const = 0;
		virtual bool BindOutput(size_t paramIdx, const char* szName, const char* szDescription) = 0;
		virtual size_t GetOutputCount() const = 0;
		virtual IAnyConstPtr GetOutputValue(size_t outputIdx) const = 0;
		virtual const char* GetOutputName(size_t outputIdx) const = 0;
		virtual const char* GetOutputDescription(size_t outputIdx) const = 0;
		virtual TVariantConstArray GetVariantOutputs() const = 0;
		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const = 0;
		virtual void RegisterInputParamsForResourcePrecache() = 0;
		virtual bool AreInputParamsResources() const = 0;

		template <typename TYPE> inline void BindInput(size_t paramIdx, const char* szName, const char* szDescription, const TYPE& value)
		{
			BindInput_Protected(paramIdx, szName, szDescription, MakeAny(value));
		}

		inline void BindInput(size_t paramIdx, const char* szName, const char* szDescription, const char* value)
		{
			BindInput_Protected(paramIdx, szName, szDescription, MakeAny(CPoolString(value)));
		}

	protected:

		virtual bool BindInput_Protected(size_t paramIdx, const char* szName, const char* szDescription, const IAny& value) = 0;
	};

	DECLARE_SHARED_POINTERS(IGlobalFunction)
}
