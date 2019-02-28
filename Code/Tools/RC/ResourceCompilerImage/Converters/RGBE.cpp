// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>                         // assert()

#include "../ImageCompiler.h"               // CImageCompiler
#include "../ImageObject.h"                 // ImageToProcess

#include "IRCLog.h"                         // IRCLog

///////////////////////////////////////////////////////////////////////////////////

#define RGB9E5_EXPONENT_BITS          5
#define RGB9E5_MANTISSA_BITS          9
#define RGB9E5_EXP_BIAS               15
#define RGB9E5_MAX_VALID_BIASED_EXP   31

#define MAX_RGB9E5_EXP               (RGB9E5_MAX_VALID_BIASED_EXP - RGB9E5_EXP_BIAS)
#define RGB9E5_MANTISSA_VALUES       (1 << RGB9E5_MANTISSA_BITS)
#define MAX_RGB9E5_MANTISSA          (RGB9E5_MANTISSA_VALUES - 1)
#define MAX_RGB9E5                   (((float)MAX_RGB9E5_MANTISSA) / RGB9E5_MANTISSA_VALUES * (1 << MAX_RGB9E5_EXP))
#define EPSILON_RGB9E5               ((1.0 / RGB9E5_MANTISSA_VALUES) / (1 << RGB9E5_EXP_BIAS))

static int ilog2(float x)
{
	int32 bitfield = *(reinterpret_cast<uint32*>(&x));
	bitfield &= ~0x80000000;

	return ((bitfield >> 23) - 127);
}

struct RgbE {
	unsigned int r : RGB9E5_MANTISSA_BITS;
	unsigned int g : RGB9E5_MANTISSA_BITS;
	unsigned int b : RGB9E5_MANTISSA_BITS;
	unsigned int e : RGB9E5_EXPONENT_BITS;

	operator ColorF() const
	{
		int exponent = e - RGB9E5_EXP_BIAS - RGB9E5_MANTISSA_BITS;
		float scale = powf(2.0f, exponent);
		ColorF ret;

		ret.r = r * scale;
		ret.g = g * scale;
		ret.b = b * scale;

		return ret;
	}

	void operator =(const ColorF& rgbx)
	{
		float rf = Util::getMax(0.0f, Util::getMin(rgbx.r, MAX_RGB9E5));
		float gf = Util::getMax(0.0f, Util::getMin(rgbx.g, MAX_RGB9E5));
		float bf = Util::getMax(0.0f, Util::getMin(rgbx.b, MAX_RGB9E5));
		float mf = Util::getMax(rf, Util::getMax(gf, bf));

		e = Util::getMax(0, ilog2(mf) + (RGB9E5_EXP_BIAS + 1));

		int exponent = e - RGB9E5_EXP_BIAS - RGB9E5_MANTISSA_BITS;
		float scale = powf(2.0f, exponent);

		r = Util::getMin(511, (int)floorf(rf / scale + 0.5f));
		g = Util::getMin(511, (int)floorf(gf / scale + 0.5f));
		b = Util::getMin(511, (int)floorf(bf / scale + 0.5f));
	}
};

///////////////////////////////////////////////////////////////////////////////////

void ImageToProcess::ConvertBetweenRGB32FAndRGBE(EPixelFormat dstFormat)
{
	if ((get()->GetPixelFormat() != ePixelFormat_A32B32G32R32F && get()->GetPixelFormat() != ePixelFormat_E5B9G9R9) ||
		  (dstFormat != ePixelFormat_A32B32G32R32F && dstFormat != ePixelFormat_E5B9G9R9))
	{
		assert(0);
		set(0);
		return;
	}

	if (get()->GetPixelFormat() == ePixelFormat_A32B32G32R32F && dstFormat == ePixelFormat_E5B9G9R9)
	{
		std::auto_ptr<ImageObject> pRet(get()->AllocateImage(0, dstFormat));

		const uint32 dwMips = pRet->GetMipCount();
		for (uint32 dwMip = 0; dwMip < dwMips; ++dwMip)
		{
			const ColorF* const pPixelsIn = get()->GetPixelsPointer<ColorF>(dwMip);
			RgbE* const pPixelsOut = pRet->GetPixelsPointer<RgbE>(dwMip);

			const uint32 pixelCount = get()->GetPixelCount(dwMip);

			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pPixelsOut[i] = pPixelsIn[i];
			}
		}

		set(pRet.release());
	}

	else if (get()->GetPixelFormat() == ePixelFormat_E5B9G9R9 && dstFormat == ePixelFormat_A32B32G32R32F)
	{
		std::auto_ptr<ImageObject> pRet(get()->AllocateImage(0, dstFormat));

		const uint32 dwMips = pRet->GetMipCount();
		for (uint32 dwMip = 0; dwMip < dwMips; ++dwMip)
		{
			const RgbE* const pPixelsIn = get()->GetPixelsPointer<RgbE>(dwMip);
			ColorF* const pPixelsOut = pRet->GetPixelsPointer<ColorF>(dwMip);

			const uint32 pixelCount = get()->GetPixelCount(dwMip);

			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pPixelsOut[i] = (ColorF)pPixelsIn[i];
			}
		}

		set(pRet.release());
	}
}
