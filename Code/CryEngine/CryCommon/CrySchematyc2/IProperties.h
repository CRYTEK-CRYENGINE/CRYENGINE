// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/Env/EnvTypeId.h"
#include "CrySchematyc2/Serialization/SerializationUtils.h"

namespace Schematyc2
{
	struct IProperties;

	DECLARE_SHARED_POINTERS(IProperties)

	struct IProperties
	{
		virtual ~IProperties() {}

		virtual EnvTypeId GetEnvTypeId() const = 0;
		virtual IPropertiesPtr Clone() const = 0;
		virtual GameSerialization::IContextPtr BindSerializationContext(Serialization::IArchive& archive) const = 0; // #SchematycTODO : Can we query domain context instead?
		virtual void Serialize(Serialization::IArchive& archive) = 0;
		virtual void* ToVoidPtr() = 0;
		virtual const void* ToVoidPtr() const = 0;
		
		template <typename TYPE> inline TYPE* ToPtr()
		{
			if(GetEnvTypeId() == Schematyc2::GetEnvTypeId<TYPE>()) // #SchematycTODO : Perform type check in actual implementation?
			{
				return static_cast<TYPE*>(ToVoidPtr());
			}
			return nullptr;
		}

		template <typename TYPE> inline const TYPE* ToPtr() const
		{
			if(GetEnvTypeId() == Schematyc2::GetEnvTypeId<TYPE>()) // #SchematycTODO : Perform type check in actual implementation?
			{
				return static_cast<const TYPE*>(ToVoidPtr());
			}
			return nullptr;
		}
	};
}
