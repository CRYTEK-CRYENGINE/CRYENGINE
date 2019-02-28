// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FIXED_POINT_H
#define __FIXED_POINT_H

#pragma once

#include <CryCore/BitFiddling.h>

#ifdef fabsf
	#undef fabsf
#endif
#ifdef fmodf
	#undef fmodf
#endif
#ifdef sqrtf
	#undef sqrtf
#endif
#ifdef expf
	#undef expf
#endif
#ifdef logf
	#undef logf
#endif
#ifdef powf
	#undef powf
#endif
#ifdef fmodf
	#undef fmodf
#endif

/*
   This file implements a configurable fixed point number type.
   If base type is unsigned, fixed point number will be unsigned too and any signed base operations will be not work properly.
   Only 8..32bit basic types are supported.

   TODO: -When overflow_type is 64bit, shift operations are implemented in software through in the CRT through
    _allshl, _allshr, _alldiv, _allmul - extra call is generated and no optimizations applied.
   -Implement basic trigonometry routines: (arc)sin, (arc)cos, (arc)tan
 */

/*

   Here's a Visual Studio Debugger Visualizer - to be put under the [Visualizer] section in your autoexp.dat

   ;------------------------------------------------------------------------------
   ; fixed_t
   ;------------------------------------------------------------------------------
   fixed_t<*,*> {
    preview (
   #([$e.v / (float)fixed_t<$T1,$T2>::fractional_bitcount, f])
    )
   }

 */

#ifdef _DEBUG
	#define FIXED_POINT_CHECK_CONSISTENCY
#endif

namespace CryFixedPoint
{
//! Rounding Modes supported by fixed_t.
enum ERoundingModes
{
	eRM_None = 0,     //!< Similar to truncation, but uses >> in multiply (results vary for negative numbers).
	eRM_Truncate,     //!< Truncation rounding (or round towards zero).
	eRM_Nearest,      //!< Classic "round to nearest" (or round away from zero).
	eRM_Last,         //!< Internal usage only.
};

template<ERoundingModes roundingMode>
struct SelectorRoundingMode
{
	SelectorRoundingMode(){};
};

template<typename Ty>
struct overflow_type
{
};

template<>
struct overflow_type<char>
{
	typedef short type;
};

template<>
struct overflow_type<unsigned char>
{
	typedef unsigned short type;
};

template<>
struct overflow_type<short>
{
	typedef int type;
};

template<>
struct overflow_type<unsigned short>
{
	typedef unsigned int type;
};

template<>
struct overflow_type<int>
{
	typedef int64 type;
};

template<>
struct overflow_type<unsigned int>
{
	typedef uint64 type;
};

template<typename Ty>
struct unsigned_overflow_type
{
};

template<>
struct unsigned_overflow_type<char>
{
	typedef unsigned short type;
	enum { bit_size = sizeof(type) * 8, };
};

template<>
struct unsigned_overflow_type<unsigned char>
{
	typedef unsigned short type;
	enum { bit_size = sizeof(type) * 8, };
};

template<>
struct unsigned_overflow_type<short>
{
	typedef unsigned int type;
	enum { bit_size = sizeof(type) * 8, };
};

template<>
struct unsigned_overflow_type<unsigned short>
{
	typedef unsigned int type;
	enum { bit_size = sizeof(type) * 8, };
};

template<>
struct unsigned_overflow_type<int>
{
	typedef uint64 type;
	enum { bit_size = sizeof(type) * 8, };
};

template<>
struct unsigned_overflow_type<unsigned int>
{
	typedef uint64 type;
	enum { bit_size = sizeof(type) * 8, };
};
}

template<bool IsTrue>
struct Selector {};

template<>
struct Selector<true> {};
template<>
struct Selector<false> {};

#define DEFAULT_ROUNDING_POLICY CryFixedPoint::eRM_Truncate
const static CryFixedPoint::SelectorRoundingMode<DEFAULT_ROUNDING_POLICY> kDefaultRoundingPolicy;

template<typename BaseType, size_t IntegerBitCount>
struct fixed_t
{

	typedef BaseType                                                         value_type;
	typedef typename CryFixedPoint::overflow_type<value_type>::type          overflow_type;
	typedef typename CryFixedPoint::unsigned_overflow_type<value_type>::type unsigned_overflow_type;

	enum { value_size = sizeof(value_type), };
	enum { bit_size = sizeof(value_type) * 8, };
	enum { is_signed = std::numeric_limits<value_type>::is_signed ? 1 : 0, };
	enum { integer_bitcount = IntegerBitCount, };
	enum { fractional_bitcount = bit_size - integer_bitcount - is_signed, };
	enum { integer_scale = 1 << fractional_bitcount, };
	enum { half_unit = integer_scale >> 1, };
	enum { fractional_bitmask = integer_scale - 1, };

	typedef Selector<is_signed == 1> IsSignedSelector;

	inline fixed_t()
	{
	}

	inline fixed_t(const fixed_t& other)
		: v(other.v)
	{
	}

	template<typename BaseTypeO, size_t IntegerBitCountO>
	void ConstructorHelper(const fixed_t<BaseTypeO, IntegerBitCountO>& other, const Selector<true>&)
	{
		v = other.get();
		v <<= static_cast<unsigned>(fractional_bitcount) - static_cast<unsigned>(fixed_t<BaseTypeO, IntegerBitCountO>::fractional_bitcount);
	}

	template<typename BaseTypeO, size_t IntegerBitCountO>
	void ConstructorHelper(const fixed_t<BaseTypeO, IntegerBitCountO>& other, const Selector<false>&)
	{
		BaseTypeO r = other.get();
		r >>= static_cast<unsigned>(fixed_t<BaseTypeO, IntegerBitCountO>::fractional_bitcount - fractional_bitcount);
		v = r;
	}

	template<typename BaseTypeO, size_t IntegerBitCountO>
	inline explicit fixed_t(const fixed_t<BaseTypeO, IntegerBitCountO>& other)
	{
		ConstructorHelper(other, Selector<static_cast<unsigned>(fractional_bitcount) >= static_cast<unsigned>(fixed_t<BaseTypeO, IntegerBitCountO>::fractional_bitcount)>());
	}

	inline fixed_t(const int& value)
		: v(value << fractional_bitcount)
	{
	}

	inline fixed_t(const unsigned int& value)
		: v(value << fractional_bitcount)
	{
	}

	inline explicit fixed_t(const float& value)
		: v((value_type)(value * integer_scale + (value >= 0.0f ? 0.5f : -0.5f)))
	{
	}

	inline explicit fixed_t(const double& value)
		: v((value_type)(value * integer_scale + (value >= 0.0 ? 0.5 : -0.5)))
	{
	}

	inline explicit fixed_t(const bool& value)
		: v((value_type)(value * integer_scale))
	{
	}

	inline fixed_t operator-() const
	{
		fixed_t r;
		r.v = -v;

		return r;
	}

	inline fixed_t& operator+=(const fixed_t& other)
	{
		v += other.v;
		return *this;
	}

	inline fixed_t& operator-=(const fixed_t& other)
	{
		v -= other.v;
		return *this;
	}

	inline fixed_t& operator*=(const fixed_t& other)
	{
		v = rounded_multiplication(v, other.v);

		return *this;
	}

	inline fixed_t& operator/=(const fixed_t& other)
	{
		CRY_ASSERT_MESSAGE(other.v != 0, "ERROR: Divide by ZERO!");

		v = rounded_division(v, other.v);
		return *this;
	}

	inline fixed_t operator+(const fixed_t& other) const
	{
		fixed_t r(*this);
		r += other;
		return r;
	}

	inline fixed_t operator-(const fixed_t& other) const
	{
		fixed_t r(*this);
		r -= other;
		return r;
	}

	inline fixed_t operator*(const fixed_t& other) const
	{
		fixed_t r(*this);
		r *= other;
		return r;
	}

	inline fixed_t operator/(const fixed_t& other) const
	{
		fixed_t r(*this);
		r /= other;
		return r;
	}

	inline bool operator<(fixed_t other) const
	{
		return v < other.v;
	}

	inline bool operator<=(fixed_t other) const
	{
		return v <= other.v;
	}

	inline bool operator>(fixed_t other) const
	{
		return v > other.v;
	}

	inline bool operator>=(fixed_t other) const
	{
		return v >= other.v;
	}

	inline bool operator==(fixed_t other) const
	{
		return v == other.v;
	}

	inline bool operator!=(fixed_t other) const
	{
		return v != other.v;
	}

	inline fixed_t& operator++()
	{
		v += integer_scale;
		return *this;
	}

	inline fixed_t operator++(int)
	{
		fixed_t tmp(*this);
		operator++();
		return tmp;
	}

	inline fixed_t& operator--()
	{
		v -= integer_scale;
		return *this;
	}

	inline fixed_t operator--(int)
	{
		fixed_t tmp(*this);
		operator--();
		return tmp;
	}

	inline fixed_t& operator=(const fixed_t& other)
	{
		v = other.v;
		return *this;
	}

	inline fixed_t& operator=(const float& other)
	{
		fixed_t(other).swap(*this);
		return *this;
	}

	inline fixed_t& operator=(const double& other)
	{
		fixed_t(other).swap(*this);
		return *this;
	}

	template<typename Ty>
	inline fixed_t& operator=(const Ty& other)
	{
		fixed_t(other).swap(*this);
		return *this;
	}

	inline unsigned_overflow_type sqr() const
	{
		return (unsigned_overflow_type(v) * v) >> fractional_bitcount;
	}

	inline float as_float() const
	{
		return v / (float)integer_scale;
	}

	inline double as_double() const
	{
		return v / (double)integer_scale;
	}

	inline int as_int() const
	{
		return v >> fractional_bitcount;
	}

	inline unsigned int as_uint() const
	{
		return v >> fractional_bitcount;
	}

	inline value_type get() const
	{
		return v;
	}

	inline void set(value_type value)
	{
		v = value;
	}

	inline void swap(fixed_t& other)
	{
		std::swap(v, other.v);
	}

	inline static fixed_t max()
	{
		fixed_t v;
		v.set(std::numeric_limits<value_type>::max());
		return v;
	}

	inline static fixed_t min()
	{
		fixed_t v;
		v.set(std::numeric_limits<value_type>::min());
		return v;
	}

	inline static fixed_t epsilon()
	{
		fixed_t v;
		value_type epsilon = std::numeric_limits<value_type>::is_integer ? 1 : std::numeric_limits<value_type>::epsilon();
		v.set(epsilon);
		return v;
	}

	inline static fixed_t fraction(int num, int denom)
	{
		fixed_t v;
		v.set((overflow_type(num) << (fractional_bitcount + fractional_bitcount))
		      / (overflow_type(denom) << fractional_bitcount));
		return v;
	}

	// Utility functions for the overflow_type used for this template.

	inline static float as_float(const overflow_type& x)
	{
		return x / (float)integer_scale;
	}

	inline static double as_double(const overflow_type& x)
	{
		return x / (double)integer_scale;
	}

	inline static int as_int(const overflow_type& x)
	{
		return x >> fractional_bitcount;
	}

	inline static unsigned int as_uint(const overflow_type& x)
	{
		return x >> fractional_bitcount;
	}

	inline static fixed_t sqrtf(const unsigned_overflow_type& x)
	{
		unsigned_overflow_type root = 0;
		unsigned_overflow_type remHi = 0;
		unsigned_overflow_type remLo = x << (fractional_bitcount & 1);
		unsigned_overflow_type count = (CryFixedPoint::unsigned_overflow_type<value_type>::bit_size >> 1) + (fractional_bitcount >> 1) - ((fractional_bitcount + 1) & 1);

		do
		{
			remHi = (remHi << 2) | (remLo >> (CryFixedPoint::unsigned_overflow_type<value_type>::bit_size - 2));
			remLo <<= 2;
			root <<= 1;
			unsigned_overflow_type div = (root << 1) + 1;
			if (remHi >= div)
			{
				remHi -= div;
				root += 1;
			}
		}
		while (count--);

		fixed_t r;
		r.set(static_cast<value_type>(root >> (fractional_bitcount & 1)));

		return r;
	}

	AUTO_STRUCT_INFO;

protected:
	// Multiplication helper functions
	inline value_type rounded_multiplication(const value_type& a, const value_type& b)
	{
		return rounded_multiplication(a, b, kDefaultRoundingPolicy);
	}

	inline value_type rounded_multiplication(const value_type& a, const value_type& b, const CryFixedPoint::SelectorRoundingMode<CryFixedPoint::eRM_None>&)
	{
		const value_type product = static_cast<value_type>((overflow_type(a) * b) >> fractional_bitcount);

#ifdef FIXED_POINT_CHECK_CONSISTENCY
		fixed_t expectedResult;
		expectedResult.set(product);
		CheckIfMultiplicationIsCorrect(a, b, expectedResult, CryFixedPoint::eRM_None);
#endif

		return product;
	}

	inline value_type rounded_multiplication(const value_type& a, const value_type& b, const CryFixedPoint::SelectorRoundingMode<CryFixedPoint::eRM_Nearest>&)
	{
		overflow_type product = overflow_type(a) * b;

		if (isNegative(product))
		{
			product -= half_unit;
			product += integer_scale - 1;
		}
		else
		{
			product += half_unit;
		}
		product >>= fractional_bitcount;

#ifdef FIXED_POINT_CHECK_CONSISTENCY
		fixed_t expectedResult;
		expectedResult.set(static_cast<value_type>(product));
		CheckIfMultiplicationIsCorrect(a, b, expectedResult, CryFixedPoint::eRM_Nearest);
#endif

		return static_cast<value_type>(product);
	}

	inline value_type rounded_multiplication(const value_type& a, const value_type& b, const CryFixedPoint::SelectorRoundingMode<CryFixedPoint::eRM_Truncate>&)
	{
		overflow_type product = overflow_type(a) * b;

		if (isNegative(product))
		{
			product += integer_scale - 1;
		}
		product >>= fractional_bitcount;

#ifdef FIXED_POINT_CHECK_CONSISTENCY
		fixed_t expectedResult;
		expectedResult.set(static_cast<value_type>(product));
		CheckIfMultiplicationIsCorrect(a, b, expectedResult, CryFixedPoint::eRM_Truncate);
#endif

		return static_cast<value_type>(product);
	}

	// Division helper functions
	inline value_type rounded_division(const value_type& a, const value_type& b)
	{
		return rounded_division(a, b, kDefaultRoundingPolicy);
	}

	inline value_type general_rounded_division(const value_type& a, const value_type& b, const CryFixedPoint::ERoundingModes roundingMode)
	{
		CRY_ASSERT_MESSAGE(b != 0, "ERROR: Divide by ZERO!");

		overflow_type quotient = (overflow_type(a) << fractional_bitcount) / b;

#ifdef FIXED_POINT_CHECK_CONSISTENCY
		fixed_t expectedResult;
		expectedResult.set(static_cast<value_type>(quotient));
		CheckIfDivisionIsCorrect(a, b, expectedResult, roundingMode);
#endif

		return static_cast<value_type>(quotient);
	}

	inline value_type rounded_division(const value_type& a, const value_type& b, const CryFixedPoint::SelectorRoundingMode<CryFixedPoint::eRM_None>&)
	{
		return general_rounded_division(a, b, CryFixedPoint::eRM_None);
	}

	inline value_type rounded_division(const value_type& a, const value_type& b, const CryFixedPoint::SelectorRoundingMode<CryFixedPoint::eRM_Truncate>&)
	{
		return general_rounded_division(a, b, CryFixedPoint::eRM_Truncate);
	}

	inline value_type rounded_division(const value_type& a, const value_type& b, const CryFixedPoint::SelectorRoundingMode<CryFixedPoint::eRM_Nearest>&)
	{
		CRY_ASSERT_MESSAGE(b != 0, "ERROR: Divide by ZERO!");

		overflow_type dividend = overflow_type(a);
		overflow_type divisor = overflow_type(b);

		const int nRoundingShift = 1;
		const int nShiftBits = fractional_bitcount + nRoundingShift;
		overflow_type dividend_scaled = dividend << nShiftBits;

		overflow_type rounding = fpAbs(divisor);
		if (isNegative(dividend_scaled))
		{
			dividend_scaled -= rounding;
		}
		else
		{
			dividend_scaled += rounding;
		}

		overflow_type quotient = dividend_scaled / (divisor << nRoundingShift);

#ifdef FIXED_POINT_CHECK_CONSISTENCY
		fixed_t expectedResult;
		expectedResult.set(static_cast<value_type>(quotient));
		CheckIfDivisionIsCorrect(a, b, expectedResult, CryFixedPoint::eRM_Nearest);
#endif

		return static_cast<value_type>(quotient);
	}

	// -------------------------------------------------------------------

	inline static bool isNegative(const value_type& result)
	{
		return isNegative(result, IsSignedSelector());
	}

	inline static bool isNegative(const value_type& result, const Selector<true>&)
	{
		return result < 0;
	}

	inline static bool isNegative(const value_type& result, const Selector<false>&)
	{
		return false;
	}

	inline static bool isNegative(const overflow_type& result)
	{
		return isNegative(result, IsSignedSelector());
	}

	inline static bool isNegative(const overflow_type& result, const Selector<true>&)
	{
		return result < 0;
	}

	inline static bool isNegative(const overflow_type& result, const Selector<false>&)
	{
		return false;
	}

	// -------------------------------------------------------------------

	inline static value_type fpNegate(const value_type& result)
	{
		return fpNegate(result, IsSignedSelector());
	}

	inline static value_type fpNegate(const value_type& result, const Selector<true>&)
	{
		return -result;
	}

	inline static value_type fpNegate(const value_type& result, const Selector<false>&)
	{
		return result;
	}

	inline static overflow_type fpNegate(const overflow_type& result)
	{
		return fpNegate(result, IsSignedSelector());
	}

	inline static overflow_type fpNegate(const overflow_type& result, const Selector<true>&)
	{
		return -result;
	}

	inline static overflow_type fpNegate(const overflow_type& result, const Selector<false>&)
	{
		return result;
	}

	//// -------------------------------------------------------------------

	inline static value_type fpAbs(const value_type& result)
	{
		return fpAbs(result, IsSignedSelector());
	}

	inline static value_type fpAbs(const value_type& result, const Selector<true>&)
	{
		return (result >= 0 ? result : -result);
	}

	inline static value_type fpAbs(const value_type& result, const Selector<false>&)
	{
		return result;
	}

	inline static overflow_type fpAbs(const overflow_type& result)
	{
		return fpAbs(result, IsSignedSelector());
	}

	inline static overflow_type fpAbs(const overflow_type& result, const Selector<true>&)
	{
		return (result >= 0 ? result : -result);
	}

	inline static overflow_type fpAbs(const overflow_type& result, const Selector<false>&)
	{
		return result;
	}

	///////////////////////////////////////////

#ifdef FIXED_POINT_CHECK_CONSISTENCY

	void CheckIfMultiplicationIsCorrect(const value_type& a, const value_type& b, const fixed_t& expectedResult, const CryFixedPoint::ERoundingModes roundingMode)
	{
		// Slow but precise multiplication calculation using only positive numbers
		const bool a_isNeg = isNegative(a);
		const bool b_isNeg = isNegative(b);
		overflow_type a_pos = fpAbs(overflow_type(a));
		overflow_type b_pos = fpAbs(overflow_type(b));
		overflow_type product2 = a_pos * b_pos;

		// perform any required rounding
		if (roundingMode == CryFixedPoint::eRM_Nearest)
		{
			product2 += half_unit;
		}

		// shift result back into normal scale
		product2 >>= fractional_bitcount;

		// if product was supposed to be negative, set sign now.
		const bool sign = (a_isNeg ^ b_isNeg);
		if (is_signed && sign)
		{
			product2 = fpNegate(product2);
		}

		// compare alternative math with actual results
		fixed_t product_from_alt;
		product_from_alt.set(static_cast<value_type>(product2));

		CRY_ASSERT_TRACE(expectedResult == product_from_alt, ("ERROR: %.6f / %.6f should equal %.6f, but instead it is %.6f",
		                                                      a / (double)integer_scale, b / (double)integer_scale,
		                                                      product_from_alt.as_double(), expectedResult.as_double()));
	}

	void CheckIfDivisionIsCorrect(const value_type& dividend, const value_type& divisor, const fixed_t& expectedResult, const CryFixedPoint::ERoundingModes roundingMode)
	{
		// compute quotient using positive numbers
		const bool dividend_isNeg = isNegative(dividend);
		const bool divisor_isNeg = isNegative(divisor);
		overflow_type dividend_pos = fpAbs(dividend);
		overflow_type divisor_pos = fpAbs(divisor);
		overflow_type quotient2 = (dividend_pos << fractional_bitcount) / divisor_pos;

		// perform any required rounding
		if (roundingMode == CryFixedPoint::eRM_Nearest)
		{
			// If twice the remainder is >= the divisor, quotient must be rounded.
			// This is more accurate than comparing remainder with half the dividend
			// since it ensures that we will not lose any precision on the right.
			overflow_type remainder = (dividend_pos << fractional_bitcount) % divisor_pos;
			if ((remainder << 1) >= divisor_pos)
			{
				quotient2 += overflow_type(1);
			}
		}

		// if quotient was supposed to be negative, set sign now.
		const bool sign = (dividend_isNeg ^ divisor_isNeg);
		if (is_signed && sign)
		{
			quotient2 = fpNegate(quotient2);
		}

		// compare alternative math with actual results
		fixed_t quotient_from_alt;
		quotient_from_alt.set(static_cast<value_type>(quotient2));

		CRY_ASSERT_TRACE(expectedResult == quotient_from_alt,
		                 ("ERROR: %.6f / %.6f should equal %.6f, but instead it is %.6f",
		                  dividend / (double)integer_scale, divisor / (double)integer_scale,
		                  quotient_from_alt.as_double(), expectedResult.as_double()));
	}

#endif

	///////////////////////////////////////////

	value_type v;
};

template<typename BaseType, size_t IntegerBitCount>
inline fixed_t<BaseType, IntegerBitCount> floor(const fixed_t<BaseType, IntegerBitCount>& x)
{
	fixed_t<BaseType, IntegerBitCount> r;
	r.set(x.get() & ~(fixed_t<BaseType, IntegerBitCount>::fractional_bitmask));

	return r;
}

template<typename BaseType, size_t IntegerBitCount>
inline fixed_t<BaseType, IntegerBitCount> ceil(const fixed_t<BaseType, IntegerBitCount>& x)
{
	fixed_t<BaseType, IntegerBitCount> r;
	r.set(x.get() & ~(fixed_t<BaseType, IntegerBitCount>::fractional_bitmask));
	if (x.get() & (fixed_t<BaseType, IntegerBitCount>::fractional_bitmask))
		r += fixed_t<BaseType, IntegerBitCount>(1);

	return r;
}

//! Disable fabsf for unsigned base types to avoid sign warning.
template<typename BaseType, size_t IntegerBitCount>
inline fixed_t<BaseType, IntegerBitCount> fabsf(const fixed_t<BaseType, IntegerBitCount>& x)
{
	typedef fixed_t<BaseType, IntegerBitCount> type;

	const BaseType mask = x.get() >> (type::bit_size - 1);

	type r;
	r.set((x.get() + mask) ^ mask);

	return r;
}

template<typename BaseType, size_t IntegerBitCount>
inline fixed_t<BaseType, IntegerBitCount> fmodf(const fixed_t<BaseType, IntegerBitCount>& x,
                                                const fixed_t<BaseType, IntegerBitCount>& y)
{
	fixed_t<BaseType, IntegerBitCount> r;
	r.set(x.get() % y.get());
	return r;
}

template<typename BaseType, size_t IntegerBitCount>
inline fixed_t<BaseType, IntegerBitCount> sqrtf(const fixed_t<BaseType, IntegerBitCount>& x)
{
	typedef fixed_t<BaseType, IntegerBitCount>                    type;
	typedef typename CryFixedPoint::overflow_type<BaseType>::type overflow_type;

	overflow_type root = 0;
	overflow_type remHi = 0;
	overflow_type remLo = overflow_type(x.get()) << (type::fractional_bitcount & 1);
	overflow_type count = (type::bit_size >> 1) + (type::fractional_bitcount >> 1) - ((type::fractional_bitcount + 1) & 1);

	do
	{
		remHi = (remHi << 2) | (remLo >> (type::bit_size - 2));
		remLo <<= 2;
		remLo &= (overflow_type(1) << type::bit_size) - 1;
		root <<= 1;
		root &= (overflow_type(1) << type::bit_size) - 1;
		overflow_type div = (root << 1) + 1;
		if (remHi >= div)
		{
			remHi -= div;
			remHi &= (overflow_type(1) << type::bit_size) - 1;
			root += 1;
		}
	}
	while (count--);

	type r;
	r.set(static_cast<typename type::value_type>(root >> (type::fractional_bitcount & 1)));

	return r;
}

template<typename BaseType, size_t IntegerBitCount>
inline fixed_t<BaseType, IntegerBitCount> isqrtf(const fixed_t<BaseType, IntegerBitCount>& x)
{
	if (x > fixed_t<BaseType, IntegerBitCount>(0))
	{
		fixed_t<BaseType, IntegerBitCount> r(1);
		r /= sqrtf(x);

		return r;
	}

	return fixed_t<BaseType, IntegerBitCount>(0);
}

//! \see https://en.wikipedia.org/wiki/Taylor_series
template<typename BaseType, size_t IntegerBitCount>
inline fixed_t<BaseType, IntegerBitCount> expf(const fixed_t<BaseType, IntegerBitCount>& x)
{
	const fixed_t<BaseType, IntegerBitCount> one(1);
	fixed_t<BaseType, IntegerBitCount> w(one);
	fixed_t<BaseType, IntegerBitCount> y(one);
	fixed_t<BaseType, IntegerBitCount> n(one);

	for (; w != 0; ++n)
		y += (w *= x / n);

	return y;
}

template<typename BaseType, size_t IntegerBitCount>
inline fixed_t<BaseType, IntegerBitCount> logf(const fixed_t<BaseType, IntegerBitCount>& x)
{
	fixed_t<BaseType, IntegerBitCount> b((x - 1) / (x + 1));
	fixed_t<BaseType, IntegerBitCount> w;
	fixed_t<BaseType, IntegerBitCount> y(b);
	fixed_t<BaseType, IntegerBitCount> z(b);
	fixed_t<BaseType, IntegerBitCount> n(3);

	b *= b;

	for (; w != 0; n += 2)
	{
		z *= b;
		w = z / n;
		y += w;
	}

	return y + y;
}

template<typename BaseType, size_t IntegerBitCount>
inline fixed_t<BaseType, IntegerBitCount> powf(const fixed_t<BaseType, IntegerBitCount>& x,
                                               const fixed_t<BaseType, IntegerBitCount>& y)
{
	const fixed_t<BaseType, IntegerBitCount> zero(0);

	if (x < zero)
	{
		if (fmodf(y, fixed_t<BaseType, IntegerBitCount>(2)) != zero)
			return -expf((y * logf(-x)));
		else
			return expf((y * logf(-x)));
	}
	else
		return expf(y * logf(x));
}

#endif  // #ifndef __FIXED_POINT_H
