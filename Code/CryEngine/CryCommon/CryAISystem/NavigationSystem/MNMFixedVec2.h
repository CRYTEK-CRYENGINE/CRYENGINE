// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MNM_FIXED_VEC2_H
#define __MNM_FIXED_VEC2_H

#pragma once

#include <CryCore/FixedPoint.h>
#include "MNMFixedVec3.h"

namespace MNM
{
template<typename BaseType, size_t IntegerBitCount>
struct FixedVec2
{
	typedef fixed_t<BaseType, IntegerBitCount> value_type;
	value_type x, y;

	inline FixedVec2()
	{
	}

	inline FixedVec2(const value_type& _x)
		: x(_x)
		, y(_x)
	{
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline FixedVec2(const FixedVec2<OBaseType, OIntegerBitCount>& _x)
		: x(value_type(_x.x))
		, y(value_type(_x.y))
	{
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline explicit FixedVec2(const FixedVec3<OBaseType, OIntegerBitCount>& _x)
		: x(value_type(_x.x))
		, y(value_type(_x.y))
	{
	}

	inline FixedVec2(const value_type& _x, const value_type& _y)
		: x(_x)
		, y(_y)
	{
	}

	inline const value_type& operator[](size_t i) const
	{
		assert(i < 2);
		return ((value_type*)&x)[i];
	}

	inline value_type& operator[](size_t i)
	{
		assert(i < 2);
		return ((value_type*)&x)[i];
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline FixedVec2 operator+(const FixedVec2<OBaseType, OIntegerBitCount>& other) const
	{
		return FixedVec2(x + other.x, y + other.y);
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline FixedVec2 operator-(const FixedVec2<OBaseType, OIntegerBitCount>& other) const
	{
		return FixedVec2(x - other.x, y - other.y);
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline FixedVec2& operator+=(const FixedVec2<OBaseType, OIntegerBitCount>& other)
	{
		x += other.x;
		y += other.y;
		return *this;
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline FixedVec2& operator-=(const FixedVec2<OBaseType, OIntegerBitCount>& other)
	{
		x -= other.x;
		y -= other.y;
		return *this;
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline FixedVec2 operator/(const FixedVec2<OBaseType, OIntegerBitCount>& other) const
	{
		return FixedVec2(x / other.x, y / other.y);
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline FixedVec2 operator*(const FixedVec2<OBaseType, OIntegerBitCount>& other) const
	{
		return FixedVec2(x * other.x, y * other.y);
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline FixedVec2 operator/(const fixed_t<OBaseType, OIntegerBitCount>& value) const
	{
		return FixedVec2(x / value, y / value);
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline FixedVec2 operator*(const fixed_t<OBaseType, OIntegerBitCount>& value) const
	{
		return FixedVec2(x * value, y * value);
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline bool operator==(const FixedVec2<OBaseType, OIntegerBitCount>& other) const
	{
		return (x == other.x) && (y == other.y);
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline bool operator!=(const FixedVec2<OBaseType, OIntegerBitCount>& other) const
	{
		return (x != other.x) || (y != other.y);
	}

	inline value_type dot(const FixedVec2& other) const
	{
		return x * other.x + y * other.y;
	}

	inline value_type cross(const FixedVec2& other) const
	{
		return x * other.y - y * other.x;
	}

	inline value_type len() const
	{
		return sqrtf(lenSq());
	}

	template<typename BaseTypeO, size_t IntegerBitCountO>
	inline fixed_t<BaseTypeO, IntegerBitCountO> len() const
	{
		return sqrtf(lenSq<BaseTypeO, IntegerBitCountO>());
	}

	inline value_type lenSq() const
	{
		return x * x + y * y;
	}

	template<typename BaseTypeO, size_t IntegerBitCountO>
	inline fixed_t<BaseTypeO, IntegerBitCountO> lenSq() const
	{
		return fixed_t<BaseTypeO, IntegerBitCountO>(x) * fixed_t<BaseTypeO, IntegerBitCountO>(x)
		       + fixed_t<BaseTypeO, IntegerBitCountO>(y) * fixed_t<BaseTypeO, IntegerBitCountO>(y);
	}

	inline FixedVec2 abs()
	{
		return FixedVec2(fabsf(x), fabsf(y));
	}

	inline static FixedVec2 minimize(const FixedVec2& a, const FixedVec2& b)
	{
		return FixedVec2(std::min(a.x, b.x), std::min(a.y, b.y));
	}

	inline static FixedVec2 minimize(const FixedVec2& a, const FixedVec2& b, const FixedVec2& c)
	{
		return FixedVec2(std::min(std::min(a.x, b.x), c.x), std::min(std::min(a.y, b.y), c.y));
	}

	inline static FixedVec2 maximize(const FixedVec2& a, const FixedVec2& b)
	{
		return FixedVec2(std::max(a.x, b.x), std::max(a.y, b.y));
	}

	inline static FixedVec2 maximize(const FixedVec2& a, const FixedVec2& b, const FixedVec2& c)
	{
		return FixedVec2(std::max(std::max(a.x, b.x), c.x), std::max(std::max(a.y, b.y), c.y));
	}

	inline Vec3 GetVec2() const
	{
		return Vec3(x.as_float(), y.as_float());
	}
};
}

#endif  // #ifndef __MNM_FIXED_VEC2_H
