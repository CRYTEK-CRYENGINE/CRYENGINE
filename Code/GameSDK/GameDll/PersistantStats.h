// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 04:02:2010		Created by Ben Parbury
*************************************************************************/

#ifndef __PERSISTANTSTATS_H__
#define __PERSISTANTSTATS_H__


#include "GameRulesTypes.h"
#include "GameRulesModules/IGameRulesKillListener.h"
#include "GameRulesModules/IGameRulesClientScoreListener.h"
#include "GameRulesModules/IGameRulesRoundsListener.h"
#include "PlayerProgression.h"
#include <IGameRulesSystem.h>
#include <CryLobby/ICryStats.h>
#include <CryLobby/ICryLobby.h>
#include <IPlayerProfiles.h>

#include "AutoEnum.h"
#include <CryCore/TypeInfo_impl.h>

#include "AfterMatchAwards.h"	
#include "Network/Lobby/GameAchievements.h"

#include "ScriptBind_ProtectedBinds.h"

#include "StatsStructures.h"

struct IPlayerProfile;
struct ICryPak;
struct IFlashPlayer;
struct IFlashVariableObject;
class CPlayer;
class CHUDMissionObjective;
struct SDatabaseEntry;
struct SCrysis3GeneralPlayerStats;
struct SCrysis3MiscPlayerStats;
struct SCrysis3ScopedPlayerStats;
struct SCrysis3WeaponStats;

#define MELEE_WEAPON_NAME "Melee"
#define POLE_WEAPON_NAME "pole"
#define PANEL_WEAPON_NAME "panel"
#define ENV_WEAPON_THROWN "_thrown"

#define NUM_HISTORY_ENTRIES 5

static const char* s_unlockableAttachmentNames[] =
{
	// SP unlockable attachments
	"Silencer",
	"SilencerPistol",
	"LaserSight",
	"PistolLaserSight",
	"MatchBarrel",
	"MuzzleBreak",
	"Bayonet",
	"GrenadeLauncher",
	"ExtendedClipSCAR",
	"ExtendedClipFeline",
	"ExtendedClipJackal",
	"DoubleClipFeline",
	"DoubleClipJackal",
	"ForeGrip",
	"TyphoonAttachment",
	"Flashlight",
	"Reflex",
	"AssaultScope",
	"SniperScope",
	"TechScope",
	// MP exclusive (add here below)
	"GaussAttachment",
};

static const int s_numUnlockableAttachments = sizeof(s_unlockableAttachmentNames)/sizeof(char*);

static const char* s_displayableAttachmentNamesSP[] = 
{
	"Silencer",
	"SilencerPistol",
	"LaserSight",
	"PistolLaserSight",
	"MatchBarrel",
	"MuzzleBreak",
	"Bayonet",
	"GrenadeLauncher",
	"ExtendedClipSCAR",
	"ExtendedClipFeline",
	"ExtendedClipJackal",
	"DoubleClipFeline",
	"DoubleClipJackal",
	"ForeGrip",
	"TyphoonAttachment",
	"Flashlight",
	"Reflex",
	"AssaultScope",
	"SniperScope",
	"TechScope"
};

static const int s_numDisplayableAttachmentsSP = sizeof(s_displayableAttachmentNamesSP)/sizeof(char*);


enum EStatsFlags
{
	eSF_LocalClient			= 0,				//stats that are calculated for local client (All stats hence it's zero)
	eSF_RemoteClients		= BIT(0),		//stats that are tracked for remote clients
	//eST_ServerClient		=	BIT(1),		//stats that are tracked on the server and for local client

	eSF_MapParamNone		= BIT(2),		//Map stat that stores anything added (can't use online storage as it's stored as a string)
	eSF_MapParamWeapon	= BIT(3),		//Map stat that only stores Weapon names from the persistent map stats
	eSF_MapParamHitType	= BIT(4),		//Map stat that only stores HitType names from the persistent map stats
	eSF_MapParamGameRules	= BIT(5),	//Map stat that only stores GameRules names from the persistent map stats
	eSF_MapParamLevel		=	 BIT(6),	//Map stat that only stores Level names from the persistent map stats
	
	eSF_StreakMultiSession	=	 BIT(8),	//Streaks that shouldn't be reset per session, such as games won in a row
};

struct SPersistantStatsStrings
{
	CryFixedStringT<64> m_title;
	CryFixedStringT<64> m_value;
	float m_numericValue;
	bool m_highestWins;

	SPersistantStatsStrings()
		: m_numericValue(0.f)
		, m_highestWins(true)
	{}
};

enum EDatabaseStatValueFlag
{
	eDatabaseStatValueFlag_None = 0,
	eDatabaseStatValueFlag_Available = BIT(0),
	eDatabaseStatValueFlag_Viewed = BIT(1),
	eDatabaseStatValueFlag_Decrypted = BIT(2),
};

//Helper func
void GetClientScoreInfo( CGameRules* pGameRules, int &position, int &points, EntityId &outClosestCompetitorEntityId, int &outClosestCompetitorScore );

class CPersistantStats : public SGameRulesListener,
	public IGameRulesKillListener,
	public IPlayerProgressionEventListener,
	public IWeaponEventListener,
	public IItemSystemListener,
	public IGameRulesClientScoreListener,
	public IGameRulesRoundsListener,
	public IPlayerProfileListener,
	public ISystemEventListener,
	public IInputEventListener
{
public:
	struct SMVPCompare
	{
		SMVPCompare(int _kills, int _deaths, int _assists, int _points, int _gamemodePoints)
			: kills(_kills), deaths(_deaths), assists(_assists), points(_points), gamemodePoints(_gamemodePoints)
		{ }

		const bool CompareForMVP(const EGameMode gamemode, const SMVPCompare& otherPlayer) const;	// Compares against other player for MVP, returns true if better or draw, false if worse.
		const int MVPScore(const EGameMode gamemode) const;	// Returns the score used for MVP per gamemode

		int kills;
		int deaths;
		int assists;
		int points;
		int gamemodePoints;
	};

	friend class CPersistantStatsDebugScreen;
	
	typedef std::vector<string> TMapParams;

public:
	friend class CAfterMatchAwards;
	friend class CScriptBind_ProtectedBinds;

	CPersistantStats();
	virtual ~CPersistantStats();

	static CPersistantStats* GetInstance();

	void Init();
	void RegisterLevelTimeListeners();
	void UnRegisterLevelTimeListeners();
	void Update(const float dt);
	void Reset();
	void AddListeners();

	int GetStat(EIntPersistantStats stat);
	float GetStat(EFloatPersistantStats stat);
	int GetStat(EStreakIntPersistantStats stat);
	float GetStat(EStreakFloatPersistantStats stat);
	int GetStat(const char* name, EMapPersistantStats);
	const SSessionStats::SMap::MapNameToCount& GetStat(EMapPersistantStats stat);
	int GetDerivedStat(EDerivedIntPersistantStats stat);
	float GetDerivedStat(EDerivedFloatPersistantStats stat);
	const char *GetDerivedStat(EDerivedStringPersistantStats);
	const char *GetDerivedStat(EDerivedStringMapPersistantStats);
	int GetDerivedStat(const char* name, EDerivedIntMapPersistantStats);
	float GetDerivedStat(const char* name, EDerivedFloatMapPersistantStats);

	int GetStatThisSession(EStreakIntPersistantStats stat);
	float GetStatThisSession(EStreakFloatPersistantStats stat);

	void ResetStat(EStreakIntPersistantStats stat);
	void ResetStat(EStreakFloatPersistantStats stat);

	int GetStatForActorThisSession(EStreakIntPersistantStats stat, EntityId inActorId);
	float GetStatForActorThisSession(EStreakFloatPersistantStats stat, EntityId inActorId);
	int GetStatForActorThisSession(EIntPersistantStats stat, EntityId inActorId);
	float GetStatForActorThisSession(EFloatPersistantStats stat, EntityId inActorId);
	int GetStatForActorThisSession(EMapPersistantStats stat, const char* param, EntityId inActorId);
	int GetDerivedStatForActorThisSession(EDerivedIntPersistantStats stat, EntityId inActorId);
	float GetDerivedStatForActorThisSession(EDerivedFloatPersistantStats stat, EntityId inActorId);
	void GetMapStatForActorThisSession(EMapPersistantStats stat, EntityId inActorId, std::vector<int> &result);

	static EIntPersistantStats GetIntStatFromName(const char* name);
	static EFloatPersistantStats GetFloatStatFromName(const char* name);
	static EStreakIntPersistantStats GetStreakIntStatFromName(const char* name);
	static EStreakFloatPersistantStats GetStreakFloatStatFromName(const char* name);
	static EMapPersistantStats GetMapStatFromName(const char* name);
	static EDerivedIntPersistantStats GetDerivedIntStatFromName(const char* name);
	static EDerivedFloatPersistantStats GetDerivedFloatStatFromName(const char* name);
	static EDerivedIntMapPersistantStats GetDerivedIntMapStatFromName(const char* name);
	static EDerivedFloatMapPersistantStats GetDerivedFloatMapStatFromName(const char* name);

	static const char* GetNameFromIntStat(EIntPersistantStats stat);
	static const char* GetNameFromFloatStat(EFloatPersistantStats stat);
	static const char* GetNameFromStreakIntStat(EStreakIntPersistantStats stat);
	static const char* GetNameFromStreakFloatStat(EStreakFloatPersistantStats stat);
	static const char* GetNameFromMapStat(EMapPersistantStats stat);
	static const char* GetNameFromDerivedIntStat(EDerivedIntPersistantStats stat);
	static const char* GetNameFromDerivedFloatStat(EDerivedFloatPersistantStats stat);
	static const char* GetNameFromDerivedIntMapStat(EDerivedIntMapPersistantStats stat);
	static const char* GetNameFromDerivedFloatMapStat(EDerivedFloatMapPersistantStats stat);

	int GetCurrentStat(EntityId actorId, EStreakIntPersistantStats stat);

	CAfterMatchAwards *GetAfterMatchAwards() { return &m_afterMatchAwards; }
		
	bool IncrementWeaponHits(CProjectile& projectile, EntityId targetId);
	void ClientHit(const HitInfo&);
	void ClientShot(CGameRules* pGameRules, uint8 hitType, const Vec3 & dir);
	void ClientDelayedExplosion(EntityId projectileDelayed);
	void ClientRegenerated();
	void ClientDied( CPlayer* pClientPlayer );
	void OnSPLevelComplete(const char* levelName);
	void OnGiveAchievement(int achievement);

	void IncrementStatsForActor( EntityId inActorId, EIntPersistantStats stats, int amount = 1 );
	void IncrementStatsForActor( EntityId inActorId, EFloatPersistantStats stats, float amount = 1.0f );
	void IncrementClientStats(EIntPersistantStats stats, int amount = 1);
	void IncrementClientStats(EFloatPersistantStats stats, float amount = 1.0f);
	void IncrementMapStats(EMapPersistantStats stats, const char * name);
	void SetClientStat(EIntPersistantStats stat, int value );

	void SetMapStat(EMapPersistantStats stats, const char * name, int amount);
	void ResetMapStat(EMapPersistantStats stats);

	void HandleTaggingEntity(EntityId shooterId, EntityId targetId);
	void HandleLocalWinningKills();

	void OnClientDestroyedVehicle(const SVehicleDestroyedParams& vehicleInfo); 

	//SGameRulesListener
	virtual void GameOver(EGameOverType localWinner, bool isClientSpectator);
	virtual void EnteredGame();
	virtual void ClientDisconnect( EntityId clientId );
	virtual void ClTeamScoreFeedback(int teamId, int prevScore, int newScore);
	//~SGameRulesListener

	//IGameRulesKillListener
	virtual void OnEntityKilledEarly(const HitInfo& hitInfo){}
	virtual void OnEntityKilled(const HitInfo &hitInfo);
	//~IGameRulesKillListener

	//IPlayerProgressionEventListener
	virtual void OnEvent(EPPType type, bool skillKill, void *data);
	//~IPlayerProgressionEventListener

	// IWeaponEventListener
	virtual void OnShoot(IWeapon *pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3 &pos, const Vec3 &dir, const Vec3 &vel);
	virtual void OnStartFire(IWeapon *pWeapon, EntityId shooterId) {}
	virtual void OnStopFire(IWeapon *pWeapon, EntityId shooterId) {}
	virtual void OnFireModeChanged(IWeapon *pWeapon, int currentFireMode){}
	virtual void OnStartReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType);
	virtual void OnEndReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType) {}
	virtual void OnOutOfAmmo(IWeapon *pWeapon, IEntityClass* pAmmoType) {}
	virtual void OnReadyToFire(IWeapon *pWeapon) {}
	virtual void OnPickedUp(IWeapon *pWeapon, EntityId actorId, bool destroyed) {}
	virtual void OnDropped(IWeapon *pWeapon, EntityId actorId) {}
	virtual void OnMelee(IWeapon* pWeapon, EntityId shooterId);
	virtual void OnStartTargetting(IWeapon *pWeapon) {}
	virtual void OnStopTargetting(IWeapon *pWeapon) {}
	virtual void OnSelected(IWeapon *pWeapon, bool selected);
	virtual void OnSetAmmoCount(IWeapon *pWeapon, EntityId shooterId) {}
	virtual void OnEndBurst(IWeapon *pWeapon, EntityId shooterId) {}
	//~IWeaponEventListener

	//IItemSystemListener
	virtual void OnSetActorItem(IActor *pActor, IItem *pItem );
	virtual void OnDropActorItem(IActor *pActor, IItem *pItem ) {}
	virtual void OnSetActorAccessory(IActor *pActor, IItem *pItem ) {}
	virtual void OnDropActorAccessory(IActor *pActor, IItem *pItem ){}
	//~IItemSystemListener

	//IGameRulesClientScoreListener
	virtual void ClientScoreEvent(EGameRulesScoreType scoreType, int points, EXPReason inReason, int currentTeamScore);
	//~IGameRulesClientScoreListener

	//IGameRulesRoundsListener
	virtual void OnRoundStart() {}
	virtual void OnRoundEnd();
	virtual void OnSuddenDeath() {}
	virtual void ClRoundsNetSerializeReadState(int newState, int curState) {}
	virtual void OnRoundAboutToStart() {}
	//~IGameRulesRoundsListener

	//IPlayerProfileListener
	virtual void SaveToProfile(IPlayerProfile* pProfile, bool online, unsigned int reason);
	virtual void LoadFromProfile(IPlayerProfile* pProfile, bool online, unsigned int reason);
	//~IPlayerProfileListener

	//ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);
	//~ISystemEventListener

	// IInputEventListener
	virtual bool OnInputEvent(const SInputEvent &rInputEvent);
	// ~IInputEventListener

	void OnEnteredVTOL( EntityId playerId );
	void OnExitedVTOL( EntityId playerId );

	const bool ShouldSaveClientTelemetry() const;

	static const char * GetAdditionalWeaponNameForSharedStats(const char * name);

	void UpdateClientGrenadeBounce(const Vec3 pos, const float radius);
	void AddEntityToNearGrenadeList(EntityId entityId, Vec3 actorPos);
	bool HasClientFlushedTarget(EntityId targetId, Vec3 targetPos);
	bool IsClientDualWeaponKill(EntityId targetId);

	float GetLastFiredTime() const { return m_lastFiredTime; }

	void UpdateMultiKillStreak(EntityId shooterId, EntityId targetId);
	bool CheckRetaliationKillTarget(EntityId victimId);

	void SetMultiplayer(const bool multiplayer);
	int	 GetOnlineAttributesVersion();

	void OnQuit();
	
	void GetMemoryUsage(ICrySizer *pSizer ) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

	ILINE bool IsInGame() const { return m_inGame; }
	ILINE void SetInGame(const bool inGame) { m_inGame = inGame; }

	void UnlockAll();
	
	void OnEnterFindGame();
	void OnGameActuallyStarting();

	void GetStatStrings(EIntPersistantStats stat, SPersistantStatsStrings *statsStrings);
	void GetStatStrings(EFloatPersistantStats stat, SPersistantStatsStrings *statsStrings);
	void GetStatStrings(EStreakIntPersistantStats stat, SPersistantStatsStrings *statsStrings);
	void GetStatStrings(EStreakFloatPersistantStats stat, SPersistantStatsStrings *statsStrings);

	void GetStatStrings(EDerivedIntPersistantStats stat, SPersistantStatsStrings *statsStrings);
	void GetStatStrings(EDerivedFloatPersistantStats stat, SPersistantStatsStrings *statsStrings);
	void GetStatStrings(EDerivedStringPersistantStats stat, SPersistantStatsStrings *statsStrings);
	void GetStatStrings(EDerivedStringMapPersistantStats stat, SPersistantStatsStrings *statsStrings);

	void GetStatStrings(const char* name, EDerivedIntMapPersistantStats stat, const char* paramString, SPersistantStatsStrings *statsStrings);
	void GetStatStrings(const char* name, EDerivedFloatMapPersistantStats stat, const char* paramString, SPersistantStatsStrings *statsStrings);
	void GetStatStrings(const char* name, EMapPersistantStats stat, const char* paramString, SPersistantStatsStrings *statsStrings);

	void GetStatStrings( EMapPersistantStats stat, SPersistantStatsStrings *statsStrings);

	const char * GetMapParamAt(const char * name, unsigned int at) const;
	const TMapParams* GetMapParamsExt(const char* name) const;
	const char* GetWeaponMapParamName() const;

	const static float	k_actorStats_inAirMinimum;
	const static int		kNumEnemyRequiredForAwards = 4;
	const static float	kTimeAllowedToKillEntireEnemyTeam;

	// Get Session stats for previous game - 0 == previous game, 1 == game before that etc. Up to max of previous CRYSIS3_NUM_HISTORY_ENTRIES-1
	const SSessionStats* GetPreviousGameSessionStatsForClient(uint8 previousGameIndex) const; 
	uint32 GetAverageDeltaPreviousGameXp(const uint8 desiredNumGamesToAverageOver) const;

	const char* GetFavoriteWeaponStr() const { return m_favoriteWeapon; }
	const char* GetFavoriteAttachmentStr() const { return m_favoriteAttachment; }

	static const int k_nLoadoutModules = 3;

protected:
	static CPersistantStats* s_persistantStats_instance;
	
	void SetWeaponStat(const char* weaponName, IFlashPlayer *pFlashPlayer, IFlashVariableObject *pPushArray);
	void SetSPWeaponStat(const char* weaponName, IFlashPlayer *pFlashPlayer, IFlashVariableObject *pPushArray, const char* nameOverride=NULL);
	void SetGamemodeStat(const char* gamemodeName, IFlashPlayer *pFlashPlayer, IFlashVariableObject *pPushArray);
	void SetMissionObjectivesStat(const CHUDMissionObjective& objective, IFlashPlayer *pFlashPlayer, IFlashVariableObject *pPushArray);
	void SetDatabaseStat(int iDatabaseStatValueFlags, const SDatabaseEntry& entry, IFlashPlayer *pFlashPlayer, IFlashVariableObject *pPushArray);

	bool ShouldUpdateStats();

	void SaveLatestGameHistoryDeltas( const SSessionStats& latestSessionStats, const SSessionStats& startSessionStats, SSessionStats& outStatHistory ); 
	

	float m_lastFiredTime;

	EntityId m_retaliationTargetId;
	bool m_weaponDamaged;

	const static int MAX_GROUP_STATS = 64;
	typedef CryFixedArray<SPersistantStatsStrings, MAX_GROUP_STATS> TPersistantStatsStringsArray;
	typedef std::vector<const char*> TStringsPushArray;

	void CalculateOverallWeaponStats();
	void CalculateOverallAttachmentStats();

	int m_iCalculatedTotalWeaponsUsedTime;
	CryFixedStringT<16> m_favoriteWeapon;
	CryFixedStringT<32> m_favoriteAttachment;
	CryFixedStringT<32> m_dummyStr;

	CAfterMatchAwards m_afterMatchAwards;

	//Sessions are stored for every player for the current session
	typedef std::map<EntityId, SSessionStats, std::less<EntityId>, stl::STLGlobalAllocator<std::pair<const EntityId, SSessionStats> > > ActorSessionMap;
	ActorSessionMap m_sessionStats;

	//local clients stats that are updated at the end of session
	SSessionStats m_clientPersistantStats;

	//local clients stats that are cached at the start of the session so deltas can be observed
	SSessionStats m_clientPersistantStatsAtGameStart;

	// We maintain a history of local client stats (initially populated via blaze game history.. but then maintained locally). 
	// Represents 'deltas' (??)
	typedef std::deque<SSessionStats> TSessionStatHistoryQueue; // random access + constant time insertion at front + removal at end:) 
	TSessionStatHistoryQueue m_clientPersistantStatHistory; 
	
	typedef VectorMap<EntityId, float> NearGrenadeMap;
	NearGrenadeMap m_nearGrenadeMap;

	struct SPreviousWeaponHit
	{
		SPreviousWeaponHit()
		{
			m_curWeaponLastHitTime = 0.f;
			m_prevWeaponLastHitTime = 0.f;
			m_curWeaponClassId = 0;
			m_prevWeaponClassId = 0;
		}

		SPreviousWeaponHit(int _weaponClassId, float _currentTime)
		{
			m_curWeaponLastHitTime = _currentTime;
			m_prevWeaponLastHitTime = 0.f;
			m_curWeaponClassId = _weaponClassId;
			m_prevWeaponClassId = 0;
		}

		float m_curWeaponLastHitTime;
		float m_prevWeaponLastHitTime;

		int m_curWeaponClassId;
		int m_prevWeaponClassId;
	};

	typedef VectorMap<EntityId, SPreviousWeaponHit> PreviousWeaponHitMap;
	PreviousWeaponHitMap m_previousWeaponHitMap;

	struct SGrenadeKillElement
	{
		EntityId m_victimId;
		float m_atTime;

		SGrenadeKillElement()
		{
			m_victimId=0;
			m_atTime=0.f;
		}

		SGrenadeKillElement(EntityId inVictimId, float inAtTime)
		{
			m_victimId=inVictimId;
			m_atTime=inAtTime;
		}
	};
	typedef CryFixedArray<SGrenadeKillElement, 3> TGrenadeKills;
	TGrenadeKills m_grenadeKills;
	static const float k_fastGrenadeKillTimeout;

	struct SEnemyTeamMemberInfo
	{
		EntityId m_entityId;											// duplicated from the map index
		float m_timeEnteredKilledState;
		float m_timeOfLastKill;
		static const float k_timeInKillStateTillReset;
		
		enum EState
		{
			k_state_initial=0,
			k_state_tagged,
			k_state_killed,
			k_state_crouchedOver,
		};
		EState m_state;

		int m_killedThisMatch;
		int m_beenKilledByThisMatch;
		bool m_killedThisMatchNotDied;
		bool m_teabaggedThisDeath;
		bool m_taggedThisMatch;

		SEnemyTeamMemberInfo()
			: m_entityId(INVALID_ENTITYID)
			, m_state(k_state_initial)
			, m_timeEnteredKilledState(0.0f)
			, m_timeOfLastKill(0.0f)
			, m_killedThisMatch(0)
			, m_beenKilledByThisMatch(0)
			, m_killedThisMatchNotDied(false)
			, m_teabaggedThisDeath(false)
			, m_taggedThisMatch(false)
		{}
	};

	struct SSortStat
	{
		SSortStat(const char* name );
		static bool WeaponCompare ( SSortStat elem1, SSortStat elem2 );
		static bool GamemodeCompare ( SSortStat elem1, SSortStat elem2 );

		const char* m_name;
	};

	struct SPreviousKillData
	{
		SPreviousKillData();
		SPreviousKillData(int hitType, bool bEnvironmental, bool bWasPole);
		int m_hitType;
		bool m_bEnvironmental;
		bool m_bWasPole;
	};

	typedef VectorMap<EntityId, SEnemyTeamMemberInfo> EnemyTeamMemberInfoMap;
	EnemyTeamMemberInfoMap m_enemyTeamMemberInfoMap;

	const static uint32 k_previousKillsToTrack = 2;
	std::deque<SPreviousKillData> m_clientPreviousKillData;

	void AddClientHitActorWithWeaponClassId(EntityId actorHit, int weaponClassId, float currentTime);
	SEnemyTeamMemberInfo *GetEnemyTeamMemberInfo(EntityId inEntityId);

	SSessionStats* GetActorSessionStats(EntityId actorId);
	SSessionStats* GetClientPersistantStats();
	SSessionStats* GetClientPersistantStatsAtSessionStart();

	void PostGame( bool bAllowToDoStats );

	bool LoadFromProfile(SSessionStats* pSessionStats);
	bool SaveToProfile(const SSessionStats* pSessionStats);

	static const char* GetAttributePrefix();
	void SaveTelemetry(bool description, bool toDisk);

	int GetBinaryVersionHash(uint32 flags);
	void SaveTelemetryInternal(const char* filenameNoExt, const SSessionStats* pSessionStats, uint32 flags, bool description, bool toDisk);

	template <class T>
	void WriteToBuffer(char* buffer, int &bufferPosition, const int bufferSize, T *data, size_t length, size_t elems);

	void ClearListeners();
	
	typedef std::map<EntityId, EntityId> ActorWeaponListenerMap;
	ActorWeaponListenerMap m_actorWeaponListener;

	void SetNewWeaponListener(IWeapon* pWeapon, EntityId weaponId, EntityId actorId);
	void RemoveWeaponListener(EntityId weaponId);
	void RemoveAllWeaponListeners();

	const char* GetActorItemName(EntityId actorId);
	const char* GetItemName(EntityId weaponId);
	const char* GetItemName(IItem* pItem);

	const bool IsClientMVP(CGameRules* pGameRules) const;

	static float GetRatio(int a, int b);

	typedef std::vector<char> TDescriptionVector;
#ifndef _RELEASE
	static void CmdDumpTelemetryDescription(IConsoleCmdArgs* pCmdArgs);
	static void CmdSetStat(IConsoleCmdArgs* pCmdArgs);
	static void CmdTestStats(IConsoleCmdArgs* pCmdArgs);
	template <class T>
	static void CreateDescriptionNode(int &pos, TDescriptionVector& buffer, const char* codeName, size_t size, const char* type, const char* mapName, T testValue);

	static void WriteDescBuffer(TDescriptionVector& buffer, string text);
#endif

	struct SMapParam
	{
		SMapParam(const char* name, EStatsFlags flag)
		{
			m_name = name;
			m_flag = flag;
		};

		const char* m_name;
		EStatsFlags m_flag;
		TMapParams m_mapParam;
	};

	static SMapParam s_mapParams[];
	void InitMapParams();
	void InitStreakParams();
	void ClearMapParams();
	TMapParams* GetMapParams(const char* name);
	void SaveMapStat(IPlayerProfile* pProfile, SSessionStats* pSessionStats, int index);
	void LoadMapStat(IPlayerProfile* pProfile, SSessionStats* pSessionStats, int index);

	const int MapParamCount(const uint32 flags);
	const char* MapParamName(const uint32 flags, const int index);
	const bool IsMapParam(const uint32 flag, const char* paramName);

	bool IsMultiplayerMapName(const char* name) const;

	bool NearFriendly(CPlayer *pClientPlayer, float distanceSqr = 16.0f);
	bool NearEnemy(CPlayer *pClientPlayer, float distanceSqr = 16.0f);
	EntityId ClientNearCorpse(CPlayer *pClientActor);

	void OnEntityKilledCheckSpecificMultiKills(const HitInfo &hitInfo, CPlayer* pShooterPlayer);
	bool CheckPreviousKills(uint32 killsToCheck, int desiredHitType, bool bEnvironmental, bool bIsPole) const;

	bool ShouldProcessOnEntityKilled(const HitInfo &hitInfo);
	void UpdateGamemodeTime(const char* gamemodeName, SSessionStats* pClientStats, CActor* pClientActor);
	void UpdateWeaponTime(CWeapon* pCWeapon);
	void UpdateModulesTimes();
	void OnGameStarted();
	
	const static int k_noMoreMerryMenHunterKillsNeeded = 10;
	const static int k_20MetreHighClubVTOLHMGKillsNeeded = 10;
	const static int k_crouchesToggleNeeded = 4;
	const static int k_warbirdTimeFromCloak = 5;
	const static float k_cloakedVictimTimeFromCloak;
	const static float k_delayDetonationTime;
	const static float k_longDistanceThrowKillMinDistanceSquared;
	static bool s_multiplayer;

	bool m_crouching;
	float m_crouchToggleTime[k_crouchesToggleNeeded];
	
	uint16 m_pickAndThrowWeaponClassId;
	
	CWeapon* m_selectedWeapon;

	uint16 m_selectedWeaponId;
	
	float m_selectedWeaponTime;
	float m_gamemodeStartTime;
	float m_lastTimeMPTimeUpdated;

	float m_lastModeChangedTime;
	float m_fSecondTimer;
	int		m_idleTime;

	bool m_gamemodeTimeValid;
	bool m_inGame; 
	bool m_bHasCachedStartingStats;
	bool m_bSentStats;	//will be false at level unload if we weren't allowed to send stats
	bool m_localPlayerInVTOL;

	int	m_DateFirstPlayed_Year;
	int	m_DateFirstPlayed_Month;
	int	m_DateFirstPlayed_Day;

	//Used for Telemetry uploading
	static int s_levelNamesVersion;
	static const char* sz_levelNames[];
	static const char* sz_weaponNames[];
};

#endif
