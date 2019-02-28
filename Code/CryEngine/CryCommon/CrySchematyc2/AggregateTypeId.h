// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Add native support for guid remapping?

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/GUID.h"
#include "CrySchematyc2/Env/EnvTypeId.h"

namespace Schematyc2
{
	class CAggregateTypeId
	{
	public:

		inline CAggregateTypeId()
			: m_domain(EDomain::Unknown)
		{}

		inline CAggregateTypeId(const CAggregateTypeId& rhs)
			: m_domain(rhs.m_domain)
			, m_envTypeId(rhs.m_envTypeId)
			, m_scriptTypeGUID(rhs.m_scriptTypeGUID)
		{}

		inline operator bool () const
		{
			return m_domain != EDomain::Unknown;
		}

		inline EDomain GetDomain() const
		{
			return m_domain;
		}

		inline EnvTypeId AsEnvTypeId() const
		{
			return m_envTypeId;
		}

		inline SGUID AsScriptTypeGUID() const
		{
			return m_scriptTypeGUID;
		}

		inline bool operator == (const CAggregateTypeId& rhs) const
		{
			if(m_domain != rhs.m_domain)
			{
				return false;
			}
			switch(m_domain)
			{
			case EDomain::Env:
				{
					return m_envTypeId == rhs.m_envTypeId;
				}
			case EDomain::Script:
				{
					return m_scriptTypeGUID == rhs.m_scriptTypeGUID;
				}
			}
			return true;
		}

		inline bool operator != (const CAggregateTypeId& rhs) const
		{
			return !(*this == rhs);
		}

		static inline CAggregateTypeId FromEnvTypeId(const EnvTypeId& envTypeId)
		{
			return CAggregateTypeId(envTypeId);
		}

		static inline CAggregateTypeId FromScriptTypeGUID(const SGUID& scriptTypeGUID)
		{
			return CAggregateTypeId(scriptTypeGUID);
		}

	private:

		explicit inline CAggregateTypeId(const EnvTypeId& envTypeId)
			: m_domain(EDomain::Env)
			, m_envTypeId(envTypeId)
		{}

		explicit inline CAggregateTypeId(const SGUID& scriptTypeGUID)
			: m_domain(EDomain::Script)
			, m_scriptTypeGUID(scriptTypeGUID)
		{}

		EDomain   m_domain;
		// #SchematycTODO : Store m_envTypeId and m_scriptTypeGUID in a union?
		EnvTypeId m_envTypeId;
		SGUID     m_scriptTypeGUID;
	};

	template <typename TYPE> const CAggregateTypeId& GetAggregateTypeId()
	{
		static const CAggregateTypeId s_aggregateTypeId = CAggregateTypeId::FromEnvTypeId(GetEnvTypeId<TYPE>());
		return s_aggregateTypeId;
	}
}
