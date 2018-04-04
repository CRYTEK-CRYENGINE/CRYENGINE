// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "CryAssert.h"
#if defined(USE_CRY_ASSERT)

//! This flag is linked once into every module.
//! With this, it's possible to disable asserts on a per-module scope.
static bool g_bAssertsAreDisabledForThisModule = false;

//! This is copy of the address of the sys_asserts CVar value.
//! We need this pointer so we can access the value even after CSystem is already destroyed.
static int* g_pAssertsCVarAddress = nullptr;

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

//! Store pointer to global address.
//! Call once for each module (on start-up) that participates in CryAssert handling.
void CryAssertSetGlobalFlagAddress(int* pAssertsCVarAddress)
{
	g_pAssertsCVarAddress = pAssertsCVarAddress;
}

//! Check if assert is enabled (the same on every platform).
bool CryAssertIsEnabled()
{
	#if defined(_DEBUG)
	static const bool bDefaultIfUnknown = true;
	#else
	static const bool bDefaultIfUnknown = false;
	#endif
	const bool bGlobalSuppress = g_pAssertsCVarAddress ? *g_pAssertsCVarAddress == 0 : !bDefaultIfUnknown;
	const bool bUserSuppress = gEnv ? gEnv->bIgnoreAllAsserts : !bDefaultIfUnknown;
	const bool bModuleSuppress = g_bAssertsAreDisabledForThisModule;
	return !(bGlobalSuppress || bUserSuppress || bModuleSuppress);
}

namespace Detail
{

void CryAssertHandler(SAssertData const& data, SAssertCond& cond, char const* const szMessage)
{
	CryAssertTrace(szMessage);
	CryAssertHandler(data, cond);
}

NO_INLINE
void CryAssertHandler(SAssertData const& data, SAssertCond& cond)
{
	if (cond.bLogAssert) // Just log assert the first time
	{
		CryLogAssert(data.szExpression, data.szFile, data.line, &cond.bIgnoreAssert);
		cond.bLogAssert = false;
	}

	if (!cond.bIgnoreAssert && CryAssertIsEnabled()) // Don't show assert once it was ignored
	{
		if (CryAssert(data.szExpression, data.szFile, data.line, &cond.bIgnoreAssert))
		{
			__debugbreak();
		}
	}
}
} // namespace Detail

#endif // defined(USE_CRY_ASSERT)
