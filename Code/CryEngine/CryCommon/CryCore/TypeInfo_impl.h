// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   TypeInfo.h
//  Version:     v1.00
//  Created:     03/05/2005 by Scott.
//  Description: Declaration of CTypeInfo, and other things to access meta-type info.
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef TYPE_INFO_IMPL_H
#define TYPE_INFO_IMPL_H

#include "CryCustomTypes.h"

// DECLARATION MACROS
// Used to construct meta TypeInfo objects in AutoTypeInfo files.
// Two possible levels of TypeInfo: default, with size and offset info only, allowing Endian conversion;
// and full, with string, attr, and enum info, allowing UI and serialisation.
// The full version is selected by the ENABLE_TYPE_INFO_NAMES macro.

#ifdef ENABLE_TYPE_INFO

	#ifndef ENABLE_TYPE_INFO_NAMES
//! This symbol must be defined only once per module.
		#define ENABLE_TYPE_INFO_NAMES 0
	#endif

	#if ENABLE_TYPE_INFO_NAMES
		#define TYPE_INFO_NAME(n) # n
	#else
		#define TYPE_INFO_NAME(n) ""
	#endif
	#define TYPE_INFO_NAME_T(n) # n "<>"

// Set of template functions for automatically returning the base element type for any scalar or array variable.

template<class T>
inline T& ElemType(T* at)
{ return *at; }

template<class T, size_t N>
inline T& ElemType(T (*at)[N])
{ return **at; }

template<class T, size_t N, size_t N2>
inline T& ElemType(T (*at)[N][N2])
{ return ***at; }

template<class T, size_t N, size_t N2, size_t N3>
inline T& ElemType(T (*at)[N][N2][N3])
{ return ****at; }

template<class T>
inline T& ValType(T t)
{ static T _t; return _t; }

// Macros for constructing StructInfos (invoked by AutoTypeInfo.h)

	#define DEFINE_TYPE_INFO(T, Type, Args)          \
	  template<> const CTypeInfo &TypeInfo(const T*) \
	  { static Type Info Args; return Info; }        \

	#define STRUCT_INFO_EMPTY_BODY(T)                         \
	  {                                                       \
	    static CStructInfo Info( # T, sizeof(T), alignof(T)); \
	    return Info;                                          \
	  }                                                       \

	#define STRUCT_INFO_EMPTY(T)           \
	  const CTypeInfo &T::TypeInfo() const \
	  STRUCT_INFO_EMPTY_BODY(T)            \

	#define STRUCT_INFO_T_EMPTY(T, Key, Arg) \
	  template<Key Arg> STRUCT_INFO_EMPTY(T<Arg> )

	#define STRUCT_INFO_TYPE_EMPTY(T) \
	  DEFINE_TYPE_INFO(T, CStructInfo, ( # T, sizeof(T), alignof(T)))

	#define STRUCT_INFO_TYPE_T_EMPTY(T, TArgs, TDecl)          \
	  template TDecl const CTypeInfo &TypeInfo(const T TArgs*) \
	  STRUCT_INFO_EMPTY_BODY(T TArgs)                          \

//! Define TypeInfo for a primitive type, without string conversion.
	#define TYPE_INFO_PLAIN(T) DEFINE_TYPE_INFO(T, CTypeInfo, ( # T, sizeof(T), alignof(T)))

//! Define TypeInfo for a basic type (undecomposable as far as TypeInfo cares), with external string converters.
	#define TYPE_INFO_BASIC(T) DEFINE_TYPE_INFO(T, TTypeInfo<T>, ( # T))

//! Variant for int types, allowing conversion between sizes.
	#define TYPE_INFO_INT(T) DEFINE_TYPE_INFO(T, TIntTypeInfo<T>, ( # T))

	#define STRUCT_INFO_BEGIN_INTERNAL(T)       \
	  const CTypeInfo &TypeInfo() const {       \
	    static CStructInfo::CVarInfo Vars[] = { \

	#define STRUCT_INFO_BEGIN(T)                \
	  const CTypeInfo &T::TypeInfo() const {    \
	    static CStructInfo::CVarInfo Vars[] = { \

	#define STRUCT_INFO_END(T)                                                   \
	  };                                                                         \
	  static CStructInfo Info( # T, sizeof(T), alignof(T), CRY_ARRAY_VAR(Vars)); \
	  return Info;                                                               \
	  }

	#define BASE_INFO_ATTRS(BaseType, Attrs) \
	  { ::TypeInfo((const BaseType*)this), "", Attrs, uint32((char*)static_cast<const BaseType*>(this) - (char*)this), 1, 1, 0, 0, 0 },

	#define BASE_INFO(BaseType) BASE_INFO_ATTRS(BaseType, "")

	#define ALIAS_INFO_ATTRS(AliasName, VarName, Attrs) \
	  { ::TypeInfo(&ElemType(&VarName)), TYPE_INFO_NAME(AliasName), Attrs, uint32((char*)&VarName - (char*)this), sizeof(VarName) / sizeof(ElemType(&VarName)), 0, 0, 0, 0 },

	#define VAR_INFO_ATTRS(VarName, Attrs) \
	  ALIAS_INFO_ATTRS(VarName, VarName, Attrs)

	#define VAR_INFO(VarName) \
	  VAR_INFO_ATTRS(VarName, "")

	#define ALIAS_INFO(AliasName, VarName) \
	  ALIAS_INFO_ATTRS(AliasName, VarName, "")

	#define ATTRS_INFO(Attrs) \
	  { ::TypeInfo((void*)0), "", Attrs, 0, 0, 0, 0, 0, 0 },

	#define BITFIELD_INFO(VarName, Bits) \
	  { ::TypeInfo(&ValType(VarName)), TYPE_INFO_NAME(VarName), "", 0, Bits, 0, 1, 0, 0 },

// Conversion macros for older system.
	#define STRUCT_BASE_INFO(BaseType)                   BASE_INFO(BaseType)
	#define STRUCT_VAR_INFO(VarName, InfoName)           VAR_INFO(VarName)
	#define STRUCT_BITFIELD_INFO(VarName, VarType, Bits) BITFIELD_INFO(VarName, Bits)

// Template versions

template<class T>
Array<CTypeInfo const*> TypeInfoArray1(T const* pt)
{
	static CTypeInfo const* s_info = &::TypeInfo(pt);
	return ArrayT(&s_info, 1);
}

	#define STRUCT_INFO_T_BEGIN(T, Key, Arg) \
	  template<Key Arg> STRUCT_INFO_BEGIN(T<Arg> )

	#define STRUCT_INFO_T_END(T, Key, Arg)                                                                                \
	  };                                                                                                                  \
	  static Arg dummy;                                                                                                   \
	  static CStructInfo Info( # T "<>", sizeof(T<Arg> ), alignof(T<Arg> ), CRY_ARRAY_VAR(Vars), TypeInfoArray1(&dummy)); \
	  return Info;                                                                                                        \
	  }

	#define STRUCT_INFO_T2_BEGIN(T, Key1, Arg1, Key2, Arg2) \
	  template<Key1 Arg1, Key2 Arg2>                        \
	  const CTypeInfo &T<Arg1, Arg2>::TypeInfo() const {    \
	    typedef T<Arg1, Arg2> TThis;                        \
	    static CStructInfo::CVarInfo Vars[] = {             \

	#define STRUCT_INFO_T2_END(T, Key1, Arg1, Key2, Arg2)                                                                    \
	  };                                                                                                                     \
	  static Arg1 dummy1; static Arg2 dummy2;                                                                                \
	  static CTypeInfo const* TemplateTypes[] = { &::TypeInfo(&dummy1), &::TypeInfo(&dummy2) };                              \
	  static CStructInfo Info( # T "<,>", sizeof(TThis), alignof(TThis), CRY_ARRAY_VAR(Vars), CRY_ARRAY_VAR(TemplateTypes)); \
	  return Info;                                                                                                           \
	  }

	#define STRUCT_INFO_T_INSTANTIATE(T, TArgs) \
	  template const CTypeInfo &T TArgs::TypeInfo() const;

// Enum type info.

	#if ENABLE_TYPE_INFO_NAMES

// Enums represented as full CEnumInfo types, with string conversion.
		#define ENUM_INFO_BEGIN(T)                         \
		  template<> const CTypeInfo &TypeInfo(const T*) { \
		    static CEnumDef::SElem Elems[] = {             \

		#define ENUM_INFO_END(T)                                   \
		  };                                                       \
		  typedef TIntType<sizeof(T)>::TType TInt;                 \
		  static CEnumInfo<TInt> Info( # T, CRY_ARRAY_VAR(Elems)); \
		  return Info;                                             \
		  }                                                        \

		#define ENUM_ELEM_INFO(Scope, Elem) \
		  { static_cast<CEnumDef::TValue>(Scope Elem), # Elem },

	#else // ENABLE_TYPE_INFO_NAMES

// Enums represented as simple types, with no elements or string conversion.
		#define ENUM_INFO_BEGIN(T) \
		  TYPE_INFO_PLAIN(T)       \

		#define ENUM_INFO_END(T)
		#define ENUM_ELEM_INFO(Scope, Elem)

	#endif // ENABLE_TYPE_INFO_NAMES

#endif // ENABLE_TYPE_INFO

#endif // TYPE_INFO_IMPL_H
