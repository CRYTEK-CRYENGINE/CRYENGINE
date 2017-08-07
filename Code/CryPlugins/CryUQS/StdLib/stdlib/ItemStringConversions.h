// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

#include <CryAISystem/INavigationSystem.h>

namespace UQS
{
	namespace StdLib
	{

		//===================================================================================
		//
		// string-conversion functions for item types that can have a textual representation
		//
		//===================================================================================

		// NavigationAgentTypeID
		bool NavigationAgentTypeID_Serialize(Serialization::IArchive& archive, NavigationAgentTypeID& value, const char* szName, const char* szLabel);

	}
}
