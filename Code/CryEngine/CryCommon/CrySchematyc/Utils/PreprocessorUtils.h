// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/CryCustomTypes.h>

#ifndef SCHEMATYC_NOP
#define SCHEMATYC_NOP ((void)0)
#endif

#ifndef SCHEMATYC_DEBUG_BREAK
#ifdef _RELEASE
#define SCHEMATYC_DEBUG_BREAK
#else
#define SCHEMATYC_DEBUG_BREAK CryDebugBreak();
#endif
#endif

#ifndef SCHEMATYC_FILE_NAME
#define SCHEMATYC_FILE_NAME __FILE__
#endif

#ifndef SCHEMATYC_LINE_NUMBER
#define SCHEMATYC_LINE_NUMBER __LINE__
#endif

#ifndef SCHEMATYC_FUNCTION_NAME
#ifdef _MSC_VER
#define SCHEMATYC_FUNCTION_NAME __FUNCSIG__
#else
#define SCHEMATYC_FUNCTION_NAME __PRETTY_FUNCTION__
#endif
#endif

#define SCHEMATYC_PP_EMPTY
#define SCHEMATYC_PP_COMMA                        ,
#define SCHEMATYC_PP_LEFT_BRACKET                 (
#define SCHEMATYC_PP_RIGHT_BRACKET                )
#define SCHEMATYC_PP_SEMI_COLON                   ;
#define SCHEMATYC_PP_COLON                        :

#define SCHEMATYC_PP_NUMBER(x)                    x

#define SCHEMATYC_PP_TO_STRING_(x)                #x
#define SCHEMATYC_PP_TO_STRING(x)                 SCHEMATYC_PP_TO_STRING_(x)
#define SCHEMATYC_PP_JOIN_XY_(x, y)               x##y
#define SCHEMATYC_PP_JOIN_XY(x, y)                SCHEMATYC_PP_JOIN_XY_(x, y)
#define SCHEMATYC_PP_JOIN_XYZ_(x, y, z)           x##y##z
#define SCHEMATYC_PP_JOIN_XYZ(x, y, z)            SCHEMATYC_PP_JOIN_XYZ_(x, y, z)
