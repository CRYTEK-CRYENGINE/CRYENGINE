// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#ifndef IRB_h
#define IRB_h

#include <CryMath/Cry_Color.h>

struct IRB
{
	// (I)ntensity, (R)ed chromaticity, (B)lue chromaticity, (A)lpha
	// NOTE: Crytek original research
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
		const float iTmp = i * 2.0f;

		// LUV inspired 565-storeable chromaticity space
		ret.b = bTmp * iTmp;
		ret.r = rTmp * iTmp;
		ret.g = iTmp - std::max(ret.r, ret.b);

		ret.r = saturate(ret.r);
		ret.g = saturate(ret.g);
		ret.b = saturate(ret.b);

		ret.a = a;

		return ret;
	}

	IRB& operator =(const ColorF& rgbx)
	{
		const float iTmp = rgbx.g + std::max(rgbx.r, rgbx.b);
		const float rTmp = (iTmp != 0.0f) ? rgbx.r / iTmp : (0.0f);
		const float bTmp = (iTmp != 0.0f) ? rgbx.b / iTmp : (0.0f);

		i = saturate(iTmp * 0.5f);
		r = saturate(rTmp);
		b = saturate(bTmp);

		a = rgbx.a;

		return *this;
	}
};

#endif
