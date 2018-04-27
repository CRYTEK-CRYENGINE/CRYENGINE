// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CrySystem/ISystem.h>

#include <CryCore/Platform/platform_impl.inl>

#if defined(LINUX32)
#include <CryCore/Platform/Linux32Specific.h>
#elif (CRY_PLATFORM_LINUX && CRY_PLATFORM_64BIT)
#include <CryCore/Platform/Linux64Specific.h>
#endif
#if !defined(DEDICATED_SERVER)
#include <SDL.h>
#endif

#include <CryEntitySystem/IEntity.h>
#include <CrySystem/IConsole.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>				// used for crash handling
#include <sys/resource.h>
#include <sys/prctl.h>
#include <libgen.h>
#include <execinfo.h>
#include <CrySerialization/ClassFactory.h>

// FIXME: get the correct version string from somewhere, maybe a -D supplied
// by the build environment?
#undef VERSION_STRING
#define VERSION_STRING "0.0.1"

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
		//if (uMax == RLIM_INFINITY || uMax > kLimit.rlim_max)
		{
			kLimit.rlim_max = uMax;
			if (setrlimit(iResource, &kLimit) != 0)
			{
				fprintf(stderr, "error: setrlimit (%d, %lu) failed\n", iResource, uMax);
				return false;
			}
		}
	}

	return true;
}

bool IncreaseStackSizeToMax()
{
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
			fprintf(stderr, "error: setrlimit (%d, %ld) failed\n", RLIMIT_STACK, kLimit.rlim_max);
			return false;
		}
	}

	return true;
}

#define RunGame_EXIT(exitCode) (exit(exitCode))

//-------------------------------------------------------------------------------------
// Backtrace + core dump
//-------------------------------------------------------------------------------------

static const char* g_btLibPath = 0;

static void SignalHandler(int sig, siginfo_t* info, void* secret)
{
	void* trace[32];
	int trace_size;

	pid_t pid = getpid();

	FILE* ftrace = fopen("backtrace.log","w");
	if (!ftrace)
		ftrace = stderr;

	// Log symbol dump first
	fprintf(ftrace,"# Stack Trace with Symbols:\n");
	fflush(ftrace);

	trace_size = backtrace(trace, 32);
	backtrace_symbols_fd(trace,trace_size,fileno(ftrace));

	// Log demangled Stack trace
	fprintf(ftrace,"\n# Stack trace demangled (note: some system libraries can not be demangled correctly:\n");

	// close file since the following ouput is sent there from the shell
	if (ftrace != stderr) fclose(ftrace);

	for (int i = 0; i < trace_size; ++i)
	{
		char objname[MAX_PATH];

		Dl_info info;
		if (dladdr(trace[i], &info))
		{
			uintptr_t addr = (uintptr_t)trace[i] - (uintptr_t)info.dli_fbase;
			string base_name = basename((char*)info.dli_fname);

			if (info.dli_fname[0] != '/')
				cry_sprintf(objname, "%s/%s",g_btLibPath, base_name.c_str());
			else
				cry_sprintf(objname, "%s",info.dli_fname);

			char cmd[MAX_PATH];
			// check if it is the launcher or not. If it is the laucnher we need ot use the
			// absolute address instead of the relative address inside the module
			size_t pos = base_name.find_first_of("LinuxLauncher",0);
			if (pos != 0)
				cry_sprintf(cmd, "echo [%02d] $(addr2line  -C -f -i -e \"%s\" -p %p) >> 'backtrace.log'", i, objname, (void*) addr);
			else
				cry_sprintf(cmd, "echo [%02d] $(addr2line  -C -f -i -e \"%s\" -p %p) >> 'backtrace.log'", i, objname, trace[i]);
			system(cmd);
		}
	}

	// If the Linux crash handler exists, use it.
	if(!access("Tools/CrashHandler/LinuxCrashUploader.py", R_OK))
	{
		pid_t thispid = fork();
		if (thispid == 0)			// we're the child process
		{
			char pid_arg[MAX_PATH];
			char sig_arg[MAX_PATH];
			cry_sprintf(pid_arg, "--pid=%d", pid);
			cry_sprintf(sig_arg, "--signal=%d", sig);
			int rval = execlp("python3", "python3", "Tools/CrashHandler/LinuxCrashUploader.py", pid_arg, sig_arg, NULL);
			CryLogAlways("Error number = %d (rval = %d)", errno, rval);
		}
	}
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
	prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);
}

int RunGame(const char *, int, char**) __attribute__ ((noreturn));

int RunGame(const char *commandLine, int argc, char* argv[])
{
	char absPath[ MAX_PATH];
	memset(absPath,0,sizeof(char)* MAX_PATH);
	if (!getcwd(absPath,sizeof(char)* MAX_PATH))
		RunGame_EXIT(1);

	InitStackTracer(absPath);

	size_t uDefStackSize;

	if (!IncreaseResourceMaxLimit(RLIMIT_CORE, RLIM_INFINITY) || !IncreaseStackSizeToMax())
		RunGame_EXIT(1);

#if !defined(DEDICATED_SERVER)
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
		exit(1);
	}
#endif

	SSystemInitParams startupParams;
	memset(&startupParams, 0, sizeof(SSystemInitParams));

	cry_strcpy(startupParams.szSystemCmdLine, commandLine);
#if defined(DEDICATED_SERVER)
	startupParams.sLogFileName = "Server.log";
	startupParams.bDedicatedServer = true;
#else
	startupParams.sLogFileName = "Game.log";
#endif
	startupParams.pUserCallback = NULL;

#if !defined(_LIB)
	SetModulePath(absPath);
#endif
	
	if (!CryInitializeEngine(startupParams))
	{
		fprintf(stderr, "Failed to initialize engine!!\n");
		RunGame_EXIT(1);
	}

	RunGame_EXIT(0);
}

// An unreferenced function.  This function is needed to make sure that
// unneeded functions don't make it into the final executable.  The function
// section for this function should be removed in the final linking.
void this_function_is_not_used(void)
{
}

#if defined(_DEBUG)
// Debug code for running the executable with GDB attached.  We'll start an
// XTerm with a GDB attaching to the running process.
void AttachGDB(const char *program)
{
	int pid = getpid();
	char command[1024];

	// check if more advanced terminal is available
	// e.g.: gnome-terminal on ubuntu
	if (system("which gnome-terminal") == 0)
	{
		cry_sprintf(
			command,
			"gnome-terminal -x gdb '%s' %i",
			program,
			pid);
	}
	else //fall back to xterm
	{
		cry_sprintf(
			command,
			"xterm -n 'GDB [Linux Launcher]' -T 'GDB [Linux Launcher]' -e gdb '%s' %i",
			program,
			pid);
	}
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
	int err;
	// Chdir into exec path if called outside of it.

	char bindir[PATH_MAX];
	cry_strcpy(bindir, argv[0]);
	const char* base = dirname(bindir);
	if (chdir(base) != 0)
	{
		fprintf(stderr,"Failed to chdir into binary directory %s", base);
		exit(EXIT_FAILURE);
	}

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
				fprintf(stderr, "CRYSIS LinuxLauncher Error: Garbled command line\n");
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

	return RunGame(cmdLine, argc, argv);
}

// vim:sw=2:ts=2:si

