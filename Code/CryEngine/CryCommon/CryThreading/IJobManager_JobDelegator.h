// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   IJobManager_JobDelegator.h
//  Version:     v1.00
//  Created:     11/3/2011 by Christopher Bolte
//  Compilers:   Visual Studio.NET
//  Description: Interface for the DECLARE_JOB macro used to generate job delegators for the JobManager
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef IJOBMANAGER_JOBDELEGATOR_H_
#define IJOBMANAGER_JOBDELEGATOR_H_

#include <CryThreading/IJobManager.h>

//! Macro to assit generating the required functions for the JobDelegator.
//! Was split into it's own header since this code is very template heavy.
#define DECLARE_JOB(name, type, function)                                                       \
  PREFAST_SUPPRESS_WARNING(4345)                                                                \
  namespace JobManager { namespace Detail { JobManager::TJobHandle gJobHandle ## type = NULL; } \
  }                                                                                             \
  DECLARE_JOB_INVOKER_GENERATOR(type, function)                                                 \
  DECLARE_JOB_INVOKER_CLASS(name, type, function)

//! Generic Job class.
//! Job classes use a combination of macros/templates and function overloads to find out the
//! types of all parameters, and to also verify correct usage of the job structure.
namespace JobManager {
namespace Detail {
//! Template utility to help to handle this ptr while keeping the correct layout.
struct SNullType {};

template<typename ClassType>
struct SClassArg;

template<typename ClassType>
struct SClassArg
{
	typedef ClassType* TClassTypePtr;
};

template<>
struct SClassArg<SNullType>
{
	typedef SNullType TClassTypePtr;
};

//! Template helper to find out of a function pointer is a member function pointer or a free function pointer.
//! The FUNCTION_PTR_TYPE will yield an object of the type FreeFunctionPtrTrait or MemberFunctionPtrTrait which
//! can be used to select an overloaded function at compile time.
struct _True
{
	char m[2];
};
struct _False {};

template<int Value>
struct IntType
{
	enum { value = Value };
};

typedef IntType<0> FreeFunctionPtrTrait;
typedef IntType<1> MemberFunctionPtrTrait;

//! Default overload which yields a free function pointer trait.
_False is_member_function(...);

//! Overloads which yield a member function ptr trait.
template<typename C>
_True is_member_function(void (C::*)());
template<typename C, typename T0>
_True is_member_function(void (C::*)(T0));
template<typename C, typename T0, typename T1>
_True is_member_function(void (C::*)(T0, T1));
template<typename C, typename T0, typename T1, typename T2>
_True is_member_function(void (C::*)(T0, T1, T2));
template<typename C, typename T0, typename T1, typename T2, typename T3>
_True is_member_function(void (C::*)(T0, T1, T2, T3));
template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4>
_True is_member_function(void (C::*)(T0, T1, T2, T3, T4));
template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
_True is_member_function(void (C::*)(T0, T1, T2, T3, T4, T5));
template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
_True is_member_function(void (C::*)(T0, T1, T2, T3, T4, T5, T6));
template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
_True is_member_function(void (C::*)(T0, T1, T2, T3, T4, T5, T6, T7));
template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
_True is_member_function(void (C::*)(T0, T1, T2, T3, T4, T5, T6, T7, T8));
template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
_True is_member_function(void (C::*)(T0, T1, T2, T3, T4, T5, T6, T7, T8, T9));
template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>
_True is_member_function(void (C::*)(T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10));
template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10, typename T11>
_True is_member_function(void (C::*)(T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11));

//! Macro to pass the function and create the right object.
#define FUNCTION_PTR_TYPE(function) IntType<sizeof(_True) == sizeof(is_member_function(& function))>()

//! Template Helper to cast the serialized parameters back to their correct types.
//! One variation for free-functions and a partial specialization for member function pointer.
template<typename T>
struct CRY_ALIGN(16) SParams0
{
	typename SClassArg<T>::TClassTypePtr _pthis;
};

template<>
struct CRY_ALIGN(16) SParams0<SNullType>
{
};

template<typename T, typename T0>
struct CRY_ALIGN(16) SParams1
{
	typename SClassArg<T>::TClassTypePtr _pthis;
	T0 t0;
	SParams1(T0 t0_) : t0(t0_)
	{
	}
};

template<typename T0>
struct CRY_ALIGN(16) SParams1<SNullType, T0>
{
	T0 t0;
	SParams1(T0 t0_) : t0(t0_)
	{
	}
};

template<typename T, typename T0, typename T1>
struct CRY_ALIGN(16) SParams2
{
	typename SClassArg<T>::TClassTypePtr _pthis;
	T0 t0;
	T1 t1;
	SParams2(T0 t0_, T1 t1_) : t0(t0_), t1(t1_)
	{
	}
};

template<typename T0, typename T1>
struct CRY_ALIGN(16) SParams2<SNullType, T0, T1>
{
	T0 t0;
	T1 t1;
	SParams2(T0 t0_, T1 t1_) : t0(t0_), t1(t1_)
	{
	}
};

template<typename T, typename T0, typename T1, typename T2>
struct CRY_ALIGN(16) SParams3
{
	typename SClassArg<T>::TClassTypePtr _pthis;
	T0 t0;
	T1 t1;
	T2 t2;
	SParams3(T0 t0_, T1 t1_, T2 t2_) : t0(t0_), t1(t1_), t2(t2_)
	{
	}
};

template<typename T0, typename T1, typename T2>
struct CRY_ALIGN(16) SParams3<SNullType, T0, T1, T2>
{
	T0 t0;
	T1 t1;
	T2 t2;
	SParams3(T0 t0_, T1 t1_, T2 t2_) : t0(t0_), t1(t1_), t2(t2_)
	{
	}
};

template<typename T, typename T0, typename T1, typename T2, typename T3>
struct CRY_ALIGN(16) SParams4
{
	typename SClassArg<T>::TClassTypePtr _pthis;
	T0 t0;
	T1 t1;
	T2 t2;
	T3 t3;
	SParams4(T0 t0_, T1 t1_, T2 t2_, T3 t3_) : t0(t0_), t1(t1_), t2(t2_), t3(t3_)
	{
	}
};

template<typename T0, typename T1, typename T2, typename T3>
struct CRY_ALIGN(16) SParams4<SNullType, T0, T1, T2, T3>
{
	T0 t0;
	T1 t1;
	T2 t2;
	T3 t3;
	SParams4(T0 t0_, T1 t1_, T2 t2_, T3 t3_) : t0(t0_), t1(t1_), t2(t2_), t3(t3_)
	{
	}
};

template<typename T, typename T0, typename T1, typename T2, typename T3, typename T4>
struct CRY_ALIGN(16) SParams5
{
	typename SClassArg<T>::TClassTypePtr _pthis;
	T0 t0;
	T1 t1;
	T2 t2;
	T3 t3;
	T4 t4;
	SParams5(T0 t0_, T1 t1_, T2 t2_, T3 t3_, T4 t4_) : t0(t0_), t1(t1_), t2(t2_), t3(t3_), t4(t4_)
	{
	}
};

template<typename T0, typename T1, typename T2, typename T3, typename T4>
struct CRY_ALIGN(16) SParams5<SNullType, T0, T1, T2, T3, T4>
{
	T0 t0;
	T1 t1;
	T2 t2;
	T3 t3;
	T4 t4;
	SParams5(T0 t0_, T1 t1_, T2 t2_, T3 t3_, T4 t4_) : t0(t0_), t1(t1_), t2(t2_), t3(t3_), t4(t4_)
	{
	}
};

template<typename T, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
struct CRY_ALIGN(16) SParams6
{
	typename SClassArg<T>::TClassTypePtr _pthis;
	T0 t0;
	T1 t1;
	T2 t2;
	T3 t3;
	T4 t4;
	T5 t5;
	SParams6(T0 t0_, T1 t1_, T2 t2_, T3 t3_, T4 t4_, T5 t5_) : t0(t0_), t1(t1_), t2(t2_), t3(t3_), t4(t4_), t5(t5_)
	{
	}
};

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
struct CRY_ALIGN(16) SParams6<SNullType, T0, T1, T2, T3, T4, T5>
{
	T0 t0;
	T1 t1;
	T2 t2;
	T3 t3;
	T4 t4;
	T5 t5;
	SParams6(T0 t0_, T1 t1_, T2 t2_, T3 t3_, T4 t4_, T5 t5_) : t0(t0_), t1(t1_), t2(t2_), t3(t3_), t4(t4_), t5(t5_)
	{
	}
};

template<typename T, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
struct CRY_ALIGN(16) SParams7
{
	typename SClassArg<T>::TClassTypePtr _pthis;
	T0 t0;
	T1 t1;
	T2 t2;
	T3 t3;
	T4 t4;
	T5 t5;
	T6 t6;
	SParams7(T0 t0_, T1 t1_, T2 t2_, T3 t3_, T4 t4_, T5 t5_, T6 t6_) : t0(t0_), t1(t1_), t2(t2_), t3(t3_), t4(t4_), t5(t5_), t6(t6_)
	{
	}
};

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
struct CRY_ALIGN(16) SParams7<SNullType, T0, T1, T2, T3, T4, T5, T6>
{
	T0 t0;
	T1 t1;
	T2 t2;
	T3 t3;
	T4 t4;
	T5 t5;
	T6 t6;
	SParams7(T0 t0_, T1 t1_, T2 t2_, T3 t3_, T4 t4_, T5 t5_, T6 t6_) : t0(t0_), t1(t1_), t2(t2_), t3(t3_), t4(t4_), t5(t5_), t6(t6_)
	{
	}
};

template<typename T, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
struct CRY_ALIGN(16) SParams8
{
	typename SClassArg<T>::TClassTypePtr _pthis;
	T0 t0;
	T1 t1;
	T2 t2;
	T3 t3;
	T4 t4;
	T5 t5;
	T6 t6;
	T7 t7;
	SParams8(T0 t0_, T1 t1_, T2 t2_, T3 t3_, T4 t4_, T5 t5_, T6 t6_, T7 t7_) :
		t0(t0_), t1(t1_), t2(t2_), t3(t3_), t4(t4_), t5(t5_), t6(t6_), t7(t7_)
	{
	}
};

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
struct CRY_ALIGN(16) SParams8<SNullType, T0, T1, T2, T3, T4, T5, T6, T7>
{
	T0 t0;
	T1 t1;
	T2 t2;
	T3 t3;
	T4 t4;
	T5 t5;
	T6 t6;
	T7 t7;
	SParams8(T0 t0_, T1 t1_, T2 t2_, T3 t3_, T4 t4_, T5 t5_, T6 t6_, T7 t7_) :
		t0(t0_), t1(t1_), t2(t2_), t3(t3_), t4(t4_), t5(t5_), t6(t6_), t7(t7_)
	{
	}
};

template<typename T, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
struct CRY_ALIGN(16) SParams9
{
	typename SClassArg<T>::TClassTypePtr _pthis;
	T0 t0;
	T1 t1;
	T2 t2;
	T3 t3;
	T4 t4;
	T5 t5;
	T6 t6;
	T7 t7;
	T8 t8;
	SParams9(T0 t0_, T1 t1_, T2 t2_, T3 t3_, T4 t4_, T5 t5_, T6 t6_, T7 t7_, T8 t8_) :
		t0(t0_), t1(t1_), t2(t2_), t3(t3_), t4(t4_), t5(t5_), t6(t6_), t7(t7_), t8(t8_)
	{
	}
};

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
struct CRY_ALIGN(16) SParams9<SNullType, T0, T1, T2, T3, T4, T5, T6, T7, T8>
{
	T0 t0;
	T1 t1;
	T2 t2;
	T3 t3;
	T4 t4;
	T5 t5;
	T6 t6;
	T7 t7;
	T8 t8;
	SParams9(T0 t0_, T1 t1_, T2 t2_, T3 t3_, T4 t4_, T5 t5_, T6 t6_, T7 t7_, T8 t8_) :
		t0(t0_), t1(t1_), t2(t2_), t3(t3_), t4(t4_), t5(t5_), t6(t6_), t7(t7_), t8(t8_)
	{
	}
};

template<typename T, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
struct CRY_ALIGN(16) SParams10
{
	typename SClassArg<T>::TClassTypePtr _pthis;
	T0 t0;
	T1 t1;
	T2 t2;
	T3 t3;
	T4 t4;
	T5 t5;
	T6 t6;
	T7 t7;
	T8 t8;
	T9 t9;
	SParams10(T0 t0_, T1 t1_, T2 t2_, T3 t3_, T4 t4_, T5 t5_, T6 t6_, T7 t7_, T8 t8_, T9 t9_) :
		t0(t0_), t1(t1_), t2(t2_), t3(t3_), t4(t4_), t5(t5_), t6(t6_), t7(t7_), t8(t8_), t9(t9_)
	{
	}
};

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
struct CRY_ALIGN(16) SParams10<SNullType, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9>
{
	T0 t0;
	T1 t1;
	T2 t2;
	T3 t3;
	T4 t4;
	T5 t5;
	T6 t6;
	T7 t7;
	T8 t8;
	T9 t9;
	SParams10(T0 t0_, T1 t1_, T2 t2_, T3 t3_, T4 t4_, T5 t5_, T6 t6_, T7 t7_, T8 t8_, T9 t9_) :
		t0(t0_), t1(t1_), t2(t2_), t3(t3_), t4(t4_), t5(t5_), t6(t6_), t7(t7_), t8(t8_), t9(t9_)
	{
	}
};

template<typename T, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>
struct CRY_ALIGN(16) SParams11
{
	typename SClassArg<T>::TClassTypePtr _pthis;
	T0 t0;
	T1 t1;
	T2 t2;
	T3 t3;
	T4 t4;
	T5 t5;
	T6 t6;
	T7 t7;
	T8 t8;
	T9 t9;
	T10 t10;
	SParams11(T0 t0_, T1 t1_, T2 t2_, T3 t3_, T4 t4_, T5 t5_, T6 t6_, T7 t7_, T8 t8_, T9 t9_, T10 t10_) :
		t0(t0_), t1(t1_), t2(t2_), t3(t3_), t4(t4_), t5(t5_), t6(t6_), t7(t7_), t8(t8_), t9(t9_), t10(t10_)
	{
	}
};

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>
struct CRY_ALIGN(16) SParams11<SNullType, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>
{
	T0 t0;
	T1 t1;
	T2 t2;
	T3 t3;
	T4 t4;
	T5 t5;
	T6 t6;
	T7 t7;
	T8 t8;
	T9 t9;
	T10 t10;
	SParams11(T0 t0_, T1 t1_, T2 t2_, T3 t3_, T4 t4_, T5 t5_, T6 t6_, T7 t7_, T8 t8_, T9 t9_, T10 t10_) :
		t0(t0_), t1(t1_), t2(t2_), t3(t3_), t4(t4_), t5(t5_), t6(t6_), t7(t7_), t8(t8_), t9(t9_), t10(t10_)
	{
	}
};

///////////////////////////////////////////////////////////////////////////////
// Template-Utils to verify the parameter types passed into the job
struct SVerifyParameter0
{};

template<typename T0>
struct SVerifyParameter1
{
	typedef T0 _t0;
};

template<typename T0, typename T1>
struct SVerifyParameter2
{
	typedef T0 _t0;
	typedef T1 _t1;
};

template<typename T0, typename T1, typename T2>
struct SVerifyParameter3
{
	typedef T0 _t0;
	typedef T1 _t1;
	typedef T2 _t2;
};

template<typename T0, typename T1, typename T2, typename T3>
struct SVerifyParameter4
{
	typedef T0 _t0;
	typedef T1 _t1;
	typedef T2 _t2;
	typedef T3 _t3;
};

template<typename T0, typename T1, typename T2, typename T3, typename T4>
struct SVerifyParameter5
{
	typedef T0 _t0;
	typedef T1 _t1;
	typedef T2 _t2;
	typedef T3 _t3;
	typedef T4 _t4;
};

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
struct SVerifyParameter6
{
	typedef T0 _t0;
	typedef T1 _t1;
	typedef T2 _t2;
	typedef T3 _t3;
	typedef T4 _t4;
	typedef T5 _t5;
};

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
struct SVerifyParameter7
{
	typedef T0 _t0;
	typedef T1 _t1;
	typedef T2 _t2;
	typedef T3 _t3;
	typedef T4 _t4;
	typedef T5 _t5;
	typedef T6 _t6;
};

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
struct SVerifyParameter8
{
	typedef T0 _t0;
	typedef T1 _t1;
	typedef T2 _t2;
	typedef T3 _t3;
	typedef T4 _t4;
	typedef T5 _t5;
	typedef T6 _t6;
	typedef T7 _t7;
};

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
struct SVerifyParameter9
{
	typedef T0 _t0;
	typedef T1 _t1;
	typedef T2 _t2;
	typedef T3 _t3;
	typedef T4 _t4;
	typedef T5 _t5;
	typedef T6 _t6;
	typedef T7 _t7;
	typedef T8 _t8;
};

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
struct SVerifyParameter10
{
	typedef T0 _t0;
	typedef T1 _t1;
	typedef T2 _t2;
	typedef T3 _t3;
	typedef T4 _t4;
	typedef T5 _t5;
	typedef T6 _t6;
	typedef T7 _t7;
	typedef T8 _t8;
	typedef T9 _t9;
};

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>
struct SVerifyParameter11
{
	typedef T0  _t0;
	typedef T1  _t1;
	typedef T2  _t2;
	typedef T3  _t3;
	typedef T4  _t4;
	typedef T5  _t5;
	typedef T6  _t6;
	typedef T7  _t7;
	typedef T8  _t8;
	typedef T9  _t9;
	typedef T10 _t10;
};

///////////////////////////////////////////////////////////////////////////////
// Template-Overload functions to verify the correct parameter are passed to a member-function pointer
template<typename C, typename P0, typename T>
void VerifyParameter_1(void (C::* func)(P0), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}

template<typename C, typename P0, typename P1, typename T>
void VerifyParameter_2(void (C::* func)(P0, P1), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P1, typename T::_t1>::value), ERROR_SECOND_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}

template<typename C, typename P0, typename P1, typename P2, typename T>
void VerifyParameter_3(void (C::* func)(P0, P1, P2), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P1, typename T::_t1>::value), ERROR_SECOND_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P2, typename T::_t2>::value), ERROR_THIRD_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}

template<typename C, typename P0, typename P1, typename P2, typename P3, typename T>
void VerifyParameter_4(void (C::* func)(P0, P1, P2, P3), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P1, typename T::_t1>::value), ERROR_SECOND_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P2, typename T::_t2>::value), ERROR_THIRD_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P3, typename T::_t3>::value), ERROR_FORTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}

template<typename C, typename P0, typename P1, typename P2, typename P3, typename P4, typename T>
void VerifyParameter_5(void (C::* func)(P0, P1, P2, P3, P4), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P1, typename T::_t1>::value), ERROR_SECOND_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P2, typename T::_t2>::value), ERROR_THIRD_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P3, typename T::_t3>::value), ERROR_FORTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P4, typename T::_t4>::value), ERROR_FIFTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}

template<typename C, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename T>
void VerifyParameter_6(void (C::* func)(P0, P1, P2, P3, P4, P5), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P1, typename T::_t1>::value), ERROR_SECOND_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P2, typename T::_t2>::value), ERROR_THIRD_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P3, typename T::_t3>::value), ERROR_FORTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P4, typename T::_t4>::value), ERROR_FIFTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P5, typename T::_t5>::value), ERROR_SIXTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}

template<typename C, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename T>
void VerifyParameter_7(void (C::* func)(P0, P1, P2, P3, P4, P5, P6), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P1, typename T::_t1>::value), ERROR_SECOND_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P2, typename T::_t2>::value), ERROR_THIRD_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P3, typename T::_t3>::value), ERROR_FORTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P4, typename T::_t4>::value), ERROR_FIFTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P5, typename T::_t5>::value), ERROR_SIXTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P6, typename T::_t6>::value), ERROR_SEVENTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}

template<typename C, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename T>
void VerifyParameter_8(void (C::* func)(P0, P1, P2, P3, P4, P5, P6, P7), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P1, typename T::_t1>::value), ERROR_SECOND_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P2, typename T::_t2>::value), ERROR_THIRD_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P3, typename T::_t3>::value), ERROR_FORTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P4, typename T::_t4>::value), ERROR_FIFTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P5, typename T::_t5>::value), ERROR_SIXTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P6, typename T::_t6>::value), ERROR_SEVENTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P7, typename T::_t7>::value), ERROR_EIGHT_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}

template<typename C, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename T>
void VerifyParameter_9(void (C::* func)(P0, P1, P2, P3, P4, P5, P6, P7, P8), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P1, typename T::_t1>::value), ERROR_SECOND_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P2, typename T::_t2>::value), ERROR_THIRD_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P3, typename T::_t3>::value), ERROR_FORTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P4, typename T::_t4>::value), ERROR_FIFTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P5, typename T::_t5>::value), ERROR_SIXTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P6, typename T::_t6>::value), ERROR_SEVENTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P7, typename T::_t7>::value), ERROR_EIGHT_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P8, typename T::_t8>::value), ERROR_NINETH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}

template<typename C, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename T>
void VerifyParameter_10(void (C::* func)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P1, typename T::_t1>::value), ERROR_SECOND_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P2, typename T::_t2>::value), ERROR_THIRD_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P3, typename T::_t3>::value), ERROR_FORTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P4, typename T::_t4>::value), ERROR_FIFTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P5, typename T::_t5>::value), ERROR_SIXTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P6, typename T::_t6>::value), ERROR_SEVENTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P7, typename T::_t7>::value), ERROR_EIGHT_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P8, typename T::_t8>::value), ERROR_NINETH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P9, typename T::_t9>::value), ERROR_TENTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}

template<typename C, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename T>
void VerifyParameter_11(void (C::* func)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P1, typename T::_t1>::value), ERROR_SECOND_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P2, typename T::_t2>::value), ERROR_THIRD_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P3, typename T::_t3>::value), ERROR_FORTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P4, typename T::_t4>::value), ERROR_FIFTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P5, typename T::_t5>::value), ERROR_SIXTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P6, typename T::_t6>::value), ERROR_SEVENTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P7, typename T::_t7>::value), ERROR_EIGHT_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P8, typename T::_t8>::value), ERROR_NINETH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P9, typename T::_t9>::value), ERROR_TENTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P10, typename T::_t10>::value), ERROR_ELEVENTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}

///////////////////////////////////////////////////////////////////////////////
// Template-Overload functions to verify the correct parameter are passed to a free-function pointer
template<typename P0, typename T>
void VerifyParameter_1(void (* func)(P0), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}

template<typename P0, typename P1, typename T>
void VerifyParameter_2(void (* func)(P0, P1), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P1, typename T::_t1>::value), ERROR_SECOND_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}

template<typename P0, typename P1, typename P2, typename T>
void VerifyParameter_3(void (* func)(P0, P1, P2), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P1, typename T::_t1>::value), ERROR_SECOND_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P2, typename T::_t2>::value), ERROR_THIRD_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}

template<typename P0, typename P1, typename P2, typename P3, typename T>
void VerifyParameter_4(void (* func)(P0, P1, P2, P3), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P1, typename T::_t1>::value), ERROR_SECOND_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P2, typename T::_t2>::value), ERROR_THIRD_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P3, typename T::_t3>::value), ERROR_FORTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}

template<typename P0, typename P1, typename P2, typename P3, typename P4, typename T>
void VerifyParameter_5(void (* func)(P0, P1, P2, P3, P4), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P1, typename T::_t1>::value), ERROR_SECOND_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P2, typename T::_t2>::value), ERROR_THIRD_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P3, typename T::_t3>::value), ERROR_FORTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P4, typename T::_t4>::value), ERROR_FIFTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}

template<typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename T>
void VerifyParameter_6(void (* func)(P0, P1, P2, P3, P4, P5), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P1, typename T::_t1>::value), ERROR_SECOND_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P2, typename T::_t2>::value), ERROR_THIRD_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P3, typename T::_t3>::value), ERROR_FORTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P4, typename T::_t4>::value), ERROR_FIFTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P5, typename T::_t5>::value), ERROR_SIXTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}

template<typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename T>
void VerifyParameter_7(void (* func)(P0, P1, P2, P3, P4, P5, P6), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P1, typename T::_t1>::value), ERROR_SECOND_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P2, typename T::_t2>::value), ERROR_THIRD_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P3, typename T::_t3>::value), ERROR_FORTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P4, typename T::_t4>::value), ERROR_FIFTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P5, typename T::_t5>::value), ERROR_SIXTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P6, typename T::_t6>::value), ERROR_SEVENTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}

template<typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename T>
void VerifyParameter_8(void (* func)(P0, P1, P2, P3, P4, P5, P6, P7), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P1, typename T::_t1>::value), ERROR_SECOND_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P2, typename T::_t2>::value), ERROR_THIRD_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P3, typename T::_t3>::value), ERROR_FORTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P4, typename T::_t4>::value), ERROR_FIFTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P5, typename T::_t5>::value), ERROR_SIXTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P6, typename T::_t6>::value), ERROR_SEVENTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P7, typename T::_t7>::value), ERROR_EIGHT_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}

template<typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename T>
void VerifyParameter_9(void (* func)(P0, P1, P2, P3, P4, P5, P6, P7, P8), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P1, typename T::_t1>::value), ERROR_SECOND_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P2, typename T::_t2>::value), ERROR_THIRD_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P3, typename T::_t3>::value), ERROR_FORTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P4, typename T::_t4>::value), ERROR_FIFTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P5, typename T::_t5>::value), ERROR_SIXTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P6, typename T::_t6>::value), ERROR_SEVENTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P7, typename T::_t7>::value), ERROR_EIGHT_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P8, typename T::_t8>::value), ERROR_NINETH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}

template<typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename T>
void VerifyParameter_10(void (* func)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P1, typename T::_t1>::value), ERROR_SECOND_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P2, typename T::_t2>::value), ERROR_THIRD_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P3, typename T::_t3>::value), ERROR_FORTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P4, typename T::_t4>::value), ERROR_FIFTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P5, typename T::_t5>::value), ERROR_SIXTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P6, typename T::_t6>::value), ERROR_SEVENTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P7, typename T::_t7>::value), ERROR_EIGHT_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P8, typename T::_t8>::value), ERROR_NINETH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P9, typename T::_t9>::value), ERROR_TENTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}
template<typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename T>
void VerifyParameter_11(void (* func)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10), const T&)
{
	STATIC_CHECK((std::is_convertible<P0, typename T::_t0>::value), ERROR_FIRST_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P1, typename T::_t1>::value), ERROR_SECOND_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P2, typename T::_t2>::value), ERROR_THIRD_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P3, typename T::_t3>::value), ERROR_FORTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P4, typename T::_t4>::value), ERROR_FIFTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P5, typename T::_t5>::value), ERROR_SIXTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P6, typename T::_t6>::value), ERROR_SEVENTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P7, typename T::_t7>::value), ERROR_EIGHT_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P8, typename T::_t8>::value), ERROR_NINETH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P9, typename T::_t9>::value), ERROR_TENTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
	STATIC_CHECK((std::is_convertible<P10, typename T::_t10>::value), ERROR_ELEVENTH_PARAMETER_IS_OF_A_DIFFERENT_TYPE_FOR_JOBMANAGER_FUNCTION);
}

//! Template-Utility to verify the this pointer is correct.
template<typename T>
struct SVerifyThisPtr
{
	typedef T thisT;
};

// Using c++11 variadic templates

///////////////////////////////////////////////////////////////////////////////
// Template-Overload functions to verify the correct this pointer for a member functions pointer
template<typename C, typename ... Params, typename T>
void VerifyThisPtr(void (C::* func)(Params ...), const T&)
{
	static_assert(std::is_same<C, typename T::thisT>::value, "THIS_POINTER_IN_JOB_HAS_WRONG_TYPE");
}

///////////////////////////////////////////////////////////////////////////////
// Template-Overload functions to verify that no this pointer is passed to a free function
template<typename ... Params, typename T>
void VerifyThisPtr(void (* func)(Params ...), const T&)
{
	static_assert(std::is_same<SNullType, typename T::thisT>::value, "PASSING_A_THIS_POINTER_TO_A_FREE_FUNCTION_IS_NOT_SUPPORTED");
}

} // namespace Detail
} // namespace JobManager

//! Macro to generate all the overloads to generate the Invoker Function is responsible to de-serialize the parameters passed into a job.
#define DECLARE_JOB_INVOKER_GENERATOR(type, function)                                                                                                                                                                                                                                                                                \
  namespace JobManager {                                                                                                                                                                                                                                                                                                             \
  namespace Detail {                                                                                                                                                                                                                                                                                                                 \
  /* Forward declaration for partial-specialization */                                                                                                                                                                                                                                                                               \
  template<class T, typename P0 = SNullType, typename P1 = SNullType, typename P2 = SNullType, typename P3 = SNullType, typename P4 = SNullType, typename P5 = SNullType, typename P6 = SNullType, typename P7 = SNullType, typename P8 = SNullType, typename P9 = SNullType, typename P10 = SNullType, typename PDUMMY = SNullType> \
  struct SGenInvoker ## type;                                                                                                                                                                                                                                                                                                        \
                                                                                                                                                                                                                                                                                                                                     \
  template<>                                                                                                                                                                                                                                                                                                                         \
  struct SGenInvoker ## type<SNullType>                                                                                                                                                                                                                                                                                              \
  {                                                                                                                                                                                                                                                                                                                                  \
    /* To prevent compile errors with strict-standard confirming compilers, which check during the first template pass */                                                                                                                                                                                                            \
    /* we cannot call free functions provided by the macro directly, so we add a abstraction */                                                                                                                                                                                                                                      \
    /* layer of function overloads, and only the for this invoker correct overload does an actual function call */                                                                                                                                                                                                                   \
    /* We provide one template overload for the actual parameter number we want to be able to call*/                                                                                                                                                                                                                                 \
    /* and one elipse function to catch all unwanted versions (never called, but possible calles are seen during compilation*/                                                                                                                                                                                                       \
    static void CallFunction(...) { /*Catch em all - do nothing*/ }                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                     \
    /* call the correct version */                                                                                                                                                                                                                                                                                                   \
    static void CallFunction(void (* pFunc)()) { (*pFunc)(); }                                                                                                                                                                                                                                                                       \
                                                                                                                                                                                                                                                                                                                                     \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams0<SNullType>* params = reinterpret_cast<SParams0<SNullType>*>(p);                                                                                                                                                                                                                                                       \
      CallFunction(&function);                                                                                                                                                                                                                                                                                                       \
      params->~SParams0();                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename T0>                                                                                                                                                                                                                                                                                                              \
  struct SGenInvoker ## type<SNullType, T0>                                                                                                                                                                                                                                                                                          \
  {                                                                                                                                                                                                                                                                                                                                  \
    /* To prevent compile errors with strict-standard confirming compilers, which check during the first template pass */                                                                                                                                                                                                            \
    /* we cannot call free functions provided by the macro directly, so we add a abstraction */                                                                                                                                                                                                                                      \
    /* layer of function overloads, and only the for this invoker correct overload does an actual function call */                                                                                                                                                                                                                   \
    /* We provide one template overload for the actual parameter number we want to be able to call*/                                                                                                                                                                                                                                 \
    /* and one elipse function to catch all unwanted versions (never called, but possible calles are seen during compilation*/                                                                                                                                                                                                       \
    static void CallFunction(...) { /*Catch em all - do nothing*/ }                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                     \
    /* call the correct version */                                                                                                                                                                                                                                                                                                   \
    template<typename TT0>                                                                                                                                                                                                                                                                                                           \
    static void CallFunction(void (* pFunc)(TT0), TT0 t0) { (*pFunc)(t0); }                                                                                                                                                                                                                                                          \
                                                                                                                                                                                                                                                                                                                                     \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams1<SNullType, T0>* params = reinterpret_cast<SParams1<SNullType, T0>*>(p);                                                                                                                                                                                                                                               \
      CallFunction(&function, params->t0);                                                                                                                                                                                                                                                                                           \
      params->~SParams1();                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename T0, typename T1>                                                                                                                                                                                                                                                                                                 \
  struct SGenInvoker ## type<SNullType, T0, T1>                                                                                                                                                                                                                                                                                      \
  {                                                                                                                                                                                                                                                                                                                                  \
    /* To prevent compile errors with strict-standard confirming compilers, which check during the first template pass */                                                                                                                                                                                                            \
    /* we cannot call free functions provided by the macro directly, so we add a abstraction */                                                                                                                                                                                                                                      \
    /* layer of function overloads, and only the for this invoker correct overload does an actual function call */                                                                                                                                                                                                                   \
    /* We provide one template overload for the actual parameter number we want to be able to call*/                                                                                                                                                                                                                                 \
    /* and one elipse function to catch all unwanted versions (never called, but possible calles are seen during compilation*/                                                                                                                                                                                                       \
    static void CallFunction(...) { /*Catch em all - do nothing*/ }                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                     \
    /* call the correct version */                                                                                                                                                                                                                                                                                                   \
    template<typename TT0, typename TT1>                                                                                                                                                                                                                                                                                             \
    static void CallFunction(void (* pFunc)(TT0, TT1), TT0 t0, TT1 t1) { (*pFunc)(t0, t1); }                                                                                                                                                                                                                                         \
                                                                                                                                                                                                                                                                                                                                     \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams2<SNullType, T0, T1>* params = reinterpret_cast<SParams2<SNullType, T0, T1>*>(p);                                                                                                                                                                                                                                       \
      CallFunction(&function, params->t0, params->t1);                                                                                                                                                                                                                                                                               \
      params->~SParams2();                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename T0, typename T1, typename T2>                                                                                                                                                                                                                                                                                    \
  struct SGenInvoker ## type<SNullType, T0, T1, T2>                                                                                                                                                                                                                                                                                  \
  {                                                                                                                                                                                                                                                                                                                                  \
    /* To prevent compile errors with strict-standard confirming compilers, which check during the first template pass */                                                                                                                                                                                                            \
    /* we cannot call free functions provided by the macro directly, so we add a abstraction */                                                                                                                                                                                                                                      \
    /* layer of function overloads, and only the for this invoker correct overload does an actual function call */                                                                                                                                                                                                                   \
    /* We provide one template overload for the actual parameter number we want to be able to call*/                                                                                                                                                                                                                                 \
    /* and one elipse function to catch all unwanted versions (never called, but possible calles are seen during compilation*/                                                                                                                                                                                                       \
    static void CallFunction(...) { /*Catch em all - do nothing*/ }                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                     \
    /* call the correct version */                                                                                                                                                                                                                                                                                                   \
    template<typename TT0, typename TT1, typename TT2>                                                                                                                                                                                                                                                                               \
    static void CallFunction(void (* pFunc)(TT0, TT1, TT2), TT0 t0, TT1 t1, TT2 t2) { (*pFunc)(t0, t1, t2); }                                                                                                                                                                                                                        \
                                                                                                                                                                                                                                                                                                                                     \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams3<SNullType, T0, T1, T2>* params = reinterpret_cast<SParams3<SNullType, T0, T1, T2>*>(p);                                                                                                                                                                                                                               \
      CallFunction(&function, params->t0, params->t1, params->t2);                                                                                                                                                                                                                                                                   \
      params->~SParams3();                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename T0, typename T1, typename T2, typename T3>                                                                                                                                                                                                                                                                       \
  struct SGenInvoker ## type<SNullType, T0, T1, T2, T3>                                                                                                                                                                                                                                                                              \
  {                                                                                                                                                                                                                                                                                                                                  \
    /* To prevent compile errors with strict-standard confirming compilers, which check during the first template pass */                                                                                                                                                                                                            \
    /* we cannot call free functions provided by the macro directly, so we add a abstraction */                                                                                                                                                                                                                                      \
    /* layer of function overloads, and only the for this invoker correct overload does an actual function call */                                                                                                                                                                                                                   \
    /* We provide one template overload for the actual parameter number we want to be able to call*/                                                                                                                                                                                                                                 \
    /* and one elipse function to catch all unwanted versions (never called, but possible calles are seen during compilation*/                                                                                                                                                                                                       \
    static void CallFunction(...) { /*Catch em all - do nothing*/ }                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                     \
    /* call the correct version */                                                                                                                                                                                                                                                                                                   \
    template<typename TT0, typename TT1, typename TT2, typename TT3>                                                                                                                                                                                                                                                                 \
    static void CallFunction(void (* pFunc)(TT0, TT1, TT2, TT3), TT0 t0, TT1 t1, TT2 t2, TT3 t3) { (*pFunc)(t0, t1, t2, t3); }                                                                                                                                                                                                       \
                                                                                                                                                                                                                                                                                                                                     \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams4<SNullType, T0, T1, T2, T3>* params = reinterpret_cast<SParams4<SNullType, T0, T1, T2, T3>*>(p);                                                                                                                                                                                                                       \
      CallFunction(&function, params->t0, params->t1, params->t2, params->t3);                                                                                                                                                                                                                                                       \
      params->~SParams4();                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename T0, typename T1, typename T2, typename T3, typename T4>                                                                                                                                                                                                                                                          \
  struct SGenInvoker ## type<SNullType, T0, T1, T2, T3, T4>                                                                                                                                                                                                                                                                          \
  {                                                                                                                                                                                                                                                                                                                                  \
    /* To prevent compile errors with strict-standard confirming compilers, which check during the first template pass */                                                                                                                                                                                                            \
    /* we cannot call free functions provided by the macro directly, so we add a abstraction */                                                                                                                                                                                                                                      \
    /* layer of function overloads, and only the for this invoker correct overload does an actual function call */                                                                                                                                                                                                                   \
    /* We provide one template overload for the actual parameter number we want to be able to call*/                                                                                                                                                                                                                                 \
    /* and one elipse function to catch all unwanted versions (never called, but possible calles are seen during compilation*/                                                                                                                                                                                                       \
    static void CallFunction(...) { /*Catch em all - do nothing*/ }                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                     \
    /* call the correct version */                                                                                                                                                                                                                                                                                                   \
    template<typename TT0, typename TT1, typename TT2, typename TT3, typename TT4>                                                                                                                                                                                                                                                   \
    static void CallFunction(void (* pFunc)(TT0, TT1, TT2, TT3, TT4), TT0 t0, TT1 t1, TT2 t2, TT3 t3, TT4 t4) { (*pFunc)(t0, t1, t2, t3, t4); }                                                                                                                                                                                      \
                                                                                                                                                                                                                                                                                                                                     \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams5<SNullType, T0, T1, T2, T3, T4>* params = reinterpret_cast<SParams5<SNullType, T0, T1, T2, T3, T4>*>(p);                                                                                                                                                                                                               \
      CallFunction(&function, params->t0, params->t1, params->t2, params->t3, params->t4);                                                                                                                                                                                                                                           \
      params->~SParams5();                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>                                                                                                                                                                                                                                             \
  struct SGenInvoker ## type<SNullType, T0, T1, T2, T3, T4, T5>                                                                                                                                                                                                                                                                      \
  {                                                                                                                                                                                                                                                                                                                                  \
    /* To prevent compile errors with strict-standard confirming compilers, which check during the first template pass */                                                                                                                                                                                                            \
    /* we cannot call free functions provided by the macro directly, so we add a abstraction */                                                                                                                                                                                                                                      \
    /* layer of function overloads, and only the for this invoker correct overload does an actual function call */                                                                                                                                                                                                                   \
    /* We provide one template overload for the actual parameter number we want to be able to call*/                                                                                                                                                                                                                                 \
    /* and one elipse function to catch all unwanted versions (never called, but possible calles are seen during compilation*/                                                                                                                                                                                                       \
    static void CallFunction(...) { /*Catch em all - do nothing*/ }                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                     \
    /* call the correct version */                                                                                                                                                                                                                                                                                                   \
    template<typename TT0, typename TT1, typename TT2, typename TT3, typename TT4, typename TT5>                                                                                                                                                                                                                                     \
    static void CallFunction(void (* pFunc)(TT0, TT1, TT2, TT3, TT4, TT5), TT0 t0, TT1 t1, TT2 t2, TT3 t3, TT4 t4, TT5 t5) { (*pFunc)(t0, t1, t2, t3, t4, t5); }                                                                                                                                                                     \
                                                                                                                                                                                                                                                                                                                                     \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams6<SNullType, T0, T1, T2, T3, T4, T5>* params = reinterpret_cast<SParams6<SNullType, T0, T1, T2, T3, T4, T5>*>(p);                                                                                                                                                                                                       \
      CallFunction(&function, params->t0, params->t1, params->t2, params->t3, params->t4, params->t5);                                                                                                                                                                                                                               \
      params->~SParams6();                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>                                                                                                                                                                                                                                \
  struct SGenInvoker ## type<SNullType, T0, T1, T2, T3, T4, T5, T6>                                                                                                                                                                                                                                                                  \
  {                                                                                                                                                                                                                                                                                                                                  \
    /* To prevent compile errors with strict-standard confirming compilers, which check during the first template pass */                                                                                                                                                                                                            \
    /* we cannot call free functions provided by the macro directly, so we add a abstraction */                                                                                                                                                                                                                                      \
    /* layer of function overloads, and only the for this invoker correct overload does an actual function call */                                                                                                                                                                                                                   \
    /* We provide one template overload for the actual parameter number we want to be able to call*/                                                                                                                                                                                                                                 \
    /* and one elipse function to catch all unwanted versions (never called, but possible calles are seen during compilation*/                                                                                                                                                                                                       \
    static void CallFunction(...) { /*Catch em all - do nothing*/ }                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                     \
    /* call the correct version */                                                                                                                                                                                                                                                                                                   \
    template<typename TT0, typename TT1, typename TT2, typename TT3, typename TT4, typename TT5, typename TT6>                                                                                                                                                                                                                       \
    static void CallFunction(void (* pFunc)(TT0, TT1, TT2, TT3, TT4, TT5, TT6), TT0 t0, TT1 t1, TT2 t2, TT3 t3, TT4 t4, TT5 t5, TT6 t6) { (*pFunc)(t0, t1, t2, t3, t4, t5, t6); }                                                                                                                                                    \
                                                                                                                                                                                                                                                                                                                                     \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams7<SNullType, T0, T1, T2, T3, T4, T5, T6>* params = reinterpret_cast<SParams7<SNullType, T0, T1, T2, T3, T4, T5, T6>*>(p);                                                                                                                                                                                               \
      CallFunction(&function, params->t0, params->t1, params->t2, params->t3, params->t4, params->t5, params->t6);                                                                                                                                                                                                                   \
      params->~SParams7();                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>                                                                                                                                                                                                                   \
  struct SGenInvoker ## type<SNullType, T0, T1, T2, T3, T4, T5, T6, T7>                                                                                                                                                                                                                                                              \
  {                                                                                                                                                                                                                                                                                                                                  \
    /* To prevent compile errors with strict-standard confirming compilers, which check during the first template pass */                                                                                                                                                                                                            \
    /* we cannot call free functions provided by the macro directly, so we add a abstraction */                                                                                                                                                                                                                                      \
    /* layer of function overloads, and only the for this invoker correct overload does an actual function call */                                                                                                                                                                                                                   \
    /* We provide one template overload for the actual parameter number we want to be able to call*/                                                                                                                                                                                                                                 \
    /* and one elipse function to catch all unwanted versions (never called, but possible calles are seen during compilation*/                                                                                                                                                                                                       \
    static void CallFunction(...) { /*Catch em all - do nothing*/ }                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                     \
    /* call the correct version */                                                                                                                                                                                                                                                                                                   \
    template<typename TT0, typename TT1, typename TT2, typename TT3, typename TT4, typename TT5, typename TT6, typename TT7>                                                                                                                                                                                                         \
    static void CallFunction(void (* pFunc)(TT0, TT1, TT2, TT3, TT4, TT5, TT6, TT7), TT0 t0, TT1 t1, TT2 t2, TT3 t3, TT4 t4, TT5 t5, TT6 t6, TT7 t7) { (*pFunc)(t0, t1, t2, t3, t4, t5, t6, t7); }                                                                                                                                   \
                                                                                                                                                                                                                                                                                                                                     \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams8<SNullType, T0, T1, T2, T3, T4, T5, T6, T7>* params = reinterpret_cast<SParams8<SNullType, T0, T1, T2, T3, T4, T5, T6, T7>*>(p);                                                                                                                                                                                       \
      CallFunction(&function, params->t0, params->t1, params->t2, params->t3, params->t4, params->t5, params->t6, params->t7);                                                                                                                                                                                                       \
      params->~SParams8();                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>                                                                                                                                                                                                      \
  struct SGenInvoker ## type<SNullType, T0, T1, T2, T3, T4, T5, T6, T7, T8>                                                                                                                                                                                                                                                          \
  {                                                                                                                                                                                                                                                                                                                                  \
    /* To prevent compile errors with strict-standard confirming compilers, which check during the first template pass */                                                                                                                                                                                                            \
    /* we cannot call free functions provided by the macro directly, so we add a abstraction */                                                                                                                                                                                                                                      \
    /* layer of function overloads, and only the for this invoker correct overload does an actual function call */                                                                                                                                                                                                                   \
    /* We provide one template overload for the actual parameter number we want to be able to call*/                                                                                                                                                                                                                                 \
    /* and one elipse function to catch all unwanted versions (never called, but possible calles are seen during compilation*/                                                                                                                                                                                                       \
    static void CallFunction(...) { /*Catch em all - do nothing*/ }                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                     \
    /* call the correct version */                                                                                                                                                                                                                                                                                                   \
    template<typename TT0, typename TT1, typename TT2, typename TT3, typename TT4, typename TT5, typename TT6, typename TT7, typename TT8>                                                                                                                                                                                           \
    static void CallFunction(void (* pFunc)(TT0, TT1, TT2, TT3, TT4, TT5, TT6, TT7, TT8), TT0 t0, TT1 t1, TT2 t2, TT3 t3, TT4 t4, TT5 t5, TT6 t6, TT7 t7, TT8 t8) { (*pFunc)(t0, t1, t2, t3, t4, t5, t6, t7, t8); }                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                     \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams9<SNullType, T0, T1, T2, T3, T4, T5, T6, T7, T8>* params = reinterpret_cast<SParams9<SNullType, T0, T1, T2, T3, T4, T5, T6, T7, T8>*>(p);                                                                                                                                                                               \
      CallFunction(&function, params->t0, params->t1, params->t2, params->t3, params->t4, params->t5, params->t6, params->t7, params->t8);                                                                                                                                                                                           \
      params->~SParams9();                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>                                                                                                                                                                                         \
  struct SGenInvoker ## type<SNullType, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9>                                                                                                                                                                                                                                                      \
  {                                                                                                                                                                                                                                                                                                                                  \
    /* To prevent compile errors with strict-standard confirming compilers, which check during the first template pass */                                                                                                                                                                                                            \
    /* we cannot call free functions provided by the macro directly, so we add a abstraction */                                                                                                                                                                                                                                      \
    /* layer of function overloads, and only the for this invoker correct overload does an actual function call */                                                                                                                                                                                                                   \
    /* We provide one template overload for the actual parameter number we want to be able to call*/                                                                                                                                                                                                                                 \
    /* and one elipse function to catch all unwanted versions (never called, but possible calles are seen during compilation*/                                                                                                                                                                                                       \
    static void CallFunction(...) { /*Catch em all - do nothing*/ }                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                     \
    /* call the correct version */                                                                                                                                                                                                                                                                                                   \
    template<typename TT0, typename TT1, typename TT2, typename TT3, typename TT4, typename TT5, typename TT6, typename TT7, typename TT8, typename TT9>                                                                                                                                                                             \
    static void CallFunction(void (* pFunc)(TT0, TT1, TT2, TT3, TT4, TT5, TT6, TT7, TT8, TT8, TT9), TT0 t0, TT1 t1, TT2 t2, TT3 t3, TT4 t4, TT5 t5, TT6 t6, TT7 t7, TT8 t8, TT9 t9) { (*pFunc)(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9); }                                                                                            \
                                                                                                                                                                                                                                                                                                                                     \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams10<SNullType, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9>* params = reinterpret_cast<SParams10<SNullType, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9>*>(p);                                                                                                                                                                     \
      CallFunction(&function, params->t0, params->t1, params->t2, params->t3, params->t4, params->t5, params->t6, params->t7, params->t8, params->t9);                                                                                                                                                                               \
      params->~SParams10();                                                                                                                                                                                                                                                                                                          \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>                                                                                                                                                                           \
  struct SGenInvoker ## type<SNullType, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>                                                                                                                                                                                                                                                 \
  {                                                                                                                                                                                                                                                                                                                                  \
    /* To prevent compile errors with strict-standard confirming compilers, which check during the first template pass */                                                                                                                                                                                                            \
    /* we cannot call free functions provided by the macro directly, so we add a abstraction */                                                                                                                                                                                                                                      \
    /* layer of function overloads, and only the for this invoker correct overload does an actual function call */                                                                                                                                                                                                                   \
    /* We provide one template overload for the actual parameter number we want to be able to call*/                                                                                                                                                                                                                                 \
    /* and one elipse function to catch all unwanted versions (never called, but possible calles are seen during compilation*/                                                                                                                                                                                                       \
    static void CallFunction(...) { /*Catch em all - do nothing*/ }                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                     \
    /* call the correct version */                                                                                                                                                                                                                                                                                                   \
    template<typename TT0, typename TT1, typename TT2, typename TT3, typename TT4, typename TT5, typename TT6, typename TT7, typename TT8, typename TT9, typename TT10>                                                                                                                                                              \
    static void CallFunction(void (* pFunc)(TT0, TT1, TT2, TT3, TT4, TT5, TT6, TT7, TT8, TT9, TT10), TT0 t0, TT1 t1, TT2 t2, TT3 t3, TT4 t4, TT5 t5, TT6 t6, TT7 t7, TT8 t8, TT9 t9, TT10 t10) { (*pFunc)(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10); }                                                                            \
                                                                                                                                                                                                                                                                                                                                     \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams11<SNullType, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>* params = reinterpret_cast<SParams11<SNullType, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>*>(p);                                                                                                                                                           \
      CallFunction(&function, params->t0, params->t1, params->t2, params->t3, params->t4, params->t5, params->t6, params->t7, params->t8, params->t9, params->t10);                                                                                                                                                                  \
      params->~SParams11();                                                                                                                                                                                                                                                                                                          \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  /* Overloads for Member-function pointer */                                                                                                                                                                                                                                                                                        \
  template<typename C>                                                                                                                                                                                                                                                                                                               \
  struct SGenInvoker ## type<C>                                                                                                                                                                                                                                                                                                      \
  {                                                                                                                                                                                                                                                                                                                                  \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams0<C>* params = reinterpret_cast<SParams0<C>*>(p);                                                                                                                                                                                                                                                                       \
      assert(params->_pthis != NULL && "Missing this pointer to jobmanager call to: " # function);                                                                                                                                                                                                                                   \
      (params->_pthis->function)();                                                                                                                                                                                                                                                                                                  \
      params->~SParams0();                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename C, typename T0>                                                                                                                                                                                                                                                                                                  \
  struct SGenInvoker ## type<C, T0>                                                                                                                                                                                                                                                                                                  \
  {                                                                                                                                                                                                                                                                                                                                  \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams1<C, T0>* params = reinterpret_cast<SParams1<C, T0>*>(p);                                                                                                                                                                                                                                                               \
      assert(params->_pthis != NULL && "Missing this pointer to jobmanager call to: " # function);                                                                                                                                                                                                                                   \
      (params->_pthis->function)(params->t0);                                                                                                                                                                                                                                                                                        \
      params->~SParams1();                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename C, typename T0, typename T1>                                                                                                                                                                                                                                                                                     \
  struct SGenInvoker ## type<C, T0, T1>                                                                                                                                                                                                                                                                                              \
  {                                                                                                                                                                                                                                                                                                                                  \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams2<C, T0, T1>* params = reinterpret_cast<SParams2<C, T0, T1>*>(p);                                                                                                                                                                                                                                                       \
      assert(params->_pthis != NULL && "Missing this pointer to jobmanager call to: " # function);                                                                                                                                                                                                                                   \
      (params->_pthis->function)(params->t0, params->t1);                                                                                                                                                                                                                                                                            \
      params->~SParams2();                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename C, typename T0, typename T1, typename T2>                                                                                                                                                                                                                                                                        \
  struct SGenInvoker ## type<C, T0, T1, T2>                                                                                                                                                                                                                                                                                          \
  {                                                                                                                                                                                                                                                                                                                                  \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams3<C, T0, T1, T2>* params = reinterpret_cast<SParams3<C, T0, T1, T2>*>(p);                                                                                                                                                                                                                                               \
      assert(params->_pthis != NULL && "Missing this pointer to jobmanager call to: " # function);                                                                                                                                                                                                                                   \
      (params->_pthis->function)(params->t0, params->t1, params->t2);                                                                                                                                                                                                                                                                \
      params->~SParams3();                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename C, typename T0, typename T1, typename T2, typename T3>                                                                                                                                                                                                                                                           \
  struct SGenInvoker ## type<C, T0, T1, T2, T3>                                                                                                                                                                                                                                                                                      \
  {                                                                                                                                                                                                                                                                                                                                  \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams4<C, T0, T1, T2, T3>* params = reinterpret_cast<SParams4<C, T0, T1, T2, T3>*>(p);                                                                                                                                                                                                                                       \
      assert(params->_pthis != NULL && "Missing this pointer to jobmanager call to: " # function);                                                                                                                                                                                                                                   \
      (params->_pthis->function)(params->t0, params->t1, params->t2, params->t3);                                                                                                                                                                                                                                                    \
      params->~SParams4();                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4>                                                                                                                                                                                                                                              \
  struct SGenInvoker ## type<C, T0, T1, T2, T3, T4>                                                                                                                                                                                                                                                                                  \
  {                                                                                                                                                                                                                                                                                                                                  \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams5<C, T0, T1, T2, T3, T4>* params = reinterpret_cast<SParams5<C, T0, T1, T2, T3, T4>*>(p);                                                                                                                                                                                                                               \
      assert(params->_pthis != NULL && "Missing this pointer to jobmanager call to: " # function);                                                                                                                                                                                                                                   \
      (params->_pthis->function)(params->t0, params->t1, params->t2, params->t3, params->t4);                                                                                                                                                                                                                                        \
      params->~SParams5();                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>                                                                                                                                                                                                                                 \
  struct SGenInvoker ## type<C, T0, T1, T2, T3, T4, T5>                                                                                                                                                                                                                                                                              \
  {                                                                                                                                                                                                                                                                                                                                  \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams6<C, T0, T1, T2, T3, T4, T5>* params = reinterpret_cast<SParams6<C, T0, T1, T2, T3, T4, T5>*>(p);                                                                                                                                                                                                                       \
      assert(params->_pthis != NULL && "Missing this pointer to jobmanager call to: " # function);                                                                                                                                                                                                                                   \
      (params->_pthis->function)(params->t0, params->t1, params->t2, params->t3, params->t4, params->t5);                                                                                                                                                                                                                            \
      params->~SParams6();                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>                                                                                                                                                                                                                    \
  struct SGenInvoker ## type<C, T0, T1, T2, T3, T4, T5, T6>                                                                                                                                                                                                                                                                          \
  {                                                                                                                                                                                                                                                                                                                                  \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams7<C, T0, T1, T2, T3, T4, T5, T6>* params = reinterpret_cast<SParams7<C, T0, T1, T2, T3, T4, T5, T6>*>(p);                                                                                                                                                                                                               \
      assert(params->_pthis != NULL && "Missing this pointer to jobmanager call to: " # function);                                                                                                                                                                                                                                   \
      (params->_pthis->function)(params->t0, params->t1, params->t2, params->t3, params->t4, params->t5, params->t6);                                                                                                                                                                                                                \
      params->~SParams7();                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>                                                                                                                                                                                                       \
  struct SGenInvoker ## type<C, T0, T1, T2, T3, T4, T5, T6, T7>                                                                                                                                                                                                                                                                      \
  {                                                                                                                                                                                                                                                                                                                                  \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams8<C, T0, T1, T2, T3, T4, T5, T6, T7>* params = reinterpret_cast<SParams8<C, T0, T1, T2, T3, T4, T5, T6, T7>*>(p);                                                                                                                                                                                                       \
      assert(params->_pthis != NULL && "Missing this pointer to jobmanager call to: " # function);                                                                                                                                                                                                                                   \
      (params->_pthis->function)(params->t0, params->t1, params->t2, params->t3, params->t4, params->t5, params->t6, params->t7);                                                                                                                                                                                                    \
      params->~SParams8();                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>                                                                                                                                                                                          \
  struct SGenInvoker ## type<C, T0, T1, T2, T3, T4, T5, T6, T7, T8>                                                                                                                                                                                                                                                                  \
  {                                                                                                                                                                                                                                                                                                                                  \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams9<C, T0, T1, T2, T3, T4, T5, T6, T7, T8>* params = reinterpret_cast<SParams9<C, T0, T1, T2, T3, T4, T5, T6, T7, T8>*>(p);                                                                                                                                                                                               \
      assert(params->_pthis != NULL && "Missing this pointer to jobmanager call to: " # function);                                                                                                                                                                                                                                   \
      (params->_pthis->function)(params->t0, params->t1, params->t2, params->t3, params->t4, params->t5, params->t6, params->t7, params->t8);                                                                                                                                                                                        \
      params->~SParams9();                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>                                                                                                                                                                             \
  struct SGenInvoker ## type<C, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9>                                                                                                                                                                                                                                                              \
  {                                                                                                                                                                                                                                                                                                                                  \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams10<C, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9>* params = reinterpret_cast<SParams10<C, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9>*>(p);                                                                                                                                                                                     \
      assert(params->_pthis != NULL && "Missing this pointer to jobmanager call to: " # function);                                                                                                                                                                                                                                   \
      (params->_pthis->function)(params->t0, params->t1, params->t2, params->t3, params->t4, params->t5, params->t6, params->t7, params->t8, params->t9);                                                                                                                                                                            \
      params->~SParams10();                                                                                                                                                                                                                                                                                                          \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                     \
  template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>                                                                                                                                                               \
  struct SGenInvoker ## type<C, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>                                                                                                                                                                                                                                                         \
  {                                                                                                                                                                                                                                                                                                                                  \
    static void Invoke(void* p)                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                \
      SParams11<C, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>* params = reinterpret_cast<SParams11<C, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>*>(p);                                                                                                                                                                           \
      assert(params->_pthis != NULL && "Missing this pointer to jobmanager call to: " # function);                                                                                                                                                                                                                                   \
      (params->_pthis->function)(params->t0, params->t1, params->t2, params->t3, params->t4, params->t5, params->t6, params->t7, params->t8, params->t9, params->t10);                                                                                                                                                               \
      params->~SParams11();                                                                                                                                                                                                                                                                                                          \
    }                                                                                                                                                                                                                                                                                                                                \
  };                                                                                                                                                                                                                                                                                                                                 \
  } /* namespace Detail */                                                                                                                                                                                                                                                                                                           \
  } /* namespace JobManager */

//! Macro to generate the actual job class definition.
#define DECLARE_JOB_INVOKER_CLASS(name, type, function)                                                                                                                  \
  namespace JobManager {                                                                                                                                                 \
  namespace Detail {                                                                                                                                                     \
                                                                                                                                                                         \
  /* packet structure for the Producer/Consumer Queue */                                                                                                                 \
  struct CRY_ALIGN(128) packet ## type                                                                                                                                   \
  {                                                                                                                                                                      \
  public:                                                                                                                                                                \
    /* non parameter dependent functions */                                                                                                                              \
    void SetupCommonJob();                                                                                                                                               \
    void SetParamDataSize(unsigned int cParamSize);                                                                                                                      \
    const unsigned int GetPacketSize() const;                                                                                                                            \
    SVEC4_UINT* const __restrict GetPacketCont() const;                                                                                                                  \
    void RegisterJobState(JobManager::SJobState * __restrict pJobState);                                                                                                 \
    JobManager::SJobState* GetJobStateAddress() const;                                                                                                                   \
    template<typename C>                                                                                                                                                 \
    void SetClassInstance(C * pClassInstance);                                                                                                                           \
    uint16 GetProfilerIndex() const;                                                                                                                                     \
                                                                                                                                                                         \
    /*******************************************************************/                                                                                                \
    /* Constructors, one overload for each parameter counter */                                                                                                          \
    /* They all call an overload to determine if it is a free function */                                                                                                \
    /* or a member function pointer */                                                                                                                                   \
    packet ## type();                                                                                                                                                    \
                                                                                                                                                                         \
    template<typename T0>                                                                                                                                                \
    packet ## type(T0 t0);                                                                                                                                               \
                                                                                                                                                                         \
    template<typename T0, typename T1>                                                                                                                                   \
    packet ## type(T0 t0, T1 t1);                                                                                                                                        \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2>                                                                                                                      \
    packet ## type(T0 t0, T1 t1, T2 t2);                                                                                                                                 \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3>                                                                                                         \
    packet ## type(T0 t0, T1 t1, T2 t2, T3 t3);                                                                                                                          \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4>                                                                                            \
    packet ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4);                                                                                                                   \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>                                                                               \
    packet ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5);                                                                                                            \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>                                                                  \
    packet ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6);                                                                                                     \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>                                                     \
    packet ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7);                                                                                              \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>                                        \
    packet ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8);                                                                                       \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>                           \
    packet ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9);                                                                                \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>             \
    packet ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10);                                                                       \
                                                                                                                                                                         \
  private:                                                                                                                                                               \
    CCommonDMABase* GetCommonDMABase();                                                                                                                                  \
                                                                                                                                                                         \
    /**********************************************************************/                                                                                             \
    /* Functions to initialize the parameters - member function overloads */                                                                                             \
    void InitParameter_0(const MemberFunctionPtrTrait &);                                                                                                                \
                                                                                                                                                                         \
    template<typename T0>                                                                                                                                                \
    void InitParameter_1(T0 t0, const MemberFunctionPtrTrait &);                                                                                                         \
                                                                                                                                                                         \
    template<typename T0, typename T1>                                                                                                                                   \
    void InitParameter_2(T0 t0, T1 t1, const MemberFunctionPtrTrait &);                                                                                                  \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2>                                                                                                                      \
    void InitParameter_3(T0 t0, T1 t1, T2 t2, const MemberFunctionPtrTrait &);                                                                                           \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3>                                                                                                         \
    void InitParameter_4(T0 t0, T1 t1, T2 t2, T3 t3, const MemberFunctionPtrTrait &);                                                                                    \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4>                                                                                            \
    void InitParameter_5(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, const MemberFunctionPtrTrait &);                                                                             \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>                                                                               \
    void InitParameter_6(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, const MemberFunctionPtrTrait &);                                                                      \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>                                                                  \
    void InitParameter_7(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, const MemberFunctionPtrTrait &);                                                               \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>                                                     \
    void InitParameter_8(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, const MemberFunctionPtrTrait &);                                                        \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>                                        \
    void InitParameter_9(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, const MemberFunctionPtrTrait &);                                                 \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>                           \
    void InitParameter_10(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, const MemberFunctionPtrTrait &);                                         \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>             \
    void InitParameter_11(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10, const MemberFunctionPtrTrait &);                                \
                                                                                                                                                                         \
    /********************************************************************/                                                                                               \
    /* Functions to initialize the parameters - free function overloads */                                                                                               \
    void InitParameter_0(const FreeFunctionPtrTrait &);                                                                                                                  \
                                                                                                                                                                         \
    template<typename T0>                                                                                                                                                \
    void InitParameter_1(T0 t0, const FreeFunctionPtrTrait &);                                                                                                           \
                                                                                                                                                                         \
    template<typename T0, typename T1>                                                                                                                                   \
    void InitParameter_2(T0 t0, T1 t1, const FreeFunctionPtrTrait &);                                                                                                    \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2>                                                                                                                      \
    void InitParameter_3(T0 t0, T1 t1, T2 t2, const FreeFunctionPtrTrait &);                                                                                             \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3>                                                                                                         \
    void InitParameter_4(T0 t0, T1 t1, T2 t2, T3 t3, const FreeFunctionPtrTrait &);                                                                                      \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4>                                                                                            \
    void InitParameter_5(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, const FreeFunctionPtrTrait &);                                                                               \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>                                                                               \
    void InitParameter_6(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, const FreeFunctionPtrTrait &);                                                                        \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>                                                                  \
    void InitParameter_7(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, const FreeFunctionPtrTrait &);                                                                 \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>                                                     \
    void InitParameter_8(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, const FreeFunctionPtrTrait &);                                                          \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>                                        \
    void InitParameter_9(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, const FreeFunctionPtrTrait &);                                                   \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>                           \
    void InitParameter_10(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, const FreeFunctionPtrTrait &);                                           \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>             \
    void InitParameter_11(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10, const FreeFunctionPtrTrait &);                                  \
                                                                                                                                                                         \
    /*****************/                                                                                                                                                  \
    /* class members */                                                                                                                                                  \
    unsigned char m_ParameterStorage[JobManager::SInfoBlock::scAvailParamSize]; /* Storage for job parameter */                                                          \
    unsigned int paramSize;                                                     /* size of set parameters */                                                             \
    JobManager::SJobState* m_pJobState;                                         /* address of per packet job address */                                                  \
    CCommonDMABase commonDMABase;                                                                                                                                        \
  };  /* struct packet */                                                                                                                                                \
                                                                                                                                                                         \
                                                                                                                                                                         \
  class CRY_ALIGN(128) SGenericJob ## type: public CJobBase                                                                                                              \
  {                                                                                                                                                                      \
  public:                                                                                                                                                                \
    typedef JobManager::Detail::packet ## type packet;                                                                                                                   \
                                                                                                                                                                         \
    /*******************************************/                                                                                                                        \
    /* Common non job dependent interface part */                                                                                                                        \
    const JobManager::TJobHandle GetProgramHandle()  const volatile;                                                                                                     \
    uint32 GetJobID();                                                                                                                                                   \
    const char* GetJobName() const;                                                                                                                                      \
    const char* GetJobName() const volatile;                                                                                                                             \
    void SetParamDataSize(const unsigned int cParamSize);                                                                                                                \
    void SetPriorityLevel(unsigned int nPriorityLevel);                                                                                                                  \
    void ForceUpdateOfProfilingDataIndex();                                                                                                                              \
    void SetBlocking();                                                                                                                                                  \
    unsigned int GetParamDataSize();                                                                                                                                     \
    void SetJobParamData(void* paramMem);                                                                                                                                \
    Invoker GetGenericDelegator() const;                                                                                                                                 \
    void SetupCommonJob();                                                                                                                                               \
    template<typename C>                                                                                                                                                 \
    void SetClassInstance(C * pClassInstance);                                                                                                                           \
                                                                                                                                                                         \
                                                                                                                                                                         \
    /*******************************************************************/                                                                                                \
    /* Constructors, one overload for each parameter counter */                                                                                                          \
    /* They all call an overload to determine if it is a free function */                                                                                                \
    /* or a member function pointer */                                                                                                                                   \
    SGenericJob ## type();                                                                                                                                               \
                                                                                                                                                                         \
    template<typename T0>                                                                                                                                                \
    SGenericJob ## type(T0 t0);                                                                                                                                          \
                                                                                                                                                                         \
    template<typename T0, typename T1>                                                                                                                                   \
    SGenericJob ## type(T0 t0, T1 t1);                                                                                                                                   \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2>                                                                                                                      \
    SGenericJob ## type(T0 t0, T1 t1, T2 t2);                                                                                                                            \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3>                                                                                                         \
    SGenericJob ## type(T0 t0, T1 t1, T2 t2, T3 t3);                                                                                                                     \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4>                                                                                            \
    SGenericJob ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4);                                                                                                              \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>                                                                               \
    SGenericJob ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5);                                                                                                       \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>                                                                  \
    SGenericJob ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6);                                                                                                \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>                                                     \
    SGenericJob ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7);                                                                                         \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>                                        \
    SGenericJob ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8);                                                                                  \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>                           \
    SGenericJob ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9);                                                                           \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>             \
    SGenericJob ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10);                                                                  \
                                                                                                                                                                         \
    /* overloaded new/delete operator to ensure aligned memory when doing a heap allocation */                                                                           \
    void* operator new(size_t nSize)        { return CryModuleMemalign(nSize, 128); }                                                                                    \
    void  operator delete(void* pPtr)       { CryModuleMemalignFree(pPtr); }                                                                                             \
                                                                                                                                                                         \
    void* operator new(size_t, void* where) { return where; }                                                                                                            \
                                                                                                                                                                         \
  private:                                                                                                                                                               \
    /**********************************************************************/                                                                                             \
    /* Functions to initialize the parameters - member function overloads */                                                                                             \
                                                                                                                                                                         \
    void InitParameter_0(const MemberFunctionPtrTrait &);                                                                                                                \
                                                                                                                                                                         \
    template<typename T0>                                                                                                                                                \
    void InitParameter_1(T0 t0, const MemberFunctionPtrTrait &);                                                                                                         \
                                                                                                                                                                         \
    template<typename T0, typename T1>                                                                                                                                   \
    void InitParameter_2(T0 t0, T1 t1, const MemberFunctionPtrTrait &);                                                                                                  \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2>                                                                                                                      \
    void InitParameter_3(T0 t0, T1 t1, T2 t2, const MemberFunctionPtrTrait &);                                                                                           \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3>                                                                                                         \
    void InitParameter_4(T0 t0, T1 t1, T2 t2, T3 t3, const MemberFunctionPtrTrait &);                                                                                    \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4>                                                                                            \
    void InitParameter_5(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, const MemberFunctionPtrTrait &);                                                                             \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>                                                                               \
    void InitParameter_6(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, const MemberFunctionPtrTrait &);                                                                      \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>                                                                  \
    void InitParameter_7(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, const MemberFunctionPtrTrait &);                                                               \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>                                                     \
    void InitParameter_8(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, const MemberFunctionPtrTrait &);                                                        \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>                                        \
    void InitParameter_9(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, const MemberFunctionPtrTrait &);                                                 \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>                           \
    void InitParameter_10(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, const MemberFunctionPtrTrait &);                                         \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>             \
    void InitParameter_11(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10, const MemberFunctionPtrTrait &);                                \
                                                                                                                                                                         \
    /********************************************************************/                                                                                               \
    /* Functions to initialize the parameters - free function overloads */                                                                                               \
    /* The null parameter version needs special handling, since it is no template */                                                                                     \
                                                                                                                                                                         \
    void InitParameter_0(const FreeFunctionPtrTrait &);                                                                                                                  \
                                                                                                                                                                         \
    template<typename T0>                                                                                                                                                \
    void InitParameter_1(T0 t0, const FreeFunctionPtrTrait &);                                                                                                           \
                                                                                                                                                                         \
    template<typename T0, typename T1>                                                                                                                                   \
    void InitParameter_2(T0 t0, T1 t1, const FreeFunctionPtrTrait &);                                                                                                    \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2>                                                                                                                      \
    void InitParameter_3(T0 t0, T1 t1, T2 t2, const FreeFunctionPtrTrait &);                                                                                             \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3>                                                                                                         \
    void InitParameter_4(T0 t0, T1 t1, T2 t2, T3 t3, const FreeFunctionPtrTrait &);                                                                                      \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4>                                                                                            \
    void InitParameter_5(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, const FreeFunctionPtrTrait &);                                                                               \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>                                                                               \
    void InitParameter_6(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, const FreeFunctionPtrTrait &);                                                                        \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>                                                                  \
    void InitParameter_7(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, const FreeFunctionPtrTrait &);                                                                 \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>                                                     \
    void InitParameter_8(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, const FreeFunctionPtrTrait &);                                                          \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>                                        \
    void InitParameter_9(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, const FreeFunctionPtrTrait &);                                                   \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>                           \
    void InitParameter_10(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, const FreeFunctionPtrTrait &);                                           \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>             \
    void InitParameter_11(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10, const FreeFunctionPtrTrait &);                                  \
                                                                                                                                                                         \
                                                                                                                                                                         \
    /***************************************************************************/                                                                                        \
    /* Function overloads to get the right invoker function */                                                                                                           \
    /* Overloads for free-functions */                                                                                                                                   \
    static Invoker GenerateInvoker(void (* pFunc)());                                                                                                                    \
                                                                                                                                                                         \
    template<typename T0>                                                                                                                                                \
    static Invoker GenerateInvoker(void (* pFunc)(T0));                                                                                                                  \
                                                                                                                                                                         \
    template<typename T0, typename T1>                                                                                                                                   \
    static Invoker GenerateInvoker(void (* pFunc)(T0, T1));                                                                                                              \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2>                                                                                                                      \
    static Invoker GenerateInvoker(void (* pFunc)(T0, T1, T2));                                                                                                          \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3>                                                                                                         \
    static Invoker GenerateInvoker(void (* pFunc)(T0, T1, T2, T3));                                                                                                      \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4>                                                                                            \
    static Invoker GenerateInvoker(void (* pFunc)(T0, T1, T2, T3, T4));                                                                                                  \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>                                                                               \
    static Invoker GenerateInvoker(void (* pFunc)(T0, T1, T2, T3, T4, T5));                                                                                              \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>                                                                  \
    static Invoker GenerateInvoker(void (* pFunc)(T0, T1, T2, T3, T4, T5, T6));                                                                                          \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>                                                     \
    static Invoker GenerateInvoker(void (* pFunc)(T0, T1, T2, T3, T4, T5, T6, T7));                                                                                      \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>                                        \
    static Invoker GenerateInvoker(void (* pFunc)(T0, T1, T2, T3, T4, T5, T6, T7, T8));                                                                                  \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>                           \
    static Invoker GenerateInvoker(void (* pFunc)(T0, T1, T2, T3, T4, T5, T6, T7, T8, T9));                                                                              \
                                                                                                                                                                         \
    template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>             \
    static Invoker GenerateInvoker(void (* pFunc)(T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10));                                                                         \
                                                                                                                                                                         \
    /* Overloads for free-functions */                                                                                                                                   \
    template<typename C>                                                                                                                                                 \
    static Invoker GenerateInvoker(void (C::* pFunc)());                                                                                                                 \
                                                                                                                                                                         \
    template<typename C, typename T0>                                                                                                                                    \
    static Invoker GenerateInvoker(void (C::* pFunc)(T0));                                                                                                               \
                                                                                                                                                                         \
    template<typename C, typename T0, typename T1>                                                                                                                       \
    static Invoker GenerateInvoker(void (C::* pFunc)(T0, T1));                                                                                                           \
                                                                                                                                                                         \
    template<typename C, typename T0, typename T1, typename T2>                                                                                                          \
    static Invoker GenerateInvoker(void (C::* pFunc)(T0, T1, T2));                                                                                                       \
                                                                                                                                                                         \
    template<typename C, typename T0, typename T1, typename T2, typename T3>                                                                                             \
    static Invoker GenerateInvoker(void (C::* pFunc)(T0, T1, T2, T3));                                                                                                   \
                                                                                                                                                                         \
    template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4>                                                                                \
    static Invoker GenerateInvoker(void (C::* pFunc)(T0, T1, T2, T3, T4));                                                                                               \
                                                                                                                                                                         \
    template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>                                                                   \
    static Invoker GenerateInvoker(void (C::* pFunc)(T0, T1, T2, T3, T4, T5));                                                                                           \
                                                                                                                                                                         \
    template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>                                                      \
    static Invoker GenerateInvoker(void (C::* pFunc)(T0, T1, T2, T3, T4, T5, T6));                                                                                       \
                                                                                                                                                                         \
    template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>                                         \
    static Invoker GenerateInvoker(void (C::* pFunc)(T0, T1, T2, T3, T4, T5, T6, T7));                                                                                   \
                                                                                                                                                                         \
    template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>                            \
    static Invoker GenerateInvoker(void (C::* pFunc)(T0, T1, T2, T3, T4, T5, T6, T7, T8));                                                                               \
                                                                                                                                                                         \
    template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>               \
    static Invoker GenerateInvoker(void (C::* pFunc)(T0, T1, T2, T3, T4, T5, T6, T7, T8, T9));                                                                           \
                                                                                                                                                                         \
    template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10> \
    static Invoker GenerateInvoker(void (C::* pFunc)(T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10));                                                                      \
                                                                                                                                                                         \
                                                                                                                                                                         \
    /***************************************************************************/                                                                                        \
    /* class members */                                                                                                                                                  \
    CRY_ALIGN(128) unsigned char m_ParameterStorage[JobManager::SInfoBlock::scAvailParamSize]; /* memory to store serialized parameters */                               \
    JobManager::Invoker m_Invoker;                                                             /* Invoker function to de-serialize the parameters*/                      \
                                                                                                                                                                         \
  };                                                                                                                                                                     \
                                                                                                                                                                         \
                                                                                                                                                                         \
  /*******************************************/                                                                                                                          \
  /* Implementation of SGenericJob functions */                                                                                                                          \
  inline const JobManager::TJobHandle SGenericJob ## type::GetProgramHandle() const volatile                                                                             \
  {                                                                                                                                                                      \
    if (gJobHandle ## type == NULL)                                                                                                                                      \
    {                                                                                                                                                                    \
      /*Safe, since GetJobHandle itself is thread safe and should always return the same result for a job */                                                             \
      gJobHandle ## type = gEnv->GetJobManager()->GetJobHandle(name, sizeof(name) - 1, GenerateInvoker(&function));                                                      \
    }                                                                                                                                                                    \
    return gJobHandle ## type;                                                                                                                                           \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  inline uint32 SGenericJob ## type::GetJobID()                                                                                                                          \
  {                                                                                                                                                                      \
    return GetProgramHandle()->jobId;                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  inline const char* SGenericJob ## type::GetJobName() const                                                                                                             \
  {                                                                                                                                                                      \
    return GetProgramHandle()->cpString;                                                                                                                                 \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  inline const char* SGenericJob ## type::GetJobName() const volatile                                                                                                    \
  {                                                                                                                                                                      \
    return GetProgramHandle()->cpString;                                                                                                                                 \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  inline void SGenericJob ## type::SetParamDataSize(const unsigned int cParamSize)                                                                                       \
  {                                                                                                                                                                      \
    this->m_JobDelegator.SetParamDataSize((cParamSize + 0xF) & ~0xF);                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  inline void SGenericJob ## type::ForceUpdateOfProfilingDataIndex()                                                                                                     \
  {                                                                                                                                                                      \
    this->m_JobDelegator.ForceUpdateOfProfilingDataIndex();                                                                                                              \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  inline void SGenericJob ## type::SetPriorityLevel(unsigned int nPriorityLevel)                                                                                         \
  {                                                                                                                                                                      \
    this->m_JobDelegator.SetPriorityLevel(nPriorityLevel);                                                                                                               \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  inline void SGenericJob ## type::SetBlocking()                                                                                                                         \
  {                                                                                                                                                                      \
    this->m_JobDelegator.SetBlocking();                                                                                                                                  \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  inline unsigned int SGenericJob ## type::GetParamDataSize()                                                                                                            \
  {                                                                                                                                                                      \
    return this->m_JobDelegator.GetParamDataSize();                                                                                                                      \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  inline void SGenericJob ## type::SetJobParamData(void* paramMem)                                                                                                       \
  {                                                                                                                                                                      \
    this->m_JobDelegator.SetJobParamData(paramMem);                                                                                                                      \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  inline Invoker SGenericJob ## type::GetGenericDelegator() const                                                                                                        \
  {                                                                                                                                                                      \
    return m_Invoker;                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  inline void SGenericJob ## type::SetupCommonJob()                                                                                                                      \
  {                                                                                                                                                                      \
    this->m_JobDelegator.SetJobParamData((void*)m_ParameterStorage);                                                                                                     \
    this->m_JobDelegator.SetDelegator(m_Invoker);                                                                                                                        \
    this->SetJobProgramData(GetProgramHandle());                                                                                                                         \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename C>                                                                                                                                                   \
  inline void SGenericJob ## type::SetClassInstance(C * pClassInstance)                                                                                                  \
  {                                                                                                                                                                      \
    VerifyThisPtr(&function, SVerifyThisPtr<C>());                                                                                                                       \
    *alias_cast<C**>(m_ParameterStorage) = pClassInstance;                                                                                                               \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  /**********************************/                                                                                                                                   \
  /* Implementation of Constructors */                                                                                                                                   \
  inline SGenericJob ## type::SGenericJob ## type()                                                                                                                      \
  {                                                                                                                                                                      \
    m_Invoker = GenerateInvoker(&function);                                                                                                                              \
    InitParameter_0(FUNCTION_PTR_TYPE(function));                                                                                                                        \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0>                                                                                                                                                  \
  inline SGenericJob ## type::SGenericJob ## type(T0 t0)                                                                                                                 \
  {                                                                                                                                                                      \
    VerifyParameter_1(&function, SVerifyParameter1<T0>());                                                                                                               \
    m_Invoker = GenerateInvoker(&function);                                                                                                                              \
    InitParameter_1(t0, FUNCTION_PTR_TYPE(function));                                                                                                                    \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1>                                                                                                                                     \
  inline SGenericJob ## type::SGenericJob ## type(T0 t0, T1 t1)                                                                                                          \
  {                                                                                                                                                                      \
    VerifyParameter_2(&function, SVerifyParameter2<T0, T1>());                                                                                                           \
    m_Invoker = GenerateInvoker(&function);                                                                                                                              \
    InitParameter_2(t0, t1, FUNCTION_PTR_TYPE(function));                                                                                                                \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2>                                                                                                                        \
  inline SGenericJob ## type::SGenericJob ## type(T0 t0, T1 t1, T2 t2)                                                                                                   \
  {                                                                                                                                                                      \
    VerifyParameter_3(&function, SVerifyParameter3<T0, T1, T2>());                                                                                                       \
    m_Invoker = GenerateInvoker(&function);                                                                                                                              \
    InitParameter_3(t0, t1, t2, FUNCTION_PTR_TYPE(function));                                                                                                            \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3>                                                                                                           \
  inline SGenericJob ## type::SGenericJob ## type(T0 t0, T1 t1, T2 t2, T3 t3)                                                                                            \
  {                                                                                                                                                                      \
    VerifyParameter_4(&function, SVerifyParameter4<T0, T1, T2, T3>());                                                                                                   \
    m_Invoker = GenerateInvoker(&function);                                                                                                                              \
    InitParameter_4(t0, t1, t2, t3, FUNCTION_PTR_TYPE(function));                                                                                                        \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4>                                                                                              \
  inline SGenericJob ## type::SGenericJob ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4)                                                                                     \
  {                                                                                                                                                                      \
    VerifyParameter_5(&function, SVerifyParameter5<T0, T1, T2, T3, T4>());                                                                                               \
    m_Invoker = GenerateInvoker(&function);                                                                                                                              \
    InitParameter_5(t0, t1, t2, t3, t4, FUNCTION_PTR_TYPE(function));                                                                                                    \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>                                                                                 \
  inline SGenericJob ## type::SGenericJob ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5)                                                                              \
  {                                                                                                                                                                      \
    VerifyParameter_6(&function, SVerifyParameter6<T0, T1, T2, T3, T4, T5>());                                                                                           \
    m_Invoker = GenerateInvoker(&function);                                                                                                                              \
    InitParameter_6(t0, t1, t2, t3, t4, t5, FUNCTION_PTR_TYPE(function));                                                                                                \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>                                                                    \
  inline SGenericJob ## type::SGenericJob ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6)                                                                       \
  {                                                                                                                                                                      \
    VerifyParameter_7(&function, SVerifyParameter7<T0, T1, T2, T3, T4, T5, T6>());                                                                                       \
    m_Invoker = GenerateInvoker(&function);                                                                                                                              \
    InitParameter_7(t0, t1, t2, t3, t4, t5, t6, FUNCTION_PTR_TYPE(function));                                                                                            \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>                                                       \
  inline SGenericJob ## type::SGenericJob ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7)                                                                \
  {                                                                                                                                                                      \
    VerifyParameter_8(&function, SVerifyParameter8<T0, T1, T2, T3, T4, T5, T6, T7>());                                                                                   \
    m_Invoker = GenerateInvoker(&function);                                                                                                                              \
    InitParameter_8(t0, t1, t2, t3, t4, t5, t6, t7, FUNCTION_PTR_TYPE(function));                                                                                        \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>                                          \
  inline SGenericJob ## type::SGenericJob ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8)                                                         \
  {                                                                                                                                                                      \
    VerifyParameter_9(&function, SVerifyParameter9<T0, T1, T2, T3, T4, T5, T6, T7, T8>());                                                                               \
    m_Invoker = GenerateInvoker(&function);                                                                                                                              \
    InitParameter_9(t0, t1, t2, t3, t4, t5, t6, t7, t8, FUNCTION_PTR_TYPE(function));                                                                                    \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>                             \
  inline SGenericJob ## type::SGenericJob ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9)                                                  \
  {                                                                                                                                                                      \
    VerifyParameter_10(&function, SVerifyParameter10<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9>());                                                                         \
    m_Invoker = GenerateInvoker(&function);                                                                                                                              \
    InitParameter_10(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, FUNCTION_PTR_TYPE(function));                                                                               \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>               \
  inline SGenericJob ## type::SGenericJob ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10)                                         \
  {                                                                                                                                                                      \
    VerifyParameter_11(&function, SVerifyParameter11<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>());                                                                    \
    m_Invoker = GenerateInvoker(&function);                                                                                                                              \
    InitParameter_11(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, FUNCTION_PTR_TYPE(function));                                                                          \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
                                                                                                                                                                         \
  /**********************************************************************/                                                                                               \
  /* Implementation of the InitParameter functions */                                                                                                                    \
  /* Functions to initialize the parameters - member function overloads */                                                                                               \
                                                                                                                                                                         \
  inline void SGenericJob ## type::InitParameter_0(const MemberFunctionPtrTrait &)                                                                                       \
  {                                                                                                                                                                      \
    typedef SParams0<void*> SParameterType;                                                                                                                              \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType();                                                                                                                            \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0>                                                                                                                                                  \
  inline void SGenericJob ## type::InitParameter_1(T0 t0, const MemberFunctionPtrTrait &)                                                                                \
  {                                                                                                                                                                      \
    typedef SParams1<void*, T0> SParameterType;                                                                                                                          \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0);                                                                                                                          \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1>                                                                                                                                     \
  inline void SGenericJob ## type::InitParameter_2(T0 t0, T1 t1, const MemberFunctionPtrTrait &)                                                                         \
  {                                                                                                                                                                      \
    typedef SParams2<void*, T0, T1> SParameterType;                                                                                                                      \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1);                                                                                                                      \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2>                                                                                                                        \
  inline void SGenericJob ## type::InitParameter_3(T0 t0, T1 t1, T2 t2, const MemberFunctionPtrTrait &)                                                                  \
  {                                                                                                                                                                      \
    typedef SParams3<void*, T0, T1, T2> SParameterType;                                                                                                                  \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2);                                                                                                                  \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3>                                                                                                           \
  inline void SGenericJob ## type::InitParameter_4(T0 t0, T1 t1, T2 t2, T3 t3, const MemberFunctionPtrTrait &)                                                           \
  {                                                                                                                                                                      \
    typedef SParams4<void*, T0, T1, T2, T3> SParameterType;                                                                                                              \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3);                                                                                                              \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4>                                                                                              \
  inline void SGenericJob ## type::InitParameter_5(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, const MemberFunctionPtrTrait &)                                                    \
  {                                                                                                                                                                      \
    typedef SParams5<void*, T0, T1, T2, T3, T4> SParameterType;                                                                                                          \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4);                                                                                                          \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>                                                                                 \
  inline void SGenericJob ## type::InitParameter_6(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, const MemberFunctionPtrTrait &)                                             \
  {                                                                                                                                                                      \
    typedef SParams6<void*, T0, T1, T2, T3, T4, T5> SParameterType;                                                                                                      \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5);                                                                                                      \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>                                                                    \
  inline void SGenericJob ## type::InitParameter_7(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, const MemberFunctionPtrTrait &)                                      \
  {                                                                                                                                                                      \
    typedef SParams7<void*, T0, T1, T2, T3, T4, T5, T6> SParameterType;                                                                                                  \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5, t6);                                                                                                  \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>                                                       \
  inline void SGenericJob ## type::InitParameter_8(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, const MemberFunctionPtrTrait &)                               \
  {                                                                                                                                                                      \
    typedef SParams8<void*, T0, T1, T2, T3, T4, T5, T6, T7> SParameterType;                                                                                              \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5, t6, t7);                                                                                              \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>                                          \
  inline void SGenericJob ## type::InitParameter_9(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, const MemberFunctionPtrTrait &)                        \
  {                                                                                                                                                                      \
    typedef SParams9<void*, T0, T1, T2, T3, T4, T5, T6, T7, T8> SParameterType;                                                                                          \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5, t6, t7, t8);                                                                                          \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>                             \
  inline void SGenericJob ## type::InitParameter_10(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, const MemberFunctionPtrTrait &)                \
  {                                                                                                                                                                      \
    typedef SParams10<void*, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9> SParameterType;                                                                                     \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9);                                                                                      \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>               \
  inline void SGenericJob ## type::InitParameter_11(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10, const MemberFunctionPtrTrait &)       \
  {                                                                                                                                                                      \
    typedef SParams11<void*, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10> SParameterType;                                                                                \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10);                                                                                 \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
                                                                                                                                                                         \
  /* Functions to initialize the parameters - free function overloads */                                                                                                 \
  inline void SGenericJob ## type::InitParameter_0(const FreeFunctionPtrTrait &)                                                                                         \
  {                                                                                                                                                                      \
    typedef SParams0<SNullType> SParameterType;                                                                                                                          \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType();                                                                                                                            \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0>                                                                                                                                                  \
  inline void SGenericJob ## type::InitParameter_1(T0 t0, const FreeFunctionPtrTrait &)                                                                                  \
  {                                                                                                                                                                      \
    typedef SParams1<SNullType, T0> SParameterType;                                                                                                                      \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0);                                                                                                                          \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1>                                                                                                                                     \
  inline void SGenericJob ## type::InitParameter_2(T0 t0, T1 t1, const FreeFunctionPtrTrait &)                                                                           \
  {                                                                                                                                                                      \
    typedef SParams2<SNullType, T0, T1> SParameterType;                                                                                                                  \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1);                                                                                                                      \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2>                                                                                                                        \
  inline void SGenericJob ## type::InitParameter_3(T0 t0, T1 t1, T2 t2, const FreeFunctionPtrTrait &)                                                                    \
  {                                                                                                                                                                      \
    typedef SParams3<SNullType, T0, T1, T2> SParameterType;                                                                                                              \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2);                                                                                                                  \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3>                                                                                                           \
  inline void SGenericJob ## type::InitParameter_4(T0 t0, T1 t1, T2 t2, T3 t3, const FreeFunctionPtrTrait &)                                                             \
  {                                                                                                                                                                      \
    typedef SParams4<SNullType, T0, T1, T2, T3> SParameterType;                                                                                                          \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3);                                                                                                              \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4>                                                                                              \
  inline void SGenericJob ## type::InitParameter_5(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, const FreeFunctionPtrTrait &)                                                      \
  {                                                                                                                                                                      \
    typedef SParams5<SNullType, T0, T1, T2, T3, T4> SParameterType;                                                                                                      \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4);                                                                                                          \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>                                                                                 \
  inline void SGenericJob ## type::InitParameter_6(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, const FreeFunctionPtrTrait &)                                               \
  {                                                                                                                                                                      \
    typedef SParams6<SNullType, T0, T1, T2, T3, T4, T5> SParameterType;                                                                                                  \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5);                                                                                                      \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>                                                                    \
  inline void SGenericJob ## type::InitParameter_7(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, const FreeFunctionPtrTrait &)                                        \
  {                                                                                                                                                                      \
    typedef SParams7<SNullType, T0, T1, T2, T3, T4, T5, T6> SParameterType;                                                                                              \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5, t6);                                                                                                  \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>                                                       \
  inline void SGenericJob ## type::InitParameter_8(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, const FreeFunctionPtrTrait &)                                 \
  {                                                                                                                                                                      \
    typedef SParams8<SNullType, T0, T1, T2, T3, T4, T5, T6, T7> SParameterType;                                                                                          \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5, t6, t7);                                                                                              \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>                                          \
  inline void SGenericJob ## type::InitParameter_9(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, const FreeFunctionPtrTrait &)                          \
  {                                                                                                                                                                      \
    typedef SParams9<SNullType, T0, T1, T2, T3, T4, T5, T6, T7, T8> SParameterType;                                                                                      \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5, t6, t7, t8);                                                                                          \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>                             \
  inline void SGenericJob ## type::InitParameter_10(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, const FreeFunctionPtrTrait &)                  \
  {                                                                                                                                                                      \
    typedef SParams10<SNullType, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9> SParameterType;                                                                                 \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9);                                                                                      \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>               \
  inline void SGenericJob ## type::InitParameter_11(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10, const FreeFunctionPtrTrait &)         \
  {                                                                                                                                                                      \
    typedef SParams11<SNullType, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10> SParameterType;                                                                            \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10);                                                                                 \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  /*****************************************************************************/                                                                                        \
  /* Function to generate the right Invoker function */                                                                                                                  \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (* pFunc)())                                                                                                  \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<SNullType>::Invoke;                                                                                                                      \
  }                                                                                                                                                                      \
  template<typename T0>                                                                                                                                                  \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (* pFunc)(T0))                                                                                                \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<SNullType, T0>::Invoke;                                                                                                                  \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1>                                                                                                                                     \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (* pFunc)(T0, T1))                                                                                            \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<SNullType, T0, T1>::Invoke;                                                                                                              \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2>                                                                                                                        \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (* pFunc)(T0, T1, T2))                                                                                        \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<SNullType, T0, T1, T2>::Invoke;                                                                                                          \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3>                                                                                                           \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (* pFunc)(T0, T1, T2, T3))                                                                                    \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<SNullType, T0, T1, T2, T3>::Invoke;                                                                                                      \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4>                                                                                              \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (* pFunc)(T0, T1, T2, T3, T4))                                                                                \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<SNullType, T0, T1, T2, T3, T4>::Invoke;                                                                                                  \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>                                                                                 \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (* pFunc)(T0, T1, T2, T3, T4, T5))                                                                            \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<SNullType, T0, T1, T2, T3, T4, T5>::Invoke;                                                                                              \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>                                                                    \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (* pFunc)(T0, T1, T2, T3, T4, T5, T6))                                                                        \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<SNullType, T0, T1, T2, T3, T4, T5, T6>::Invoke;                                                                                          \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>                                                       \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (* pFunc)(T0, T1, T2, T3, T4, T5, T6, T7))                                                                    \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<SNullType, T0, T1, T2, T3, T4, T5, T6, T7>::Invoke;                                                                                      \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>                                          \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (* pFunc)(T0, T1, T2, T3, T4, T5, T6, T7, T8))                                                                \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<SNullType, T0, T1, T2, T3, T4, T5, T6, T7, T8>::Invoke;                                                                                  \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>                             \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (* pFunc)(T0, T1, T2, T3, T4, T5, T6, T7, T8, T9))                                                            \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<SNullType, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9>::Invoke;                                                                              \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>               \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (* pFunc)(T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10))                                                       \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<SNullType, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>::Invoke;                                                                         \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  /* Overloads for member-functions */                                                                                                                                   \
  template<typename C>                                                                                                                                                   \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (C::* pFunc)())                                                                                               \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<C>::Invoke;                                                                                                                              \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename C, typename T0>                                                                                                                                      \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (C::* pFunc)(T0))                                                                                             \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<C, T0>::Invoke;                                                                                                                          \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename C, typename T0, typename T1>                                                                                                                         \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (C::* pFunc)(T0, T1))                                                                                         \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<C, T0, T1>::Invoke;                                                                                                                      \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename C, typename T0, typename T1, typename T2>                                                                                                            \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (C::* pFunc)(T0, T1, T2))                                                                                     \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<C, T0, T1, T2>::Invoke;                                                                                                                  \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename C, typename T0, typename T1, typename T2, typename T3>                                                                                               \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (C::* pFunc)(T0, T1, T2, T3))                                                                                 \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<C, T0, T1, T2, T3>::Invoke;                                                                                                              \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4>                                                                                  \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (C::* pFunc)(T0, T1, T2, T3, T4))                                                                             \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<C, T0, T1, T2, T3, T4>::Invoke;                                                                                                          \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>                                                                     \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (C::* pFunc)(T0, T1, T2, T3, T4, T5))                                                                         \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<C, T0, T1, T2, T3, T4, T5>::Invoke;                                                                                                      \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>                                                        \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (C::* pFunc)(T0, T1, T2, T3, T4, T5, T6))                                                                     \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<C, T0, T1, T2, T3, T4, T5, T6>::Invoke;                                                                                                  \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>                                           \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (C::* pFunc)(T0, T1, T2, T3, T4, T5, T6, T7))                                                                 \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<C, T0, T1, T2, T3, T4, T5, T6, T7>::Invoke;                                                                                              \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>                              \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (C::* pFunc)(T0, T1, T2, T3, T4, T5, T6, T7, T8))                                                             \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<C, T0, T1, T2, T3, T4, T5, T6, T7, T8>::Invoke;                                                                                          \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>                 \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (C::* pFunc)(T0, T1, T2, T3, T4, T5, T6, T7, T8, T9))                                                         \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<C, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9>::Invoke;                                                                                      \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename C, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>   \
  inline Invoker SGenericJob ## type::GenerateInvoker(void (C::* pFunc)(T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10))                                                    \
  {                                                                                                                                                                      \
    return &SGenInvoker ## type<C, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>::Invoke;                                                                                 \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
                                                                                                                                                                         \
  /*****************************************************************************/                                                                                        \
  /*****************************************************************************/                                                                                        \
  /* Implementation of the embedded packt structure used for the producer/consumer queue */                                                                              \
  inline void packet ## type::SetupCommonJob()                                                                                                                           \
  {                                                                                                                                                                      \
    commonDMABase.SetJobParamData((void*)m_ParameterStorage);                                                                                                            \
    m_pJobState = 0;                                                                                                                                                     \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  inline void packet ## type::SetParamDataSize(unsigned int cParamSize)                                                                                                  \
  {                                                                                                                                                                      \
    paramSize = ((cParamSize + 0xF) & ~0xF);                                                                                                                             \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  inline const unsigned int packet ## type::GetPacketSize() const                                                                                                        \
  {                                                                                                                                                                      \
    return paramSize;                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  inline SVEC4_UINT* const __restrict packet ## type::GetPacketCont() const                                                                                              \
  {                                                                                                                                                                      \
    return (SVEC4_UINT*)m_ParameterStorage;                                                                                                                              \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  inline JobManager::SJobState* packet ## type::GetJobStateAddress() const                                                                                               \
  {                                                                                                                                                                      \
    return m_pJobState;                                                                                                                                                  \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  inline void packet ## type::RegisterJobState(JobManager::SJobState * pJobState)                                                                                        \
  {                                                                                                                                                                      \
    m_pJobState = pJobState;                                                                                                                                             \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename C>                                                                                                                                                   \
  inline void packet ## type::SetClassInstance(C * pClassInstance)                                                                                                       \
  {                                                                                                                                                                      \
    VerifyThisPtr(&function, SVerifyThisPtr<C>());                                                                                                                       \
    *alias_cast<C**>(m_ParameterStorage) = pClassInstance;                                                                                                               \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  inline uint16 packet ## type::GetProfilerIndex() const                                                                                                                 \
  {                                                                                                                                                                      \
    return commonDMABase.GetProfilingDataIndex();                                                                                                                        \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  /*****************************************************************************/                                                                                        \
  /* Implementation of the embedded packet structure constructors */                                                                                                     \
  inline packet ## type::packet ## type()                                                                                                                                \
  {                                                                                                                                                                      \
    InitParameter_0(FUNCTION_PTR_TYPE(function));                                                                                                                        \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0>                                                                                                                                                  \
  inline packet ## type::packet ## type(T0 t0)                                                                                                                           \
  {                                                                                                                                                                      \
    VerifyParameter_1(&function, SVerifyParameter1<T0>());                                                                                                               \
    InitParameter_1(t0, FUNCTION_PTR_TYPE(function));                                                                                                                    \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1>                                                                                                                                     \
  inline packet ## type::packet ## type(T0 t0, T1 t1)                                                                                                                    \
  {                                                                                                                                                                      \
    VerifyParameter_2(&function, SVerifyParameter2<T0, T1>());                                                                                                           \
    InitParameter_2(t0, t1, FUNCTION_PTR_TYPE(function));                                                                                                                \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2>                                                                                                                        \
  inline packet ## type::packet ## type(T0 t0, T1 t1, T2 t2)                                                                                                             \
  {                                                                                                                                                                      \
    VerifyParameter_3(&function, SVerifyParameter3<T0, T1, T2>());                                                                                                       \
    InitParameter_3(t0, t1, t2, FUNCTION_PTR_TYPE(function));                                                                                                            \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3>                                                                                                           \
  inline packet ## type::packet ## type(T0 t0, T1 t1, T2 t2, T3 t3)                                                                                                      \
  {                                                                                                                                                                      \
    VerifyParameter_4(&function, SVerifyParameter4<T0, T1, T2, T3>());                                                                                                   \
    InitParameter_4(t0, t1, t2, t3, FUNCTION_PTR_TYPE(function));                                                                                                        \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4>                                                                                              \
  inline packet ## type::packet ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4)                                                                                               \
  {                                                                                                                                                                      \
    VerifyParameter_5(&function, SVerifyParameter5<T0, T1, T2, T3, T4>());                                                                                               \
    InitParameter_5(t0, t1, t2, t3, t4, FUNCTION_PTR_TYPE(function));                                                                                                    \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>                                                                                 \
  inline packet ## type::packet ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5)                                                                                        \
  {                                                                                                                                                                      \
    VerifyParameter_6(&function, SVerifyParameter6<T0, T1, T2, T3, T4, T5>());                                                                                           \
    InitParameter_6(t0, t1, t2, t3, t4, t5, FUNCTION_PTR_TYPE(function));                                                                                                \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>                                                                    \
  inline packet ## type::packet ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6)                                                                                 \
  {                                                                                                                                                                      \
    VerifyParameter_7(&function, SVerifyParameter7<T0, T1, T2, T3, T4, T5, T6>());                                                                                       \
    InitParameter_7(t0, t1, t2, t3, t4, t5, t6, FUNCTION_PTR_TYPE(function));                                                                                            \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>                                                       \
  inline packet ## type::packet ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7)                                                                          \
  {                                                                                                                                                                      \
    VerifyParameter_8(&function, SVerifyParameter8<T0, T1, T2, T3, T4, T5, T6, T7>());                                                                                   \
    InitParameter_8(t0, t1, t2, t3, t4, t5, t6, t7, FUNCTION_PTR_TYPE(function));                                                                                        \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>                                          \
  inline packet ## type::packet ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8)                                                                   \
  {                                                                                                                                                                      \
    VerifyParameter_9(&function, SVerifyParameter9<T0, T1, T2, T3, T4, T5, T6, T7, T8>());                                                                               \
    InitParameter_9(t0, t1, t2, t3, t4, t5, t6, t7, t8, FUNCTION_PTR_TYPE(function));                                                                                    \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>                             \
  inline packet ## type::packet ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9)                                                            \
  {                                                                                                                                                                      \
    VerifyParameter_10(&function, SVerifyParameter10<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9>());                                                                         \
    InitParameter_10(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, FUNCTION_PTR_TYPE(function));                                                                               \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>               \
  inline packet ## type::packet ## type(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10)                                                   \
  {                                                                                                                                                                      \
    VerifyParameter_11(&function, SVerifyParameter11<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>());                                                                    \
    InitParameter_11(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, FUNCTION_PTR_TYPE(function));                                                                          \
    SetupCommonJob();                                                                                                                                                    \
  }                                                                                                                                                                      \
  /**********************************************************************/                                                                                               \
  /* Implementation of the InitParameter functions */                                                                                                                    \
  /* Functions to initialize the parameters - member function overloads */                                                                                               \
  inline void packet ## type::InitParameter_0(const MemberFunctionPtrTrait &)                                                                                            \
  {                                                                                                                                                                      \
    typedef SParams0<void*> SParameterType;                                                                                                                              \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType();                                                                                                                            \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0>                                                                                                                                                  \
  inline void packet ## type::InitParameter_1(T0 t0, const MemberFunctionPtrTrait &)                                                                                     \
  {                                                                                                                                                                      \
    typedef SParams1<void*, T0> SParameterType;                                                                                                                          \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0);                                                                                                                          \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1>                                                                                                                                     \
  inline void packet ## type::InitParameter_2(T0 t0, T1 t1, const MemberFunctionPtrTrait &)                                                                              \
  {                                                                                                                                                                      \
    typedef SParams2<void*, T0, T1> SParameterType;                                                                                                                      \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1);                                                                                                                      \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2>                                                                                                                        \
  inline void packet ## type::InitParameter_3(T0 t0, T1 t1, T2 t2, const MemberFunctionPtrTrait &)                                                                       \
  {                                                                                                                                                                      \
    typedef SParams3<void*, T0, T1, T2> SParameterType;                                                                                                                  \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2);                                                                                                                  \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3>                                                                                                           \
  inline void packet ## type::InitParameter_4(T0 t0, T1 t1, T2 t2, T3 t3, const MemberFunctionPtrTrait &)                                                                \
  {                                                                                                                                                                      \
    typedef SParams4<void*, T0, T1, T2, T3> SParameterType;                                                                                                              \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3);                                                                                                              \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4>                                                                                              \
  inline void packet ## type::InitParameter_5(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, const MemberFunctionPtrTrait &)                                                         \
  {                                                                                                                                                                      \
    typedef SParams5<void*, T0, T1, T2, T3, T4> SParameterType;                                                                                                          \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4);                                                                                                          \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>                                                                                 \
  inline void packet ## type::InitParameter_6(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, const MemberFunctionPtrTrait &)                                                  \
  {                                                                                                                                                                      \
    typedef SParams6<void*, T0, T1, T2, T3, T4, T5> SParameterType;                                                                                                      \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5);                                                                                                      \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>                                                                    \
  inline void packet ## type::InitParameter_7(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, const MemberFunctionPtrTrait &)                                           \
  {                                                                                                                                                                      \
    typedef SParams7<void*, T0, T1, T2, T3, T4, T5, T6> SParameterType;                                                                                                  \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5, t6);                                                                                                  \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>                                                       \
  inline void packet ## type::InitParameter_8(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, const MemberFunctionPtrTrait &)                                    \
  {                                                                                                                                                                      \
    typedef SParams8<void*, T0, T1, T2, T3, T4, T5, T6, T7> SParameterType;                                                                                              \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5, t6, t7);                                                                                              \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>                                          \
  inline void packet ## type::InitParameter_9(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, const MemberFunctionPtrTrait &)                             \
  {                                                                                                                                                                      \
    typedef SParams9<void*, T0, T1, T2, T3, T4, T5, T6, T7, T8> SParameterType;                                                                                          \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5, t6, t7, t8);                                                                                          \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>                             \
  inline void packet ## type::InitParameter_10(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, const MemberFunctionPtrTrait &)                     \
  {                                                                                                                                                                      \
    typedef SParams10<void*, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9> SParameterType;                                                                                     \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9);                                                                                      \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>               \
  inline void packet ## type::InitParameter_11(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10, const MemberFunctionPtrTrait &)            \
  {                                                                                                                                                                      \
    typedef SParams11<void*, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10> SParameterType;                                                                                \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10);                                                                                 \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
                                                                                                                                                                         \
  /* Functions to initialize the parameters - free function overloads */                                                                                                 \
  inline void packet ## type::InitParameter_0(const FreeFunctionPtrTrait &)                                                                                              \
  {                                                                                                                                                                      \
    typedef SParams0<SNullType> SParameterType;                                                                                                                          \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType();                                                                                                                            \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0>                                                                                                                                                  \
  inline void packet ## type::InitParameter_1(T0 t0, const FreeFunctionPtrTrait &)                                                                                       \
  {                                                                                                                                                                      \
    typedef SParams1<SNullType, T0> SParameterType;                                                                                                                      \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0);                                                                                                                          \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1>                                                                                                                                     \
  inline void packet ## type::InitParameter_2(T0 t0, T1 t1, const FreeFunctionPtrTrait &)                                                                                \
  {                                                                                                                                                                      \
    typedef SParams2<SNullType, T0, T1> SParameterType;                                                                                                                  \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1);                                                                                                                      \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2>                                                                                                                        \
  inline void packet ## type::InitParameter_3(T0 t0, T1 t1, T2 t2, const FreeFunctionPtrTrait &)                                                                         \
  {                                                                                                                                                                      \
    typedef SParams3<SNullType, T0, T1, T2> SParameterType;                                                                                                              \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2);                                                                                                                  \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3>                                                                                                           \
  inline void packet ## type::InitParameter_4(T0 t0, T1 t1, T2 t2, T3 t3, const FreeFunctionPtrTrait &)                                                                  \
  {                                                                                                                                                                      \
    typedef SParams4<SNullType, T0, T1, T2, T3> SParameterType;                                                                                                          \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3);                                                                                                              \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4>                                                                                              \
  inline void packet ## type::InitParameter_5(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, const FreeFunctionPtrTrait &)                                                           \
  {                                                                                                                                                                      \
    typedef SParams5<SNullType, T0, T1, T2, T3, T4> SParameterType;                                                                                                      \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4);                                                                                                          \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>                                                                                 \
  inline void packet ## type::InitParameter_6(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, const FreeFunctionPtrTrait &)                                                    \
  {                                                                                                                                                                      \
    typedef SParams6<SNullType, T0, T1, T2, T3, T4, T5> SParameterType;                                                                                                  \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5);                                                                                                      \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>                                                                    \
  inline void packet ## type::InitParameter_7(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, const FreeFunctionPtrTrait &)                                             \
  {                                                                                                                                                                      \
    typedef SParams7<SNullType, T0, T1, T2, T3, T4, T5, T6> SParameterType;                                                                                              \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5, t6);                                                                                                  \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>                                                       \
  inline void packet ## type::InitParameter_8(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, const FreeFunctionPtrTrait &)                                      \
  {                                                                                                                                                                      \
    typedef SParams8<SNullType, T0, T1, T2, T3, T4, T5, T6, T7> SParameterType;                                                                                          \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5, t6, t7);                                                                                              \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>                                          \
  inline void packet ## type::InitParameter_9(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, const FreeFunctionPtrTrait &)                               \
  {                                                                                                                                                                      \
    typedef SParams9<SNullType, T0, T1, T2, T3, T4, T5, T6, T7, T8> SParameterType;                                                                                      \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5, t6, t7, t8);                                                                                          \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>                             \
  inline void packet ## type::InitParameter_10(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, const FreeFunctionPtrTrait &)                       \
  {                                                                                                                                                                      \
    typedef SParams10<SNullType, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9> SParameterType;                                                                                 \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9);                                                                                      \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
                                                                                                                                                                         \
  template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>               \
  inline void packet ## type::InitParameter_11(T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10, const FreeFunctionPtrTrait &)              \
  {                                                                                                                                                                      \
    typedef SParams11<SNullType, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10> SParameterType;                                                                            \
    STATIC_CHECK(sizeof(SParameterType) <= SInfoBlock::scAvailParamSize, JOB_PARAMETER_NEED_MORE_THAN_80_BYTES);                                                         \
    STATIC_CHECK(sizeof(m_ParameterStorage) == SInfoBlock::scAvailParamSize, JOB_PARAMETER_STORAGE_DIFFERES_BETWEEN_SGENERICJOB_AND_JOBMANAGER_QUEUE);                   \
    new(m_ParameterStorage) SParameterType(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10);                                                                                 \
    MEMORY_RW_REORDERING_BARRIER;                                                                                                                                        \
    SetParamDataSize(sizeof(SParameterType));                                                                                                                            \
  }                                                                                                                                                                      \
  } /* namespace Detail */                                                                                                                                               \
  } /* namespace JobManager */                                                                                                                                           \
                                                                                                                                                                         \
  /* Provide a convenient macro for client code */                                                                                                                       \
  typedef JobManager::Detail::SGenericJob ## type type;

#endif //IJOBMANAGER_JOBDELEGATOR_H_
