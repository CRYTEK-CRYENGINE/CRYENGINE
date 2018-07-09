// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Noise class based on Ken Perlin's papers
   Modified to be a standalone class, speed improvements and uses the cryengine
   pseudo-random generator to generate the gradients from the unit sphere.
   See comments in the class for usage

   -------------------------------------------------------------------------
   History:
   - 11:04:2007   Created by Marco Corbetta

*************************************************************************/

#pragma once

#define NOISE_TABLE_SIZE 256
#define NOISE_MASK       (NOISE_TABLE_SIZE - 1)

namespace
{
const int kDefaultSeed = 0;
}
//////////////////////////////////////////////////////////////////////////
class CPNoise3
{
public:
	CPNoise3(void)
	{
		SetSeedAndReinitialize(kDefaultSeed);
	}

	//! 1D quality noise generator, good for many situations like up/down movements, flickering/ambient lights etc.
	//! A typical usage would be to pass system time multiplied by a frequency value, like:
	//! float fRes=pNoise->Noise1D(fCurrentTime*fFreq);
	//! the lower the frequency, the smoother the output
	inline float Noise1D(float x)
	{
		// Compute what gradients to use
		int qx0 = (int)floorf(x);
		int qx1 = qx0 + 1;
		float tx0 = x - (float)qx0;
		float tx1 = tx0 - 1;

		// Make sure we don't come outside the lookup table
		qx0 = qx0 & NOISE_MASK;
		qx1 = qx1 & NOISE_MASK;

		// Compute the dotproduct between the vectors and the gradients
		float v0 = m_gx[qx0] * tx0;
		float v1 = m_gx[qx1] * tx1;

		// Modulate with the weight function
		float wx = (3 - 2 * tx0) * tx0 * tx0;
		float v = v0 - wx * (v0 - v1);

		return v;
	}

	//! 2D quality noise generator, twice slower than Noise1D.
	//! A typical usage would be to pass 2d coordinates multiplied by a frequency value, like:
	//! float fRes=pNoise->Noise2D(fX*fFreq,fY*fFreq);
	inline float Noise2D(float x, float y)
	{
		// Compute what gradients to use
		int qx0 = (int)floorf(x);
		int qx1 = qx0 + 1;
		float tx0 = x - (float)qx0;
		float tx1 = tx0 - 1;

		int qy0 = (int)floorf(y);
		int qy1 = qy0 + 1;
		float ty0 = y - (float)qy0;
		float ty1 = ty0 - 1;

		// Make sure we don't come outside the lookup table
		qx0 = qx0 & NOISE_MASK;
		qx1 = qx1 & NOISE_MASK;

		qy0 = qy0 & NOISE_MASK;
		qy1 = qy1 & NOISE_MASK;

		// Permutate values to get pseudo randomly chosen gradients
		int q00 = m_p[(qy0 + m_p[qx0]) & NOISE_MASK];
		int q01 = m_p[(qy0 + m_p[qx1]) & NOISE_MASK];

		int q10 = m_p[(qy1 + m_p[qx0]) & NOISE_MASK];
		int q11 = m_p[(qy1 + m_p[qx1]) & NOISE_MASK];

		// Compute the dotproduct between the vectors and the gradients
		float v00 = m_gx[q00] * tx0 + m_gy[q00] * ty0;
		float v01 = m_gx[q01] * tx1 + m_gy[q01] * ty0;

		float v10 = m_gx[q10] * tx0 + m_gy[q10] * ty1;
		float v11 = m_gx[q11] * tx1 + m_gy[q11] * ty1;

		// Modulate with the weight function
		float wx = (3 - 2 * tx0) * tx0 * tx0;
		float v0 = v00 - wx * (v00 - v01);
		float v1 = v10 - wx * (v10 - v11);

		float wy = (3 - 2 * ty0) * ty0 * ty0;
		float v = v0 - wy * (v0 - v1);

		return v;
	}

	//! 3D quality noise generator, twice slower than Noise2D, so use with care.
	//! A typical usage would be to pass 3d coordinates multiplied by a frequency value, like:
	//! float fRes=pNoise->Noise2D(fX*fFreq,fY*fFreq,fZ*fFreq);
	inline float Noise3D(float x, float y, float z)
	{
		// Compute what gradients to use
		int qx0 = (int)floorf(x);
		int qx1 = qx0 + 1;
		float tx0 = x - (float)qx0;
		float tx1 = tx0 - 1;

		int qy0 = (int)floorf(y);
		int qy1 = qy0 + 1;
		float ty0 = y - (float)qy0;
		float ty1 = ty0 - 1;

		int qz0 = (int)floorf(z);
		int qz1 = qz0 + 1;
		float tz0 = z - (float)qz0;
		float tz1 = tz0 - 1;

		// Make sure we don't come outside the lookup table
		qx0 = qx0 & NOISE_MASK;
		qx1 = qx1 & NOISE_MASK;

		qy0 = qy0 & NOISE_MASK;
		qy1 = qy1 & NOISE_MASK;

		qz0 = qz0 & NOISE_MASK;
		qz1 = qz1 & NOISE_MASK;

		// Permutate values to get pseudo randomly chosen gradients
		int q000 = m_p[(qz0 + m_p[(qy0 + m_p[qx0]) & NOISE_MASK]) & NOISE_MASK];
		int q001 = m_p[(qz0 + m_p[(qy0 + m_p[qx1]) & NOISE_MASK]) & NOISE_MASK];

		int q010 = m_p[(qz0 + m_p[(qy1 + m_p[qx0]) & NOISE_MASK]) & NOISE_MASK];
		int q011 = m_p[(qz0 + m_p[(qy1 + m_p[qx1]) & NOISE_MASK]) & NOISE_MASK];

		int q100 = m_p[(qz1 + m_p[(qy0 + m_p[qx0]) & NOISE_MASK]) & NOISE_MASK];
		int q101 = m_p[(qz1 + m_p[(qy0 + m_p[qx1]) & NOISE_MASK]) & NOISE_MASK];

		int q110 = m_p[(qz1 + m_p[(qy1 + m_p[qx0]) & NOISE_MASK]) & NOISE_MASK];
		int q111 = m_p[(qz1 + m_p[(qy1 + m_p[qx1]) & NOISE_MASK]) & NOISE_MASK];

		// Compute the dotproduct between the vectors and the gradients
		float v000 = m_gx[q000] * tx0 + m_gy[q000] * ty0 + m_gz[q000] * tz0;
		float v001 = m_gx[q001] * tx1 + m_gy[q001] * ty0 + m_gz[q001] * tz0;

		float v010 = m_gx[q010] * tx0 + m_gy[q010] * ty1 + m_gz[q010] * tz0;
		float v011 = m_gx[q011] * tx1 + m_gy[q011] * ty1 + m_gz[q011] * tz0;

		float v100 = m_gx[q100] * tx0 + m_gy[q100] * ty0 + m_gz[q100] * tz1;
		float v101 = m_gx[q101] * tx1 + m_gy[q101] * ty0 + m_gz[q101] * tz1;

		float v110 = m_gx[q110] * tx0 + m_gy[q110] * ty1 + m_gz[q110] * tz1;
		float v111 = m_gx[q111] * tx1 + m_gy[q111] * ty1 + m_gz[q111] * tz1;

		// Modulate with the weight function
		float wx = (3 - 2 * tx0) * tx0 * tx0;
		float v00 = v000 - wx * (v000 - v001);
		float v01 = v010 - wx * (v010 - v011);
		float v10 = v100 - wx * (v100 - v101);
		float v11 = v110 - wx * (v110 - v111);

		float wy = (3 - 2 * ty0) * ty0 * ty0;
		float v0 = v00 - wy * (v00 - v01);
		float v1 = v10 - wy * (v10 - v11);

		float wz = (3 - 2 * tz0) * tz0 * tz0;
		float v = v0 - wz * (v0 - v1);

		return v;
	}

	//! This needs to be called only once and it's already done by crysystem in the singleton constructor.
	//! Note that every time the initialization function gets called, the PRNG will return different values,
	//! thus creating different gradients. Not a bad thing but probably not your expected behavior.
	inline void SetSeedAndReinitialize(uint seedValue)
	{
		int i, j, nSwap;
		m_random_generator.Seed(seedValue);

		// Initialize the permutation table
		for (i = 0; i < NOISE_TABLE_SIZE; i++)
			m_p[i] = i;

		for (i = 0; i < NOISE_TABLE_SIZE; i++)
		{
			j = (m_random_generator.GenerateUint32()) & NOISE_MASK;

			nSwap = m_p[i];
			m_p[i] = m_p[j];
			m_p[j] = nSwap;
		}

		// Generate the gradient lookup tables
		for (i = 0; i < NOISE_TABLE_SIZE; i++)
		{
			// Ken Perlin proposes that the gradients are taken from the unit
			// circle/sphere for 2D/3D.
			// So lets generate a good pseudo-random vector and normalize it

			Vec3 v;
			// cry_frand is in the 0..1 range
			v.x = -0.5f + m_random_generator.GenerateFloat();
			v.y = -0.5f + m_random_generator.GenerateFloat();
			v.z = -0.5f + m_random_generator.GenerateFloat();
			v.Normalize();
			m_gx[i] = v.x;
			m_gy[i] = v.y;
			m_gz[i] = v.z;
		}
	}

private:

	CRndGen       m_random_generator;
	//! Permutation table.
	unsigned char m_p[NOISE_TABLE_SIZE];

	// Gradients.
	float m_gx[NOISE_TABLE_SIZE];
	float m_gy[NOISE_TABLE_SIZE];
	float m_gz[NOISE_TABLE_SIZE];
};
