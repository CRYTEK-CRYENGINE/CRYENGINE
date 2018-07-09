// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Interface for the game rule module to handle objectives
	-------------------------------------------------------------------------
	History:
	- 21:09:2009  : Created by Colin Gulliver

*************************************************************************/

#ifndef _IGAME_RULES_OBJECTIVES_MODULE_H_
#define _IGAME_RULES_OBJECTIVES_MODULE_H_

#if _MSC_VER > 1000
# pragma once
#endif

#include <IGameRulesSystem.h>
#include "GameRulesTypes.h"
#include <CryNetwork/SerializeFwd.h>
#include "ItemDefinitions.h"

struct SInteractionInfo;

class IGameRulesObjectivesModule
{
public:
	virtual ~IGameRulesObjectivesModule() {}

	virtual void Init(XmlNodeRef xml) = 0;
	virtual void Update(float frameTime) = 0;

	virtual void OnStartGame() = 0;
	virtual void OnStartGamePost() = 0;
	virtual void OnGameReset() = 0;

	virtual void PostInitClient(int channelId) = 0;
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags ) = 0;

	virtual bool HasCompleted(int teamId) = 0;

	virtual void OnHostMigration(bool becomeServer) = 0;		// Host migration

	virtual bool AreSuddenDeathConditionsValid() const = 0;
	virtual void CheckForInteractionWithEntity(EntityId interactId, EntityId playerId, SInteractionInfo& interactionInfo) = 0;
	virtual bool CheckIsPlayerEntityUsingObjective(EntityId playerId) = 0;

	virtual ESpawnEntityState GetObjectiveEntityState(EntityId entityId) = 0;

	virtual int GetAutoAssignTeamId(int channelId) = 0;	// Only called if neither lobby or teams module assign a team

	virtual void OnEntitySignal(EntityId entityId, int signal) = 0; 

	virtual bool CanPlayerRevive(EntityId playerId) const { return true; }
	virtual float GetPlayerAutoReviveAdditionalTime(IActor* pActor) const { return 0.f; }
	virtual bool RequestReviveOnLoadoutChange() { return true; }
	virtual bool CanPlayerRegenerate(EntityId playerId) const {return true;}
	virtual bool MustShowHealthEffect(EntityId playerId) const {return true;}
	virtual bool IndividualScore() const {return false;}
	virtual bool ShowRoundsAsDraw() const {return false;}
	virtual bool IsWinningKill(const HitInfo &hitInfo) const { return false; }

	virtual void UpdateInitialAmmoCounts(const IEntityClass *pWeaponClass, TAmmoVector &ammo, TAmmoVector &bonusAmmo) {}
	virtual void OnNewGroupIdSelected(uint32 id) {}
	virtual void OnPrematchStateEnded() {}

	// Optional Intro sequence
	virtual bool CanPlayIntroSequence() const {return false;}
	virtual bool SquadChallengesEnabled() const {return true;}
};

#endif // _IGAME_RULES_OBJECTIVES_MODULE_H_
