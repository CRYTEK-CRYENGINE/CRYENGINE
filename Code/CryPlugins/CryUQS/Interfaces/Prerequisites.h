// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//-----------------------------------------------------------------------------------------------

#include <CryCore/Platform/platform.h>
#include <CryExtension/ClassWeaver.h>
#include <CryExtension/CryCreateClassInstance.h>

// yasli::TypeID is used by UQS::Shared::CTypeInfo
#include <CrySerialization/TypeID.h>
#include <CrySerialization/ClassFactory.h>  // weird: this header includes ClassFactory.cpp (yeah, .cpp), which has some methods marked as inline (!) that we make use of, so we need to include that header to prevent unresolved symbols at link time

//-----------------------------------------------------------------------------------------------

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

// convenience macro that can be put into the private section of a class to prevent copying objects
#define UQS_NON_COPYABLE(className)         \
	className(const className&);            \
	className(className&&);                 \
	className& operator=(const className&);	\
	className& operator=(className&&)

// if this is #defined, then CFunctionBlueprint::InstantiateCallHierarchy() will add extra code to check for correct return types inside the call-hierarchy
#define UQS_CHECK_RETURN_TYPE_CONSISTENCY_IN_CALL_HIERARCHY

// if this is #defined, then the CQuery will add some asserts() to ensure proper cleanup once all deferred-evaluators report having finished their work on the remaining items
#define UQS_CHECK_PROPER_CLEANUP_ONCE_ALL_ITEMS_ARE_INSPECTED

// - the maximum number of each, instant- and deferred-evaluators in a query blueprint
// - this constant is used in conjunction with a bitfield inside the working data per item to represent some status information of all evaluators of a query at runtime
#define UQS_MAX_EVALUATORS                  31

namespace UQS
{
	namespace Core
	{

#if UQS_MAX_EVALUATORS == 31
		typedef uint32                      evaluatorsBitfield_t;
#elif UQS_MAX_EVALUATORS == 15
		typedef uint16                      evaluatorsBitfield_t;
#elif UQS_MAX_EVALUATORS == 7
		typedef uint8                       evaluatorsBitfield_t;
#else
#error UQS_MAX_EVALUATORS not defined or weird value
#endif

	}
}

// - these macros can be used on the client-side when registering input parameters of functions, generators and evaluators
// - they have to be put into the SParams struct of the according class
#define UQS_EXPOSE_PARAMS_BEGIN static void Expose(UQS::Client::Internal::CInputParameterRegistry& registry) {
#define UQS_EXPOSE_PARAM(nameOfParam, memberInParamsStruct, idAsFourCharacterString, description)  registry.RegisterParameterType(nameOfParam, idAsFourCharacterString, UQS::Shared::SDataTypeHelper<decltype(memberInParamsStruct)>::GetTypeInfo(), offsetof(SParams, memberInParamsStruct), description)
#define UQS_EXPOSE_PARAMS_END }

// UQS_TODO() macro
#if defined(_MSC_VER)
#define _UQS_STRINGANIZE2(x) #x
#define _UQS_STRINGANIZE1(x) _UQS_STRINGANIZE2(x)
#define UQS_TODO(y) __pragma(message(__FILE__ "(" _UQS_STRINGANIZE1(__LINE__) ") : " "[UQS] TODO >>> " _UQS_STRINGANIZE2(y)))
#else
#define UQS_TODO(y)
#endif

// - this #define controls whether some additional sections in the code will be added to adapt to Schematyc
// - it has to be set from the outside (i. e. at compiler level) to 0 or 1
// - the reasoning behind forcing it to get set from one central place (and *not* defaulting it to some value *here*) is to detect potential configuration-related bugs as early as possible
#ifndef UQS_SCHEMATYC_SUPPORT
#error UQS_SCHEMATYC_SUPPORT has to be set from the outside (i. e. at compiler level) to 0 or 1 (but is not set at all)
#endif


// - specifies the subsystem for use in the frame profiler
// - currently, we don't have specific subsystems for plugins like UQS, so we specify a rather generic one here
#define UQS_PROFILED_SUBSYSTEM_TO_USE PROFILE_GAME
