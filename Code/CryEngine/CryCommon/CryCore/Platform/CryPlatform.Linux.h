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
#ifndef _CRY_PLATFORM_LINUX_H_
#define _CRY_PLATFORM_LINUX_H_

////////////////////////////////////////////////////////////////////////////
// check that we are allowed to be included
#if !defined(CRYPLATFROM_ALLOW_DETAIL_INCLUDES)
	#error Please include CryPlatfrom.h instead of this private implementation header
#endif

////////////////////////////////////////////////////////////////////////////
// util macros for __DETAIL__LINK_THIRD_PARTY_LIBRARY and __DETAIL__LINK_SYSTEM_PARTY_LIBRARY
// SNC is a little strange with macros and pragmas
// the lib pragma requieres a string literal containing a escaped string literal
// eg _Pragma ("comment (lib, \"<lib>\")")
#define __DETAIL__CREATE_PRAGMA(x) _Pragma(CRY_CREATE_STRING(x))

////////////////////////////////////////////////////////////////////////////
// Create a string from an Preprocessor macro or a literal text
#define CRY_DETAIL_CREATE_STRING(string) # string
#define CRY_CREATE_STRING(string)        CRY_DETAIL_CREATE_STRING(string)

////////////////////////////////////////////////////////////////////////////
#define __DETAIL__LINK_THIRD_PARTY_LIBRARY(name) \
  __DETAIL__CREATE_PRAGMA("comment(lib, \"" CRY_CREATE_STRING(CODE_BASE_FOLDER) name "\")")

////////////////////////////////////////////////////////////////////////////
#define __DETAIL__LINK_SYSTEM_PARTY_LIBRARY(name) \
  __DETAIL__CREATE_PRAGMA("comment(lib, \"" name "\")")

#endif // _CRY_PLATFORM_LINUX_H_
