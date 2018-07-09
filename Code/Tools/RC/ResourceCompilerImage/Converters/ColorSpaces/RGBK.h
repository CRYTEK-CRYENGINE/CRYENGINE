// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef RGBK_h
#define RGBK_h

#include <CryMath/Cry_Color.h>

// Compressing float RGB HDR color into ubyte RGBK
namespace RGBK
{
	static inline ColorB CompressSquared(ColorF color, const int rgbkMaxValue, bool bSparseK)
	{
		color.ScaleCol(1.f / rgbkMaxValue);
		color.Clamp(0.0f, 1.0f);

		const float mx = color.Max();
		int k = Util::getClamped(int(sqrtf(mx) * 255.0f), 1, 255);
		while (Util::square(k / 255.0f) < mx)
		{
			++k;
		}
		assert(k <= 255);

		if (bSparseK)
		{
			// 1->1, 2->8, 3->8, ..., 8->8, 9->15, ..., 15->15, 16->22, ..., 22->22, 23->29, ...
			// Step 7 (6 in-between values) is chosen because DXT5 compressor
			// can encode alphas with difference 6 or less without any loss.
			k = ((k + 5) / 7) * 7 + 1;
			// Make sure that the last range has big enough difference
			if (k + 7 > 255) 
			{
				k = 255;
			}
		}

		color.ScaleCol(1.0f / Util::square(k / 255.0f));
		assert(color.Max() < 1.001f);

		ColorB res;
		res.b = uint8(color.r * 255.0f + 0.5f);
		res.g = uint8(color.g * 255.0f + 0.5f);
		res.r = uint8(color.b * 255.0f + 0.5f);
		res.a = uint8(k);

		return res;
	}

	static inline ColorB CompressSquared(ColorF color, const int rgbkMaxValue, uint8 forcedK)
	{
		if (forcedK <= 0)
		{
			assert(0);
			forcedK = 1;
		}

		color.ScaleCol(1.f / rgbkMaxValue);
		color.Clamp(0.0f, 1.0f);

		const float mx = color.Max();
		int k = Util::getClamped(int(sqrtf(mx) * 255.0f), 1, 255);
		while (Util::square(k / 255.0f) < mx)
		{
			++k;
		}
		assert(k <= 255);

		if (k < (int)forcedK)
		{
			k = forcedK;
		}

		color.ScaleCol(1.0f / Util::square(k / 255.0f));
		assert(color.Max() < 1.001f);

		ColorB res;
		res.b = uint8(color.r * 255.0f + 0.5f);
		res.g = uint8(color.g * 255.0f + 0.5f);
		res.r = uint8(color.b * 255.0f + 0.5f);
		res.a = uint8(k);

		return res;
	}


	static inline ColorB Compress(ColorF color, const int rgbkMaxValue, bool bSparseK)
	{
		color.ScaleCol(1.f / rgbkMaxValue);
		color.Clamp(0.0f, 1.0f);

		int k = Util::getClamped(int(ceilf(color.Max() * 255.0f)), 1, 255);

		if (bSparseK)
		{
			// 1->1, 2->8, 3->8, ..., 8->8, 9->15, ..., 15->15, 16->22, ..., 22->22, 23->29, ...
			// Step 7 (6 in-between values) is chosen because DXT5 compressor
			// can encode alphas with difference 6 or less without any loss.
			k = ((k + 5) / 7) * 7 + 1;
			// Make sure that the last range has big enough difference
			if (k + 7 > 255) 
			{
				k = 255;
			}
		}

		// c' = c / (k / 255)
		color.ScaleCol(255.0f / k);
		assert(color.Max() < 1.001f);

		ColorB res;
		res.b = uint8(color.r * 255.0f + 0.5f);
		res.g = uint8(color.g * 255.0f + 0.5f);
		res.r = uint8(color.b * 255.0f + 0.5f);
		res.a = uint8(k);

		return res;
	}

	static inline ColorB Compress(ColorF color, const int rgbkMaxValue, uint8 forcedK)
	{
		if (forcedK <= 0)
		{
			assert(0);
			forcedK = 1;
		}

		color.ScaleCol(1.f / rgbkMaxValue);
		color.Clamp(0.0f, 1.0f);

		int k = Util::getClamped(int(ceilf(color.Max() * 255.0f)), 1, 255);

		if (k < (int)forcedK)
		{
			k = forcedK;
		}

		// c' = c / (k / 255)
		color.ScaleCol(255.0f / k);
		assert(color.Max() < 1.001f);

		ColorB res;
		res.b = uint8(color.r * 255.0f + 0.5f);
		res.g = uint8(color.g * 255.0f + 0.5f);
		res.r = uint8(color.b * 255.0f + 0.5f);
		res.a = uint8(k);

		return res;
	}
}

#endif
