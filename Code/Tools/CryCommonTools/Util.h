// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __UTIL_H__
#define __UTIL_H__

#include <cstring>  // memset()

#ifndef BIT
#define BIT(x) (1 << (x))
#endif


union IntOrPtr
{
	int m_int;
	unsigned int m_uint;
	void* m_pVoid;
	char* m_pChar;

	void setZero()
	{
		memset(this, 0, sizeof(*this));
	}

	bool operator==(const IntOrPtr& a) const
	{
		return memcmp(this, &a, sizeof(*this)) == 0;
	}

	bool operator!=(const IntOrPtr& a) const
	{
		return memcmp(this, &a, sizeof(*this)) != 0;
	}
};


namespace Util
{

	// note: names 'getMin' and 'getMax' (instead of usual 'min' and 'max') are
	// used to avoid conflicts with global and/or user's #define of 'min' and 'max'

	template<class T>
	inline const T& getMin(const T& a, const T& b)
	{
		return (a < b) ? a : b;
	}

	template<class T>
	inline const T& getMax(const T& a, const T& b)
	{
		return (a < b) ? b : a;
	}

	template<class T>
	inline const T& getMin(const T& a, const T& b, const T& c)
	{
		return (a < b)
			? ((a < c) ? a : c)
			: ((b < c) ? b : c);
	}

	template<class T>
	inline const T& getMax(const T& a, const T& b, const T& c)
	{
		return (a < b)
			? ((b < c) ? c : b)
			: ((a < c) ? c : a);
	}


	template<class T>
	inline const T& getClamped(const T& a, const T& a_min, const T& a_max)
	{
		if (a < a_min)
		{
			return a_min;
		}
		if (a_max < a)
		{
			return a_max;
		}
		return a;
	}

	// note: name 'clampMinMax' (instead of usual 'clamp') is used
	// to avoid conflicts with global and/or user's #define of 'clamp'
	template<class T>
	inline void clampMinMax(T& a, const T& a_min, const T& a_max)
	{
		if (a < a_min)
		{
			a = a_min;
		}
		else if (a_max < a)
		{
			a = a_max;
		}
	}

	template<class T>
	inline void clampMin(T& a, const T& a_min)
	{
		if (a < a_min)
		{
			a = a_min;
		}
	}

	template<class T>
	inline void clampMax(T& a, const T& a_max)
	{
		if (a_max < a)
		{
			a = a_max;
		}
	}


	template <class TInteger>
	inline bool isPowerOfTwo(TInteger x)
	{
		return (x & (x - 1)) == 0;
	}


	template <class TInteger>
	inline TInteger getCeiledPowerOfTwo(TInteger x)
	{
		x = x - 1;

#pragma warning(push)
#pragma warning(disable : 4293)
		if (sizeof(TInteger) > 0) x |= x >> 1;
		if (sizeof(TInteger) > 0) x |= x >> 2;
		if (sizeof(TInteger) > 0) x |= x >> 4;
		if (sizeof(TInteger) > 1) x |= x >> 8;
		if (sizeof(TInteger) > 2) x |= x >> 16;
		if (sizeof(TInteger) > 4) x |= x >> 32;
#pragma warning(pop)

		return x + 1;
	}


	template <class TInteger>
	inline TInteger getFlooredPowerOfTwo(TInteger x)
	{
		if (!isPowerOfTwo(x))
		{
			x = getCeiledPowerOfTwo(x) >> 1;
		}

		return x;
	}


	template <class T>
	inline T square(T x)
	{
		return x * x;
	}


	template <class T>
	inline T cube(T x)
	{
		return x * x * x;
	}
} // namespace Util

#endif
