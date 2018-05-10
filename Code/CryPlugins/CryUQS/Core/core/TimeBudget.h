// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CTimeBudget
		//
		//===================================================================================

		class CTimeBudget final : public ITimeBudget
		{
		public:
			void            Restart(const CTimeValue& amountOfTimeFromNowOn);

			// ITimeBudget
			virtual bool    IsExhausted() const override;
			// ~ITimeBudget

		private:
			CTimeValue      m_futureTimestampOfExhaustion;      // timestamp of when we will have run out of granted time
		};

	}
}
