// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// IDebugMessageCollection - allows Generators, Evaluators and Functions to add arbitrary log messages during execution of a query
		//
		//===================================================================================

		struct IDebugMessageCollection
		{
			virtual         ~IDebugMessageCollection() {}
			virtual void    AddInformation(const char* szFormat, ...) PRINTF_PARAMS(2, 3) = 0;
			virtual void    AddWarning(const char* szFormat, ...) PRINTF_PARAMS(2, 3) = 0;
		};

	}
}
