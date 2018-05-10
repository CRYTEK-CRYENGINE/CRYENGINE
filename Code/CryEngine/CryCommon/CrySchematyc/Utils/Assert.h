// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Services/ILog.h"
#include "CrySchematyc/Utils/PreprocessorUtils.h"

#ifndef SCHEMATYC_ASSERTS_ENABLED
	#ifdef _RELEASE
		#define SCHEMATYC_ASSERTS_ENABLED 0
	#else
		#define SCHEMATYC_ASSERTS_ENABLED 1
	#endif
#endif

#if SCHEMATYC_ASSERTS_ENABLED

	#define SCHEMATYC_ASSERT(streamId, expression)                    CRY_ASSERT((expression)); if (!((expression))) { SCHEMATYC_CRITICAL_ERROR(streamId, ("ASSERT" # expression)); }
	#define SCHEMATYC_ASSERT_MESSAGE(streamId, expression, ...)       CRY_ASSERT_MESSAGE((expression),__VA_ARGS__); if (!((expression))) { SCHEMATYC_CRITICAL_ERROR(streamId, __VA_ARGS__); }
	#define SCHEMATYC_ASSERT_FATAL(streamId, expression)              CRY_ASSERT((expression)); if (!((expression))) { SCHEMATYC_FATAL_ERROR(streamId, ("ASSERT" # expression)); }
	#define SCHEMATYC_ASSERT_FATAL_MESSAGE(streamId, expression, ...) CRY_ASSERT_MESSAGE((expression),__VA_ARGS__); if (!((expression))) { SCHEMATYC_FATAL_ERROR(streamId, __VA_ARGS__); }

	#define SCHEMATYC_CORE_ASSERT(expression)                         SCHEMATYC_ASSERT(Schematyc::LogStreamId::Core, (expression))
	#define SCHEMATYC_CORE_ASSERT_MESSAGE(expression, ...)            SCHEMATYC_ASSERT_MESSAGE(Schematyc::LogStreamId::Core, (expression), __VA_ARGS__)
	#define SCHEMATYC_CORE_ASSERT_FATAL(expression)                   SCHEMATYC_ASSERT_FATAL(Schematyc::LogStreamId::Core, (expression))
	#define SCHEMATYC_CORE_ASSERT_FATAL_MESSAGE(expression, ...)      SCHEMATYC_CORE_ASSERT_FATAL_MESSAGE(Schematyc::LogStreamId::Core,, (expression), __VA_ARGS__)

	#define SCHEMATYC_COMPILER_ASSERT(expression)                     SCHEMATYC_ASSERT(Schematyc::LogStreamId::Compiler, (expression))
	#define SCHEMATYC_COMPILER_ASSERT_MESSAGE(expression, ...)        SCHEMATYC_ASSERT_MESSAGE(Schematyc::LogStreamId::Compiler, (expression), __VA_ARGS__)
	#define SCHEMATYC_COMPILER_ASSERT_FATAL(expression)               SCHEMATYC_ASSERT_FATAL(Schematyc::LogStreamId::Compiler, (expression))
	#define SCHEMATYC_COMPILER_ASSERT_FATAL_MESSAGE(expression, ...)  SCHEMATYC_CORE_ASSERT_FATAL_MESSAGE(Schematyc::LogStreamId::Compiler, (expression), __VA_ARGS__)

	#define SCHEMATYC_EDITOR_ASSERT(expression)                       SCHEMATYC_ASSERT(Schematyc::LogStreamId::Editor, (expression))
	#define SCHEMATYC_EDITOR_ASSERT_MESSAGE(expression, ...)          SCHEMATYC_ASSERT_MESSAGE(Schematyc::LogStreamId::Editor, (expression), __VA_ARGS__)
	#define SCHEMATYC_EDITOR_ASSERT_FATAL(expression)                 SCHEMATYC_ASSERT_FATAL(Schematyc::LogStreamId::Editor, (expression))
	#define SCHEMATYC_EDITOR_ASSERT_FATAL_MESSAGE(expression, ...)    SCHEMATYC_CORE_ASSERT_FATAL_MESSAGE(Schematyc::LogStreamId::Editor, (expression), __VA_ARGS__)

	#define SCHEMATYC_ENV_ASSERT(expression)                          SCHEMATYC_ASSERT(Schematyc::LogStreamId::Env, (expression))
	#define SCHEMATYC_ENV_ASSERT_MESSAGE(expression, ...)             SCHEMATYC_ASSERT_MESSAGE(Schematyc::LogStreamId::Env, (expression), __VA_ARGS__)
	#define SCHEMATYC_ENV_ASSERT_FATAL(expression)                    SCHEMATYC_ASSERT_FATAL(Schematyc::LogStreamId::Env, (expression))
	#define SCHEMATYC_ENV_ASSERT_FATAL_MESSAGE(expression, ...)       SCHEMATYC_CORE_ASSERT_FATAL_MESSAGE(Schematyc::LogStreamId::Env, (expression), __VA_ARGS__)

#else

	#define SCHEMATYC_ASSERT(streamId, expression)                    SCHEMATYC_NOP;
	#define SCHEMATYC_ASSERT_MESSAGE(streamId, expression, ...)       SCHEMATYC_NOP;
	#define SCHEMATYC_ASSERT_FATAL(streamId, expression)              SCHEMATYC_NOP;
	#define SCHEMATYC_ASSERT_FATAL_MESSAGE(streamId, expression, ...) SCHEMATYC_NOP;

	#define SCHEMATYC_CORE_ASSERT(expression)                         SCHEMATYC_NOP;
	#define SCHEMATYC_CORE_ASSERT_MESSAGE(expression, ...)            SCHEMATYC_NOP;
	#define SCHEMATYC_CORE_ASSERT_FATAL(expression)                   SCHEMATYC_NOP;
	#define SCHEMATYC_CORE_ASSERT_FATAL_MESSAGE(expression, ...)      SCHEMATYC_NOP;

	#define SCHEMATYC_COMPILER_ASSERT(expression)                     SCHEMATYC_NOP;
	#define SCHEMATYC_COMPILER_ASSERT_MESSAGE(expression, ...)        SCHEMATYC_NOP;
	#define SCHEMATYC_COMPILER_ASSERT_FATAL(expression)               SCHEMATYC_NOP;
	#define SCHEMATYC_COMPILER_ASSERT_FATAL_MESSAGE(expression, ...)  SCHEMATYC_NOP;

	#define SCHEMATYC_EDITOR_ASSERT(expression)                       SCHEMATYC_NOP;
	#define SCHEMATYC_EDITOR_ASSERT_MESSAGE(expression, ...)          SCHEMATYC_NOP;
	#define SCHEMATYC_EDITOR_ASSERT_FATAL(expression)                 SCHEMATYC_NOP;
	#define SCHEMATYC_EDITOR_ASSERT_FATAL_MESSAGE(expression, ...)    SCHEMATYC_NOP;

	#define SCHEMATYC_ENV_ASSERT(expression)                          SCHEMATYC_NOP;
	#define SCHEMATYC_ENV_ASSERT_MESSAGE(expression, ...)             SCHEMATYC_NOP;
	#define SCHEMATYC_ENV_ASSERT_FATAL(expression)                    SCHEMATYC_NOP;
	#define SCHEMATYC_ENV_ASSERT_FATAL_MESSAGE(expression, ...)       SCHEMATYC_NOP;

#endif
