// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <stdio.h>
#include <assert.h>                   // assert()
#include <math.h>                     // floorf()
#include "SummedAreaFilterKernel.h"   // CSummedAreaTableFilterKernel
#include <CryMath/Cry_Math.h>                 // gf_PI
#include <CryString/StringUtils.h>              // cry_strcpy()
#include "IRCLog.h"                   // debug messages


static inline int getNearestInt(const float v)
{
	return (v < 0) ? int(v - 0.5f) : int(v + 0.5f);
}

static inline int getNearestInt(const double v)
{
	return (v < 0) ? int(v - 0.5) : int(v + 0.5);
}


// http://en.wikipedia.org/wiki/Sinc_function
static inline float sinc(const float x)
{
	return (fabs(x) < 0.0001f) ? 1 : sinf(x) / x;
}


// https://ccrma.stanford.edu/~jos/sasp/Kaiser_Window.html (see I0(x))
inline static float bessel0(float x)
{
	const float xHalf = 0.5f * x;
	float sum = 1.0f;
	float pow = 1.0f;
	int k = 0;

	for (;;)
	{
		++k;
		pow *= xHalf / k;
		const float ds = pow * pow;
		sum += ds;

		if (ds <= sum * 1e-6f)
		{
			return sum;
		}
	}
}


CSummedAreaTableFilterKernel::CSummedAreaTableFilterKernel()
	: m_fCorrectionFactor(0.0f)
{
	cry_strcpy(m_description, "Empty");
}


// http://www.sixsigma.de/english/sixsigma/6s_e_gauss.htm
bool CSummedAreaTableFilterKernel::CreateFromGauss(
	const uint32 indwSize, 
	const float fInnerDiameter,
	const float fNegativeLobe)
{
	assert(indwSize > 4);
	assert(fInnerDiameter > 0.0f);

	if (!SetSize(indwSize, indwSize))
	{
		return false;
	}
	Fill(0);

	const float radius = (indwSize - 1) * 0.5f;

	const float rcpDiameter = 1 / fInnerDiameter; 

	for (uint32 iy = 0; iy < indwSize; ++iy)
	{
		const float y = -radius + iy;

		for (uint32 ix = 0; ix < indwSize; ++ix)
		{
			const float x = -radius + ix;
			const float t = sqrt(x * x + y * y) / radius;

			if (t > 1.0f)
			{
				m_data[iy * indwSize + ix] = 0;
			}
			else
			{
				const double r = t * rcpDiameter;

				const double fSigma = 1.0 / 3.0;   // we aim for 6*sigma = 99,99996 of all values

				const double fOuterLobeWeight =
					exp(-t * t / (2 * fSigma * fSigma)) - (1.0 - 0.9999996);

				const double fInnerLobeWeight =
					(r <= 1)
					? exp(-r * r / (2 * fSigma * fSigma)) - (1.0 - 0.9999996)
					: 0;

				m_data[iy * indwSize + ix] = getNearestInt(float(255 * (fInnerLobeWeight - fOuterLobeWeight * fNegativeLobe)));
			}	
		}
	}

	assert(IsSymmetrical());

	cry_sprintf(m_description,
		"Gauss (innerDiam:%g, negativeLobe:%g)", 
		fInnerDiameter, fNegativeLobe);

	SumUpTableAndNormalize();

	return true;
}


bool CSummedAreaTableFilterKernel::CreateFromLanczos(const uint32 indwSize, float sincLobeCount)
{
	assert(indwSize > 4);
	Util::clampMin(sincLobeCount, 1.0f);

	if (!SetSize(indwSize, indwSize))
	{
		return false;
	}
	Fill(0);

	const float radius = (indwSize - 1) * 0.5f;

	for (uint32 iy = 0; iy < indwSize; ++iy)
	{
		const float y = -radius + iy;

		for (uint32 ix = 0; ix < indwSize; ++ix)
		{
			const float x = -radius + ix;
			const float t = sqrtf(x * x + y * y) / radius;	 // [0;1] for points from center to side of the disk

			if (t > 1.0f)
			{
				m_data[iy * indwSize + ix] = 0;
			}
			else
			{
				const float xx = float(t * sincLobeCount * gf_PI);
				const float sincValue = sinc(xx);

				const float windowValue = sinc(xx / sincLobeCount);

				const float w = sincValue * windowValue;

				m_data[iy * indwSize + ix] = getNearestInt(255 * w);
			}
		}
	}

	assert(IsSymmetrical());

	cry_sprintf(m_description,
		"Lanczos (lobes:%g)", 
		sincLobeCount);
		
	SumUpTableAndNormalize();

	return true;
}


// http://en.wikipedia.org/wiki/Kaiser_window
bool CSummedAreaTableFilterKernel::CreateFromKaiser(const uint32 indwSize, float sincLobeCount, const float pi_mul_alpha)
{
	assert(indwSize > 4);
	Util::clampMin(sincLobeCount, 1.0f);

	if (!SetSize(indwSize, indwSize))
	{
		return false;
	}
	Fill(0);

	const float radius = (indwSize - 1) * 0.5f;

	const float denom = bessel0(pi_mul_alpha);

	for (uint32 iy = 0; iy < indwSize; ++iy)
	{
		const float y = -radius + iy;

		for (uint32 ix = 0; ix < indwSize; ++ix)
		{
			const float x = -radius + ix;
			const float t = sqrtf(x * x + y * y) / radius;	 // [0;1] for points from center to side of the disk

			if (t > 1.0f)
			{
				m_data[iy * indwSize + ix] = 0;
			}
			else
			{
				const float xx = float(t * sincLobeCount * gf_PI);
				const float sincValue = sinc(xx);

				const float nom = bessel0(pi_mul_alpha * sqrtf(1 - t * t));
				const float windowValue = nom / denom;

				const float w = sincValue * windowValue;

				m_data[iy * indwSize + ix] = getNearestInt(255 * w);
			}
		}
	}

	assert(IsSymmetrical());

	cry_sprintf(m_description,
		"Kaiser (lobes:%g, pi*A:%g)", 
		sincLobeCount, pi_mul_alpha);

	SumUpTableAndNormalize();

	return true;
}


bool CSummedAreaTableFilterKernel::SaveToRAW(const char* filename) const
{
	FILE* const out = fopen(filename,"wb");
	if (!out)
	{
		return false;
	}

	for (uint32 y = 0; y < m_dwHeight; ++y)
	{
		for (uint32 x = 0; x < m_dwWidth; ++x)
		{
			const uint8 val = m_data[y * m_dwWidth + x] / 2 + 127;

			if (fwrite(&val, 1, 1, out) != 1)
			{ 
				fclose(out);
				return false; 
			}
		}
	}

	fclose(out);
	return true;
}


bool CSummedAreaTableFilterKernel::CreateFromRawFile(const char* filename, const uint32 indwSize, const int iniMidValue)
{
	assert(iniMidValue >= 0 && iniMidValue < 255);

	if (!SetSize(indwSize, indwSize))
	{
		return false;
	}
	Fill(0);

	FILE* const in = fopen(filename,"rb");
	if (!in)
	{
		return false;
	}

	for (uint32 y = 0; y < m_dwHeight; ++y)
	{
		for (uint32 x = 0; x < m_dwWidth; ++x)
		{
			uint8 val;

			if (fread(&val, 1, 1, in) != 1)
			{
				fclose(in);
				return false;
			}

			m_data[y * m_dwWidth + x] = (int)val - iniMidValue;
		}
	}

	fclose(in);

	cry_sprintf(m_description,
		"RAW (%ix%i)",
		GetWidth(), GetHeight());

	SumUpTableAndNormalize();

	return true;
}


const char* CSummedAreaTableFilterKernel::GetDescription() const
{
	return &m_description[0];
}


float CSummedAreaTableFilterKernel::GetAreaBoxAA(
	float minX, float minY, 
	float maxX, float maxY) const
{
	assert(GetWidth() > 0);

	minX *= m_dwWidth;
	minY *= m_dwHeight;
	maxX *= m_dwWidth;
	maxY *= m_dwHeight;

	const float fSum = 
		+ GetBilinearFiltered(maxX, maxY)
		- GetBilinearFiltered(minX, maxY)
		- GetBilinearFiltered(maxX, minY)
		+ GetBilinearFiltered(minX, minY);

	return fSum * m_fCorrectionFactor;
}


float CSummedAreaTableFilterKernel::GetBilinearFiltered(const float infX, const float infY) const
{
	float wLeftTop = -FLT_MIN;
	float wRightTop = -FLT_MIN;
	float wLeftBottom = -FLT_MIN;
	float wRightBottom = -FLT_MIN;

	float fFX, fFY;
	int iX, iY;

	if (infX <= 0)
	{
		iX = 0;
		fFX = 0;
		wLeftTop = 0.0f;
		wLeftBottom = 0.0f;
	}
	else if (infX >= m_dwWidth)
	{
		iX = m_dwWidth - 1;
		fFX = 1.0f;
	}
	else
	{
		iX = (int)infX;
		fFX = infX - iX;
	}

	if (infY <= 0)
	{
		iY = 0;
		fFY = 0;
		wLeftTop = 0.0f;
		wRightTop = 0.0f;
	}
	else if (infY >= m_dwHeight)
	{
		iY = m_dwHeight - 1;
		fFY = 1.0f;
	}
	else
	{
		iY = (int)infY;
		fFY = infY - iY;
	}

	if (wLeftTop <= -FLT_MIN)
	{
		wLeftTop = float(m_data[(iY - 1) * m_dwWidth + (iX - 1)]);
	}
	if (wRightTop <= -FLT_MIN)
	{
		wRightTop = float(m_data[(iY - 1) * m_dwWidth + iX]);
	}
	if (wLeftBottom <= -FLT_MIN)
	{
		wLeftBottom = float(m_data[iY * m_dwWidth + (iX - 1)]);
	}
	wRightBottom = float(m_data[iY * m_dwWidth + iX]);

	const float fArea = 
		+ wLeftTop     * ((1.0f - fFX) * (1.0f - fFY))   // left top
		+ wRightTop    * ((       fFX) * (1.0f - fFY))   // right top
		+ wLeftBottom  * ((1.0f - fFX) * (       fFY))   // left bottom
		+ wRightBottom * ((       fFX) * (       fFY));  // right bottom

	return fArea;
}


bool CSummedAreaTableFilterKernel::ComputeWeightFilterBlock(
	CSimpleBitmap<float>& outFilter,
	float radius, 
	const bool bCenter) const
{
	assert(radius > 0.0f);
	Util::clampMin(radius, 0.001f);

	const int width =
		bCenter
		? int(ceilf(radius + 0.5f)) * 2 - 1
		: int(ceilf(radius)) * 2;
	assert(width >= 1);

	if (!outFilter.SetSize(width, width))
	{
		return false;
	}
	outFilter.Fill(0.0f);

	AddWeights(outFilter, radius);

	return true;
}


void CSummedAreaTableFilterKernel::AddWeights(
	CSimpleBitmap<float>& inoutFilter, 
	const float radius) const
{
	assert(radius > 0.0f);

	const int iw = inoutFilter.GetWidth();
	const int ih = inoutFilter.GetHeight();
	assert(iw == ih);
	assert(iw > 1 && ih > 1);

	// Compute coordinates of the filter pixel in SAT filter kernel space, assuming that the AA box of the SAT in SAT space is { (0;0), (+1;+1) }
	// and the top left corner of AA box of the radius area in the filter maps to the top left corner of the SAT's AA box.
	// Those coordinates in SAT space are: (xMin, yMin), (xMax, yMax). See code below.

	const float mul = iw / (2 * radius);
	const float add = 0.5f * (1 - mul);

	// Compute weight for each pixel of the filter, one by one.
	for (int iy = 0; iy < ih; ++iy)
	{
		const float yMin = ((iy * mul) / ih) + add;
		const float yMax = (((iy + 1) * mul) / ih) + add;

		for (int ix = 0; ix < iw; ++ix)
		{
			const float xMin = ((ix * mul) / ih) + add;
			const float xMax = (((ix + 1) * mul) / ih) + add;

			const float fArea = GetAreaBoxAA(xMin, yMin, xMax, yMax);

			const float* const pOldVal = inoutFilter.Get(ix, iy); 
			inoutFilter.Set(ix, iy, *pOldVal + fArea);
		}
	}
}


bool CSummedAreaTableFilterKernel::IsSymmetrical() const
{
	const uint32 nW = (m_dwWidth + 1) / 2;
	const uint32 nH = (m_dwHeight + 1) / 2;
	for (uint32 y = 0; y <= nH ; ++y)
	{
		for (uint32 x = 0; x <= nW; ++x)
		{
			if (m_data[y * m_dwWidth + x] != m_data[y * m_dwWidth + (m_dwWidth - 1 - x)])
			{
				return false;
			}
			if (m_data[y * m_dwWidth + x] != m_data[(m_dwHeight - 1 - y) * m_dwWidth + x])
			{
				return false;
			}
			if (m_data[y * m_dwWidth + x] != m_data[(m_dwHeight - 1 - y) * m_dwWidth + (m_dwWidth - 1 - x)])
			{
				return false;
			}
		}
	}

	return true;
}


// create summed area table
void CSummedAreaTableFilterKernel::SumUpTableAndNormalize()
{
	for (uint32 y = 0; y < m_dwHeight; ++y)
	{
		int iFromLeftInRow = 0;

		for (uint32 x = 0; x < m_dwWidth; ++x)
		{
			iFromLeftInRow += m_data[y * m_dwWidth + x];

			m_data[y * m_dwWidth + x] = 
				(y != 0)
				? iFromLeftInRow + m_data[(y - 1) * m_dwWidth + x]
				: iFromLeftInRow;
		}
	}

	assert(m_data[m_dwHeight * m_dwWidth - 1] != 0);
	m_fCorrectionFactor = 1.0f / m_data[m_dwHeight * m_dwWidth - 1];
}
