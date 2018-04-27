// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   iOSSpecific.h
//  Version:     v1.00
//  Created:     Leander Beernaert based on the MacSpecifc files
//  Compilers:   Clang
//  Description: iOS specific declarations
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __IOSSPECIFIC_H__
#define __IOSSPECIFIC_H__

#include <CryPlatform/AppleSpecific.h>
#include <float.h>
#include <TargetConditionals.h>

#if TARGET_IPHONE_SIMULATOR
	#define IOS_SIMULATOR
	#include <xmmintrin.h>
#endif

// stubs for virtual keys, isn't used on iOS
#define VK_UP      0
#define VK_DOWN    0
#define VK_RIGHT   0
#define VK_LEFT    0
#define VK_CONTROL 0
#define VK_SCROLL  0

//#define USE_CRT 1
#if !CRY_PLATFORM_64BIT
	#error "iOS build only supports the 64bit architecture"
#else
	#define SIZEOF_PTR 8
typedef uint64_t threadID;
#endif

#endif // __IOSSPECIFIC_H__
