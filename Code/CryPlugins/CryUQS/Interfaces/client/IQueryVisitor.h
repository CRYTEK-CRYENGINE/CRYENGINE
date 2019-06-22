// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{

		//===================================================================================
		//
		// IQueryVisitor - allows for visiting all currently running queries in the Core::IQueryManager
		//
		//===================================================================================

		struct IQueryVisitor
		{
			struct SQueryInfo
			{
				explicit        SQueryInfo(const Core::CQueryID& _queryID, const char* _szQuerierName, const char* _szQueryBlueprintName, int _priority, int _numGeneratedItems, int _numRemainingItemsToInspect, int _queryCreatedFrame, const CTimeValue& _queryCreatedTimestamp, int _numConsumedFramesSoFar, int _numElapsedFramesSoFar, const CTimeValue& _timeConsumedSoFar);

				Core::CQueryID  queryID;
				const char*     szQuerierName;
				const char*     szQueryBlueprintName;
				int             priority;
				int             numGeneratedItems;
				int             numRemainingItemsToInspect;
				int             queryCreatedFrame;
				CTimeValue      queryCreatedTimestamp;
				int             numConsumedFramesSoFar;
				int             numElapsedFramesSoFar;
				CTimeValue      timeConsumedSoFar;
			};

			virtual void        OnQueryVisited(const SQueryInfo& queryInfo) = 0;

		protected:

			~IQueryVisitor() {}		// protected non-virtual dtor since deletion through base-class pointers is not intended
		};

		inline IQueryVisitor::SQueryInfo::SQueryInfo(const Core::CQueryID& _queryID, const char* _szQuerierName, const char* _szQueryBlueprintName, int _priority, int _numGeneratedItems, int _numRemainingItemsToInspect, int _queryCreatedFrame, const CTimeValue& _queryCreatedTimestamp, int _numConsumedFramesSoFar, int _numElapsedFramesSoFar, const CTimeValue& _timeConsumedSoFar)
			: queryID(_queryID)
			, szQuerierName(_szQuerierName)
			, szQueryBlueprintName(_szQueryBlueprintName)
			, priority(_priority)
			, numGeneratedItems(_numGeneratedItems)
			, numRemainingItemsToInspect(_numRemainingItemsToInspect)
			, queryCreatedFrame(_queryCreatedFrame)
			, queryCreatedTimestamp(_queryCreatedTimestamp)
			, numConsumedFramesSoFar(_numConsumedFramesSoFar)
			, numElapsedFramesSoFar(_numElapsedFramesSoFar)
			, timeConsumedSoFar(_timeConsumedSoFar)
		{}

	}
}
