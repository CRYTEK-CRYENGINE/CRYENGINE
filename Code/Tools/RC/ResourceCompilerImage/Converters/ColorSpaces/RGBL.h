// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef RGBL_h
#define RGBL_h

#include <CryMath/Cry_Color.h>

namespace RGBL
{

	template<typename T>
	static inline T GetLuminance(const T& r, const T& g, const T& b)
	{
		return (r * 30 + g * 59 + b * 11 + 50) / 100;
	}

	template<>
	static inline float GetLuminance<float>(const float& r, const float& g, const float& b)
	{
		return (r * 0.30f + g * 0.59f + b * 0.11f);
	}

	static inline uint8 GetLuminance(const ColorB& rgbx)
	{
		const ColorB col = rgbx;

		return GetLuminance(col.r, col.g, col.b);
	}

	static inline float GetLuminance(const ColorF& rgbx)
	{
		const ColorF col = rgbx;

		return GetLuminance(col.r, col.g, col.b);
	}

}

#endif
