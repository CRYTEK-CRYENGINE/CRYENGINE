// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <type_traits>
#include <limits>
#include <cmath>

#include "Cry_Math.h"

//--------------------------------------------------------------------------------
// IsValid() overloads for basic types

namespace ValidNumber
{
template<typename T, bool Converted> struct Float
{
	static void SetInvalid(T& val)
	{
		val = std::numeric_limits<T>::signaling_NaN();
	}
	static bool IsValid(T val)
	{
		return crymath::valueisfinite(val);
	}
	static bool IsEquivalent(T a, T b, T e = std::numeric_limits<T>::epsilon())
	{
		float e2 = sqr(e);
		if (e < 0) // Negative epislon denotes a relative comparison
			e2 *= max(sqr(a), sqr(b));
		return sqr(a - b) <= e2;
	}
};

template<typename T, bool Converted> struct Integral
{
	static void SetInvalid(T& val)
	{
		val = std::numeric_limits<T>::max();
	}
	static bool IsValid(T val)
	{
		return true; 
	}
	static bool IsEquivalent(T a, T b, T e = T())
	{
		return sqr(a - b) <= sqr(e);
	}
};

template<typename T> struct Boolean
{
	static void SetInvalid(T& val)
	{
		val = false;
	}
	static bool IsValid(T val)
	{
		return true; 
	}
	static bool IsEquivalent(T a, T b, T)
	{
		return a == b;
	}
};

template<typename T> struct Class
{
	static void SetInvalid(T& val)
	{
	}
	static bool IsValid(const T& val)
	{
		return val.IsValid();
	}
	template<typename E>
	static bool IsEquivalent(const T& a, const T& b, E e)
	{
		return a.IsEquivalent(b, e);
	}
	static bool IsEquivalent(const T& a, const T& b)
	{
		return a.IsEquivalent(b);
	}
};

template<typename T> struct Type
{
	typedef typename std::conditional<
		std::is_floating_point<T>::value, ValidNumber::Float<T, 0>,
		typename std::conditional<
			std::is_same<T, bool>::value, ValidNumber::Boolean<T>,
			typename std::conditional<
				std::is_integral<T>::value || std::is_enum<T>::value, ValidNumber::Integral<T, 0>,
				typename std::conditional<
					std::is_convertible<T, float>::value, ValidNumber::Float<T, 1>,
					typename std::conditional<
						std::is_convertible<T, int>::value, ValidNumber::Integral<T, 1>,
						ValidNumber::Class<T>
					>::type
				>::type
			>::type
		>::type
	>::type type;
};

}

template<typename T>
void SetInvalid(T& val)
{
	ValidNumber::Type<T>::type::SetInvalid(val);
}

template<typename T>
bool IsValid(const T& val)
{
	return ValidNumber::Type<T>::type::IsValid(val);
}

// Alias
template<typename T> bool NumberValid(const T& val)
{
	return IsValid(val); 
}

template<typename T, typename U, typename E> bool IsEquivalent(const T& a, const U& b, E e)
{
	return ValidNumber::Type<T>::type::IsEquivalent(a, b, e);
}
template<typename T, typename U> bool IsEquivalent(const T& a, const U& b)
{
	return ValidNumber::Type<T>::type::IsEquivalent(a, b);
}
