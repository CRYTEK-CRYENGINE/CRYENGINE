// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __HUDEVENTWRAPPER_H__
#define __HUDEVENTWRAPPER_H__


//////////////////////////////////////////////////////////////////////////
#include "Audio/AudioSignalPlayer.h"
#include "Audio/AudioTypes.h"
#include "Audio/AudioSignalPlayer.h"
#include "GameRulesTypes.h"
#include "ItemSharedParams.h"
#include "UI/UITypes.h"

//////////////////////////////////////////////////////////////////////////

struct IWeapon;

//////////////////////////////////////////////////////////////////////////

enum EBL_IconFrames{
	eBLIF_NoIcon = 0,
	eBLIF_Melee = 1,
	eBLIF_Suicide = 2,
	eBLIF_Weapon = 3,	// only frame used in pvp messages now
	eBLIF_Stamp,	// not a valid frame. Used as an ID instead
	eBLIF_FragGrenade,		// not a valid frame. Used as an ID instead
	eBLIF_FlashGrenade,		// not a valid frame. Used as an ID instead
	eBLIF_SmokeGrenade,		// not a valid frame. Used as an ID instead
	eBLIF_C4,							// not a valid frame. Used as an ID instead
	eBLIF_HeadShot,				// not a valid frame. Used as an ID instead
	eBLIF_CarDeath,				// not a valid frame. Used as an ID instead
	eBLIF_ExplodingBarrel, // not a valid frame. Used as an ID instead
	eBLIF_MicrowaveBeam,		// not a valid frame. Used as an ID instead
	eBLIF_SuitDisrupter,		// not a valid frame. Used as an ID instead
	eBLIF_CrashSiteLanding,	// not a valid frame. Used as an ID instead
	eBLIF_MeleeInWeaponsFlash, // not a valid frame. Used as an ID instead
	eBLIF_SlidingMelee,			// not a valid frame. Used as an ID instead
	eBLIF_FallDeath,				// not a valid frame. Used as an ID instead
	eBLIF_PhysicsImpact,		// not a valid frame. Used as an ID instead
	eBLIF_StealthKill,			// not a valid frame. Used as an ID instead
	eBLIF_AutoTurret,				// not a valid frame. Used as an ID instead
	eBLIF_FireDamage,				// not a valid frame. Used as an ID instead
	eBLIF_SuicideInWeaponsFlash, // not a valid frame. Used as an ID instead
	eBLIF_ShotgunAttachment,  // not a valid frame. Used as an ID instead
	eBLIF_GaussAttachment,		// not a valid frame. Used as an ID instead
	eBLIF_GrenadeAttachment,	// not a valid frame. Used as an ID instead
	eBLIF_Relay,							// not a valid frame. Used as an ID instead
	eBLIF_Tick,								// not a valid frame. Used as an ID instead
	eBLIF_Swarmer,						// not a valid frame. Used as an ID instead

	// TODO : Headshot, currently using melee
};


// Wrappers for sending Events to the HUD...
struct SHUDEventWrapper
{
	// Message data //////////

	struct SMsgAudio
	{
	public:
		enum EType
		{
			eT_None = 0,
			eT_Signal,
			eT_Announcement
		};
		union UData
		{
			struct SSignal
			{
				TAudioSignalID  id;
			}
			signal;
			struct SAnnouncment
			{
				int  teamId;
				EAnnouncementID  id;
			}
			announcement;
		};

	public:
		UData  m_datau;
		EType  m_type;

	public:
		SMsgAudio()
		{
			m_type = eT_None;
		}
		SMsgAudio(const TAudioSignalID signalId)
		{
			m_datau.signal.id = signalId;
			m_type = eT_Signal;
		}
		SMsgAudio(const int teamId, const EAnnouncementID announceId)
		{
			m_datau.announcement.teamId = teamId;
			m_datau.announcement.id = announceId;
			m_type = eT_Announcement;
		}
	};

	static const SMsgAudio  kMsgAudioNULL;

	static CAudioSignalPlayer s_unlockSound;
	static CAudioSignalPlayer s_unlockSoundRare;

	// Messages //////////

	static void GameStateNotify(const char* msg, const SMsgAudio& audio, const char* p1=NULL, const char* p2=NULL, const char* p3=NULL, const char* p4=NULL);
	static void GameStateNotify( const char* msg, const char* p1=NULL, const char* p2=NULL, const char* p3=NULL, const char* p4=NULL);
	static void RoundMessageNotify(const char* msg, const SMsgAudio& audio=kMsgAudioNULL);
	static void DisplayLateJoinMessage();
	static void TeamMessage( const char *msg, const int team, const SHUDEventWrapper::SMsgAudio& audio, const bool bShowTeamName, const bool bShowTeamIcon, const char * pCustomHeader=NULL, const float timeToHoldPauseState=0.f);
	static void SimpleBannerMessage( const char *msg, const SHUDEventWrapper::SMsgAudio& audio, const float timeToHoldPauseState=0.f);
	static void OnChatMessage( const EntityId entity, const int teamId, const char* message);
	static void OnGameStateMessage( const EntityId actorId, const bool bIsFriendly, const char* pMessage, EBL_IconFrames icon=eBLIF_NoIcon);
	static void OnAssessmentCompleteMessage( const char* groupName, const char* assesmentName, const char* description, const char* descriptionParam, const int xp );
	static void OnNewMedalMessage( const char* name, const char* description, const char* descriptionParam );
	static void OnPromotionMessage( const char* rankName, const int rank, const int xpRequired );
	static void OnSupportBonusXPMessage( const int xpType /*EPPType*/, const int xpGained );
	static void OnSkillKillMessage( const int xpType /*EPPType*/, const int xpGained );
	static void OnGameStatusUpdate( const EGoodBadNeutralForLocalPlayer good, const char* message );
	static void OnRoundEnd( const int winner, const bool clientScoreIsTop, const char* victoryDescMessage, const char* victoryMessage, const EAnnouncementID announcement );
	static void OnSuddenDeath( void );
	static void OnGameEnd(const int teamOrPlayerId, const bool clientScoreIsTop, const EGameOverReason reason, const char* message, const ESVC_DrawResolution drawResolution, const EAnnouncementID announcement);

	static void UpdateGameStartCountdown(const EPreGameCountdownType countdownType, const float timeTillStartInSeconds);

	static void CantFire(void);
	static void FireModeChanged(IWeapon* pWeapon, const int currentFireMode, bool bForceFireModeUpdate = false);
	static void ForceCrosshairType(IWeapon* pWeapon, const ECrosshairTypes forcedCrosshairType );
	static void OnInteractionUseHoldTrack(const bool bTrackingUse);
	static void OnInteractionUseHoldActivated(const bool bSuccessful);
	static void InteractionRequest(const bool activate, const char* msg, const char* action, const char* actionmap, const float duration, const bool bGameRulesRequest = false, const bool bIsFlowNodeReq = false, const bool bShouldSerialize = true); // TODO: Remove this and all uses of it, should use SetInteractionMsg
	static void ClearInteractionRequest(const bool bGameRulesRequest = false); // TODO: Remove this and all uses of it, should use ClearInteractionMsg
	static void SetInteractionMsg(const EHUDInteractionMsgType interactionType, 
																const char* message, 
																const float duration, 
																const bool bShouldSerialize, 
																const char* interactionAction = NULL,																// Interaction types only (But don't need if using new method (i.e. message="Press [[ACTIONNAME,ACTIONMAPNAME]] to do action")
																const char* interactionActionmap = NULL,														// Interaction types only (But don't need if using new method (i.e. message="Press [[ACTIONNAME,ACTIONMAPNAME]] to do action")
																const EntityId usableEntityId = 0,																	// Usable types only
																const EntityId usableSwapEntityId = 0,															// Usable types only
																const EInteractionType usableInteractionType = eInteraction_None,   // Usable types only
																const char* szExtraParam1 = NULL);																	// Used as a parameter for %2 for non usable msgs
	static void ClearInteractionMsg(EHUDInteractionMsgType interactionType, const char* onlyRemoveMsg = NULL, const float fadeOutTime = -1.0f);
	static void DisplayInfo(const int system, const float time, const char* infomsg, const char* param1=NULL, const char* param2=NULL, const char* param3=NULL, const char* param4=NULL, const EInfoSystemPriority priority=eInfoPriority_Low);
	static void ClearDisplayInfo(const int system, const EInfoSystemPriority priority=eInfoPriority_Low);
	static void GenericBattleLogMessage(const EntityId actorId, const char *message, const char* p1=NULL, const char* p2=NULL, const char* p3=NULL, const char* p4=NULL);

	static void HitTarget( EGameRulesTargetType targetType, int bulletType, EntityId targetId );

	static void OnNewObjective( const EntityId entityId, const EGameRulesMissionObjectives iconType );
	static void OnNewObjective( const EntityId entityId, const EGameRulesMissionObjectives iconType, const float progress, const int priority );
	static void OnNewObjective( const EntityId entityId, const EGameRulesMissionObjectives iconType, const float progress, const int priority, const char *pNameOverride, const char *pColourStr );
	static void OnNewObjectiveWithRadarEntity( const EntityId entityId, const EntityId radarEntityId, const EGameRulesMissionObjectives iconType, const float progress, const int priority, const char *pNameOverride, const char *pColourStr );
	static void OnRemoveObjective( const EntityId entityId );
	static void OnRemoveObjective( const EntityId entityId, const int priority );
	static void OnRemoveObjectiveWithRadarEntity( const EntityId entityId, const EntityId radarEntityId, const int priority );
	static void OnNewGameRulesObjective( const EntityId entityId );

	static void RadarSweepActivated(int targetTeam, float activateDuration);

	static void OnBootAction(const uint32 bootAction, const char* elementName);

	static void ActivateOverlay(const int type);
	static void DeactivateOverlay(const int type);

	// Items
	static void OnPrepareItemSelected(const EntityId itemId, const EItemCategoryType category, const EWeaponSwitchSpecialParam specialParam );

	// Players
	static void PlayerRename(const EntityId playerId, const bool isReplay = false);

	// Vehicles
	static void OnPlayerLinkedToVehicle(const EntityId playerId, const EntityId vehicleId, bool alwaysShowHealthBar = false);
	static void OnPlayerUnlinkedFromVehicle(const EntityId playerId, const EntityId vehicleId, const bool keepHealthBar = false);

	static void OnStartTrackFakePlayerTagname( const EntityId tagEntityId, const int sessionNameIdxToTrack );
	static void OnStopTrackFakePlayerTagname( const EntityId tagEntityId );

	static void PowerStruggleNodeStateChange(const int activeNodeIdentityId, const int nodeHUDState);
	static void PowerStruggleManageCaptureBar(EHUDPowerStruggleCaptureBarType inType, float inChargeAmount, bool inContention, const char *inBarString);

	static void OnBigMessage(const char *inSubTitle, const char *inTitle);
	static void OnBigWarningMessage(const char *line1, const char *line2, const float duration = -1.f);
	static void OnBigWarningMessageUnlocalized(const char *line1, const char *line2, const float duration = -1.f);
	static void UpdatedDirectionIndicator(const EntityId &id);
	static void SetStaticTimeLimit(bool active, int time);

	static void PlayUnlockSound();
	static void PlayUnlockSoundRare();

	static void DisplayWeaponUnlockMsg(const char* szCollectibleId);
};

//////////////////////////////////////////////////////////////////////////

#endif // __HUDEVENTWRAPPER_H__
