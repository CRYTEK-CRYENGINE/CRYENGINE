// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MNM_FIXED_VEC3_H
#define __MNM_FIXED_VEC3_H

#pragma once

#include <CryCore/FixedPoint.h>

namespace MNM
{
template<typename BaseType, size_t IntegerBitCount>
struct FixedVec3
{
	typedef fixed_t<BaseType, IntegerBitCount>                             value_type;
	typedef typename CryFixedPoint::unsigned_overflow_type<BaseType>::type unsigned_overflow_type;

	value_type x, y, z;

	inline FixedVec3()
	{
	}

	inline FixedVec3(const value_type& _x)
		: x(_x)
		, y(_x)
		, z(_x)
	{
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline FixedVec3(const FixedVec3<OBaseType, OIntegerBitCount>& _x)
		: x(value_type(_x.x))
		, y(value_type(_x.y))
		, z(value_type(_x.z))
	{
	}

	inline FixedVec3(const value_type& _x, const value_type& _y, const value_type& _z)
		: x(_x)
		, y(_y)
		, z(_z)
	{
	}

	inline FixedVec3(const Vec3& _vec)
		: x(value_type(_vec.x))
		, y(value_type(_vec.y))
		, z(value_type(_vec.z))
	{
	}

	inline const value_type& operator[](size_t i) const
	{
		assert(i < 3);
		return ((value_type*)&x)[i];
	}

	inline value_type& operator[](size_t i)
	{
		assert(i < 3);
		return ((value_type*)&x)[i];
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline FixedVec3 operator+(const FixedVec3<OBaseType, OIntegerBitCount>& other) const
	{
		return FixedVec3(x + other.x, y + other.y, z + other.z);
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline FixedVec3 operator-(const FixedVec3<OBaseType, OIntegerBitCount>& other) const
	{
		return FixedVec3(x - other.x, y - other.y, z - other.z);
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline FixedVec3& operator+=(const FixedVec3<OBaseType, OIntegerBitCount>& other)
	{
		x += other.x;
		y += other.y;
		z += other.z;
		return *this;
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline FixedVec3& operator-=(const FixedVec3<OBaseType, OIntegerBitCount>& other)
	{
		x -= other.x;
		y -= other.y;
		z -= other.z;
		return *this;
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline FixedVec3 operator/(const FixedVec3<OBaseType, OIntegerBitCount>& other) const
	{
		return FixedVec3(x / other.x, y / other.y, z / other.z);
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline FixedVec3 operator*(const FixedVec3<OBaseType, OIntegerBitCount>& other) const
	{
		return FixedVec3(x * other.x, y * other.y, z * other.z);
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline FixedVec3 operator/(const fixed_t<OBaseType, OIntegerBitCount>& value) const
	{
		return FixedVec3(x / value, y / value, z / value);
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline FixedVec3 operator*(const fixed_t<OBaseType, OIntegerBitCount>& value) const
	{
		return FixedVec3(x * value, y * value, z * value);
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline bool operator==(const FixedVec3<OBaseType, OIntegerBitCount>& other) const
	{
		return (x == other.x) && (y == other.y) && (z == other.z);
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline bool operator!=(const FixedVec3<OBaseType, OIntegerBitCount>& other) const
	{
		return (x != other.x) || (y != other.y || (z != other.z));
	}

	inline void Set(const value_type& _x, const value_type& _y, const value_type& _z)
	{
		x = _x;
		y = _y;
		z = _z;
	}

	inline value_type dot(const FixedVec3& other) const
	{
		return x * other.x + y * other.y + z * other.z;
	}

	inline value_type approximatedLen() const
	{
		const value_type heuristicFactor1(0.5f * (1.0f + (1.0f / (4.0f * sqrtf(3.0f)))));
		const value_type heuristicFactor2(1.0f / sqrtf(3.0f));

		const value_type targetDistAbsX = fabsf(x), targetDistAbsY = fabsf(y), targetDistAbsZ = fabsf(z);
		return heuristicFactor1 * min(heuristicFactor2 * (targetDistAbsX + targetDistAbsY + targetDistAbsZ), max(max(targetDistAbsX, targetDistAbsY), targetDistAbsZ));
	}

	// Be careful. This function can be safely used only when the vector length
	// respects len^2 <= value_type::max().
	// In all the other cases the value returned is incorrect and undefined
	inline value_type len() const
	{
		return sqrtf(lenSq());
	}

	// Be careful. This function can be safely used only when the vector length
	// respects len^2 <= value_type::max().
	// In all the other cases the value returned is incorrect and undefined
	template<typename BaseTypeO, size_t IntegerBitCountO>
	inline fixed_t<BaseTypeO, IntegerBitCountO> len() const
	{
		return sqrtf(lenSq<BaseTypeO, IntegerBitCountO>());
	}

	inline value_type lenNoOverflow() const
	{
		assert(value_type::sqrtf(lenSqNoOverflow()) >= value_type(0));
		return value_type::sqrtf(lenSqNoOverflow());
	}

	// Note: This function can overflow
	inline value_type lenSq() const
	{
		assert((x * x + y * y + z * z) >= value_type(0));
		return x * x + y * y + z * z;
	}

	inline unsigned_overflow_type lenSqNoOverflow() const
	{
		return x.sqr() + y.sqr() + z.sqr();
	}

	template<typename BaseTypeO, size_t IntegerBitCountO>
	inline fixed_t<BaseTypeO, IntegerBitCountO> lenSq() const
	{
		return fixed_t<BaseTypeO, IntegerBitCountO>(x) * fixed_t<BaseTypeO, IntegerBitCountO>(x)
		       + fixed_t<BaseTypeO, IntegerBitCountO>(y) * fixed_t<BaseTypeO, IntegerBitCountO>(y)
		       + fixed_t<BaseTypeO, IntegerBitCountO>(z) * fixed_t<BaseTypeO, IntegerBitCountO>(z);
	}

	inline FixedVec3 abs() const
	{
		return FixedVec3(fabsf(x), fabsf(y), fabsf(z));
	}

	// This function uses the len() value to normalize the vector.
	// If the vector is really long, len() could overflow and the normalized
	// resulting vector could be not correct.
	inline FixedVec3 normalized() const
	{
		const value_type invLen = value_type(1) / len();
		return FixedVec3(x * invLen, y * invLen, z * invLen);
	}

	// This function uses the len() value to normalize the vector.
	// If the vector is really long, len() could overflow and the normalized
	// resulting vector could be not correct.
	template<typename BaseTypeO, size_t IntegerBitCountO>
	inline FixedVec3 normalized() const
	{
		const fixed_t<BaseTypeO, IntegerBitCountO> invLen =
		  fixed_t<BaseTypeO, IntegerBitCountO>(1) / len<BaseTypeO, IntegerBitCountO>();

		return FixedVec3(fixed_t<BaseTypeO, IntegerBitCountO>(x) * invLen,
		                 fixed_t<BaseTypeO, IntegerBitCountO>(y) * invLen,
		                 fixed_t<BaseTypeO, IntegerBitCountO>(z) * invLen);
	}

	inline static FixedVec3 minimize(const FixedVec3& a, const FixedVec3& b)
	{
		return FixedVec3(std::min(a.x, b.x),
		                 std::min(a.y, b.y),
		                 std::min(a.z, b.z));
	}

	inline static FixedVec3 minimize(const FixedVec3& a, const FixedVec3& b, const FixedVec3& c)
	{
		return FixedVec3(std::min(std::min(a.x, b.x), c.x),
		                 std::min(std::min(a.y, b.y), c.y),
		                 std::min(std::min(a.z, b.z), c.z));
	}

	inline static FixedVec3 maximize(const FixedVec3& a, const FixedVec3& b)
	{
		return FixedVec3(std::max(a.x, b.x),
		                 std::max(a.y, b.y),
		                 std::max(a.z, b.z));
	}

	inline static FixedVec3 maximize(const FixedVec3& a, const FixedVec3& b, const FixedVec3& c)
	{
		return FixedVec3(std::max(std::max(a.x, b.x), c.x),
		                 std::max(std::max(a.y, b.y), c.y),
		                 std::max(std::max(a.z, b.z), c.z));
	}

	inline Vec3 GetVec3() const
	{
		return Vec3(x.as_float(), y.as_float(), z.as_float());
	}

	AUTO_STRUCT_INFO;
};
}

#endif  // #ifndef __MNM_FIXED_VEC3_H
