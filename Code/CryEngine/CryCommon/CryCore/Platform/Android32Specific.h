// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#define SIZEOF_PTR    4
//#define TARGET_DEFAULT_ALIGN (0x4U)
#define TARGET_DEFAULT_ALIGN (16U)

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

typedef double                        real;

typedef uint32                        DWORD;
typedef DWORD*                        LPDWORD;
typedef DWORD                         DWORD_PTR;
typedef int INT_PTR, *PINT_PTR;
typedef unsigned int UINT_PTR, *PUINT_PTR;
typedef char* LPSTR, *PSTR;
typedef uint32                        __uint32;
typedef int32                         INT32;
typedef uint32                        UINT32;
typedef uint64                        __uint64;
typedef int64                         INT64;
typedef uint64                        UINT64;

typedef long LONG_PTR, *PLONG_PTR, *PLONG;
typedef unsigned long ULONG_PTR, *PULONG_PTR;

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
typedef const char* LPCSTR, *PCSTR;
typedef long long                     LONGLONG;
typedef ULONG_PTR                     SIZE_T;
typedef unsigned char                 byte;

typedef const void* LPCVOID;

#include "AndroidSpecific.h"