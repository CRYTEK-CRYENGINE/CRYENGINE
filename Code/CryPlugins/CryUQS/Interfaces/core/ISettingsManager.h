// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// ISettingsManager
		//
		//===================================================================================

		struct ISettingsManager
		{
			virtual         ~ISettingsManager() {}

			//
			// Set/get the time budget (in seconds) that will be granted to all running queries combined.
			// Simply controls the "uqs_timeBudgetInSeconds" cvar.
			//

			virtual float   GetTimeBudgetInSeconds() const = 0;
			virtual void    SetTimeBudgetInSeconds(float timeBudgetInSeconds) = 0;
		};

	}
}
