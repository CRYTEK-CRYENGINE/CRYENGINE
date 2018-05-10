// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   AndroidSpecific.h
//  Version:     v1.00
//  Created:     24/10/2013 by Leander Beernaert.
//  Compilers:   Android NDK
//  Description: Specific to Android declarations, inline functions etc.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#pragma once

#define RC_EXECUTABLE "rc"
#define USE_CRT       1

// Standard includes.
#include <malloc.h>
#include <stdint.h>
#include <fcntl.h>
#include <float.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>
#include <sys/socket.h>

// Define platform independent types.
#include <CryCore/BaseTypes.h>

typedef signed long long              INT64;

typedef char* LPSTR, *PSTR;
typedef uint32                        __uint32;
typedef int32                         INT32;
typedef uint32                        UINT32;
typedef uint64                        __uint64;
typedef int64                         INT64;
typedef uint64                        UINT64;

typedef long LONG_PTR, * PLONG_PTR, * PLONG;
typedef unsigned long ULONG_PTR, *    PULONG_PTR;

typedef unsigned char                 BYTE;
typedef unsigned short                WORD;
typedef int                           INT;
typedef unsigned int                  UINT;
typedef float                         FLOAT;
typedef void*                         HWND;
typedef UINT_PTR                      WPARAM;
typedef LONG_PTR                      LPARAM;
typedef LONG_PTR                      LRESULT;
#define PLARGE_INTEGER LARGE_INTEGER *
typedef const char* LPCSTR, *         PCSTR;
typedef long long                     LONGLONG;
typedef ULONG_PTR                     SIZE_T;
typedef unsigned char                 byte;

typedef uint32 HMONITOR;
typedef const void* LPCVOID;

#define _A_RDONLY (0x01)
#define _A_SUBDIR (0x10)

// Win32 FileAttributes.
#define FILE_ATTRIBUTE_READONLY            0x00000001
#define FILE_ATTRIBUTE_HIDDEN              0x00000002
#define FILE_ATTRIBUTE_SYSTEM              0x00000004
#define FILE_ATTRIBUTE_DIRECTORY           0x00000010
#define FILE_ATTRIBUTE_ARCHIVE             0x00000020
#define FILE_ATTRIBUTE_DEVICE              0x00000040
#define FILE_ATTRIBUTE_NORMAL              0x00000080
#define FILE_ATTRIBUTE_TEMPORARY           0x00000100
#define FILE_ATTRIBUTE_SPARSE_FILE         0x00000200
#define FILE_ATTRIBUTE_REPARSE_POINT       0x00000400
#define FILE_ATTRIBUTE_COMPRESSED          0x00000800
#define FILE_ATTRIBUTE_OFFLINE             0x00001000
#define FILE_ATTRIBUTE_NOT_CONTENT_INDEXED 0x00002000
#define FILE_ATTRIBUTE_ENCRYPTED           0x00004000

#define INVALID_FILE_ATTRIBUTES            (-1)

#include "LinuxSpecific.h"
// These functions do not exist int the wchar.h header.
#undef wscasecomp
#undef wscasencomp
extern int wcsicmp(const wchar_t* s1, const wchar_t* s2);
extern int wcsnicmp(const wchar_t* s1, const wchar_t* s2, size_t count);

#define TARGET_DEFAULT_ALIGN (16U)

#define __debugbreak() __builtin_trap()

// There is no __finite in android, only __isfinite.
#undef __finite
#define __finite  __isfinite

#define S_IWRITE  S_IWUSR

#define _A_RDONLY (0x01)
#define _A_SUBDIR (0x10)
#define _A_HIDDEN (0x02)

// Force reading all paks from single Android OBB.
// #define ANDROID_OBB

// In RELEASE disable printf and fprintf
#if defined(_RELEASE) && !defined(RELEASE_LOGGING)
	#include <stdio.h>
	#undef printf
	#define printf(...)  (void) 0
	#undef fprintf
	#define fprintf(...) (void) 0
#endif


////////////////////////////////////////
// TEMPORARY HARDCODED PATHS
// Could be pulled into CryCommon
////////////////////////////////////////

#include <cstdio>
// Returns path to CRYENGINE and Crytek provided 3rd Party shared libraries 
inline const char* CryGetSharedLibraryStoragePath()
{
	// 1) Try do get storage path via SDLExt from self
	typedef const char* CB_SDLExt_GetSharedLibDirectory();
	void* pFunc = ::dlsym(RTLD_DEFAULT, "SDLExt_GetSharedLibDirectory");
	if (pFunc)
		return ((CB_SDLExt_GetSharedLibDirectory*)(pFunc))();

	// 2) Try to brute force find storage path (arm64-v8a) layout
	static char tempPath[512] = "";
	for (int i = 0; i < 10; i++)
	{
		sprintf(tempPath, "%s%i%s", "/data/app/com.crytek.cryengine-", i, "/lib/arm64");

		// Does directory exist
		struct stat info;
		if (stat(tempPath, &info) == 0 && (info.st_mode & S_IFMT) == S_IFDIR)
		{
			return tempPath;
		}
	}

	// 3) Try to brute force find storage path (arm-v7a) layout
	struct stat info;
	if (stat("/data/data/com.crytek.cryengine/lib", &info) == 0 && (info.st_mode & S_IFMT) == S_IFDIR)
	{
		return tempPath;
	}

	return "";
}

// Get path to user folder
inline const char* CryGetUserStoragePath()
{
	// Could be JNI
	return "/data/user/0/com.crytek.cryengine/files";
}

// Get path to project root. i.e. assets are stored in a sub folder here
inline const char* CryGetProjectStoragePath()
{
	// Could be JNI
	return "/storage/emulated/0";
}

#include <dlfcn.h>
// Returns a handle to the launcher
inline void* CryGetLauncherModuleHandle()
{
	// Calling dlopen(NULL, RTLD_LAZY):
	//   Native App: [might work]   
	//   Java App: [doesn't work]: Java -> Native e.g. via SDL2: ::dlopen(NULL, RTLD_LAZY) does not return handle of launcher but of process that loaded the launcher's .so file

	// TO DO:
	// Could use JNI callback to activity which loaded the launcher .so file so the name does not need to be hardcoded

	char tempPath[1024] = "";
	sprintf(tempPath, "%s%s", CryGetSharedLibraryStoragePath(), "/libAndroidLauncher.so");
	static void* hLauncherModule = ::dlopen(tempPath, RTLD_LAZY);
	return hLauncherModule;

	// On Linux
	//return ::dlopen(NULL, RTLD_LAZY);

	// On Windows
	//GetModuleHandle(0)
}
