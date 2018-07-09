// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __TYPELIST_H__
#define __TYPELIST_H__

#pragma once

namespace NTypelist
{

class CEnd
{
};

template<class H, class T>
class CNode
{
public:
	typedef H THead;
	typedef T TTail;
};

template<class T0 = CEnd, class T1 = CEnd, class T2 = CEnd, class T3 = CEnd, class T4 = CEnd, class T5 = CEnd, class T6 = CEnd, class T7 = CEnd, class T8 = CEnd, class T9 = CEnd, class T10 = CEnd, class T11 = CEnd, class T12 = CEnd, class T13 = CEnd, class T14 = CEnd, class T15 = CEnd, class T16 = CEnd, class T17 = CEnd, class T18 = CEnd, class T19 = CEnd, class T20 = CEnd, class T21 = CEnd, class T22 = CEnd, class T23 = CEnd, class T24 = CEnd, class T25 = CEnd>
class CConstruct;

template<>
class CConstruct<CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CEnd TType;
};

template<class T0>
class CConstruct<T0, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<>::TType> TType;
};

template<class T0, class T1>
class CConstruct<T0, T1, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1>::TType> TType;
};

template<class T0, class T1, class T2>
class CConstruct<T0, T1, T2, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2>::TType> TType;
};

template<class T0, class T1, class T2, class T3>
class CConstruct<T0, T1, T2, T3, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3>::TType> TType;
};

template<class T0, class T1, class T2, class T3, class T4>
class CConstruct<T0, T1, T2, T3, T4, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3, T4>::TType> TType;
};

template<class T0, class T1, class T2, class T3, class T4, class T5>
class CConstruct<T0, T1, T2, T3, T4, T5, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3, T4, T5>::TType> TType;
};

template<class T0, class T1, class T2, class T3, class T4, class T5, class T6>
class CConstruct<T0, T1, T2, T3, T4, T5, T6, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3, T4, T5, T6>::TType> TType;
};

template<class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7>
class CConstruct<T0, T1, T2, T3, T4, T5, T6, T7, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3, T4, T5, T6, T7>::TType> TType;
};

template<class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
class CConstruct<T0, T1, T2, T3, T4, T5, T6, T7, T8, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3, T4, T5, T6, T7, T8>::TType> TType;
};

template<class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
class CConstruct<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3, T4, T5, T6, T7, T8, T9>::TType> TType;
};

template<class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
class CConstruct<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>::TType> TType;
};

template<class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11>
class CConstruct<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11>::TType> TType;
};

template<class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12>
class CConstruct<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12>::TType> TType;
};

template<class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13>
class CConstruct<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13>::TType> TType;
};

template<class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13, class T14>
class CConstruct<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14>::TType> TType;
};

template<class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13, class T14, class T15>
class CConstruct<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15>::TType> TType;
};

template<class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13, class T14, class T15, class T16>
class CConstruct<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16>::TType> TType;
};

template<class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13, class T14, class T15, class T16, class T17>
class CConstruct<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17>::TType> TType;
};

template<class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13, class T14, class T15, class T16, class T17, class T18>
class CConstruct<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, CEnd, CEnd, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18>::TType> TType;
};

template<class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13, class T14, class T15, class T16, class T17, class T18, class T19>
class CConstruct<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, CEnd, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19>::TType> TType;
};

template<class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13, class T14, class T15, class T16, class T17, class T18, class T19, class T20>
class CConstruct<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, CEnd, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20>::TType> TType;
};

template<class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13, class T14, class T15, class T16, class T17, class T18, class T19, class T20, class T21>
class CConstruct<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, CEnd, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21>::TType> TType;
};

template<class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13, class T14, class T15, class T16, class T17, class T18, class T19, class T20, class T21, class T22>
class CConstruct<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, CEnd, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22>::TType> TType;
};

template<class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13, class T14, class T15, class T16, class T17, class T18, class T19, class T20, class T21, class T22, class T23>
class CConstruct<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, CEnd>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23>::TType> TType;
};

template<class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13, class T14, class T15, class T16, class T17, class T18, class T19, class T20, class T21, class T22, class T23, class T24>
class CConstruct<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24>
{
public:
	typedef CNode<T0, typename CConstruct<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24>::TType> TType;
};

}

#define __INCLUDING_FROM_TYPELIST_H__
#include "TypelistUtils.h"
#undef __INCLUDING_FROM_TYPELIST_H__

#endif // __TYPELIST_H__
