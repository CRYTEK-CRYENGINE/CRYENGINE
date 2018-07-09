// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Separate interface and implementation?

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/GUID.h"

namespace Schematyc2
{
	struct IGUIDRemapper
	{
		virtual ~IGUIDRemapper() {}

		virtual bool Empty() const = 0;
		virtual void Bind(const SGUID& from, const SGUID& to) = 0;
		virtual SGUID Remap(const SGUID& from) const = 0;
	};

	class CGUIDRemapper : public IGUIDRemapper
	{
	public:

		virtual bool Empty() const override
		{
			return m_guids.empty();
		}

		virtual void Bind(const SGUID& from, const SGUID& to) override
		{
			m_guids.insert(GUIDMap::value_type(from, to));
		}

		virtual SGUID Remap(const SGUID& from) const override
		{
			GUIDMap::const_iterator itGUID = m_guids.find(from);
			return itGUID != m_guids.end() ? itGUID->second : from;
		}

	private:

		typedef std::unordered_map<SGUID, SGUID> GUIDMap;

		GUIDMap m_guids;
	};
}
