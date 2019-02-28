// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef YCbCr_h
#define YCbCr_h

#include <CryMath/Cry_Color.h>

struct YCbCr
{
	float cr;
	float y;
	float cb;
	float a;

	operator ColorF() const
	{
		ColorF ret;

		const float crTmp = this->cr * 2.0f - 1.0f;
		const float  yTmp = this->y;
		const float cbTmp = this->cb * 2.0f - 1.0f;

		ret.r = yTmp                     + 1.402000f * crTmp;
		ret.g = yTmp - 0.344136f * cbTmp - 0.714136f * crTmp;
		ret.b = yTmp + 1.772000f * cbTmp                    ;

		ret.a = a;

		return ret;
	}

	YCbCr& operator =(const ColorF& rgbx)
	{
		float  yTmp =  0.299000f * rgbx.r + 0.587000f * rgbx.g + 0.114000f * rgbx.b;
		float cbTmp = -0.168736f * rgbx.r - 0.331264f * rgbx.g + 0.500000f * rgbx.b;
		float crTmp =  0.500000f * rgbx.r - 0.418688f * rgbx.g - 0.081312f * rgbx.b;

		this->cr = crTmp * 0.5f + 0.5f;
		this->y  =  yTmp;
		this->cb = cbTmp * 0.5f + 0.5f;

		a  = rgbx.a;

		return *this;
	}
};

#endif
