// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: TODO macro messages

-------------------------------------------------------------------------
History:
- 31:03:2009: Created by Filipe Amim

*************************************************************************/
#ifndef __CRY_MACROS_H__
#define __CRY_MACROS_H__

// Disabled by Will with Filipe's consent to keep the compile output more readable...
// #define CRY_SHOW_COMPILE_MESSAGES
// #define CRY_COMPILE_MESSAGES_AS_WARNINGS


// messaging
# define CRY_PP_STRINGIZE(L)					#L
# define CRY_PP_APPLY(function, target)			function(target)
# define CRY_PP_LINE							CRY_PP_APPLY(CRY_PP_STRINGIZE, __LINE__)
# define CRY_PP_FORMAT_DATE(day, month, year)	#day "/" #month "/" #year
# define CRY_PP_FORMAT_FILE_LINE				__FILE__ "(" CRY_PP_LINE ")"
#
# ifdef CRY_COMPILE_MESSAGES_AS_WARNINGS
#	define CRY_PP_FORMAT_MESSAGE(day, month, year, reason, message)		CRY_PP_FORMAT_FILE_LINE " : Cry message(" #reason "): " CRY_PP_FORMAT_DATE(day, month, year) ": " message
# else
#	define CRY_PP_FORMAT_MESSAGE(day, month, year, reason, message)		CRY_PP_FORMAT_FILE_LINE " : cry " #reason ": " CRY_PP_FORMAT_DATE(day, month, year) ": " message
# endif
#
#
# if defined(_MSC_VER)
#	define CRY_PP_PRINT(msg)					__pragma(message(msg))
# else
#	define CRY_PP_PRINT(msg)
# endif
#
#
# if defined(CRY_SHOW_COMPILE_MESSAGES)
#	define CRY_PRINT(msg)								CRY_PP_PRINT(msg)
#	define CRY_MESSAGE(msg)								CRY_PP_PRINT(CRY_PP_FORMAT_FILE_LINE " : " msg)
#	define CRY_TODO(day, month, year, message)			CRY_PP_PRINT(CRY_PP_FORMAT_MESSAGE(day, month, year, TODO, message))
#	define CRY_HACK(day, month, year, message)			CRY_PP_PRINT(CRY_PP_FORMAT_MESSAGE(day, month, year, HACK, message))
#	define CRY_FIXME(day, month, year, message)			CRY_PP_PRINT(CRY_PP_FORMAT_MESSAGE(day, month, year, FIXME, message))
# else
#	define CRY_PRINT(msg)
#	define CRY_MESSAGE(msg)
#	define CRY_TODO(day, month, year, message)
#	define CRY_HACK(day, month, year, message)
#	define CRY_FIXME(day, month, year, message)
# endif

// TODO: improve these macros to behave like the CryUnitAsserts 
#define ASSERT_ARE_EQUAL(expected, actual)									CRY_ASSERT( expected == actual )
#define ASSERT_ARE_NOT_EQUAL(expected, actual)							CRY_ASSERT( expected != actual )
#define ASSERT_IS_TRUE(cond)																CRY_ASSERT(cond )
#define ASSERT_IS_FALSE(cond)																CRY_ASSERT( !cond )
#define ASSERT_IS_NULL(ptr)																	CRY_ASSERT( ptr == NULL )
#define ASSERT_IS_NOT_NULL(ptr)															CRY_ASSERT( ptr != NULL )
#define ASSERT_FLOAT_ARE_EQUAL(expected, actual, epsilon)		CRY_ASSERT( fabs( (expected) - (actual) ) <= (epsilon) )

#endif
