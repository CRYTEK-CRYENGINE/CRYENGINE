// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		struct IQueryResultSet;         // below
		struct SQueryResultSetDeleter;  // below
		class CQueryResultSet;          // actually lives inside the core implemenation; forward-declared here only for IQueryResultSet::GetImplementation()

		//===================================================================================
		//
		// QueryResultSetUnqiuePtr
		//
		//===================================================================================

		typedef std::unique_ptr<IQueryResultSet, SQueryResultSetDeleter> QueryResultSetUniquePtr;

		//===================================================================================
		//
		// IQueryResultSet - all resulting items of a finished query
		//
		//===================================================================================

		struct IQueryResultSet
		{
			struct SResultSetEntry
			{
				explicit                          SResultSetEntry(void* _pItem, float _score);
				void*                             pItem;
				float                             score;
			};

			virtual                               ~IQueryResultSet() {}
			virtual Client::IItemFactory&         GetItemFactory() const = 0;
			virtual size_t                        GetResultCount() const = 0;
			virtual SResultSetEntry               GetResult(size_t index) const = 0;
			virtual const CQueryResultSet&        GetImplementation() const = 0;  // only used by the core implementation: type-safe way of down-casting along the inheritance hierarchy (some local code there needs access to the implementation but only gets an IQueryResultSet passed in)

		private:
			friend struct SQueryResultSetDeleter;
			virtual void                          DeleteSelf() = 0;
		};

		inline IQueryResultSet::SResultSetEntry::SResultSetEntry(void* _pItem, float _score)
			: pItem(_pItem)
			, score(_score)
		{}

		//===================================================================================
		//
		// SQueryResultSetDeleter
		//
		//===================================================================================

		struct SQueryResultSetDeleter
		{
			void operator()(IQueryResultSet* pQueryResultSetToDelete)
			{
				assert(pQueryResultSetToDelete);
				pQueryResultSetToDelete->DeleteSelf();
			}
		};

	}
}
