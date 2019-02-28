// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
// Description: EyeTracker for Windows (covering EyeX)
// - 20/04/2016 Created by Benjamin Peters
// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryInput/IInput.h>

#ifdef CRY_PLATFORM_WINDOWS
#ifdef USE_EYETRACKER

class CEyeTrackerInput : public IEyeTrackerInput
{
public:
	CEyeTrackerInput();
	~CEyeTrackerInput();

	virtual bool        Init();
	virtual void        Update();

	void                SetEyeProjection(int iX, int iY);

private:
	bool    m_bProjectionChanged;
	float   m_fEyeX;
	float   m_fEyeY;
};

#endif //USE_EYETRACKER
#endif //CRY_PLATFORM_WINDOWS
