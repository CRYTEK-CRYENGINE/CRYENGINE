// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

#if defined(CRY_IS_MONOLITHIC_BUILD) && defined(_LAUNCHER)
// Include common type defines for static linking
// Manually instantiate templates as needed here.
#include <CryCore/Common_TypeInfo.h>
STRUCT_INFO_T_INSTANTIATE(Vec2_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Vec2_tpl, <int>)
STRUCT_INFO_T_INSTANTIATE(Vec4_tpl, <short>)
STRUCT_INFO_T_INSTANTIATE(Vec3_tpl, <int>)
STRUCT_INFO_T_INSTANTIATE(Ang3_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Quat_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Plane_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Matrix33_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Color_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Color_tpl, <uint8>)
#endif

#if CRY_PLATFORM_WINAPI && defined(CRY_IS_APPLICATION) 
// This belongs to the ClassFactoryManager::the() singleton in ClassFactory.h and must only exist in executables, not in DLLs.
#include <CrySerialization/yasli/ClassFactory.h>
extern "C" DLL_EXPORT yasli::ClassFactoryManager* GetYasliClassFactoryManager()
{
#if defined(NOT_USE_CRY_MEMORY_MANAGER)
	// Cannot be used by code that uses CryMemoryManager as it might not be initialized yet.
	static yasli::ClassFactoryManager* g_classFactoryManager = nullptr;
	if (g_classFactoryManager == nullptr)
	{
		g_classFactoryManager = new yasli::ClassFactoryManager();
	}
	return g_classFactoryManager;
#else
	// Cannot be used in Sandbox due as we would create a static while creating a static. MSVC doesn't like that.
	static yasli::ClassFactoryManager classFactoryManager;
	return &classFactoryManager;
#endif
}
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
bool CryInitializeEngine(SSystemInitParams& startupParams, bool bManualEngineLoop)
{
	CryFindRootFolderAndSetAsCurrentWorkingDirectory();

#if !defined(CRY_IS_MONOLITHIC_BUILD)
	CCryLibrary systemLibrary(CryLibraryDefName("CrySystem"));
	if (!systemLibrary.IsLoaded())
	{
		string errorStr = string().Format("Failed to load the " CryLibraryDefName("CrySystem") " library!");
		CryMessageBox(errorStr.c_str(), "Engine initialization failed!");
		return false;
	}

	PFNCREATESYSTEMINTERFACE CreateSystemInterface = systemLibrary.GetProcedureAddress<PFNCREATESYSTEMINTERFACE>("CreateSystemInterface");
	if (CreateSystemInterface == nullptr)
	{
		string errorStr = string().Format(CryLibraryDefName("CrySystem") " library was invalid, entry-point not found!");
		CryMessageBox(errorStr.c_str(), "Engine initialization failed!");

		return false;
	}
#endif

	if (ISystem* pSystem = CreateSystemInterface(startupParams, bManualEngineLoop))
	{
#if !defined(CRY_IS_MONOLITHIC_BUILD)
		if (bManualEngineLoop)
		{
			// Forward ownership to the function caller
			// This is done since the engine loop will be updated outside of this function scope
			// In other cases we would be exiting the engine at this point.
			systemLibrary.ReleaseOwnership();
		}
#endif
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CrySleep(unsigned int dwMilliseconds)
{
	::Sleep(dwMilliseconds);
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

			if(CryDirectoryExists(strTempPath.c_str()))
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

#endif 

int64 CryGetTicks()
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return li.QuadPart;
}

#if CRY_PLATFORM_WINAPI
#include "platform_impl_winapi.inl"
#elif CRY_PLATFORM_ANDROID
#include "platform_impl_android.inl"
#elif CRY_PLATFORM_ORBIS
#include "platform_impl_orbis.inl"
#elif CRY_PLATFORM_MAC
#include "platform_impl_mac.inl"
#endif

// Functions that depend on the platform-specific includes below

EQuestionResult CryMessageBox(const char* szText, const char* szCaption, EMessageBox type)
{
	if (gEnv && gEnv->bUnattendedMode)
	{
		return eQR_None;
	}

	if (gEnv && gEnv->pSystem && gEnv->pSystem->GetUserCallback() != nullptr)
	{
		return gEnv->pSystem->GetUserCallback()->ShowMessage(szText, szCaption, type);
	}

#if !defined(CRY_PLATFORM_MOBILE) && !defined(CRY_PLATFORM_LINUX)
	// Invoke platform-specific implementation
	EQuestionResult result = CryMessageBoxImpl(szText, szCaption, type);
#else
	EQuestionResult result = eQR_None;
#endif

	if (gEnv && gEnv->pSystem && gEnv->pLog)
	{
		CryLogAlways("Messagebox: cap: %s  text:%s\n", szCaption != nullptr ? szCaption : " ", szText != nullptr ? szText : " ");
	}
	else
	{
		printf("Messagebox: cap: %s  text:%s\n", szCaption != nullptr ? szCaption : " ", szText != nullptr ? szText : " ");
	}

	return result;
}

EQuestionResult CryMessageBox(const wchar_t* szText, const wchar_t* szCaption, EMessageBox type)
{
#if CRY_PLATFORM_WINAPI
	if (gEnv && gEnv->bUnattendedMode)
	{
		return eQR_None;
	}

	// Invoke platform-specific implementation
	return CryMessageBoxImpl(szText, szCaption, type);
#else
	return eQR_None;
#endif
}

//////////////////////////////////////////////////////////////////////////
// Support for automatic FlowNode types registration
//////////////////////////////////////////////////////////////////////////
CAutoRegFlowNodeBase* CAutoRegFlowNodeBase::s_pFirst = nullptr;
CAutoRegFlowNodeBase* CAutoRegFlowNodeBase::s_pLast = nullptr;
bool                  CAutoRegFlowNodeBase::s_bNodesRegistered = false;

// load implementation of platform profile marker
#include <CrySystem/Profilers/FrameProfiler/FrameProfiler_impl.h>

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

#include <Cry3DEngine/GeomRef.inl>
#include <CryMath/GeomQuery.inl>

#endif //!defined(_LIB) || defined(_LAUNCHER)

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Threads implementation. For static linking it must be declared inline otherwise creating multiple symbols
////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(_LIB) && !defined(_LAUNCHER)
	#define THR_INLINE inline
#else
	#define THR_INLINE
#endif