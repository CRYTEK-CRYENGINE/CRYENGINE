// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <algorithm>           // std::swap()
#include <limits>              // std::numeric_limits
#include <type_traits>         // std::make_unsigned
#include <CryCore/BaseTypes.h> // uint32, uint64
#include "Cry_Vector2.h"
#include "Cry_Vector3.h"
#include "Cry_Vector4.h"

namespace CryRandom_Internal
{

template<class R, class T, size_t size>
struct BoundedRandomUint
{
	static_assert(std::numeric_limits<T>::is_integer, "'T' is not an integer value!");
	static_assert(!std::numeric_limits<T>::is_signed, "'T' is not an unsigned value!");
	static_assert(sizeof(T) == size, "sizeof(T) does not match the specified size!");
	static_assert(sizeof(T) <= sizeof(uint32), "sizeof(T) is too big!");

	inline static T Get(R& randomGenerator, const T maxValue)
	{
		const uint32 r = randomGenerator.GenerateUint32();
		// Note that the computation below is biased. An alternative computation
		// (also biased): uint32((uint64)r * ((uint64)maxValue + 1)) >> 32)
		return (T)((uint64)r % ((uint64)maxValue + 1));
	}
};

template<class R, class T>
struct BoundedRandomUint<R, T, 8>
{
	static_assert(std::numeric_limits<T>::is_integer, "'T' is not an integer value!");
	static_assert(!std::numeric_limits<T>::is_signed, "'T' is not an unsigned value!");
	static_assert(sizeof(T) == sizeof(uint64), "Wrong type size!");

	inline static T Get(R& randomGenerator, const T maxValue)
	{
		const uint64 r = randomGenerator.GenerateUint64();
		if (maxValue >= (std::numeric_limits<uint64>::max)())
		{
			return r;
		}
		// Note that the computation below is biased.
		return (T)(r % ((uint64)maxValue + 1));
	}
};

//////////////////////////////////////////////////////////////////////////

template<class R, class T, bool bInteger = std::numeric_limits<T>::is_integer>
struct BoundedRandom;

template<class R, class T>
struct BoundedRandom<R, T, true>
{
	static_assert(std::numeric_limits<T>::is_integer, "'T' is not an integer value!");
	typedef typename std::make_unsigned<T>::type UT;
	static_assert(sizeof(T) == sizeof(UT), "Wrong type size!");
	static_assert(std::numeric_limits<UT>::is_integer, "'UT' is not an integer value!");
	static_assert(!std::numeric_limits<UT>::is_signed, "'UT' is not an unsigned value!");

	inline static T Get(R& randomGenerator, T minValue, T maxValue)
	{
		if (minValue > maxValue)
		{
			std::swap(minValue, maxValue);
		}
		return (T)((UT)minValue + (UT)BoundedRandomUint<R, UT, sizeof(UT)>::Get(randomGenerator, (UT)(maxValue - minValue)));
	}
};

template<class R, class T>
struct BoundedRandom<R, T, false>
{
	static_assert(!std::numeric_limits<T>::is_integer, "'T' must not be an integer value!");

	inline static T Get(R& randomGenerator, const T minValue, const T maxValue)
	{
		return minValue + (maxValue - minValue) * randomGenerator.GenerateFloat();
	}
};

//////////////////////////////////////////////////////////////////////////

template<class R, class VT, class T = typename VT::value_type, size_t componentCount = VT::component_count>
struct BoundedRandomComponentwise;

template<class R, class VT, class T>
struct BoundedRandomComponentwise<R, VT, T, 2>
{
	inline static VT Get(R& randomGenerator, const VT& minValue, const VT& maxValue)
	{
		const T x = BoundedRandom<R, T>::Get(randomGenerator, minValue.x, maxValue.x);
		const T y = BoundedRandom<R, T>::Get(randomGenerator, minValue.y, maxValue.y);
		return VT(x, y);
	}
};

template<class R, class VT, class T>
struct BoundedRandomComponentwise<R, VT, T, 3>
{
	inline static VT Get(R& randomGenerator, const VT& minValue, const VT& maxValue)
	{
		const T x = BoundedRandom<R, T>::Get(randomGenerator, minValue.x, maxValue.x);
		const T y = BoundedRandom<R, T>::Get(randomGenerator, minValue.y, maxValue.y);
		const T z = BoundedRandom<R, T>::Get(randomGenerator, minValue.z, maxValue.z);
		return VT(x, y, z);
	}
};

template<class R, class VT, class T>
struct BoundedRandomComponentwise<R, VT, T, 4>
{
	inline static VT Get(R& randomGenerator, const VT& minValue, const VT& maxValue)
	{
		const T x = BoundedRandom<R, T>::Get(randomGenerator, minValue.x, maxValue.x);
		const T y = BoundedRandom<R, T>::Get(randomGenerator, minValue.y, maxValue.y);
		const T z = BoundedRandom<R, T>::Get(randomGenerator, minValue.z, maxValue.z);
		const T w = BoundedRandom<R, T>::Get(randomGenerator, minValue.w, maxValue.w);
		return VT(x, y, z, w);
	}
};

//////////////////////////////////////////////////////////////////////////

template<class R, class VT>
inline VT GetRandomUnitVector(R& randomGenerator)
{
	typedef typename VT::value_type T;
	static_assert(!std::numeric_limits<T>::is_integer, "'T' must not be an integer value!");

	VT res;
	T lenSquared;

	do
	{
		res = BoundedRandomComponentwise<R, VT>::Get(randomGenerator, VT(-1), VT(1));
		lenSquared = res.GetLengthSquared();
	}
	while (lenSquared > 1);

	if (lenSquared >= (std::numeric_limits<T>::min)())
	{
		return res * isqrt_tpl(lenSquared);
	}

	res = VT(ZERO);
	res.x = 1;
	return res;
}

} // namespace CryRandom_Internal
