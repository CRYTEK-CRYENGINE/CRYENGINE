// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TimeValueUtil.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Shared
	{

		void CTimeValueUtil::Split(const CTimeValue& time, int* pHours, int* pMinutes, int* pSeconds, int* pMilliseconds)
		{
			const int64 totalMilliseconds = time.GetMilliSecondsAsInt64();

			if (pMilliseconds)
			{
				*pMilliseconds = totalMilliseconds % 1000;
			}

			if (pSeconds)
			{
				*pSeconds = (totalMilliseconds / 1000) % 60;
			}

			if (pMinutes)
			{
				*pMinutes = (totalMilliseconds / (1000 * 60)) % 60;
			}

			if (pHours)
			{
				*pHours = (int)(totalMilliseconds / (1000 * 60 * 60));
			}
		}

	}
}
