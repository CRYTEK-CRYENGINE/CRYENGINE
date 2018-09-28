// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		void CTimeBudget::Restart(const CTimeValue& amountOfTimeFromNowOn)
		{
			m_futureTimestampOfExhaustion = GetGTimer()->GetAsyncTime() + amountOfTimeFromNowOn;
		}

		bool CTimeBudget::IsExhausted() const
		{
			return (GetGTimer()->GetAsyncTime() >= m_futureTimestampOfExhaustion);
		}

	}
}
