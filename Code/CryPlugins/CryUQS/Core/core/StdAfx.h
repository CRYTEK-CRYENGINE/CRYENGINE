// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

// non-Lib projects need eCryModule to be #defined, or else an #error will occur
#define eCryModule eCryM_EnginePlugin

#include "../Interfaces/InterfacesIncludes.h"
#include "../Shared/SharedIncludes.h"

// actual serialization is used for dumping query histories to files
#include <CrySerialization/Forward.h>
#include <CrySerialization/Enum.h>
#include <CrySerialization/Math.h>
#include <CrySerialization/Color.h>
#include <CrySerialization/CryStrings.h>
#include <CrySerialization/IArchiveHost.h>

// - redirects to g_pHub->HaveConsistencyChecksBeenDoneAlready()
// - CFactoryDatabase<>::RegisterFactory() doesn't know about the global g_pHub variable yet due to the way the headers need to be included, so it uses this function instead
// - function is implemented in Hub.cpp
namespace UQS
{
	namespace Core
	{
		bool Hub_HaveConsistencyChecksBeenDoneAlready();
	}
}

#include "Serialize.h"
#include "Logger.h"
#include "FactoryDatabase.h"
#include "ItemFactoryDatabase.h"
#include "DebugRenderPrimitives.h"
#include "DebugRenderWorld.h"
#include "ItemList.h"
#include "ItemSerializationSupport.h"
#include "LeafFunctionReturnValue.h"
#include "InputBlueprint.h"
#include "BlueprintWithInputs.h"
#include "FunctionFactoryDatabase.h"
#include "FunctionBlueprint.h"
#include "FunctionCallHierarchy.h"
#include "ScoreTransformFactory.h"
#include "EvaluationResultTransform.h"
#include "TextualEvaluatorBlueprint.h"
#include "EvaluatorBlueprintBase.h"
#include "InstantEvaluatorFactoryDatabase.h"
#include "InstantEvaluatorBlueprint.h"
#include "DeferredEvaluatorFactoryDatabase.h"
#include "DeferredEvaluatorBlueprint.h"
#include "GeneratorFactoryDatabase.h"
#include "GeneratorBlueprint.h"
#include "GlobalConstantParamsBlueprint.h"
#include "GlobalRuntimeParamsBlueprint.h"
#include "TimeBudget.h"
#include "QueryResultSet.h"
#include "QueryBase.h"
#include "Query_Regular.h"
#include "Query_SequentialBase.h"
#include "Query_Chained.h"
#include "Query_Fallbacks.h"
#include "QueryFactory.h"
#include "QueryBlueprint.h"
#include "QueryBlueprintLibrary.h"
#include "ItemDebugProxies.h"
#include "ItemDebugProxyFactory.h"
#include "QueryHistory.h"
#include "QueryHistoryManager.h"
#include "QueryHistoryInGameGUI.h"
#include "QueryManager.h"
#include "StartupConsistencyChecker.h"
#include "EditorService.h"
#include "Utils.h"
#include "SettingsManager.h"
#if UQS_SCHEMATYC_SUPPORT
#include "SchematycSupport.h"
#endif
#include "Hub.h"
#include "Cvars.h"
#include "DrawUtil2d.h"
#include "GeomUtil.h"
