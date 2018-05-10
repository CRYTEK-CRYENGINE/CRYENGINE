// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// IQueryHistoryListener
		//
		// - notifies about changes in the query histories through events
		// - this is typically used to have an IQueryHistoryConsumer read back the changed data from the IQueryHistoryManager
		//
		//===================================================================================

		struct IQueryHistoryListener
		{
			enum EEventType
			{
				QueryHistoryDeserialized,                       // an archived query history was just loaded from disk and is now ready to be inspected once having switched from the "live" one to it
				HistoricQueryJustGotCreatedInLiveQueryHistory,  // a new query has just been started
				HistoricQueryJustFinishedInLiveQueryHistory,    // a query has just finished running and its historic counterpart (that was added before already to the "live" query history), now counts as "finished" as well
				LiveQueryHistoryCleared,                        // all historic queries in the "live" query history just go cleared (typically to free some memory)
				DeserializedQueryHistoryCleared,                // all historic queries in the "deserialized" query history just go cleared (typically to free some memory)
				CurrentQueryHistorySwitched,                    // switched from the "live" query history to the "deserialized" one (or vice versa); finished queries will still add their historic counterparts to the "live" one in the background
				DifferentHistoricQuerySelected,                 // scrolling through the list of historic queries of either the "live" or "deserialized" query history (whichever one we switched to before)
				FocusedItemChanged,                             // panning around with the camera and now a different item might be in focus (or even no item may be focused any more)
			};

			struct SEvent
			{
				explicit       SEvent(EEventType _type, const CQueryID& _relatedQueryID);

				EEventType     type;
				CQueryID       relatedQueryID;                // the relatedQueryID will only be valid for certain events which relate to a single HistoricQuery (i. e. EEventType::HistoricQueryJustGotCreatedInLiveQueryHistory and EEventType::HistoricQueryJustFinishedInLiveQueryHistory)
			};

			virtual            ~IQueryHistoryListener() {}
			virtual void       OnQueryHistoryEvent(const SEvent& ev) = 0;
		};

		inline IQueryHistoryListener::SEvent::SEvent(EEventType _type, const CQueryID& _relatedQueryID)
			: type(_type)
			, relatedQueryID(_relatedQueryID)
		{}

	}
}
