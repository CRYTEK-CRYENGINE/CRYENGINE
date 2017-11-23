// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <CryExtension/TypeList.h>

namespace Cry {

namespace SchematycEd {

//////////////////////////////////////////////////////////////////////////
using TStringVector = std::vector<string>;
void GetSubFoldersAndFileNames(const char* szFolderName, const char* szExtension, bool bIgnorePakFiles, TStringVector& subFolderNames, TStringVector& fileNames);

//////////////////////////////////////////////////////////////////////////
#define TYPELIST_1( T0)                                           TL::Typelist<T0>
#define TYPELIST_2( T0, T1)                                       TL::Typelist<T1,  TYPELIST_1( T0)>
#define TYPELIST_3( T0, T1, T2)                                   TL::Typelist<T2,  TYPELIST_2( T0, T1)>
#define TYPELIST_4( T0, T1, T2, T3)                               TL::Typelist<T3,  TYPELIST_3( T0, T1, T2)>
#define TYPELIST_5( T0, T1, T2, T3, T4)                           TL::Typelist<T4,  TYPELIST_4( T0, T1, T2, T3)>
#define TYPELIST_6( T0, T1, T2, T3, T4, T5)                       TL::Typelist<T5,  TYPELIST_5( T0, T1, T2, T3, T4)>
#define TYPELIST_7( T0, T1, T2, T3, T4, T5, T6)                   TL::Typelist<T6,  TYPELIST_6( T0, T1, T2, T3, T4, T5)>
#define TYPELIST_8( T0, T1, T2, T3, T4, T5, T6, T7)               TL::Typelist<T7,  TYPELIST_7( T0, T1, T2, T3, T4, T5, T6)>
#define TYPELIST_9( T0, T1, T2, T3, T4, T5, T6, T7, T8)           TL::Typelist<T8,  TYPELIST_8( T0, T1, T2, T3, T4, T5, T6, T7)>
#define TYPELIST_10(T0, T1, T2, T3, T4, T5, T6, T7, T8, T9)       TL::Typelist<T9,  TYPELIST_9( T0, T1, T2, T3, T4, T5, T6, T7, T8)>
#define TYPELIST_11(T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10)  TL::Typelist<T10, TYPELIST_10(T0, T1, T2, T3, T4, T5, T6, T7, T8, T9)>

//////////////////////////////////////////////////////////////////////////
template <class T, template <class> class Leaf>
struct CHierarchy;

//////////////////////////////////////////////////////////////////////////
template <template <class> class Leaf>
struct CHierarchy<TL::NullType, Leaf>
{
	template <typename V> void Visit(const V& v) {}
};

//////////////////////////////////////////////////////////////////////////
template <class T, template <class> class Leaf>
struct CHierarchy : public Leaf<T>
{
	template <typename V> void Visit(const V& v) { Leaf<T>::Visit(v); }
};

//////////////////////////////////////////////////////////////////////////
template < class T0, class T1, template <class> class Leaf>
struct CHierarchy<TL::Typelist<T0, T1>, Leaf> : public CHierarchy<T0, Leaf>, public CHierarchy<T1, Leaf>
{
	template <typename V> void Visit(const V& v)
	{
		CHierarchy<T0, Leaf>::Visit(v);
		CHierarchy<T1, Leaf>::Visit(v);
	}
};



} // namespace SchematycEd

} // namespace Cry