// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 

-------------------------------------------------------------------------
History:
- 04:09:2009 : Created by James Bamford

*************************************************************************/

#ifndef __GAMERULESMPSPAWNING_H__
#define __GAMERULESMPSPAWNING_H__

#include "GameRulesModules/GameRulesSpawningBase.h"
#include "GameRulesModules/IGameRulesRevivedListener.h"
#include "GameRulesModules/IGameRulesRoundsListener.h"
#include "GameRulesModules/IGameRulesKillListener.h"
#include <CryCore/Containers/CryFixedArray.h>
#include <CryInput/IInput.h>
#include "SpawningVisTable.h"

#if !defined(_RELEASE)
#define DEBUG_SPAWNING_VISTABLE (0)
#else
#define DEBUG_SPAWNING_VISTABLE (0)
#endif

#if DEBUG_SPAWNING_VISTABLE
#define MONITOR_BAD_SPAWNS 1
#else
#define MONITOR_BAD_SPAWNS 0
#endif

#include "GameRules.h"	// only needed for CryLog below

#define ETSTList(f) \
	f(eTST_None)						/*0*/ \
	f(eTST_Standard)				/*1*/ \
	f(eTST_RoundSwap)				/*2*/ \
	f(eTST_Initial)					/*3*/ \


AUTOENUM_BUILDENUMWITHTYPE_WITHNUM(ETeamSpawnsType, ETSTList, eTST_NUM);

//These are used for the RS spawning, but are here for convenience for debugging the spawn scoring
const float fScoreForSpawnerBeingVisible = 800.0f;
const float fScoreForPOICollision = 2000.0f;

class CGameRulesMPSpawningBase :	public CGameRulesSpawningBase,
																	public IGameRulesRevivedListener,
																	public IGameRulesRoundsListener,
																	public IGameRulesKillListener
#if MONITOR_BAD_SPAWNS
																	,	public IInputEventListener
#endif
{
private:
	typedef CGameRulesSpawningBase inherited;

protected:
	bool m_teamGame;
	ETeamSpawnsType m_teamSpawnsType;
	EntityId m_currentBestSpawnId;
	float m_fTimeLocalActorDead;
	float m_fPrecacheTimer;
	float m_fInitialAutoSpawnTimer;
	float m_fTimeTillInitialAutoSpawn;
	EntityId m_lastFoundSpawnId;
	EntityId m_cachedRandomSpawnId;
	float m_fCachedRandomSpawnTimeOut;
	int  m_svKillsSum;
	bool m_bPrecacheContinuousBestSpawns;
	bool m_bEquipmentScreenShown;
	bool m_bNeedToStartAutoReviveTimer;
	bool m_allowMidRoundJoining;
	bool m_allowMidRoundJoiningBeforeFirstDeath;	// All or Nothing - if set then code in the spawning with lives will handle the first death case - for spectator cam reasons we don't want to just have allowMidRoundJoining set for these modes


	typedef std::map<int, float> TTeamMinEnemyDistMap;
	TTeamMinEnemyDistMap m_minEnemyDistanceMap;

	struct SSpawnHistory
	{
		SSpawnHistory(EntityId _spawnId, float _fGameTime, int _nTeamSpawned)
		{
			spawnId				= _spawnId;
			fGameTime			= _fGameTime;
			nTeamSpawned	= _nTeamSpawned;
		}

		EntityId	spawnId;
		float			fGameTime;
		int				nTeamSpawned;
	};

	std::vector<SSpawnHistory> m_spawnHistory;

	CSpawningVisTable		m_spawningVisTable;

#if MONITOR_BAD_SPAWNS
	struct SScoringResults
	{
		SScoringResults(float _scoreFromEnemies, float _scoreFromFriendlies, float _scoreFromIdeal, bool _inLOS, bool _inExemptArea)
			: scoreFromEnemies(_scoreFromEnemies)
			, scoreFromFriendlies(_scoreFromFriendlies)
			, scoreFromIdeal(_scoreFromIdeal)
			, inLOS(_inLOS)
			, inExemptArea(_inExemptArea)
		{
		}

		SScoringResults(float _scoreFromEnemies, bool _inLOS, bool _inExemptArea)
			: scoreFromEnemies(_scoreFromEnemies)
			, scoreFromFriendlies(0.0f)
			, scoreFromIdeal(0.0f)
			, inLOS(_inLOS)
			, inExemptArea(_inExemptArea)
		{
		}

		float scoreFromEnemies;
		float scoreFromFriendlies;
		float scoreFromIdeal;
		bool	inLOS;
		bool	inExemptArea;
	};

	struct SCachedSpawnData
	{
		EntityId	selectedSpawn;
		int				iSelectedSpawnIndex;
		std::vector<SScoringResults> scoringResults;
		uint32		visibleState[MAX_PLAYER_LIMIT];

	} m_DBG_spawnData;
	float m_fLastSpawnCheckTime;
#endif

	enum EPointOfInterestState
	{
		EPOIS_NONE=0,
		EPOIS_AVOID,		// maintain a minimum distance away from POI when spawning
		EPOIS_ATTRACT,	// maintain a maximum distance away from POI when spawning
	};

	enum EPointOfInterestFlags
	{
		EPOIFL_NONE							= 0,
		EPOIFL_ENABLED					= (1<<0),
		EPOIFL_USESTATICPOS			= (1<<1)
	};

	struct SPointOfInterest
	{
		EntityId	m_entityId;
		Vec3			m_posn;
		uint32		m_flags;		// maybe be state instead
		uint8			m_state;

		union UStateData
		{
			struct 
			{
				float avoidDistance;
			}	avoid;
			struct 
			{
				float attractDistance;
			} attract;
		};

		UStateData m_stateData;

		SPointOfInterest()
		{
			memset(this, 0, sizeof(*this));
		}

		void SetToAvoidEntity(EntityId entityId, float avoidDistance, bool enabled=true, bool bUseStaticPos=false)
		{
			m_entityId=entityId;
			m_state=EPOIS_AVOID;
			m_stateData.avoid.avoidDistance=avoidDistance;
			if(bUseStaticPos)
			{				
				IEntity * pEntity = gEnv->pEntitySystem->GetEntity(entityId);
				if(pEntity)
				{
					m_posn = pEntity->GetWorldPos();
					m_flags |= EPOIFL_USESTATICPOS;
				}				
				else
				{
					m_flags &= ~EPOIFL_USESTATICPOS;
				}
			}
			else
			{
				m_flags &= ~EPOIFL_USESTATICPOS;
			}

			if (enabled)
			{
				Enable();
			}
			else
			{
				Disable();
			}
		}

		void SetToAttractEntity(EntityId entityId, float attractDistance, bool enabled)
		{
			m_entityId=entityId;
			m_state=EPOIS_ATTRACT;
			m_stateData.attract.attractDistance=attractDistance;
			if (enabled)
			{
				Enable();
			}
			else
			{
				Disable();
			}
		}

		void Enable()
		{
			m_flags |= EPOIFL_ENABLED;
		}

		void Disable()
		{
			m_flags &= ~EPOIFL_ENABLED;
		}
	};

	typedef std::vector<SPointOfInterest>								TPointsOfInterest;
	TPointsOfInterest m_pointsOfInterest;

	CryFixedArray<uint32, MAX_PLAYER_LIMIT> m_initialChannelIDs;
	float m_autoReviveTimeScaleTeam1;
	float m_autoReviveTimeScaleTeam2;

public:
	CGameRulesMPSpawningBase();
	virtual ~CGameRulesMPSpawningBase();

#if MONITOR_BAD_SPAWNS
	virtual bool OnInputEvent( const SInputEvent &event );
#endif

	// IGameRulesSpawningModule
	virtual void Init(XmlNodeRef xml);
	virtual void Update(float frameTime);
	virtual void OnStartGame();

	virtual void AddSpawnLocation(EntityId location, bool isInitialSpawn, bool doVisTest, const char *pGroupName);
	virtual void RemoveSpawnLocation(EntityId id, bool isInitialSpawn);
	virtual void PlayerJoined(EntityId playerId);
	virtual void PlayerLeft(EntityId playerId);

	virtual void AddAvoidPOI(EntityId entityId, float avoidDistance, bool enabled, bool bStaticPOI);
	virtual void RemovePOI(EntityId entityId);
	virtual void EnablePOI(EntityId entityId);
	virtual void DisablePOI(EntityId entityId);

	virtual void ClRequestRevive(EntityId playerId);
	virtual bool SvRequestRevive(EntityId playerId, EntityId preferredSpawnId);
	virtual void PerformRevive(EntityId playerId, int teamId, EntityId preferredSpawnId);
	virtual void OnSetTeam(EntityId playerId, int teamId);

	virtual EntityId GetSpawnLocation(EntityId playerId);

	virtual int GetSpawnLocationTeam(EntityId spawnLocEid) const;

	virtual void ReviveAllPlayers(bool isReset, bool bOnlyIfDead);
	virtual void ReviveAllPlayersOnTeam(int teamId);
	
	void ReviveAllPlayers_NoTeam(bool isReset, bool bOnlyIfDead);
	void GetEligibleSpawnListForTeam(const TSpawnLocations& rFullList, TSpawnLocations& rEligibleLocations, int nTeam, bool bInitialSpawn) const;

	virtual float GetTimeFromDeathTillAutoReviveForTeam(int teamId) const;
	virtual float GetPlayerAutoReviveAdditionalTime(IActor* pActor) const;
	virtual float GetAutoReviveTimeScaleForTeam(int teamId) const;
	virtual void SetAutoReviveTimeScaleForTeam(int teamId, float newScale);
	
	virtual void HostMigrationInsertIntoReviveQueue(EntityId playerId, float timeTillRevive);

	virtual void OnInGameBegin();
	virtual void OnInitialEquipmentScreenShown();
	virtual void OnNewRoundEquipmentScreenShown();
	virtual float GetRemainingInitialAutoSpawnTimer();
	virtual bool SvIsMidRoundJoiningAllowed() const;
	virtual bool IsInInitialChannelsList(uint32 channelId) const;

	virtual void HostMigrationStopAddingPlayers();
	virtual void HostMigrationResumeAddingPlayers();
	// ~IGameRulesSpawningModule

	bool CheckMidRoundJoining(CActor * pActor, EntityId playerId);

	// IGameRulesRevivedListener
	virtual void EntityRevived(EntityId entityId);
	// ~IGameRulesRevivedListener

	// IGameRulesRoundsListener
	virtual void OnRoundStart();
	virtual void OnRoundEnd();
	virtual void OnSuddenDeath() {}
	virtual void ClRoundsNetSerializeReadState(int newState, int curState) {}
	virtual void OnRoundAboutToStart() {}
	// ~IGameRulesRoundsListener

	// IGameRulesKillListener
	virtual void OnEntityKilledEarly(const HitInfo &hitInfo) {};
	virtual void OnEntityKilled(const HitInfo &hitInfo);
	// ~IGameRulesKillListener

protected:
	bool SpawnIsValid(EntityId spawnId, EntityId playerId) const;

	bool TestSpawnLocationWithEnvironment(const IEntity *pSpawn, EntityId playerId, float offset, float height) const;
	bool IsSpawnLocationSafeFromExplosives(const IEntity *pSpawn) const;
	bool IsSpawnLocationSafe(EntityId playerId, const IEntity * pSpawn, float safeDistance, bool ignoreTeam, float zoffset) const;
	bool DoesSpawnLocationRespectPOIs(EntityId spawnLocationId) const;

#if !defined(_RELEASE)
	void DebugPOIs() const;
#endif

	EntityId GetRandomSpawnLocation_Cached(const TSpawnLocations &spawnLocations, EntityId playerId, int playerTeamId, bool isInitalSpawn);
	EntityId GetRandomSpawnLocation(const TSpawnLocations &spawnLocations, EntityId playerId, int playerTeamId, bool isInitalSpawn) const;

	virtual EntityId GetSpawnLocationNonTeamGame(EntityId playerId, const Vec3 &deathPos) = 0;
	virtual EntityId GetSpawnLocationTeamGame(EntityId playerId, const Vec3 &deathPos) = 0;
	
	int GetTeamSpawnLocations(int teamId, bool bInitialSpawns, TSpawnLocations &results);
	void ReviveAllPlayers_Internal( IActor * pActor, EntityId spawnId, int nPlayerTeam );

	bool m_bGameHasStarted;
};

#endif // __GAMERULESMPSPAWNING_H__
