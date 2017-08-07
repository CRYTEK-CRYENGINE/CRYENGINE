// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryString/StringUtils.h>
#include <CryCore/Platform/platform.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/CryUnitTest.h>
#include <CryExtension/RegFactoryNode.h>
#include <CryExtension/ICryFactoryRegistryImpl.h>
#include <CryString/UnicodeFunctions.h>
#include <CrySystem/CryUtils.h>
#include <CryCore/Platform/CryWindows.h>

#include <CryFlowGraph/IFlowBaseNode.h>

//////////////////////////////////////////////////////////////////////////
// Global environment variable.
//////////////////////////////////////////////////////////////////////////
#if defined(SYS_ENV_AS_STRUCT)
	#if defined(_LAUNCHER)
SSystemGlobalEnvironment gEnv;
	#else
extern SSystemGlobalEnvironment gEnv;
	#endif
#else
struct SSystemGlobalEnvironment* gEnv = nullptr;
#endif

#if (defined(_LAUNCHER) && defined(CRY_IS_MONOLITHIC_BUILD)) || !defined(_LIB)
//The reg factory is used for registering the different modules along the whole project
struct SRegFactoryNode* g_pHeadToRegFactories = nullptr;
std::vector<const char*> g_moduleCommands;
std::vector<const char*> g_moduleCVars;

extern "C" DLL_EXPORT void CleanupModuleCVars()
{
	if (auto pConsole = gEnv->pConsole)
	{
		// Unregister all commands that were registered from within the plugin/module
		for (auto& it : g_moduleCommands)
		{
			pConsole->RemoveCommand(it);
		}
		g_moduleCommands.clear();

		// Unregister all CVars that were registered from within the plugin/module
		for (auto& it : g_moduleCVars)
		{
			pConsole->UnregisterVariable(it);
		}
		g_moduleCVars.clear();
	}
}
#endif

#if !defined(CRY_IS_MONOLITHIC_BUILD)  || defined(_LAUNCHER)
extern "C" DLL_EXPORT SRegFactoryNode* GetHeadToRegFactories()
{
	return g_pHeadToRegFactories;
}
#endif

#if !defined(_LIB) || defined(_LAUNCHER)
//////////////////////////////////////////////////////////////////////////
// If not in static library.

	#include <CryThreading/CryThreadImpl.h>
	#include <CryMath/ISplineSerialization_impl.h>

	#include <CryCore/TypeInfo_impl.h>

	#define CRY_PLATFORM_IMPL_H_FILE 1
	#include <CryCore/CryTypeInfo.inl>
	#if CRY_PLATFORM_POSIX
		#include "WinBase.inl"
	#endif
	#undef CRY_PLATFORM_IMPL_H_FILE
	#if CRY_PLATFORM_WINDOWS
void CryPureCallHandler()
{
	CryFatalError("Pure function call");
}

void CryInvalidParameterHandler(
  const wchar_t* expression,
  const wchar_t* function,
  const wchar_t* file,
  unsigned int line,
  uintptr_t pReserved)
{
	//size_t i;
	//char sFunc[128];
	//char sExpression[128];
	//char sFile[128];
	//wcstombs_s( &i,sFunc,sizeof(sFunc),function,_TRUNCATE );
	//wcstombs_s( &i,sExpression,sizeof(sExpression),expression,_TRUNCATE );
	//wcstombs_s( &i,sFile,sizeof(sFile),file,_TRUNCATE );
	//CryFatalError( "Invalid parameter detected in function %s. File: %s Line: %d, Expression: %s",sFunc,sFile,line,sExpression );
	CryFatalError("Invalid parameter detected in CRT function");
}

void InitCRTHandlers()
{
	_set_purecall_handler(CryPureCallHandler);
	_set_invalid_parameter_handler(CryInvalidParameterHandler);
}
	#else
void InitCRTHandlers() {}
	#endif

//////////////////////////////////////////////////////////////////////////
// This is an entry to DLL initialization function that must be called for each loaded module
//////////////////////////////////////////////////////////////////////////
extern "C" DLL_EXPORT void ModuleInitISystem(ISystem* pSystem, const char* moduleName)
{
	#if defined(USE_CRY_ASSERT)
	CryAssertSetGlobalFlagAddress(pSystem->GetAssertFlagAddress());
	#endif

	if (gEnv) // Already registered.
		return;

	InitCRTHandlers();

	#if !defined(SYS_ENV_AS_STRUCT)
	if (pSystem) // DONT REMOVE THIS. ITS FOR RESOURCE COMPILER!!!!
		gEnv = pSystem->GetGlobalEnvironment();
	#endif
	#if !defined(_LIB) && !(CRY_PLATFORM_DURANGO && defined(_LIB))
	if (pSystem)
	{
		ICryFactoryRegistryImpl* pCryFactoryImpl = static_cast<ICryFactoryRegistryImpl*>(pSystem->GetCryFactoryRegistry());
		pCryFactoryImpl->RegisterFactories(g_pHeadToRegFactories);
	}
	#endif
	#ifdef CRY_UNIT_TESTING
	// Register All unit tests of this module.
	if (pSystem)
	{
		if (CryUnitTest::IUnitTestManager* pTestManager = pSystem->GetITestSystem()->GetIUnitTestManager())
			pTestManager->CreateTests(moduleName);
	}
	#endif //CRY_UNIT_TESTING
}

int g_iTraceAllocations = 0;

//////////////////////////////////////////////////////////////////////////

// If we use cry memory manager this should be also included in every module.
	#if defined(USING_CRY_MEMORY_MANAGER)
		#include <CryMemory/CryMemoryManager_impl.h>
	#endif

	#include <CryCore/Assert/CryAssert_impl.h>

//////////////////////////////////////////////////////////////////////////
void CrySleep(unsigned int dwMilliseconds)
{
	Sleep(dwMilliseconds);
}

//////////////////////////////////////////////////////////////////////////
void CryLowLatencySleep(unsigned int dwMilliseconds)
{
	#if CRY_PLATFORM_DURANGO
	if (dwMilliseconds > 32) // just do an OS sleep for long periods, because we just assume that if we sleep for this long, we don't need a accurate latency (max diff is likly 15ms)
		CrySleep(dwMilliseconds);
	else // do a more accurate "sleep" by yielding this CPU to other threads
	{
		LARGE_INTEGER frequency;
		LARGE_INTEGER currentTime;
		LARGE_INTEGER endTime;

		QueryPerformanceCounter(&endTime);

		// Ticks in microseconds (1/1000 ms)
		QueryPerformanceFrequency(&frequency);
		endTime.QuadPart += (dwMilliseconds * frequency.QuadPart) / (1000ULL);

		do
		{
			SwitchToThread();
			SwitchToThread();
			SwitchToThread();
			SwitchToThread();
			SwitchToThread();
			SwitchToThread();
			SwitchToThread();
			SwitchToThread();

			QueryPerformanceCounter(&currentTime);
		}
		while (currentTime.QuadPart < endTime.QuadPart);
	}
	#else
	CrySleep(dwMilliseconds);
	#endif
}

//////////////////////////////////////////////////////////////////////////
threadID CryGetCurrentThreadId()
{
	return GetCurrentThreadId();
}

//////////////////////////////////////////////////////////////////////////
void CryDebugBreak()
{
	#if CRY_PLATFORM_WINDOWS && !defined(RELEASE)
	if (IsDebuggerPresent())
	#endif
	{
		__debugbreak();
	}
}

	#if CRY_PLATFORM_WINAPI

//////////////////////////////////////////////////////////////////////////
EQuestionResult CryMessageBox(const char* lpText, const char* lpCaption, EMessageBox uType)
{
#if CRY_PLATFORM_WINDOWS
	#if !defined(RESOURCE_COMPILER)
	ICVar* const pCVar = gEnv->pConsole ? gEnv->pConsole->GetCVar("sys_no_crash_dialog") : NULL;
	if ((pCVar && pCVar->GetIVal() != 0) || gEnv->bNoAssertDialog)
	{
		return eQR_None;
	}
	#endif

	if (gEnv && gEnv->pSystem)
	{
		return gEnv->pSystem->ShowMessage(lpText, lpCaption, uType);
	}
#endif
	return eQR_None;
}

//////////////////////////////////////////////////////////////////////////
bool CryCreateDirectory(const char* lpPathName)
{
	// Convert from UTF-8 to UNICODE
	wstring widePath;
	Unicode::Convert(widePath, lpPathName);

	const DWORD dwAttr = ::GetFileAttributesW(widePath.c_str());
	if (dwAttr != INVALID_FILE_ATTRIBUTES && (dwAttr & FILE_ATTRIBUTE_DIRECTORY) != 0)
	{
		return true;
	}

	return ::CreateDirectoryW(widePath.c_str(), 0) != 0;
}

//////////////////////////////////////////////////////////////////////////
void CryGetCurrentDirectory(unsigned int nBufferLength, char* lpBuffer)
{
	if (nBufferLength <= 0 || !lpBuffer)
	{
		return;
	}

	*lpBuffer = 0;

	// Get directory in UTF-16
	std::vector<wchar_t> buffer;
	{
		const size_t requiredLength = ::GetCurrentDirectoryW(0, 0);

		if (requiredLength <= 0)
		{
			return;
		}

		buffer.resize(requiredLength, 0);

		if (::GetCurrentDirectoryW(requiredLength, &buffer[0]) != requiredLength - 1)
		{
			return;
		}
	}

	// Convert to UTF-8
	if (Unicode::Convert<Unicode::eEncoding_UTF16, Unicode::eEncoding_UTF8>(lpBuffer, nBufferLength, &buffer[0]) > nBufferLength)
	{
		*lpBuffer = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CrySetCurrentWorkingDirectory(const char* szWorkingDirectory)
{
	SetCurrentDirectoryW(Unicode::Convert<wstring>(szWorkingDirectory).c_str());
}

//////////////////////////////////////////////////////////////////////////
		#include <CryString/CryPath.h>
void CryGetExecutableFolder(unsigned int pathSize, char* szPath)
{
	WCHAR filePath[512];
	size_t nLen = GetModuleFileNameW(CryGetCurrentModule(), filePath, CRY_ARRAY_COUNT(filePath));

	if (nLen >= CRY_ARRAY_COUNT(filePath))
	{
		CryFatalError("The path to the current executable exceeds the expected length. TruncatedPath:%s", filePath);
	}

	if (nLen <= 0)
	{
		CryFatalError("Unexpected error encountered trying to get executable path. GetModuleFileNameW failed.");
	}

	if (wchar_t* pathEnd = wcsrchr(filePath, L'\\'))
	{
		pathEnd[1] = L'\0';
	}

	size_t requiredLength = Unicode::Convert(szPath, pathSize, filePath);
	if (requiredLength > pathSize)
	{
		CryFatalError("Executable path is to long. MaxPathSize:%u, PathSize:%u, Path:%s", pathSize, (uint)requiredLength, filePath);
	}
}

//////////////////////////////////////////////////////////////////////////
int CryGetWritableDirectory(unsigned int nBufferLength, char* lpBuffer)
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////
short CryGetAsyncKeyState(int vKey)
{
		#if CRY_PLATFORM_WINDOWS
	return GetAsyncKeyState(vKey);
		#else
	return 0;
		#endif
}

//////////////////////////////////////////////////////////////////////////
void* CryCreateCriticalSection()
{
	CRITICAL_SECTION* pCS = new CRITICAL_SECTION;
	InitializeCriticalSection(pCS);
	return pCS;
}

void CryCreateCriticalSectionInplace(void* pCS)
{
	InitializeCriticalSection((CRITICAL_SECTION*)pCS);
}
//////////////////////////////////////////////////////////////////////////
void CryDeleteCriticalSection(void* cs)
{
	CRITICAL_SECTION* pCS = (CRITICAL_SECTION*)cs;
	if (pCS->LockCount >= 0)
		CryFatalError("Critical Section hanging lock");
	DeleteCriticalSection(pCS);
	delete pCS;
}

//////////////////////////////////////////////////////////////////////////
void CryDeleteCriticalSectionInplace(void* cs)
{
	CRITICAL_SECTION* pCS = (CRITICAL_SECTION*)cs;
	if (pCS->LockCount >= 0)
		CryFatalError("Critical Section hanging lock");
	DeleteCriticalSection(pCS);
}

//////////////////////////////////////////////////////////////////////////
void CryEnterCriticalSection(void* cs)
{
	EnterCriticalSection((CRITICAL_SECTION*)cs);
}

//////////////////////////////////////////////////////////////////////////
bool CryTryCriticalSection(void* cs)
{
	return TryEnterCriticalSection((CRITICAL_SECTION*)cs) != 0;
}

//////////////////////////////////////////////////////////////////////////
void CryLeaveCriticalSection(void* cs)
{
	LeaveCriticalSection((CRITICAL_SECTION*)cs);
}

//////////////////////////////////////////////////////////////////////////
uint32 CryGetFileAttributes(const char* lpFileName)
{
	// Normal GetFileAttributes not available anymore in non desktop applications (eg Durango)
	WIN32_FILE_ATTRIBUTE_DATA data;
	BOOL res;
		#if CRY_PLATFORM_DURANGO
	res = GetFileAttributesExA(lpFileName, GetFileExInfoStandard, &data);
		#else
	res = GetFileAttributesEx(lpFileName, GetFileExInfoStandard, &data);
		#endif
	return res ? data.dwFileAttributes : -1;
}

//////////////////////////////////////////////////////////////////////////
bool CrySetFileAttributes(const char* lpFileName, uint32 dwFileAttributes)
{
		#if CRY_PLATFORM_DURANGO
	return SetFileAttributesA(lpFileName, dwFileAttributes) != 0;
		#else
	return SetFileAttributes(lpFileName, dwFileAttributes) != 0;
		#endif
}

	#endif // CRY_PLATFORM_WINAPI

//////////////////////////////////////////////////////////////////////////
void CryFindRootFolderAndSetAsCurrentWorkingDirectory()
{
	char szEngineRootDir[_MAX_PATH] = "";
	CryFindEngineRootFolder(CRY_ARRAY_COUNT(szEngineRootDir), szEngineRootDir);

#if CRY_PLATFORM_WINAPI || CRY_PLATFORM_LINUX
	CrySetCurrentWorkingDirectory(szEngineRootDir);
#endif
}

#if CRY_PLATFORM_WINAPI || CRY_PLATFORM_LINUX
//////////////////////////////////////////////////////////////////////////
void CryFindEngineRootFolder(unsigned int engineRootPathSize, char* szEngineRootPath)
{
		#if CRY_PLATFORM_WINAPI
	char osSeperator = '\\';
		#elif CRY_PLATFORM_POSIX
	char osSeperator = '/';
		#endif
	char szExecFilePath[_MAX_PATH] = "";
	CryGetExecutableFolder(CRY_ARRAY_COUNT(szExecFilePath), szExecFilePath);

	string strTempPath(szExecFilePath);
	size_t nCurDirSlashPos = strTempPath.size() - 1;

	// Try to find directory named "Engine" or "engine"
	do
	{
		bool bFoundMatch = false;
		for (int i = 0; i < 2; ++i)
		{
			strTempPath.erase(nCurDirSlashPos + 1, string::npos);
			strTempPath.append(i == 0 ? "Engine" : "engine");

			// Does directory exist
			struct stat info;
			if (stat(strTempPath.c_str(), &info) == 0 && (info.st_mode & S_IFMT) == S_IFDIR)
			{
				bFoundMatch = true;
				break;
			}
		}
		if (bFoundMatch)
		{
			break;
		}
		// Move up to parent directory
		nCurDirSlashPos = strTempPath.rfind(osSeperator, nCurDirSlashPos - 1);

	}
	while (nCurDirSlashPos != 0 && nCurDirSlashPos != string::npos);

	if (nCurDirSlashPos == 0 || nCurDirSlashPos == string::npos)
	{
		CryFatalError("Unable to locate CryEngine root folder. Ensure that the 'engine' folder exists in your CryEngine root directory");
		return;
	}

	strTempPath.erase(strTempPath.rfind(osSeperator) + 1, string::npos);
	if (!cry_strcpy(szEngineRootPath, engineRootPathSize, strTempPath.c_str()))
	{
		CryFatalError("CryEngine root path is to long. MaxPathSize:%u, PathSize:%u, Path:%s", engineRootPathSize, (uint)strTempPath.length() + 1, strTempPath.c_str());
	}
}

#elif CRY_PLATFORM_ORBIS
void CryFindEngineRootFolder(unsigned int engineRootPathSize, char* szEngineRootPath)
{
	cry_strcpy(szEngineRootPath, engineRootPathSize, ".");
}

void CryGetExecutableFolder(unsigned int nBufferLength, char* lpBuffer)
{
	CryFindEngineRootFolder(nBufferLength, lpBuffer);
}

#elif CRY_PLATFORM_ANDROID

void CryFindEngineRootFolder(unsigned int engineRootPathSize, char* szEngineRootPath)
{
	// Hack! Android currently does not support a directory layout, there is an explicit search in main for GameSDK/GameData.pak
	// and the executable folder is not related to the engine or game folder. - 18/03/2016
	cry_strcpy(szEngineRootPath, engineRootPathSize, CryGetProjectStoragePath());
}

#endif 

#if CRY_PLATFORM_DURANGO
HMODULE DurangoLoadLibrary(const char* libName)
{
	HMODULE h = ::LoadLibraryExA(libName, 0, 0);
	return h;
}
	#endif

int64 CryGetTicks()
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return li.QuadPart;
}

#endif //!defined(_LIB) || defined(_LAUNCHER)

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Threads implementation. For static linking it must be declared inline otherwise creating multiple symbols
////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(_LIB) && !defined(_LAUNCHER)
	#define THR_INLINE inline
#else
	#define THR_INLINE
#endif

//////////////////////////////////////////////////////////////////////////
// Support for automatic FlowNode types registration
//////////////////////////////////////////////////////////////////////////
#if !defined(_LIB) || defined(_LAUNCHER)
CAutoRegFlowNodeBase* CAutoRegFlowNodeBase::s_pFirst = nullptr;
CAutoRegFlowNodeBase* CAutoRegFlowNodeBase::s_pLast = nullptr;
bool                  CAutoRegFlowNodeBase::s_bNodesRegistered = false;
#endif

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//inline void CryDebugStr( const char *format,... )
//{
/*
   #ifdef CRYSYSTEM_EXPORTS
   va_list	ArgList;
   char		szBuffer[65535];
   va_start(ArgList, format);
   cry_vsprintf(szBuffer, format, ArgList);
   va_end(ArgList);
   cry_strcat(szBuffer,"\n");
   OutputDebugString(szBuffer);
   #endif
 */
//}

// load implementation of platform profile marker
#if !defined(_LIB) || defined(_LAUNCHER)
	#include <CrySystem/Profilers/FrameProfiler/FrameProfiler_impl.h>
#endif

#if !defined(_LIB) || defined(_LAUNCHER)
CRY_ALIGN(64) uint32 BoxSides[0x40 * 8] = {
	0, 0, 0, 0, 0, 0, 0, 0, //00
	0, 4, 6, 2, 0, 0, 0, 4, //01
	7, 5, 1, 3, 0, 0, 0, 4, //02
	0, 0, 0, 0, 0, 0, 0, 0, //03
	0, 1, 5, 4, 0, 0, 0, 4, //04
	0, 1, 5, 4, 6, 2, 0, 6, //05
	7, 5, 4, 0, 1, 3, 0, 6, //06
	0, 0, 0, 0, 0, 0, 0, 0, //07
	7, 3, 2, 6, 0, 0, 0, 4, //08
	0, 4, 6, 7, 3, 2, 0, 6, //09
	7, 5, 1, 3, 2, 6, 0, 6, //0a
	0, 0, 0, 0, 0, 0, 0, 0, //0b
	0, 0, 0, 0, 0, 0, 0, 0, //0c
	0, 0, 0, 0, 0, 0, 0, 0, //0d
	0, 0, 0, 0, 0, 0, 0, 0, //0e
	0, 0, 0, 0, 0, 0, 0, 0, //0f
	0, 2, 3, 1, 0, 0, 0, 4, //10
	0, 4, 6, 2, 3, 1, 0, 6, //11
	7, 5, 1, 0, 2, 3, 0, 6, //12
	0, 0, 0, 0, 0, 0, 0, 0, //13
	0, 2, 3, 1, 5, 4, 0, 6, //14
	1, 5, 4, 6, 2, 3, 0, 6, //15
	7, 5, 4, 0, 2, 3, 0, 6, //16
	0, 0, 0, 0, 0, 0, 0, 0, //17
	0, 2, 6, 7, 3, 1, 0, 6, //18
	0, 4, 6, 7, 3, 1, 0, 6, //19
	7, 5, 1, 0, 2, 6, 0, 6, //1a
	0, 0, 0, 0, 0, 0, 0, 0, //1b
	0, 0, 0, 0, 0, 0, 0, 0, //1c
	0, 0, 0, 0, 0, 0, 0, 0, //1d
	0, 0, 0, 0, 0, 0, 0, 0, //1e
	0, 0, 0, 0, 0, 0, 0, 0, //1f
	7, 6, 4, 5, 0, 0, 0, 4, //20
	0, 4, 5, 7, 6, 2, 0, 6, //21
	7, 6, 4, 5, 1, 3, 0, 6, //22
	0, 0, 0, 0, 0, 0, 0, 0, //23
	7, 6, 4, 0, 1, 5, 0, 6, //24
	0, 1, 5, 7, 6, 2, 0, 6, //25
	7, 6, 4, 0, 1, 3, 0, 6, //26
	0, 0, 0, 0, 0, 0, 0, 0, //27
	7, 3, 2, 6, 4, 5, 0, 6, //28
	0, 4, 5, 7, 3, 2, 0, 6, //29
	6, 4, 5, 1, 3, 2, 0, 6, //2a
	0, 0, 0, 0, 0, 0, 0, 0, //2b
	0, 0, 0, 0, 0, 0, 0, 0, //2c
	0, 0, 0, 0, 0, 0, 0, 0, //2d
	0, 0, 0, 0, 0, 0, 0, 0, //2e
	0, 0, 0, 0, 0, 0, 0, 0, //2f
	0, 0, 0, 0, 0, 0, 0, 0, //30
	0, 0, 0, 0, 0, 0, 0, 0, //31
	0, 0, 0, 0, 0, 0, 0, 0, //32
	0, 0, 0, 0, 0, 0, 0, 0, //33
	0, 0, 0, 0, 0, 0, 0, 0, //34
	0, 0, 0, 0, 0, 0, 0, 0, //35
	0, 0, 0, 0, 0, 0, 0, 0, //36
	0, 0, 0, 0, 0, 0, 0, 0, //37
	0, 0, 0, 0, 0, 0, 0, 0, //38
	0, 0, 0, 0, 0, 0, 0, 0, //39
	0, 0, 0, 0, 0, 0, 0, 0, //3a
	0, 0, 0, 0, 0, 0, 0, 0, //3b
	0, 0, 0, 0, 0, 0, 0, 0, //3c
	0, 0, 0, 0, 0, 0, 0, 0, //3d
	0, 0, 0, 0, 0, 0, 0, 0, //3e
	0, 0, 0, 0, 0, 0, 0, 0, //3f
};
#endif // !_LIB || _LAUNCHER

#if !defined(_LIB) || defined(_LAUNCHER)
	#include <Cry3DEngine/GeomRef.inl>
#endif