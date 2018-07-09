// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   Win32specific.h
//  Version:     v1.00
//  Created:     31/03/2003 by Sergiy.
//  Compilers:   Visual Studio.NET
//  Description: Specific to Win32 declarations, inline functions etc.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once

// Ensure WINAPI version is consistent everywhere
#define _WIN32_WINNT  0x0600
#define NTDDI_VERSION 0x06000000
#define WINVER        0x0600

#define RC_EXECUTABLE "rc.exe"
#define SIZEOF_PTR    8

#pragma warning( disable : 4267 ) //warning C4267: 'initializing' : conversion from 'size_t' to 'unsigned int', possible loss of data

//////////////////////////////////////////////////////////////////////////
// Standard includes.
//////////////////////////////////////////////////////////////////////////
#include <malloc.h>
#include <io.h>
#include <new.h>
#include <direct.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <float.h>
//////////////////////////////////////////////////////////////////////////

// Special intrinsics
#include <math.h> // Should be included before intrin.h
#include <intrin.h>
#include <process.h>

//////////////////////////////////////////////////////////////////////////
// Define platform independent types.
//////////////////////////////////////////////////////////////////////////
#include <CryCore/BaseTypes.h>

#define THREADID_NULL 0
typedef long                          LONG;
typedef unsigned char                 BYTE;
typedef unsigned long                 threadID;
typedef unsigned long                 DWORD;
typedef double                        real; //!< Biggest float-type on this machine.

typedef void*                         THREAD_HANDLE;
typedef void*                         EVENT_HANDLE;

typedef __int64 INT_PTR, *            PINT_PTR;
typedef unsigned __int64 UINT_PTR, *  PUINT_PTR;

typedef __int64 LONG_PTR, *           PLONG_PTR;
typedef unsigned __int64 ULONG_PTR, * PULONG_PTR;

typedef ULONG_PTR DWORD_PTR, *        PDWORD_PTR;

#define SIZEOF_PTR 8

#ifndef FILE_ATTRIBUTE_NORMAL
	#define FILE_ATTRIBUTE_NORMAL 0x00000080
#endif

#define TARGET_DEFAULT_ALIGN (0x8U)
