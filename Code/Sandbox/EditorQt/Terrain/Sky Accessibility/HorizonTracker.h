// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#define _USE_MATH_DEFINES // M_PI
#include <math.h>         // sin()

#include <vector>                     // STL vector<>
#include <float.h>                    // FLT_MAX

// helper class to calculated hemisphere accessibility of a 2d graph or more useful for a heightmap (apply the same algo. n times)
// call Insert() 2d point on a graph (x coordiante has to advance for each point
// this method returns the slope at that position (without any noise)
//
// Performance for a line with n points:
//    O_min(n), O_avg(k*n*n) k<1 depending on input, Omax(n*n)
//
// because of the cache locality this is a very fast way to calculate the accessibility
//
// Comparison with simple raycasting (other algothms are much more complex to code):
//    result is noisy, O_avg(q*n*n), q>8 depending on quality, with more programming you can get cache locality almost like here
//
class CHorizonTracker
{
public:

	//! constructor (reinstance for every line you want to calculate)
	CHorizonTracker(void)
	{
		m_vHorizon.reserve(1024);   // to avoid too many reallocations
	}

	//! \param infX has to be always bigger that the value put in before
	//! \param infY (height) has to be in the same scale as infX
	//! \param xDist is set to the distance the slope is located
	//! \return slope (always positive or 0)
	float Insert(const float infX, const float infY, float& xDist)
	{
		int iSize = (int)m_vHorizon.size();

		for (; iSize >= 2; )
		{
			SPoint2D& Current = m_vHorizon[iSize - 1];
			SPoint2D& Prev = m_vHorizon[iSize - 2];

			assert(Prev.x < Current.x);
			assert(Current.x < infX);

			// build a line between the given new point and Prev
			// if the current point is below that line then the current is no longer a potential horizon
			// so remove it

			float fFactor = (Current.x - Prev.x) / (infX - Prev.x);
			float fHeightAtCurrent = Prev.y + fFactor * (infY - Prev.y);       // height at Current.x on the line

			if (fHeightAtCurrent > Current.y)
			{
				m_vHorizon.pop_back();
				iSize--;                                                    // remove current
			}
			else break;                                                   // horizon found
		}

		m_vHorizon.push_back(SPoint2D(infX, infY));

		// calculate slope
		if (iSize <= 0)
		{
			xDist = 0.f;
			return 0;
		}
		else
		{
			SPoint2D& Current = m_vHorizon[iSize - 1];

			assert(infX - Current.x != 0);      // infX has to advance (check your input)

			xDist = fabs(infX - Current.x);
			return min((infY - Current.y) / (infX - Current.x), 0.0f);
		}
	}

	//! reset to initial state
	void Clear()
	{
		m_vHorizon.resize(0);
	}

private:

	struct SPoint2D
	{
		SPoint2D() {}
		SPoint2D(const float infX, const float infY) { x = infX; y = infY; }
		float x, y;
	};

	std::vector<SPoint2D> m_vHorizon;             //!< sorted by x (first element has the lowest x)
};

//! slow but stable
//!\param indwValue has to be power of two
inline DWORD GetIntLog2(const DWORD indwValue)
{
	for (DWORD i = 0; i < 32; i++)
		if ((1 << i) == indwValue)
			return(i);

	assert(0);              // input was not power of two
	return(0xffffffff);
}

