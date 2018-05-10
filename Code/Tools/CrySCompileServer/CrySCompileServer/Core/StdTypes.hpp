// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __STDTYPES_DUMMY__
#define __STDTYPES_DUMMY__

/*#if (_MSC_VER < 1300) && defined(__cplusplus)
   extern "C++" {
#endif 
#     include <wchar.h>
#if (_MSC_VER < 1300) && defined(__cplusplus)
   }
#endif*/

#ifdef _MSC_VER
typedef signed __int8     int8_t;
typedef signed __int16    int16_t;
typedef signed __int32    int32_t;
typedef signed __int64    int64_t;
typedef unsigned __int8   uint8_t;
typedef unsigned __int16  uint16_t;
typedef unsigned __int32  uint32_t;
typedef unsigned __int64  uint64_t;
#endif

#ifdef UNIX
#include <stdint.h>
#include "Core/UnixCompat.h"
#endif

#endif

