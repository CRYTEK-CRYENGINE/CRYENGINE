// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include <CrySerialization/Forward.h>

namespace Serialization
{
struct SSliderF
{
	SSliderF(float* value, float minLimit, float maxLimit)
		: valuePointer(value)
		, minLimit(minLimit)
		, maxLimit(maxLimit)
	{
	}

	SSliderF()
		: valuePointer(0)
		, minLimit(0.0f)
		, maxLimit(1.0f)
	{
	}

	float* valuePointer;
	float  minLimit;
	float  maxLimit;
};

struct SSliderI
{
	SSliderI(int* value, int minLimit, int maxLimit)
		: valuePointer(value)
		, minLimit(minLimit)
		, maxLimit(maxLimit)
	{
	}

	SSliderI()
		: valuePointer(0)
		, minLimit(0)
		, maxLimit(1)
	{
	}

	int* valuePointer;
	int  minLimit;
	int  maxLimit;
};

inline SSliderF Slider(float& value, float minLimit, float maxLimit)
{
	return SSliderF(&value, minLimit, maxLimit);
}

inline SSliderI Slider(int& value, int minLimit, int maxLimit)
{
	return SSliderI(&value, minLimit, maxLimit);
}

bool Serialize(IArchive& ar, SSliderF& slider, const char* name, const char* label);
bool Serialize(IArchive& ar, SSliderI& slider, const char* name, const char* label);

namespace Decorators
{
//! OBSOLETE NAME, please use Serialization::Slider instead (without Decorators namespace).
using Serialization::Slider;
}

}

#include "SliderImpl.h"
