// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// SWIG can't handle static_assert so we hide it behind a macro.
#if SWIG
#define static_assert(...)
#endif

static_assert(sizeof(char) == 1, "Wrong type size!");
static_assert(sizeof(float) == 4, "Wrong type size!");
static_assert(sizeof(int) >= 4, "Wrong type size!");

typedef unsigned char  uchar;
typedef signed char    schar;

typedef unsigned short ushort;
typedef signed short   sshort;

#if !defined(CLANG_FIX_UINT_REDEF)
typedef unsigned int       uint;
#endif
typedef signed int         sint;

typedef unsigned long      ulong;
typedef signed long        slong;

typedef unsigned long long ulonglong;
typedef signed long long   slonglong;

static_assert(sizeof(uchar) == sizeof(schar), "Wrong type size!");
static_assert(sizeof(ushort) == sizeof(sshort), "Wrong type size!");
static_assert(sizeof(uint) == sizeof(sint), "Wrong type size!");
static_assert(sizeof(ulong) == sizeof(slong), "Wrong type size!");
static_assert(sizeof(ulonglong) == sizeof(slonglong), "Wrong type size!");

static_assert(sizeof(uchar) <= sizeof(ushort), "Wrong type size!");
static_assert(sizeof(ushort) <= sizeof(uint), "Wrong type size!");
static_assert(sizeof(uint) <= sizeof(ulong), "Wrong type size!");
static_assert(sizeof(ulong) <= sizeof(ulonglong), "Wrong type size!");

typedef schar int8;
typedef schar sint8;
typedef uchar uint8;
static_assert(sizeof(uint8) == 1, "Wrong type size!");
static_assert(sizeof(sint8) == 1, "Wrong type size!");

typedef sshort int16;
typedef sshort sint16;
typedef ushort uint16;
static_assert(sizeof(uint16) == 2, "Wrong type size!");
static_assert(sizeof(sint16) == 2, "Wrong type size!");

typedef sint int32;
typedef sint sint32;
typedef uint uint32;
static_assert(sizeof(uint32) == 4, "Wrong type size!");
static_assert(sizeof(sint32) == 4, "Wrong type size!");

typedef slonglong int64;
typedef slonglong sint64;
typedef ulonglong uint64;
static_assert(sizeof(uint64) == 8, "Wrong type size!");
static_assert(sizeof(sint64) == 8, "Wrong type size!");

typedef float  f32;
typedef double f64;
static_assert(sizeof(f32) == 4, "Wrong type size!");
static_assert(sizeof(f64) == 8, "Wrong type size!");
