// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

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

		class CSettingsManager final : public ISettingsManager
		{
		public:

			// ISettingsManager
			virtual float   GetTimeBudgetInSeconds() const override;
			virtual void    SetTimeBudgetInSeconds(float timeBudgetInSeconds) override;
			// ~ISettingsManager
		};

	}
}
