// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/BaseTypes.h>
#include "LCGRandom.h"
#include "MTPseudoRandom.h"

#include <CrySystem/ISystem.h>

struct SScopedRandomSeedChange
{
	SScopedRandomSeedChange()
	{
		m_prevSeed = gEnv->pSystem->GetRandomGenerator().GetState();
	}

	SScopedRandomSeedChange(uint32 seed)
	{
		m_prevSeed = gEnv->pSystem->GetRandomGenerator().GetState();

		gEnv->pSystem->GetRandomGenerator().Seed(seed);
	}

	~SScopedRandomSeedChange()
	{
		gEnv->pSystem->GetRandomGenerator().SetState(m_prevSeed);
	}

	void Seed(uint32 seed)
	{
		gEnv->pSystem->GetRandomGenerator().Seed(seed);
	}

	uint64 m_prevSeed;
};

inline CRndGen cry_random_next()
{
	return gEnv->pSystem->GetRandomGenerator().Next();
}

inline uint32 cry_random_uint32()
{
	return gEnv->pSystem->GetRandomGenerator().GenerateUint32();
}

// PERSONAL NOTE: A tad messier due to conflicting templates.
// Plus header dependencies......simpler to put overloads here.

//! Scalar ranged random function.
//! Any orderings work correctly: minValue <= maxValue and minValue >= minValue.
//! \return Random value between minValue and maxValue (inclusive).
MPOff
inline T cry_random(const T minValue, const T maxValue)
{
	return gEnv->pSystem->GetRandomGenerator().GetRandom(minValue, maxValue);
}

// MPFloat setup
MPOnly 
inline T cry_random(const T minValue, const T maxValue)
{
	// Float inaccuracy is fine, random
	return T().lossy(gEnv->pSystem->GetRandomGenerator().GetRandom((float)minValue, (float)maxValue));
}

// CTimeValue
template<> 
inline CTimeValue cry_random(const CTimeValue minValue, const CTimeValue maxValue)
{
	return CTimeValue(cry_random(minValue.GetSeconds(), maxValue.GetSeconds()));
}

//! Vector (Vec2, Vec3, Vec4) ranged random function.
//! All orderings work correctly: minValue.component <= maxValue.component and
//! minValue.component >= maxValue.component.
//! \return A vector, every component of which is between minValue.component and maxValue.component (inclusive).
template<class T>
inline T cry_random_componentwise(const T& minValue, const T& maxValue)
{
	return gEnv->pSystem->GetRandomGenerator().GetRandomComponentwise(minValue, maxValue);
}

//! \return A random unit vector (Vec2, Vec3, Vec4).
template<class T>
inline T cry_random_unit_vector()
{
	return gEnv->pSystem->GetRandomGenerator().GetRandomUnitVector<T>();
}
