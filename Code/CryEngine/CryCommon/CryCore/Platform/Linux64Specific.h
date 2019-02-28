// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   Linux32Specific.h
//  Version:     v1.00
//  Created:     05/03/2004 by MarcoK.
//  Compilers:   Visual Studio.NET, GCC 3.2
//  Description: Specific to Linux declarations, inline functions etc.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#define __debugbreak() raise(SIGTRAP)
#define RC_EXECUTABLE "rc"
#define USE_CRT       1
#define SIZEOF_PTR    8
#define TARGET_DEFAULT_ALIGN (0x8U)

//////////////////////////////////////////////////////////////////////////
// Standard includes.
//////////////////////////////////////////////////////////////////////////
#include <malloc.h>
#include <stdint.h>
#include <sys/dir.h>
#include <sys/io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

#if CRY_PLATFORM_SSE2
	#include <xmmintrin.h>
#endif
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Define platform independent types.
//////////////////////////////////////////////////////////////////////////
#include <CryCore/BaseTypes.h>

typedef double                        real;

typedef uint32                        DWORD;
typedef DWORD*                        LPDWORD;
typedef uint64                        DWORD_PTR;
typedef intptr_t INT_PTR, *           PINT_PTR;
typedef uintptr_t UINT_PTR, *         PUINT_PTR;
typedef char* LPSTR, *                PSTR;
typedef uint64                        __uint64;
#if !defined(__clang__)
typedef int64                         __int64;
#endif
typedef int64                         INT64;
typedef uint64                        UINT64;

typedef long LONG_PTR, * PLONG_PTR, * PLONG;
typedef unsigned long ULONG_PTR, *    PULONG_PTR;

typedef uint8                         BYTE;
typedef uint16                        WORD;
typedef void*                         HWND;
typedef UINT_PTR                      WPARAM;
typedef LONG_PTR                      LPARAM;
typedef LONG_PTR                      LRESULT;
#define PLARGE_INTEGER LARGE_INTEGER *
typedef const char* LPCSTR, *         PCSTR;
typedef long long                     LONGLONG;
typedef ULONG_PTR                     SIZE_T;
typedef uint8                         byte;


#include "LinuxSpecific.h"



#define __assume(x)
#define __analysis_assume(x)
#define _msize(p) malloc_usable_size(p)
