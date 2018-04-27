// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   Description:
   Helper macros/methods/classes for boost.
   -------------------------------------------------------------------------
   History:
   - 02:07:2009: Created by Alex McCarthy
*************************************************************************/

#ifndef __BOOST_HELPERS_H__
#define __BOOST_HELPERS_H__
#include <CryCore/Assert/CryAssert.h>

#include <boost/version.hpp>
//////////////////////////////////////////////////////////////////////////
// Boost config
//////////////////////////////////////////////////////////////////////////
// For boost 1_58_0 +
#if BOOST_VERSION >= 105800
	#define BOOST_NO_IOSTREAM
	#define BOOST_NO_RTTI
	#define BOOST_NO_EXCEPTIONS
	#define BOOST_EXCEPTION_DISABLE

// disable noexcept warning inside boost.
	#define BOOST_NO_CXX11_NOEXCEPT

	#ifdef BOOST_NO_EXCEPTIONS
namespace boost
{
template<class E> inline void throw_exception(E const& e)
{
	CryFatalError("Boost threw an exception: %s", e.what());
	while (true)
	{
	}
	;
}
}
	#endif //BOOST_NO_EXCEPTIONS
#endif

#if CRY_PLATFORM_ORBIS
// Boost expects type_info in the global namespace, to import
// it into the std namespace. But Orbis already provides
// typeinfo in the std namespace. Hence import type_info
// into the global namespace, to allow boost to compile
	#include <typeinfo>
using std::type_info;
#endif

#pragma warning(push)
#pragma warning(disable : 4345)
#include <boost/variant.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/find.hpp>
#pragma warning(pop)

#endif // __BOOST_HELPERS_H__
