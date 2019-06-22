// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <bitset>
#include <CryCore/Project/CryModuleDefs.h>

// Note: Can't use #pragma once here, since (like assert.h) this file CAN be included more than once.
// Each time it's included, it will re-define assert to match CRY_ASSERT.
// This behavior can be used to "fix" 3rdParty headers that #include <assert.h> (by including CryAssert.h after the 3rdParty header).
// In the case where assert's definition gets trampled, just including CryAssert.h should fix that problem for the remainder of the file.
#ifndef CRY_ASSERT_H_INCLUDED
#define CRY_ASSERT_H_INCLUDED

	// FORCE_STANDARD_ASSERT is set by some tools (like RC)
	#if !defined(_RELEASE) && !defined(FORCE_STANDARD_ASSERT)
	#	define USE_CRY_ASSERT
	#else
	#	undef USE_CRY_ASSERT
	#endif

	//-----------------------------------------------------------------------------------------------------
	// Use like this:
	// CRY_ASSERT(expression);
	// CRY_ASSERT_MESSAGE(expression,"Useful message");
	// CRY_ASSERT_TRACE(expression,("This should never happen because parameter %d named %s is %f",iParameter,szParam,fValue));
	//-----------------------------------------------------------------------------------------------------

	#if defined(USE_CRY_ASSERT)

		// defined in CryAssert_impl.h
		const char* GetCryModuleName(uint cryModuleId);

		namespace Cry
		{
			namespace Assert
			{
				typedef void(*CustomPreAssertCallback)(bool& inout_logAssert, bool& inout_ignoreAssert, const char* msg, const char* pszFile, unsigned int uiLine);

				enum class ELevel
				{
					Disabled,
					Enabled,
					FatalErrorOnAssert,
					DebugBreakOnAssert
				};

				bool AreAssertsEnabledForModule(uint32 moduleId);
				void DisableAssertionsForModule(uint32 moduleId);

				bool IsInAssert();

				bool IgnoreAllAsserts();
				void IgnoreAllAsserts(bool ignoreAsserts);

				bool ShowDialogOnAssert();
				void ShowDialogOnAssert(bool showAssertDialogue);

				bool LogAssertsAlways();
				void LogAssertsAlways(bool logAlways);

				ELevel GetAssertLevel();
				bool IsAssertLevel(ELevel assertLevel);
				void SetAssertLevel(ELevel assertLevel);

				CustomPreAssertCallback GetCustomPreAssertCallback();
				bool SetCustomAssertCallback(CustomPreAssertCallback customAssertCallback);

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

					struct SSettings
					{
						std::bitset<eCryM_Num> disabledAssertModules; // Used to check if CryAssert is enabled for a specific module
						CustomPreAssertCallback customPreAssertCallback = nullptr;
						ELevel assertLevel = ELevel::Enabled;
						bool isInAssertHandler = false;
						bool ignoreAllAsserts = false;
						bool showAssertDialog = true;
						bool logAlways = false;
						// used to ensure we only assert in one thread at a time
						volatile int lock = 0;
					};

					bool CryAssertIsEnabled();
					void IsInAssert(bool bIsInAssertHandler);
					void CryAssertFormat(const char* szFormat, ...);
					bool CryAssertHandler(SAssertData const& data, SAssertCond& cond);

					template<typename ... TraceArgs>
					bool CryAssertHandler(SAssertData const& data, SAssertCond& cond, char const* const szFormattedMessage, TraceArgs ... traceArgs)
					{
						CryAssertFormat(szFormattedMessage, traceArgs ...);
						return CryAssertHandler(data, cond);
					}
				} // namespace Detail
			} // namespace Assert
		} // namespace Cry

		//! The code to insert when assert is used.
		//! There should be no need to use this macro directly in your code.
		//! Use CRY_ASSERT[_MESSAGE]  and CRY_VERIFY[_WITH_MESSAGE] instead.
		#define CRY_ASSERT_MESSAGE_IMPL(condition, szCondition, file, line, ...) \
			do                                                 \
			{                                                  \
				if (Cry::Assert::Detail::CryAssertIsEnabled()) \
				{                                              \
					IF_UNLIKELY (!(condition))                 \
					{                                          \
						Cry::Assert::Detail::SAssertData const assertData = \
						{                                      \
							szCondition,                       \
							__func__,                          \
							file,                              \
							line };                            \
						static Cry::Assert::Detail::SAssertCond assertCond = \
							{ false, true };                   \
						if (Cry::Assert::Detail::CryAssertHandler(assertData, assertCond, __VA_ARGS__)  \
								|| Cry::Assert::IsAssertLevel(Cry::Assert::ELevel::DebugBreakOnAssert)) \
							CryDebugBreak();                   \
					}                                          \
				}                                              \
				PREFAST_ASSUME(condition);                     \
			} while (false)	

		#define CRY_ASSERT_MESSAGE(condition, ...) CRY_ASSERT_MESSAGE_IMPL(condition, # condition, __FILE__, __LINE__, __VA_ARGS__)

		#define CRY_AUX_VA_ARGS(...)    __VA_ARGS__
		#define CRY_AUX_STRIP_PARENS(X) X
		#define CRY_ASSERT_TRACE(condition, parenthese_message) \
			CRY_ASSERT_MESSAGE(condition, CRY_AUX_STRIP_PARENS(CRY_AUX_VA_ARGS parenthese_message))

		#define CRY_ASSERT(condition) CRY_ASSERT_MESSAGE(condition, nullptr)

	#else // defined(USE_CRY_ASSERT)

		//! Use the platform's default assert.
		#include <assert.h>
		#define CRY_ASSERT_TRACE(condition, parenthese_message) assert(condition)
		#define CRY_ASSERT_MESSAGE_IMPL(condition, szCondition, file, line, ...) assert(condition)
		#define CRY_ASSERT_MESSAGE(condition, ... )             assert(condition)
		#define CRY_ASSERT(condition)                           assert(condition)

	#endif // defined(USE_CRY_ASSERT)

	#ifdef IS_EDITOR_BUILD
		#undef  Q_ASSERT
		#undef  Q_ASSERT_X
		// Q_ASSERT handler
		// Qt uses expression with chained asserts separated by ',' like: { return Q_ASSERT(n >= 0), Q_ASSERT(n < size()), QChar(m_data[n]); }
		// We're always passing 0 into CRY_ASSERT_MESSAGE_IMPL, as the handler is called in the failed branch.
		// Thus we avoid evaluating the condition twice. (Also, passing it on caused a compilation error.)
		#define  CRY_ASSERT_HANDLER_QT(cond, message)                                           \
						([&] {                                                                  \
							CRY_ASSERT_MESSAGE_IMPL(0/*see comment*/, # cond, __FILE__, __LINE__, message); \
							return static_cast<void>(0);                                        \
						} ())

		#define Q_ASSERT(cond)                ((cond) ? static_cast<void>(0) : CRY_ASSERT_HANDLER_QT(cond, "Q_ASSERT"))
		#define Q_ASSERT_X(cond, where, what) ((cond) ? static_cast<void>(0) : CRY_ASSERT_HANDLER_QT(cond, "Q_ASSERT_X" where what))
	#endif

	namespace Cry
	{
		template<typename T, typename ... Args>
		inline T const& VerifyWithMessage(T const& expr, const char* szExpr, const char* szFile, int line, Args&& ... args)
		{
			CRY_ASSERT_MESSAGE_IMPL(expr, szExpr, szFile, line, std::forward<Args>(args) ...);
			return expr;
		}
	}

	#define CRY_VERIFY(expr)                   Cry::VerifyWithMessage(expr, # expr, __FILE__, __LINE__, nullptr)
	#define CRY_VERIFY_WITH_MESSAGE(expr, ...) Cry::VerifyWithMessage(expr, # expr, __FILE__, __LINE__, __VA_ARGS__)

	//! This forces boost to use CRY_ASSERT, regardless of what it is defined as.
	#define BOOST_ENABLE_ASSERT_HANDLER
	namespace boost
	{
		inline void assertion_failed_msg(char const* const szExpr, char const* const szMsg, char const* const szFunction, char const* const szFile, long const line)
		{
	#ifdef USE_CRY_ASSERT
			::Cry::Assert::Detail::SAssertData const assertData =
			{
				szExpr, szFunction, szFile, line
			};
			static ::Cry::Assert::Detail::SAssertCond assertCond =
			{
				false, true
			};
			::Cry::Assert::Detail::CryAssertHandler(assertData, assertCond, szMsg);
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

#endif // !defined(CRY_ASSERT_H_INCLUDED)

// Note: This ends the "once per compilation unit" part of this file, from here on, the code is included every time
// See the top of this file on the reasoning behind this.

#if defined(USE_CRY_ASSERT)
	#if CRY_WAS_USE_ASSERT_SET != 1
		#error USE_CRY_ASSERT value changed between includes of CryAssert.h (was undefined, now defined)
	#endif
	#undef assert
	#define assert CRY_ASSERT
#elif CRY_WAS_USE_ASSERT_SET != 0
	#error USE_CRY_ASSERT value changed between includes of CryAssert.h (was defined, now undefined)
#endif
