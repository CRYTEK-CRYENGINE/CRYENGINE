// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		struct IQueryResultSet;         // below
		struct SQueryResultSetDeleter;  // below

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
