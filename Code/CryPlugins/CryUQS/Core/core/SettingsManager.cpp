// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CSettingsManager
		//
		//===================================================================================

		float CSettingsManager::GetTimeBudgetInSeconds() const
		{
			return SCvars::timeBudgetInSeconds;
		}

		void CSettingsManager::SetTimeBudgetInSeconds(float timeBudgetInSeconds)
		{
			SCvars::timeBudgetInSeconds = timeBudgetInSeconds;
		}

	}
}
