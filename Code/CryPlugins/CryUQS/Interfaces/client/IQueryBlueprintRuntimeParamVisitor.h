// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{

		//===================================================================================
		//
		// IQueryBlueprintRuntimeParamVisitor
		//
		// - allows core::IQueryBlueprint to visit all its required runtime-parameters and call back into client code on each one
		// - the client code could use it to build a dictionary of runtime-parameters for each query-blueprint  (basically:   queryBlueprintName -> [runtimeParamName -> runtimeParamItemFactory])
		// - careful though: when a particular query-blueprint is reloaded, the runtime-parameters might have changed, so one should re-build such a dictionary then
		//
		//===================================================================================

		struct IQueryBlueprintRuntimeParamVisitor
		{
			virtual          ~IQueryBlueprintRuntimeParamVisitor() {}
			virtual void     OnRuntimeParamVisited(const char* szParamName, IItemFactory& itemFactory) = 0;
		};

	}
}
