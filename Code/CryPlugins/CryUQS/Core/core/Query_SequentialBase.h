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
			bool                                     HasMoreChildrenLeftToInstantiate() const;
			void                                     InstantiateNextChildQueryBlueprint(const std::shared_ptr<CItemList>& pResultingItemsOfPotentialPreviousChildQuery);

		private:
			// CQueryBase
			virtual bool                             OnStart(const Shared::IVariantDict& runtimeParams, Shared::IUqsString& error) override final;
			virtual EUpdateState                     OnUpdate(const CTimeValue& amountOfGrantedTime, Shared::CUqsString& error) override final;
			virtual void                             OnCancel() override final;
			virtual void                             OnGetStatistics(SStatistics& out) const override final;
			// ~CQueryBase

			virtual void                             HandleChildQueryFinishedWithSuccess(CQueryBase& childQuery) = 0;

		private:
			Shared::CVariantDict                     m_runtimeParams;
			int                                      m_priority;
			size_t                                   m_indexOfNextChildToInstantiate;
			QueryBaseUniquePtr                       m_pCurrentlyRunningChildQuery;
			bool                                     m_bExceptionOccurredUponChildInstantiation;
			Shared::CUqsString                       m_exceptionMessageFromChildInstantiation;
		};

	}
}
