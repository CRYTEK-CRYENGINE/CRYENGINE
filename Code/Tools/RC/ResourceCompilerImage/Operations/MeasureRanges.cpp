// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>                         // assert()

#include "../ImageCompiler.h"               // CImageCompiler
#include "../ImageObject.h"                 // ImageToProcess

///////////////////////////////////////////////////////////////////////////////////

void ImageObject::GetComponentMaxima(ColorF& res) const
{
	if (GetPixelFormat() != ePixelFormat_A32B32G32R32F)
	{
		assert(0);
		RCLogError("%s: unsupported source format. Contact an RC programmer.", __FUNCTION__);
		return;
	}

	const uint32 mipLevel = 0;
	const ColorF* const pPixelsIn = GetPixelsPointer<ColorF>(mipLevel);
	const uint32 width = GetWidth(mipLevel);
	const uint32 height = GetHeight(mipLevel);

	res.set(-1.0f, -1.0f, -1.0f, -1.0f);

	const uint32 pixelCount = width * height;
	for (uint32 i = 0; i < pixelCount; ++i)
	{
		Util::clampMin(res.r, pPixelsIn[i].r);
		Util::clampMin(res.g, pPixelsIn[i].g);
		Util::clampMin(res.b, pPixelsIn[i].b);
		Util::clampMin(res.a, pPixelsIn[i].a);
	}
}


float ImageObject::CalculateAverageBrightness() const
{
	if (GetPixelFormat() != ePixelFormat_A32B32G32R32F)
	{
		assert(0);
		RCLogError("%s: unsupported source format. Contact an RC programmer.", __FUNCTION__);
		return 0.5f;
	}

	// Accumulate pixel colors of the top mip
	double avgOverall[3] = { 0.0, 0.0, 0.0 };

	const uint32 mipLevel = 0;
	const ColorF* const pPixelsIn = GetPixelsPointer<ColorF>(mipLevel);
	const uint32 width = GetWidth(mipLevel);
	const uint32 height = GetHeight(mipLevel);
	const double invWidth = 1.0 / width;

	uint32 pixelIndex = 0;
	for (uint32 y = 0; y < height; ++y)
	{
		double sumRow[3] = { 0.0, 0.0, 0.0 };
		for (uint32 x = 0; x < width; ++x)
		{
			sumRow[0] += pPixelsIn[pixelIndex].r;
			sumRow[1] += pPixelsIn[pixelIndex].g;
			sumRow[2] += pPixelsIn[pixelIndex].b;

			++pixelIndex;
		}

		// Accumulate averages
		avgOverall[0] += sumRow[0] * invWidth;
		avgOverall[1] += sumRow[1] * invWidth;
		avgOverall[2] += sumRow[2] * invWidth;
	}

	// Compute overall greyscale average
	const double invHeight = 1.0 / height;

	avgOverall[0] *= invHeight;
	avgOverall[1] *= invHeight;
	avgOverall[2] *= invHeight;

	const double avg = (avgOverall[0] + avgOverall[1] + avgOverall[2]) / 3;

	return (float)avg;
}


bool ImageObject::IsPerfectGreyscale(ColorF* epsilon) const
{
	if (GetPixelFormat() != ePixelFormat_A32B32G32R32F)
	{
		assert(0);
		RCLogError("%s: unsupported source format. Contact an RC programmer.", __FUNCTION__);
		return false;
	}

	const uint32 mipLevel = 0;
	const ColorF* const pPixelsIn = GetPixelsPointer<ColorF>(mipLevel);
	const uint32 width = GetWidth(mipLevel);
	const uint32 height = GetHeight(mipLevel);
	const uint32 pixelCount = width * height;

	if (!epsilon)
	{
		for (uint32 i = 0; i < pixelCount; ++i)
		{
			if (pPixelsIn[i].r != pPixelsIn[i].g || pPixelsIn[i].g != pPixelsIn[i].b)
			{
				// R=G=B is not the case
				return false;
			}
		}
	}
	else
	{
		ColorF gDelta = ColorF(0.0f);

		for (uint32 i = 0; i < pixelCount; ++i)
		{
			if (pPixelsIn[i].r != pPixelsIn[i].g || pPixelsIn[i].g != pPixelsIn[i].b)
			{
				// R=G=B is not the case
				float grey = (pPixelsIn[i].r * 0.33333333f + pPixelsIn[i].g * 0.33333334f + pPixelsIn[i].b * 0.33333333f);

				ColorF gPixel(grey, grey, grey, pPixelsIn[i].a);
				ColorF gVariance(gPixel - pPixelsIn[i]);

				gVariance.abs();
				gDelta.maximum(gDelta, gVariance);

				if (gVariance.r != 0 || gVariance.g != 0 || gVariance.b != 0)
				{
					bool fhm = true;
					(void)fhm;
				}
			}
		}

		*epsilon = gDelta;
		if (gDelta.r != 0.0f || gDelta.g != 0.0f || gDelta.b != 0.0f)
			return false;
	}

	return true;
}


bool ImageObject::HasSingleColor(int mip, const Vec4& color, float epsilon) const
{
	const EPixelFormat format = GetPixelFormat();
	if (format != ePixelFormat_A32B32G32R32F)
	{
		assert(0);
		RCLogError("%s: unsupported source format. Contact an RC programmer.", __FUNCTION__);
		return false;
	}

	uint32 width, height, mipCount;
	GetExtent(width, height, mipCount);

	for (uint32 i = 0; i < mipCount; ++i)
	{
		const uint32 m = (mip < 0) ? i : mip;

		width = GetWidth(m);
		height = GetHeight(m);

		char* buf;
		uint32 pitch;
		GetImagePointer(m, buf, pitch);

		for (uint32 y = 0; y < height; ++y)
		{
			const Vec4f* const pC = (const Vec4f *)&buf[pitch * y];

			for (uint32 x = 0; x < width; ++x)
			{
				if (!pC[x].IsEquivalent(color, epsilon))
				{
					return false;
				}				
			}
		}

		if (mip >= 0)
		{
			break;
		}
	}

	return true;
}


ImageObject::EAlphaContent ImageObject::ClassifyAlphaContent() const
{
	const EPixelFormat format = GetPixelFormat();
	const PixelFormatInfo* info = CPixelFormats::GetPixelFormatInfo(format);

	if (!info->bHasAlpha)
	{
		return eAlphaContent_Absent;
	}

	if (format != ePixelFormat_A8 &&
	    format != ePixelFormat_A8L8 &&
	    format != ePixelFormat_A8R8G8B8 &&
	    format != ePixelFormat_A16B16G16R16 &&
	    format != ePixelFormat_A16B16G16R16F &&
	    format != ePixelFormat_A32B32G32R32F)
	{
		assert(0);
		RCLogError("%s: unsupported source format. Contact an RC programmer.", __FUNCTION__);
		return eAlphaContent_Indeterminate;
	}

	uint32 width, height, mipCount;
	GetExtent(width, height, mipCount);

	char* buf;
	uint32 pitch;
	GetImagePointer(0, buf, pitch);

	// counts of blacks and white
	uint nBlacks = 0;
	uint nWhites = 0;

	if (format == ePixelFormat_A32B32G32R32F)
	{
		const size_t alphaOffset = 3;
		const size_t channelCount = 4;

		for (uint32 y = 0; y < height; ++y)
		{
			const float* colbuf = (const float *)&buf[pitch * y] + alphaOffset;

			for (uint32 x = 0; x < width; ++x)
			{
				if (*colbuf == 0.0f)
				{
					++nBlacks;
				}
				else if (*colbuf == 1.0f)
				{
					++nWhites;
				}
				else
				{
					return eAlphaContent_Greyscale;
				}

				colbuf += channelCount;
			}
		}
	}
	else if (format == ePixelFormat_A16B16G16R16F)
	{
		const size_t alphaOffset = 3;
		const size_t channelCount = 4;

		for (uint32 y = 0; y < height; ++y)
		{
			const SHalf* colbuf = (const SHalf *)&buf[pitch * y] + alphaOffset;

			for (uint32 x = 0; x < width; ++x)
			{
				if (*colbuf == 0.0f)
				{
					++nBlacks;
				}
				else if (*colbuf == 1.0f)
				{
					++nWhites;
				}
				else
				{
					return eAlphaContent_Greyscale;
				}

				colbuf += channelCount;
			}
		}
	}
	else if (format == ePixelFormat_A16B16G16R16)
	{
		const size_t alphaOffset = 3;
		const size_t channelCount = 4;

		for (uint32 y = 0; y < height; ++y)
		{
			const uint16* colbuf = (const uint16 *)&buf[pitch * y] + alphaOffset;

			for (uint32 x = 0; x < width; ++x)
			{
				if (*colbuf == 0x0000)
				{
					++nBlacks;
				}
				else if (*colbuf == 0xFFFF)
				{
					++nWhites;
				}
				else
				{
					return eAlphaContent_Greyscale;
				}

				colbuf += channelCount;
			}
		}
	}
	else 
	{
		size_t alphaOffset;
		size_t channelCount;

		if (format == ePixelFormat_A8R8G8B8)
		{
			alphaOffset = 3;
			channelCount = 4;
		}
		else if (format == ePixelFormat_A8L8)
		{
			alphaOffset = 1;
			channelCount = 2;
		}
		else // format == ePixelFormat_A8
		{
			alphaOffset = 0;
			channelCount = 1;
		}

		for (uint32 y = 0; y < height; ++y)
		{
			const uint8* colbuf = (const uint8 *)&buf[pitch * y] + alphaOffset;

			for (uint32 x = 0; x < width; ++x)
			{
				if (*colbuf == 0x00)
				{
					++nBlacks;
				}
				else if (*colbuf == 0xFF)
				{
					++nWhites;
				}
				else
				{
					return eAlphaContent_Greyscale;
				}

				colbuf += channelCount;
			}
		}
	}

	if (nBlacks == 0)
	{
		return eAlphaContent_OnlyWhite;
	}

	if (nWhites == 0)
	{
		return eAlphaContent_OnlyBlack;
	}

	return eAlphaContent_OnlyBlackAndWhite;
}
