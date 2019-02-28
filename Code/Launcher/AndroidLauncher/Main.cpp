// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

#include <CryEntitySystem/IEntity.h>
#include <CrySystem/IConsole.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include <dlfcn.h>

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

size_t fopenwrapper_basedir_maxsize = MAX_PATH;
namespace { char fopenwrapper_basedir_buffer[MAX_PATH] = ""; }
char * fopenwrapper_basedir = fopenwrapper_basedir_buffer;
bool fopenwrapper_trace_fopen = false;

#define RunGame_EXIT(exitCode) (exit(exitCode))

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

//-------------------------------------------------------------------------------------
// Name: main()
// Desc: The application's entry point
//-------------------------------------------------------------------------------------
int _main(int argc, char **argv)
{
	// Initialize SDL.
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK /*| SDL_INIT_HAPTIC*/ | SDL_INIT_GAMECONTROLLER ) < 0)
	{
		fprintf(stderr, "ERROR: SDL initialization failed: %s\n",
			SDL_GetError());
		exit(EXIT_FAILURE);
	}
	atexit(SDL_Quit);

	char absPath[MAX_PATH];
	memset(absPath, 0, sizeof(char)* MAX_PATH);

	if (!getcwd(absPath, sizeof(char)* MAX_PATH))
		RunGame_EXIT(1);
	LOGI("CWD = %s", absPath);

	// Try to figure out where the PAK files are stored
	const char* paths[] = {
		SDLExt_AndroidGetExternalStorageDirectory(),
		SDL_AndroidGetExternalStoragePath(),
		SDL_AndroidGetInternalStoragePath()  // user folder files e.g. "/data/user/0/com.crytek.cryengine/files"
	};
	for (int i = 0; i < CRY_ARRAY_COUNT(paths); ++i)
	{
		char path[1024];
		cry_strcpy(path, paths[i]);
		cry_strcat(path, "/engine");
		LOGI("Searching for %s", path);
		struct stat info;
		if (stat(path, &info) == 0 && (info.st_mode & S_IFMT) == S_IFDIR)
		{
			g_androidPakPath = paths[i];
			break;
		}
	}

	if (strcmp(CryGetProjectStoragePath(), g_androidPakPath) != 0)
	{
		LOGE("Hardcoded path does not match runtime identified internal storage location: Hard coded path:%s Runtime path:%s", CryGetProjectStoragePath(), g_androidPakPath);
		RunGame_EXIT(1);
	}

	chdir(CryGetProjectStoragePath());

	if (strlen(g_androidPakPath) == 0)
	{
		LOGE("Unable to locate system.cfg files.  Exiting!");
		RunGame_EXIT(1);
	}
	LOGI("system.cfg found in: %s", g_androidPakPath);

	size_t uDefStackSize;

	if (!IncreaseResourceMaxLimit(RLIMIT_CORE, RLIM_INFINITY) || !GetDefaultThreadStackSize(&uDefStackSize) ||
		!IncreaseResourceMaxLimit(RLIMIT_STACK, RLIM_INFINITY*uDefStackSize))
		RunGame_EXIT(1);

	SSystemInitParams startupParams;
	memset(&startupParams, 0, sizeof(SSystemInitParams));

	startupParams.szSystemCmdLine = "";

#if CAPTURE_REPLAY_LOG
	// Since Android doesn't support native command line argument, please
	// uncomment the following line if -memreplay is needed.
	// startupParams.szSystemCmdLine = "-memreplay";
#endif

	startupParams.sLogFileName = "Game.log";
	startupParams.pUserCallback = NULL;

#if !defined(_RELEASE)
	startupParams.pPrintSync = &g_androidPrintSink;
#endif

#if !defined(_LIB)
	SetModulePath((strcmp(absPath, "/") == 0) ? "" : absPath);
#endif

	if (!CryInitializeEngine(startupParams))
	{
		LOGE("ERROR: Failed to initialize the engine!\n");
		RunGame_EXIT(1);
	}

	RunGame_EXIT(0);
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

