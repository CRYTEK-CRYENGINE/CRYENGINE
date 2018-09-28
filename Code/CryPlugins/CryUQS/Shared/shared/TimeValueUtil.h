// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Shared
	{

		class CTimeValueUtil
		{
		public:

			static void Split(const CTimeValue& time, mpfloat* pHours, mpfloat* pMinutes, mpfloat* pSeconds, mpfloat* pMilliseconds);

		};

		inline void CTimeValueUtil::Split(const CTimeValue& time, mpfloat* pHours, mpfloat* pMinutes, mpfloat* pSeconds, mpfloat* pMilliseconds)
		{
			if (pMilliseconds)
			{
				*pMilliseconds = time.GetMilliSeconds();
			}

			if (pSeconds)
			{
				*pSeconds = time.GetSeconds();
			}

			if (pMinutes)
			{
				*pMinutes = time.GetSeconds() / 60;
			}

			if (pHours)
			{
				*pHours = time.GetSeconds() / (60 * 60);
			}
		}

	}
}
