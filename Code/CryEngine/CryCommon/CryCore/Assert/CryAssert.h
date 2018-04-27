// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Note: Can't use #pragma once here, since (like assert.h) this file CAN be included more than once.
// Each time it's included, it will re-define assert to match CRY_ASSERT.
// This behavior can be used to "fix" 3rdParty headers that #include <assert.h> (by including CryAssert.h after the 3rdParty header).
// In the case where assert's definition gets trampled, just including CryAssert.h should fix that problem for the remainder of the file.
#ifndef CRY_ASSERT_H_INCLUDED
	#define CRY_ASSERT_H_INCLUDED

// FORCE_STANDARD_ASSERT is set by some tools (like RC)
	#if !defined(_RELEASE) && !defined(FORCE_STANDARD_ASSERT)
		#define USE_CRY_ASSERT
	#else
		#undef USE_CRY_ASSERT
	#endif

//-----------------------------------------------------------------------------------------------------
// Use like this:
// CRY_ASSERT(expression);
// CRY_ASSERT_MESSAGE(expression,"Useful message");
// CRY_ASSERT_TRACE(expression,("This should never happen because parameter %d named %s is %f",iParameter,szParam,fValue));
//-----------------------------------------------------------------------------------------------------

	#if defined(USE_CRY_ASSERT)

enum class ECryAssertLevel
{
	Disabled,
	Enabled,
	FatalErrorOnAssert,
	DebugBreakOnAssert
};

bool CryAssertIsEnabled();
void CryAssertTrace(const char*, ...);
void CryLogAssert(const char*, const char*, unsigned int, bool*);
bool CryAssert(const char*, const char*, unsigned int, bool*);
void CryDebugBreak();

namespace Detail
{
struct SAssertData
{
	char const* const szExpression;
	char const* const szFunction;
	char const* const szFile;
	long const        line;
};

struct SAssertCond
{
	bool bIgnoreAssert;
	bool bLogAssert;
};

bool CryAssertHandler(SAssertData const&, SAssertCond&);
bool  CryAssertHandler(SAssertData const& data, SAssertCond& cond, char const* const szMessage);

template<typename ... TraceArgs>
bool CryAssertHandler(SAssertData const& data, SAssertCond& cond, char const* const szFormattedMessage, TraceArgs ... traceArgs)
{
	CryAssertTrace(szFormattedMessage, traceArgs ...);
	return CryAssertHandler(data, cond);
}
} // namespace Detail

//! The code to insert when assert is used.
		#define CRY_AUX_VA_ARGS(...)    __VA_ARGS__
		#define CRY_AUX_STRIP_PARENS(X) X

		#define CRY_ASSERT_MESSAGE(condition, ...)                                 \
		  do                                                                       \
		  {                                                                        \
		    IF_UNLIKELY (!(condition))                                             \
		    {                                                                      \
		      ::Detail::SAssertData const assertData =                             \
		      {                                                                    \
		        # condition,                                                       \
		        __func__,                                                          \
		        __FILE__,                                                          \
		        __LINE__                                                           \
		      };                                                                   \
		      static ::Detail::SAssertCond assertCond =                            \
		      {                                                                    \
		        false, true                                                        \
		      };                                                                   \
		      if (::Detail::CryAssertHandler(assertData, assertCond, __VA_ARGS__)) \
		        CryDebugBreak();                                                   \
		    }                                                                      \
		    PREFAST_ASSUME(condition);                                         \
		  } while (false)

		#define CRY_ASSERT_TRACE(condition, parenthese_message) \
		  CRY_ASSERT_MESSAGE(condition, CRY_AUX_STRIP_PARENS(CRY_AUX_VA_ARGS parenthese_message))

		#define CRY_ASSERT(condition) CRY_ASSERT_MESSAGE(condition, nullptr)

	#else

//! Use the platform's default assert.
		#include <assert.h>
		#define CRY_ASSERT_TRACE(condition, parenthese_message) assert(condition)
		#define CRY_ASSERT_MESSAGE(condition, ... )             assert(condition)
		#define CRY_ASSERT(condition)                           assert(condition)

	#endif

	#ifdef IS_EDITOR_BUILD
		#undef  Q_ASSERT
		#undef  Q_ASSERT_X
		#define Q_ASSERT(cond)                CRY_ASSERT_MESSAGE(cond, "Q_ASSERT")
		#define Q_ASSERT_X(cond, where, what) CRY_ASSERT_MESSAGE(cond, "Q_ASSERT_X" where what)
	#endif

	template<typename T>
	inline T const& CryVerify(T const& expr, const char* szMessage)
	{
		CRY_ASSERT_MESSAGE(expr, szMessage);
		return expr;
	}

	#define CRY_VERIFY(expr) CryVerify(expr, #expr)

//! This forces boost to use CRY_ASSERT, regardless of what it is defined as.
	#define BOOST_ENABLE_ASSERT_HANDLER
namespace boost
{
inline void assertion_failed_msg(char const* const szExpr, char const* const szMsg, char const* const szFunction, char const* const szFile, long const line)
{
	#ifdef USE_CRY_ASSERT
		::Detail::SAssertData const assertData =
		{
			szExpr, szFunction, szFile, line
		};
		static ::Detail::SAssertCond assertCond =
		{
			false, true
		};
		if (::Detail::CryAssertHandler(assertData, assertCond, szMsg))
			CryDebugBreak();
	#else
		CRY_ASSERT_TRACE(false, ("An assertion failed in boost: expr=%s, msg=%s, function=%s, file=%s, line=%d", szExpr, szMsg, szFunction, szFile, (int)line));
	#endif // USE_CRY_ASSERT
}

inline void assertion_failed(char const* const szExpr, char const* const szFunction, char const* const szFile, long const line)
{
	assertion_failed_msg(szExpr, "BOOST_ASSERT", szFunction, szFile, line);
}
} // namespace boost

// Remember the value of USE_CRY_ASSERT
	#if defined(USE_CRY_ASSERT)
		#define CRY_WAS_USE_ASSERT_SET 1
	#else
		#define CRY_WAS_USE_ASSERT_SET 0
	#endif

#endif

// Note: This ends the "once per compilation unit" part of this file, from here on, the code is included every time
// See the top of this file on the reasoning behind this.
#if defined(USE_CRY_ASSERT)
	#if CRY_WAS_USE_ASSERT_SET != 1
		#error USE_CRY_ASSERT value changed between includes of CryAssert.h (was undefined, now defined)
	#endif
	#undef assert
	#define assert CRY_ASSERT
#else
	#if CRY_WAS_USE_ASSERT_SET != 0
		#error USE_CRY_ASSERT value changed between includes of CryAssert.h (was defined, now undefined)
	#endif
#endif
