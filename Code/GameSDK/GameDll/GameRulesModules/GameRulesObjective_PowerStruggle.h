// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Implementation of power struggle objectives
	-------------------------------------------------------------------------
	History:
	- 21:03:2011  : Created by Colin Gulliver

*************************************************************************/

#ifndef _GAME_RULES_OBJECTIVE_POWER_STRUGGLE_H_
#define _GAME_RULES_OBJECTIVE_POWER_STRUGGLE_H_

#if _MSC_VER > 1000
# pragma once
#endif

#include "IGameRulesSystem.h"
#include "GameRulesModules/IGameRulesObjectivesModule.h"
#include "GameRulesModules/IGameRulesModuleRMIListener.h"
#include "GameRulesModules/IGameRulesKillListener.h"
#include "GameRulesModules/IGameRulesClientConnectionListener.h"
#include <CryCore/Containers/CryFixedArray.h>

#include <CryEntitySystem/IEntitySystem.h>

struct IActor;

class CGameRulesObjective_PowerStruggle :	public IGameRulesObjectivesModule,
																					public IEntityEventListener,
																					public IGameRulesKillListener,
																					public IGameRulesClientConnectionListener
{
public:
	CGameRulesObjective_PowerStruggle();
	~CGameRulesObjective_PowerStruggle();

	// IGameRulesObjectivesModule
	virtual void Init(XmlNodeRef xml);
	virtual void Update(float frameTime);

	virtual void OnStartGame();
	virtual void OnStartGamePost() {}
	virtual void OnGameReset() {}

	virtual void PostInitClient(int channelId);
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );

	virtual bool HasCompleted(int teamId) { return false; }

	virtual void OnHostMigration(bool becomeServer) {}

	virtual bool AreSuddenDeathConditionsValid() const { return false; }
	virtual void CheckForInteractionWithEntity(EntityId interactId, EntityId playerId, SInteractionInfo &interactionInfo) {}
	virtual bool CheckIsPlayerEntityUsingObjective(EntityId playerId) { return false; }

	virtual ESpawnEntityState GetObjectiveEntityState(EntityId entityId) { return eSES_Unknown; }

	virtual int GetAutoAssignTeamId(int channelId) { return 0; }
	virtual void OnEntitySignal(EntityId entityId, int signal);
	// ~IGameRulesObjectivesModule

	// IEntityEventListener
	virtual void OnEntityEvent( IEntity *pEntity, const SEntityEvent& event );
	// ~IEntityEventListener

	// IGameRulesKillListener
	virtual void OnEntityKilledEarly(const HitInfo &hitInfo) {};
	virtual void OnEntityKilled(const HitInfo &hitInfo);
	// ~IGameRulesKillListener

	// IGameRulesClientConnectionListener
	virtual void OnClientConnect(int channelId, bool isReset, EntityId playerId) {}
	virtual void OnClientDisconnect(int channelId, EntityId playerId);
	virtual void OnClientEnteredGame(int channelId, bool isReset, EntityId playerId) {}
	virtual void OnOwnClientEnteredGame();
	// ~IGameRulesClientConnectionListener

	bool HasSoloCapturedSpear( EntityId playerId );

	// for signaling from lua
	enum EEntitySignal
	{
		eEntitySignal_node_buried=0,
		eEntitySignal_node_emerged,
	};

	enum ENodeHUDState	// values map to frames in flash
	{
		eNHS_none=0,
		eNHS_neutral,											// 1
		eNHS_enemyCaptureFromNeutral,			// 2
		eNHS_friendlyCaptureFromNeutral,	// 3
		eNHS_enemyCaptureFromFriendly,		// 4
		eNHS_friendlyCaptureFromEnemy,		// 5
		eNHS_enemyOwned,									// 6
		eNHS_friendlyOwned,								// 7
		eNHS_contention,									// 8
		eNHS_fadeOut,											// 9
	};

	enum EActiveIdentityId
	{
		eAII_unknown_id=0,
		eAII_spearA_id,
		eAII_spearB_id,
		eAII_spearC_id,
		eAII_num_ids,
	};

private:
	static const int MAX_NODES = 3;
	static const int MAX_PLAYERS_PER_TEAM  = (MAX_PLAYER_LIMIT / 2);
	static const int NUM_TEAMS = 2;
	static const int CONTESTED_TEAM_ID = -1;

	typedef CryFixedArray<EntityId, MAX_PLAYERS_PER_TEAM> TPlayerIdArray;
	typedef CryFixedStringT<32> TFixedString;

	struct SNodeIdentityInfo
	{
		SNodeIdentityInfo(EActiveIdentityId _eIdentity, const char * _pIconName) { eIdentity = _eIdentity; pIconName = _pIconName; }
		EActiveIdentityId eIdentity;
		const char				* pIconName;
	};

	static SNodeIdentityInfo s_nodeIdentityInfo[MAX_NODES + 1];

	enum ENodeState
	{
		ENS_uninitialised=0,
		ENS_hidden,
		ENS_emerging,
		ENS_neutral,
		ENS_capturing_from_neutral,
		ENS_captured,											// lastCaptureTeamId specifies the holding team
		ENS_capturing_from_capture				// one team is trying to capture from another team who already captured this		
	};

	enum ESoloCaptureState
	{
		ESolo_Available,	//spear has reset to an uncontested state, possible to solo cap it 
		ESolo_Activated,	//local player has moved into zone to cap spear, if it gets capped they can get awarded
		ESolo_Denied,			//non local player is helping cap spear, award not available
	};

	struct SNodeInfo
	{
		SNodeInfo()
			: m_id(0) 
			, m_controllingTeamId(0) 
			, m_lastControllingTeamId(0)
			, m_lastCapturedTeamId(0)
			, m_captureStatusOwningTeam(0)
			, m_activeIdentityId(eAII_unknown_id)
			, m_captureStatus(0.0f)
			, m_controlRadius(0.0f)
			, m_controlHeight(0.0f)
			, m_controlZOffset(0.0f)
			, m_currentCapturingAudioParam(0.0f)
			, m_currentContendedAudioParam(0.0f)
			, m_currentCompletedAudioParam(0.0f)
			, m_cameraShakeId(0)
			, m_havePlayedSignal(false)
			, m_clientPlayerPresentAtCaptureTime(false)
			, m_currentObjectiveIconType(EGRMO_Unknown)
			, m_currentHUDState(eNHS_none)
			, m_soloCaptureState(ESolo_Available)
			, m_state(ENS_uninitialised)
		{
			m_insideCount[0] = 0;
			m_insideCount[1] = 0;
		}

		void SetState(ENodeState inState);
	
		CAudioSignalPlayer m_signalPlayer;
		EntityId m_id;
		int m_controllingTeamId;		// describes the current state of this node. neutral, team1, team2, contested
		int m_lastControllingTeamId;	// describes the last set current state of this node. is consulted when in contention to see who the last controllingTeam was
		int m_insideCount[NUM_TEAMS];
		EActiveIdentityId m_activeIdentityId;		
		float m_captureStatus;	// 0 = Neutral, -1 = owned by team 1, 1 = owned by team 2
		float m_controlRadius;
		float m_controlHeight;
		float m_controlZOffset;
		float m_currentCapturingAudioParam;
		float m_currentContendedAudioParam;
		float m_currentCompletedAudioParam;
		Vec3 m_position;	// TODO - perhaps remove this and just get the pos from the resolved entity. Now they're going to be moving
		TPlayerIdArray m_insideBoxEntities[NUM_TEAMS];
		TPlayerIdArray m_insideEntities[NUM_TEAMS];
		int m_cameraShakeId;
		uint8 m_lastCapturedTeamId;		// 0: never captured; 1,2 for team that last captured it
		uint8 m_captureStatusOwningTeam; // 0: never captured; 1,2 for the team who's owning captureStatus
		bool m_havePlayedSignal;
		bool m_clientPlayerPresentAtCaptureTime;
		EGameRulesMissionObjectives m_currentObjectiveIconType;
		ENodeHUDState m_currentHUDState;
		ENodeState m_state;
		ESoloCaptureState m_soloCaptureState;
		EntityId m_soloCapturerId;
	};

	struct SChargeeInfo
	{
		SChargeeInfo() : m_TeamId(0), m_ChargeValue(0.f), m_PrevChargeValue(0.f) {}

		EntityId m_TeamId;
		float m_ChargeValue;
		float m_PrevChargeValue;
	};

	bool NetSerializeNode(SNodeInfo *pNodeInfo, TSerialize ser, EEntityAspects aspect, uint8 profile, int flags);
	void NodeCaptureStatusChanged(SNodeInfo *pNodeInfo);
	void Common_NewlyContestedSpear(SNodeInfo *pNodeInfo);

	void Client_SetIconForNode(SNodeInfo *pNodeInfo);

	void Client_SetAllIcons(const float frameTime);

	void Common_AddNode(IEntity *pNode, int forceIndex);
	void Common_RemovePlayerFromNode(EntityId playerId, SNodeInfo *pNodeInfo);
	void Common_OnPlayerKilledOrLeft(EntityId targetId, EntityId shooterId);
	void Common_CheckNotifyStringForChargee( SChargeeInfo* pChargeeInfo, const float inPrevChargeValue, const float inChargeValue );

	float SmoothlyUpdateValue(float currentVal, float desiredVal, float updateRate, float frameTime);
	void Client_UpdateSoundSignalForNode(SNodeInfo *pNodeInfo, float frameTime);
	void Common_HandleSpearCaptureStarting(SNodeInfo *pNodeInfo);
	void Common_HandleCapturedSpear(SNodeInfo *pNodeInfo);
	void Common_UpdateActiveNode(SNodeInfo *pNodeInfo, float frameTime, bool &bAspectChanged);
	void Server_ActivateNode(int index);
	void Server_InitNode(SNodeInfo *pNodeInfo);
	void Server_ActivateAllNodes();
	void Common_UpdateNode( SNodeInfo *pNodeInfo, float frameTime, bool &bAspectChanged);
	void Server_UpdateScoring(float frameTime, bool &bAspectChanged);
	void Common_UpdateDebug(float frameTime);

	// Calculates + Returns the team number of the winning team
	int Server_CalculateWinningTeam(); 

	void Common_SortNodes();
	static bool CanActorCaptureNodes(IActor * pActor);

	void Client_ShakeCameraForSpear(const SNodeInfo *pNodeInfo);

	// Capture progress
	bool Client_UpdateCaptureProgressBar(const SNodeInfo *pNodeInfo ); 
	void Client_CheckCapturedAllPoints();

	// Additional players at a node will increase the capture rate
	float CalculateCapturePointQuantity(const SNodeInfo *pNodeInfo, float frameTime);
	SNodeInfo *Common_FindNodeFromEntityId(EntityId entityId);

	void LogSpearStateToTelemetry( uint8 state, SNodeInfo* pSpearInfo );

	// TODO - move these arrays into CryFixedArray for added range safety
	SNodeInfo m_nodes[MAX_NODES];
	SChargeeInfo m_Chargees[NUM_TEAMS];

	CryFixedArray< EntityId, MAX_PLAYER_LIMIT > m_soloCapturingPlayers;

	CGameRules *m_pGameRules;

	float m_scoreTimerLength;
	float m_timeTillScore;
	float m_timeTillManageActiveNodes;
	float m_invCaptureIncrementMultiplier;
	float m_captureDecayMultiplier;
	float m_chargeSpeed;
	float m_captureTimePercentageReduction; 
	float m_avoidSpawnPOIDistance;					// how far away a spawnpoint has to be from POI (nodes) to be able to spawn
	float m_manageActiveSpearsTimer;	
	bool m_haveDisplayedInitialSpearsEmergingMsg;
	bool m_haveDisplayedProgressBar; 	// When capturing.. we display a 2D progress bar
	bool m_setupHasHadNodeWithNoLetterSet;
	bool m_setupIsSpecifyingNodeLetters;
	int m_spearCameraShakeIdToUse;

	EGameRulesMissionObjectives m_iconNeutralNode;
	EGameRulesMissionObjectives m_iconFriendlyNode;
	EGameRulesMissionObjectives m_iconHostileNode;
	EGameRulesMissionObjectives m_iconFriendlyCapturingNode;
	EGameRulesMissionObjectives m_iconHostileCapturingNode;
	EGameRulesMissionObjectives m_iconFriendlyCapturingNodeFromHostile;
	EGameRulesMissionObjectives m_iconHostileCapturingNodeFromFriendly;
	EGameRulesMissionObjectives m_iconContentionOverNode;

	TAudioSignalID m_captureSignalId;
	TAudioSignalID m_spear_going_up;
	TAudioSignalID m_positiveSignalId;
	TAudioSignalID m_negativeSignalId;

	TFixedString m_stringFriendlyCaptured;
	TFixedString m_stringHostileCaptured;
	TFixedString m_stringEnemyCapturingASpear;
	TFixedString m_stringFriendlyCapturingASpear;
	TFixedString m_stringWeHaveNoSpears;
	TFixedString m_stringWeHave1Spear;
	TFixedString m_stringWeHaveNSpears;
	TFixedString m_stringNewSpearsEmerging;
	TFixedString m_stringFriendlyCharged20;
	TFixedString m_stringFriendlyCharged40;
	TFixedString m_stringFriendlyCharged60;
	TFixedString m_stringFriendlyCharged80;
	TFixedString m_stringFriendlyCharged90;
	TFixedString m_stringEnemyCharged20;
	TFixedString m_stringEnemyCharged40;
	TFixedString m_stringEnemyCharged60;
	TFixedString m_stringEnemyCharged80;
	TFixedString m_stringEnemyCharged90;
	TFixedString m_stringCaptureTheSpears;
	TFixedString m_stringCaptured;
	TFixedString m_stringCapturing;
};

#endif // _GAME_RULES_POWER_STRUGGLE_OBJECTIVE_H_
 
