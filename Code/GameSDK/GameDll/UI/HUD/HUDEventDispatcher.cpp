// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HUDEventDispatcher.h"

//////////////////////////////////////////////////////////////////////////
namespace
{
	typedef std::vector<IHUDEventListener*>         THUDEventListeners;
	typedef std::vector<THUDEventListeners>         TEventVector;

	static bool                                     s_safe = true;
	static bool                                     s_stlSafe = true;
	static TEventVector                             s_eventVec;
}

/*static*/ SHUDEvent::THUDEventDataStack SHUDEvent::s_dataStack;
/*static*/ bool SHUDEvent::s_usedDataIds[MAX_CONCURRENT_HUDEVENTS];

//////////////////////////////////////////////////////////////////////////

typedef TKeyValuePair<string, EHUDEventType> THUDEventLookup;
static THUDEventLookup s_eventLookUpTable[] = {
	// HUD State & resolutions
	{"OnHUDUnload",                  eHUDEvent_OnHUDUnload},
	{"OnHUDLoadDone",                eHUDEvent_OnHUDLoadDone},
	{"OnHUDReload",                  eHUDEvent_OnHUDReload},
	{"OnAssetOnDemandUnload",				 eHUDEvent_OnAssetOnDemandUnload},
	{"OnAssetOnDemandLoad",					 eHUDEvent_OnAssetOnDemandLoad},
	{"OnUpdate",                     eHUDEvent_OnUpdate},
	{"OnPostHUDDraw",                eHUDEvent_OnPostHUDDraw},
	{"OnResolutionChange",           eHUDEvent_OnResolutionChange},
	{"OnOverscanBordersChange",			 eHUDEvent_OnOverscanBordersChange},
	{"HUDElementVisibility",         eHUDEvent_HUDElementVisibility},
	{"HUDBoot",                      eHUDEvent_HUDBoot},
	{"OnChangeHUDStyle",             eHUDEvent_OnChangeHUDStyle},
	{"OnHUDStyleChanged",            eHUDEvent_OnHUDStyleChanged},
	{"OnPreHUDStyleChanged",         eHUDEvent_OnPreHUDStyleChanged},
	{"OnHUDStyleChangeWeaponArea",   eHUDEvent_OnHUDStyleChangeWeaponArea},
	{"OnSubtitlesDraw",              eHUDEvent_OnSubtitlesDraw},
	{"OnSubtitlesChanged",           eHUDEvent_OnSubtitlesChanged},
	{"OnShowSkippablePrompt",        eHUDEvent_OnShowSkippablePrompt},
	{"ForceDrawSubtitles",					 eHUDEvent_ForceDrawSubtitles},
	{"OnCustomTextSet",              eHUDEvent_OnCustomTextSet},
	{"OnCustomTextDraw",						 eHUDEvent_OnCustomTextDraw},
	{"OnInteractiveHintDraw",        eHUDEvent_OnInteractiveHintDraw},
	{"HUDSnapToCamera",              eHUDEvent_HUDSnapToCamera},
	{"OnStartActivateNewState",      eHUDEvent_OnStartActivateNewState },
	{"OnEndActivateNewState",        eHUDEvent_OnEndActivateNewState },
	{"OnControlSchemeSwitch",        eHUDEvent_OnControlSchemeSwitch },
	// Local Player
	{"OnInitLocalPlayer",            eHUDEvent_OnInitLocalPlayer},
	{"OnHealthChanged",              eHUDEvent_OnHealthChanged},
	{"OnLocalPlayerKilled",          eHUDEvent_OnLocalPlayerKilled},
	{"OnLocalPlayerDeath",           eHUDEvent_OnLocalPlayerDeath},
	{"OnLocalPlayerCanNowRespawn",	 eHUDEvent_OnLocalPlayerCanNowRespawn},
	{"OnViewDistanceChanged",        eHUDEvent_OnViewDistanceChanged},
	{"OnSpawn",                      eHUDEvent_OnSpawn},
	{"PlayerSwitchTeams",            eHUDEvent_PlayerSwitchTeams},
	{"ShowReviveCycle",              eHUDEvent_ShowReviveCycle},
	{"OnShowHitIndicator",           eHUDEvent_OnShowHitIndicator},
	{"OnShowHitIndicatorPlayerUpdated",eHUDEvent_OnShowHitIndicatorPlayerUpdated},
	{"OnShowHitIndicatorBothUpdated",eHUDEvent_OnShowHitIndicatorBothUpdated},
	{"OnViewUpdate",                 eHUDEvent_OnViewUpdate},
	{"SetupPlayerTeamSpecifics",     eHUDEvent_SetupPlayerTeamSpecifics},
	{"OnScannedExternally",					 eHUDEvent_OnScannedExternally},
	{"OnHitEMP",										 eHUDEvent_OnHitEMP},
	{"ShowEnemyHealthbars",					 eHUDEvent_ShowEnemyHealthbars},
	{"OnUseLedge",									 eHUDEvent_OnUseLedge},
	{"ResetHUDStyle",								 eHUDEvent_ResetHUDStyle},
		// Remote player
	{"OnEnterGame_RemotePlayer",     eHUDEvent_OnEnterGame_RemotePlayer},
	{"OnLeaveGame_RemotePlayer",     eHUDEvent_OnLeaveGame_RemotePlayer},
	{"OnRemotePlayerDeath",          eHUDEvent_OnRemotePlayerDeath},
	{"OnPlayerRevive",							 eHUDEvent_OnPlayerRevive}, // Only called in Multiplayer!
	{"OnGenericBattleLogMessage",		 eHUDEvent_OnGenericBattleLogMessage},
	{"OnLobbyRemove_RemotePlayer",     eHUDEvent_OnLobbyRemove_RemotePlayer},
	//{"OnOnSpawn_RemotePlayer",       eHUDEvent_OnSpawn_RemotePlayer},
	//{"OnLeave_RemotePlayer",         eHUDEvent_OnLeave_RemotePlayer},
	{"OnRenamePlayer",               eHUDEvent_RenamePlayer},
	// Score and progression
	{"OnNewScore",                   eHUDEvent_OnNewScore},
	{"OnNewSkillKill",               eHUDEvent_OnNewSkillKill},
	{"OnAssessmentComplete",         eHUDEvent_OnAssessmentComplete},
	{"OnPlayerPromotion",            eHUDEvent_OnPlayerPromotion},
	// SuitMode & Menu
	{"OnSuitModeChanged",            eHUDEvent_OnSuitModeChanged},
	{"OnEnergyChanged",              eHUDEvent_OnEnergyChanged},
	{"OnSuitOverChargeChanged",      eHUDEvent_OnSuitOverChargeChanged},
	{"OnSuitStateChanged",           eHUDEvent_OnSuitStateChanged},
	{"OnSuitMenuOpened",             eHUDEvent_OnSuitMenuOpened},
	{"OnSuitMenuClosed",             eHUDEvent_OnSuitMenuClosed},
	{"OnSuitPowerSpecial",           eHUDEvent_OnSuitPowerSpecial},
	{"OnSuitPowerActivated",         eHUDEvent_OnSuitPowerActivated},
	{"ShowVisionMenu",               eHUDEvent_OnShowVisionMenu},
	{"HideVisionMenu",               eHUDEvent_OnHideVisionMenu},
	// Weapons, Weapon Feedback & Crosshair
	{"OnStartReload",                eHUDEvent_OnStartReload},
	{"OnReloaded",                   eHUDEvent_OnReloaded},
	{"OnItemPickUp",                 eHUDEvent_OnItemPickUp},
	{"OnAmmoPickUp",                 eHUDEvent_OnAmmoPickUp},
	{"OnSetAmmoCount",               eHUDEvent_OnSetAmmoCount},
	{"OnSetInventoryAmmoCount",      eHUDEvent_OnSetInventoryAmmoCount},
	{"OnShoot",                      eHUDEvent_OnShoot},
	{"OnItemSelected",               eHUDEvent_OnItemSelected},
	{"OnPrepareItemSelected",        eHUDEvent_OnPrepareItemSelected},
	{"OnFireModeChanged",            eHUDEvent_OnFireModeChanged},
	{"OnWeaponFireModeChanged",			 eHUDEvent_OnWeaponFireModeChanged},
	{"OnHitTarget",                  eHUDEvent_OnHitTarget},
	{"OnHit",                        eHUDEvent_OnHit},
	{"OnZoom",                       eHUDEvent_OnZoom},
	{"OnExplosiveSpawned",           eHUDEvent_OnExplosiveSpawned},
	{"OnExplosiveDetonationDelayed", eHUDEvent_OnExplosiveDetonationDelayed},
	{"OnGrenadeExploded",            eHUDEvent_OnGrenadeExploded},
	{"OnGrenadeThrow",							 eHUDEvent_OnGrenadeThrow},
	{"FadeCrosshair",                eHUDEvent_FadeCrosshair},
	{"CantFire",                     eHUDEvent_CantFire},
	{"ShowExplosiveMenu",            eHUDEvent_ShowExplosiveMenu},
	{"HideExplosiveMenu",            eHUDEvent_HideExplosiveMenu},
	{"SelectExplosive",              eHUDEvent_SelectExplosive},
	{"ShowDPadMenu",                 eHUDEvent_ShowDpadMenu},
	{"OnCrosshairVisibilityChanged", eHUDEvent_OnCrosshairVisibilityChanged},
	{"OnCrosshairModeChanged",       eHUDEvent_OnCrosshairModeChanged},
	{"ShowNoAmmoWarning",						 eHUDEvent_ShowNoAmmoWarning},
	{"OnNoPickableAmmoForItem",			 eHUDEvent_OnNoPickableAmmoForItem},
	{"IsDoubleTapExplosiveSelect",	 eHUDEvent_IsDoubleTapExplosiveSelect},
	{"ForceCrosshairType",					 eHUDEvent_ForceCrosshairType},
	{"OnC4Spawned",									 eHUDEvent_OnC4Spawned},
	{"RemoveC4Icon",								 eHUDEvent_RemoveC4Icon},
	{"OnForceWeaponDisplay",				 eHUDEvent_OnForceWeaponDisplay},
	{"ForceInfiniteAmmoIcon",				 eHUDEvent_ForceInfiniteAmmoIcon},
	{"OnBowChargeValueChanged",			 eHUDEvent_OnBowChargeValueChanged},
	{"OnBowFired",									 eHUDEvent_OnBowFired},
	{"DisplayTrajectory",					   eHUDEvent_DisplayTrajectory},
	{"OnTrajectoryTimeChanged",			 eHUDEvent_OnTrajectoryTimeChanged},
	{"UpdateExplosivesAmmo",				 eHUDEvent_UpdateExplosivesAmmo},
	
	// Prompts (pickups and interactions)
	{"OnLookAtChanged",              eHUDEvent_OnLookAtChanged},
	{"OnInteractionRequest",         eHUDEvent_OnInteractionRequest},
	{"OnSetInteractionMsg"	,				 eHUDEvent_OnSetInteractionMsg},
	{"OnClearInteractionMsg"	,      eHUDEvent_OnClearInteractionMsg},
	{"OnSetInteractionMsg3D"	,			 eHUDEvent_OnSetInteractionMsg3D},
	{"OnClearInteractionMsg3D",	     eHUDEvent_OnClearInteractionMsg3D},
	{"OnSetWeaponOverlay",					 eHUDEvent_OnSetWeaponOverlay},
	{"OnUsableChanged",              eHUDEvent_OnUsableChanged},
	{"ShowingUsablePrompt",          eHUDEvent_ShowingUsablePrompt},
	{"HidingUsablePrompt",           eHUDEvent_HidingUsablePrompt},
	{"OnInteractionUseHoldTrack",    eHUDEvent_OnInteractionUseHoldTrack},
	{"OnInteractionUseHoldActivated",eHUDEvent_OnInteractionUseHoldActivated},
	{"AddOnScreenMessage",           eHUDEvent_AddOnScreenMessage},
	{"FlushMpMessages",              eHUDEvent_FlushMpMessages},
	{"OnGameStateNotifyMessage",     eHUDEvent_OnGameStateNotifyMessage},
	{"DisplayLateJoinMessage",           eHUDEvent_DisplayLateJoinMessage},
	{"OnTeamMessage",								 eHUDEvent_OnTeamMessage},
	{"OnSimpleBannerMessage",				 eHUDEvent_OnSimpleBannerMessage},
	{"OnPromotionMessage",           eHUDEvent_OnPromotionMessage},
	{"OnServerMessage",              eHUDEvent_OnServerMessage},
	{"OnNewMedalAward",              eHUDEvent_OnNewMedalAward},
	{"InfoSystemsEvent",             eHUDEvent_InfoSystemsEvent},
	{"OnSetCustomInputAction",       eHUDEvent_OnSetCustomInputAction},
	{"OnCustomInputAction",          eHUDEvent_OnCustomInputAction},
	// MP Gamemodes
	{"GameEnded",                    eHUDEvent_GameEnded},
	{"MakeMatchEndScoreboardVisible",eHUDEvent_MakeMatchEndScoreboardVisible},
	{"OnGamemodeChange",             eHUDEvent_OnGamemodeChange},
	{"OnInitGameRules",              eHUDEvent_OnInitGameRules},
	{"OnUpdateGameStartMessage",     eHUDEvent_OnUpdateGameStartMessage},
	{"OnWaitingForPlayers",          eHUDEvent_OnWaitingForPlayers},
	{"OnGameStart",                  eHUDEvent_OnGameStart},
	{"OnSetGameStateMessage",        eHUDEvent_OnSetGameStateMessage},
	{"OnRoundStart",                 eHUDEvent_OnRoundStart},
	{"OnPrematchFinished",           eHUDEvent_OnPrematchFinished},
	{"OnRoundEnd",                   eHUDEvent_OnRoundEnd},
	{"OnRoundAboutToStart",          eHUDEvent_OnRoundAboutToStart},
	{"OnSuddenDeath",                eHUDEvent_OnSuddenDeath},
	{"OnRoundMessage",               eHUDEvent_OnRoundMessage},
	{"OnUpdateGameResumeMessage",    eHUDEvent_OnUpdateGameResumeMessage},
	{"ForceRespawnTimer",            eHUDEvent_ForceRespawnTimer},
	// CTF Specific
	{"OnLocalClientStartCarryingFlag",          eHUDEvent_OnLocalClientStartCarryingFlag},
	// Power Struggle Specific
	{"OnPowerStruggle_NodeStateChange",						eHUDEvent_OnPowerStruggle_NodeStateChange},
	{"OnPowerStruggle_GiantSpearCharge",					eHUDEvent_OnPowerStruggle_GiantSpearCharge},
	{"OnPowerStruggle_ManageCaptureBar",					eHUDEvent_OnPowerStruggle_ManageCaptureBar},
	// SP Objectives
	{"OnObjectiveChanged",           eHUDEvent_OnObjectiveChanged},
	{"HighlightPrimaryObjective",    eHUDEvent_HighlightPrimaryObjective},
	{"OnObjectiveAnalysis",          eHUDEvent_OnObjectiveAnalysis},
	
	{"OnRadioReceived",              eHUDEvent_OnRadioReceived},
	{"OnDrawLine",                   eHUDEvent_OnDrawLine},
	{"OnRemoveLine",                 eHUDEvent_OnRemoveLine},
	{"OnRemoveAllLines",             eHUDEvent_OnRemoveAllLines},

	// MP Objectives
	{"OnNewObjective",               eHUDEvent_OnNewObjective},
	{"OnRemoveObjective",            eHUDEvent_OnRemoveObjective},
	{"OnNewGameRulesObjective",			 eHUDEvent_OnNewGameRulesObjective},
	{"OnSiteBeingCaptured",          eHUDEvent_OnSiteBeingCaptured},
	{"OnSiteCaptured",               eHUDEvent_OnSiteCaptured},
	{"OnSiteLost",                   eHUDEvent_OnSiteLost},
	{"OnSiteAboutToExplode",         eHUDEvent_OnSiteAboutToExplode },
	{"OnClientInOwnedSite",          eHUDEvent_OnClientInOwnedSite},
	{"OnClientInContestedSite",      eHUDEvent_OnClientInContestedSite},
	{"OnClientLeftSite",             eHUDEvent_OnClientLeftSite},
	{"OnCaptureObjectiveNumChanged", eHUDEvent_OnCaptureObjectiveNumChanged},
	{"OnCaptureObjectiveRefreshEnabledState", eHUDEvent_OnCaptureObjectiveRefreshEnabledState},
	{"OnNewCaptureObjectiveWave",    eHUDEvent_OnNewCaptureObjectiveWave},
	{"OnOverallCaptureProgressUpdate", eHUDEvent_OnOverallCaptureProgressUpdate},
	{"ObjectivesUpdateText",         eHUDEvent_ObjectivesUpdateText},
	{"ObjectiveUpdateClock",				 eHUDEvent_ObjectiveUpdateClock},
	{"OnUpdateObjectivesLeft",       eHUDEvent_OnUpdateObjectivesLeft},
	{"PredatorGracePeriodFinished",  eHUDEvent_PredatorGracePeriodFinished},
	{"PredatorAllMarinesDead",			 eHUDEvent_PredatorAllMarinesDead},
	{"SuicidePenalty",							 eHUDEvent_SuicidePenalty},
	{"SetAttackingTeam",						 eHUDEvent_SetAttackingTeam},
	{"OnBigMessage",								 eHUDEvent_OnBigMessage},
	{"OnBigWarningMessage",					 eHUDEvent_OnBigWarningMessage},
	{"OnBigWarningMessageUnlocalized",					eHUDEvent_OnBigWarningMessageUnlocalized},
	{"OnSetStaticTimeLimit",				 eHUDEvent_OnSetStaticTimeLimit},
	{"EnableNetworkDebug",					 eHUDEvent_EnableNetworkDebug},
	{"DisableNetworkDebug",					 eHUDEvent_DisableNetworkDebug},
	/// Kill cam replay & Spectator Cam
	{"OnKillCamStartPlay",           eHUDEvent_OnKillCamStartPlay},
	{"OnKillCamStopPlay",            eHUDEvent_OnKillCamStopPlay},
	{"OnKillCamPostStopPlay",        eHUDEvent_OnKillCamPostStopPlay},
	{"OnKillCamWeaponSwitch",        eHUDEvent_OnKillCamWeaponSwitch},
	{"OnKillCamSuitMode",            eHUDEvent_OnKillCamSuitMode},
	{"OnWinningKillCam",						 eHUDEvent_OnWinningKillCam},
	{"OnHighlightsReel",						 eHUDEvent_OnHighlightsReel},
	// PC Kick Voting
	{"OnVotingStarted",			         eHUDEvent_OnVotingStarted},
	{"OnVotingProgressUpdate",			 eHUDEvent_OnVotingProgressUpdate},
	{"OnVotingEnded",                eHUDEvent_OnVotingEnded},
	// Blind
	{"OnBlind",                      eHUDEvent_OnBlind},
	{"OnEndBlind",                   eHUDEvent_OnEndBlind},
	// Radar & Map
	{"AlignToRadarAsset",            eHUDEvent_AlignToRadarAsset},
	{"RadarRotation",                eHUDEvent_RadarRotation},
	{"ToggleMap",                    eHUDEvent_ToggleMap},
	{"TemporarilyTrackEntity",       eHUDEvent_TemporarilyTrackEntity},
	{"OnLocalActorTagged",					 eHUDEvent_OnLocalActorTagged},
	{"RescanActors",                 eHUDEvent_RescanActors},
	{"AddEntity",                    eHUDEvent_AddEntity},
	{"RemoveEntity",                 eHUDEvent_RemoveEntity},
	{"OnUpdateMinimap",              eHUDEvent_OnUpdateMinimap},
	{"LoadMinimap",                  eHUDEvent_LoadMinimap},
	{"OnChangeSquadState",           eHUDEvent_OnChangeSquadState},
	{"OnRadarJammingChanged",        eHUDEvent_OnRadarJammingChanged},
	{"OnRadarSweep",								 eHUDEvent_OnRadarSweep},
	// Battle area
	{"LeavingBattleArea",            eHUDEvent_LeavingBattleArea},
	{"ReturningToBattleArea",        eHUDEvent_ReturningToBattleArea},
	// Host migration
	{"ShowHostMigrationScreen",      eHUDEvent_ShowHostMigrationScreen},
	{"HideHostMigrationScreen",      eHUDEvent_HideHostMigrationScreen},
	{"HostMigrationOnNewPlayer",     eHUDEvent_HostMigrationOnNewPlayer},
	{"NetLimboStateChanged",         eHUDEvent_NetLimboStateChanged},
	
	{"OnAIAwarenessChanged",         eHUDEvent_OnAIAwarenessChanged},
	{"OnTeamRadarChanged",           eHUDEvent_OnTeamRadarChanged},
	{"OnMaxSuitRadarChanged",				 eHUDEvent_OnMaxSuitRadarChanged},
	{"SetRadarSweepCount",					 eHUDEvent_SetRadarSweepCount},
	{"OnMapDeploymentChanged",			 eHUDEvent_OnMapDeploymentChanged},
	// Tagnames
	{"OnIgnoreEntity",               eHUDEvent_OnIgnoreEntity},
	{"OnStopIgnoringEntity",         eHUDEvent_OnStopIgnoringEntity},
	{"OnStartTrackFakePlayerTagname",eHUDEvent_OnStartTrackFakePlayerTagname},
	{"OnEndTrackFakePlayerTagname",  eHUDEvent_OnEndTrackFakePlayerTagname},
	// Scanning/Tactical info
	{"OnScanningStart",              eHUDEvent_OnScanningStart},
	{"OnScanningComplete",           eHUDEvent_OnScanningComplete},
	{"OnEntityScanned",              eHUDEvent_OnEntityScanned},
	{"OnTaggingStart",	             eHUDEvent_OnTaggingStart},
	{"OnTaggingComplete",	           eHUDEvent_OnTaggingComplete},
	{"OnEntityTagged",	             eHUDEvent_OnEntityTagged},
	{"OnScanningStop",               eHUDEvent_OnScanningStop},
	{"OnControlCurrentTacticalScan", eHUDEvent_OnControlCurrentTacticalScan},
	{"OnCenterEntityChanged",        eHUDEvent_OnCenterEntityChanged},
	{"OnCenterEntityScannableChanged",        eHUDEvent_OnCenterEntityScannableChanged},
	{"OnVisorChanged",               eHUDEvent_OnVisorChanged},
	{"OnTacticalInfoChanged",        eHUDEvent_OnTacticalInfoChanged},
	{"OnWeaponInfoChanged",		       eHUDEvent_OnWeaponInfoChanged},
	{"OnChatMessage",                eHUDEvent_OnChatMessage},
	{"OnNewBattleLogMessage",        eHUDEvent_OnNewBattleLogMessage},
	{"OnNewGameStateMessage",        eHUDEvent_OnNewGameStateMessage},
	{"ToggleChatInput",              eHUDEvent_ToggleChatInput},
	{"OnScannedEnemyRemove",         eEHUDEvent_OnScannedEnemyRemove},
	// Menus & Scoreboards
	{"ShowScoreboard",               eHUDEvent_ShowScoreboard},
	{"HideScoreboard",               eHUDEvent_HideScoreboard},
	{"OpenedIngameMenu",             eHUDEvent_OpenedIngameMenu},
	{"ClosedIngameMenu",             eHUDEvent_ClosedIngameMenu},
	{"ShowWeaponCustomization",      eHUDEvent_ShowWeaponCustomization},
	{"HideWeaponCustomization",      eHUDEvent_HideWeaponCustomization},
	{"OnTutorialEvent",              eHUDEvent_OnTutorialEvent},
	// Save / Load
	{"OnGameSave",                   eHUDEvent_OnGameSave},
	{"OnGameLoad",                   eHUDEvent_OnGameLoad},
	{"OnFileIO",                     eHUDEvent_OnFileIO},
  // Nano Web
  {"ShowNanoWeb",                  eHUDEvent_ShowNanoWeb},
  {"HideNanoWeb",                  eHUDEvent_HideNanoWeb},
	// Navigation Path
	{"PosOnNavPath",                 eHUDEvent_PosOnNavPath},
	// Cinematic
  {"OnCinematicPlay",              eHUDEvent_OnCinematicPlay},
  {"OnCinematicStop",              eHUDEvent_OnCinematicStop},
	{"OnBeginCutScene",              eHUDEvent_OnBeginCutScene},
	{"OnEndCutScene",                eHUDEvent_OnEndCutScene},
	{"CinematicVTOLUpdate",          eHUDEvent_CinematicVTOLUpdate},
	{"CinematicTurretUpdate",        eHUDEvent_CinematicTurretUpdate},
	{"OnShowVideoOnHUD",             eHUDEvent_OnShowVideoOnHUD},
	{"TurretHUDActivated",           eHUDEvent_TurretHUDActivated},
	
	{"CinematicScanning",            eHUDEvent_CinematicScanning},
	
	{"ClearRadarData",               eHUDEvent_ClearRadarData},
	// MouseWheel
	{"ShowMouseWheel",							 eHUDEvent_ShowMouseWheel},
	{"HideMouseWheel",							 eHUDEvent_HideMouseWheel},
	// Nano Glass
	{"ShowNanoGlass",                eHUDEvent_ShowNanoGlass},
	// EnergyOverlay
	{"ActivateOverlay",							 eHUDEvent_ActivateOverlay},
	{"DeactivateOverlay",						 eHUDEvent_DeactivateOverlay},
	//vehicle
	{"PlayerLinkedToVehicle",				 eHUDEvent_PlayerLinkedToVehicle},
	{"PlayerUnlinkedFromVehicle",		 eHUDEvent_PlayerUnlinkedFromVehicle},
	{"OnVehiclePlayerEnter",				 eHUDEvent_OnVehiclePlayerEnter},
	{"OnVehiclePlayerLeave",		     eHUDEvent_OnVehiclePlayerLeave},
	{"OnVehiclePassengerEnter",			 eHUDEvent_OnVehiclePassengerEnter},
	{"OnVehiclePassengerLeave",		   eHUDEvent_OnVehiclePassengerLeave},
	// local player target
	{"LocalPlayerTargeted",				eHUDEvent_LocalPlayerTargeted}
};

//////////////////////////////////////////////////////////////////////////
/*static*/ void CHUDEventDispatcher::SetUpEventListener()
{
#if !defined(DEDICATED_SERVER)
    // Remove all previous elements
	TEventVector::iterator it = s_eventVec.begin();
	TEventVector::iterator end = s_eventVec.end();
	for (; it != end; ++it)
	{
		it->clear();
	}
	s_eventVec.clear();

	// Setup the vector
	s_eventVec.resize(eHUDEvent_LAST + 1, THUDEventListeners());
#endif
}

/*static*/ void CHUDEventDispatcher::CheckEventListenersClear()
{
#if ENABLE_HUD_EXTRA_DEBUG
	TEventVector::iterator vecIt = s_eventVec.begin();
	const TEventVector::const_iterator vecEnd = s_eventVec.end();
	string tmpstr;
	for (; vecIt != vecEnd; ++vecIt)
	{
		THUDEventListeners& listeners = (*vecIt);
		THUDEventListeners::iterator it = listeners.begin();
		const THUDEventListeners::const_iterator end = listeners.end();

		for(; it!=end; ++it)
		{
			IHUDEventListener* pListener = (*it);
			CRY_ASSERT_MESSAGE(!pListener, tmpstr.Format("HUD: Event listener '%p' is still listening for events when it shouldn't be!", pListener).c_str());
		}
	}
#endif
}

/*static*/ void CHUDEventDispatcher::FreeEventListeners()
{
#if !defined(DEDICATED_SERVER)
	TEventVector::iterator it = s_eventVec.begin();
	TEventVector::iterator end = s_eventVec.end();
	for (; it != end; ++it)
	{
		stl::free_container(*it);
	}
#endif
}

/*static*/ void CHUDEventDispatcher::AddHUDEventListener( IHUDEventListener* pListener, const char* in_eventName )
{
#if !defined(DEDICATED_SERVER)
	CRY_ASSERT_MESSAGE(s_stlSafe, "HUD: Can't call AddHUDEventListener() in an OnHUDEvent() callback!" );
	if(s_stlSafe==false)
	{
		// HUD Event Sending is now unsafe and should not be done!
		// Do not use event callbacks to un-listen for events.
		return;
	}

	const EHUDEventType eventType = GetEvent(in_eventName);

	if( eventType == eHUDEvent_None )
	{
		GameWarning( "HUD: Event '%s' not found/registered! (whilst adding HUD Event listener for it!)", in_eventName );
	}
	if (!s_eventVec.empty())
	{
		THUDEventListeners& listeners = s_eventVec[eventType];
		stl::push_back_unique(listeners, pListener);
	}
#endif
}


/*static*/ void CHUDEventDispatcher::AddHUDEventListener( IHUDEventListener* pListener, EHUDEventType in_eventType )
{
#if !defined(DEDICATED_SERVER)
	CRY_ASSERT_MESSAGE(s_stlSafe, "HUD: Can't call AddHUDEventListener() in an OnHUDEvent() callback!" );
	if(s_stlSafe==false)
	{
		// HUD Event Sending is now unsafe and should not be done!
		// Do not use event callbacks to un-listen for events.
		return;
	}

	if (!s_eventVec.empty())
	{
		THUDEventListeners& listeners = s_eventVec[in_eventType];
		stl::push_back_unique(listeners, pListener);
	}
#endif
}


/*static*/ void CHUDEventDispatcher::RemoveHUDEventListener( const IHUDEventListener* pListener )
{
#if !defined(DEDICATED_SERVER)
	CRY_ASSERT_MESSAGE(s_stlSafe, "HUD: Can't call RemoveHUDEventListener() in an OnHUDEvent() callback!" );
	if(s_stlSafe==false)
	{
		// HUD Event Unregistering is now unsafe and should not be done!
		// We are currently iterating over the vector, this operation is unsafe!
		return;
	}

	TEventVector::iterator vecIt = s_eventVec.begin();
	TEventVector::iterator vecEnd = s_eventVec.end();
	for (; vecIt != vecEnd; ++vecIt)
	{
		THUDEventListeners& listeners = (*vecIt);
		THUDEventListeners::iterator it = listeners.begin();
		THUDEventListeners::iterator end = listeners.end();
		
		for(; it!=end; ++it)
		{
			if((*it)==pListener)
			{
				listeners.erase(it);
				break;
			}
		}
	}
#endif
}



/*static*/ void CHUDEventDispatcher::CallEvent( const SHUDEvent& event )
{
#if !defined(DEDICATED_SERVER)
	CRY_PROFILE_FUNCTION(PROFILE_GAME);
	CRY_ASSERT_MESSAGE(s_safe, "HUD: Can't send HUDEvent whilst initialising the HUD! i.e. from CHUDObject() or CHUDObject::Init()!" );
	if(s_safe && !gEnv->pSystem->IsQuitting() && !s_eventVec.empty())
	{
		s_stlSafe = false;
		THUDEventListeners& listeners = s_eventVec[event.eventType];
		THUDEventListeners::const_iterator it = listeners.begin();
		THUDEventListeners::const_iterator end = listeners.end();
		for(; it!=end; ++it)
		{
			IHUDEventListener* listener = *it;
			listener->OnHUDEvent(event);
		}
		s_stlSafe = true;
	}
	else
	{
		// HUD Event Sending is now unsafe and should not be done!
		// If you want to send HUD Events on initialization then use OnHUDLoadDone.
		return;
	}
#endif
}


/*static*/ EHUDEventType CHUDEventDispatcher::GetEvent(const char* eventName)
{
#if !defined(DEDICATED_SERVER)
	size_t regdEventCount = sizeof( s_eventLookUpTable )/sizeof(THUDEventLookup);
	for(size_t i=0; i<regdEventCount; ++i )
	{
		const THUDEventLookup& eventInfo = s_eventLookUpTable[i];
		const char* arrayName = eventInfo.key.c_str();
		const EHUDEventType eventType = eventInfo.value;

		if( 0==stricmp(eventName,arrayName))
		{
			return eventType;
		}
	}
#endif
	return eHUDEvent_None;
}

string CHUDEventDispatcher::GetEventName(EHUDEventType inputEvent)
{
#if !defined(DEDICATED_SERVER)
	const size_t regdEventCount = sizeof(s_eventLookUpTable)/sizeof(THUDEventLookup);
	for (size_t i = 0; i<regdEventCount; ++i)
	{
		const THUDEventLookup& eventInfo = s_eventLookUpTable[i];
		if(eventInfo.value == inputEvent)
		{
			return eventInfo.key;
		}
	}
#endif
	return string("");
}

/*static*/ void CHUDEventDispatcher::CheckRegisteredEvents( void )
{
#if !defined(DEDICATED_SERVER)
	size_t regdEventCount = CRY_ARRAY_COUNT(s_eventLookUpTable);

	// Check for delta sizes
	CRY_ASSERT_MESSAGE( regdEventCount == eHUDEvent_LAST-1, string().Format("HUD: Wrong number of events registered (%d) when compared to the number defined (%d)! /FH", regdEventCount, (int)eHUDEvent_LAST - 1 ).c_str() );

	// Check for unreg'd
	for(size_t defdEventId=eHUDEvent_None+1; defdEventId<eHUDEvent_LAST; ++defdEventId )
	{
		bool definedEventFound = false;
		for( size_t regdEventId=0; regdEventId<regdEventCount; ++regdEventId )
		{
			if( s_eventLookUpTable[regdEventId].value == defdEventId )
			{
				definedEventFound = true;
			}
		}
		CRY_ASSERT_MESSAGE( definedEventFound, string().Format("HUD: A defined event id was not found when looking for it in registered events. ID was %d. /FH", defdEventId).c_str() );
	}

	// Check for dupes
	for(size_t i=0; i<regdEventCount; ++i )
	{
		const char* regdName = s_eventLookUpTable[i].key.c_str();
		const EHUDEventType redgType = s_eventLookUpTable[i].value;
		for(size_t j=i+1; j<regdEventCount; ++j )
		{
			const char* checkName = s_eventLookUpTable[j].key.c_str();
			const EHUDEventType checkType = s_eventLookUpTable[j].value;
			CRY_ASSERT_MESSAGE( stricmp(regdName, checkName), string().Format("HUD: Duplicate event names found for %s(id%d). /FH", regdName, redgType ).c_str() );
			CRY_ASSERT_MESSAGE( redgType != checkType, string().Format("HUD: Duplicate event ID entries found names found for ID%d(%s). /FH", redgType, regdName).c_str() );
		}
	}
#endif
}

void CHUDEventDispatcher::SetSafety( const bool safe)
{
	s_safe = safe;
}

//////////////////////////////////////////////////////////////////////////
