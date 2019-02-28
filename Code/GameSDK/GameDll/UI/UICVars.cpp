// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UICVars.h"

CUICVars::CUICVars()
	: hud_hide(0)
	, hud_detach(0)
	, hud_bobHud(2.5f)
	, hud_debug3dpos(0)
	, hud_cameraOverride(0)
	, hud_cameraDistance(1.5f)
	, hud_cameraOffsetZ(0)
	, hud_overscanBorder_depthScale(3.0f)
	, hud_cgf_positionScaleFOV(0.0f)
	, hud_cgf_positionRightScale(0.0f)
	, hud_tagging_enabled(1)
	, hud_tagging_duration_assaultDefenders(1.0f)
	, hud_warningDisplayTimeSP(1.0f)
	, hud_warningDisplayTimeMP(1.0f)
	, hud_inputprompts_dropPromptTime(1.0f)
	, hud_Crosshair_shotgun_spreadMultiplier(1.0f)
	, hud_tagging_duration(1.0f)
	, hud_double_taptime(0.25f)
	, hud_tagnames_EnemyTimeUntilLockObtained(1.0f)
	, hud_InterestPointsAtActorsHeads(1.0f)
	, hud_Crosshair_ironsight_fadeInDelay(1.0f)
	, hud_Crosshair_ironsight_fadeInTime(1.0f)
	, hud_Crosshair_ironsight_fadeOutTime(1.0f)
	, hud_Crosshair_laser_fadeInTime(1.0f)
	, hud_Crosshair_laser_fadeOutTime(1.0f)
	, hud_stereo_icon_depth_multiplier(1.0f)
	, hud_stereo_minDist(1.0f)
	, hud_colour_enemy(nullptr)
	, hud_colour_friend(nullptr)
	, hud_colour_squaddie(nullptr)
	, hud_colour_localclient(nullptr)
{}

CUICVars::~CUICVars()
{

}

void CUICVars::RegisterConsoleCommandsAndVars( void )
{
	REGISTER_CVAR(hud_hide, 0, 0, "1:hides the hud, 0:unhides it");

	REGISTER_CVAR(hud_detach, 0, 0, "Detaches the hud and leaves it where you were");
	REGISTER_CVAR(hud_bobHud, 2.5f, 0, "Hud bobbing distance");
	REGISTER_CVAR(hud_debug3dpos, 0, 0, "Draws a red sphere where the root of the hud-prefab is");

	REGISTER_CVAR(hud_cameraOverride, 0, 0, "1=override hud position with values from hud_cameraDistance and hud_cameraOffsetZ");
	REGISTER_CVAR(hud_cameraDistance, 1.5f, 0, "used when hud_cameraOverride is enabled - the higher this value the further the HUD is pushed away");
	REGISTER_CVAR(hud_cameraOffsetZ, 0.0f, 0, "used when hud_cameraOverride is enabled - the higher this value, the higher the hud is placed in the world (0 = in front of camera)");

	REGISTER_CVAR(hud_overscanBorder_depthScale, 3.0f, 0, "If you have overscan enabled (r_OverscanBorderScaleX/Y) then this multiplier is in effect, the higher this value the smaller the HUD");

	REGISTER_CVAR(hud_cgf_positionScaleFOV, 0.0f, 0, "Scales the FOV only for the HUD3D cgfs");
	REGISTER_CVAR(hud_cgf_positionRightScale, 0.0f, 0, "Changing this value will scale the position over the right vector (move the entire hud to the right)");

	REGISTER_CVAR(hud_tagging_enabled, 1, 0, "In multiplayer, tag killed entities on the radar");
	REGISTER_CVAR(hud_tagging_duration_assaultDefenders, 1.0f, 0, "if tagging is enabled, this is the duration the tag is shown on the radar for the defenders");

	REGISTER_CVAR(hud_warningDisplayTimeSP, 1.0f, 0, "Time a warning is displayed (e.g empty grenades) in singleplayer");
	REGISTER_CVAR(hud_warningDisplayTimeMP, 1.0f, 0, "Time a warning is displayed (e.g empty grenades) in multiplayer");

	REGISTER_CVAR(hud_inputprompts_dropPromptTime, 1.0f, 0, "Time spend displaying the drop message in CTF/Extraction");

	REGISTER_CVAR(hud_Crosshair_shotgun_spreadMultiplier, 1.0f, 0, "Change this to change the spread for the shotgun visually when you shoot (ActorSensor:OnShoot requests it)");
	REGISTER_CVAR(hud_tagging_duration, 1.0f, 0, "if tagging is enabled, this is the duration the tag is shown on the radar");
	REGISTER_CVAR(hud_double_taptime, 0.25f, 0, "Time between switch weapon button to register a double tap");
	REGISTER_CVAR(hud_tagnames_EnemyTimeUntilLockObtained, 1.0f, 0, "Time until the predator locks on given by the enemy team (player can see this as a countdown)");
	REGISTER_CVAR(hud_InterestPointsAtActorsHeads, 1.0f, 0, "Uses actors BONE_HEAD or BONE_SPINE rather than the center of the bounding box when tagged");

	REGISTER_CVAR(hud_Crosshair_ironsight_fadeInDelay, 1.0f, 0, "This is send with the eHUDEvent_FadeCrosshair event so the UI can respond to different fade time settings when the weapon zooms in");
	REGISTER_CVAR(hud_Crosshair_ironsight_fadeInTime, 1.0f, 0, "This is send with the eHUDEvent_FadeCrosshair event so the UI can respond to different fade time settings when the weapon zooms in");
	REGISTER_CVAR(hud_Crosshair_ironsight_fadeOutTime, 1.0f, 0, "This is send with the eHUDEvent_FadeCrosshair event so the UI can respond to different fade time settings when the weapon zooms in");

	REGISTER_CVAR(hud_Crosshair_laser_fadeInTime, 1.0f, 0, "When the laser is turned on, this is the time it will take for the crosshair to come back");
	REGISTER_CVAR(hud_Crosshair_laser_fadeOutTime, 1.0f, 0, "When the laser is turned off, this is the time it will take for the crosshair to go away");

	REGISTER_CVAR(hud_stereo_icon_depth_multiplier, 1.0f, 0, "The distance of an icon on the hud is multiplied by this value");
	REGISTER_CVAR(hud_stereo_minDist, 1.0f, 0, "This is the minimum distance an icon will have (in case the icon_depth_multiplier is not strong enough)");

	// rank text colour in tganmes
	hud_colour_enemy = REGISTER_STRING("hud_colour_enemy","AC0000",0,"An enemy specific hex code colour string. Used in tagnames, battlelog etc.");
	hud_colour_friend = REGISTER_STRING("hud_colour_friend","9AD5B7",0,"A friend specific hex code colour string. Used in tagnames, battlelog etc..");
	hud_colour_squaddie = REGISTER_STRING("hud_colour_squaddie","00CCFF",0,"A friend on local player's squad specific hex code colour string. Used in tagnames, battlelog etc.");
	hud_colour_localclient = REGISTER_STRING("hud_colour_localclient","FFC800",0,"A local player specific hex code colour string. Used in tagnames, battlelog etc..");

}

void CUICVars::UnregisterConsoleCommandsAndVars( void )
{
	gEnv->pConsole->UnregisterVariable("hud_hide", true);

	gEnv->pConsole->UnregisterVariable("hud_detach", true);
	gEnv->pConsole->UnregisterVariable("hud_bobHud", true);
	gEnv->pConsole->UnregisterVariable("hud_debug3dpos", true);

	gEnv->pConsole->UnregisterVariable("hud_cameraOverride", true);
	gEnv->pConsole->UnregisterVariable("hud_cameraDistance", true);
	gEnv->pConsole->UnregisterVariable("hud_cameraOffsetZ", true);

	gEnv->pConsole->UnregisterVariable("hud_overscanBorder_depthScale", true);

	gEnv->pConsole->UnregisterVariable("hud_cgf_positionScaleFOV", true);
	gEnv->pConsole->UnregisterVariable("hud_cgf_positionRightScale", true);

	gEnv->pConsole->UnregisterVariable("hud_tagging_enabled", true);
	gEnv->pConsole->UnregisterVariable("hud_tagging_duration_assaultDefenders", true);

	gEnv->pConsole->UnregisterVariable("hud_warningDisplayTimeSP", true);
	gEnv->pConsole->UnregisterVariable("hud_warningDisplayTimeMP", true);

	gEnv->pConsole->UnregisterVariable("hud_inputprompts_dropPromptTime", true);

	gEnv->pConsole->UnregisterVariable("hud_Crosshair_shotgun_spreadMultiplier", true);
	gEnv->pConsole->UnregisterVariable("hud_tagging_duration", true);
	gEnv->pConsole->UnregisterVariable("hud_double_taptime", true);
	gEnv->pConsole->UnregisterVariable("hud_tagnames_EnemyTimeUntilLockObtained", true);
	gEnv->pConsole->UnregisterVariable("hud_InterestPointsAtActorsHeads", true);

	gEnv->pConsole->UnregisterVariable("hud_Crosshair_ironsight_fadeInDelay", true);
	gEnv->pConsole->UnregisterVariable("hud_Crosshair_ironsight_fadeInTime", true);
	gEnv->pConsole->UnregisterVariable("hud_Crosshair_ironsight_fadeOutTime", true);

	gEnv->pConsole->UnregisterVariable("hud_Crosshair_laser_fadeInTime", true);
	gEnv->pConsole->UnregisterVariable("hud_Crosshair_laser_fadeOutTime", true);

	gEnv->pConsole->UnregisterVariable("hud_stereo_icon_depth_multiplier", true);
	gEnv->pConsole->UnregisterVariable("hud_stereo_minDist", true);

	gEnv->pConsole->UnregisterVariable("hud_colour_enemy", true);
	gEnv->pConsole->UnregisterVariable("hud_colour_friend", true);
	gEnv->pConsole->UnregisterVariable("hud_colour_squaddie", true);
	gEnv->pConsole->UnregisterVariable("hud_colour_localclient", true);
}
