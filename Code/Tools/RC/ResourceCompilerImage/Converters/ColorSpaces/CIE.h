// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef CIE_h
#define CIE_h

#include <CryMath/Cry_Color.h>

struct CIE
{
	// (I)ntensity, (R)ed chromaticity, (B)lue chromaticity, (A)lpha
	// NOTE: http://en.wikipedia.org/wiki/CIE_1931_color_space
	float i;
	float r;
	float b;
	float a;

	static inline float saturate(float x)
	{
		return std::max(0.0f, std::min(x, 1.0f));
	}

	operator ColorF() const
	{
		ColorF ret;

		const float bTmp = b;
		const float rTmp = r;
		const float iTmp = i * 3.0f;

		// LUV/Yxz 565-storeable chromaticity space
		ret.b = bTmp * iTmp;
		ret.r = rTmp * iTmp;
		ret.g = iTmp - (ret.r + ret.b);

		ret.r = saturate(ret.r);
		ret.g = saturate(ret.g);
		ret.b = saturate(ret.b);

		ret.a = a;

		return ret;
	}

	CIE& operator =(const ColorF& rgbx)
	{
		const float iTmp = rgbx.g + (rgbx.r + rgbx.b);
		const float rTmp = (iTmp != 0.0f) ? rgbx.r / iTmp : (0.0f);
		const float bTmp = (iTmp != 0.0f) ? rgbx.b / iTmp : (0.0f);

		i = saturate(iTmp / 3.0f);
		r = saturate(rTmp);
		b = saturate(bTmp);

		a = rgbx.a;

		return *this;
	}
};

#endif
