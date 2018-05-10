// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _GAME_CONSTANT_CVARS_H_
#define _GAME_CONSTANT_CVARS_H_

#if defined(_RELEASE) || defined(PERFORMANCE_BUILD)
	#define DeclareGameConstFloatCVar(name)
	#define DefineGameConstFloatCVar(name, flags, help)
	#define DeclareGameConstIntCVar(name)
	#define DefineGameConstIntCVar(name, flags, help)
	#define GetGameConstCVar(name)                      GameConstCVar_ ## name ## _Default
#else
	#define DeclareGameConstFloatCVar(name)             float name
	#define DefineGameConstFloatCVar(name, flags, help) ConsoleRegistrationHelper::Register(( # name), &name, GameConstCVar_ ## name ## _Default, flags | CONST_CVAR_FLAGS, help, 0, false)
	#define DeclareGameConstIntCVar(name)               int name
	#define DefineGameConstIntCVar(name, flags, help)   ConsoleRegistrationHelper::Register(( # name), &name, GameConstCVar_ ## name ## _Default, flags | CONST_CVAR_FLAGS, help)
	#define GetGameConstCVar(name)                      SGameReleaseConstantCVars::Get().name
#endif

// Button mashing sequence (SystemX)
#define GameConstCVar_g_SystemX_buttonMashing_initial_Default                (0.3f)
#define GameConstCVar_g_SystemX_buttonMashing_attack_Default                 (0.045f)
#define GameConstCVar_g_SystemX_buttonMashing_decayStart_Default             (0.12f)
#define GameConstCVar_g_SystemX_buttonMashing_decayMax_Default               (1.0f)
#define GameConstCVar_g_SystemX_buttonMashing_decayTimeOutToIncrease_Default (9.0f)
#define GameConstCVar_g_SystemX_buttonMashing_decayRampUpTime_Default        (6.0f)

// System X
#define GameConstCVar_g_SystemXDebug_State_Default                   (0)
#define GameConstCVar_g_SystemXDebug_HealthStatus_Default            (0)
#define GameConstCVar_g_SystemXDebug_Movement_Default                (0)
#define GameConstCVar_g_SystemXDebug_MeleeAttacks_Default            (0)
#define GameConstCVar_g_SystemX_Stomp_Radius_Default                 (9.0f)
#define GameConstCVar_g_SystemX_Sweep_Radius_Default                 (7.0f)
#define GameConstCVar_g_SystemX_Sweep_ZOffset_Default                (1.5f)
#define GameConstCVar_g_SystemX_Sweep_SideSelectionThreshold_Default (40.5f)

// Frontend
#define GameConstCVar_g_loadingHintRefreshTimeSP_Default   (6.0f)
#define GameConstCVar_g_loadingHintRefreshTimeMP_Default   (6.0f)
#define GameConstCVar_g_storyinfo_openitem_enabled_Default (1)
#define GameConstCVar_g_storyinfo_openitem_time_Default    (3.0f)

//HUD tweaking cvars
#define GameConstCVar_hud_InterestPoints_ScanDistance_Hazards_Default    (20.0f)
#define GameConstCVar_hud_InterestPoints_ScanDistance_Vehicles_Default   (150.0f)
#define GameConstCVar_hud_InterestPoints_ScanDistance_Units_Default      (150.0f)
#define GameConstCVar_hud_InterestPoints_ScanDistance_Items_Default      (35.0f)
#define GameConstCVar_hud_InterestPoints_ScanDistance_Ammo_Default       (50.0f)
#define GameConstCVar_hud_InterestPoints_ScanDistance_Explosives_Default (50.0f)
#define GameConstCVar_hud_InterestPoints_ScanDistance_Story_Default      (50.0f)

#define GameConstCVar_hud_ondemandloading_2d_Default                     (0)
#define GameConstCVar_hud_ondemandloading_3d_Default                     (0)

//Debug cheats
#define GameConstCVar_g_infiniteAmmo_Default          (0)
#define GameConstCVar_g_infiniteSprintStamina_Default (0)
#define GameConstCVar_g_debugTutorialData_Default     (0)

// Flash door panel
#define GameConstCVar_g_flashdoorpanel_distancecheckinterval_Default (0.5f)

// Achievements
#define GameConstCVar_g_achievements_aiDetectionDelay_Default (2.5f)

// STAP and FP aiming
#define GameConstCVar_g_stapEnable_Default                (1)
#define GameConstCVar_g_stapLayer_Default                 (5)
#define GameConstCVar_g_pwaLayer_Default                  (6)
#define GameConstCVar_g_translationPinningEnable_Default  (1)

#define GameConstCVar_STAP_MF_All_Default                 (1.0f)
#define GameConstCVar_STAP_MF_Scope_Default               (1.0f)
#define GameConstCVar_STAP_MF_ScopeVertical_Default       (0.0f)
#define GameConstCVar_STAP_MF_HeavyWeapon_Default         (1.5f)
#define GameConstCVar_STAP_MF_Up_Default                  (1.0f)
#define GameConstCVar_STAP_MF_Down_Default                (1.0f)
#define GameConstCVar_STAP_MF_Left_Default                (1.0f)
#define GameConstCVar_STAP_MF_Right_Default               (1.0f)
#define GameConstCVar_STAP_MF_Front_Default               (1.0f)
#define GameConstCVar_STAP_MF_Back_Default                (1.0f)
#define GameConstCVar_STAP_MF_StrafeLeft_Default          (1.0f)
#define GameConstCVar_STAP_MF_StrafeRight_Default         (1.0f)
#define GameConstCVar_STAP_MF_VerticalMotion_Default      (0.4f)
#define GameConstCVar_STAP_MF_VelFactorVertical_Default   (0.4f)
#define GameConstCVar_STAP_MF_VelFactorHorizontal_Default (0.3f)

// Tracers
#define GameConstCVar_g_tracers_slowDownAtCameraDistance_Default (20.0f)
#define GameConstCVar_g_tracers_minScale_Default                 (0.25f)
#define GameConstCVar_g_tracers_minScaleAtDistance_Default       (15.0f)
#define GameConstCVar_g_tracers_maxScale_Default                 (1.0f)
#define GameConstCVar_g_tracers_maxScaleAtDistance_Default       (80.0f)

struct SGameReleaseConstantCVars
{
	SGameReleaseConstantCVars();
	~SGameReleaseConstantCVars();

	void Init(IConsole* pConsole);

	// Button mashing sequence (SystemX)
	DeclareGameConstFloatCVar(g_SystemX_buttonMashing_initial);
	DeclareGameConstFloatCVar(g_SystemX_buttonMashing_attack);
	DeclareGameConstFloatCVar(g_SystemX_buttonMashing_decayStart);
	DeclareGameConstFloatCVar(g_SystemX_buttonMashing_decayMax);
	DeclareGameConstFloatCVar(g_SystemX_buttonMashing_decayTimeOutToIncrease);
	DeclareGameConstFloatCVar(g_SystemX_buttonMashing_decayRampUpTime);

	// SystemX
	DeclareGameConstIntCVar(g_SystemXDebug_State);
	DeclareGameConstIntCVar(g_SystemXDebug_HealthStatus);
	DeclareGameConstIntCVar(g_SystemXDebug_Movement);
	DeclareGameConstIntCVar(g_SystemXDebug_MeleeAttacks);
	DeclareGameConstFloatCVar(g_SystemX_Stomp_Radius);
	DeclareGameConstFloatCVar(g_SystemX_Sweep_Radius);
	DeclareGameConstFloatCVar(g_SystemX_Sweep_ZOffset);
	DeclareGameConstFloatCVar(g_SystemX_Sweep_SideSelectionThreshold);

	// Frontend
	DeclareGameConstFloatCVar(g_loadingHintRefreshTimeSP);
	DeclareGameConstFloatCVar(g_loadingHintRefreshTimeMP);
	DeclareGameConstIntCVar(g_storyinfo_openitem_enabled);
	DeclareGameConstFloatCVar(g_storyinfo_openitem_time);

	//HUD tweaking cvars
	DeclareGameConstFloatCVar(hud_InterestPoints_ScanDistance_Hazards);
	DeclareGameConstFloatCVar(hud_InterestPoints_ScanDistance_Vehicles);
	DeclareGameConstFloatCVar(hud_InterestPoints_ScanDistance_Units);
	DeclareGameConstFloatCVar(hud_InterestPoints_ScanDistance_Items);
	DeclareGameConstFloatCVar(hud_InterestPoints_ScanDistance_Ammo);
	DeclareGameConstFloatCVar(hud_InterestPoints_ScanDistance_Explosives);
	DeclareGameConstFloatCVar(hud_InterestPoints_ScanDistance_Story);

	DeclareGameConstIntCVar(hud_ondemandloading_2d);
	DeclareGameConstIntCVar(hud_ondemandloading_3d);

	//Debug cheats
	DeclareGameConstIntCVar(g_infiniteSprintStamina);
	DeclareGameConstIntCVar(g_debugTutorialData);

	// Flash door panel
	DeclareGameConstFloatCVar(g_flashdoorpanel_distancecheckinterval);

	// Achievements
	DeclareGameConstFloatCVar(g_achievements_aiDetectionDelay);

	// STAP
	DeclareGameConstIntCVar(g_stapEnable);
	DeclareGameConstIntCVar(g_stapLayer);
	DeclareGameConstIntCVar(g_pwaLayer);
	DeclareGameConstIntCVar(g_translationPinningEnable);

	DeclareGameConstFloatCVar(STAP_MF_All);
	DeclareGameConstFloatCVar(STAP_MF_Scope);
	DeclareGameConstFloatCVar(STAP_MF_ScopeVertical);
	DeclareGameConstFloatCVar(STAP_MF_HeavyWeapon);
	DeclareGameConstFloatCVar(STAP_MF_Up);
	DeclareGameConstFloatCVar(STAP_MF_Down);
	DeclareGameConstFloatCVar(STAP_MF_Left);
	DeclareGameConstFloatCVar(STAP_MF_Right);
	DeclareGameConstFloatCVar(STAP_MF_Front);
	DeclareGameConstFloatCVar(STAP_MF_Back);
	DeclareGameConstFloatCVar(STAP_MF_StrafeLeft);
	DeclareGameConstFloatCVar(STAP_MF_StrafeRight);
	DeclareGameConstFloatCVar(STAP_MF_VerticalMotion);
	DeclareGameConstFloatCVar(STAP_MF_VelFactorVertical);
	DeclareGameConstFloatCVar(STAP_MF_VelFactorHorizontal);

	// Tracers
	DeclareGameConstFloatCVar(g_tracers_slowDownAtCameraDistance);
	DeclareGameConstFloatCVar(g_tracers_minScale);
	DeclareGameConstFloatCVar(g_tracers_minScaleAtDistance);
	DeclareGameConstFloatCVar(g_tracers_maxScale);
	DeclareGameConstFloatCVar(g_tracers_maxScaleAtDistance);

	static const SGameReleaseConstantCVars& Get() { assert(m_pThis != NULL); return *m_pThis; }

private:
	static SGameReleaseConstantCVars* m_pThis;
};

#endif //_GAME_CONSTANT_CVARS_H_
