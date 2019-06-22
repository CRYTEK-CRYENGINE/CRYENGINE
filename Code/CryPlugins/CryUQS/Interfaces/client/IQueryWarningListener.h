// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{

		//===================================================================================
		//
		// IQueryWarningListener - callback used by Core::IQueryManager to notify whenever a warning related to a specific query occurrs during the execution of that query
		//
		//===================================================================================

		struct IQueryWarningListener
		{
			struct SWarningInfo
			{
				explicit        SWarningInfo(const Core::CQueryID& _queryID, int _priority, const char* _szQuerierName, const char* _szQueryBlueprintName, const char* _szWarningMessage);

				Core::CQueryID  queryID;
				int             priority;
				const char*     szQuerierName;
				const char*     szQueryBlueprintName;
				const char*     szWarningMessage;
			};

			virtual void        OnQueryWarning(const SWarningInfo& warningInfo) = 0;

		protected:

			~IQueryWarningListener() {}	// protected non-virtual dtor since deletion through base-class pointers is not intended
		};

		inline IQueryWarningListener::SWarningInfo::SWarningInfo(const Core::CQueryID& _queryID, int _priority, const char* _szQuerierName, const char* _szQueryBlueprintName, const char* _szWarningMessage)
			: queryID(_queryID)
			, priority(_priority)
			, szQuerierName(_szQuerierName)
			, szQueryBlueprintName(_szQueryBlueprintName)
			, szWarningMessage(_szWarningMessage)
		{}

	}
}
