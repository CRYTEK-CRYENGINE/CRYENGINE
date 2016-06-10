// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "resource.h"

// Insert your headers here
#include <CryCore/Platform/CryWindows.h>
#include <ShellAPI.h> // requires <windows.h>

// We need shell api for Current Root Extraction.
#include "shlwapi.h"
#pragma comment(lib, "shlwapi.lib")

#include <CryCore/Platform/CryLibrary.h>
#include <CryGame/IGameStartup.h>
#include <CrySystem/IConsole.h>
#include <CrySystem/File/ICryPak.h>

#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/Profilers/FrameProfiler/FrameProfiler_impl.h>
#include <CryString/StringUtils.h>

#include <CrySystem/ParseEngineConfig.h>

#define DLL_INITFUNC_SYSTEM "CreateSystemInterface"


// Advise notebook graphics drivers to prefer discrete GPU when no explicit application profile exists
extern "C"
{
	// nVidia
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	// AMD
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}


#ifdef _LIB
extern "C" IGameStartup* CreateGameStartup();
#endif //_LIB

#ifdef _LIB
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

#define CRY_LIVE_CREATE_MUTEX_ENABLE 0

#if CRY_LIVE_CREATE_MUTEX_ENABLE
class CCryLiveCreateMutex
{
private:	
	HANDLE m_hMutex;

public:
	CCryLiveCreateMutex(bool bCreateMutex)
		: m_hMutex(NULL)
	{
		if (bCreateMutex)
		{
			// create PC only mutex that signals that the game is running
			// this is used by LiveCreate to make sure only one instance of game is running
			const char* szMutexName = "Global\\CryLiveCreatePCMutex";
			m_hMutex = ::CreateMutexA(NULL, FALSE, szMutexName);
			if (NULL == m_hMutex)
			{
				// mutex may already exist, then just open it
				m_hMutex = ::OpenMutexA(MUTEX_ALL_ACCESS, FALSE, szMutexName);
			}
		}
	}

	~CCryLiveCreateMutex()
	{
		if (NULL != m_hMutex)
		{
			::CloseHandle(m_hMutex);
			m_hMutex = NULL;
		}
	}
};
#endif

static const LPWSTR GetProjectRootArgument(int argc, const LPWSTR argv[])
{
	for (int i = 1; i < argc - 1; i++)
	{
		if (wcscmp(argv[i], L"-projectroot") == 0)
			return argv[i + 1];
	}

	return nullptr;
}

static const LPWSTR GetProjectDllDirArgument(int argc, const LPWSTR argv[])
{
	for (int i = 1; i < argc - 1; i++)
	{
		if (wcscmp(argv[i], L"-projectdlldir") == 0)
			return argv[i + 1];
	}

	return nullptr;
}

int RunGame(const char *commandLine)
{
	SSystemInitParams startupParams;
	string gameDLLName;

	// set game project related system changes
	{
		int argc = 0;
		LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
		const LPWSTR sProjectRoot = GetProjectRootArgument(argc, argv);
		if (sProjectRoot)
		{
			SetCurrentDirectoryW(sProjectRoot);

			const LPWSTR sProjectDllDir = GetProjectDllDirArgument(argc, argv);
			if (sProjectDllDir && sProjectDllDir[0])
			{
				SetDllDirectoryW(sProjectDllDir);
				WideCharToMultiByte(CP_ACP, 0, sProjectDllDir, -1, startupParams.szProjectDllDir, CRY_ARRAY_COUNT(startupParams.szProjectDllDir), NULL, NULL);
			}

			CProjectConfig projectCfg;
			gameDLLName = projectCfg.m_gameDLL;
		}
		else
		{
			// default to old game folder behavior
			CryFindRootFolderAndSetAsCurrentWorkingDirectory();
			CEngineConfig engineCfg;
			gameDLLName = engineCfg.m_gameDLL;
		}
		LocalFree(argv);
	}
	
	//restart parameters
	static const size_t MAX_RESTART_LEVEL_NAME = 256;
	char fileName[MAX_RESTART_LEVEL_NAME];
	cry_strcpy(fileName, "");
	static const char logFileName[] = "Game.log";

#if CRY_LIVE_CREATE_MUTEX_ENABLE
	const bool bCreateMutex = (NULL == strstr(commandLine, "-nomutex"));
	CCryLiveCreateMutex liveCreateMutex(bCreateMutex);
#endif

	HMODULE gameDll = 0;

#if !defined(_LIB)
	// load the game dll
	gameDll = CryLoadLibrary( gameDLLName );

	if (!gameDll)
	{
		string errorStr;
		errorStr.Format("Failed to load the Game DLL! %s", gameDLLName.c_str());

		MessageBox(0, errorStr.c_str(), "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);
		// failed to load the dll

		return 0;
	}

	// get address of startup function
	IGameStartup::TEntryFunction CreateGameStartup = (IGameStartup::TEntryFunction)CryGetProcAddress(gameDll, "CreateGameStartup");

	if (!CreateGameStartup)
	{
		// dll is not a compatible game dll
		CryFreeLibrary(gameDll);

		MessageBox(0, "Specified Game DLL is not valid! Please make sure you are running the correct executable", "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);
		
		return 0;
	}
#endif //!defined(_LIB)

	//SSystemInitParams startupParams;

	startupParams.hInstance = GetModuleHandle(0);
	startupParams.sLogFileName = logFileName;
	cry_strcpy(startupParams.szSystemCmdLine, commandLine);
	//startupParams.pProtectedFunctions[0] = &TestProtectedFunction;

	// create the startup interface
	IGameStartup *pGameStartup = CreateGameStartup();

	if (!pGameStartup)
	{
		// failed to create the startup interface
		CryFreeLibrary(gameDll);

		const char* noPromptArg = strstr(commandLine, "-noprompt");
		if (noPromptArg == NULL)
			MessageBox(0, "Failed to create the GameStartup Interface!", "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);

		return 0;
	}

	bool oaRun = false;

	if(strstr(commandLine, "-norandom"))
		startupParams.bNoRandom = 1;

	// run the game
	startupParams.pGameStartup = pGameStartup;
	if (pGameStartup->Init(startupParams))
	{
#if !defined(SYS_ENV_AS_STRUCT)
		gEnv = startupParams.pSystem->GetGlobalEnvironment();
#endif
		//startupParams.pProtectedFunctions[0](0,0);

		// Check for signature file.
		if (!gEnv->pCryPak->IsFileExist("config/config.dat"))
			return -1;

		char * pRestartLevelName = NULL;
		if (fileName[0])
			pRestartLevelName = fileName;

		CryStackStringT<char, 256> newCommandLine;
 		const char* substr = oaRun ? 0 : strstr(commandLine, "restart");
 		if(substr != NULL)
 		{
 			int len = (int)(substr - commandLine);
			newCommandLine.assign(commandLine, len);
			newCommandLine.append(commandLine + len + 7);
 			pGameStartup->Run(newCommandLine.c_str());	//restartLevel to be loaded
 		}
 		else
 		{
 			const char* loadstr = oaRun ? 0 : strstr(commandLine, "load");
 			if(loadstr != NULL)
 			{
 				int len = (int)(loadstr - commandLine);
				newCommandLine.assign(commandLine, len);
				newCommandLine.append(commandLine + len + 4);
 				pGameStartup->Run(newCommandLine.c_str()); //restartLevel to be loaded
 			}
 			else
 				pGameStartup->Run(NULL);
		}

		bool isLevelRequested = pGameStartup->GetRestartLevel(&pRestartLevelName);
		if (pRestartLevelName)
		{
			if (strlen(pRestartLevelName) < MAX_RESTART_LEVEL_NAME)
				cry_strcpy(fileName, pRestartLevelName);
		}

		char pRestartMod[255];
		bool isModRequested = pGameStartup->GetRestartMod(pRestartMod, sizeof(pRestartMod));

		if (isLevelRequested || isModRequested)
		{
			STARTUPINFO si;
			ZeroMemory( &si, sizeof(si) );
			si.cb = sizeof(si);
			PROCESS_INFORMATION pi;

			if (isLevelRequested)
			{
				newCommandLine.assign("restart ");
				newCommandLine.append(fileName);
			}

			if (isModRequested)
			{
				newCommandLine.append("-modrestart");
				if (pRestartMod[0] != '\0')
				{
					newCommandLine.append(" -mod ");
					newCommandLine.append(pRestartMod);
				}
			}

			wchar_t szExecutablePath[512];
			GetModuleFileNameW(GetModuleHandle(NULL), szExecutablePath, CRY_ARRAY_COUNT(szExecutablePath));

			if (!CreateProcess(Unicode::Convert<string>(szExecutablePath).c_str(), (char*)newCommandLine.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
			{
				char pMessage[256];
				cry_sprintf(pMessage, "Failed to restart the game: %S", szExecutablePath);
				MessageBox(0, pMessage, "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY | MB_ICONERROR);
			}
		}
		else
		{
			// check if there is a patch to install. If there is, do it now.
			const char* pfilename = pGameStartup->GetPatch();
			if(pfilename)
			{
				STARTUPINFO si;
				ZeroMemory( &si, sizeof(si) );
				si.cb = sizeof(si);
				PROCESS_INFORMATION pi;
				CreateProcess(pfilename, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
			}
		}

		pGameStartup->Shutdown();
		pGameStartup = 0;

		CryFreeLibrary(gameDll);
	}
	else
	{
#ifndef _RELEASE
		const char* noPromptArg = strstr(commandLine, "-noprompt");
		if (noPromptArg == NULL)
			MessageBox(0, "Failed to initialize the GameStartup Interface!", "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);
#endif
		// if initialization failed, we still need to call shutdown
		pGameStartup->Shutdown();
		pGameStartup = 0;

		CryFreeLibrary(gameDll);

		return 0;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
// Support relaunching for windows media center edition.
//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_WINDOWS
bool ReLaunchMediaCenter()
{
	// Skip if not running on a Media Center
	if( GetSystemMetrics( SM_MEDIACENTER ) == 0 ) 
		return false;

	// Get the path to Media Center
	char szExpandedPath[MAX_PATH];
	if( !ExpandEnvironmentStrings( "%SystemRoot%\\ehome\\ehshell.exe", szExpandedPath, MAX_PATH) )
		return false;

	// Skip if ehshell.exe doesn't exist
	if( GetFileAttributes( szExpandedPath ) == 0xFFFFFFFF )
		return false;

	// Launch ehshell.exe 
	INT_PTR result = (INT_PTR)ShellExecute( NULL, TEXT("open"), szExpandedPath, NULL, NULL, SW_SHOWNORMAL);
	return (result > 32);
}
#endif // CRY_PLATFORM_WINDOWS
///////////////////////////////////////////////
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// we need pass the full command line, including the filename
	// lpCmdLine does not contain the filename.
#if CAPTURE_REPLAY_LOG
#ifndef _LIB
	CryLoadLibrary("CrySystem.dll");
#endif
	CryGetIMemReplay()->StartOnCommandLine(lpCmdLine);
#endif

	//check for a restart
	const char* pos = strstr(lpCmdLine, "restart");
	if(pos != NULL)
	{
		Sleep(5000); //wait for old instance to be deleted
		return RunGame(lpCmdLine);	//pass the restart level if restarting
	}
	else
		pos = strstr(lpCmdLine, " -load ");// commandLine.find("load");

	if(pos != NULL)
		RunGame(lpCmdLine);

	int nRes = RunGame(GetCommandLineA());

	//////////////////////////////////////////////////////////////////////////
	// Support relaunching for windows media center edition.
	//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_WINDOWS
	if (strstr(lpCmdLine,"ReLaunchMediaCenter") != 0)
	{
		ReLaunchMediaCenter();
	}
#endif
	//////////////////////////////////////////////////////////////////////////

	return nRes;
}
