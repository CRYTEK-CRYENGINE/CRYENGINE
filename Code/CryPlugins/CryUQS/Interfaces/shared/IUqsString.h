// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Shared
	{

		//===================================================================================
		//
		// IUqsString
		//
		//===================================================================================

		struct IUqsString
		{
			virtual                       ~IUqsString() {}
			virtual void                  Set(const char* szString) = 0;
			virtual void                  Format(const char* szFormat, ...) PRINTF_PARAMS(2, 3) = 0;
			virtual const char*           c_str() const = 0;
		};

	}
}
