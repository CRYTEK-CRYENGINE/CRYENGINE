// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"  // the actual location would be "stdlib/StdAfx.h", but for win64_no_uber builds, we added "stdlib" as an include path to make it happy
#include "StdLibRegistration.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace StdLib
	{

		void CStdLibRegistration::InstantiateAllFactoriesForRegistration()
		{
			InstantiateItemFactoriesForRegistration();
			InstantiateFunctionFactoriesForRegistration();
			InstantiateGeneratorFactoriesForRegistration();
			InstantiateInstantEvaluatorFactoriesForRegistration();
			InstantiateDeferredEvaluatorFactoriesForRegistration();
		}

	}
}
