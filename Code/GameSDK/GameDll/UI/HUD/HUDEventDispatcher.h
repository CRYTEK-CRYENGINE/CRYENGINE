// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __HUDEVENTDISPATCHER_H__
#define __HUDEVENTDISPATCHER_H__


//////////////////////////////////////////////////////////////////////////


#include <CryCore/Containers/CryFixedArray.h>


//////////////////////////////////////////////////////////////////////////

#define MAX_CONCURRENT_HUDEVENTS 10
#define MAX_HUDEVENT_DATA 10

//////////////////////////////////////////////////////////////////////////

typedef CryFixedStringT<32> TEventName;

//////////////////////////////////////////////////////////////////////////

enum EFadeCrosshairFlags
{
	eFadeCrosshair_None				= 0,
	eFadeCrosshair_Flowgraph	= BIT(0),
	eFadeCrosshair_Ironsight	= BIT(1),
	eFadeCrosshair_KVolt			= BIT(2),
	eFadeCrosshair_Other			= BIT(3),
	eFadeCrosshair_KillCam		= BIT(4),
};

enum EHUDEventType
{
	eHUDEvent_None = 0,
	// HUD State & resolutions
	eHUDEvent_OnHUDUnload,     // cleanup
	eHUDEvent_OnHUDLoadDone,   // load done
	eHUDEvent_OnHUDReload,     // reinitialize
	eHUDEvent_OnAssetOnDemandUnload,
	eHUDEvent_OnAssetOnDemandLoad,
	eHUDEvent_OnUpdate,        // game tick i.e. when not in the active state.
	eHUDEvent_OnPostHUDDraw,
	eHUDEvent_OnResolutionChange,
	eHUDEvent_OnOverscanBordersChange,
	eHUDEvent_HUDElementVisibility,
	eHUDEvent_HUDBoot,
	eHUDEvent_OnChangeHUDStyle,
	eHUDEvent_OnHUDStyleChanged,
	eHUDEvent_OnPreHUDStyleChanged,
	eHUDEvent_OnHUDStyleChangeWeaponArea,
	eHUDEvent_OnSubtitlesDraw,
	eHUDEvent_OnSubtitlesChanged,
	eHUDEvent_OnShowSkippablePrompt,
	eHUDEvent_ForceDrawSubtitles,
	eHUDEvent_OnCustomTextSet,
	eHUDEvent_OnCustomTextDraw,
	eHUDEvent_OnInteractiveHintDraw,
	eHUDEvent_HUDSnapToCamera,
	eHUDEvent_OnStartActivateNewState,
	eHUDEvent_OnEndActivateNewState,
	eHUDEvent_OnControlSchemeSwitch,
	// Local Player
	eHUDEvent_OnInitLocalPlayer,
	eHUDEvent_OnHealthChanged,
	eHUDEvent_OnLocalPlayerKilled,
	eHUDEvent_OnLocalPlayerDeath,
	eHUDEvent_OnLocalPlayerCanNowRespawn,
	eHUDEvent_OnViewDistanceChanged,
	eHUDEvent_OnSpawn,
	eHUDEvent_PlayerSwitchTeams,
	eHUDEvent_ShowReviveCycle,
	eHUDEvent_OnShowHitIndicator, // local player taking damage
	eHUDEvent_OnShowHitIndicatorPlayerUpdated,
	eHUDEvent_OnShowHitIndicatorBothUpdated,
	eHUDEvent_OnViewUpdate,
	eHUDEvent_SetupPlayerTeamSpecifics,
	eHUDEvent_OnScannedExternally,
	eHUDEvent_OnHitEMP,
	eHUDEvent_ShowEnemyHealthbars,
	eHUDEvent_OnUseLedge,
	eHUDEvent_ResetHUDStyle,
	// Remote player
	eHUDEvent_OnEnterGame_RemotePlayer,
	eHUDEvent_OnLeaveGame_RemotePlayer,
	eHUDEvent_OnRemotePlayerDeath,
	eHUDEvent_OnPlayerRevive,
	eHUDEvent_OnGenericBattleLogMessage,
	eHUDEvent_OnLobbyRemove_RemotePlayer,
	//eHUDEvent_OnSpawn_NonLocalPlayer,
	//eHUDEvent_OnLeave_NonLocalPlayer,
	// Player
	eHUDEvent_RenamePlayer,
	// Score and progression
	eHUDEvent_OnNewScore,
	eHUDEvent_OnNewSkillKill,
	eHUDEvent_OnAssessmentComplete,
	eHUDEvent_OnPlayerPromotion,
	// SuitMode & Menu
	eHUDEvent_OnSuitModeChanged,
	eHUDEvent_OnEnergyChanged,
	eHUDEvent_OnSuitOverChargeChanged,
	eHUDEvent_OnSuitStateChanged,
	eHUDEvent_OnSuitMenuOpened,
	eHUDEvent_OnSuitMenuClosed,
	eHUDEvent_OnSuitPowerActivated,
	eHUDEvent_OnSuitPowerSpecial,
	eHUDEvent_OnShowVisionMenu,
	eHUDEvent_OnHideVisionMenu,
	// Weapons, Weapon Feedback & Crosshair
	eHUDEvent_OnStartReload,
	eHUDEvent_OnReloaded,
	eHUDEvent_OnItemPickUp,
	eHUDEvent_OnAmmoPickUp,
	eHUDEvent_OnSetAmmoCount,
	eHUDEvent_OnSetInventoryAmmoCount,
	eHUDEvent_OnShoot,
	eHUDEvent_OnItemSelected,
	eHUDEvent_OnPrepareItemSelected,
	eHUDEvent_OnFireModeChanged,
	eHUDEvent_OnWeaponFireModeChanged, //This one get's fired when the weapon it self is doing a firemode change, and not when we are switching weapons or such.
	eHUDEvent_OnHitTarget,
	eHUDEvent_OnTargetStuck,
	eHUDEvent_OnHit,
	eHUDEvent_OnZoom,
	eHUDEvent_OnExplosiveSpawned,
	eHUDEvent_OnExplosiveDetonationDelayed,
	eHUDEvent_OnGrenadeExploded,
	eHUDEvent_OnGrenadeThrow,
	eHUDEvent_FadeCrosshair,
	eHUDEvent_CantFire,
	eHUDEvent_ShowExplosiveMenu,
	eHUDEvent_HideExplosiveMenu,
	eHUDEvent_SelectExplosive,
	eHUDEvent_ShowDpadMenu,
	eHUDEvent_OnCrosshairVisibilityChanged,
	eHUDEvent_OnCrosshairModeChanged,
	eHUDEvent_ShowNoAmmoWarning,
	eHUDEvent_OnNoPickableAmmoForItem, // Called for items that can not pick up ammo from ammo boxes
	eHUDEvent_IsDoubleTapExplosiveSelect, 
	eHUDEvent_ForceCrosshairType,			// Allows forced override of current crosshair for special cases
	eHUDEvent_OnC4Spawned,
	eHUDEvent_RemoveC4Icon,
	eHUDEvent_OnForceWeaponDisplay,
	eHUDEvent_ForceInfiniteAmmoIcon,
	eHUDEvent_OnBowChargeValueChanged,
	eHUDEvent_OnBowFired,
	eHUDEvent_DisplayTrajectory,
	eHUDEvent_OnTrajectoryTimeChanged,
	eHUDEvent_UpdateExplosivesAmmo,
	// Prompts (pickups and interactions)
	eHUDEvent_OnLookAtChanged,
	eHUDEvent_OnInteractionRequest,
	eHUDEvent_OnSetInteractionMsg,
	eHUDEvent_OnClearInteractionMsg,
	eHUDEvent_OnSetInteractionMsg3D,
	eHUDEvent_OnClearInteractionMsg3D,
	eHUDEvent_OnSetWeaponOverlay,
	eHUDEvent_OnUsableChanged,
	eHUDEvent_ShowingUsablePrompt,
	eHUDEvent_HidingUsablePrompt,
	eHUDEvent_OnInteractionUseHoldTrack,
	eHUDEvent_OnInteractionUseHoldActivated,
	eHUDEvent_OnGameStateNotifyMessage,	// on sending event - do not localise message; on receiving event - message is already localised!
	eHUDEvent_DisplayLateJoinMessage,
	eHUDEvent_OnTeamMessage,
	eHUDEvent_OnSimpleBannerMessage,
	eHUDEvent_OnPromotionMessage,
	eHUDEvent_OnServerMessage,
	eHUDEvent_OnNewMedalAward,
	eHUDEvent_AddOnScreenMessage,
	eHUDEvent_FlushMpMessages,
	eHUDEvent_InfoSystemsEvent,
	eHUDEvent_OnSetCustomInputAction,
	eHUDEvent_OnCustomInputAction,
	// MP Gamemodes
	eHUDEvent_GameEnded,
	eHUDEvent_MakeMatchEndScoreboardVisible,
	eHUDEvent_OnGamemodeChange,
	eHUDEvent_OnInitGameRules,
	eHUDEvent_OnUpdateGameStartMessage,
	eHUDEvent_OnWaitingForPlayers,
	eHUDEvent_OnGameStart,
	eHUDEvent_OnSetGameStateMessage,
	eHUDEvent_OnRoundStart,
	eHUDEvent_OnRoundMessage,
	eHUDEvent_OnRoundEnd,
	eHUDEvent_OnRoundAboutToStart,
	eHUDEvent_OnSuddenDeath,
	eHUDEvent_OnUpdateGameResumeMessage,
	eHUDEvent_ForceRespawnTimer,
	eHUDEvent_OnPrematchFinished,
	// CTF Specific
	eHUDEvent_OnLocalClientStartCarryingFlag,
	// Power Struggle Specific
	eHUDEvent_OnPowerStruggle_NodeStateChange,
	eHUDEvent_OnPowerStruggle_GiantSpearCharge,
	eHUDEvent_OnPowerStruggle_ManageCaptureBar,
	// SP Objectives
	eHUDEvent_OnObjectiveChanged, // used by visor, objective display, nave path.
	eHUDEvent_HighlightPrimaryObjective,
	eHUDEvent_OnObjectiveAnalysis,
	eHUDEvent_OnRadioReceived,
	eHUDEvent_OnDrawLine,
	eHUDEvent_OnRemoveLine,
	eHUDEvent_OnRemoveAllLines,
	// MP Objectives
	eHUDEvent_OnNewObjective,
	eHUDEvent_OnRemoveObjective,
	eHUDEvent_OnNewGameRulesObjective,
	eHUDEvent_OnSiteBeingCaptured, // used by assault & power struggle light.
	eHUDEvent_OnSiteCaptured,
	eHUDEvent_OnSiteLost,
	eHUDEvent_OnSiteAboutToExplode,
	eHUDEvent_OnClientInOwnedSite,
	eHUDEvent_OnClientInContestedSite,
	eHUDEvent_OnClientLeftSite,
	eHUDEvent_OnCaptureObjectiveNumChanged, // called when the number of tracked entities changes
	eHUDEvent_OnCaptureObjectiveRefreshEnabledState,
	eHUDEvent_OnNewCaptureObjectiveWave,
	eHUDEvent_OnOverallCaptureProgressUpdate,
	eHUDEvent_ObjectivesUpdateText,
	eHUDEvent_ObjectiveUpdateClock,
	eHUDEvent_OnUpdateObjectivesLeft,
	eHUDEvent_PredatorGracePeriodFinished,
	eHUDEvent_PredatorAllMarinesDead,
	eHUDEvent_SuicidePenalty, // Currently only hunter game mode has a time penalty for suicide
	eHUDEvent_SetAttackingTeam,
	eHUDEvent_OnBigMessage,
	eHUDEvent_OnBigWarningMessage,
	eHUDEvent_OnBigWarningMessageUnlocalized,
	eHUDEvent_EnableNetworkDebug,
	eHUDEvent_DisableNetworkDebug,
	eHUDEvent_OnSetStaticTimeLimit,
	// Kill cam replay & Spectator Cam
	eHUDEvent_OnKillCamStartPlay,
	eHUDEvent_OnKillCamStopPlay,
	eHUDEvent_OnKillCamPostStopPlay,
	eHUDEvent_OnKillCamWeaponSwitch,
	eHUDEvent_OnKillCamSuitMode,
	eHUDEvent_OnWinningKillCam,
	eHUDEvent_OnHighlightsReel,
	// PC Kick Voting
	eHUDEvent_OnVotingStarted,
	eHUDEvent_OnVotingProgressUpdate,
	eHUDEvent_OnVotingEnded,
	// Blind
	eHUDEvent_OnBlind,
	eHUDEvent_OnEndBlind,
	// Radar & Map
	eHUDEvent_AlignToRadarAsset,
	eHUDEvent_RadarRotation,
	eHUDEvent_ToggleMap,
	eHUDEvent_TemporarilyTrackEntity,
	eHUDEvent_OnLocalActorTagged,
	eHUDEvent_RescanActors,  
	eHUDEvent_AddEntity,     // TODO: Use objective events instead.
	eHUDEvent_RemoveEntity,  // TODO: Use objective events instead.
	eHUDEvent_OnUpdateMinimap,
	eHUDEvent_LoadMinimap,
	eHUDEvent_OnChangeSquadState,
	eHUDEvent_OnRadarJammingChanged,
	eHUDEvent_OnRadarSweep,
	// Battle area
	eHUDEvent_LeavingBattleArea,
	eHUDEvent_ReturningToBattleArea,
	// Host migration
	eHUDEvent_ShowHostMigrationScreen,
	eHUDEvent_HideHostMigrationScreen,
	eHUDEvent_HostMigrationOnNewPlayer,
	eHUDEvent_NetLimboStateChanged,

	eHUDEvent_OnAIAwarenessChanged,
	eHUDEvent_OnTeamRadarChanged,
	eHUDEvent_OnMaxSuitRadarChanged,
	eHUDEvent_SetRadarSweepCount,
	eHUDEvent_OnMapDeploymentChanged,
	// Tagnames
	eHUDEvent_OnIgnoreEntity,
	eHUDEvent_OnStopIgnoringEntity,
	eHUDEvent_OnStartTrackFakePlayerTagname,
	eHUDEvent_OnEndTrackFakePlayerTagname,
	// Scanning/Tactical info
	eHUDEvent_OnScanningStart,
	eHUDEvent_OnScanningComplete,
	eHUDEvent_OnEntityScanned,
	eHUDEvent_OnTaggingStart,
	eHUDEvent_OnTaggingComplete,
	eHUDEvent_OnEntityTagged,
	eHUDEvent_OnScanningStop,
	eHUDEvent_OnControlCurrentTacticalScan,
	eHUDEvent_OnCenterEntityChanged,
	eHUDEvent_OnCenterEntityScannableChanged,
	eHUDEvent_OnVisorChanged,
	eHUDEvent_OnTacticalInfoChanged,
	eHUDEvent_OnWeaponInfoChanged,
	eHUDEvent_OnChatMessage,
	eHUDEvent_OnNewBattleLogMessage,
	eHUDEvent_OnNewGameStateMessage,
	eHUDEvent_ToggleChatInput,
	eEHUDEvent_OnScannedEnemyRemove,
	// Menus & Scoreboards
	eHUDEvent_OpenedIngameMenu,
	eHUDEvent_ClosedIngameMenu,
	eHUDEvent_ShowScoreboard,
	eHUDEvent_HideScoreboard,
	eHUDEvent_ShowWeaponCustomization,
	eHUDEvent_HideWeaponCustomization,
	eHUDEvent_OnTutorialEvent,
	// Save / Load
	eHUDEvent_OnGameSave,
	eHUDEvent_OnGameLoad,
	eHUDEvent_OnFileIO,
  // Nano Web
	eHUDEvent_ShowNanoWeb,
	eHUDEvent_HideNanoWeb,
	// Navigation Path
	eHUDEvent_PosOnNavPath,
	// cinematic hud
  eHUDEvent_OnCinematicPlay,
  eHUDEvent_OnCinematicStop,
	eHUDEvent_OnBeginCutScene,
	eHUDEvent_OnEndCutScene,
	eHUDEvent_CinematicVTOLUpdate,
	eHUDEvent_CinematicTurretUpdate,
	eHUDEvent_CinematicScanning,
	eHUDEvent_OnShowVideoOnHUD,
	eHUDEvent_TurretHUDActivated,
	//Radar
	eHUDEvent_ClearRadarData,
	// MouseWheel
	eHUDEvent_ShowMouseWheel,
	eHUDEvent_HideMouseWheel,
	// Nano Glass
	eHUDEvent_ShowNanoGlass,
	// EnergyOverlay
	eHUDEvent_ActivateOverlay,
	eHUDEvent_DeactivateOverlay,
	// vehicles
	eHUDEvent_PlayerLinkedToVehicle,
	eHUDEvent_PlayerUnlinkedFromVehicle,
	eHUDEvent_OnVehiclePlayerEnter,
	eHUDEvent_OnVehiclePlayerLeave,
	eHUDEvent_OnVehiclePassengerEnter,
	eHUDEvent_OnVehiclePassengerLeave,
	// local player target
	eHUDEvent_LocalPlayerTargeted,
  // Last
	eHUDEvent_LAST // KEEP LAST!
};

//////////////////////////////////////////////////////////////////////////


struct SHUDEventData
{
	enum EType
	{
		eSEDT_undef,
		eSEDT_int,
		eSEDT_float,
		eSEDT_bool,
		eSEDT_voidptr,
	};

	union
	{
		int			m_int;
		float		m_float;
		bool		m_bool;
		const void*		m_pointer;
	};

	int8 m_type;

	SHUDEventData() : m_pointer(NULL), m_type(eSEDT_undef) {}
	SHUDEventData(int val) : m_int(val), m_type(eSEDT_int) {}
	SHUDEventData(float val) : m_float(val), m_type(eSEDT_float) {}
	SHUDEventData(bool val) : m_bool(val), m_type(eSEDT_bool) {}
	SHUDEventData(const void* val) : m_pointer(val), m_type(eSEDT_voidptr) {}

	int GetInt() const { assert(m_type==eSEDT_int); return m_int; }
	float GetFloat() const { assert(m_type==eSEDT_float); return m_float; }
	bool GetBool() const { assert(m_type==eSEDT_bool); return m_bool; }
	const void* GetPtr() const { assert( (m_pointer==NULL) || (m_type==eSEDT_voidptr) ); return m_pointer; }
};


//////////////////////////////////////////////////////////////////////////
struct SHUDEvent
{

public:

	typedef CryFixedArray<SHUDEventData,MAX_HUDEVENT_DATA> THUDEventData;
	typedef CryFixedArray<THUDEventData,MAX_CONCURRENT_HUDEVENTS> THUDEventDataStack;

	// cppcheck-suppress uninitMemberVar
	SHUDEvent()
		: eventType(eHUDEvent_None)
		, eventPtrData(nullptr)
		, eventIntData(0)
		, eventIntData2(0)
		, eventIntData3(0)
		, eventFloatData(0.0f)
		, eventFloat2Data(0.0f)
	{
#if !defined(DEDICATED_SERVER)
		GetNextFreeData();
		ClearData();
#endif
	}

	// cppcheck-suppress uninitMemberVar
	SHUDEvent(EHUDEventType type)
		: eventType(type)
		, eventPtrData(nullptr)
		, eventIntData(0)
		, eventIntData2(0)
		, eventIntData3(0)
		, eventFloatData(0.0f)
		, eventFloat2Data(0.0f)
	{
#if !defined(DEDICATED_SERVER)
		GetNextFreeData();
		ClearData();
#endif
	}

	~SHUDEvent()
	{
#if !defined(DEDICATED_SERVER)
		SHUDEvent::FreeDataId(m_dataId);
#endif
	}

	void ReserveData(uint8 size)
	{
	}

	void AddData(const SHUDEventData& data)
	{
#if !defined(DEDICATED_SERVER)
		assert(m_data->size()<MAX_HUDEVENT_DATA);
		m_data->push_back(data);
#endif
	}

	const SHUDEventData& GetData(uint8 index) const
	{
		CRY_ASSERT_MESSAGE(index < m_data->size(), "More SHUDEventData requested than provided!");
		return (*m_data)[index];
	}

	const unsigned int GetDataSize( void ) const
	{
		return m_data->size();
	}

	void ClearData( void )
	{
		
#if !defined(DEDICATED_SERVER)
		m_data->clear();
		eventPtrData = nullptr;
		eventIntData = 0;
		eventIntData2 = 0;
		eventIntData3 = 0;
		eventFloatData = 0.0f;
		eventFloat2Data = 0.0f;
#endif
	}

	void GetNextFreeData( void )
	{
#if !defined(DEDICATED_SERVER)
		m_dataId = SHUDEvent::GetNextFreeDataId();
		m_data = &(s_dataStack[m_dataId]);
#endif
	}

	static void InitDataStack()
	{
		for(uint8 i=0; i<MAX_CONCURRENT_HUDEVENTS; ++i)
		{
			THUDEventData data;
			s_dataStack.push_back(data);
			s_usedDataIds[i] = false;
		}
	}

	static void ClearDataStack()
	{
		s_dataStack.clear();
	}

private:

	static uint8 GetNextFreeDataId()
	{

		for(uint8 i=0; i<MAX_CONCURRENT_HUDEVENTS; ++i)
		{
			if(!s_usedDataIds[i])
			{
				s_usedDataIds[i] = true;
				return i;
			}
		}
		//We don't have any hud event data available anymore.
		assert(false);
		return 0;
	}

	static void FreeDataId(const uint8 dataId)
	{
		s_usedDataIds[dataId] = false;
		s_dataStack[dataId].clear();
	}

public:

	static bool s_usedDataIds[MAX_CONCURRENT_HUDEVENTS];
	static THUDEventDataStack s_dataStack;

	EHUDEventType	eventType;
	THUDEventData* m_data;

	//the following are depricated and should not be used anymore
	void* eventPtrData;
	int eventIntData;
	int eventIntData2;
	int eventIntData3;
	float eventFloatData;
	float eventFloat2Data;

	uint8	m_dataId;
};


//////////////////////////////////////////////////////////////////////////

struct SHUDEventData_String
{
	SHUDEventData_String()
	{
	}

	CryFixedStringT<32> string1;
	CryFixedStringT<32> string2;
};

//////////////////////////////////////////////////////////////////////////


struct IHUDEventListener
{
	virtual ~IHUDEventListener(){}
	virtual void OnHUDEvent(const SHUDEvent& event) = 0;
};


//////////////////////////////////////////////////////////////////////////

// These are static as there are time when the HUD itself may not be instantiated
// when registering unregistering listeners.
namespace CHUDEventDispatcher
{
	void					SetUpEventListener();
	void					CheckEventListenersClear();
	void					FreeEventListeners();
	void					AddHUDEventListener(IHUDEventListener* listener, const char* eventName);
	void					AddHUDEventListener(IHUDEventListener* listener, EHUDEventType eventType);
	void					RemoveHUDEventListener(const IHUDEventListener* listener);
	void					CallEvent(const SHUDEvent& event);
	EHUDEventType	GetEvent(const char* eventName);
	string        GetEventName(EHUDEventType inputEvent);

	void          CheckRegisteredEvents( void );

	void          SetSafety( const bool safe);
}


//////////////////////////////////////////////////////////////////////////

#endif

