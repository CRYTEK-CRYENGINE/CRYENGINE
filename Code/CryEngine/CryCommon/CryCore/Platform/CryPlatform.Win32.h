// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CryPlatform.h
//  Version:     v1.00
//  Created:     31/01/2013 by Christopher Bolte.
//  Compilers:   Visual Studio.NET
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef _CRY_PLATFORM_WIN32_H_
#define _CRY_PLATFORM_WIN32_H_

////////////////////////////////////////////////////////////////////////////
// check that we are allowed to be included
#if !defined(CRYPLATFROM_ALLOW_DETAIL_INCLUDES)
	#error Please include CryPlatfrom.h instead of this private implementation header
#endif

////////////////////////////////////////////////////////////////////////////
// Create a string from an Preprocessor macro or a literal text
#define CRY_DETAIL_CREATE_STRING(string) # string
#define CRY_CREATE_STRING(string)        CRY_DETAIL_CREATE_STRING(string)
#define RESOLVE_MACRO(x)                 x

////////////////////////////////////////////////////////////////////////////
#define __DETAIL__LINK_THIRD_PARTY_LIBRARY(name)                                                \
  __pragma(message(__FILE__ "(" CRY_CREATE_STRING(__LINE__) "): Including SDK Library: " name)) \
  __pragma(comment(lib, RESOLVE_MACRO(CODE_BASE_FOLDER) name))

////////////////////////////////////////////////////////////////////////////
#define __DETAIL__LINK_SYSTEM_PARTY_LIBRARY(name)                                                  \
  __pragma(message(__FILE__ "(" CRY_CREATE_STRING(__LINE__) "): Including System Library: " name)) \
  __pragma(comment(lib, name))

#endif // _CRY_PLATFORM_WIN32_H_
