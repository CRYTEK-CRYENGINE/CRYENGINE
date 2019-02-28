// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//	Interpolator deals with animations' interpolation.
//	These are bare bones interpolators: they take 3 key values as inputs.
//	The first two specify the two end points(start and end), and the last one specify the interpolation tweenness (x-value).
//	The output is the y-value.
//	For additional time-based playback type animation, see Timeline.

template<class T>
class Interpolator
{
protected:
	virtual T mix(T v0, T v1, float mixRatio)
	{
		return (T)(v0 * (1 - mixRatio) + v1 * mixRatio);
	}
public:
	virtual ~Interpolator(){}
	virtual T compute(T start, T end, float xValue) = 0;
};

template<class T>
class LinearInterp : public Interpolator<T>
{
public:
	virtual T compute(T start, T end, float xValue)
	{
		return Interpolator<T>::mix(start, end, xValue);
	}
};

template<class T>
class CubicInterp : public Interpolator<T>
{
public:
	virtual T compute(T start, T end, float xValue)
	{
		float x = xValue * xValue * (3 - 2 * xValue);
		return Interpolator<T>::mix(start, end, x);
	}
};

namespace InterpPredef
{
static LinearInterp<float> Linear_FLOAT;
static LinearInterp<int> Linear_INT;

static CubicInterp<float> CUBIC_FLOAT;
static CubicInterp<int> CUBIC_INT;
}
