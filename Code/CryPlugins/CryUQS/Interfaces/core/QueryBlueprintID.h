// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CQueryBlueprintID
		//
		// - represents a handle to a query blueprint in the IQueryBlueprintLibrary
		// - such handles act as kind of a weak pointer and will never cause crashes in the sense that they might start to dangle (even if a blueprint gets removed from the library)
		// - the worst thing that can happen is, that a handle will eventually point to an "empty" slot after its blueprint got removed
		// - in such a case, if trying to run a query, it will simply not start
		//
		//===================================================================================

		class CQueryBlueprintID
		{
			friend class CQueryBlueprintLibrary;

		public:
			                     CQueryBlueprintID();
			bool                 IsOrHasBeenValid() const;
			const char*          GetQueryBlueprintName() const;  // name of the query blueprint that the caller requested; this is mostly for debugging purpose; should not be use for critical stuff as the underlying char buffer is limited in size

		private:
			explicit             CQueryBlueprintID(int32 index, const char* szQueryBlueprintName);

		private:
			int32                m_index;                        // index in CQueryBlueprintLibrary's private array of blueprints
			char                 m_queryBlueprintName[128];      // name of the query blueprint that the caller requested (notice: there might not necessarily be a blueprint under this name in the library, though); FIXME: turn this into a DLL-boundary safe object geared around std::unique_ptr
			static const int32   s_invalidIndex = -1;
		};

		inline CQueryBlueprintID::CQueryBlueprintID()
			: m_index(s_invalidIndex)
		{
			m_queryBlueprintName[0] = '\0';
		}

		inline CQueryBlueprintID::CQueryBlueprintID(int32 index, const char* szQueryBlueprintName)
			: m_index(index)
		{
			cry_strcpy(m_queryBlueprintName, szQueryBlueprintName);
		}

		inline bool CQueryBlueprintID::IsOrHasBeenValid() const
		{
			return m_index != s_invalidIndex;
		}

		inline const char* CQueryBlueprintID::GetQueryBlueprintName() const
		{
			return m_queryBlueprintName;
		}

	}
}
