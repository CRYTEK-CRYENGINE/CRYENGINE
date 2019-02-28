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

#include "StdAfx.h"

#include "AnimationGraphCVars.h"
#include "../Animation/PoseAligner/PoseAligner.h"
#include <CryAction.h>

CAnimationGraphCVars* CAnimationGraphCVars::s_pThis = 0;

CAnimationGraphCVars::CAnimationGraphCVars()
{
	assert(s_pThis == 0);
	s_pThis = this;

	IConsole* pConsole = gEnv->pConsole;
	assert(pConsole);

	// TODO: remove once animation graph transition is complete
	REGISTER_CVAR2("ag_debugExactPos", &m_debugExactPos, 0, VF_CHEAT, "Enable/disable exact positioning debugger");
	// ~TODO

	REGISTER_CVAR2("ac_forceSimpleMovement", &m_forceSimpleMovement, 0, VF_CHEAT, "Force enable simplified movement (not visible, dedicated server, etc).");

	REGISTER_CVAR2("ac_debugAnimEffects", &m_debugAnimEffects, 0, VF_CHEAT, "Print log messages when anim events spawn effects.");

	REGISTER_CVAR2("ac_movementControlMethodHor", &m_MCMH, 0, VF_CHEAT, "Overrides the horizontal movement control method specified by AG (overrides filter).");
	REGISTER_CVAR2("ac_movementControlMethodVer", &m_MCMV, 0, VF_CHEAT, "Overrides the vertical movement control method specified by AG (overrides filter).");
	REGISTER_CVAR2("ac_movementControlMethodFilter", &m_MCMFilter, 0, VF_CHEAT, "Force reinterprets Decoupled/CatchUp MCM specified by AG as Entity MCM (H/V overrides override this).");
	REGISTER_CVAR2("ac_templateMCMs", &m_TemplateMCMs, 1, VF_CHEAT, "Use MCMs from AG state templates instead of AG state headers.");
	REGISTER_CVAR2("ac_ColliderModePlayer", &m_forceColliderModePlayer, 0, VF_CHEAT, "Force override collider mode for all players.");
	REGISTER_CVAR2("ac_ColliderModeAI", &m_forceColliderModeAI, 0, VF_CHEAT, "Force override collider mode for all AI.");
	REGISTER_CVAR2("ac_enableExtraSolidCollider", &m_enableExtraSolidCollider, 1, VF_CHEAT, "Enable extra solid collider (for non-pushable characters).");

	REGISTER_CVAR2("ac_entityAnimClamp", &m_entityAnimClamp, 1, VF_CHEAT, "Forces the entity movement to be limited by animation.");
	REGISTER_CVAR2("ac_debugAnimTarget", &m_debugAnimTarget, 0, 0, "Display debug history graphs of anim target correction.");
	REGISTER_CVAR2("ac_debugLocationsGraphs", &m_debugLocationsGraphs, 0, 0, "Display debug history graphs of entity locations and movement.");

	REGISTER_CVAR2("ac_debugLocations", &m_debugLocations, 0, 0, "Debug render entity location.");
	REGISTER_CVAR2("ac_debugMotionParams", &m_debugMotionParams, 0, 0, "Display graph of motion parameters.");

	REGISTER_CVAR2("ac_frametime", &m_debugFrameTime, 0, 0, "Display a graph of the frametime.");

	REGISTER_CVAR2("ac_debugText", &m_debugText, 0, 0, "Display entity/animation location/movement values, etc.");
	REGISTER_CVAR2("ac_debugXXXValues", &m_debugTempValues, 0, 0, "Display some values temporarily hooked into temp history graphs.");
	REGISTER_CVAR2("ac_debugEntityParams", &m_debugEntityParams, 0, VF_CHEAT, "Display entity params graphs");
	m_pDebugFilter = REGISTER_STRING("ac_DebugFilter", "0", 0, "Debug specified entity name only.");

	REGISTER_CVAR2("ac_debugColliderMode", &m_debugColliderMode, 0, 0, "Display filtered and requested collider modes.");
	REGISTER_CVAR2("ac_debugMovementControlMethods", &m_debugMovementControlMethods, 0, 0, "Display movement control methods.");

	REGISTER_CVAR2("ac_disableSlidingContactEvents", &m_disableSlidingContactEvents, 0, 0, "Force disable sliding contact events.");
	REGISTER_CVAR2("ac_useMovementPrediction", &m_useMovementPrediction, 1, 0, "When using animation driven motion sample animation for the root one frame ahead to take into account 1 frame of physics delay");
	REGISTER_CVAR2("ac_useQueuedRotation", &m_useQueuedRotation, 0, 0, "Instead of applying rotation directly, queue it until the beginning of the next frame (synchronizes rotation with translation coming from asynchronous physics)");

	REGISTER_CVAR2("g_landingBobTimeFactor", &m_landingBobTimeFactor, 0.2f, VF_CHEAT, "Fraction of the bob time to release full compression");
	REGISTER_CVAR2("g_landingBobLandTimeFactor", &m_landingBobLandTimeFactor, 0.1f, VF_CHEAT, "Fraction of the bob time to be at full compression");

	REGISTER_CVAR2("g_distanceForceNoIk", &m_distanceForceNoIk, 0.0f, VF_NULL, "Distance at which to disable ground alignment IK");
	REGISTER_CVAR2("g_distanceForceNoLegRaycasts", &m_distanceForceNoLegRaycasts, 5.0f, VF_NULL, "Distance at which to disable ground alignment IKs raycasts");
	REGISTER_CVAR2("g_spectatorCollisions", &m_spectatorCollisions, 1, VF_CHEAT, "Collide against the geometry in spectator mode");

	REGISTER_CVAR2("g_groundAlignAll", &m_groundAlignAll, 1, VF_NULL, "Enable ground alignment for every character that supports it.");

	REGISTER_CVAR2("ag_turnSpeedParamScale", &m_turnSpeedParamScale, 0.2f, VF_NULL, "Scale the turn speed animation param (used for leaning)");
	REGISTER_CVAR2("ag_turnSpeedSmoothing", &m_enableTurnSpeedSmoothing, 1, VF_NULL, "Enables/Disables turn speed smoothing.");
	REGISTER_CVAR2("ag_turnAngleSmoothing", &m_enableTurnAngleSmoothing, 1, VF_NULL, "Enables/Disables turn angle smoothing.");
	REGISTER_CVAR2("ag_travelSpeedSmoothing", &m_enableTravelSpeedSmoothing, 1, VF_NULL, "Enables/Disables travel speed smoothing.");

	PoseAligner::CVars::GetInstance();
}

CAnimationGraphCVars::~CAnimationGraphCVars()
{
	assert(s_pThis != 0);
	s_pThis = 0;

	IConsole* pConsole = gEnv->pConsole;

	pConsole->UnregisterVariable("ag_debugExactPos", true);
	pConsole->UnregisterVariable("ac_predictionSmoothingPos", true);
	pConsole->UnregisterVariable("ac_predictionSmoothingOri", true);
	pConsole->UnregisterVariable("ac_predictionProbabilityPos", true);
	pConsole->UnregisterVariable("ac_predictionProbabilityOri", true);

	pConsole->UnregisterVariable("ac_triggercorrectiontimescale", true);
	pConsole->UnregisterVariable("ac_targetcorrectiontimescale", true);

	pConsole->UnregisterVariable("ac_new", true);
	pConsole->UnregisterVariable("ac_movementControlMethodHor", true);
	pConsole->UnregisterVariable("ac_movementControlMethodVer", true);
	pConsole->UnregisterVariable("ac_movementControlMethodFilter", true);
	pConsole->UnregisterVariable("ac_templateMCMs", true);

	pConsole->UnregisterVariable("ac_entityAnimClamp", true);
	pConsole->UnregisterVariable("ac_animErrorClamp", true);
	pConsole->UnregisterVariable("ac_animErrorMaxDistance", true);
	pConsole->UnregisterVariable("ac_animErrorMaxAngle", true);
	pConsole->UnregisterVariable("ac_debugAnimError", true);

	pConsole->UnregisterVariable("ac_debugLocations", true);
	pConsole->UnregisterVariable("ac_debugMotionParams", true);

	pConsole->UnregisterVariable("ac_frametime", true);

	pConsole->UnregisterVariable("ac_debugText", true);
	pConsole->UnregisterVariable("ac_debugXXXValues", true);
	pConsole->UnregisterVariable("ac_debugEntityParams", true);
	pConsole->UnregisterVariable("ac_DebugFilter", true);

	pConsole->UnregisterVariable("ac_debugColliderMode", true);
	pConsole->UnregisterVariable("ac_debugMovementControlMethods", true);

	pConsole->UnregisterVariable("ac_disableSlidingContactEvents", true);
	pConsole->UnregisterVariable("ac_useMovementPrediction", true);
	pConsole->UnregisterVariable("ac_useQueuedRotation", true);

	pConsole->UnregisterVariable("ac_debugCarryCorrection", true);

	pConsole->UnregisterVariable("g_distanceForceNoIk", true);
	pConsole->UnregisterVariable("g_spectatorCollisions", true);

	pConsole->UnregisterVariable("ag_turnSpeedParamScale", true);

	pConsole->UnregisterVariable("ac_forceSimpleMovement", true);
	pConsole->UnregisterVariable("ac_debugAnimEffects", true);
	pConsole->UnregisterVariable("ac_ColliderModePlayer", true);
	pConsole->UnregisterVariable("ac_ColliderModeAI", true);
	pConsole->UnregisterVariable("ac_enableExtraSolidCollider", true);
	pConsole->UnregisterVariable("ac_debugAnimTarget", true);
	pConsole->UnregisterVariable("ac_debugLocationsGraphs", true);
	pConsole->UnregisterVariable("g_landingBobTimeFactor", true);
	pConsole->UnregisterVariable("g_landingBobLandTimeFactor", true);
	pConsole->UnregisterVariable("g_distanceForceNoLegRaycasts", true);
	pConsole->UnregisterVariable("g_groundAlignAll", true);
	pConsole->UnregisterVariable("ag_turnSpeedSmoothing", true);
	pConsole->UnregisterVariable("ag_turnAngleSmoothing", true);
	pConsole->UnregisterVariable("ag_travelSpeedSmoothing", true);
}
