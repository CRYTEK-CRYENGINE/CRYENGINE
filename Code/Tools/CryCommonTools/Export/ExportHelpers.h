// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __EXPORTHELPERS_H__
#define __EXPORTHELPERS_H__

#include <cmath>

namespace ExportHelpers
{
	inline void GenerateTextureCoordinates(float* const res_s, float* const res_t, const float x, const float y, const float z)
	{
		const float ax = ::fabs(x);
		const float ay = ::fabs(y);
		const float az = ::fabs(z);

		float s = 0.0f;
		float t = 0.0f;

		if (ax > 1e-3f || ay > 1e-3f || az > 1e-3f)
		{
			if (ax > ay)
			{
				if (ax > az)
				{
					// X rules
					s = y / ax;
					t = z / ax;
				}
				else
				{
					// Z rules
					s = x / az;
					t = y / az;
				}
			}
			else
			{	
				// ax <= ay
				if (ay > az)
				{
					// Y rules
					s = x / ay;
					t = z / ay;
				}
				else
				{
					// Z rules
					s = x / az;
					t = y / az;
				}
			}
		}

		// Now the texture coordinates are in the range [-1,1].
		// We want normalized [0,1] texture coordinates.
		s = (s + 1) * 0.5f;
		t = (t + 1) * 0.5f;

		if (res_s)
		{
			*res_s = s;
		}
		if (res_t)
		{
			*res_t = t;
		}
	}
}

#endif
