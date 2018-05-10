// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __STLHELPERS_H__
#define __STLHELPERS_H__

#include <functional>

namespace STLHelpers
{
	template <class Type> inline const char* constchar_cast( const Type &type )
	{
		return type;
	}

	template <> inline const char* constchar_cast( const std::string &type )
	{
		return type.c_str();
	}

	template <class Type> struct less_strcmp : public std::binary_function<Type,Type,bool> 
	{
		bool operator()( const Type &left,const Type &right ) const
		{
			return strcmp(constchar_cast(left),constchar_cast(right)) < 0;
		}
	};

	template <class Type> struct less_stricmp : public std::binary_function<Type,Type,bool> 
	{
		bool operator()( const Type &left,const Type &right ) const
		{
			return _stricmp(constchar_cast(left),constchar_cast(right)) < 0;
		}
	};
}

#endif //__STLHELPERS_H__
