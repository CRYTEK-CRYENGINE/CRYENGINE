// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/AggregateTypeId.h"

namespace Schematyc2
{
	enum class ETypeFlags
	{
		None        = 0,
		POD         = BIT(0),
		Fundamental = BIT(1),
		Enum        = BIT(2),
		Class       = BIT(3),
		Array       = BIT(4)
	};

	DECLARE_ENUM_CLASS_FLAGS(ETypeFlags)

	class CTypeInfo
	{
		template <typename TYPE> friend const CTypeInfo& GetTypeInfo();

	public:

		inline CTypeInfo()
			: m_flags(ETypeFlags::None)
		{}

		explicit inline CTypeInfo(const SGUID& scriptTypeGUID, ETypeFlags flags)
			: m_typeId(CAggregateTypeId::FromScriptTypeGUID(scriptTypeGUID))
			, m_flags(flags)
		{}

		inline CTypeInfo(const CTypeInfo& rhs)
			: m_typeId(rhs.m_typeId)
			, m_flags(rhs.m_flags)
		{}

		inline CAggregateTypeId GetTypeId() const
		{
			return m_typeId;
		}

		inline ETypeFlags GetFlags() const
		{
			return m_flags;
		}

	private:

		explicit inline CTypeInfo(const EnvTypeId& typeId, ETypeFlags flags)
			: m_typeId(CAggregateTypeId::FromEnvTypeId(typeId))
			, m_flags(flags)
		{}

		CAggregateTypeId m_typeId;
		ETypeFlags       m_flags;
	};

	template <typename TYPE> inline ETypeFlags GetTypeFlags()
	{
		ETypeFlags typeFlags = ETypeFlags::None;
		if(std::is_pod<TYPE>::value)
		{
			typeFlags |= ETypeFlags::POD;
		}
		if(std::is_fundamental<TYPE>::value)
		{
			typeFlags |= ETypeFlags::Fundamental;
		}
		if(std::is_enum<TYPE>::value)
		{
			typeFlags |= ETypeFlags::Enum;
		}
		if(std::is_class<TYPE>::value)
		{
			typeFlags |= ETypeFlags::Class;
		}
		if(std::is_array<TYPE>::value)
		{
			typeFlags |= ETypeFlags::Array;
		}
		return typeFlags;
	}

	template <typename TYPE> inline const CTypeInfo& GetTypeInfo()
	{
		static const CTypeInfo s_typeInfo(GetEnvTypeId<TYPE>(), GetTypeFlags<TYPE>());
		return s_typeInfo;
	}
}
