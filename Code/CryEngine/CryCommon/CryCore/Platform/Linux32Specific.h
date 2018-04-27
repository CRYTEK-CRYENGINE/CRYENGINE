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
#define SIZEOF_PTR    4
#define TARGET_DEFAULT_ALIGN (0x4U)

//////////////////////////////////////////////////////////////////////////
// Standard includes.
//////////////////////////////////////////////////////////////////////////
#include <malloc.h>
//#include <winbase.h>
#include <stdint.h>
#include <sys/dir.h>
#include <sys/io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Define platform independent types.
//////////////////////////////////////////////////////////////////////////
#include <CryCore/BaseTypes.h>

typedef signed long long              INT64;

typedef double                        real;

typedef unsigned long                 DWORD;
typedef unsigned long*                LPDWORD;
typedef DWORD                         DWORD_PTR;
typedef int INT_PTR, *                PINT_PTR;
typedef unsigned int UINT_PTR, *      PUINT_PTR;
typedef char* LPSTR, *                PSTR;

typedef long LONG_PTR, * PLONG_PTR, * PLONG;
typedef unsigned long ULONG_PTR, *    PULONG_PTR;

typedef unsigned char                 BYTE;
typedef unsigned short                WORD;
typedef void*                         HWND;
typedef UINT_PTR                      WPARAM;
typedef LONG_PTR                      LPARAM;
typedef LONG_PTR                      LRESULT;
#define PLARGE_INTEGER LARGE_INTEGER *
typedef const char* LPCSTR, *         PCSTR;
typedef long long                     LONGLONG;
typedef ULONG_PTR                     SIZE_T;
typedef unsigned char                 byte;

#define __int64   long long


#include "LinuxSpecific.h"

