// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Platform/platform.h>

#if defined(EDITOR_COMMON_EXPORTS)
#define PROPERTY_TREE_API __declspec(dllexport)
#elif defined(EDITOR_COMMON_IMPORTS)
#define PROPERTY_TREE_API __declspec(dllimport)
#else
#define PROPERTY_TREE_API 
#endif

#if defined(CRY_PLATFORM_ORBIS) || defined(CRY_PLATFORM_ANDROID)
#define YASLI_NO_FCVT 1
#else
#define YASLI_NO_FCVT 0
#endif

#define YASLI_SERIALIZE_METHOD Serialize
#define YASLI_SERIALIZE_OVERRIDE Serialize

#define YASLI_STRINGS_DEFINED
#ifdef RESOURCE_COMPILER
namespace yasli {
typedef CryStringLocalT<char> string;
typedef CryStringLocalT<wchar_t> wstring;
}
#else
namespace yasli {
typedef ::string string;
typedef ::wstring wstring;
}
#endif
namespace Serialization
{
typedef yasli::string string;
typedef yasli::wstring wstring;
}

#define YASLI_STRING_NAMESPACE_BEGIN 
#define YASLI_STRING_NAMESPACE_END 

#define YASLI_ASSERT_DEFINED
#define YASLI_ASSERT(x) CRY_ASSERT(x)
#define YASLI_ASSERT_STR(x,str) CRY_ASSERT_MESSAGE(x,str)
#define YASLI_ESCAPE(x, action) if(!(x)) { YASLI_ASSERT(0 && #x); action; };
#define YASLI_CHECK(x) (x)

#define YASLI_INTS_DEFINED 1
namespace yasli
{
	typedef int8 i8;
	typedef int16 i16;
	typedef int32 i32;
	typedef int64 i64;
	typedef uint8 u8;
	typedef uint16 u16;
	typedef uint32 u32;
	typedef uint64 u64;
}

#define YASLI_INLINE_IMPLEMENTATION 1
#define YASLI_INLINE inline
#define YASLI_NO_MAP_AS_DICTIONARY 1

#ifndef CRY_PLATFORM_DESKTOP
#define YASLI_NO_EDITING 1
#endif

// Override string list types so that we can safely pass string lists over dll boundaries.


namespace yasli
{
	class Archive;
}


#include <CryCore/Containers/CryArray.h>

template<class T, class I, class S>
bool Serialize(yasli::Archive& ar, DynArray<T, I, S>& container, const char* name, const char* label);

namespace yasli
{
	typedef ::DynArray<const char*> StringListStaticBase;
	typedef ::DynArray<string> StringListBase;
}

#define YASLI_STRING_LIST_BASE_DEFINED
#define YASLI_INCLUDE_PROPERTY_TREE_CONFIG_LOCAL 1