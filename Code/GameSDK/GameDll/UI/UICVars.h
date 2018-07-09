// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __HUDCVARS_H__
#define __HUDCVARS_H__

//////////////////////////////////////////////////////////////////////////
	
class CUICVars
{

public:

	CUICVars();
	~CUICVars();

	void RegisterConsoleCommandsAndVars( void );
	void UnregisterConsoleCommandsAndVars( void );

	void GetMemoryUsage(ICrySizer *pSizer) const
	{	
		pSizer->AddObject(this, sizeof(*this));
	}

	int					hud_hide;

	int					hud_detach;
	float				hud_bobHud;
	int					hud_debug3dpos;

	int					hud_cameraOverride;
	float				hud_cameraDistance;
	float				hud_cameraOffsetZ;

	float				hud_overscanBorder_depthScale;

	float				hud_cgf_positionScaleFOV;
	float				hud_cgf_positionRightScale;

	int					hud_tagging_enabled;
	float				hud_tagging_duration_assaultDefenders;

	float				hud_warningDisplayTimeSP;
	float				hud_warningDisplayTimeMP;

	float				hud_inputprompts_dropPromptTime;

	float				hud_Crosshair_shotgun_spreadMultiplier;
	float				hud_tagging_duration;
	float				hud_double_taptime;
	float				hud_tagnames_EnemyTimeUntilLockObtained;
	float				hud_InterestPointsAtActorsHeads;

	float				hud_Crosshair_ironsight_fadeInDelay;
	float				hud_Crosshair_ironsight_fadeInTime;
	float				hud_Crosshair_ironsight_fadeOutTime;

	float				hud_Crosshair_laser_fadeInTime;
	float				hud_Crosshair_laser_fadeOutTime;

	float				hud_stereo_icon_depth_multiplier;
	float				hud_stereo_minDist;

	ICVar*      hud_colour_enemy;
	ICVar*      hud_colour_friend;
	ICVar*      hud_colour_squaddie;
	ICVar*      hud_colour_localclient;
};

//////////////////////////////////////////////////////////////////////////

#endif


