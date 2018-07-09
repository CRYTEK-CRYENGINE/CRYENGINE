// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "CryAssert.h"
#if defined(USE_CRY_ASSERT)

	#if !CRY_PLATFORM_WINDOWS
void CryLogAssert(const char* _pszCondition, const char* _pszFile, unsigned int _uiLine, bool* _pbIgnore)
{
	// Empty on purpose
}
	#endif

	#if CRY_PLATFORM_DURANGO
		#include <CryAssert/CryAssert_Durango.h>
	#elif CRY_PLATFORM_MAC
		#include <CryAssert/CryAssert_Mac.h>
	#elif CRY_PLATFORM_IOS
		#include <CryAssert/CryAssert_iOS.h>
	#elif CRY_PLATFORM_ANDROID
		#include <CryAssert/CryAssert_Android.h>
	#elif CRY_PLATFORM_LINUX
		#include <CryAssert/CryAssert_Linux.h>
	#elif CRY_PLATFORM_WINDOWS
		#include <CryAssert/CryAssert_Windows.h>
	#elif CRY_PLATFORM_ORBIS
		#include <CryAssert/CryAssert_Orbis.h>
	#else

// Pull in system assert macro
		#include <assert.h>

void CryAssertTrace(const char*, ...)
{
	// Empty on purpose
}

bool CryAssert(const char*, const char*, unsigned int, bool*)
{
	assert(false && "CryAssert not implemented");
	return true;
}

// Restore previous assert definition
		#include "CryAssert.h"

	#endif

//! Check if assert is enabled (the same on every platform).
bool CryAssertIsEnabled()
{
	#if defined(_DEBUG)
	static const bool defaultIfUnknown = true;
	#else
	static const bool defaultIfUnknown = false;
	#endif

	const bool suppressGlobally = gEnv ? gEnv->cryAssertLevel == ECryAssertLevel::Disabled : !defaultIfUnknown;
	const bool suppressedByUser = gEnv ? gEnv->ignoreAllAsserts : !defaultIfUnknown;
#ifdef eCryModule
	const bool suppressedCurrentModule = gEnv && gEnv->pSystem ? !gEnv->pSystem->AreAssertsEnabledForModule(eCryModule) : !defaultIfUnknown;
#else
	const bool suppressedCurrentModule = false;
#endif

	return !(suppressGlobally || suppressedByUser || suppressedCurrentModule);
}

namespace Detail
{

bool CryAssertHandler(SAssertData const& data, SAssertCond& cond, char const* const szMessage)
{
	CryAssertTrace(szMessage);
	return CryAssertHandler(data, cond);
}

NO_INLINE
bool CryAssertHandler(SAssertData const& data, SAssertCond& cond)
{
	if (cond.bLogAssert) // Just log assert the first time
	{
		CryLogAssert(data.szExpression, data.szFile, data.line, &cond.bIgnoreAssert);
		cond.bLogAssert = false;
	}

	if (!cond.bIgnoreAssert && CryAssertIsEnabled()) // Don't show assert once it was ignored
	{
		if (CryAssert(data.szExpression, data.szFile, data.line, &cond.bIgnoreAssert))
			return true;
	}
	return false;
}
} // namespace Detail

#endif // defined(USE_CRY_ASSERT)
