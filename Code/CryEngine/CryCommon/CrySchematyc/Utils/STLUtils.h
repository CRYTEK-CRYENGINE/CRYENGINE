// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Merge with StlUtils.h in CryCommon.

#pragma once

namespace stl
{
template<typename TYPE> struct reverse_adapter
{
	explicit inline reverse_adapter(TYPE& _value)
		: value(_value)
	{}

	inline typename TYPE::reverse_iterator begin()
	{
		return value.rbegin();
	}

	inline typename TYPE::reverse_iterator end()
	{
		return value.rend();
	}

	TYPE& value;
};

template<typename TYPE> struct reverse_adapter<const TYPE>
{
	explicit inline reverse_adapter(const TYPE& _value)
		: value(_value)
	{}

	inline typename TYPE::const_reverse_iterator begin()
	{
		return value.rbegin();
	}

	inline typename TYPE::const_reverse_iterator end()
	{
		return value.rend();
	}

	const TYPE& value;
};

template<typename TYPE> reverse_adapter<TYPE> reverse(TYPE& container)
{
	return reverse_adapter<TYPE>(container);
}

template<typename TYPE> reverse_adapter<const TYPE> reverse(const TYPE& container)
{
	return reverse_adapter<const TYPE>(container);
}
} // stl
