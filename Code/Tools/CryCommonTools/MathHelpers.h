// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MATHHELPERS_H__
#define __MATHHELPERS_H__

#include <float.h>
#if (_M_IX86_FP > 0)
#include <intrin.h>
#endif

namespace MathHelpers
{
#if (_M_IX86_FP > 0)
	inline int FastRoundFloatTowardZero(float f)
	{
		return _mm_cvtt_ss2si(_mm_set_ss(f));
	}
#else
	inline int FastRoundFloatTowardZero(float f)
	{
		return int(f);
	}
#endif

	inline unsigned int EnableFloatingPointExceptions(unsigned int mask)
	{
		_clearfp();
		unsigned int oldMask;
		_controlfp_s(&oldMask, 0, 0);
		unsigned int newMask;
		_controlfp_s(&newMask, ~mask, _MCW_EM);
		return ~oldMask;
	}

	class AutoFloatingPointExceptions
	{
	public:
		AutoFloatingPointExceptions(const unsigned int mask)
			: m_mask(EnableFloatingPointExceptions(mask))
		{
		}

		~AutoFloatingPointExceptions()
		{
			EnableFloatingPointExceptions(m_mask);
		}

	private:
		unsigned int m_mask;
	};
}

#endif


