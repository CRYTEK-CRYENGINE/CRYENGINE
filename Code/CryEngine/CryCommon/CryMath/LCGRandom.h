// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include <CryCore/BaseTypes.h>  // uint32, uint64
#include "CryRandomInternal.h"  // CryRandom_Internal::BoundedRandom

//! A simple Linear Congruential Generator (LCG) of pseudo-randomized numbers.
//! NOTE: It should *not* be used for any encryption methods.
//! We use Microsoft Visual/Quick C/C++ generator's settings (mul 214013, add 2531011)
//! (see http://en.wikipedia.org/wiki/Linear_congruential_generator), but our generator
//! returns results that are different from Microsoft's:
//! Microsoft's version returns 15-bit values (bits 30..16 of the 32-bit state),
//! our version returns 32-bit values (bits 47..16 of the 64-bit state).
class CRndGen
{
public:
	CRndGen()
	{
		Seed(5489UL);
	}

	CRndGen(uint32 seed)
	{
		Seed(seed);
	}

	//! Initializes the generator using an unsigned 32-bit number.
	void Seed(uint32 seed)
	{
		m_state = (uint64)seed;
	}

	void SetState(uint64 state)
	{
		m_state = state;
	}

	uint64 GetState()
	{
		return m_state;
	}

	CRndGen& Next()
	{
		m_state = ((uint64)214013) * m_state + ((uint64)2531011);
		return *this;
	}

	//! Generates a random number in the closed interval [0; max uint32].
	uint32 GenerateUint32()
	{
		Next();
		return (uint32)(m_state >> 16);
	}

	//! Generates a random number in the closed interval [0; max uint64].
	uint64 GenerateUint64()
	{
		const uint32 a = GenerateUint32();
		const uint32 b = GenerateUint32();
		return ((uint64)b << 32) | (uint64)a;
	}

	//! Generates a random number in the closed interval [0.0f; 1.0f].
	float GenerateFloat()
	{
		return (float)GenerateUint32() * (1.0f / 4294967295.0f);
	}

	//! Ranged function returns random value within the *inclusive* range between minValue and maxValue.
	//! Any orderings work correctly: minValue <= maxValue and minValue >= minValue.
	template<class T>
	T GetRandom(const T minValue, const T maxValue)
	{
		return CryRandom_Internal::BoundedRandom<CRndGen, T>::Get(*this, minValue, maxValue);
	}

	//! Vector (Vec2, Vec3, Vec4) ranged function returns vector with every component within the *inclusive* ranges between minValue.component and maxValue.component.
	//! All orderings work correctly: minValue.component <= maxValue.component and minValue.component >= maxValue.component.
	template<class T>
	T GetRandomComponentwise(const T& minValue, const T& maxValue)
	{
		return CryRandom_Internal::BoundedRandomComponentwise<CRndGen, T>::Get(*this, minValue, maxValue);
	}

	//! The function returns a random unit vector (Vec2, Vec3, Vec4).
	template<class T>
	T GetRandomUnitVector()
	{
		return CryRandom_Internal::GetRandomUnitVector<CRndGen, T>(*this);
	}

private:
	uint64 m_state;
};

//! \endcond