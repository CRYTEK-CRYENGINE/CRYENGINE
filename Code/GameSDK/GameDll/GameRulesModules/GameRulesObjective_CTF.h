// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Objectives module for CTF
	-------------------------------------------------------------------------
	History:
	- 23:02:2011  : Created by Colin Gulliver

*************************************************************************/

#ifndef _GAME_RULES_OBJECTIVE_CTF_H
#define _GAME_RULES_OBJECTIVE_CTF_H

#if _MSC_VER > 1000
# pragma once
#endif

#include "IGameRulesObjectivesModule.h"
#include "IGameRulesModuleRMIListener.h"
#include "IGameRulesClientConnectionListener.h"
#include "IGameRulesKillListener.h"
#include "IGameRulesTeamChangedListener.h"
#include "IGameRulesRoundsListener.h"
#include "IGameRulesActorActionListener.h"
#include <CryEntitySystem/IEntitySystem.h>

class CGameRulesObjective_CTF : public IGameRulesObjectivesModule,
																public IGameRulesModuleRMIListener,
																public IGameRulesClientConnectionListener,
																public IGameRulesKillListener,
																public IGameRulesTeamChangedListener,
																public IGameRulesRoundsListener,
																public IGameRulesActorActionListener,
																public IEntityEventListener
{
public:
	CGameRulesObjective_CTF();
	virtual ~CGameRulesObjective_CTF();

	// IGameRulesObjectivesModule
	virtual void Init(XmlNodeRef xml);
	virtual void Update(float frameTime);

	virtual void OnStartGame();
	virtual void OnStartGamePost() {}
	virtual void OnGameReset() {}

	virtual void PostInitClient(int channelId);
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags ) { return true; }

	virtual bool HasCompleted(int teamId) { return false; }

	virtual void OnHostMigration(bool becomeServer);

	virtual bool AreSuddenDeathConditionsValid() const { return false; }
	virtual void CheckForInteractionWithEntity(EntityId interactId, EntityId playerId, SInteractionInfo &interactionInfo);
	virtual bool CheckIsPlayerEntityUsingObjective(EntityId playerId);

	virtual ESpawnEntityState GetObjectiveEntityState(EntityId entityId);

	virtual int GetAutoAssignTeamId(int channelId) { return 0; }
	virtual void OnEntitySignal(EntityId entityId, int signal) { } 
	// ~IGameRulesObjectivesModule

	// IGameRulesModuleRMIListener
	virtual void OnSingleEntityRMI(CGameRules::SModuleRMIEntityParams params);
	virtual void OnDoubleEntityRMI(CGameRules::SModuleRMITwoEntityParams params);
	virtual void OnEntityWithTimeRMI(CGameRules::SModuleRMIEntityTimeParams params) {}
	virtual void OnSvClientActionRMI(CGameRules::SModuleRMISvClientActionParams params, EntityId fromEid);
	// ~IGameRulesModuleRMIListener

	// IGameRulesClientConnectionListener
	virtual void OnClientConnect(int channelId, bool isReset, EntityId playerId) {}
	virtual void OnClientDisconnect(int channelId, EntityId playerId) { OnPlayerKilledOrLeft(playerId, 0); }
	virtual void OnClientEnteredGame(int channelId, bool isReset, EntityId playerId) {}
	virtual void OnOwnClientEnteredGame();
	// ~IGameRulesClientConnectionListener

	// IGameRulesKillListener
	virtual void OnEntityKilledEarly(const HitInfo &hitInfo) {}
	virtual void OnEntityKilled(const HitInfo &hitInfo) { OnPlayerKilledOrLeft(hitInfo.targetId, hitInfo.shooterId); }
	// ~IGameRulesKillListener

	// IGameRulesTeamChangedListener
	virtual void OnChangedTeam(EntityId entityId, int oldTeamId, int newTeamId);
	// ~IGameRulesTeamChangedListener

	// IGameRulesRoundsListener
	virtual void OnRoundStart() {};
	virtual void OnRoundEnd() {}
	virtual void OnSuddenDeath() {};
	virtual void ClRoundsNetSerializeReadState(int newState, int curState) {};
	virtual void OnRoundAboutToStart();
	// ~IGameRulesRoundsListener

	// IGameRulesActorActionListener
	virtual void OnAction(const ActionId& action, int activationMode, float value);
	// ~IGameRulesActorActionListener

	// IEntityEventListener
	virtual void OnEntityEvent( IEntity *pEntity, const SEntityEvent& event );
	// ~IEntityEventListener

private:
	enum ERMITypes
	{
		// Single entity RMIs
		eRMI_SetupBase,
		eRMI_Pickup,
		eRMI_Drop,
		eRMI_SetupFlag,
		eRMI_SetupDroppedFlag,
		eRMI_ResetAll,
		eRMI_Scored,

		// Double entity RMIs
		eRMI_ResetFlag,
		eRMI_SetupFallbackWeapon,
	};

	struct STeamInfo
	{
		EntityId m_flagId;
		EntityId m_baseId;
		EntityId m_carrierId;
		EntityId m_fallbackWeaponId;

		ESpawnEntityState m_state;

		int m_droppedIconFrame;

		float m_resetTimer;
	};

	typedef CryFixedStringT<32> TFixedString;

	void OnPlayerKilledOrLeft(EntityId targetId, EntityId shooterId);

	void AttachFlagToBase(STeamInfo *pTeamInfo);
	void DetachFlagFromBase(STeamInfo *pTeamInfo);

	STeamInfo *FindInfoByFlagId(EntityId flagId);
	int FindTeamIndexByCarrierId(EntityId playerId) const;
	STeamInfo *FindInfoByCarrierId(EntityId playerId);

	// Pick up records
	bool IsPlayerInPickedUpRecord(EntityId playerId) const; 
	void SetPlayerHasPickedUp(EntityId playerId, const bool bHasPickedUp); 
	void ClearPlayerPickedUpRecords(); 

	// Client to server RMIs
	void Handle_RequestPickup(EntityId playerId);
	void Handle_RequestDrop(EntityId playerId);

	// Server to client RMIs
	void Handle_ResetFlag(EntityId flagId, EntityId playerId);
	void Handle_SetupBase(EntityId baseId);
	void Handle_Pickup(EntityId playerId);
	void Handle_Drop(EntityId playerId);
	void Handle_SetupFlag(EntityId flagId);
	void Handle_SetupDroppedFlag(EntityId flagId);
	void Handle_ResetAll();
	void Handle_SetupFallbackWeapon(EntityId weaponId, EntityId flagId);
	void Handle_Scored(EntityId playerId);

	void Server_ResetAll();
	void Server_Drop(EntityId playerId, STeamInfo *pTeamInfo, EntityId shooterId);
	void Server_ResetFlag(STeamInfo *pTeamInfo, EntityId playerId);
	void Server_SwapSides();
	void Server_CheckForFlagRetrieval();
	void Server_PhysicalizeFlag(EntityId flagId, bool bEnable);

	void Common_Pickup(EntityId playerId);
	void Common_Drop(EntityId playerId);
	void Common_ResetFlag(EntityId flagId, EntityId playerId);
	void Common_Scored(EntityId playerId);
	void Common_ResetAll();

	void Client_RemoveAllIcons();
	void Client_SetIconForBase(int teamIndex);
	void Client_SetIconForFlag(int teamIndex);
	void Client_SetIconForCarrier(int teamIndex, EntityId carrierId, bool bRemoveIcon);
	void Client_SetAllIcons();
	void Client_UpdatePickupAndDrop();

	CryFixedArray<EntityId, MAX_PLAYER_LIMIT> m_playersThatHavePickedUp;

	CAudioSignalPlayer m_baseAlarmSound[2];
	STeamInfo m_teamInfo[2];

	TFixedString m_stringComplete;
	TFixedString m_stringHasTaken;
	TFixedString m_stringHasCaptured;
	TFixedString m_stringHasDropped;
	TFixedString m_stringHasReturned;

	TFixedString m_iconStringDefend;
	TFixedString m_iconStringReturn;
	TFixedString m_iconStringEscort;
	TFixedString m_iconStringSeize;
	TFixedString m_iconStringKill;
	TFixedString m_iconStringBase;

	EGameRulesMissionObjectives m_iconFriendlyFlagCarried;
	EGameRulesMissionObjectives m_iconHostileFlagCarried;
	EGameRulesMissionObjectives m_iconFriendlyFlagDropped;
	EGameRulesMissionObjectives m_iconHostileFlagDropped;
	EGameRulesMissionObjectives m_iconFriendlyBaseWithFlag;
	EGameRulesMissionObjectives m_iconHostileBaseWithFlag;
	EGameRulesMissionObjectives m_iconFriendlyBaseWithNoFlag;
	EGameRulesMissionObjectives m_iconHostileBaseWithNoFlag;
	
	TAudioSignalID m_soundFriendlyComplete;
	TAudioSignalID m_soundHostileComplete;
	TAudioSignalID m_soundFriendlyPickup;
	TAudioSignalID m_soundHostilePickup;
	TAudioSignalID m_soundFriendlyReturn;
	TAudioSignalID m_soundHostileReturn;
	TAudioSignalID m_soundFriendlyDropped;
	TAudioSignalID m_soundHostileDropped;
	TAudioSignalID m_soundBaseAlarmOff;

	CGameRules *m_pGameRules;
	IEntityClass *m_pFallbackWeaponClass;

	int m_moduleRMIIndex;

	float m_resetTimerLength;
	float m_retrieveFlagDistSqr;
	float m_pickUpVisCheckHeight;

	bool m_bUseButtonHeld;
};

#endif // _GAME_RULES_OBJECTIVE_CTF_H

