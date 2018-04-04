/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */


#pragma once

// Use ConfigLocal.h to redefine any of the options below.
#include "ConfigLocal.h"

#ifndef YASLI_API
#define YASLI_API
#endif

#ifndef PROPERTY_TREE_API
#define PROPERTY_TREE_API
#endif

#ifndef YASLI_INLINE_IMPLEMENTATION
#define YASLI_INLINE_IMPLEMENTATION 0
#define YASLI_INLINE
#endif

#ifndef YASLI_CXX11
#define YASLI_CXX11 1
#endif

// Disable C++ RTTI use (e.g. typeid())
#ifndef YASLI_NO_RTTI
#define YASLI_NO_RTTI 1
#endif

#ifndef YASLI_NO_FCVT
#define YASLI_NO_FCVT 0
#endif

// Disable extra struct-level for polymorphic pointer serialization
#ifndef YASLI_NO_EXTRA_BLOCK_FOR_POINTERS
#define YASLI_NO_EXTRA_BLOCK_FOR_POINTERS 0
#endif

// Default filter for Archive: 0 - strict mode, -1 - silent mode.
#ifndef YASLI_DEFAULT_FILTER
#define YASLI_DEFAULT_FILTER 0
#endif

// Serialize std::pair: 1 - ("first", "second") or 0 - ("key", "value")
#ifndef YASLI_STD_PAIR_FIRST_SECOND
#define YASLI_STD_PAIR_FIRST_SECOND 0
#endif

// Useful to compile out editing functionality like Archive::isEdit() check
// warning/error messages.
#ifndef YASLI_NO_EDITING
#define YASLI_NO_EDITING 0
#endif

#ifdef _DEBUG
// BinArchives use short hash of name to compact and speed up. Collision on particular level of hierarchy could cause to wrong result.
//#define YASLI_BIN_ARCHIVE_CHECK_HASH_COLLISION 

// Node with empty name hasn't size information. So this block will not be suited for search. Using non-empty names in such a block is dangerous.
#define YASLI_BIN_ARCHIVE_CHECK_EMPTY_NAME_MIX
#endif

// This allows to change the name of global serialization function and
// serialization method to match the coding conventions of the codebase.
#ifndef YASLI_SERIALIZE_OVERRIDE
#define YASLI_SERIALIZE_OVERRIDE serialize
#endif

#ifndef YASLI_SERIALIZE_METHOD
#define YASLI_SERIALIZE_METHOD serialize
#endif

// Allows to override default integer typedefs
// See note at CastInteger in Archive.h for details.
#ifndef YASLI_INTS_DEFINED
#ifdef _MSC_VER
namespace yasli
{
	typedef signed __int8 i8;
	typedef signed __int16 i16;
	typedef signed __int32 i32;
	typedef signed __int64 i64;
	typedef unsigned __int8 u8;
	typedef unsigned __int16 u16;
	typedef unsigned __int32 u32;
	typedef unsigned __int64 u64;
};
#else
#include <stdint.h>
namespace yasli
{
	typedef int8_t i8;
	typedef int16_t i16;
	typedef int32_t i32;
	typedef int64_t i64;
	typedef uint8_t u8;
	typedef uint16_t u16;
	typedef uint32_t u32;
	typedef uint64_t u64;
};
#endif
#endif

// Allows to override default string implementation (should be STL-compatible)
#ifndef YASLI_STRINGS_DEFINED
#include <string>
namespace yasli {
	using std::string;
	using std::wstring;
}
#endif

#ifndef YASLI_STRING_NAMESPACE_BEGIN
#define YASLI_STRING_NAMESPACE_BEGIN namespace std {
#endif
#ifndef YASLI_STRING_NAMESPACE_END
#define YASLI_STRING_NAMESPACE_END }
#endif

#ifndef YASLI_STRING_LIST_BASE_DEFINED
#include <vector>
namespace yasli {
	typedef std::vector<const char*> StringListStaticBase;
	typedef std::vector<string> StringListBase;
}
#endif

// Can be used to override YASLI_ASSERT and YASLI_CHECK, see yasli/Assert.h
#ifndef YASLI_ASSERT_DEFINED
#endif

#ifndef YASLI_INCLUDE_PROPERTY_TREE_CONFIG_LOCAL
#define YASLI_INCLUDE_PROPERTY_TREE_CONFIG_LOCAL 0
#endif