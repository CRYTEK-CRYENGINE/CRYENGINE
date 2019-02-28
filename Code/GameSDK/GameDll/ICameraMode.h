// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Camera mode interface, enums, etc

-------------------------------------------------------------------------
History:
- 15:10:2009   Created by Benito Gangoso Rodriguez

*************************************************************************/

#pragma once

#ifndef _ICAMERA_MODE_H_
#define _ICAMERA_MODE_H_

#include "AutoEnum.h"

#define CameraModeList(f)            \
	f(eCameraMode_Default)             \
	f(eCameraMode_SpectatorFollow)     \
	f(eCameraMode_SpectatorFixed)      \
	f(eCameraMode_AnimationControlled) \
	f(eCameraMode_Death)               \
	f(eCameraMode_Death_SinglePlayer)	 \
	f(eCameraMode_Vehicle)             \
	f(eCameraMode_PartialAnimationControlled)					 \

AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(ECameraMode, CameraModeList, eCameraMode_Invalid, eCameraMode_Last);

class CPlayer;
struct SViewParams;

struct ICameraMode
{
	struct AnimationSettings
	{
		AnimationSettings()
			: positionFactor(0.0f)
			, rotationFactor(0.0f)
			,	applyResult(false)
			, stableBlendOff(false)
		{

		}

		float positionFactor;
		float rotationFactor;
		bool  applyResult;
		bool  stableBlendOff;
	};

	ICameraMode();
	virtual ~ICameraMode();

	void ActivateMode(const CPlayer & clientPlayer);
	void DeactivateMode(const CPlayer & clientPlayer);

	virtual bool UpdateView(const CPlayer& clientPlayer, SViewParams& viewParams, float frameTime) = 0;
	
	virtual bool CanTransition();
	virtual void SetCameraAnimationFactor(const AnimationSettings& animationSettings);
	virtual void GetCameraAnimationFactor(float &pos, float &rot);

	ILINE const bool IsBlendingOff() const
	{
		return m_isBlendingOff;
	}

protected:
	virtual void Activate(const CPlayer & clientPlayer);
	virtual void Deactivate(const CPlayer & clientPlayer);

	void SetDrawNearestFlag(const CPlayer & clientPlayer, bool bSetDrawNearestFlag);

	bool m_isBlendingOff;
	bool m_disableDrawNearest;
};

#endif
