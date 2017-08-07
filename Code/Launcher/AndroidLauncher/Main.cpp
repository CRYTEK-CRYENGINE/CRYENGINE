// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryCore/Platform/platform_impl.inl>
#include <CryCore/Platform/AndroidSpecific.h>
#include <android/log.h>
#include <android/asset_manager.h>

#define LOG_TAG "Cry"
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))

#include <SDL.h>
#include <SDL_Extension.h>

#include <CryGame/IGameStartup.h>
#include <CryEntitySystem/IEntity.h>
#include <CryGame/IGameFramework.h>
#include <CrySystem/IConsole.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include <dlfcn.h>

#if defined(_LIB)
#include <CryCore/Common_TypeInfo.h>
STRUCT_INFO_T_INSTANTIATE(Vec2_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Vec2_tpl, <int>)
STRUCT_INFO_T_INSTANTIATE(Vec4_tpl, <short>)
STRUCT_INFO_T_INSTANTIATE(Vec3_tpl, <int>)
STRUCT_INFO_T_INSTANTIATE(Vec3_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Ang3_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Quat_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(QuatT_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Plane_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Matrix33_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Color_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Color_tpl, <uint8>)

#endif

size_t linux_autoload_level_maxsize = PATH_MAX;
char linux_autoload_level_buf[PATH_MAX];
char *linux_autoload_level = linux_autoload_level_buf;


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

#if defined(_LIB)
extern "C" DLL_IMPORT IGameFramework* CreateGameFramework();
#endif

size_t fopenwrapper_basedir_maxsize = MAX_PATH;
namespace { char fopenwrapper_basedir_buffer[MAX_PATH] = ""; }
char * fopenwrapper_basedir = fopenwrapper_basedir_buffer;
bool fopenwrapper_trace_fopen = false;

#define RunGame_EXIT(exitCode) (exit(exitCode))

#define LINUX_LAUNCHER_CONF "launcher.cfg"

static void strip(char *s)
{
	char *p = s, *p_end = s + strlen(s);

	while (*p && isspace(*p)) ++p;
	if (p > s) { memmove(s, p, p_end - s + 1); p_end -= p - s; }
	for (p = p_end; p > s && isspace(p[-1]); --p);
	*p = 0;
}

static void LoadLauncherConfig(void)
{
	char conf_filename[MAX_PATH];
	char line[1024], *eq = 0;
	int n = 0;

	cry_sprintf(conf_filename, "%s/%s", fopenwrapper_basedir, LINUX_LAUNCHER_CONF);
	FILE *fp = fopen(conf_filename, "r");
	if (!fp) return;
	while (true)
	{
		++n;
		if (!fgets(line, sizeof line - 1, fp)) break;
		line[sizeof line - 1] = 0;
		strip(line);
		if (!line[0] || line[0] == '#') continue;
		eq = strchr(line, '=');
		if (!eq)
		{
			fprintf(stderr, "'%s': syntax error in line %i\n",
					conf_filename, n);
			exit(EXIT_FAILURE);
		}
		*eq = 0;
		strip(line);
		strip(++eq);

		if (!strcasecmp(line, "autoload"))
		{
			if (strlen(eq) >= linux_autoload_level_maxsize)
			{
				fprintf(stderr, "'%s', line %i: autoload value too long\n",
						conf_filename, n);
				exit(EXIT_FAILURE);
			}
			strcpy(linux_autoload_level, eq);
		} else
		{
			fprintf(stderr, "'%s': unrecognized config variable '%s' in line %i\n",
					conf_filename, line, n);
			exit(EXIT_FAILURE);
		}
	}
	fclose(fp);
}

int RunGame(const char *) __attribute__ ((noreturn));

const char* g_androidPakPath = "";

#if defined(ANDROID_OBB)
DLL_EXPORT const char* androidGetPackageName()
{
	return SDLExt_AndroidGetPackageName();
}

DLL_EXPORT const char* androidGetExpFilePath()
{
	static char path[1024];

	if (path[0] == '\0')
	{
		cry_strcpy(path, CryGetProjectStoragePath());
		cry_strcat(path, "/Android/obb/");
		cry_strcat(path, androidGetPackageName());
		cry_strcat(path, "/");
	}
	return path;
}

char g_androidMainExpName[256] = {0};
char g_androidPatchExpName[256] = {0};
    
DLL_EXPORT const char* androidGetMainExpName()
{
	return g_androidMainExpName; 
}

DLL_EXPORT const char* androidGetPatchExpName()
{
	return g_androidPatchExpName;
}

DLL_EXPORT const char* androidGetAssetFileName()
{
	return "assets.ogg";
}

DLL_EXPORT AAssetManager* androidGetAssetManager()
{
	return SDLExt_GetAssetManager();
}
#endif

#if !defined(_RELEASE)
struct COutputPrintSink : public IOutputPrintSink
{
	virtual void Print( const char *inszText )
	{
		LOGI("%s", inszText);
	}
};

COutputPrintSink g_androidPrintSink;
#endif

int RunGame(const char *commandLine)
{
	char absPath[ MAX_PATH];
	memset(absPath,0,sizeof(char)* MAX_PATH);

	if (!getcwd(absPath,sizeof(char)* MAX_PATH))
		RunGame_EXIT(1);
	LOGI( "CWD = %s", absPath );

	// Try to figure out where the PAK files are stored
	const char* paths[] = {		
		SDLExt_AndroidGetExternalStorageDirectory(),
		SDL_AndroidGetExternalStoragePath(),		
		SDL_AndroidGetInternalStoragePath()  // user folder files e.g. "/data/user/0/com.crytek.cryengine/files"
	};
	for (int i = 0; i < CRY_ARRAY_COUNT(paths); ++i )
	{
		char path[1024];
		cry_strcpy(path, paths[i]);
		cry_strcat(path, "/engine");
		LOGI( "Searching for %s", path);
		struct stat info;
		if (stat(path, &info) == 0 && (info.st_mode & S_IFMT) == S_IFDIR)
		{
			g_androidPakPath = paths[i];
			break;
		}
	}

	if (strcmp(CryGetProjectStoragePath(), g_androidPakPath) != 0)
	{
		LOGE("Hardcoded path does not match runtime identified internal storage location: Hard coded path:%s Runtime path:%s", CryGetProjectStoragePath(),  g_androidPakPath);
		RunGame_EXIT(1);
	}

	chdir(CryGetProjectStoragePath());

	if (strlen(g_androidPakPath) == 0)
	{
		LOGE( "Unable to locate system.cfg files.  Exiting!" );
		RunGame_EXIT(1);
	}
	LOGI( "system.cfg found in: %s", g_androidPakPath );

	size_t uDefStackSize;

	if (!IncreaseResourceMaxLimit(RLIMIT_CORE, RLIM_INFINITY) || !GetDefaultThreadStackSize(&uDefStackSize) ||
		!IncreaseResourceMaxLimit(RLIMIT_STACK, RLIM_INFINITY*uDefStackSize))
		RunGame_EXIT(1);

	SSystemInitParams startupParams;
	memset(&startupParams, 0, sizeof(SSystemInitParams));

	cry_strcpy(startupParams.szSystemCmdLine, commandLine);
	startupParams.sLogFileName = "Game.log";
	startupParams.pUserCallback = NULL;

#if !defined(_RELEASE)
	startupParams.pPrintSync = &g_androidPrintSink;
#endif

#if !defined(_LIB)
	SetModulePath((strcmp(absPath,"/") == 0) ? "" : absPath);
	HMODULE systemlib = CryLoadLibraryDefName("CrySystem");
	if(!systemlib)
	{
		LOGE("Failed to load CrySystem: %s", dlerror());
		exit(1);
	}
#endif

	HMODULE frameworkDll = 0;

#ifndef _LIB
	frameworkDll = CryLoadLibraryDefName("CryAction");
	if (!frameworkDll)
	{
		LOGE("ERROR: failed to load CryAction! (%s)\n", dlerror());
		RunGame_EXIT(1);
	}

	// get address of startup function
	IGameFramework::TEntryFunction CreateGameFramework = (IGameFramework::TEntryFunction)CryGetProcAddress(frameworkDll, "CreateGameFramework");
	if (!CreateGameFramework)
	{
		CryFreeLibrary(frameworkDll);
		LOGE("ERROR: Specified CryAction library is not valid!\n");
		RunGame_EXIT(1);
	}
#endif //_LIB

	const char *const szAutostartLevel = linux_autoload_level[0] ? linux_autoload_level : NULL;

	// create the startup interface
	IGameFramework* pFramework = CreateGameFramework();
	if (!pFramework)
	{
#ifndef _LIB
		CryFreeLibrary(frameworkDll);
#endif

		LOGE("ERROR: Failed to create the Game Framework Interface!\n");
		RunGame_EXIT(1);
	}

	pFramework->StartEngine(startupParams);

	// The main engine loop has exited at this point, shut down
	pFramework->ShutdownEngine();

#ifndef LIB
	CryFreeLibrary(frameworkDll);
#endif

	RunGame_EXIT(0);
}

//-------------------------------------------------------------------------------------
// Name: main()
// Desc: The application's entry point
//-------------------------------------------------------------------------------------
int _main(int argc, char **argv)
{
	char* cmdLine = "";
	LoadLauncherConfig();

	// Initialize SDL.
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK /*| SDL_INIT_HAPTIC*/ | SDL_INIT_GAMECONTROLLER ) < 0)
	{
		fprintf(stderr, "ERROR: SDL initialization failed: %s\n",
			SDL_GetError());
		exit(EXIT_FAILURE);
	}
	atexit(SDL_Quit);

#if CAPTURE_REPLAY_LOG
	// Since Android doesn't support native command line argument, please
	// uncomment the following line if -memreplay is needed.
	// CryGetIMemReplay()->StartOnCommandLine("-memreplay");
	CryGetIMemReplay()->StartOnCommandLine(cmdLine);
#endif
	return RunGame(cmdLine);
}

/**
 * Entry point when running in SDL framework.
 */
extern "C" int SDL_main(int argc, char* argv[])
{
	_main(0, NULL);
	return 0;
}

// vim:sw=4:ts=4:si:noet

