// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// SDebugCameraView
		//
		// - camera view for interactive item debugging in the 3D world
		// - gets passed in from game code to the IHub on each update call
		//
		//===================================================================================

		struct SDebugCameraView
		{
			Vec3   pos;
			Vec3   dir;
		};

		//===================================================================================
		//
		// IQueryHistoryManager
		//
		// - gives access to the histories and allows to control them
		// - this goes hand in hand with the IQueryHistoryConsumer
		// - there are always 2 query histories around: the "live" one which grows whenever a single query instance finishes and a "deserialized" one which was loaded from disk
		//
		//===================================================================================

		struct IQueryHistoryManager
		{

			//===================================================================================
			//
			// EHistoryOrigin
			//
			//===================================================================================

			enum EHistoryOrigin
			{
				Live         = 0,   // designates the "live" query history to which more and more historic queries will be added whenever a single query instance has just finished running
				Deserialized = 1    // designates the "deserialized" query history which got loaded from disk; loading a new one will discard the previously loaded one (but never affect the "live" one, of course)
			};

			//===================================================================================
			//
			// SEvaluatorDrawMasks
			//
			// - used when debug-rendering the items in the 3D world
			// - each bit represents the index inside the evaluator blueprints
			// - it specifies whether that particular evaluator's score should be used to compute the color for drawing the item
			// - as such, we can "visualize" the score of individual evaluators better by masking out evaluators that we don't want to have an impact on the item's drawing color
			//
			//===================================================================================

			struct SEvaluatorDrawMasks
			{
				evaluatorsBitfield_t        maskInstantEvaluators;
				evaluatorsBitfield_t        maskDeferredEvaluators;

				explicit                    SEvaluatorDrawMasks(evaluatorsBitfield_t _maskInstantEvaluators, evaluatorsBitfield_t _maskDeferredEvaluators);
				static SEvaluatorDrawMasks  CreateAllBitsSet();
				static SEvaluatorDrawMasks  CreateNoBitSet();
			};

			virtual                         ~IQueryHistoryManager() {}

			//
			// listener management
			//

			virtual void                    RegisterQueryHistoryListener(IQueryHistoryListener* pListener) = 0;
			virtual void                    UnregisterQueryHistoryListener(IQueryHistoryListener* pListener) = 0;

			//
			// - 3D debug rendering of the items and general debug primitives
			// - if given SDebugCameraView is a valid pointer, it will also figure out which item is currently focused by the camera and fires according event(s)
			//

			virtual void                    UpdateDebugRendering3D(const SDebugCameraView* pOptionalView, const SEvaluatorDrawMasks& evaluatorDrawMasks) = 0;

			//
			// Saving of the "live" query history and loading a previously saved one back into memory. When loading one back, the "live" one will *not* be affected and just keep
			// growing during gameplay.
			//

			virtual bool                    SerializeLiveQueryHistory(const char* szXmlFilePath, Shared::IUqsString& error) = 0;
			virtual bool                    DeserializeQueryHistory(const char* szXmlFilePath, Shared::IUqsString& error) = 0;

			//
			// selects the query history from which we will be able to select one of its historic queries for rendering their debug primitives in the 3D world
			//

			virtual void                    MakeQueryHistoryCurrent(EHistoryOrigin whichHistory) = 0;

			//
			// returns which query history is currently the one from which a historic query can be picked for debug rendering
			//

			virtual EHistoryOrigin          GetCurrentQueryHistory() const = 0;

			//
			// Clears all historic queries from the given query history in memory. This can be used to free some memory or clear entries in a UI control.
			//

			virtual void                    ClearQueryHistory(EHistoryOrigin whichHistory) = 0;

			//
			// outputs information of given query to the receiver
			//

			virtual void                    EnumerateSingleHistoricQuery(EHistoryOrigin whichHistory, const CQueryID& queryIDToEnumerate, IQueryHistoryConsumer& receiver) const = 0;

			//
			// outputs a list of all historic queries of given query history into the receiver
			//

			virtual void                    EnumerateHistoricQueries(EHistoryOrigin whichHistory, IQueryHistoryConsumer& receiver) const = 0;

			//
			// From the current query history, this method picks the one historic query whose debug primitives shall be rendered in the 3D world.
			// Also, the potential item that the camera focuses is one of the items of this historic query.
			//

			virtual void                    MakeHistoricQueryCurrentForInWorldRendering(EHistoryOrigin whichHistory, const CQueryID& queryIDToMakeCurrent) = 0;

			//
			// Goes through all instant/deferred evaluators of given query and enumerates the names of all of their blueprints.
			// Has no effect if given queryID does not exist.
			//

			virtual void                    EnumerateInstantEvaluatorNames(EHistoryOrigin fromWhichHistory, const CQueryID& idOfQueryUsingTheseEvaluators, IQueryHistoryConsumer& receiver) = 0;
			virtual void                    EnumerateDeferredEvaluatorNames(EHistoryOrigin fromWhichHistory, const CQueryID& idOfQueryUsingTheseEvaluators, IQueryHistoryConsumer& receiver) = 0;

			//
			// counterpart of MakeHistoricQueryCurrentForInWorldRendering()
			//

			virtual CQueryID                GetCurrentHistoricQueryForInWorldRendering(EHistoryOrigin fromWhichHistory) const = 0;

			//
			// outputs details of the given historic query of given history to the receiver
			//

			virtual void                    GetDetailsOfHistoricQuery(EHistoryOrigin whichHistory, const CQueryID& queryIDToGetDetailsFor, IQueryHistoryConsumer& receiver) const = 0;

			//
			// Outputs details of the potentially currently focused item to the receiver. There will only ever be at most one item
			// focused by the camera, so no extra information about the query history and historic query is needed here.
			//

			virtual void                    GetDetailsOfFocusedItem(IQueryHistoryConsumer& receiver) const = 0;

			//
			// rough amount of memory (in bytes) used by each of the 2 query histories so far
			//

			virtual size_t                  GetRoughMemoryUsageOfQueryHistory(EHistoryOrigin whichHistory) const = 0;

			//
			// number of historic queries stored in given history
			//

			virtual size_t                  GetHistoricQueriesCount(EHistoryOrigin whichHistory) const = 0;

			//
			// ideal camera position and orientation for a given historic query considering an already existing camera view
			//

			virtual SDebugCameraView        GetIdealDebugCameraView(EHistoryOrigin whichHistory, const CQueryID& queryID, const SDebugCameraView& currentCameraView) const = 0;
		};

		inline IQueryHistoryManager::SEvaluatorDrawMasks::SEvaluatorDrawMasks(evaluatorsBitfield_t _maskInstantEvaluators, evaluatorsBitfield_t _maskDeferredEvaluators)
			: maskInstantEvaluators(_maskInstantEvaluators)
			, maskDeferredEvaluators(_maskDeferredEvaluators)
		{}

		inline IQueryHistoryManager::SEvaluatorDrawMasks IQueryHistoryManager::SEvaluatorDrawMasks::CreateAllBitsSet()
		{
			return SEvaluatorDrawMasks(std::numeric_limits<evaluatorsBitfield_t>::max(), std::numeric_limits<evaluatorsBitfield_t>::max());
		}

		inline IQueryHistoryManager::SEvaluatorDrawMasks IQueryHistoryManager::SEvaluatorDrawMasks::CreateNoBitSet()
		{
			return SEvaluatorDrawMasks(0, 0);
		}

	}
}
