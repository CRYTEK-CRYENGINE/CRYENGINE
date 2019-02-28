// mtrand.h
// C++ include file for MT19937, with initialization improved 2002/1/26.
// Coded by Takuji Nishimura and Makoto Matsumoto.
// Ported to C++ by Jasper Bedaux 2003/1/1 (see http://www.bedaux.net/mtrand/).
// The generators returning floating point numbers are based on
// a version by Isaku Wada, 2002/01/09
// Static shared data converted to per-instance, 2008-11-13 by JSP @Crytek.
//
// Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// 3. The names of its contributors may not be used to endorse or promote
//    products derived from this software without specific prior written
//    permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Any feedback is very welcome.
// http://www.math.keio.ac.jp/matumoto/emt.html
// email: matumoto@math.keio.ac.jp
//
// Feedback about the C++ port should be sent to Jasper Bedaux,
// see http://www.bedaux.net/mtrand/ for e-mail address and info.

//! \cond INTERNAL

#pragma once

#include <CryCore/BaseTypes.h>  // uint32, uint64
#include "CryRandomInternal.h"  // CryRandom_Internal::BoundedRandom

//! Mersenne Twister random number generator.
class CMTRand_int32
{
public:
	//! Default constructor.
	CMTRand_int32() { seed(5489UL); }

	//! Constructor with 32 bit int as seed.
	CMTRand_int32(uint32 seed_value) { seed(seed_value); }

	//! Constructor with array of 32 bit integers as seed.
	CMTRand_int32(const uint32* array, int size) { seed(array, size); }

	//! Seeds with 32 bit integer.
	void seed(uint32 seed_value)
	{
		//if (seed_value == 0)
		//m_nRandom = 1;
		for (int i = 0; i < n; ++i)
			m_nState[i] = 0x0UL;
		m_nState[0] = seed_value;
		for (int i = 1; i < n; ++i)
		{
			m_nState[i] = 1812433253UL * (m_nState[i - 1] ^ (m_nState[i - 1] >> 30)) + i;
			// see Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier
			// in the previous versions, MSBs of the seed affect only MSBs of the array m_nState
			// 2002/01/09 modified by Makoto Matsumoto
		}
		p = n; // force gen_state() to be called for next random number
	}

	//! Seeds with array.
	void seed(const uint32* array, int size)
	{
		seed(19650218UL);
		int i = 1, j = 0;
		for (int k = ((n > size) ? n : size); k; --k)
		{
			m_nState[i] = (m_nState[i] ^ ((m_nState[i - 1] ^ (m_nState[i - 1] >> 30)) * 1664525UL))
			              + array[j] + j; // non linear
			++j;
			j %= size;
			if ((++i) == n) { m_nState[0] = m_nState[n - 1]; i = 1; }
		}
		for (int k = n - 1; k; --k)
		{
			PREFAST_SUPPRESS_WARNING(6385)  PREFAST_SUPPRESS_WARNING(6386)  m_nState[i] = (m_nState[i] ^ ((m_nState[i - 1] ^ (m_nState[i - 1] >> 30)) * 1566083941UL)) - i;
			if ((++i) == n) { m_nState[0] = m_nState[n - 1]; i = 1; }
		}
		m_nState[0] = 0x80000000UL; // MSB is 1; assuring non-zero initial array
		p = n;                      // force gen_state() to be called for next random number
	}

	~CMTRand_int32() {}

	// Functions with PascalCase names were added for interchangeability with CRndGen (see LCGRandom.h).

	void Seed(uint32 seed_value)
	{
		seed(seed_value);
	}

	uint32 GenerateUint32()
	{
		return rand_int32();
	}

	uint64 GenerateUint64()
	{
		const uint32 a = GenerateUint32();
		const uint32 b = GenerateUint32();
		return ((uint64)b << 32) | (uint64)a;
	}

	float GenerateFloat()
	{
		return (float)GenerateUint32() * (1.0f / 4294967295.0f);
	}

	//! Ranged function returns random value within the *inclusive* range between minValue and maxValue.
	//! Any orderings work correctly: minValue <= maxValue and minValue >= minValue.
	template<class T>
	T GetRandom(const T minValue, const T maxValue)
	{
		return CryRandom_Internal::BoundedRandom<CMTRand_int32, T>::Get(*this, minValue, maxValue);
	}

	//! Vector (Vec2, Vec3, Vec4) ranged function returns vector with every component within the *inclusive* ranges between minValue.component and maxValue.component.
	//! All orderings work correctly: minValue.component <= maxValue.component and minValue.component >= maxValue.component.
	template<class T>
	T GetRandomComponentwise(const T& minValue, const T& maxValue)
	{
		return CryRandom_Internal::BoundedRandomComponentwise<CMTRand_int32, T>::Get(*this, minValue, maxValue);
	}

	//! \return A random unit vector (Vec2, Vec3, Vec4).
	template<class T>
	T GetRandomUnitVector()
	{
		return CryRandom_Internal::GetRandomUnitVector<CMTRand_int32, T>(*this);
	}

protected: // Used by derived classes, otherwise not accessible; use the ()-operator.
	//! Generates 32 bit random int.
	uint32 rand_int32()
	{
		if (p >= n) gen_state(); // new m_nState vector needed
		// gen_state() is split off to be non-inline, because it is only called once
		// in every 624 calls and otherwise irand() would become too big to get inlined
		uint32 x = m_nState[p++];
		x ^= (x >> 11);
		x ^= (x << 7) & 0x9D2C5680UL;
		x ^= (x << 15) & 0xEFC60000UL;
		return x ^ (x >> 18);
	}

private:
	static const int n = 624, m = 397; // Compile time constants.

	// The variables below are static (no duplicates can exist).
	uint32 m_nState[n + 1]; //!< m_nState vector array.
	int    p;               //!< Position in m_nState array.

	// Private functions used to generate the pseudo random numbers.
	uint32 twiddle(uint32 u, uint32 v)
	{
		return (((u & 0x80000000UL) | (v & 0x7FFFFFFFUL)) >> 1)
		       ^ ((v & 1UL) ? 0x9908B0DFUL : 0x0UL);
	}

	//! Generate new m_nState vector.
	void gen_state()
	{
		for (int i = 0; i < (n - m); ++i)
			m_nState[i] = m_nState[i + m] ^ twiddle(m_nState[i], m_nState[i + 1]);
		for (int i = n - m; i < (n - 1); ++i)
			m_nState[i] = m_nState[i + m - n] ^ twiddle(m_nState[i], m_nState[i + 1]);
		m_nState[n - 1] = m_nState[m - 1] ^ twiddle(m_nState[n - 1], m_nState[0]);
		p = 0; // reset position
	}

	CMTRand_int32(const CMTRand_int32&);    //! Made unavailable, as it doesn't make sense.
	void operator=(const CMTRand_int32&);   //! Made unavailable, as it doesn't make sense.
};

//! \endcond