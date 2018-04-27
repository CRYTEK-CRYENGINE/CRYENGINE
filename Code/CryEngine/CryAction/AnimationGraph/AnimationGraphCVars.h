// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	CVars for the AnimationGraph Subsystem

   -------------------------------------------------------------------------
   History:
   - 02:03:2006  12:00 : Created by AlexL

*************************************************************************/

#ifndef __ANIMATIONGRAPH_CVARS_H__
#define __ANIMATIONGRAPH_CVARS_H__

#pragma once

struct ICVar;

struct CAnimationGraphCVars
{
	int                                 m_debugExactPos; // Enable exact positioning debug

	float                               m_landingBobTimeFactor;
	float                               m_landingBobLandTimeFactor;

	float                               m_distanceForceNoIk;
	float                               m_distanceForceNoLegRaycasts;
	int                                 m_spectatorCollisions;
	int                                 m_groundAlignAll;

	int                                 m_entityAnimClamp;

	float                               m_clampDurationEntity;
	float                               m_clampDurationAnimation;

	int                                 m_MCMH;
	int                                 m_MCMV;
	int                                 m_MCMFilter;
	int                                 m_TemplateMCMs;
	int                                 m_forceColliderModePlayer;
	int                                 m_forceColliderModeAI;
	int                                 m_enableExtraSolidCollider;
	int                                 m_debugLocations;
	ICVar*                              m_pDebugFilter;
	int                                 m_debugText;
	int                                 m_debugMotionParams;
	int                                 m_debugLocationsGraphs;
	int                                 m_debugAnimError;
	int                                 m_debugAnimTarget;
	int                                 m_debugColliderMode;
	ICVar*                              m_debugColliderModeName;
	int                                 m_debugMovementControlMethods;
	int                                 m_debugTempValues;
	int                                 m_debugFrameTime;
	int                                 m_debugEntityParams;
	int                                 m_forceSimpleMovement;
	int                                 m_disableSlidingContactEvents;
	int                                 m_debugAnimEffects;
	int                                 m_useMovementPrediction;
	int                                 m_useQueuedRotation;

	float                               m_turnSpeedParamScale;
	int                                 m_enableTurnSpeedSmoothing;
	int                                 m_enableTurnAngleSmoothing;
	int                                 m_enableTravelSpeedSmoothing;

	static inline CAnimationGraphCVars& Get()
	{
		assert(s_pThis != 0);
		return *s_pThis;
	}

private:
	friend class CCryAction; // Our only creator

	CAnimationGraphCVars(); // singleton stuff
	~CAnimationGraphCVars();
	CAnimationGraphCVars(const CAnimationGraphCVars&);
	CAnimationGraphCVars& operator=(const CAnimationGraphCVars&);

	static CAnimationGraphCVars* s_pThis;
};

#endif // __ANIMATIONGRAPH_CVARS_H__
