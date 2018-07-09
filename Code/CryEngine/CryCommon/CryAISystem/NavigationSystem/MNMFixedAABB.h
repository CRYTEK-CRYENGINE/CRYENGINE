// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MNM_FIXED_AABB_H
#define __MNM_FIXED_AABB_H

#pragma once

#include "MNMFixedVec3.h"

namespace MNM
{
template<typename BaseType, size_t IntegerBitCount>
struct FixedAABB
{
	typedef FixedVec3<BaseType, IntegerBitCount> value_type;

	value_type min;
	value_type max;

	struct init_empty {};

	inline FixedAABB()
	{
	}

	inline FixedAABB(const init_empty&)
		: min(value_type::value_type::max())
		, max(value_type::value_type::min())
	{
	}

	inline FixedAABB(const value_type& _min, const value_type& _max)
		: min(_min)
		, max(_max)
	{
	}

	template<typename OBaseType, size_t OIntegerBitCount>
	inline FixedAABB(const FixedAABB<OBaseType, OIntegerBitCount>& other)
		: min(value_type(other.min))
		, max(value_type(other.max))
	{
	}

	bool empty() const
	{
		return min.x > max.x;
	}

	value_type size() const
	{
		return (max - min).abs();
	}

	value_type center() const
	{
		return (max + min) * value_type::value_type::fraction(1, 2);
	}

	template<typename BaseTypeO, size_t IntegerBitCountO>
	bool overlaps(const FixedAABB<BaseTypeO, IntegerBitCountO>& other) const
	{
		if ((min.x > typename value_type::value_type(other.max.x)) || (min.y > typename value_type::value_type(other.max.y))
		    || (min.z > typename value_type::value_type(other.max.z)) || (max.x < typename value_type::value_type(other.min.x))
		    || (max.y < typename value_type::value_type(other.min.y)) || (max.z < typename value_type::value_type(other.min.z)))
			return false;
		return true;
	}

	template<typename BaseTypeO, size_t IntegerBitCountO>
	inline FixedAABB& operator+=(const FixedVec3<BaseTypeO, IntegerBitCountO>& other)
	{
		min = value_type::minimize(min, other);
		max = value_type::maximize(max, other);
		return *this;
	}

	template<typename BaseTypeO, size_t IntegerBitCountO>
	inline FixedAABB operator+(const FixedVec3<BaseTypeO, IntegerBitCountO>& other) const
	{
		FixedAABB r(*this);
		r += other;
		return r;
	}

	template<typename BaseTypeO, size_t IntegerBitCountO>
	inline FixedAABB& operator+=(const FixedAABB<BaseTypeO, IntegerBitCountO>& other)
	{
		min = value_type::minimize(min, other.min);
		max = value_type::maximize(max, other.max);
		return *this;
	}

	template<typename BaseTypeO, size_t IntegerBitCountO>
	inline FixedAABB operator+(const FixedAABB<BaseTypeO, IntegerBitCountO>& other) const
	{
		FixedAABB r(*this);
		r += other;
		return r;
	}

	template<typename BaseTypeO, size_t IntegerBitCountO>
	inline FixedAABB& operator*=(const fixed_t<BaseTypeO, IntegerBitCountO>& x)
	{
		min *= x;
		max *= x;
		return *this;
	}

	template<typename BaseTypeO, size_t IntegerBitCountO>
	inline FixedAABB operator*(const fixed_t<BaseTypeO, IntegerBitCountO>& other) const
	{
		FixedAABB r(*this);
		r *= other;
		return r;
	}

	AUTO_STRUCT_INFO;
};
}

#endif  // #ifndef __MNM_FIXED_AABB_H
