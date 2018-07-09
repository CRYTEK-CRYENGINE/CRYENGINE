// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

const float VEC_EPSILON = 0.05f;


//! Implement common functions for vectors of various sizes
//! This is a data-less class, to be used as a base class for actual vector types.
//! It aliases itself to an array of the given size.
template<typename T, int N>
struct INumberArray
{
	using TS = scalar_t<T>;

protected:

	// Constructors must be protected, to prevent automatic conversion, as INumberArray has no native data members.
	ILINE INumberArray()                                 { CRY_MATH_ASSERT(IsAligned(this, alignof(T))); IF_DEBUG(SetInvalid()); }
	ILINE INumberArray(type_zero)                        { CRY_MATH_ASSERT(IsAligned(this, alignof(T))); SetZero(); }
	ILINE explicit INumberArray(TS s)                    { CRY_MATH_ASSERT(IsAligned(this, alignof(T))); SetAll(convert<T>(s)); CRY_MATH_ASSERT(IsValid()); }

public:

	// Access
	ILINE T*       begin()                               { return reinterpret_cast<T*>(this); }
	ILINE const T* begin() const                         { return reinterpret_cast<const T*>(this); }

	ILINE T*       end()                                 { return begin() + N; }
	ILINE const T* end() const                           { return begin() + N; }

	ILINE T&       operator[](int i)                     { CRY_MATH_ASSERT(i < N); return begin()[i]; }
	ILINE const T& operator[](int i) const               { CRY_MATH_ASSERT(i < N); return begin()[i]; }

	// Operators
	ILINE bool  operator==(const INumberArray& o) const  { return IsEqual(o); }
	ILINE bool  operator!=(const INumberArray& o) const  { return !operator==(o); }

	// Methods
	bool IsEquivalent(const INumberArray& o, TS e = TS(VEC_EPSILON)) const
	{
		T e2 = convert<T>(sqr(e));
		if (e < 0) // Relative comparison
			e2 *= max(Square(), o.Square());
		else
			e2 *= convert<T>(N);
		return All(DiffSquare(o) <= e2);
	}
	ILINE bool IsEquivalent(type_zero, TS e = TS(VEC_EPSILON)) const
	{
		return IsZero(e);
	}
	ILINE static bool IsEquivalent(const INumberArray& a, const INumberArray& b, TS e = TS(VEC_EPSILON))
	{
		return a.IsEquivalent(b, e);
	}

	// Iterating methods
	ILINE bool IsZero(TS e = TS()) const
	{
		return All(Square() <= convert<T>(sqr(e) * TS(N)));
	}
	ILINE void SetZero()
	{
		SetAll(convert<T>());
	}
	ILINE void SetAll(T val)
	{
		for (auto& e: *this)
			e = val;
	}
	ILINE void SetInvalid()
	{
		for (auto& e: *this)
			::SetInvalid(e);
	}
	ILINE bool IsValid() const
	{
		for (auto& e: *this)
			if (!::IsValid(e))
				return false;
		return true;
	}

	template<typename T2>
	ILINE void Set(const INumberArray<T2, N>& o)
	{
		for (int i = 0; i < N; ++i)
			(*this)[i] = convert<T>(o[i]);
		CRY_MATH_ASSERT(IsValid());
	}

	ILINE bool IsEqual(const INumberArray& o) const
	{
		for (int i = 0; i < N; ++i)
			if (Any((*this)[i] != o[i]))
				return false;
		return true;
	}
	ILINE T Square() const
	{
		T s = sqr((*this)[0]);
		for (int i = 1; i < N; ++i)
			s = s + sqr((*this)[i]);
		return s;
	}
	ILINE T DiffSquare(const INumberArray& o) const
	{
		T s = sqr((*this)[0] - o[0]);
		for (int i = 1; i < N; ++i)
			s = s + sqr((*this)[i] - o[i]);
		return s;
	}
};

//! Add length functions to NumberArray
template<typename T, int N, typename Final>
struct INumberVector: INumberArray<T, N>
{
	using Base = INumberArray<T, N>;
	using Base::Base;
	using Base::IsValid;
	using Base::Square;
	using Base::DiffSquare;

	using value_type = T;
	enum { component_count = N };

	ILINE Final& final() { return *(Final*)this; }

	//
	// Length methods
	//
	ILINE T GetLengthSquared() const                         { return Square(); }
	ILINE T GetLength() const                                { return crymath::sqrt(Square()); }
	ILINE T GetLengthFast() const                            { return crymath::sqrt_fast(Square()); }
	ILINE T GetInvLength() const                             { return crymath::rsqrt(Square()); }
	ILINE T GetInvLengthFast() const                         { return crymath::rsqrt_fast(Square()); }
	ILINE T GetInvLengthSafe() const                         { return crymath::rsqrt_safe(Square()); }

	ILINE T GetSquaredDistance(const INumberVector& o) const { return DiffSquare(o); }
	ILINE T GetDistance(const INumberVector& o) const        { return crymath::sqrt(DiffSquare(o)); }

	template<typename T2, typename Final2>
	ILINE T Dot(const INumberVector<T2, N, Final2>& o) const
	{
		T s = T((*this)[0] * o[0]);
		for (int i = 1; i < N; ++i)
			s = s + T((*this)[i] * o[i]);
		return s;
	}
	template<typename T2, typename Final2>
	ILINE T operator|(const INumberVector<T2, N, Final2>& o) const
	{
		return Dot(o);
	}

	//
	// Scaling methods
	//
	ILINE Final operator-() const
	{
		Final r;
		for (int i = 0; i < N; ++i)
			r[i] = -(*this)[i];
		return r;
	}
	ILINE Final& operator *=(T s)
	{
		for (auto& e : *this)
			e *= s;
		CRY_MATH_ASSERT(IsValid());
		return final();
	}
	ILINE Final operator *(T s) const
	{
		Final r;
		for (int i = 0; i < N; ++i)
			r[i] = (*this)[i] * s;
		CRY_MATH_ASSERT(r.IsValid());
		return r;
	}
	ILINE Final& operator/=(T s)
	{
		for (auto& e : *this)
			e /= s;
		CRY_MATH_ASSERT(IsValid());
		return final();
	}
	ILINE Final operator/(T s) const
	{
		Final r;
		for (int i = 0; i < N; ++i)
			r[i] = (*this)[i] / s;
		CRY_MATH_ASSERT(r.IsValid());
		return r;
	}

	ILINE friend Final operator *(T s, const INumberVector& o) { return o * s; }

	//
	// Normalizing methods
	//

	//! normalize, either safe or fast
	ILINE Final& Normalize()                     { return *this *= GetInvLengthSafe(); }
	ILINE Final& NormalizeFast()                 { return *this *= GetInvLengthFast(); }
	ILINE Final GetNormalized() const            { return *this * GetInvLengthSafe(); }
	ILINE Final GetNormalizedFast() const        { return *this * GetInvLengthFast(); }

	//! normalize the vector to a scale
	ILINE Final& Normalize(T scale)              { return *this *= GetInvLengthSafe() * scale; }
	ILINE Final& NormalizeFast(T scale)          { return *this *= GetInvLengthFast() * scale; }
	ILINE Final GetNormalized(T scale) const     { return *this * (GetInvLengthSafe() * scale); }
	ILINE Final GetNormalizedFast(T scale) const { return *this * (GetInvLengthFast() * scale); }

	//! normalize the vector if not near zero, otherwise set to safe vector.
	//! \return The original length of the vector.
	ILINE T NormalizeSafe(const Final& safe, T minsqr = T(0))
	{
		CRY_MATH_ASSERT(IsValid());
		T lensqr = GetLengthSquared();
		if (lensqr > minsqr)
			*this *= crymath::rsqrt(lensqr);
		else
			final() = safe;
		return crymath::sqrt(lensqr);
	}

	//! return a safely normalized vector - returns safe vector (should be normalised) if original is near zero length
	ILINE Final GetNormalizedSafe(const Final& safe, T minsqr = T(0)) const
	{
		CRY_MATH_ASSERT(IsValid());
		T lensqr = GetLengthSquared();
		if (lensqr > minsqr)
			return *this * crymath::rsqrt(lensqr);
		else
			return safe;
	}

	// Arithmetic methods
	#define NUMBER_VECTOR_BINARY_OP(op) \
		template<typename T2, typename Final2> \
		ILINE Final& operator op ## =(const INumberVector<T2, N, Final2>& o) \
		{ \
			for (int i = 0; i < N; ++i) \
				(*this)[i] op ## = o[i]; \
			return final(); \
		} \
		template<typename T2, typename Final2> \
		ILINE Final operator op(const INumberVector<T2, N, Final2>& o) const \
		{ \
			Final r; \
			for (int i = 0; i < N; ++i) \
				r[i] = (*this)[i] op o[i]; \
			return r; \
		} \

	NUMBER_VECTOR_BINARY_OP(+)
	NUMBER_VECTOR_BINARY_OP(-)
	NUMBER_VECTOR_BINARY_OP(*)
	NUMBER_VECTOR_BINARY_OP(/)
};

//! \endcond