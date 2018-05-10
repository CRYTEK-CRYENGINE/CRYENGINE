// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryCore/Platform/MacSpecific.h>
#include <CryCore/Assert/CryAssert.h>
#include <CryCore/Platform/platform_impl.inl>


#include <CryCore/Platform/CryLibrary.h>
#include <CrySystem/IConsole.h>

#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <limits.h>
#include <sys/resource.h>
#include <mach-o/dyld.h>    // For _NSGetExecutablePath

#include "AppKitWrapper.h"

#include <execinfo.h>
#include <signal.h>
#include <dlfcn.h>
#include <libgen.h>
#include <inttypes.h>
#include <libunwind.h>

#if !defined(DEDICATED_SERVER)
#include <SDL.h>
#endif

#ifdef _LIB
#endif

#define RunGame_EXIT(exitCode) (exit(exitCode))

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

size_t fopenwrapper_basedir_maxsize = MAXPATHLEN;
namespace { char fopenwrapper_basedir_buffer[MAXPATHLEN] = ""; }
char * fopenwrapper_basedir = fopenwrapper_basedir_buffer;
bool fopenwrapper_trace_fopen = false;

bool IncreaseResourceMaxLimit(int iResource, rlim_t uMax)
{
    struct rlimit kLimit;
    if (getrlimit(iResource, &kLimit) != 0)
    {
        fprintf(stderr, "error: getrlimit (%d) failed\n", iResource);
        return false;
    }
    
    if (uMax != kLimit.rlim_max)
    {
        if (uMax == RLIM_INFINITY || uMax > kLimit.rlim_max)
        {
            kLimit.rlim_max = uMax;
            if (setrlimit(iResource, &kLimit) != 0)
            {
                fprintf(stderr, "error: setrlimit (%d, %" PRIu64 ") failed\n", iResource, uMax);
                return false;
            }
        }
    }
           
    return true;
}


bool IncreaseSackSizeToMax()
{
    // On OS X we can't increase the max limit to infinity without running in
    // sudo. The next best thing we can do is to increase the current limit
    // to the allowed maximum.
    struct rlimit kLimit;
    if (getrlimit(RLIMIT_STACK, &kLimit) != 0)
    {
        fprintf(stderr, "error: getrlimit (%d) failed\n", RLIMIT_STACK);
        return false;
    }
    
    if (kLimit.rlim_cur < kLimit.rlim_max)
    {

        kLimit.rlim_cur = kLimit.rlim_max;
        if (setrlimit(RLIMIT_STACK, &kLimit) != 0)
        {
            fprintf(stderr, "error: setrlimit (%d, %" PRIu64 ") failed\n", RLIMIT_STACK, kLimit.rlim_max);
            return false;
        }
    }
    
    return true;
}

bool GetDefaultThreadStackSize(size_t* pStackSize)
{
	pthread_attr_t kDefAttr;
    pthread_attr_init(&kDefAttr);   // Required on Mac OS or pthread_attr_getstacksize will fail
	int iRes(pthread_attr_getstacksize(&kDefAttr, pStackSize));
    if (iRes != 0)
    {
        fprintf(stderr, "error: pthread_attr_getstacksize returned %d\n", iRes);
        return false;
    }
    return true;
}

static void InitRootDir(CryPathString& strAppDir)
{
#if !defined(APPLE_BUNDLE)
#if defined(DEDICATED_SERVER)
    static const char* s_szBinDir = "BinMac_Dedicated";
#else
    static const char* s_szBinDir = "BinMac";
#endif

    const char* szInnermostDirName = PathUtil::GetFile(strAppDir.c_str());
    
    if (strcmp(s_szBinDir, szInnermostDirName) == 0)
    {
        strAppDir = PathUtil::GetParentDirectoryStackString(strAppDir);
        int ret = chdir(strAppDir.c_str());
        printf("chdir(%s) : %s\n", strAppDir.c_str(), ret == 0 ? "success" : "fail");
        if(ret != 0) {
            RunGame_EXIT(1);
        }
    } 
#else 
    char resourceDir[MAXPATHLEN];
    if(!GetBundleResourcePath(resourceDir, MAXPATHLEN)) {
        fprintf(stderr, "error: Could not retrieve bundle resource directory\n");
        RunGame_EXIT(1);
    }
    
    if (0 != chdir(resourceDir)) {
        fprintf(stderr,"chdir(%s) : failed\n", resourceDir);
        RunGame_EXIT(1);
    }
    
#endif
}

//void HandleAssert(const char* cpMessage, const char* cpFunc, const char* cpFile, const int cLine) {
// gEnv->pLog->LogError("Assert failed: %s - Function:%s in %s:%d",cpMessage,cpFunc,cpFile,cLine);
//}


bool GetAbsoluteExecutablePath(CryPathString& strAppPath)
{
	// Get the relative path
	char szRelPathBuffer[MAXPATHLEN];
	char* szRelPath(NULL);
	uint32 uReqSize(MAXPATHLEN);
	if (_NSGetExecutablePath(szRelPathBuffer, &uReqSize) != 0)
	{
		szRelPath = new char[uReqSize];
		if (_NSGetExecutablePath(szRelPath, &uReqSize) == 0)
        {
			delete [] szRelPath;
            fprintf(stderr, "Could not read the application path\n");
            return false;
        }
	}
	
	// Convert to absolute path
    char resolvedPath[PATH_MAX];
	char* szAbsPath(realpath(szRelPath == NULL ? szRelPathBuffer : szRelPath, resolvedPath));
	strAppPath.assign(resolvedPath);
	
	delete [] szRelPath;
	
	return true;
}

#if defined(_LIB)
//extern void WrappedF_InitCWD();
extern "C" IGameStartup* CreateGameStartup();
#endif

//-------------------------------------------------------------------------------------
// Backtrace + core dump
//-------------------------------------------------------------------------------------

static const char* g_btLibPath = 0;

static void DemangleSymbols(FILE* output, const char* obj, void* baseAddress, void* ip, int index)
{
	char buffer[1024];
	snprintf(buffer, sizeof(buffer), "xcrun atos -o \"%s\" -arch x86_64 -l %p %p",obj, baseAddress, ip);

	FILE* in_pipe = popen(buffer, "r");

	if (!in_pipe)
	{
		fprintf(output, "[%02d] Failed to demangle: %s %p %p\n", index, obj, baseAddress, ip);
		return;
	}

	size_t bytes_read = 0;
	while (!feof(in_pipe))
	{
		if (fgets(buffer, sizeof(buffer), in_pipe))
		{
			fprintf(output, "[%02d] %s", index, buffer);
		}
	}

	pclose(in_pipe);
}

static void SignalHandler(int sig, siginfo_t* info, void* secret)
{
	FILE* btfile = fopen("backtrace.log","w");
	if (!btfile)
		btfile = stderr;

	fprintf(btfile, "Back Trace dump:\n\n");

	char name[256];
	unw_cursor_t cursor; unw_context_t uc;
	unw_word_t ip, sp, offp;

	unw_getcontext(&uc);
	unw_init_local(&cursor, &uc);
	int index = 0;
	while (unw_step(&cursor) > 0)
	{
		char file[256];
		int line = 0;

		name[0] = '\0';
		unw_get_proc_name(&cursor, name, 256, &offp);
		unw_get_reg(&cursor, UNW_REG_IP, &ip);
		unw_get_reg(&cursor, UNW_REG_SP, &sp);
		Dl_info info;
		if (dladdr((void*)ip, &info))
		{

			DemangleSymbols(btfile, info.dli_fname, info.dli_fbase, (void*) ip, index);
		}
		else
		{
			fprintf(btfile, "[%02d] Unknown\n",index);
		}
		index++;
	}
	// Stack trace is currently being handled by Apple's Crash Reporter
	// We just print out the PID of the core dump for later retrieval
	pid_t pid = getpid();

	fprintf(btfile, "\nCore file dumped to /core/core.%d\n", pid);
	fclose(btfile);
	abort();
}

static void InitStackTracer(const char* libPath)
{
	g_btLibPath = libPath;
	struct sigaction sa;
	sa.sa_sigaction = SignalHandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART | SA_SIGINFO;
	sigaction(SIGSEGV, &sa, 0);
	sigaction(SIGBUS, &sa, 0);
	sigaction(SIGILL, &sa, 0);
}

//-------------------------------------------------------------------------------------
// RunGame
//-------------------------------------------------------------------------------------
int RunGame(const char *) __attribute__ ((noreturn));
int RunGame(const char *commandLine)
{
	CryPathString strAppPath;
	if (!GetAbsoluteExecutablePath(strAppPath))
		RunGame_EXIT(1);

	CryPathString strAppDir(PathUtil::GetParentDirectoryStackString(strAppPath));
	CryPathString strLibDir = strAppDir;

	InitStackTracer(strLibDir.c_str());

	size_t uDefStackSize;
	if (!IncreaseResourceMaxLimit(RLIMIT_CORE, RLIM_INFINITY) ||         // RLIM_INFINITY is used to set as unlimited, not MAX_UINT (error on Mac OS)
		!IncreaseSackSizeToMax())
	{
		RunGame_EXIT(1);
	}


	InitRootDir(strAppDir);
	WrappedF_InitCWD();

	SetModulePath(strLibDir.c_str());
#if !defined(_LIB)
	HMODULE systemlib = CryLoadLibraryDefName("CrySystem");
	if(!systemlib)
	{
		printf("Failed to load CrySystem: %s", dlerror());
		exit(1);
	}
#endif

#if !defined(DEDICATED_SERVER)
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
		exit(1);
	}
#endif

	HMODULE gameDll = 0;
#ifndef _LIB
	CEngineConfig engineCfg;
	// workaround: compute .so name from dll name
	string dll_name = engineCfg.m_gameDLL.c_str();

	string::size_type extension_pos = dll_name.rfind(".dll");
	string shared_lib_name = string(CrySharedLibraryPrefix) + dll_name.substr(0, extension_pos) + string(CrySharedLibraryExtension);

	gameDll = CryLoadLibrary(shared_lib_name.c_str());
	if( !gameDll )
	{
		fprintf(stderr, "ERROR: failed to load GAME DLL (%s)\n", dlerror());
		RunGame_EXIT(1);
	}

	// get address of startup function
	IGameStartup::TEntryFunction CreateGameStartup = (IGameStartup::TEntryFunction)CryGetProcAddress(gameDll, "CreateGameStartup");
	if (!CreateGameStartup)
	{
		// dll is not a compatible game dll
		CryFreeLibrary(gameDll);
		fprintf(stderr, "Specified Game DLL is not valid!\n");
		RunGame_EXIT(1);
	}
#endif //_LIB

	// create the startup interface
	IGameStartup* pGameStartup = CreateGameStartup();

	if (!pGameStartup)
	{
		fprintf(stderr, "ERROR: Failed to create the GameStartup Interface!\n");
		RunGame_EXIT(1);
	}

	SSystemInitParams startupParams;
	//memset(&startupParams, 0, sizeof(SSystemInitParams));
	//strcpy(startupParams.szBinariesDir, "Bin");
#if defined(DEDICATED_SERVER)
	startupParams.sLogFileName = "Server.log";
	startupParams.bDedicatedServer = true;
#else
	startupParams.sLogFileName = "Game.log";
	startupParams.bDedicatedServer = false;
#endif
	strcpy(startupParams.szSystemCmdLine, commandLine);
	//	//startupParams.bMinimal = true;
	//startupParams.bSkipInput = true;

	startupParams.pUserCallback = NULL;
	// run the game
	if (pGameStartup->Init(startupParams))
	{
		//g_pUnixConsole = (CUNIXConsole*)gEnv->pSystem->GetITextModeConsole();
		pGameStartup->Run(NULL);
		pGameStartup->Shutdown();
		pGameStartup = 0;
		CryFreeLibrary(gameDll);
#if defined(APPLE_BUNDLE)
		ShutdownAppKit();
#endif
		RunGame_EXIT(0);
	}
	else
	{
		fprintf(stderr, "3!\n");

		// if initialization failed, we still need to call shutdown
		pGameStartup->Shutdown();
		pGameStartup = 0;

		CryFreeLibrary(gameDll);
#if defined(APPLE_BUNDLE)
		ShutdownAppKit();
#endif
		RunGame_EXIT(1);
	}
}

// An unreferenced function.  This function is needed to make sure that
// unneeded functions don't make it into the final executable.  The function
// section for this function should be removed in the final linking.
void this_function_is_not_used(void)
{
}

// CryCommon/WinBase.cpp
void InitFileList(void);

#if defined(_DEBUG)
// Debug code for running the executable with GDB attached.  We'll start an
// XTerm with a GDB attaching to the running process.
void AttachGDB(const char *program)
{
	(void) program;
	int pid = getpid();
	char command[1024];

	snprintf(
			command, sizeof command,
			"osascript -e 'tell application \"Terminal\" to do script \"lldb -p%i\"'",
			pid);
	int shell_pid = fork();
	if (shell_pid == -1)
	{
		perror("fork()");
		exit(EXIT_FAILURE);
	}
	if (shell_pid > 0)
	{
		// Allow a few seconds for GDB to attach.
		sleep(5);
		return;
	}
	// Detach.
	for (int i = 0; i < 100; ++i)
		close(i);
	setsid();
	system(command);
	exit(EXIT_SUCCESS);
}
#endif



//-------------------------------------------------------------------------------------
// Name: main()
// Desc: The application's entry point
//-------------------------------------------------------------------------------------


int main(int argc, char **argv)
{
#if defined(APPLE_BUNDLE)
	InitAppKit();
#endif

	int err;

#if defined(_DEBUG)
	// If the first command line option is -debug, then we'll attach GDB.
	for (int i = 1; i < argc; ++i)
	{
		if (!strcmp(argv[i], "-debug"))
		{
			AttachGDB(argv[0]);
			break;
		}
	}
#endif

	InitFileList();

	// Build the command line.
	// We'll attempt to re-create the argument quoting that was used in the
	// command invocation.
	size_t cmdLength = 0;
	char needQuote[argc];
	for (int i = 0; i < argc; ++i)
	{
		bool haveSingleQuote = false, haveDoubleQuote = false, haveBrackets = false;
		bool haveSpace = false;
		for (const char *p = argv[i]; *p; ++p)
		{
			switch (*p)
			{
			case '"': haveDoubleQuote = true; break;
			case '\'': haveSingleQuote = true; break;
			case '[': case ']': haveBrackets = true; break;
			case ' ': haveSpace = true; break;
			default: break;
			}
		}
		needQuote[i] = 0;
		if (haveSpace || haveSingleQuote || haveDoubleQuote || haveBrackets)
		{
			if (!haveSingleQuote)
				needQuote[i] = '\'';
			else if (!haveDoubleQuote)
				needQuote[i] = '"';
			else if (!haveBrackets)
				needQuote[i] = '[';
			else
			{
				fprintf(stderr, "MacLauncher Error: Garbled command line\n");
				exit(EXIT_FAILURE);
			}
		}
		cmdLength += strlen(argv[i]) + (needQuote[i] ? 2 : 0);
		if (i > 0)
			++cmdLength;
	}
	char cmdLine[cmdLength + 1], *q = cmdLine;
	for (int i = 0; i < argc; ++i)
	{
		if (i > 0)
			*q++ = ' ';
		if (needQuote[i])
			*q++ = needQuote[i];
		strcpy(q, argv[i]);
		q += strlen(q);
		if (needQuote[i])
			if (needQuote[i] == '[')
				*q++ = ']';
		else
			*q++ = needQuote[i];
	}
	*q = 0;
	assert(q - cmdLine == cmdLength);

	return RunGame(cmdLine);
}

// vim:sw=2:ts=2:si

