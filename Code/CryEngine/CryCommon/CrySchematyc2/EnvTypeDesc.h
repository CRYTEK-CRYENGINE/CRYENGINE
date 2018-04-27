// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Rename file!

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/Any.h"
#include "CrySchematyc2/IEnvTypeDesc.h"
#include "CrySchematyc2/TypeInfo.h"

namespace Schematyc2
{
	template <typename TYPE> inline EEnvTypeCategory GetEnvTypeCategory()
	{
		if(std::is_fundamental<TYPE>::value)
		{
			return EEnvTypeCategory::Fundamental;
		}
		else if(std::is_enum<TYPE>::value)
		{
			return EEnvTypeCategory::Enumeration;
		}
		else if(std::is_class<TYPE>::value)
		{
			return EEnvTypeCategory::Class;
		}
		else
		{
			SCHEMATYC2_SYSTEM_FATAL_ERROR("Unable to discern type category!");
		}
	}

	template <typename TYPE> class CEnvTypeDesc: public IEnvTypeDesc
	{
	public:

		inline CEnvTypeDesc(const SGUID& guid, const char* szName, const char* szDescription, const TYPE& defaultValue, EEnvTypeFlags flags)
			: m_guid(guid)
			, m_name(szName)
			, m_description(szDescription)
			, m_defaultValue(defaultValue)
			, m_flags(flags)
		{}

		// ITypeDesc

		virtual EnvTypeId GetEnvTypeId() const override // #SchematycTODO : Remove, we can access type id from type info!!!
		{
			return Schematyc2::GetEnvTypeId<TYPE>();
		}

		virtual SGUID GetGUID() const override
		{
			return m_guid;
		}

		virtual const char* GetName() const override
		{
			return m_name.c_str();
		}

		virtual const char* GetDescription() const override
		{
			return m_description.c_str();
		}

		virtual CTypeInfo GetTypeInfo() const override
		{
			return Schematyc2::GetTypeInfo<TYPE>();
		}

		virtual EEnvTypeCategory GetCategory() const override
		{
			return GetEnvTypeCategory<TYPE>();
		}

		virtual EEnvTypeFlags GetFlags() const override
		{
			return m_flags;
		}

		virtual IAnyPtr Create() const override
		{
			return MakeAnyShared(m_defaultValue);
		}

		// ITypeDesc

	private:

		string        m_name;
		SGUID         m_guid;
		string        m_description;
		TYPE          m_defaultValue;
		EEnvTypeFlags m_flags;
	};

	template <typename TYPE> inline IEnvTypeDescPtr MakeEnvTypeDescShared(const SGUID& guid, const char* szName, const char* szDescription, const TYPE& defaultValue, EEnvTypeFlags flags)
	{
		return IEnvTypeDescPtr(std::make_shared<CEnvTypeDesc<TYPE> >(guid, szName, szDescription, defaultValue, flags));
	}
}
