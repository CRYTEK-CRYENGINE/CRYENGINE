// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CQuery_SequentialBase
		//
		// - base class for queries that have children and run them one after another in sequential order
		// - each time a child query finishes (without exception) the derived class will be asked what to do with it
		// - basically, this class is only coordinating child queries, thus no time budget is required to do some complex (or ongoing) execution
		//
		//===================================================================================

		class CQuery_SequentialBase : public CQueryBase
		{
		protected:
			explicit                                 CQuery_SequentialBase(const SCtorContext& ctorContext);
			                                         ~CQuery_SequentialBase();
			bool                                     HasMoreChildrenLeftToInstantiate() const;
			void                                     InstantiateNextChildQueryBlueprint(const std::shared_ptr<CItemList>& pResultingItemsOfPotentialPreviousChildQuery);

		private:
			// CQueryBase
			virtual bool                             OnInstantiateFromQueryBlueprint(const Shared::IVariantDict& runtimeParams, Shared::CUqsString& error) override final;
			virtual EUpdateState                     OnUpdate(Shared::CUqsString& error) override final;
			virtual void                             OnCancel() override final;
			virtual void                             OnGetStatistics(SStatistics& out) const override final;
			// ~CQueryBase

			void                                     OnChildQueryFinished(const SQueryResult& result);
			virtual void                             HandleChildQueryFinishedWithSuccess(const CQueryID& childQueryID, QueryResultSetUniquePtr&& pResultSet) = 0;

		private:
			Shared::CVariantDict                     m_runtimeParams;
			size_t                                   m_indexOfNextChildToInstantiate;
			CQueryID                                 m_queryIDOfCurrentlyRunningChild;
			bool                                     m_bExceptionOccurredInChild;
			Shared::CUqsString                       m_exceptionMessageFromChild;
		};

	}
}
