// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   TypeInfo.h
//  Version:     v1.00
//  Created:     19/03/2007 by Scott.
//  Description: Macros and other definitions needed for TypeInfo declarations.
// -------------------------------------------------------------------------
//  History:		 Refactored out of Endian.h
//
////////////////////////////////////////////////////////////////////////////

#ifndef __TypeInfo_h__
#define __TypeInfo_h__

#pragma once

// Meta-type support.

// Currently enable type info for all platforms.
#if !defined(ENABLE_TYPE_INFO)
	#define ENABLE_TYPE_INFO
#endif
#ifdef ENABLE_TYPE_INFO

struct CTypeInfo;

template<class T>
inline const CTypeInfo& TypeInfo(const T* t);

namespace Detail
{
template<typename T, bool bIsEnum = std::is_enum<T>::value>
struct SEnumHelper
{
	static const CTypeInfo& TypeInfo(const T* t)
	{
		return t->TypeInfo();
	}
};

template<typename T>
struct SEnumHelper<T, true>
{
	static const CTypeInfo& TypeInfo(const T* t)
	{
		const typename std::underlying_type<T>::type u = *t;
		return ::TypeInfo(&u);
	}
};
}

//! If TypeInfo exists for T, it is accessed via TypeInfo(T*).
//! Default TypeInfo() is implemented by a struct member function.
template<class T>
inline const CTypeInfo& TypeInfo(const T* t)
{
	return Detail::SEnumHelper<T>::TypeInfo(t);
}

//! Declare a class's TypeInfo member.
	#define STRUCT_INFO \
	  const CTypeInfo &TypeInfo() const

	#define NULL_STRUCT_INFO \
	  const CTypeInfo &TypeInfo() const { return *(CTypeInfo*)0; }

//! Declare an override for a type without TypeInfo() member (e.g. basic type).
	#define DECLARE_TYPE_INFO(Type) \
	  template<> const CTypeInfo &TypeInfo(const Type*)

//! Template version.
	#define DECLARE_TYPE_INFO_T(Type) \
	  template<class T> const CTypeInfo &TypeInfo(const Type<T>*)

//! Type info declaration, with additional prototypes for string conversions.
	#define BASIC_TYPE_INFO(Type)                 \
	  string ToString(Type const & val);          \
	  bool FromString(Type & val, const char* s); \
	  DECLARE_TYPE_INFO(Type)

	#define CUSTOM_STRUCT_INFO(Struct)  \
	  const CTypeInfo &TypeInfo() const \
	  { static Struct Info; return Info; }

#else // ENABLE_TYPE_INFO

	#define STRUCT_INFO
	#define NULL_STRUCT_INFO
	#define DECLARE_TYPE_INFO(Type)
	#define DECLARE_TYPE_INFO_T(Type)
	#define BASIC_TYPE_INFO(T)

#endif // ENABLE_TYPE_INFO

//! Specify automatic tool generation of TypeInfo bodies.
#define AUTO_STRUCT_INFO STRUCT_INFO
#define AUTO_TYPE_INFO   DECLARE_TYPE_INFO
#define AUTO_TYPE_INFO_T DECLARE_TYPE_INFO_T

//! Obsolete "LOCAL" versions (all infos now generated in local files).
#define AUTO_STRUCT_INFO_LOCAL STRUCT_INFO
#define AUTO_TYPE_INFO_LOCAL   DECLARE_TYPE_INFO
#define AUTO_TYPE_INFO_LOCAL_T DECLARE_TYPE_INFO_T

//! Overrides for basic types.
DECLARE_TYPE_INFO(void);

BASIC_TYPE_INFO(bool);

BASIC_TYPE_INFO(char);
BASIC_TYPE_INFO(wchar_t);
BASIC_TYPE_INFO(signed char);
BASIC_TYPE_INFO(unsigned char);
BASIC_TYPE_INFO(short);
BASIC_TYPE_INFO(unsigned short);
BASIC_TYPE_INFO(int);
BASIC_TYPE_INFO(unsigned int);
BASIC_TYPE_INFO(long);
BASIC_TYPE_INFO(unsigned long);
BASIC_TYPE_INFO(int64);
BASIC_TYPE_INFO(uint64);

BASIC_TYPE_INFO(float);
BASIC_TYPE_INFO(double);

DECLARE_TYPE_INFO(string);

//! All pointers share same TypeInfo.
const CTypeInfo&        PtrTypeInfo();
template<class T>
inline const CTypeInfo& TypeInfo(T** t)
{
	return PtrTypeInfo();
}

#endif // __TypeInfo_h__
