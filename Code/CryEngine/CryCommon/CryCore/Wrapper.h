// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// Wrapper for basic or any other type. Provides:
// Automatic initialization (0 by default, or any int-convertible value)
// () operator returns value, for method interface

template<class T, int NInit = 0>
struct TProperty
{
	typedef T TValue;

	TProperty()
		: _val(T(NInit)) {}
	explicit TProperty(T val)
		: _val(val) {}

	const T& operator()() const { return _val; }
	T&       operator()()       { return _val; }

protected:
	T _val;
};

// TWrapper extends TProperty by automatically converting to and from value type.
// Can also serve as base class for extending basic types.

template<class T, int NInit = 0>
struct TWrapper : TProperty<T, NInit>
{
	TWrapper()
	{}
	TWrapper(T val)
		: TProperty<T, NInit>(val) {}

	// Automatic conversion to value
	operator const T&() const { return _val; }
	operator T&()  { return _val; }
	bool operator!() const { return !_val; }

protected:
	using TProperty<T, NInit>::_val;
};
