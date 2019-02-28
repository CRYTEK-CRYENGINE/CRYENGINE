// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Project/CryModuleDefs.h>
#define eCryModule eCryM_DynamicResponseSystem
#include <CryCore/Platform/platform.h>

//forward declarations
namespace DRS
{
struct IVariable;
struct IVariableCollection;
struct IResponseActor;
struct IResponseAction;
struct IResponseActionInstance;
struct IResponseCondition;
}

#include <CryCore/Project/ProjectDefines.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/ITimer.h>
#include <CryString/CryString.h>

#include <CrySerialization/yasli/ConfigLocal.h>

#include <CrySerialization/Forward.h>
#include <CrySerialization/ClassFactory.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/Enum.h>
#include <CrySerialization/STL.h>

//Schematyc includes
#include <CrySchematyc/CoreAPI.h>

#include <CryString/HashedString.h>
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>
#include <CryDynamicResponseSystem/IDynamicResponseCondition.h>
#include <CryDynamicResponseSystem/IDynamicResponseAction.h>

#define DrsLogError(x) CryFatalError("%s", x);

#if !defined(_RELEASE)
	#define DRS_COLLECT_DEBUG_DATA
	#define DrsLogWarning(x) CryWarning(VALIDATOR_MODULE_DRS, VALIDATOR_WARNING, "DRS Warning: %s: %s", __FUNCTION__, x);
//#define DrsLogInfo(x) CryLog("DRS Info: %s: %s", __FUNCTION__, x);
	#define DrsLogInfo(x)
#else
	#define DrsLogWarning(x)
	#define DrsLogInfo(x)
#endif

#define REGISTER_DRS_ACTION(_action, _actionname, _color)                       \
  namespace DRS {                                                               \
  SERIALIZATION_CLASS_NAME(IResponseAction, _action, _actionname, _actionname); \
  SERIALIZATION_CLASS_ANNOTATION(IResponseAction, _action, "color", _color); }

#define REGISTER_DRS_CONDITION(_condition, _conditionname, _color)                          \
  namespace DRS {                                                                           \
  SERIALIZATION_CLASS_NAME(IResponseCondition, _condition, _conditionname, _conditionname); \
  SERIALIZATION_CLASS_ANNOTATION(IResponseCondition, _condition, "color", _color); }
