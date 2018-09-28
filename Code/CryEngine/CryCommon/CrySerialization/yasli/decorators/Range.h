// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once 

#include <CrySerialization/yasli/Archive.h>

#include <limits>

namespace yasli
{
MPOff  ILINE T DefaultSinglestep() { return (T)(std::is_integral<T>::value ? mpfloat(1) : mpfloat("0.1")); }
MPOnly ILINE T DefaultSinglestep() { return T("0.1"); }

template<class T>
struct RangeDecorator
{
	T* value;
	T hardMin;
	T hardMax;

	// Limit for UI elements such as sliders
	T softMin;

	// Limit for UI elements such as sliders
	T softMax;
	T singleStep;

	void YASLI_SERIALIZE_METHOD(Archive& ar) {}
};

template<class T>
RangeDecorator<T> Range(T& value, T hardMin, T hardMax, T singleStep = yasli::DefaultSinglestep<T>())
{
	if (value < hardMin)
	{
		value = hardMin;
	}
	else if (value > hardMax)
	{
		value = hardMax;
	}

	RangeDecorator<T> r;
	r.value = &value;
	r.softMin = r.hardMin = hardMin;
	r.softMax = r.hardMax = hardMax;
	r.singleStep = singleStep;
	return r;
}

template<class T>
RangeDecorator<T> Range(T& value, T hardMin, T hardMax, T softMin, T softMax, T singleStep = yasli::DefaultSinglestep<T>())
{
	if (value < hardMin)
	{
		value = hardMin;
	}
	else if (value > hardMax)
	{
		value = hardMax;
	}

	RangeDecorator<T> r;
	r.value = &value;
	r.hardMin = hardMin;
	r.hardMax = hardMax;
	r.softMin = softMin;
	r.softMax = softMax;
	r.singleStep = singleStep;
	return r;
}

template<class T>
RangeDecorator<T> MinMaxRange(T& value, T singleStep = yasli::DefaultSinglestep<T>())
{
	return Range(value, std::numeric_limits<T>::lowest(), std::numeric_limits<T>::max(), singleStep);
}

template<class T>
bool YASLI_SERIALIZE_OVERRIDE(Archive& ar, RangeDecorator<T>& value, const char* name, const char* label)
{
	if (ar.isEdit())
		return ar(Serializer(value), name, label);
	else
		return ar(*value.value, name, label);
}

}
