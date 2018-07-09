// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

/*!
    Class TRange, can represent anything that is a range between two values.
 */
template<class T>
class TRange
{
public:
	T start;
	T end;

	TRange(T v = 0)
		: start(v), end(v) {}
	TRange(T s, T e)
		: start(s), end(e) {}

	enum EEmpty { EMPTY };
	TRange(EEmpty)
		: start(std::numeric_limits<T>::max()), end(std::numeric_limits<T>::lowest()) {}

	void Set(T s, T e) { start = s; end = e; };

	//! Get length of range.
	T Length() const { return end - start; };

	bool IsValid()  const { return start <= end; }
	bool IsEmpty()  const { return start >= end; }

	//! Check if value is inside range.
	bool IsInside(T val) const { return val >= start && val <= end; };

	void ClipValue(T& val) const
	{
		if (val < start) val = start;
		if (val > end) val = end;
	}

	//! Compare two ranges.
	bool operator==(const TRange& r) const
	{
		return start == r.start && end == r.end;
	}

	bool operator!=(const TRange& r) const
	{
		return !(*this == r);
	}

	//! Assign operator.
	TRange& operator=(const TRange& r)
	{
		start = r.start;
		end = r.end;
		return *this;
	}
	//! Interect two ranges.
	TRange operator&(const TRange& r) const
	{
		return TRange(std::max(start, r.start), std::min(end, r.end));
	}
	TRange& operator&=(const TRange& r)
	{
		return (*this = (*this & r));
	}
	//! Concatenate two ranges.
	TRange operator|(const TRange& r) const
	{
		return TRange(std::min(start, r.start), std::max(end, r.end));
	}
	TRange& operator|=(const TRange& r)
	{
		if (r.start < start) start = r.start;
		if (r.end > end) end = r.end;
		return *this;
	}
	//! Add new value to range.
	TRange operator|(T v) const
	{
		return TRange(std::min(start, v), std::max(end, v));
	}
	TRange& operator|=(T v)
	{
		if (v < start) start = v;
		if (v > end) end = v;
		return *this;
	}
	//! Scale range.
	TRange operator*(T v) const
	{
		return TRange(start * v, end * v);
	}
	TRange operator*(TRange r) const
	{
		return TRange(start * r.start, end * r.end);
	}
	//! Offset range.
	TRange operator+(T v) const
	{
		return TRange(start + v, end + v);
	}
	TRange operator+(TRange r) const
	{
		return TRange(start + r.start, end + r.end);
	}
	//! Interpolate range
	T operator()(float f) const
	{
		return start + T(Length() * f);
	}
};

//! Range if just TRange for floats..
typedef TRange<float> Range;
//! \endcond