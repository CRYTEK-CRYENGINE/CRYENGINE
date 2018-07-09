// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// IQueryHistoryConsumer
		//
		// - reads back data from an IQueryHistoryManager
		// - basically, it's the IQueryHistoryManager that calls back into this class to provide it with the requested data
		// - usually, reading data happens in an event-driven way by getting notified through an IQueryHistoryListener
		//
		//===================================================================================

		struct IQueryHistoryConsumer
		{
			// passed in to AddOrUpdateHistoricQuery()
			struct SHistoricQueryOverview
			{
				explicit                  SHistoricQueryOverview(const ColorF& _color, const char *_szQuerierName, const CQueryID& _queryID, const CQueryID& _parentQueryID, const char* _szQueryBlueprintName, size_t _numGeneratedItems, size_t _numResultingItems, const CTimeValue& _timeElapsedUntilResult, const CTimeValue& _timestampQueryCreated, const CTimeValue& _timestampQueryDestroyed, bool _bFoundTooFewItems, bool _bQueryEncounteredAnException, bool _bQueryEncounteredSomeWarnings);

				// TODO: itemType of the generated items

				ColorF                    color;
				const char *              szQuerierName;
				const CQueryID&           queryID;
				const CQueryID&           parentQueryID;
				const char*               szQueryBlueprintName;
				size_t                    numGeneratedItems;
				size_t                    numResultingItems;
				CTimeValue                timeElapsedUntilResult;
				CTimeValue                timestampQueryCreated;
				CTimeValue                timestampQueryDestroyed;
				bool                      bFoundTooFewItems;
				bool                      bQueryEncounteredAnException;
				bool                      bQueryEncounteredSomeWarnings;
			};

			virtual                       ~IQueryHistoryConsumer() {}

			// - called when requesting to enumerate all historic queries via IQueryHistoryManager::EnumerateHistoricQueries()
			// - also called when requesting information of a specific query via IQueryHistoryManager::EnumerateSingleHistricQuery()
			virtual void                  AddOrUpdateHistoricQuery(const SHistoricQueryOverview& overview) = 0;

			// - called when requesting details about a specific historic query via IQueryHistoryManager::GetDetailsOfHistoricQuery()
			// - details about the historic query may be comprised of multiple text lines, hence this method may get called multiple times in a row
			virtual void                  AddTextLineToCurrentHistoricQuery(const ColorF& color, const char* szFormat, ...) = 0;

			// - called when requesting details about the potentially focused item in the 3D world
			// - this function may or may not get called multiple times in a row, depending on the amount of details and whether an item is focused at all
			virtual void                  AddTextLineToFocusedItem(const ColorF& color, const char* szFormat, ...) = 0;

			// - called when requesting the names of all evaluators involved in a specific historic query
			// - see IQueryHistoryManager::EnumerateInstantEvaluatorNames() and EnumerateDeferredEvaluatorNames()
			virtual void                  AddInstantEvaluatorName(const char* szInstantEvaluatorName) = 0;
			virtual void                  AddDeferredEvaluatorName(const char* szDeferredEvaluatorName) = 0;
		};

		inline IQueryHistoryConsumer::SHistoricQueryOverview::SHistoricQueryOverview(const ColorF& _color, const char *_szQuerierName, const CQueryID& _queryID, const CQueryID& _parentQueryID, const char* _szQueryBlueprintName, size_t _numGeneratedItems, size_t _numResultingItems, const CTimeValue& _timeElapsedUntilResult, const CTimeValue& _timestampQueryCreated, const CTimeValue& _timestampQueryDestroyed, bool _bFoundTooFewItems, bool _bQueryEncounteredAnException, bool _bQueryEncounteredSomeWarnings)
			: color(_color)
			, szQuerierName(_szQuerierName)
			, queryID(_queryID)
			, parentQueryID(_parentQueryID)
			, szQueryBlueprintName(_szQueryBlueprintName)
			, numGeneratedItems(_numGeneratedItems)
			, numResultingItems(_numResultingItems)
			, timeElapsedUntilResult(_timeElapsedUntilResult)
			, timestampQueryCreated(_timestampQueryCreated)
			, timestampQueryDestroyed(_timestampQueryDestroyed)
			, bFoundTooFewItems(_bFoundTooFewItems)
			, bQueryEncounteredAnException(_bQueryEncounteredAnException)
			, bQueryEncounteredSomeWarnings(_bQueryEncounteredSomeWarnings)
		{
			// nothing
		}

	}
}
