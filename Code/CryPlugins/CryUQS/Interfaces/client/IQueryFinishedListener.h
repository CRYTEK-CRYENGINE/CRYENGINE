// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{

		//===================================================================================
		//
		// IQueryFinishedListener - callback used by Core::IQueryManager to notify whenever a query finishes
		//
		//===================================================================================

		struct IQueryFinishedListener
		{
			struct SQueryInfo
			{
				explicit        SQueryInfo(const Core::CQueryID& _queryID, const char* _szQuerierName, const char* _szQueryBlueprintName, int _numGeneratedItems, int _numItemsInFinalResultSet, const CTimeValue& _queryCreatedTimestamp, const CTimeValue& _queryFinishedTimestamp, int _numConsumedFrames, int _numElapsedFrames, bool _bExceptionOccurred, const char* _szPotentialExceptionMessage);

				Core::CQueryID  queryID;
				const char*     szQuerierName;
				const char*     szQueryBlueprintName;
				int             numGeneratedItems;
				int             numItemsInFinalResultSet;
				CTimeValue      queryCreatedTimestamp;
				CTimeValue      queryFinishedTimestamp;
				int             numConsumedFrames;
				int             numElapsedFrames;
				bool            bExceptionOccurred;
				const char*     szPotentialExceptionMessage;
			};

			virtual void        OnQueryFinished(const SQueryInfo& queryInfo) = 0;

		protected:

			~IQueryFinishedListener() {}	// protected non-virtual dtor since deletion through base-class pointers is not intended
		};

		inline IQueryFinishedListener::SQueryInfo::SQueryInfo(const Core::CQueryID& _queryID, const char* _szQuerierName, const char* _szQueryBlueprintName, int _numGeneratedItems, int _numItemsInFinalResultSet, const CTimeValue& _queryCreatedTimestamp, const CTimeValue& _queryFinishedTimestamp, int _numConsumedFrames, int _numElapsedFrames, bool _bExceptionOccurred, const char* _szPotentialExceptionMessage)
			: queryID(_queryID)
			, szQuerierName(_szQuerierName)
			, szQueryBlueprintName(_szQueryBlueprintName)
			, numGeneratedItems(_numGeneratedItems)
			, numItemsInFinalResultSet(_numItemsInFinalResultSet)
			, queryCreatedTimestamp(_queryCreatedTimestamp)
			, queryFinishedTimestamp(_queryFinishedTimestamp)
			, numConsumedFrames(_numConsumedFrames)
			, numElapsedFrames(_numElapsedFrames)
			, bExceptionOccurred(_bExceptionOccurred)
			, szPotentialExceptionMessage(_szPotentialExceptionMessage)
		{}

	}
}
