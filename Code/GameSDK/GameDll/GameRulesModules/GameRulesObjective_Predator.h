// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Objectives for the predator game mode
	-------------------------------------------------------------------------
	History:
	- 09:05:2011  : Created by Colin Gulliver

*************************************************************************/

#ifndef _GAME_RULES_OBJECTIVE_PREDATOR_H_
#define _GAME_RULES_OBJECTIVE_PREDATOR_H_

#if _MSC_VER > 1000
# pragma once
#endif

#include "IGameRulesObjectivesModule.h"
#include "GameRulesModules/IGameRulesKillListener.h"
#include "GameRulesModules/IGameRulesModuleRMIListener.h"
#include "GameRulesModules/IGameRulesActorActionListener.h"
#include "GameRulesModules/IGameRulesClientConnectionListener.h"
#include "GameRulesModules/IGameRulesTeamChangedListener.h"
#include "GameRulesModules/IGameRulesRevivedListener.h"
#include "GameRulesModules/IGameRulesRoundsListener.h"
#include "MultiplayerEntities/NetworkedPhysicsEntity.h"
#include "Audio/AudioSignalPlayer.h"

class CGameRules;

class CGameRulesObjective_Predator : public IGameRulesObjectivesModule,
																			public IGameRulesKillListener,
																			public IGameRulesModuleRMIListener,
																			public IGameRulesClientConnectionListener,
																			public IGameRulesTeamChangedListener,
																			public IGameRulesRevivedListener,
																			public IGameRulesRoundsListener
{
public:
	CGameRulesObjective_Predator();
	virtual ~CGameRulesObjective_Predator();

	ILINE float GetMinimalReviveTime() const {return m_fMinimalReviveTime;}

	// IGameRulesObjectivesModule
	virtual void Init(XmlNodeRef xml);
	virtual void Update(float fFrameTime);

	virtual void OnStartGame();
	virtual void OnStartGamePost();
	virtual void OnGameReset();

	virtual void PostInitClient(int channelId);
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );

	virtual bool HasCompleted(int teamId);

	virtual void OnHostMigration(bool becomeServer);

	virtual bool AreSuddenDeathConditionsValid() const { return false; }
	virtual void CheckForInteractionWithEntity(EntityId interactId, EntityId playerId, SInteractionInfo &interactionInfo) {}
	virtual bool CheckIsPlayerEntityUsingObjective(EntityId playerId) { return false; }

	virtual ESpawnEntityState GetObjectiveEntityState(EntityId entityId) { return eSES_Unknown; }

	virtual int GetAutoAssignTeamId(int channelId);
	virtual void OnEntitySignal(EntityId entityId, int signal) {}

	virtual bool CanPlayerRevive(EntityId playerId) const;
	virtual bool CanPlayerRegenerate(EntityId playerId) const;
	virtual float GetPlayerAutoReviveAdditionalTime(IActor* pActor) const;
	virtual bool RequestReviveOnLoadoutChange();
	virtual bool MustShowHealthEffect(EntityId playerId) const;
	virtual bool IndividualScore() const {return true;}
	virtual bool ShowRoundsAsDraw() const {return true;}
	virtual bool IsWinningKill(const HitInfo &hitInfo) const;

	virtual void UpdateInitialAmmoCounts(const IEntityClass *pWeaponClass, TAmmoVector &ammo, TAmmoVector &bonusAmmo);
	virtual void OnNewGroupIdSelected(uint32 id);
	virtual void OnPrematchStateEnded();
	virtual bool CanPlayIntroSequence() const;
	virtual bool SquadChallengesEnabled() const {return false;}
	// ~IGameRulesObjectivesModule

	// IGameRulesKillListener
	virtual void OnEntityKilledEarly(const HitInfo &hitInfo) {}
	virtual void OnEntityKilled(const HitInfo &hitInfo);
	// ~IGameRulesKillListener

	// IGameRulesModuleRMIListener 
	virtual void OnSingleEntityRMI(CGameRules::SModuleRMIEntityParams params);
	virtual void OnDoubleEntityRMI(CGameRules::SModuleRMITwoEntityParams params) {}
	virtual void OnEntityWithTimeRMI(CGameRules::SModuleRMIEntityTimeParams params) {}
	virtual void OnSvClientActionRMI(CGameRules::SModuleRMISvClientActionParams params, EntityId fromEid) {}
	// ~IGameRulesModuleRMIListener

	// IGameRulesClientConnectionListener
	virtual void OnClientConnect(int channelId, bool isReset, EntityId playerId) {}
	virtual void OnClientDisconnect(int channelId, EntityId playerId);

	bool EndGameIfOnlyOnePlayerRemains( int channelId );

	virtual void OnClientEnteredGame(int channelId, bool isReset, EntityId playerId) {}
	virtual void OnOwnClientEnteredGame() {}
	// ~IGameRulesClientConnectionListener

	// IGameRulesTeamChangedListener
	virtual void OnChangedTeam(EntityId entityId, int oldTeamId, int newTeamId);
	// ~IGameRulesTeamChangedListener

	// IGameRulesRevivedListener
	virtual void EntityRevived(EntityId entityId);
	// ~IGameRulesRevivedListener

	// IGameRulesRoundsListener
	virtual void OnRoundStart();
	virtual void OnRoundEnd();
	virtual void OnSuddenDeath() {}
	virtual void ClRoundsNetSerializeReadState(int newState, int curState) {}
	virtual void OnRoundAboutToStart();
	// ~IGameRulesRoundsListener

	static const int TEAM_SOLDIER = 1;
	static const int TEAM_PREDATOR = 2;

	bool IsNextClientBowFirstAsHunter() const;
	void FirstBowAsHunterSelected()						{ Common_ClearFlag(ePF_NextClientBowFirstAsHunter);					}

private:
	typedef CryFixedArray<int, MAX_PLAYER_LIMIT> TIntArray;
	typedef CryFixedStringT<32> TFixedString;
	typedef std::vector<CGameRules::SModuleRMIEntityParams> TRMIParamsVec;
	typedef std::vector<int> TIntList;
	typedef uint16 TPredatorFlags;

	enum EPredatorFlags
	{
		ePF_ClientSwitchingToPredator		= BIT(0),
		ePF_ClientSuicide								= BIT(1),
		ePF_ClientStartedAsPredator			= BIT(2),
		ePF_GracePeriodEventSent				= BIT(3),
		ePF_AllSoldiersDead							= BIT(4),
		ePF_HasStartedAsMarine					= BIT(5),
		ePF_Sent90SecondsRemainingEvent = BIT(6),
		ePF_Sent60SecondsRemainingEvent = BIT(7),
		ePF_Sent30SecondsRemainingEvent = BIT(8),
		ePF_Sent10SecondsRemainingEvent = BIT(9),
		ePF_SentHunterSpawn							= BIT(10),
		ePF_HasSpawned									= BIT(11),
		ePF_NextClientBowFirstAsHunter	= BIT(12)
	};

	void Server_GiveSoldierSurvivingPoints(bool survivedToEnd);
	void Server_LivingSoldiersScore(EGRST scoreType, TGameRulesScoreInt points);
	void Server_UpdateRemainingSoldierCount(int deltaCount);
	void Server_OnSoldierKilledOrLeft(EntityId soldierId);
	void Server_OnStartNewRound();
	void Server_GetMarinesStatus(EntityId excludeId, EntityId &outLastMarineId, int &outCount) const;
	bool Server_WasFinalKill(EntityId playerId) const;

	int  Common_GetMaxStartPredators(int numPlayers) const;
	void Common_OnPlayerKilledOrLeft(EntityId targetId, EntityId shooterId);
	void Common_SetAmmoCacheGroupActive(uint32 groupId, bool bActive);
	void Server_OnPredatorLeft();
	void Server_HandleNoPredatorsLeft();

	void AssignTeamsFromLobby();

	ILINE bool Common_IsFlagSet(TPredatorFlags flag) const	{ return ((m_flags & flag) == flag); }
	ILINE void Common_SetFlag(TPredatorFlags flag)					{ m_flags |= flag; }
	ILINE void Common_ClearFlag(TPredatorFlags flag)				{ m_flags &= ~flag; }

	void Client_OnSoldiersRemainingCountChanged(int previousCount);
	void Client_AnnounceTimeRemaining(EPredatorFlags flag, const char *pMarineEvent, const char *pHunterEvent);

	//Team Selection
	struct SPredatorData
	{
		SPredatorData()
		{
			bHasStartedAsPredator = false;
			bIsPredator = false;
			bStartedThisRoundAsPredator = false;
			fReviveTimeMult = 0.f;
		}

		float fReviveTimeMult;
		bool bHasStartedAsPredator;
		bool bIsPredator;
		bool bStartedThisRoundAsPredator;
	};

	struct SSurvivalPointsData
	{
		uint8 m_soldierCount;
		uint8 m_score;
	};
	typedef CryFixedArray<SSurvivalPointsData, 8> TSurvivalPointsArr;

	typedef std::map< int, SPredatorData > TPredatorData;
	TPredatorData m_PredatorData;
	//~Team Selection

	//Team switching
	// Done in two stages, beginning post death/killcam. First stage is changing team. Second stage is selecting loadout.
	void Server_PrepareChangeTeamPredator(EntityId playerId, bool wasSuicide);
	void Server_PredatorReadyToSpawn(EntityId playerId); //Dead "soldier" (Technically on team predator now) clients informing the server they have finished watching any necessary cameras
	void Common_PrepareChangeTeamPredator(EntityId playerId, bool wasFinalMarine);
	void Client_SetPredatorLoadout();
	//~Team switching

	struct SAmmoOverride
	{
		IEntityClass *m_pAmmoClass;
		int m_clips;
	};
	typedef std::vector<SAmmoOverride> TAmmoOverrideVec;

	struct SWeaponAmmoOverride
	{
		TAmmoOverrideVec m_bonusAmmo;
		IEntityClass *m_pWeaponClass;
	};

	typedef std::vector<SWeaponAmmoOverride> TWeaponAmmoOverrideVec;

	typedef CryFixedArray<EntityId, 8> TEntityIdArr;
	struct SAmmoCacheGroup
	{
		TEntityIdArr m_entityIds;
		uint32 m_groupId;
		uint32 m_usageCount;
	};
	typedef CryFixedArray<SAmmoCacheGroup, 8> TAmmoCacheGroupArr;

	TIntArray m_deadSoldierIds; //Soldiers who have died and have not yet re-spawned a a predator

	TWeaponAmmoOverrideVec m_weaponAmmoOverrides;

	TSurvivalPointsArr m_survivalPoints;

	CGameRules *m_pGameRules;

	int m_moduleRMIIndex;
	int m_numStartPredators;
	int m_clientTeam;
	int m_numRemainingSoldiers;
	int m_SoldierSurvivalScorePeriod;

	TAmmoCacheGroupArr m_ammoCacheGroups;
	uint32 m_ammoCacheGroupId;

	float m_fMinimalReviveTime;
	float m_fSoldierSuicideRespawnTimeMult;
	float m_previousEnemyTimeUntilLockObtained;
	float m_fLateJoiningGracePeriod;
	float m_fDelayedAnnouncementTimer;

	CAudioSignalPlayer m_signalPlayer;
	TAudioSignalID m_lastManSignalIdMarine;
	TAudioSignalID m_lastManSignalIdHunter;

	const char *m_pDelayedAnnouncement;

	bool m_ClientHasDiedAsMarine;
	bool m_AMarineHasBeenKilledByRemotePlayerThisRound;

	TPredatorFlags m_flags;		// EPredatorFlags
};

#endif // _GAME_RULES_OBJECTIVE_PREDATOR_H_
