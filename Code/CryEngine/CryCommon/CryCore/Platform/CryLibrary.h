// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef CRYLIBRARY_H__
#define CRYLIBRARY_H__

/*!
   CryLibrary

   Convenience-Macros which abstract the use of DLLs/shared libraries in a platform independent way.
   A short explanation of the different macros follows:

   CrySharedLibrarySupported:
    This macro can be used to test if the current active platform supports shared library calls. The default
    value is false. This gets redefined if a certain platform (Windows or Linux) is desired.

   CrySharedLibraryPrefix:
    The default prefix which will get prepended to library names in calls to CryLoadLibraryDefName
    (see below).

   CrySharedLibraryExtension:
    The default extension which will get appended to library names in calls to CryLoadLibraryDefName
    (see below).

   CryLoadLibrary(libName):
    Loads a shared library.

   CryLoadLibraryDefName(libName):
    Loads a shared library. The platform-specific default library prefix and extension are appended to the libName.
    This allows writing of somewhat platform-independent library loading code and is therefore the function
    which should be used most of the time, unless some special extensions are used (e.g. for plugins).

   CryGetProcAddress(libHandle, procName):
    Import function from the library presented by libHandle.

   CryFreeLibrary(libHandle):
    Unload the library presented by libHandle.

   HISTORY:
    03.03.2004 MarcoK
      - initial version
      - added to CryPlatform
 */

#include <stdio.h>
#include <CryCore/Platform/CryWindows.h>

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO
	#if CRY_PLATFORM_WINDOWS
		#define CryLoadLibrary(libName) ::LoadLibraryA(libName)
	#elif CRY_PLATFORM_DURANGO
		#define CryLoadLibrary(libName) ::LoadLibraryExA(libName, 0, 0)
	#endif
	#define CryGetCurrentModule() ::GetModuleHandle(nullptr)
	#define CrySharedLibrarySupported true
	#define CrySharedLibraryPrefix    ""
	#define CrySharedLibraryExtension ".dll"
	#define CryGetProcAddress(libHandle, procName) ::GetProcAddress((HMODULE)(libHandle), procName)
	#define CryFreeLibrary(libHandle)              ::FreeLibrary((HMODULE)(libHandle))
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	#include <dlfcn.h>
	#include <stdlib.h>
	#include "platform.h"

// for compatibility with code written for windows
	#define CrySharedLibrarySupported   true
	#define CrySharedLibraryPrefix      "lib"
	#if CRY_PLATFORM_APPLE
		#define CrySharedLibraryExtension ".dylib"
	#else
		#define CrySharedLibraryExtension ".so"
	#endif

	#define CryGetProcAddress(libHandle, procName) ::dlsym(libHandle, procName)
	#define CryFreeLibrary(libHandle)              ::dlclose(libHandle)
	#define CryGetCurrentModule()                  ::dlopen(NULL, RTLD_LAZY)
	#define HMODULE void*
static const char* gEnvName("MODULE_PATH");

static const char* GetModulePath()
{
	return getenv(gEnvName);
}

static void SetModulePath(const char* pModulePath)
{
	setenv(gEnvName, pModulePath ? pModulePath : "", true);
}

static HMODULE CryLoadLibrary(const char* libName, bool bLazy = false, bool bInModulePath = true)
{
	char finalPath[_MAX_PATH] = {};
	CRY_ASSERT(strlen(libName) > CRY_ARRAY_COUNT(CrySharedLibraryPrefix));
	CRY_ASSERT(strlen(libName) > CRY_ARRAY_COUNT(CrySharedLibraryExtension));
	
#if CRY_PLATFORM_ANDROID
	const char* libPath = bInModulePath ? (CryGetSharedLibraryStoragePath() ? CryGetSharedLibraryStoragePath() : ".") : "";
#else
	const char* libPath = bInModulePath ? (GetModulePath() ? GetModulePath() : ".") : "";
#endif	

	const char* filePre = strncmp(libName, CrySharedLibraryPrefix, CRY_ARRAY_COUNT(CrySharedLibraryPrefix) - 1) != 0 ? CrySharedLibraryPrefix : "";
	const char* fileExt = strcmp(libName + strlen(libName) - (CRY_ARRAY_COUNT(CrySharedLibraryExtension) - 1), CrySharedLibraryExtension) != 0 ? CrySharedLibraryExtension : "";

	cry_sprintf(finalPath, "%s%s%s%s%s", libPath, libPath ? "/" : "", filePre, libName, fileExt);

	#if CRY_PLATFORM_LINUX
	return ::dlopen(finalPath, (bLazy ? RTLD_LAZY : RTLD_NOW) | RTLD_DEEPBIND);
	#else
	return ::dlopen(finalPath, bLazy ? RTLD_LAZY : RTLD_NOW);
	#endif
}
#else
	#define CrySharedLibrarySupported false
	#define CrySharedLibraryPrefix    ""
	#define CrySharedLibraryExtension ""
	#define CryLoadLibrary(libName)                NULL
	#define CryGetProcAddress(libHandle, procName) NULL
	#define CryFreeLibrary(libHandle)
	#define GetModuleHandle(x)                     0
	#define CryGetCurrentModule()                  NULL

#endif

#define CryLibraryDefName(libName)               CrySharedLibraryPrefix libName CrySharedLibraryExtension
#define CryLoadLibraryDefName(libName)           CryLoadLibrary(CryLibraryDefName(libName))

// RAII helper to load a dynamic library and free it at the end of the scope.
class CCryLibrary
{
public:
	CCryLibrary(const char* szLibraryPath)
		: m_hModule(nullptr)
	{
		if (szLibraryPath != nullptr)
		{
			m_hModule = CryLoadLibrary(szLibraryPath);
		}
	}

	CCryLibrary(const CCryLibrary& other) = delete;

	CCryLibrary(CCryLibrary&& other)
		: m_hModule(std::move(other.m_hModule))
	{
		other.m_hModule = nullptr;
	}

	~CCryLibrary()
	{
		Free();
	}

	void Free()
	{
		if (m_hModule != nullptr)
		{
			CryFreeLibrary(m_hModule);
			m_hModule = nullptr;
		}
	}

	void ReleaseOwnership() { m_hModule = nullptr; }
	bool IsLoaded() const { return m_hModule != nullptr; }

	template<typename TProcedure>
	TProcedure GetProcedureAddress(const char* szName)
	{
		return (TProcedure)CryGetProcAddress(m_hModule, szName);
	}

	void Set(const char* szLibraryPath)
	{
		if (m_hModule != nullptr)
		{
			CryFreeLibrary(m_hModule);
			m_hModule = nullptr;
		}

		if (szLibraryPath != nullptr)
		{
			m_hModule = CryLoadLibrary(szLibraryPath);
		}
	}

	HMODULE m_hModule;
};

#endif //CRYLIBRARY_H__
