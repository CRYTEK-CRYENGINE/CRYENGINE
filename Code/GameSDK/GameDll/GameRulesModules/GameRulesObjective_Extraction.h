// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Objective handler for extraction gamemode
	-------------------------------------------------------------------------
	History:
	- 10/05/2010  : Created by Jim Bamford

*************************************************************************/

#ifndef _GAME_RULES_OBJECTIVE_EXTRACTION_H_
#define _GAME_RULES_OBJECTIVE_EXTRACTION_H_

#if _MSC_VER > 1000
# pragma once
#endif

#include "GameRulesModules/IGameRulesObjectivesModule.h"
#include "GameRulesModules/IGameRulesKillListener.h"
#include "GameRulesModules/IGameRulesClientConnectionListener.h"
#include "GameRulesModules/IGameRulesModuleRMIListener.h"
#include "GameRulesModules/IGameRulesActorActionListener.h"
#include "GameRulesModules/IGameRulesTeamChangedListener.h"
#include "GameRulesModules/IGameRulesRoundsListener.h"
/*
#include "GameRulesModules/IGameRulesPickupListener.h"
*/
#include <CryGame/IGameFramework.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryCore/Containers/CryFixedArray.h>
#include "AutoEnum.h"
#include "Audio/GameAudio.h"

#if !defined(_RELEASE)
#define DEBUG_EXTRACTION			 (1)
#else
#define DEBUG_EXTRACTION			 (0)
#endif

#define EExtractionSuitModeList(f) \
	f(eExtractionSuitMode_None)						/*0*/ \
	f(eExtractionSuitMode_Power)					/*1*/ \
	f(eExtractionSuitMode_Armour)					/*2*/ \
	f(eExtractionSuitMode_Stealth)				/*3*/ \


class CGameRulesObjective_Extraction :		
	public IGameRulesObjectivesModule,
	public IEntityEventListener,
	public IGameRulesKillListener,
	public IGameRulesClientConnectionListener,
	public IGameRulesModuleRMIListener,
	public IGameRulesActorActionListener,
	public IGameRulesTeamChangedListener,
	public IGameRulesRoundsListener
{
public:
	CGameRulesObjective_Extraction();
	virtual ~CGameRulesObjective_Extraction();

	// IGameRulesObjectivesModule
	virtual void Init(XmlNodeRef xml);
	virtual void Update(float frameTime);

	virtual void OnStartGame();
	virtual void OnStartGamePost();
	virtual void OnGameReset() { OnStartGame(); }

	virtual void PostInitClient(int channelId) {};
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );

	virtual bool HasCompleted(int teamId) { return false; }

	virtual void OnHostMigration(bool becomeServer);

	virtual bool AreSuddenDeathConditionsValid() const;
	virtual void CheckForInteractionWithEntity(EntityId interactId, EntityId playerId, SInteractionInfo& interactionInfo);
	virtual bool CheckIsPlayerEntityUsingObjective(EntityId playerId);

	virtual ESpawnEntityState GetObjectiveEntityState(EntityId entityId) { return eSES_Unknown; }

	virtual int GetAutoAssignTeamId(int channelId) { return 0; }
	virtual void OnEntitySignal(EntityId entityId, int signal) { }
	// ~IGameRulesObjectivesModule

	// IEntityEventListener
	virtual void OnEntityEvent( IEntity *pEntity, const SEntityEvent& event );
	// ~IEntityEventListener

	// IGameRulesKillListener
	virtual void OnEntityKilledEarly(const HitInfo &hitInfo);
	virtual void OnEntityKilled(const HitInfo &hitInfo);
	// ~IGameRulesKillListener

	// IGameRulesClientConnectionListener
	virtual void OnClientConnect(int channelId, bool isReset, EntityId playerId) {};
	virtual void OnClientDisconnect(int channelId, EntityId playerId);
	virtual void OnClientEnteredGame(int channelId, bool isReset, EntityId playerId);
	virtual void OnOwnClientEnteredGame();
	// ~IGameRulesClientConnectionListener

	// IGameRulesModuleRMIListener
	virtual void OnSingleEntityRMI(CGameRules::SModuleRMIEntityParams params);
	virtual void OnDoubleEntityRMI(CGameRules::SModuleRMITwoEntityParams params);
	virtual void OnEntityWithTimeRMI(CGameRules::SModuleRMIEntityTimeParams params) {};
	virtual void OnSvClientActionRMI(CGameRules::SModuleRMISvClientActionParams params, EntityId fromEid);
	// ~IGameRulesModuleRMIListener

	// IGameRulesActorActionListener
	virtual void OnAction(const ActionId& action, int activationMode, float value);
	// ~IGameRulesActorActionListener

	// IGameRulesTeamChangedListener
	virtual void OnChangedTeam(EntityId entityId, int oldTeamId, int newTeamId);
	// ~IGameRulesTeamChangedListener

	// IGameRulesRoundsListener
	virtual void OnRoundStart();
	virtual void OnRoundEnd();
	virtual void OnSuddenDeath() {};
	virtual void ClRoundsNetSerializeReadState(int newState, int curState) {};
	virtual void OnRoundAboutToStart();
	// ~IGameRulesRoundsListener

protected:

	ICVar					*m_extractionDebuggingCVar;

	const IEntityClass *m_spawnAtEntityClass;
	const IEntityClass *m_extractAtEntityClass;
	IEntityClass *m_fallbackGunEntityClass;

	int m_droppedRemoveTime;
	int m_moduleRMIIndex;

	float m_defensiveKillRadiusSqr;
	float m_defendingDroppedRadiusSqr;
	float m_defendingDroppedTimeToScore;
	float m_pickUpVisCheckHeight;

	AUTOENUM_BUILDENUMWITHTYPE_WITHNUM(EExtractionSuitMode, EExtractionSuitModeList, eExtractionSuitMode_Num);
	
	// may not be needed
	enum EPickupState
	{
		ePickupState_None=0,
		ePickupState_AtBase,			// waiting to be picked up by opposite team
		ePickupState_Carried,			// being carried, test for being extracted
		ePickupState_Dropped,			// been dropped test for being picked up again and for being reset due to timeout
		ePickupState_Extracted,		// been extracted
	};
	
	enum EPhysicsType
	{
		ePhysType_None = 0,
		ePhysType_Networked,
	};
	EPhysicsType m_physicsType;

	static const int k_maxNumPlayers=MAX_PLAYER_LIMIT;
	typedef CryFixedArray<EntityId, k_maxNumPlayers> TPlayers;

	struct SDefender
	{
		EntityId m_playerId;
		float		 m_timeInRange;

		SDefender(EntityId inPlayerId)
		{
			m_playerId = inPlayerId;
			m_timeInRange = 0.f;
		}
	};

	typedef CryFixedArray<SDefender, k_maxNumPlayers> TDefenders;

	struct SPickup
	{
		IEntityClass *m_entityClass;
		EExtractionSuitMode m_suitModeEffected;
		EntityId						m_spawnAt;
		EntityId						m_spawnedPickupEntityId;
		EntityId						m_spawnedFallbackGunId;			// the weapon that all players with two primary weapons will gain whilst holding this tick. Server spawned and net synced
		EPickupState				m_state;
		EntityId						m_carrierId;
		EntityId						m_extractedBy;
		EntityId						m_droppedBy;
		float								m_timeSinceDropped;
		float								m_timeToCheckForbiddenArea;
		EGameRulesMissionObjectives m_currentObjectiveIcon;
		EntityId						m_currentObjectiveFollowEntity;
		int									m_currentObjectivePriority;
		int									m_currentObjectiveCountDown;
		const char				 *m_currentIconName;
		TPlayers						m_carriers;			// any players who have carried this pickup, this lifetime
		TDefenders					m_droppedDefenders;			// any players who are within a radius of this pickup when its dropped
		CAudioSignalPlayer	m_signalPlayer;

		SPickup()
		{
			m_entityClass=NULL;
			m_suitModeEffected=eExtractionSuitMode_None;
			m_spawnAt=0;
			m_spawnedPickupEntityId=0;
			CryLog("[state] CGameRulesObjective_Extraction::SPickup::SPickup: setting pickup's (*%p) state to ePickupState_None", this);
			m_spawnedFallbackGunId=0;
			m_state=ePickupState_None;
			m_carrierId=0;
			m_droppedBy=0;
			m_timeSinceDropped=0.f;
			m_timeToCheckForbiddenArea = 0.f;
			m_extractedBy=0;
			m_currentObjectiveIcon=EGRMO_Unknown;
			m_currentObjectiveFollowEntity=0;
			m_currentObjectivePriority=0;
			m_currentIconName=NULL;
			m_currentObjectiveCountDown=-1;
		}

		void SetState(EPickupState inState);
		void SetAudioState(EPickupState inState);
	};

	static const int k_maxNumPickups=5;
	typedef CryFixedArray<SPickup, k_maxNumPickups> TPickupElements;
	TPickupElements m_pickups;

	struct SSpawnAt
	{
		EntityId	m_entityId;
		int				m_numInProximity;
		SSpawnAt()
		{
			m_entityId				= 0;
			m_numInProximity	= 0;
		}
	};
	static const int k_maxNumSpawnAts=20;
	typedef CryFixedArray<SSpawnAt, k_maxNumSpawnAts> TSpawnAtElements;
	TSpawnAtElements m_spawnAtElements;
	typedef CryFixedArray<SSpawnAt*, k_maxNumSpawnAts> TAvailableSpawnAtElements;
	TAvailableSpawnAtElements m_availableSpawnAtElements;

	struct SExtractAt
	{
		EntityId m_entityId;
		bool		 m_hasPickupInRange;
		float		 m_sqrPickupRange;
		EGameRulesMissionObjectives m_currentObjectiveIcon;
		SExtractAt()
		{
			m_entityId=0;
			m_hasPickupInRange = false;
			m_sqrPickupRange = 0.0f;
			m_currentObjectiveIcon=EGRMO_Unknown;
		}
		bool operator==(const SExtractAt &rhs)
		{
			return (m_entityId == rhs.m_entityId);
		}
	};
	static const int k_maxNumExtractAts=20;
	typedef CryFixedArray<SExtractAt, k_maxNumExtractAts> TExtractAtElements;
	TExtractAtElements m_extractAtElements;
	bool m_haveStartedGame;
	float m_timeLimit;
	float m_previousTimeTaken;
	int m_numberToExtract;

	Vec3 m_spawnOffset;

	bool m_useButtonHeld;
	bool m_attemptPickup;
	
	enum ERMITypes
	{
		eRMIType_None=0, 

		// single entity RMIs
		eRMITypeSingleEntity_extraction_point_added,

		// double entity RMIs
		eRMITypeDoubleEntity_power_pickup_spawned,			// pickup entity1 has been spawned at base entity2
		eRMITypeDoubleEntity_armour_pickup_spawned,			// pickup entity1 has been spawned at base entity2
		eRMITypeDoubleEntity_stealth_pickup_spawned,		// pickup entity1 has been spawned at base entity2
		eRMITypeDoubleEntity_pickup_collected,					// player entity1 has picked up pickup entity2
		eRMITypeDoubleEntity_pickup_already_collected,	// player entity1 has already picked up pickup entity2
		eRMITypeDoubleEntity_pickup_dropped,						// player entity1 has dropped pickup entity2
		eRMITypeDoubleEntity_pickup_already_dropped,		// player entity1 has already dropped pickup entity2
		eRMITypeDoubleEntity_pickup_returned_to_base,		// pickup entity1 has been returned to base entity2
		eRMITypeDoubleEntity_pickup_extracted,					// player entity1 has extracted pickup entity2 - This may not work as it will have been deleted by being extracted
		eRMITypeDoubleEntity_fallback_gun_spawned,			// pickup entity1 has had its fallback gun entity2 - spawned for it
	};

	enum EGameStateUpdate
	{
		eGameStateUpdate_None=0,
		eGameStateUpdate_Initialise,
		eGameStateUpdate_TickCollected,
		eGameStateUpdate_TickDropped,
		eGameStateUpdate_TickReturned,
		eGameStateUpdate_TickExtracted
	};

	EGameRulesMissionObjectives m_armourFriendlyIconDropped;
	EGameRulesMissionObjectives m_armourHostileIconDropped;
	EGameRulesMissionObjectives m_stealthFriendlyIconDropped;
	EGameRulesMissionObjectives m_stealthHostileIconDropped;
	EGameRulesMissionObjectives m_armourFriendlyIconCarried;
	EGameRulesMissionObjectives m_armourHostileIconCarried;
	EGameRulesMissionObjectives m_stealthFriendlyIconCarried;
	EGameRulesMissionObjectives m_stealthHostileIconCarried;
	EGameRulesMissionObjectives m_armourFriendlyIconAtBase;
	EGameRulesMissionObjectives m_armourHostileIconAtBase;
	EGameRulesMissionObjectives m_stealthFriendlyIconAtBase;
	EGameRulesMissionObjectives m_stealthHostileIconAtBase;
	EGameRulesMissionObjectives m_friendlyBaseIcon;
	EGameRulesMissionObjectives m_hostileBaseIcon;

	typedef CryFixedStringT<32> TFixedString;
	TFixedString m_textMessagePickUpFriendly;
	TFixedString m_textMessagePickUpHostile;
	TFixedString m_textMessageDroppedFriendly;
	TFixedString m_textMessageDroppedHostile;
	TFixedString m_textMessageReturnedDefender;
	TFixedString m_textMessageReturnedAttacker;
	TFixedString m_textMessageExtractedFriendly;
	TFixedString m_textMessageExtractedHostile;
	TFixedString m_textMessageExtractedFriendlyEffect;
	TFixedString m_textMessageExtractedHostileEffect;
	TFixedString m_textPowerCell;
	TFixedString m_statusAttackersTicksSafe;
	TFixedString m_statusDefendersTicksSafe;
	TFixedString m_statusAttackerCarryingTick;
  TFixedString m_statusDefendersTickCarriedDropped;
	TFixedString m_iconTextDefend;
	TFixedString m_iconTextEscort;
	TFixedString m_iconTextSeize;
	TFixedString m_iconTextKill;
	TFixedString m_iconTextExtraction;
	TFixedString m_hasTaken;
	TFixedString m_hasExtracted;
	TFixedString m_hasDropped;

	TAudioSignalID m_pickupFriendlySignal;
	TAudioSignalID m_pickupHostileSignal;
	TAudioSignalID m_extractedFriendlySignal;
	TAudioSignalID m_extractedHostileSignal;
	TAudioSignalID m_returnedDefenderSignal;	
	TAudioSignalID m_returnedAttackerSignal;
	TAudioSignalID m_droppedFriendlySignal;
	TAudioSignalID m_droppedHostileSignal;

	bool					 m_localPlayerEnergyBoostActive;
	bool					 m_hasFirstFrameInited;

	void HandleDefendersForDroppedPickup(SPickup *pickup, float frameTime);
	void InitPickups(XmlNodeRef xml);
#if DEBUG_EXTRACTION
	void DebugMe();
#endif

	void InitialiseAllHUD();

	float GetTimeLimit();

	void UpdateButtonPresses();
	
	void RemoveAllPickups();
	ERMITypes GetSpawnedPickupRMITypeFromSuitMode(EExtractionSuitMode suitMode);
	void NewFallbackGunForPickup(EntityId pickupEntityId, EntityId fallbackGunEntityId);
	void SpawnAllPickups();

	EntityId FindNewEntityIdToSpawnAt();
	IEntity *SpawnEntity( EntityId spawnAt, IEntityClass *spawnEntityClass, int teamId, const char *name );

	SPickup *GetPickupForPickupEntityId(EntityId inEntityId);
	SPickup *GetPickupForCarrierEntityId(EntityId inEntityId);
	SPickup *GetPickupForSuitMode(EExtractionSuitMode suitMode);
	SExtractAt *GetExtractAtForEntityId(EntityId inEntityId);

	void CallEntityScriptFunction(EntityId entityId, const char *function, bool active);

	int FindCarrierInPickupCarriers(SPickup *pickup, EntityId carrierId);
	bool CarrierAlreadyInPickupCarriers(SPickup *pickup, EntityId carrierId);
	bool AddCarrierToPickup(SPickup *pickup, EntityId carrierId);
	ILINE const char *GetPickupString(SPickup *pickup)
	{
		return m_textPowerCell.c_str();
	}
	void PlayerCollectsPickupCommon(EntityId playerId, SPickup *pickup, bool newPickup);
	void PlayerCollectsPickup(EntityId playerId, SPickup *pickup);
	void PlayerExtractsPickupCommon(EntityId playerId, SPickup *pickup);
	void PlayerExtractsPickup(EntityId playerId, SPickup *pickup, IEntity *extractionPointEntity);
	void PlayerDropsPickupCommon(EntityId playerId, SPickup *pickup, bool newDrop);
	void PlayerDropsPickup(EntityId playerId, SPickup *pickup);
	void PickupReturnsCommon(SPickup *pickup, IEntity *pickupEntity, IEntity *spawnedAtEntity, bool wasExtracted);

	bool CheckForAndDropItem( EntityId playerId );
	void SetIconForExtractionPoint(EntityId extractionPointEntityId);
	void SetIconForPickup(SPickup *pickup);
	void SetIconForAllExtractionPoints();
	void SetIconForAllPickups();
	void UpdateGameStateText(EGameStateUpdate inUpdate, SPickup *inPickup);
	void PhysicalizeEntityForPickup( SPickup *pickup, bool bEnable );
	void PlayerExtractsPickupCommonAnnouncement(const EntityId playerId) const;
	void OnPickupSpawned(EExtractionSuitMode suitMode, EntityId spawnAtEnt, EntityId pickupEnt);

};

#endif // _GAME_RULES_OBJECTIVE_EXTRACTION_H_
