// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Consider renaming IAny to IRuntimeVariable.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include <CrySchematyc2/TemplateUtils/TemplateUtils_ArrayView.h>

#include "CrySchematyc2/BasicTypes.h"
#include "CrySchematyc2/TypeInfo.h"
#include "CrySchematyc2/Serialization/SerializationUtils.h"

namespace Schematyc2
{
	struct IAny;

	DECLARE_SHARED_POINTERS(IAny)

	enum class EAnyExtension
	{
		Array
	};

	struct IAnyExtension
	{
		virtual ~IAnyExtension() {}
	};

	struct IAnyArrayExtension : public IAnyExtension
	{
		virtual ~IAnyArrayExtension() {}

		virtual CTypeInfo GetElementTypeInfo() const = 0;
		virtual uint32 GetSize() const = 0;
		virtual bool IsFixedSize() const = 0;
		virtual IAny* CreateElement(void* pPlacement) const = 0;
		virtual IAnyPtr CreateElement() const = 0;
		virtual IAny* CloneElement(uint32 idx, void* pPlacement) const = 0;
		virtual IAnyPtr CloneElement(uint32 idx) const = 0;
		virtual bool PushBack(const IAny& value) = 0;
		virtual bool PopBack() = 0;
	};

	struct IAny
	{
		virtual ~IAny() {}

		virtual uint32 GetSize() const = 0;
		virtual CTypeInfo GetTypeInfo() const = 0;
		virtual IAny* Clone(void* pPlacement) const = 0;
		virtual IAnyPtr Clone() const = 0;
		virtual bool ToString(const CharArrayView& str) const = 0;
		virtual GameSerialization::IContextPtr BindSerializationContext(Serialization::IArchive& archive) const = 0; // #SchematycTODO : Can we query domain context instead?
		virtual bool Serialize(Serialization::IArchive& archive, const char* szName, const char* szLabel) = 0;
		virtual IAnyExtension* QueryExtension(EAnyExtension extension) = 0;
		virtual const IAnyExtension* QueryExtension(EAnyExtension extension) const = 0;
		virtual void* ToVoidPtr() = 0;
		virtual const void* ToVoidPtr() const = 0;

		virtual IAny& operator = (const IAny& rhs) = 0; // #SchematycTODO : Do we need to be able to return true/false?
		virtual bool operator == (const IAny& rhs) const = 0;

		inline bool operator != (const IAny& rhs) const
		{
			return !(*this == rhs);
		}

		template <typename TYPE> inline bool Get(TYPE& value) const // #SchematycTODO : Remove and replace with ToPtr()!
		{
			if(GetTypeInfo().GetTypeId().AsEnvTypeId() == GetEnvTypeId<TYPE>())
			{
				value = *static_cast<const TYPE*>(ToVoidPtr());
				return true;
			}
			return false;
		}

		template <typename TYPE> inline TYPE* ToPtr()
		{
			if(GetTypeInfo().GetTypeId().AsEnvTypeId() == GetEnvTypeId<TYPE>()) // #SchematycTODO : Perform type check in actual implementation?
			{
				return static_cast<TYPE*>(ToVoidPtr());
			}
			return nullptr;
		}

		template <typename TYPE> inline const TYPE* ToPtr() const
		{
			if(GetTypeInfo().GetTypeId().AsEnvTypeId() == GetEnvTypeId<TYPE>()) // #SchematycTODO : Perform type check in actual implementation?
			{
				return static_cast<const TYPE*>(ToVoidPtr());
			}
			return nullptr;
		}
	};

	inline bool ToString(const IAny& value, const CharArrayView& output)
	{
		return value.ToString(output);
	}

	inline bool Serialize(Serialization::IArchive& archive, IAny& value, const char* szName, const char* szLabel)
	{
		return value.Serialize(archive, szName, szLabel);
	}
}
