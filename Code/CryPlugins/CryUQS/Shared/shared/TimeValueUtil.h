// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Shared
	{

		class CTimeValueUtil
		{
		public:

			static void Split(const CTimeValue& time, int* pHours, int* pMinutes, int* pSeconds, int* pMilliseconds);

		};

	}
}
