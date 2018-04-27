// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{

		//===================================================================================
		//
		// IGenerator
		//
		//===================================================================================

		struct IGenerator
		{
			enum class EUpdateStatus
			{
				StillGeneratingItems,
				FinishedGeneratingItems,
				ExceptionOccurred
			};

			struct SUpdateContext
			{
				explicit                       SUpdateContext(const Core::CQueryID& _queryID, const Core::SQueryBlackboard& _blackboard, Shared::IUqsString& _error);
				Core::CQueryID                 queryID;
				const Core::SQueryBlackboard&  blackboard;
				Shared::IUqsString&            error;
			};

			virtual                            ~IGenerator() {}
			virtual IItemFactory&              GetItemFactory() const = 0;
			virtual EUpdateStatus              Update(const SUpdateContext& updateContext, Core::IItemList& itemListToPopulate) = 0;
		};

		inline IGenerator::SUpdateContext::SUpdateContext(const Core::CQueryID& _queryID, const Core::SQueryBlackboard& _blackboard, Shared::IUqsString& _error)
			: queryID(_queryID)
			, blackboard(_blackboard)
			, error(_error)
		{}

	}
}
