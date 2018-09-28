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
	nTime prevXValue;

public:
	Interpolator<T>* pInterp;
	T                startValue;
	T                endValue;
	LoopMode         loopMode;
	CTimeValue       duration;
	CTimeValue       currentTime;

	void init(T start, T end, const CTimeValue& duration, Interpolator<T>* interp)
	{
		SetInterpolationRange(start, end);
		prevYValue = start;
		prevXValue = 0;
		this->duration = duration;
		loopMode = NORMAL;

		pInterp = interp;
	}

protected:
	CTimeValue computeTimeStepping(const CTimeValue& curTimeIn, const CTimeValue& elapsed)
	{
		CTimeValue curTime = curTimeIn;
		switch (loopMode)
		{
		case NORMAL:
			curTime += elapsed;
			if (curTime > duration)
				return duration;
		case REVERSE:
			curTime -= elapsed;
			if (curTime < 0)
				return 0;
		case LOOP:
			curTime += elapsed;
			if (curTime > duration)
				return curTime % duration;
		}

		return curTime;
	}

	void positCursorByRatio(const mpfloat& ratio)
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
	Timeline(T start, T end, const CTimeValue& duration, Interpolator<T>* interp)
	{
		currentTime.SetSeconds(0);
		init(start, end, duration, interp);
	}

	Timeline(Interpolator<T>* interp)
	{
		currentTime.SetSeconds(0);
		init(0, 1, 1, interp);
	}

	// Accumulate time and produce the interpolated value accordingly
	virtual T step(const CTimeValue& elapsed)
	{
		currentTime = computeTimeStepping(currentTime, elapsed);
		prevXValue = currentTime / duration;

		prevYValue = pInterp->compute(startValue, endValue, BADF prevXValue);
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

	nTime GetPrevXValue() { return prevXValue; }
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
	TimelineFloat() : Timeline<float>(0, 1, 1, &InterpPredef::CUBIC_FLOAT) {}
};

class TimelineInt : public Timeline<int>
{
public:
	TimelineInt() : Timeline<int>(0, 1, 1, &InterpPredef::CUBIC_INT) {}
};
