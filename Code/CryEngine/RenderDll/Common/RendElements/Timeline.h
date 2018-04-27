// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Interpolator.h"

template<class T>
class Timeline
{
public:
	enum LoopMode { NORMAL, REVERSE, LOOP };

protected:
	T     prevYValue;
	float prevXValue;

public:
	Interpolator<T>* pInterp;
	T                startValue;
	T                endValue;
	LoopMode         loopMode;
	float            duration;    // in milliseconds
	float            currentTime; // in milliseconds

	void init(T start, T end, float duration, Interpolator<T>* interp)
	{
		SetInterpolationRange(start, end);
		prevYValue = start;
		prevXValue = 0;
		this->duration = duration;
		loopMode = NORMAL;

		pInterp = interp;
	}

protected:
	float computeTimeStepping(float curTime, float elapsedMs)
	{
		switch (loopMode)
		{
		case NORMAL:
			curTime += elapsedMs;
			if (curTime > duration)
				return duration;
		case REVERSE:
			curTime -= elapsedMs;
			if (curTime < 0)
				return 0;
		case LOOP:
			curTime += elapsedMs;
			if (curTime > duration)
				return fmod(curTime, duration);
		}

		return curTime;
	}

	void positCursorByRatio(float ratio)
	{
		switch (loopMode)
		{
		case NORMAL:
			currentTime = ratio * duration;
			break;
		case REVERSE:
			currentTime = (1 - ratio) * duration;
			break;
		case LOOP:
			break;
		}
	}

public:

	virtual ~Timeline() {}
	Timeline(T start, T end, float duration, Interpolator<T>* interp)
	{
		currentTime = 0;
		init(start, end, duration, interp);
	}

	Timeline(Interpolator<T>* interp)
	{
		currentTime = 0;
		init(0, 1, 1000, interp);
	}

	// Accumulate time and produce the interpolated value accordingly
	virtual T step(float elapsedMs)
	{
		currentTime = computeTimeStepping(currentTime, elapsedMs);
		prevXValue = currentTime / duration;

		prevYValue = pInterp->compute(startValue, endValue, prevXValue);
		return prevYValue;
	}

	// Rewind the current time to initial position
	virtual void rewind()
	{
		positCursorByRatio(0);
	}

	void SetInterpolationRange(T start, T end)
	{
		startValue = start;
		endValue = end;
	}

	float GetPrevXValue() { return prevXValue; }
	T     GetPrevYValue() { return prevYValue; }

};

namespace {
float tempFloat0 = 0;
float tempFloat1 = 1;
int tempInt0 = 0;
int tempInt1 = 1;
}

class TimelineFloat : public Timeline<float>
{
public:
	TimelineFloat() : Timeline<float>(0, 1, 1000, &InterpPredef::CUBIC_FLOAT) {}
};

class TimelineInt : public Timeline<int>
{
public:
	TimelineInt() : Timeline<int>(0, 1, 1000, &InterpPredef::CUBIC_INT) {}
};
