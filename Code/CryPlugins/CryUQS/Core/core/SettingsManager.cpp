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

		const CTimeValue& CSettingsManager::GetTimeBudgetInSeconds() const
		{
			return SCvars::timeBudgetInSeconds;
		}

		void CSettingsManager::SetTimeBudgetInSeconds(const CTimeValue& timeBudgetInSeconds)
		{
			SCvars::timeBudgetInSeconds = timeBudgetInSeconds;
		}

	}
}
