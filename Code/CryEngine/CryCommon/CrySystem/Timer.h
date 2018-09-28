// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct Timer
{
	Timer()
		: endTime(-1)
	{
	}

	void Reset(const CTimeValue& duration, const CTimeValue& variation = CTimeValue(0))
	{
		endTime = gEnv->pSystem->GetITimer()->GetFrameStartTime() + duration + cry_random(CTimeValue(0), variation);
	}

	bool Elapsed() const
	{
		return endTime >= 0 && gEnv->pSystem->GetITimer()->GetFrameStartTime() >= endTime;
	}

	mpfloat GetSecondsLeft() const
	{
		return (endTime - gEnv->pSystem->GetITimer()->GetFrameStartTime()).GetSeconds();
	}

	CTimeValue endTime;
};
