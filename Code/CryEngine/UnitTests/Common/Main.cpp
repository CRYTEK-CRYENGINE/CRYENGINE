// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <CryCore/Platform/platform.h>
#include <CryCore/Platform/platform_impl.inl>
#include <CryCore/Platform/CryPlatformDefines.h>
#include "UnitTest.h"

#if CRY_PLATFORM_WINDOWS
#include <tchar.h>
#endif

void GTestPreAssertCallback(bool& inout_logAssert, bool& inout_ignoreAssert, const char* msg, const char* pszFile, unsigned int uiLine)
{
	ADD_FAILURE() << "Assert on " << pszFile << ":" << uiLine << "\n" << msg;
}

GTEST_API_ int main(int argc, char **argv)
{
	// needed in CryCommonUnitTests
#ifndef SYS_ENV_AS_STRUCT
	if (!gEnv)
	{
		gEnv = new SSystemGlobalEnvironment;
	}
#endif
#ifdef USE_CRY_ASSERT
	Cry::Assert::ShowDialogOnAssert(false);
	Cry::Assert::SetCustomAssertCallback(&GTestPreAssertCallback);
#endif

// Essential step to allow editor plugins to be delay loaded
// Editor plugin DLLs are placed in a sub folder, thus can't be loaded by the test executable directly.
// See also linker flag /DELAYLOAD
#if CRY_PLATFORM_WINDOWS && defined(EDITOR_PLUGIN_UNIT_TEST)
	TCHAR pluginDir[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, pluginDir);
	_tcscat(pluginDir, TEXT("\\EditorPlugins"));
	SetDllDirectory(pluginDir);
#endif

	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
