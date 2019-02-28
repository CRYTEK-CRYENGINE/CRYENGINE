// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Prerequisites.h"

#if UQS_SCHEMATYC_SUPPORT
#include <CrySchematyc/CoreAPI.h>
#endif

#include <CrySchematyc/Utils/EnumFlags.h>

#include "shared/TypeInfo.h"
#include "shared/DataTypeHelper.h"
#include "shared/IUqsString.h"
#include "shared/IVariantDict.h"
#include "shared/GUIDHelper.h"

#include "core/IDebugRenderWorld.h"
#include "core/IFactoryDatabase.h"
#include "core/IItemDebugProxies.h"
#include "core/IItemDebugProxyFactory.h"

#include "client/IItemConverter.h"
#include "client/IItemFactory.h"
#include "client/IParamsHolder.h"
#include "client/IItemMonitor.h"

#include "core/IItemFactoryDatabase.h"
#include "core/IItemList.h"

#include "datasource/ISyntaxErrorCollector.h"

#include "client/IInputParameterRegistry.h"

#include "core/IInputBlueprint.h"
#include "core/ItemIterationContext.h"
#include "core/ITimeBudget.h"
#include "core/QueryBlackboard.h"
#include "core/QueryID.h"
#include "core/ILeafFunctionReturnValue.h"

#include "client/IFunction.h"
#include "client/IFunctionFactory.h"

#include "core/IFunctionFactoryDatabase.h"
#include "core/ItemEvaluationResult.h"
#include "core/IScoreTransformFactory.h"

#include "core/ITextualEvaluatorBlueprint.h"

#include "client/IInstantEvaluator.h"
#include "client/IInstantEvaluatorFactory.h"

#include "core/IInstantEvaluatorFactoryDatabase.h"

#include "client/IDeferredEvaluator.h"
#include "client/IDeferredEvaluatorFactory.h"

#include "core/IDeferredEvaluatorFactoryDatabase.h"

#include "client/IGenerator.h"
#include "client/IGeneratorFactory.h"
#include "client/IQueryBlueprintRuntimeParamVisitor.h"

#include "core/IGeneratorFactoryDatabase.h"
#include "core/IGeneratorBlueprint.h"
#include "core/IGlobalConstantParamsBlueprint.h"
#include "core/IGlobalRuntimeParamsBlueprint.h"
#include "core/IQueryBlueprint.h"

#include "datasource/IQueryBlueprintLoader.h"
#include "datasource/IQueryBlueprintSaver.h"

#include "core/QueryBlueprintID.h"
#include "core/IQueryBlueprintLibrary.h"
#include "core/IQueryFactory.h"
#include "core/IQueryResultSet.h"
#include "core/QueryResult.h"

#include "client/QueryRequest.h"

#include "core/IQueryManager.h"
#include "core/IQueryHistoryListener.h"
#include "core/IQueryHistoryConsumer.h"
#include "core/IQueryHistoryManager.h"

#include "datasource/IEditorLibraryProvider.h"

#include "editor/IEditorServiceConsumer.h"

#include "core/IItemSerializationSupport.h"
#include "core/IEditorService.h"
#include "core/IUtils.h"
#include "core/ISettingsManager.h"
#include "core/IHub.h"
#include "core/IHubPlugin.h"
