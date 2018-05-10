// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/Any.h"
#include "CrySchematyc2/Deprecated/IStringPool.h"
#include "CrySchematyc2/Deprecated/Variant.h"

namespace Schematyc2
{
	struct IAbstractInterfaceFunction
	{
		virtual ~IAbstractInterfaceFunction() {}

		virtual SGUID GetGUID() const = 0;
		virtual SGUID GetOwnerGUID() const = 0; // #SchematycTODO : Rename GetAbstractInterfaceGUID()?
		virtual void SetName(const char* szName) = 0;
		virtual const char* GetName() const = 0;
		virtual void SetFileName(const char* szFileName, const char* szProjectDir) = 0;
		virtual const char* GetFileName() const = 0;
		virtual void SetAuthor(const char* szAuthor) = 0;
		virtual const char* GetAuthor() const = 0;
		virtual void SetDescription(const char* szDescription) = 0;
		virtual const char* GetDescription() const = 0;
		virtual void AddInput(const char* szName, const char* szDescription, const IAnyPtr& pValue) = 0;
		virtual size_t GetInputCount() const = 0;
		virtual const char* GetInputName(size_t inputIdx) const = 0;
		virtual const char* GetInputDescription(size_t inputIdx) const = 0;
		virtual IAnyConstPtr GetInputValue(size_t inputIdx) const = 0;
		virtual TVariantConstArray GetVariantInputs() const = 0;
		virtual void AddOutput(const char* szName, const char* szDescription, const IAnyPtr& pValue) = 0;
		virtual size_t GetOutputCount() const = 0;
		virtual const char* GetOutputName(size_t outputIdx) const = 0;
		virtual const char* GetOutputDescription(size_t outputIdx) const = 0;
		virtual IAnyConstPtr GetOutputValue(size_t outputIdx) const = 0;
		virtual TVariantConstArray GetVariantOutputs() const = 0;

		template <typename TYPE> inline void AddInput(const char* szName, const char* szDescription, const TYPE& value)
		{
			AddInput(szName, szDescription, MakeAnyShared(value));
		}

		inline void AddInput(const char* szName, const char* szDescription, const char* value)
		{
			AddInput(szName, szDescription, MakeAnyShared(CPoolString(value)));
		}

		template <typename TYPE> inline void AddOutput(const char* szName, const char* szDescription, const TYPE& value)
		{
			AddOutput(szName, szDescription, MakeAnyShared(value));
		}

		inline void AddOutput(const char* szName, const char* szDescription, const char* value)
		{
			AddOutput(szName, szDescription, MakeAnyShared(CPoolString(value)));
		}
	};

	DECLARE_SHARED_POINTERS(IAbstractInterfaceFunction)

	struct IAbstractInterface
	{
		virtual ~IAbstractInterface() {}

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
		virtual IAbstractInterfaceFunctionPtr AddFunction(const SGUID& guid) = 0;
		virtual size_t GetFunctionCount() const = 0;
		virtual IAbstractInterfaceFunctionConstPtr GetFunction(size_t functionIdx) const = 0;
	};

	DECLARE_SHARED_POINTERS(IAbstractInterface)
}
