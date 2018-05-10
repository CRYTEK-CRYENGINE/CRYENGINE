// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Controls player camera update (Refactored from PlayerView)

-------------------------------------------------------------------------
History:
- 15:10:2009   Created by Benito Gangoso Rodriguez

*************************************************************************/

#pragma once

#ifndef _PLAYER_CAMERA_H_
#define _PLAYER_CAMERA_H_

#include "ICameraMode.h"

class CPlayer;

class CPlayerCamera
{
public:

	CPlayerCamera(const CPlayer & ownerPlayer);
	~CPlayerCamera();

	bool Update(SViewParams& viewParams, float frameTime);
	void SetCameraMode(ECameraMode newMode, const char * why);
	void SetCameraModeWithAnimationBlendFactors(ECameraMode newMode, const ICameraMode::AnimationSettings& animationSettings, const char* why);

	void PostUpdate(const QuatT &camDelta);

	ILINE bool IsTransitioning() const
	{
		return (m_transitionTime > 0.0f);
	}

private:
	ECameraMode GetCurrentCameraMode();
	void UpdateCommon(SViewParams& viewParams, float frameTime);
	void FilterGroundOnlyShakes(SViewParams& viewParams);

	void UpdateTotalTransitionTime();

	// No need to use a dynamic container (eg. vector) as we know exactly how many modes there are going to be... [TF]
	ICameraMode * m_cameraModes[eCameraMode_Last];
	ECameraMode m_currentCameraMode;
	ECameraMode m_previousCameraMode;
	const CPlayer & m_ownerPlayer;

	float		m_transitionTime;
	float		m_totalTransitionTime;
	ECameraMode m_transitionCameraMode;

	bool m_enteredPartialAnimControlledCameraOnLedge;
};

#endif