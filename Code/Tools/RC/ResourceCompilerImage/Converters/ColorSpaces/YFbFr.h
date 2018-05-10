// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef YFbFr_h
#define YFbFr_h

#include <CryMath/Cry_Color.h>

struct YFbFr
{
	float fr;
	float y;
	float fb;
	float a;

	operator ColorF() const
	{
		ColorF ret;

		const float frTmp = fr * 2.0f - 1.0f;
		const float  yTmp = y;
		const float fbTmp = fb * 2.0f - 1.0f;

		ret.r = yTmp - 3.0f / 8.0f * fbTmp + 1.0f / 2.0f * frTmp;
		ret.g = yTmp + 5.0f / 8.0f * fbTmp;
		ret.b = yTmp - 3.0f / 8.0f * fbTmp - 1.0f / 2.0f * frTmp;

		ret.a = a;

		return ret;
	}

	YFbFr& operator =(const ColorF& rgbx)
	{
		const float  yTmp =  5.0f / 16.0f * rgbx.r + 3.0f / 8.0f * rgbx.g + 5.0f / 16.0f * rgbx.b;
		const float fbTmp = -1.0f /  2.0f * rgbx.r +        1.0f * rgbx.g - 1.0f /  2.0f * rgbx.b;
		const float frTmp =          1.0f * rgbx.r                        -         1.0f * rgbx.b;

		fr = frTmp * 0.5f + 0.5f;
		y  =  yTmp;
		fb = fbTmp * 0.5f + 0.5f;

		a  = rgbx.a;

		return *this;
	}
};

#endif
