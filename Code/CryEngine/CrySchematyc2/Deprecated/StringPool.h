// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Deprecated/IStringPool.h>

namespace Schematyc2
{
	// String pool.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	class CStringPool : public IStringPool
	{
	public:

		CStringPool();

		~CStringPool();

		// IStringPool
		virtual CPoolStringData* Insert(const char* data) override;
		virtual void Erase(CPoolStringData* pString) override;
		// ~IStringPool

	private:

		struct StringSortPredicate
		{
			inline bool operator () (uint32 lhs, const CPoolStringData* rhs) const
			{
				return lhs < rhs->CRC32();
			}

			inline bool operator () (const CPoolStringData* lhs, uint32 rhs) const
			{
				return lhs->CRC32() < rhs;
			}
		};

		typedef std::vector<CPoolStringData*> TStringVector;

		TStringVector	m_strings;
	};
}
