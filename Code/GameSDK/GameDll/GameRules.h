// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 

	-------------------------------------------------------------------------
	History:
	- 7:2:2006   15:38 : Created by Márcio Martins

*************************************************************************/
#ifndef __GAMERULES_H__
#define __GAMERULES_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IGameObject.h>
#include <IGameRulesSystem.h>
#include <IItemSystem.h>
#include "Game.h"
#include "IViewSystem.h"
#include "GameCVars.h"
#include "GameRulesTypes.h"
#include "CinematicInput.h"
#include "Audio/AudioSignalPlayer.h"
#include "Utility/CryHash.h"
#include "Voting.h"

#define MAX_CONCURRENT_EXPLOSIONS 64

struct IActor;
class CActor;
class CPlayer;

struct IGameObject;
struct IActorSystem;
struct SGameRulesScoreInfo;
struct SGameRulesPlayerStat;

class CEquipmentLoadout;
class CBattlechatter;
class CAreaAnnouncer;
class CMiscAnnouncer;
class CExplosionGameEffect;
class CVTOLVehicleManager;
class CCorpseManager;
class CAnnouncer;
class CClientHitEffectsMP;
class CTeamVisualizationManager;

class IGameRulesPickupListener;
class IGameRulesClientConnectionListener;
class IGameRulesTeamChangedListener;
class IGameRulesKillListener;
class IGameRulesModuleRMIListener;
class IGameRulesRevivedListener;
class IGameRulesSurvivorCountListener;
class IGameRulesPlayerStatsListener;
class IGameRulesRoundsListener;
class IGameRulesClientScoreListener;
class IGameRulesActorActionListener;

#if USE_PC_PREMATCH
class IGameRulesPrematchListener;
#endif // #USE_PC_PREMATCH

class CMPTrackViewManager;
class CMPPathFollowingManager;

#include "GameRulesModules/GameRulesModulesRegistration.h"

#define EQUIPMENT_LOADOUT_NUM_SLOTS     10

#if defined(DEV_CHEAT_HANDLING)
#define DEV_CHEAT_HANDLING_RMI_COUNT 1
#else
#define DEV_CHEAT_HANDLING_RMI_COUNT 0
#endif

#ifdef INCLUDE_GAME_AI_RECORDER_NET
#define GAME_RULES_TOTAL_RMI_COUNT (54 + PC_PREMATCH_RMI_COUNT + DEV_CHEAT_HANDLING_RMI_COUNT)
#else
#define GAME_RULES_TOTAL_RMI_COUNT (52 + PC_PREMATCH_RMI_COUNT + DEV_CHEAT_HANDLING_RMI_COUNT)
#endif



struct SProjectileExplosionParams
{
	SProjectileExplosionParams() :
		m_shooterId(0),
		m_weaponId(0),
		m_projectileId(0),
		m_impactId(0),
		m_pos(ZERO),
		m_dir(FORWARD_DIRECTION),
		m_impactDir(FORWARD_DIRECTION),
		m_impactVel(ZERO),
		m_damage(0.f),
		m_projectileClass(0),
		m_impact(false),
		m_isProxyExplosion(false)
	{
	}

	SProjectileExplosionParams(
			EntityId shooterId, 
			EntityId weaponId,
			EntityId projectileId,
			EntityId impactId,
			Vec3 pos,
			Vec3 dir,
			Vec3 impactDir,
			Vec3 impactVel,
			float damage,
			uint16 projectileClass,
			bool impact,
			bool isProxy) :
		m_shooterId(shooterId),
		m_weaponId(weaponId),
		m_projectileId(projectileId),
		m_impactId(impactId),
		m_pos(pos),
		m_dir(dir),
		m_impactDir(impactDir),
		m_impactVel(impactVel),
		m_damage(damage),
		m_projectileClass(projectileClass),
		m_impact(impact),
		m_isProxyExplosion(isProxy)
	{
	}

	void SerializeWith(TSerialize ser)
	{
		ser.Value("shooterId", m_shooterId, 'eid');
		ser.Value("weaponId", m_weaponId, 'eid');
		ser.Value("projectileId", m_projectileId, 'eid');
		ser.Value("pos", m_pos, 'wrld');
		ser.Value("dir", m_dir, 'dir0');
		ser.Value("dmg", m_damage, 'dmg');
		ser.Value("projectileClass", m_projectileClass, 'clas');
	}

	string m_overrideEffectClassName;

	EntityId m_shooterId;
	EntityId m_weaponId;
	EntityId m_projectileId;
	EntityId m_impactId;

	Vec3 m_pos;
	Vec3 m_dir;
	Vec3 m_impactDir;
	Vec3 m_impactVel;

	float m_damage;

	uint16 m_projectileClass;		
	bool m_impact;
	bool m_isProxyExplosion;
};


struct SProjectileExplosionParams_Impact
{
	SProjectileExplosionParams_Impact() { m_params.m_impact = true; }
	SProjectileExplosionParams_Impact(const SProjectileExplosionParams& params) : m_params(params)	{	m_params.m_impact = true; }

	void SerializeWith(TSerialize ser)
	{
		m_params.SerializeWith(ser);

		ser.Value("impactId", m_params.m_impactId, 'eid');
		ser.Value("impactDir", m_params.m_impactDir, 'dir1');
		ser.Value("impactVel", m_params.m_impactVel, 'pPVl');
	}

	SProjectileExplosionParams m_params;
};


// Summary
//   Structure to describe a vehicle death
struct SVehicleDestroyedParams
{
	EntityId	vehicleEntityId;
	EntityId	weaponId; // EntityId of the weapon
	uint16		projectileClassId;
	int				type; // type id of the hit, see IGameRules::GetHitTypeId for more information

	SVehicleDestroyedParams() : vehicleEntityId(0), weaponId(0), type(0), projectileClassId(~uint16(0)) {}

	SVehicleDestroyedParams(EntityId _vehicleId, EntityId _weaponId, int _hitType, uint16 _projectileClassId)
		: vehicleEntityId(_vehicleId), weaponId(_weaponId), type(_hitType), projectileClassId(_projectileClassId) {}

	void SerializeWith(TSerialize ser)
	{
		ser.Value("vehicleEntityId",	vehicleEntityId,	'eid');
		ser.Value("weaponId",					weaponId,					'eid');
		ser.Value("type",							type,							'hTyp');
		ser.Value("projectileClass",	projectileClassId,'ui16');
	}
};


//Deferred raycast state for explosions
enum EDeferredMfxExplosionState
{
	eDeferredMfxExplosionState_None = 0,					//Nothing has been done with the deferred raycast
	eDeferredMfxExplosionState_Dispatched,					//The deferred racast has been sent
	eDeferredMfxExplosionState_ProcessingComplete,			//The results from the deferred raycast have been processed
	eDeferredMfxExplosionState_ResultImpact,				//The results from the deferred raycast have been received, it was an impact
	eDeferredMfxExplosionState_ResultNoImpact				//The results from the deferred raycast have been received, there was no impact
};

struct SDeferredMfxExplosion
{
	SDeferredMfxExplosion()
		: m_rayId(0)
		, m_mfxTargetSurfaceId(0)
		, m_state(eDeferredMfxExplosionState_None)
	{

	}

	~SDeferredMfxExplosion()
	{
		Reset();
	}

	void OnRayCastDataReceived(const QueuedRayID& rayID, const RayCastResult& result);
	void Reset();

	QueuedRayID						m_rayId;
	int								m_mfxTargetSurfaceId;
	_smart_ptr<IPhysicalEntity>		m_pMfxTargetPhysEnt;

	EDeferredMfxExplosionState	m_state;
};

struct SExplosionContainer
{
	ExplosionInfo			m_explosionInfo;
	SDeferredMfxExplosion	m_mfxInfo;
};

struct SPathFollowingAttachToPathParameters
{
	SPathFollowingAttachToPathParameters()
		: classId(0)
		, pathFollowerId(0)
		, pathIndex(0)
		, shouldStartAtInitialNode(false)
		, shouldLoop(false)
		, speed(0)
		, defaultSpeed(0)
		, nodeIndex(-1)
		, interpNodeIndex(-1)
		, waitTime(0.f)
		, forceSnap(false)
	{
	}

	SPathFollowingAttachToPathParameters(uint16 followerClassId, EntityId followerEntityId, uint8 pathToFollowIndex, bool startAtInitialNode, bool loop, float followerSpeed, float _defaultSpeed, int pathNodeIndex, int interpPathNodeIndex, float _waitTime, bool _forceSnap)
	{
		classId = followerClassId;
		pathFollowerId = followerEntityId;
		pathIndex = pathToFollowIndex;
		shouldStartAtInitialNode = startAtInitialNode;
		shouldLoop = loop;
		speed = followerSpeed;
		defaultSpeed = _defaultSpeed;
		nodeIndex = pathNodeIndex;
		interpNodeIndex = interpPathNodeIndex;
		waitTime = _waitTime;
		forceSnap = _forceSnap;
	}

	void SerializeWith(TSerialize ser)
	{
		ser.Value("classId", classId, 'ui16');
		ser.Value("pathFollowerId", pathFollowerId, 'eid');
		if(pathIndex > 3)
		{
			CryFatalError("SPathFollowingAttachToPathParameters - pathIndex higher than compression policy supports.");
		}
		ser.Value("pathIndex", pathIndex, 'ui2');
		ser.Value("shouldStartAtInitialNode", shouldStartAtInitialNode, 'bool');
		ser.Value("shouldLoop", shouldLoop, 'bool');
		ser.Value("speed", speed, 'aMas');
		ser.Value("defaultSpeed", defaultSpeed, 'aMas');
		++nodeIndex;
#ifndef _RELEASE
		if(nodeIndex > 63)
		{
			CryFatalError("SPathFollowingAttachToPathParameters - nodeIndex higher than compression policy supports");
		}
#endif //_RELEASE
		ser.Value("nodeIndex", nodeIndex, 'ui6');
		--nodeIndex;

		++interpNodeIndex;
#ifndef _RELEASE
		if(interpNodeIndex > 63)
		{
			CryFatalError("SPathFollowingAttachToPathParameters - interpNodeIndex higher than compression policy supports");
		}
#endif //_RELEASE
		ser.Value("interpNodeIndex", interpNodeIndex, 'ui6');
		--interpNodeIndex;

		ser.Value("waitTime", waitTime, 'hHSz');
		ser.Value("forceSnap", forceSnap, 'bool');
	}

	uint16 classId;
	EntityId pathFollowerId;
	uint8 pathIndex;
	bool shouldStartAtInitialNode;
	bool shouldLoop;
	float speed;
	float defaultSpeed;
	int nodeIndex;
	int interpNodeIndex;
	float waitTime;
	bool forceSnap;
};


// keep in sync with CGameRulesCommonDamageHandling::Init()
// Note: Also keep in sync with HitTypes.xml and hitDeathTags.xml
#define RESERVED_HIT_TYPES(f) \
    f(Invalid)                \
    f(Melee)                  \
    f(Collision)              \
    f(Frag)                   \
    f(Explosion)              \
    f(StealthKill)            \
    f(SilentMelee)            \
    f(Punish)                 \
    f(PunishFall)             \
    f(Mike_Burn)              \
    f(Fall)                   \
    f(Normal)                 \
    f(Fire)                   \
    f(Bullet)                 \
    f(Stamp)                  \
    f(EnvironmentalThrow)     \
    f(meleeLeft)              \
    f(meleeRight)             \
    f(meleeKick)              \
    f(meleeUppercut)          \
    f(VehicleDestruction)     \
    f(Electricity)            \
    f(StealthKill_Maximum)    \
    f(EventDamage)            \
    f(VTOLExplosion)          \
    f(EnvironmentalMelee)     \



#define HIT_TYPES_FLAGS(f) \
	f(IsMeleeAttack)			\
	f(Server)							\
	f(ClientSelfHarm)			\
	f(ValidationRequired) \
	f(CustomValidationRequired) \
	f(SinglePlayerOnly)		\
	f(AllowPostDeathDamage) \
	f(IgnoreHeadshots) \



class CGameRules :	public CGameObjectExtensionHelper<CGameRules, IGameRules, GAME_RULES_TOTAL_RMI_COUNT>, 
										public IViewSystemListener,
										public IGameFrameworkListener,
										public IHostMigrationEventListener,
										public IEntityEventListener,
										public IInputEventListener,
										public ISystemEventListener
{
public:

	typedef std::vector<EntityId>								TPlayers;
	typedef std::vector<EntityId>								TSpawnLocations;
	typedef std::map<EntityId, TSpawnLocations>	TSpawnGroupMap;
	typedef std::map<EntityId, int>							TBuildings;
	typedef std::set<CryUserID>									TCryUserIdSet;

	typedef std::vector<SGameRulesListener*> TGameRulesListenerVec;

	typedef std::map<IEntity *, float> TExplosionAffectedEntities;

	#define ERTRList(f)														\
		f(eRTR_General)															\
		f(eRTR_Tagging)															\
		f(eRTR_RadarOnly)														\
		f(eRTR_OnShot)															\
		f(eRTR_OnShoot)															\

	AUTOENUM_BUILDENUMWITHTYPE_WITHNUM(ERadarTagReason, ERTRList, eRTR_Last);

	struct EHitType
	{
		AUTOENUM_BUILDENUMWITHTYPE_WITHNUM(type, RESERVED_HIT_TYPES, Unreserved);
	};

	struct EHitTypeFlag
	{
		AUTOENUM_BUILDFLAGS_WITHZERO(HIT_TYPES_FLAGS, None);
	};

	enum EHeadShotType
	{
		eHeadShotType_None = 0,
		eHeadShotType_Head,
		eHeadShotType_Helmet
	};

	// The various 'channels' for generating hit-feedback to the local player client.
	enum ELocalPlayerHitFeedbackChannel
	{
		eLocalPlayerHitFeedbackChannel_Undefined = 0,	// Only used for error handling and debugging.
		eLocalPlayerHitFeedbackChannel_HUD,             // Visual feedback on the HUD.
		eLocalPlayerHitFeedbackChannel_2DSound,         // Direct sound feedback (not in world-space).
	};


	// This structure contains the necessary information to create a new player
	// actor from a migrating one (a new player actor is created for each
	// reconnecting client and needs to be identical to the original actor on
	// the original server, or at least as close as possible)
	struct SMigratingPlayerInfo 
	{
		CryFixedStringT<HOST_MIGRATION_MAX_PLAYER_NAME_SIZE>	m_originalName;
		EntityId			m_originalEntityId;
		TNetChannelID	m_channelID;
		bool					m_inUse;

		SMigratingPlayerInfo() : m_inUse(false), m_channelID(0) {}

		void SetChannelID(uint16 id) { assert(id > 0); m_channelID = id; }

		void SetData(const char* inOriginalName, EntityId inOriginalEntityId, int inTeam, const Vec3& inPos, const Ang3& inOri, float inHealth)
		{
			m_originalName = inOriginalName;
			m_originalEntityId = inOriginalEntityId;
			m_inUse = true;
		}

		void Reset() { m_inUse = false; m_channelID = 0; }

		bool InUse() { return m_inUse; }
	};

	CGameRules();
	virtual ~CGameRules();
	//IGameObjectExtension
	virtual bool Init( IGameObject * pGameObject );
	virtual void PostInit( IGameObject * pGameObject );
	virtual void InitClient(int channelId);
	virtual void PostInitClient(int channelId);
	virtual bool ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params );
	virtual void PostReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params ) {}
	virtual bool GetEntityPoolSignature( TSerialize signature );
	virtual void FullSerialize( TSerialize ser );
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );
	virtual void PostSerialize();
	virtual void SerializeSpawnInfo( TSerialize ser ) {}
	virtual ISerializableInfoPtr GetSpawnInfo() {return 0;}
	virtual void Update( SEntityUpdateContext& ctx, int updateSlot );
	virtual void HandleEvent( const SGameObjectEvent& );
	virtual void ProcessEvent( const SEntityEvent& );
	virtual uint64 GetEventMask() const;
	virtual void SetChannelId(uint16 id) {};
	virtual void PostUpdate( float frameTime );
	virtual void PostRemoteSpawn() {};
	virtual void GetMemoryUsage(ICrySizer *pSizer) const;
	
	//~IGameObjectExtension

	// IViewSystemListener
	virtual bool OnBeginCutScene(IAnimSequence* pSeq, bool bResetFX);
	virtual bool OnEndCutScene(IAnimSequence* pSeq);
	virtual bool OnCameraChange(const SCameraParams& cameraParams){ return true; };
	// ~IViewSystemListener

	// ISystemEventListener
	virtual void OnSystemEvent( ESystemEvent event,UINT_PTR wparam,UINT_PTR lparam );
	// ~ISystemEventListener

	//IGameRules
	virtual bool ShouldKeepClient(int channelId, EDisconnectionCause cause, const char *desc) const;
	virtual void PrecacheLevel();
	virtual void PrecacheLevelResource(const char* resourceName, EGameResourceType resourceType);

	virtual XmlNodeRef FindPrecachedXmlFile(const char *sFilename); // Checks to see whether the xml node ref exists in the precache map, keyed by filename. If it does, it returns it. If it doesn't, it returns a NULL ref

	virtual void OnConnect(struct INetChannel *pNetChannel);
	virtual void OnDisconnect(EDisconnectionCause cause, const char *desc); // notification to the client that he has been disconnected

	virtual bool OnClientConnect(int channelId, bool isReset);
	virtual void OnClientDisconnect(int channelId, EDisconnectionCause cause, const char *desc, bool keepClient);
	virtual bool OnClientEnteredGame(int channelId, bool isReset);

	virtual void OnEntitySpawn(IEntity *pEntity);
	virtual void OnEntityRemoved(IEntity *pEntity);
	virtual void OnEntityReused(IEntity *pEntity, SEntitySpawnParams &params, EntityId prevId);
	
	virtual void SendTextMessage(ETextMessageType type, const char *msg, uint32 to=eRMI_ToAllClients, int channelId=-1,
		const char *p0=0, const char *p1=0, const char *p2=0, const char *p3=0);
	virtual void SendChatMessage(EChatMessageType type, EntityId sourceId, EntityId targetId, const char *msg);

	virtual void ClientHit(const HitInfo &hitInfo);
	virtual void ServerHit(const HitInfo &hitInfo);

	virtual int GetHitTypeId(const uint32 crc) const;
	virtual int GetHitTypeId(const char *type) const;
	virtual const char *GetHitType(int id) const;
	
	const HitTypeInfo *GetHitTypeInfo(int id) const;

	virtual void OnVehicleDestroyed(EntityId id);
	virtual void OnVehicleSubmerged(EntityId id, float ratio);
	virtual bool CanEnterVehicle(EntityId playerId);

	virtual void CreateEntityRespawnData(EntityId entityId);
	virtual bool HasEntityRespawnData(EntityId entityId) const;
	virtual void ScheduleEntityRespawn(EntityId entityId, bool unique, float timer);
	virtual void AbortEntityRespawn(EntityId entityId, bool destroyData);

	virtual void ScheduleEntityRemoval(EntityId entityId, float timer, bool visibility);
	virtual void AbortEntityRemoval(EntityId entityId);

	virtual void AddHitListener(IHitListener* pHitListener);
	virtual void RemoveHitListener(IHitListener* pHitListener);

	virtual bool OnCollision(const SGameCollision& event);
	virtual void OnCollision_NotifyAI( const EventPhys * pEvent );

	virtual void ShowStatus();

	virtual bool IsTimeLimited() const;
	virtual float GetRemainingGameTime() const;
	virtual void SetRemainingGameTime(float seconds);

	virtual void ClearAllMigratingPlayers(void);
	virtual EntityId SetChannelForMigratingPlayer(const char* name, uint16 channelID);
	virtual void StoreMigratingPlayer(IActor* pActor);
	virtual void RestoreChannelTeamsFromMigration(IActor* pActor)
	{
		uint16 channelId = pActor->GetChannelId();
		if (channelId != 0)
		{
			stl::push_back_unique(m_channelIds, channelId);
			int teamId = GetTeam(pActor->GetEntityId());
			if (teamId != 0)
			{
				m_channelteams[(int)channelId] = teamId;
			}
		}
	}
	virtual void OnShutDown();
	//~IGameRules
	
	// IGameFrameworkListener
	virtual void OnPostUpdate(float fDeltaTime) {}
	virtual void OnSaveGame(ISaveGame* pSaveGame);
	virtual void OnLoadGame(ILoadGame* pLoadGame);
	virtual void OnLevelEnd(const char* pNextLevel) {}
	virtual void OnActionEvent(const SActionEvent& event);
	// ~IGameFrameworkListener

	// IHostMigrationEventListener
	virtual EHostMigrationReturn OnInitiate(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual EHostMigrationReturn OnDisconnectClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state) { return IHostMigrationEventListener::Listener_Done; }
	virtual EHostMigrationReturn OnDemoteToClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual EHostMigrationReturn OnPromoteToServer(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual EHostMigrationReturn OnReconnectClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual EHostMigrationReturn OnFinalise(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual void OnComplete(SHostMigrationInfo& hostMigrationInfo);
	virtual EHostMigrationReturn OnTerminate(SHostMigrationInfo& hostMigrationInfo, HMStateType& state) { return IHostMigrationEventListener::Listener_Done; }
	virtual EHostMigrationReturn OnReset(SHostMigrationInfo& hostMigrationInfo, HMStateType& state) { return IHostMigrationEventListener::Listener_Done; }
	// ~IHostMigrationEventListener

	// IEntityEventListener
	virtual void OnEntityEvent(IEntity *pEntity, const SEntityEvent &event);
	// ~IEntityEventListener

	// IInputEventListener
	virtual bool OnInputEvent(const SInputEvent &rInputEvent);
	// ~IInputEventListener

	void OnTimeOfDaySet();

	enum
	{
		k_rptfgm_none = 0,
		k_rptfgm_standard = BIT(0),
		k_rptfgm_marines = BIT(1),
		k_rptfgm_hunter = BIT(2),
		k_rptfgm_hunter_marine = BIT(3)
	};

	uint8 GetRequiredPlayerTypesForGameMode();
	bool GameModeRequiresDifferentCloakedChatter();
	uint8 GetRequiredPlayerTypeForConversation(int speakingActorTeamId, int listeningActorTeamId);

	ILINE uint32	GetSecurity()						{ return m_uSecurity ^ 0xD5379AD1; }
	ILINE bool		IsSecurityInitialized() { return m_bSecurityInitialized; }
	void OnEntityRespawn(IEntity *pEntity);

	void OnPickupEntityAttached(EntityId entityId, EntityId actorId, const char *pExtensionName);
	void OnPickupEntityDetached(EntityId entityId, EntityId actorId, bool isOnRemove, const char *pExtensionName);

	void OnItemDropped(EntityId itemId, EntityId actorId);
	void OnItemPickedUp(EntityId itemId, EntityId actorId);

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
	void SendNetConsoleCommand(const char *msg, unsigned int to, int channelId = -1);
#endif

	void ForbiddenAreaWarning(bool active, int timer, EntityId targetId);
	void IncreasePoints(EntityId who, const SGameRulesScoreInfo & scoreInfo);

	void ResetGameTime();
	float GetRemainingGameTimeNotZeroCapped() const;
	float GetCurrentGameTime() const;

	bool GameTimeValid() const { return (m_gameStartedTime > 0.0f); }

	void ResetGameStartTimer(float time=-1);
	float GetRemainingStartTimer() const;
	float GetServerTime() const;

	void ClDoSetTeam(int teamId, EntityId entityId);

	void OnHostMigrationGotLocalPlayer(CPlayer *pPlayer);
	void OnHostMigrationStateChanged();

	void OnUserLeftLobby(int channelId);

	int GetScoreLimit() const { return m_scoreLimit; }
	int GetRoundLimit() const { return m_roundLimit; }
	float GetTimeLimit() const { return m_timeLimit; }

	int GetMigratingPlayerIndex(TNetChannelID channelID);

	void RegisterConsoleCommands(IConsole *pConsole);
	void UnregisterConsoleCommands(IConsole *pConsole);
	void RegisterConsoleVars(IConsole *pConsole);
	void UnregisterConsoleVars(IConsole *pConsole);

	void OnRevive(IActor *pActor);
	void OnReviveInVehicle(IActor *pActor, EntityId vehicleId, int seatId, int teamId);
	void OnKill(IActor *pActor, const HitInfo &hitInfo, bool winningKill, bool firstKill, bool bulletTimeReplay);
	void OnTextMessage(ETextMessageType type, const char *msg,
		const char *p0=0, const char *p1=0, const char *p2=0, const char *p3=0);
	void OnKillMessage(EntityId targetId, EntityId shooterId);
	void OnActorDeath( CActor* pActor );

	IActor *GetActorByChannelId(int channelId) const;
	bool IsRealActor(EntityId actorId) const;
	IActor *GetActorByEntityId(EntityId entityId) const;
	const char *GetActorNameByEntityId(EntityId entityId) const;
	ILINE const char *GetActorName(IActor *pActor) const;
	ILINE CBattlechatter* GetBattlechatter() const { return m_pBattlechatter; }
	ILINE CAreaAnnouncer* GetAreaAnnouncer() const { return m_pAreaAnnouncer; }
	ILINE CMiscAnnouncer* GetMiscAnnouncer() const { return m_pMiscAnnouncer; }
	ILINE CVTOLVehicleManager*		GetVTOLVehicleManager() const	{ return m_pVTOLVehicleManager; };
	ILINE CTeamVisualizationManager*		GetTeamVisualizationManager() const	{ return m_pTeamVisualizationManager; };
	ILINE CCorpseManager*					GetCorpseManager() const				{ return m_pCorpseManager; };

	int GetChannelId(EntityId entityId) const;
	ILINE int GetNumChannels() const { return m_channelIds.size(); }
	ILINE const std::vector<int>* GetChannelIds() const { return &m_channelIds; }

	void ShowScores(bool show);
	ILINE bool IsLevelLoaded() const { return m_levelLoaded; }

	// Intro
	ILINE void SetIntroSequenceRegistered(const bool bRegister) {m_bIntroSequenceRegistered = bRegister;}
	ILINE bool IsIntroSequenceRegistered() const {return m_bIntroSequenceRegistered;}
	ILINE void SetIntroSequenceCurrentlyPlaying(bool bPlaying) {m_bIntroCurrentlyPlaying = bPlaying;} 
	ILINE bool IsIntroSequenceCurrentlyPlaying() {return m_bIntroCurrentlyPlaying;}
	ILINE bool IntroSequenceHasCompletedPlaying() {return m_bIntroSequenceCompletedPlaying;}
	ILINE void SetIntroSequenceHasCompletedPlaying() {m_bIntroSequenceCompletedPlaying = true; }

	//------------------------------------------------------------------------
	// player
	IActor *SpawnPlayer(int channelId, const char *name, const char *className, const Vec3 &pos, const Ang3 &angles);
	void RevivePlayerMP(IActor *pActor, IEntity *pSpawnPoint, int teamId, bool clearInventory);
	void RevivePlayer(IActor *pActor, const Vec3 &pos, const Ang3 &angles, int teamId, uint8 modelIndex, bool clearInventory);
	void RevivePlayerInVehicle(IActor *pActor, EntityId vehicleId, int seatId, int teamId, bool clearInventory);
	void ClearInventory(IActor *pActor);
	void RenamePlayer(IActor *pActor, const char *name);
	string VerifyName(const char *name, IEntity *pEntity=0);
	bool IsNameTaken(const char *name, IEntity *pEntity=0);
	void KillPlayer(IActor* pTarget, const bool inDropItem, const bool inDoRagdoll, const HitInfo &inHitInfo);
	void PostHitKillCleanup(IActor *pActor);
	void MovePlayer(IActor *pActor, const Vec3 &pos, const Quat& orientation);
	void ChangeTeam(IActor *pActor, int teamId, bool onlyIfUnassigned);
	void ChangeTeam(IActor *pActor, const char *teamName, bool onlyIfUnassigned);
	//tagging time serialization limited to 0-60sec
	void SvAddTaggedEntity(EntityId shooter, EntityId targetId, float time, ERadarTagReason reason );
	void RequestTagEntity(EntityId shooter, EntityId targetId, float time, ERadarTagReason reason );
	int GetPlayerCount(bool inGame=false, bool includeSpectators=false) const;
	int GetPlayerCountClient() const;
	int GetLivingPlayerCount() const;
	float GetFriendlyFireRatio() const;
	int GetSpectatorCount(bool inGame=false) const;
	EntityId GetPlayer(int idx);
	void GetPlayers(TPlayers &players) const;
	void GetPlayersClient(TPlayers &players);
	bool IsPlayer( EntityId playerId ) const;
	bool IsPlayerInGame(EntityId playerId) const;
	bool IsPlayerActivelyPlaying(EntityId playerId, bool mustBeAlive=false) const;	// [playing / dead / waiting to respawn (inc spectating while dead): true] [not yet joined game / selected Spectate: false]
	bool IsChannelInGame(int channelId) const;
	void StartVoting(IActor *pActor, EVotingState t, EntityId id, const char* param);
	int KickVotesTotalRequired();
	bool KickVoteConditionsMet(bool &bSuccess);
	void UpdateKickVoteStatus(EntityId entityId);
	void Vote(IActor *pActor, bool yes);
	void EndVoting(bool success);
	int GetTotalAlivePlayerCount( const EntityId skipPlayerId ) const;
	bool CanPlayerSwitchItem( EntityId playerId );
	bool RulesUseWeaponLoadouts();
	void OnActorAction(IActor *pActor, const ActionId& actionId, int activationMode, float value);
	CCinematicInput& GetCinematicInput() { return m_cinematicInput; }
	void SetAllPlayerVisibility(const bool bInvisible, const bool bIncludeClientPlayer);

	//------------------------------------------------------------------------
	// teams
	int CreateTeam(const char *name);
	void CreateTeamAlias(const char *name, int teamId);
	void RemoveTeam(int teamId);
	const char *GetTeamName(int teamId) const;
	int GetTeamId(const char *name) const;
	ILINE int GetTeamCount() const	{	return (int)m_teams.size();	}
	int GetTeamPlayerCount(int teamId, bool inGame=false) const;
	int GetTeamPlayerCountWithStatFlags(const int teamId, const int flagsNeeded, const bool needAllFlags);
	int GetTeamPlayerCountWhoHaveSpawned(int teamId);
	EntityId GetTeamPlayer(int teamId, int idx);
	EntityId GetTeamActivePlayer(int teamId, int idx) const;

	int GetTeamsScore(int teamId) const;	// Can't be called GetTeamScore() because of TIA function. All gunna be refactored anyways.
	void SetTeamsScore(int teamId, int score);
	int  GetTeamRoundScore(int teamId) const;
	void SetTeamRoundScore(int teamId, int score);
	void SetPausedGameTimer(bool bPaused, EGameOverReason reason);

	int SvGetTeamsScoreScoredThisRound(int teamId) const;
	void SvCacheRoundStartTeamScores();
	void ClientScoreEvent(EGameRulesScoreType scoreType, int points, EXPReason inReason, int currentTeamScore);

	void ActorActionInformOnAction(const ActionId& actionId, int activationMode, float value);

	void GetTeamPlayers(int teamId, TPlayers &players);
	
	void SetTeam(int teamId, EntityId entityId, bool clientOnly = false);
	int GetTeam(EntityId entityId) const;
	int GetChannelTeam(int channelId) const;

	enum eThreatRating
	{
		eFriendly = 0,
		eHostile = 1
	};
	//  Get the threat level that entityB poses towards entityA.
	eThreatRating GetThreatRating(const EntityId entityIdA, const EntityId entityIdB) const;
	eThreatRating GetThreatRatingByTeam(const int8 teamA, const int8 teamB) const;

	void ClientTeamScoreFeedback(int teamId, int prevScore, int newScore);

	bool IsKickVoteActive() const { return m_bClientKickVoteActive; }
	bool CanSendKickVote() const { return m_bClientKickVoteActive && !m_bClientKickVoteSent; }
	bool ClientKickVotedFor() const { return m_bClientKickVotedFor; }
	bool HasVotingCooldownEnded (float &timeLeft) const;

	bool IsTeamGame() const { return m_bIsTeamGame; }
	bool IndividualScore () const;
	bool ShowRoundsAsDraw () const;
	bool IsValidPlayerTeam(int teamId) const;
	//------------------------------------------------------------------------
	// hit type
	int RegisterHitType(const char *type, const uint32 flags = 0);
	const char* GetHitType(int id, const char* defaultValue) const;
	int GetHitTypesCount() const;
	bool ShouldGiveLocalPlayerHitableFeedbackForEntityClass(const IEntityClass* pEntityClass) const;
	bool ShouldGiveLocalPlayerHitableFeedbackOnCrosshairHoverForEntityClass(const IEntityClass* pEntityClass) const;
	bool ShouldGiveLocalPlayerHitFeedback(const ELocalPlayerHitFeedbackChannel feedbackChannel, const float damage) const;

	//------------------------------------------------------------------------
	// spawn
	void AddSpawnLocation(EntityId location, bool isInitialSpawn, bool doVisTest, const char *pGroupName);
	void RemoveSpawnLocation(EntityId id, bool isInitialSpawn);
	void EnableSpawnLocation(EntityId location, bool isInitialSpawn, const char *pGroupName);
	void DisableSpawnLocation(EntityId id, bool isInitialSpawn);
	int GetSpawnLocationCount() const;
	//EntityId GetSpawnLocation(int idx, bool initialSpawn) const;
	EntityId GetFirstSpawnLocation(int teamId=0, EntityId groupId=0) const;

	int GetEnemyTeamId(int myTeamId) const;
	bool IsSpawnUsed( const EntityId spawnId );

	//------------------------------------------------------------------------
	// spawn groups
	void AddSpawnGroup(EntityId groupId);
	void AddSpawnLocationToSpawnGroup(EntityId groupId, EntityId location);
	void RemoveSpawnLocationFromSpawnGroup(EntityId groupId, EntityId location);
	void RemoveSpawnGroup(EntityId groupId);
	EntityId GetSpawnLocationGroup(EntityId spawnId) const;
	int GetSpawnGroupCount() const;
	EntityId GetSpawnGroup(int idx) const;
	void GetSpawnGroups(TSpawnLocations &groups) const;
	bool IsSpawnGroup(EntityId id) const;
	bool AllowNullSpawnGroups() const;

	void RequestSpawnGroup(EntityId spawnGroupId);
	void SetPlayerSpawnGroup(EntityId playerId, EntityId spawnGroupId);
	EntityId GetPlayerSpawnGroup(IActor *pActor);

	void CheckSpawnGroupValidity(EntityId spawnGroupId);

	//------------------------------------------------------------------------
	// game	
	void Restart();
	void NextLevel();
	void ResetEntities();
	void OnEndGame();
	void EnteredGame();
	void GameOver(EGameOverType localWinner);
	void EndGameNear(EntityId id);
	void ClientDisconnect_NotifyListeners( EntityId clientId );
	void ClientEnteredGame_NotifyListeners( EntityId clientId );
	void OnActorDeath_NotifyListeners( CActor* pActor );
	void SvOnTimeLimitExpired_NotifyListeners();
	void EntityRevived_NotifyListeners( EntityId entityId );
	void SvSurvivorCountRefresh_NotifyListeners( int count, const EntityId survivors[], int numKills );
	void ClPlayerStatsNetSerializeReadDeath_NotifyListeners(const SGameRulesPlayerStat* s, uint16 prevDeathsThisRound, uint8 prevFlags);
	void OnRoundStart_NotifyListeners();
	void OnRoundEnd_NotifyListeners();
	void OnSuddenDeath_NotifyListeners();
	void ClRoundsNetSerializeReadState_NotifyListeners(int newState, int curState);
	void OnRoundAboutToStart_NotifyListeners();
	void KnockActorDown( EntityId actorEntityId );

#if USE_PC_PREMATCH
	void OnPrematchEnd_NotifyListeners(); 
#endif
	

	void ProcessServerHit(const HitInfo &hitInfo);
	void ProcessLocalHit(const HitInfo& hitInfo, float fCausedDamage = 0.0f);

	void UpdateNetLimbo();
	void UpdateIdleKick(float frametime);

	void AddLocalHitImpulse(const HitInfo& hitInfo);

	void CullEntitiesInExplosion(const ExplosionInfo &explosionInfo);
	void ClientExplosion(SExplosionContainer &explosionInfo);
	void QueueExplosion(const ExplosionInfo &explosionInfo);
	void ProjectileExplosion(const SProjectileExplosionParams &projectileExplosionInfo);

	void ResetQueuedExplosionsAndHits();
	
	void DoEntityRespawn(EntityId id);

	void UpdateEntitySchedules(float frameTime);
	void FlushEntitySchedules();
	void ProcessQueuedExplosions();
	ILINE void ClearExplosion(SExplosionContainer *pExplosionInfo);
	void ProcessServerExplosion(SExplosionContainer &explosionInfo);
	
	void FreezeInput(bool freeze);

	bool IsProjectile(EntityId id) const;

	void GetEntitiesToSkipByExplosion(const ExplosionInfo& explosionInfo, IPhysicalEntity** skipList, int& skipListSize) const;

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	void ReconfigureVoiceGroups(EntityId id,int old_team,int new_team);
#endif
	
	// Helper for precaching an equipment pack
	void PreCacheEquipmentPack(const char* szEquipmentPackName);

	//misc 
	// Next time CGameRules::OnCollision is called, it will skip this entity and return false
	// This will prevent squad mates to be hit by the player
	void SetEntityToIgnore(EntityId id) { m_ignoreEntityNextCollision = id;}
	
	void PlayerPosForRespawn(CPlayer* pPlayer, bool save);

	//compare gamerules class (replace by enum)
	bool IsGameRulesClass(const char *cls);

	bool IsMultiplayerDeathmatch()
	{
		return gEnv->bMultiplayer && !IsGameRulesClass("Coop");
	}

	bool IsMultiplayerCampaign()
	{
		return gEnv->bMultiplayer && IsGameRulesClass("Coop");
	}

	struct StringParams
	{
		string str;

		StringParams() {}
		StringParams(const char* szStr) : str(szStr) {}
		void SerializeWith(TSerialize ser)
		{
			ser.Value("str", str);
		}
	};

	struct ChatMessageParams
	{
		uint8 type;
		EntityId sourceId;
		EntityId targetId;
		string msg;
		bool onlyTeam;

		ChatMessageParams() {};
		ChatMessageParams(EChatMessageType _type, EntityId src, EntityId trg, const char *_msg, bool _onlyTeam)
		: type(_type),
			sourceId(src),
			targetId(trg),
			msg(_msg),
			onlyTeam(_onlyTeam)
		{
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("type", type, 'ui3');
			ser.Value("source", sourceId, 'eid');
			if (type == eChatToTarget)
				ser.Value("target", targetId, 'eid');
			ser.Value("message", msg);
			ser.Value("onlyTeam", onlyTeam, 'bool');
		}
	};

	struct ForbiddenAreaWarningParams
	{
		int timer;
		bool active;
		ForbiddenAreaWarningParams() {};
		ForbiddenAreaWarningParams(bool act, int time) : active(act), timer(time)
		{}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("active", active, 'bool');
			ser.Value("timer", timer, 'ui5');
		}
	};

	struct UInt8Param
	{
		uint8	param;

		UInt8Param() {};
		UInt8Param(uint8 inParam) : param(inParam) {}
		void SerializeWith(TSerialize ser)
		{
			ser.Value("param", param, 'ui3');
		}
	};

	struct TextMessageParams
	{
		uint8	type;
		string msg;

		uint8 nparams;
		string params[4];

		TextMessageParams() {};
		TextMessageParams(ETextMessageType _type, const char *_msg)
		: type(_type),
			msg(_msg),
			nparams(0)
		{
		};
		TextMessageParams(ETextMessageType _type, const char *_msg, 
			const char *p0=0, const char *p1=0, const char *p2=0, const char *p3=0)
		: type(_type),
			msg(_msg),
			nparams(0)
		{
			if (!AddParam(p0)) return;
			if (!AddParam(p1)) return;
			if (!AddParam(p2)) return;
			if (!AddParam(p3)) return;
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("type", type, 'ui3');
			ser.Value("message", msg);
			ser.Value("nparams", nparams, 'ui3');

			for (int i=0;i<nparams; ++i)
				ser.Value("param", params[i]);
		}

		bool AddParam(const char *param)
		{
			if (!param || nparams>3)
				return false;
			params[nparams++]=param;
			return true;
		}
	};

	struct NetConsoleCommandParams
	{
		string m_commandString;

		NetConsoleCommandParams() {};
		NetConsoleCommandParams(const char * cmdIn) : m_commandString (cmdIn) {}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("cmd", m_commandString);
		}
	};

	struct SetTeamParams
	{
		int				teamId;
		EntityId	entityId;

		SetTeamParams() {};
		SetTeamParams(EntityId _entityId, int _teamId)
		: entityId(_entityId),
			teamId(_teamId)
		{
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("entityId", entityId, 'eid');
			ser.Value("teamId", teamId, 'team');
		}
	};

	struct ChangeTeamParams
	{
		EntityId	entityId;
		int				teamId;
		bool			onlyIfUnassigned;

		ChangeTeamParams() {};
		ChangeTeamParams(EntityId _entityId, int _teamId, bool _onlyIfUnassigned)
			: entityId(_entityId),
				teamId(_teamId),
				onlyIfUnassigned(_onlyIfUnassigned)
		{
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("entityId", entityId, 'eid');
			ser.Value("teamId", teamId, 'team');
			ser.Value("onlyIfUnassigned", onlyIfUnassigned, 'bool');
		}
	};

	struct SpectatorModeParams
	{
		EntityId	entityId;
		uint8			mode;
		EntityId	targetId;
		bool			resetAll;
		bool			force;

		SpectatorModeParams() {};
		SpectatorModeParams(EntityId _entityId, uint8 _mode, EntityId _target, bool _reset, bool _force)
			: entityId(_entityId),
				mode(_mode),
				targetId(_target),
				resetAll(_reset),
				force(_force)
		{
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("entityId", entityId, 'eid');
			ser.Value("mode", mode, 'ui3');
			ser.Value("targetId", targetId, 'eid');
			ser.Value("resetAll", resetAll, 'bool');
			ser.Value("force", force, 'bool');
		}
	};

	struct RenameEntityParams
	{
		EntityId	entityId;
		string		name;

		RenameEntityParams() {};
		RenameEntityParams(EntityId _entityId, const char *name)
			: entityId(_entityId),
				name(name)
		{
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("entityId", entityId, 'eid');
			ser.Value("name", name);
		}
	};

#if defined(DEV_CHEAT_HANDLING)
	struct DevCheatHandlingParams
	{
		DevCheatHandlingParams() {}
		DevCheatHandlingParams(const char * pMessage) { message = pMessage; }

		void SerializeWith(TSerialize ser)
		{
			ser.Value("message", message);
		}

		string message;
	};
#endif

	struct PostInitParams
	{
		PostInitParams() {}
		PostInitParams(const int32 &_timeSinceGameStarted, bool _firstBlood, uint32 _uSecurity) :
			timeSinceGameStarted(_timeSinceGameStarted), uSecurity(_uSecurity), firstBlood(_firstBlood) {}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("timeSinceGameStarted", timeSinceGameStarted, 'i32');
			ser.Value("security", uSecurity, 'i32');
			ser.Value("firstBlood", firstBlood, 'bool');
		}

		int32 timeSinceGameStarted;
		uint32 uSecurity;
		bool firstBlood;
	};

	struct SetGameTimeParams
	{
		CTimeValue time;

		SetGameTimeParams() {};
		SetGameTimeParams(const CTimeValue& _time)
		: time(_time)
		{
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("time", time, 'tnet');
		}
	};

	struct SPlayerEndGameStatsParams
	{
		SPlayerEndGameStatsParams()
			: m_numPlayerStats(0)
		{}

		void SerializeWith(TSerialize ser);

		struct SPlayerEndGameStats
		{
			SPlayerEndGameStats()
				: m_playerId(0)
				, m_points(0)
				, m_kills(0)
				, m_assists(0)
				, m_deaths(0)
				, m_skillPoints(0)
			{}

			void SerializeWith(TSerialize ser);

			EntityId m_playerId;
			int32 m_points;
			uint16 m_kills;
			uint16 m_assists;
			uint16 m_deaths;
			uint16 m_skillPoints;
		};

		static const int k_maxPlayerStats = MAX_PLAYER_LIMIT;
		SPlayerEndGameStats m_playerStats[k_maxPlayerStats];
		int m_numPlayerStats;
	};

	struct VictoryTeamParams
	{
		SPlayerEndGameStatsParams m_playerStats;
		int winningTeamId;
		uint8 reason; // EGameOverReason
		int team1Score;
		int team2Score;
		int drawLevel;
		SDrawResolutionData level1;
		SDrawResolutionData level2;
    EntityId killedEntity;
    EntityId shooterEntity;

		VictoryTeamParams()
			: winningTeamId(0)
			, reason(0)
			, team1Score(0)
			, team2Score(0)
			, drawLevel(0)
			, killedEntity(0)
			, shooterEntity(0)
		{}
		VictoryTeamParams(int _winningTeamId, uint8 _reason, int _team1Score, int _team2Score, int _drawLevel, SDrawResolutionData _level1, SDrawResolutionData _level2, EntityId _killedEntity, EntityId _shooterEntity)
		: winningTeamId(_winningTeamId), reason(_reason), team1Score(_team1Score), team2Score(_team2Score), drawLevel(_drawLevel), level1(_level1), level2(_level2), killedEntity(_killedEntity), shooterEntity(_shooterEntity)
		{
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("team", winningTeamId, 'team');
			ser.Value("reason", reason, 'ui2');
			ser.Value("team1Score", team1Score, 'ui16');
			ser.Value("team2Score", team2Score, 'ui16');
			ser.Value("drawLevel", drawLevel, 'i8');
			m_playerStats.SerializeWith(ser);
			level1.SerializeWith(ser);
			level2.SerializeWith(ser);
      ser.Value("killedEntity", killedEntity, 'eid');
      ser.Value("shooterEntity", shooterEntity, 'eid');
		}
	};

	struct VictoryPlayerParams
	{
		SPlayerEndGameStatsParams m_playerStats;
		EntityId playerId;
    EntityId killedEntity;
    EntityId shooterEntity;
		uint8 reason; // EGameOverReason

		VictoryPlayerParams() {};
		VictoryPlayerParams(EntityId _playerId, EntityId _killedEntity, EntityId _shooterEntity, uint8 _reason)
			: playerId(_playerId), killedEntity(_killedEntity), shooterEntity(_shooterEntity), reason(_reason)
		{
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("playerId", playerId, 'eid');
      ser.Value("killedEntity", killedEntity, 'eid');
      ser.Value("shooterEntity", shooterEntity, 'eid');
			ser.Value("reason", reason, 'ui2');
			m_playerStats.SerializeWith(ser);
		}
	};

	struct KickVoteParams
	{
		EntityId	entityId;
		EntityId	lastVoterId;
		uint8			totalRequiredVotes;
		uint8			votesFor;
		uint8			votesAgainst;
		float			voteEndTime;
		EKickState kickState;

		KickVoteParams()
			: entityId(0)
			, lastVoterId(0)
			, totalRequiredVotes(0)
			, votesFor(0)
			, votesAgainst(0)
			, voteEndTime(0.f)
			, kickState(eKS_None)
		{}
		KickVoteParams(EntityId _entityId, EntityId _lastVoterId, uint8 _totalRequiredVotes, uint8 _votesFor, uint8 _votesAgainst, float _voteEndTime, EKickState _kickState)
			: entityId(_entityId), lastVoterId(_lastVoterId), totalRequiredVotes(_totalRequiredVotes), votesFor(_votesFor), votesAgainst(_votesAgainst), voteEndTime(_voteEndTime), kickState(_kickState)
		{
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("entityId", entityId, 'eid');
			ser.Value("lastVoterId", lastVoterId, 'eid');
			ser.Value("totalRequiredVotes", totalRequiredVotes, 'ui6');
			ser.Value("votesFor", votesFor, 'ui6');
			ser.Value("votesAgainst", votesAgainst, 'ui6');
			ser.Value("voteEndTime", voteEndTime, 'fsec');
			ser.EnumValue("kickState",kickState,eKS_None,eKS_Num);
		}
	};

  struct StartVotingParams
  {
    string        param;
    EntityId      entityId;
    EVotingState  vote_type;
    StartVotingParams(){}
    StartVotingParams(EVotingState st, EntityId id, const char* cmd):vote_type(st),entityId(id),param(cmd){}
    void SerializeWith(TSerialize ser)
    {
      ser.EnumValue("type",vote_type,eVS_none,eVS_last);
      ser.Value("entityId",entityId,'eid');
      ser.Value("param",param);
    }
  };

	struct TwoEntityParams
	{
		EntityId entityId;
		EntityId entityId2;
		TwoEntityParams() {};
		TwoEntityParams(EntityId entId, EntityId entId2)
		: entityId(entId), entityId2(entId2)
		{
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("entityId", entityId, 'eid');
			ser.Value("entityId2", entityId2, 'eid');
		}
	};
	
	struct ServerReviveParams
	{
		EntityId	entityId;
		uint16		index;
		ServerReviveParams() {};
		ServerReviveParams(EntityId entId, uint16 idx, bool initial)	: entityId(entId), index(idx)
		{
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("entityId", entityId, 'eid');
			ser.Value("index", index, 'ui9');
		}
	};

	struct ServerSpectatorParams
	{
		EntityId	entityId;
		uint8 state;
		uint8 mode;

		ServerSpectatorParams() {};
		ServerSpectatorParams(EntityId entId, uint8 _state, uint8 _mode)	: entityId(entId), state(_state), mode(_mode)
		{
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("entityId", entityId, 'eid');
			ser.Value("state", state, 'ui2');
			ser.Value("mode", mode, 'ui3');
		}
	};

	struct EntityParams
	{
		EntityId entityId;
		EntityParams() {};
		EntityParams(EntityId entId)
			: entityId(entId)
		{
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("entityId", entityId, 'eid');
		}
	};

	struct SSuccessfulFlashBangParams
	{
		EntityId shooterId;
		float time;
    float damage;
		SSuccessfulFlashBangParams() {};
		SSuccessfulFlashBangParams(EntityId _shooterId, float _damage, float _time)
			: shooterId(_shooterId)
			, time(_time)
      , damage(_damage)
		{
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("entityId", shooterId, 'eid');
			ser.Value("time", time, 'smal');
      ser.Value("damage", damage, 'dmg');
		}
	};

	// TODO: Roll this and SGameRulesScoreInfo into one structure!
	struct ScoreChangeParams
	{
		EntityId m_killedEntityId;
		TGameRulesScoreInt m_changeToScore;
		EGameRulesScoreType m_type;
		EXPReason						m_reason;
		int									m_currentTeamScore;

		ScoreChangeParams() {};
		ScoreChangeParams(EntityId killedEntId, TGameRulesScoreInt changeToScore, EGameRulesScoreType theType, EXPReason inReason, int currentTeamScore) :
			m_killedEntityId(killedEntId),
			m_changeToScore(changeToScore),
			m_type(theType),
			m_reason(inReason),
			m_currentTeamScore(currentTeamScore)
		{
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("entityId", m_killedEntityId, 'eid');   // 'eid' = special policy for serializing entity IDs
			ser.Value("addToScore", m_changeToScore, 'i16');	// 'i16' = can serialize values in the range -32,768 to -32,767
			ser.Value("type", m_type, 'ui6');                 // 'ui6' = can serialize values in the range 0 to 63
			int	reason=m_reason;
			ser.Value("xpreason",reason,'ui8');
			m_reason=EXPReason(reason);
			ser.Value("currentTeamScore", m_currentTeamScore, 'ui16');
		}
	};

	struct NoParams
	{
		NoParams() {};
		void SerializeWith(const TSerialize& ser) {};
	};


	struct SpawnGroupParams
	{
		EntityId entityId;
		SpawnGroupParams() {};
		SpawnGroupParams(EntityId entId)
			: entityId(entId)
		{
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("entityId", entityId, 'eid');
		}
	};

	struct TempRadarTaggingParams
	{
		EntityId        shooterId;
		EntityId        targetId;
		float			      m_time;
		ERadarTagReason m_reason;
		TempRadarTaggingParams() : shooterId(0), targetId(0), m_time(0.0f), m_reason(eRTR_General) {};
		TempRadarTaggingParams(EntityId shtId, EntityId tgtId, float time, ERadarTagReason reason)
			: shooterId(shtId), targetId(tgtId), m_time(time), m_reason(reason)
		{
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("shooterId", shooterId, 'eid');
			ser.Value("targetId", targetId, 'eid');
			ser.Value("time", m_time, 'nNRG');
			ser.EnumValue("reason",m_reason,eRTR_General,eRTR_Last);
		}
	};

	struct ProcessHitParams
	{
		ProcessHitParams() {};
		ProcessHitParams(EntityId shtId, EntityId wpnId, const Vec3 &inDir, float inDamage, uint16 inProjectileClassId, uint8 inHitTypeId)
				: shooterId(shtId), weaponId(wpnId), dir(inDir), damage(inDamage), projectileClassId(inProjectileClassId), hitTypeId(inHitTypeId) {};

		Vec3 dir;
		EntityId shooterId;
		EntityId weaponId;
		float damage;
		uint16 projectileClassId;
		uint8 hitTypeId;

		void SerializeWith(TSerialize ser)
		{
			ser.Value("shooterId", shooterId, 'eid');
			ser.Value("weaponId", weaponId, 'eid');
			ser.Value("dir", dir, 'dir1');
			ser.Value("damage", damage, 'dmg');
			ser.Value("projectileClassId", projectileClassId, 'ui16');
			ser.Value("hitTypeId", hitTypeId, 'ui8');
		}
	};
	
	// used in the RMI that send movies (trackview) synq info to the client
  struct TSynqMoviesParams
  {
    struct TPlayingMovie
    {
      string    m_Name;
      float     m_fTime;
    };
    
    std::vector<TPlayingMovie>  m_aPlayingMovies;  
    
    TSynqMoviesParams() {}
    
    void SerializeWith(TSerialize ser)
    {
      int iNumPlayingMovies = m_aPlayingMovies.size();
      ser.Value( "Num", iNumPlayingMovies );
      
      m_aPlayingMovies.resize( iNumPlayingMovies );

      for (size_t i=0; i<m_aPlayingMovies.size(); ++i)
      {
        ser.Value( "Name", m_aPlayingMovies[i].m_Name );
        ser.Value( "Time", m_aPlayingMovies[i].m_fTime );
      }
    }
  };


	// used in the RMI that send movies (trackview) synq info to the client
  struct TFinishedOnLoadMoviesParams
  {
    std::vector<string> m_aMovies;
    
    TFinishedOnLoadMoviesParams() {}
    
    void SerializeWith(TSerialize ser)
    {
      int iNumMovies = m_aMovies.size();
      ser.Value( "Num", iNumMovies );
      
      m_aMovies.resize( iNumMovies );
      
      for (size_t i=0; i<m_aMovies.size(); ++i)
      {
        string& s = m_aMovies[i];
        ser.Value( "Name", s );
      }
    }
  };

  struct EquipmentLoadoutParams
  {
		uint8  m_contents[EQUIPMENT_LOADOUT_NUM_SLOTS];
		uint8  m_modelIndex;
		uint8  m_loadoutIndex;

		// Specify which attachments the player has access to
		uint32 m_weaponAttachmentFlags;

		EquipmentLoadoutParams() : m_modelIndex(MP_MODEL_INDEX_DEFAULT), m_loadoutIndex(0)
	  {
			memset(&m_contents,0,sizeof(m_contents));
			m_weaponAttachmentFlags = 0; 
		}

	  void SerializeWith(TSerialize ser)
	  {
			CryFixedStringT<16> name;
			for (int i=0; i<EQUIPMENT_LOADOUT_NUM_SLOTS; ++i)
			{
				name.Format("slot%d", i);
				ser.Value(name.c_str(), m_contents[i], 'ui8');
			}

			ser.Value("loadoutidx", m_loadoutIndex, 'ui4' );
			ser.Value("attachmentflags", m_weaponAttachmentFlags, 'ui32');
			ser.Value("modelIndex", m_modelIndex, MP_MODEL_INDEX_NET_POLICY);
	  }
  };

	struct SModuleRMIEntityParams
	{
		EntityId m_entityId;
		int m_listenerIndex;
		uint8 m_data;

		SModuleRMIEntityParams()
			: m_entityId(0)
			, m_listenerIndex(0)
			, m_data(0)
		{}
		SModuleRMIEntityParams(int listenerIndex, EntityId entId, uint8 data)
				: m_listenerIndex(listenerIndex), m_entityId(entId), m_data(data)
		{
		}

		void Set(int listenerId, EntityId entityId, uint8 data)
		{
			m_listenerIndex = listenerId;
			m_entityId = entityId;
			m_data = data;
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("listenerIndex", m_listenerIndex, 'ui8');
			ser.Value("entityId", m_entityId, 'eid');
			ser.Value("data", m_data, 'ui8');
		}
	};

	struct SModuleRMITwoEntityParams
	{
		EntityId m_entityId1;
		EntityId m_entityId2;
		int m_listenerIndex;
		int m_data;

		SModuleRMITwoEntityParams() {};
		SModuleRMITwoEntityParams(int listenerIndex, EntityId entId1, EntityId entId2, int data)
				: m_listenerIndex(listenerIndex), m_entityId1(entId1), m_entityId2(entId2), m_data(data)
		{
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("listenerIndex", m_listenerIndex, 'ui8');
			ser.Value("entityId1", m_entityId1, 'eid');
			ser.Value("entityId2", m_entityId2, 'eid');
			ser.Value("data", m_data, 'ui8');
		}
	};

	struct SModuleRMIEntityTimeParams
	{
		CTimeValue m_time;
		EntityId m_entityId;
		int m_listenerIndex;
		int m_data;

		SModuleRMIEntityTimeParams() 
			: m_entityId(0)
			, m_listenerIndex(0)
			, m_data(0)
		{};
		SModuleRMIEntityTimeParams(int listenerIndex, EntityId entId, int data, const CTimeValue &time)
				: m_time(time)
				, m_entityId(entId)
				, m_listenerIndex(listenerIndex)
				, m_data(data)
		{
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("listenerIndex", m_listenerIndex, 'ui8');
			ser.Value("entityId", m_entityId, 'eid');
			ser.Value("data", m_data, 'ui8');
			ser.Value("time", m_time, 'tnet');
		}
	};

	struct SHostMigrationClientRequestParams
	{
		SHostMigrationClientRequestParams()
			: m_environmentalWeaponId(0)
			, m_environmentalWeaponRot(IDENTITY)
			, m_environmentalWeaponPos(ZERO)
			, m_environmentalWeaponVel(ZERO)
		{
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("environmentalWeaponId", m_environmentalWeaponId, 'eid');
			ser.Value("environmentalWeaponRot", m_environmentalWeaponRot, 'ori1');
			ser.Value("environmentalWeaponPos", m_environmentalWeaponPos, 'wrld');
			ser.Value("environmentalWeaponVel", m_environmentalWeaponVel, 'vHPs');
		}

		Quat m_environmentalWeaponRot;
		Vec3 m_environmentalWeaponPos;
		Vec3 m_environmentalWeaponVel;

		EntityId m_environmentalWeaponId;
	};

	struct SHostMigrationClientControlledParams
	{
		SHostMigrationClientControlledParams()
		{
			m_pAmmoParams = NULL;
			m_doneEnteredGame = false;
			m_doneSetAmmo = false;
			m_pHolsteredItemClass = NULL;
			m_pSelectedItemClass = NULL;
			m_hasValidVelocity = false;
			m_bInVisorMode = false;
			m_numExpectedItems = 0;
			m_numAmmoParams = 0;
		}

		~SHostMigrationClientControlledParams()
		{
			SAFE_DELETE_ARRAY(m_pAmmoParams);
		}

		bool IsDone()
		{
			return (m_doneEnteredGame && m_doneSetAmmo);
		}

		struct SAmmoParams
		{
			IEntityClass *m_pAmmoClass;
			int m_count;
		};

		Quat m_viewQuat;
		Vec3 m_position;		// Save this since the new server may not have it stored correctly (lag dependent)
		Vec3 m_velocity;
		Vec3 m_aimDirection;

		SAmmoParams *m_pAmmoParams;
		IEntityClass *m_pHolsteredItemClass;
		IEntityClass *m_pSelectedItemClass;

		int m_numAmmoParams;
		int m_numExpectedItems;
		
		bool m_hasValidVelocity;
		bool m_bInVisorMode;

		bool m_doneEnteredGame;
		bool m_doneSetAmmo;
	};

	struct SPredictionParams
	{
		int predictionHandle;

		SPredictionParams() : predictionHandle(0) {}
		SPredictionParams(int hndl) : predictionHandle(hndl) {}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("predictionHandle", predictionHandle, 'phdl');
		}
	};

	struct SMidMigrationJoinParams
	{
		SMidMigrationJoinParams() : m_state(0), m_timeSinceStateChanged(0.f) {}
		SMidMigrationJoinParams(int state, float timeSinceStateChanged) : m_state(state), m_timeSinceStateChanged(timeSinceStateChanged) {}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("state", m_state, 'ui2');
			ser.Value("timeSinceStateChanged", m_timeSinceStateChanged, 'fsec');
		}

		int m_state;
		float m_timeSinceStateChanged;
	};

	struct SModuleRMISvClientActionParams
	{
		typedef uint8  TAction;
		enum
		{
			eACT_NULL = 0,
			eACT_HelperCarry_Pickup,
			eACT_HelperCarry_Drop,
			eACT_Objective_Use,
		};

		union UActionData
		{
			struct SHelperCarryPickup
			{
				EntityId  pickupEid;
			}
			helperCarryPickup;
		};

		UActionData  m_datau;
		int  m_listenerIndex;
		TAction  m_action;

		SModuleRMISvClientActionParams()
		{
			m_listenerIndex = -1;
			m_action = eACT_NULL;
			memset(&m_datau, 0, sizeof(m_datau));
		}

		SModuleRMISvClientActionParams(int listenerIndex, TAction a, const UActionData* datau)
		{
			m_listenerIndex = listenerIndex;
			m_action = a;
			if (datau)
				memcpy(&m_datau, datau, sizeof(m_datau));
			else
				memset(&m_datau, 0, sizeof(m_datau));
			//
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("listenerIndex", m_listenerIndex, 'ui8');
			ser.Value("action", m_action, 'ui2');
			switch (m_action)
			{
			case eACT_HelperCarry_Pickup:		// Fall through
			case eACT_Objective_Use:				//
				ser.Value("pickup_pickupEid", m_datau.helperCarryPickup.pickupEid, 'eid');
				break;
			}
		}
	};

	// for clients to send award workings to server
	struct SAfterMatchAwardWorkingsParams
	{
		static const uint8 k_maxNumAwards=100;
		// TODO add support for ints as well
		struct SWorkingValue
		{
			uint8 m_award;
			float m_workingValue;
		};
		SWorkingValue m_awards[k_maxNumAwards];
		uint8 m_numAwards;
		EntityId m_playerEntityId;

		SAfterMatchAwardWorkingsParams()
		{
			m_numAwards=0;
			memset(m_awards, 0, sizeof(m_awards));
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("playerEntityId", m_playerEntityId, 'eid');
			ser.Value("numAwards", m_numAwards, 'ui8');

			for (int i=0; i<m_numAwards; i++)
			{
				ser.Value("award", m_awards[i].m_award, 'ui8');
				ser.Value("workingValue", m_awards[i].m_workingValue);	// no policy, no compression for now.
			}
		}
	};

	// for the server to send awards to the clients that have won them
	struct SAfterMatchAwardsParams
	{
		static const uint8 k_maxNumAwards=64;
		uint8 m_awards[k_maxNumAwards];
		uint8 m_numAwards;

		SAfterMatchAwardsParams()
		{
			m_numAwards=0;
			memset(m_awards, 0, sizeof(m_awards));
		}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("numAwards", m_numAwards, 'ui8');

			for (int i=0; i<m_numAwards; i++)
			{
				ser.Value("award", m_awards[i], 'ui8');
			}
		}
	};

	struct SRespawnUpdateParams
	{
		int32 m_respawnHashId;				// Send as a int32 not a uint32 because of a bug in the compression policy
		EntityId m_respawnEntityId;

		void SerializeWith(TSerialize ser)
		{
			ser.Value("respawnHashId", m_respawnHashId, 'i32');
			ser.Value("respawnEntityId", m_respawnEntityId, 'eid');
		}
	};

	struct STrackViewParameters
	{
		static const uint8 sMaxTrackViews=16;

		STrackViewParameters()
		{
			m_NumberOfTrackViews = 0;
			m_NumberOfFinishedTrackViews = 0;
			m_bInitialData = false;

			for (uint8 i = 0; i < sMaxTrackViews; ++i)
			{
				m_Times[i] = 0.0f;
				m_Ids[i] = 0;
			}
		}

		uint8 m_NumberOfTrackViews;
		uint8 m_NumberOfFinishedTrackViews;
		float m_Times[sMaxTrackViews];
		int m_Ids[sMaxTrackViews];
		bool m_bInitialData; //When a client first joins

		void SerializeWith(TSerialize ser)
		{
			if(ser.IsWriting())
			{
				if(m_NumberOfTrackViews + m_NumberOfFinishedTrackViews > sMaxTrackViews)
				{
					m_NumberOfFinishedTrackViews = sMaxTrackViews - m_NumberOfTrackViews;
				}
			}

			ser.Value("numberOfTrackViews", m_NumberOfTrackViews, 'ui5');
			ser.Value("numberOfFinishedTrackViews", m_NumberOfFinishedTrackViews, 'ui5');

			int totalTrackViews = m_NumberOfTrackViews + m_NumberOfFinishedTrackViews;
			for(int i = 0; i < totalTrackViews; ++i)
			{
				ser.Value("trackViewId", m_Ids[i], 'i32');
				// we need times on all of them now that aborted times may be included
				//if(i < m_NumberOfTrackViews) //Finished track views don't need a time - the client can get the length themselves 
				{
					ser.Value("timeValue", m_Times[i], 'hPrs');
				}
			}

			ser.Value("m_bInitialData", m_bInitialData, 'bool');
		}
	};

	struct STrackViewRequestParameters
	{
		STrackViewRequestParameters()
		{
			m_TrackViewID = 0;
		}

		int m_TrackViewID;

		void SerializeWith(TSerialize ser)
		{
			ser.Value("trackViewId", m_TrackViewID, 'i32');
		}
	};

	struct ActivateHitIndicatorParams
	{
		ActivateHitIndicatorParams() {};
		ActivateHitIndicatorParams(Vec3 origin) : originPos(origin) {};

		void SerializeWith(TSerialize ser)
		{
			ser.Value("originPos", originPos, 'wrld');
		}

		Vec3 originPos;
	};
	
#if USE_PC_PREMATCH
	struct StartingPrematchCountDownParams
	{
		StartingPrematchCountDownParams (){};
		StartingPrematchCountDownParams(float timerLength) : m_timerLength(timerLength) {};

		void SerializeWith(TSerialize ser)
		{
			ser.Value("timerLength", m_timerLength, 'fsec');
		}

		float m_timerLength;
	};
#endif

	DECLARE_SERVER_RMI_URGENT(SvRequestHit, HitInfo, eNRT_ReliableUnordered);
	DECLARE_CLIENT_RMI_NOATTACH(ClExplosion, ExplosionInfo, eNRT_ReliableUnordered);
	DECLARE_CLIENT_RMI_URGENT(ClProjectileExplosion, SProjectileExplosionParams, eNRT_ReliableUnordered);
	DECLARE_CLIENT_RMI_NOATTACH(ClProjectileExplosion_Impact, SProjectileExplosionParams_Impact, eNRT_ReliableUnordered);
	
	DECLARE_CLIENT_RMI_NOATTACH(ClTaggedEntity, TempRadarTaggingParams, eNRT_ReliableUnordered);
	DECLARE_SERVER_RMI_NOATTACH(SvRequestTagEntity, TempRadarTaggingParams, eNRT_ReliableUnordered);

	DECLARE_SERVER_RMI_NOATTACH(SvRequestRename, RenameEntityParams, eNRT_ReliableOrdered);
	DECLARE_CLIENT_RMI_NOATTACH(ClRenameEntity, RenameEntityParams, eNRT_ReliableOrdered);

#if defined(DEV_CHEAT_HANDLING)
	DECLARE_CLIENT_RMI_NOATTACH(ClHandleCheatAccusation, DevCheatHandlingParams, eNRT_ReliableUnordered);

	void HandleDevCheat(uint16 channelId, const char * message);
#endif

	DECLARE_SERVER_RMI_NOATTACH(SvRequestChangeTeam, ChangeTeamParams, eNRT_ReliableOrdered);
	DECLARE_SERVER_RMI_NOATTACH(SvRequestSpectatorMode, SpectatorModeParams, eNRT_ReliableOrdered);
	DECLARE_CLIENT_RMI_NOATTACH(ClTeamFull, UInt8Param, eNRT_ReliableOrdered);
	DECLARE_CLIENT_RMI_NOATTACH(ClSetTeam, SetTeamParams, eNRT_ReliableOrdered);
	DECLARE_CLIENT_RMI_NOATTACH(ClTextMessage, TextMessageParams, eNRT_ReliableUnordered);

	DECLARE_CLIENT_RMI_NOATTACH(ClProcessHit, ProcessHitParams, eNRT_UnreliableUnordered);

	DECLARE_CLIENT_RMI_NOATTACH(ClPostInit, PostInitParams, eNRT_ReliableUnordered);
	DECLARE_CLIENT_RMI_NOATTACH(ClSetGameStartedTime, SetGameTimeParams, eNRT_ReliableUnordered);
	DECLARE_CLIENT_RMI_NOATTACH(ClSetGameStartTimer, SetGameTimeParams, eNRT_ReliableUnordered);

	DECLARE_SERVER_RMI_NOATTACH(SvVote, NoParams, eNRT_ReliableUnordered);
	DECLARE_SERVER_RMI_NOATTACH(SvVoteNo, NoParams, eNRT_ReliableUnordered);
  DECLARE_SERVER_RMI_NOATTACH(SvStartVoting, StartVotingParams, eNRT_ReliableUnordered);
	DECLARE_CLIENT_RMI_NOATTACH(ClKickVoteStatus, KickVoteParams, eNRT_ReliableUnordered);

	DECLARE_CLIENT_RMI_NOATTACH(ClEnteredGame, NoParams, eNRT_ReliableUnordered);
	
	DECLARE_CLIENT_RMI_NOATTACH( ClVictoryTeam,           VictoryTeamParams,            eNRT_ReliableOrdered );
	DECLARE_CLIENT_RMI_NOATTACH( ClVictoryPlayer,         VictoryPlayerParams,          eNRT_ReliableOrdered );

	DECLARE_CLIENT_RMI_NOATTACH(ClAddPoints,      	      ScoreChangeParams,            eNRT_ReliableUnordered);
	DECLARE_SERVER_RMI_NOATTACH(SvRequestRevive,					ServerReviveParams, eNRT_ReliableOrdered);
	DECLARE_SERVER_RMI_NOATTACH(SvSetSpectatorState,			ServerSpectatorParams, eNRT_ReliableOrdered);
	
	DECLARE_SERVER_RMI_NOATTACH( SvSetEquipmentLoadout,EquipmentLoadoutParams,       eNRT_ReliableOrdered );

	DECLARE_CLIENT_RMI_NOATTACH( ClModuleRMISingleEntity,	SModuleRMIEntityParams,			eNRT_ReliableOrdered );
	DECLARE_CLIENT_RMI_NOATTACH( ClModuleRMIDoubleEntity,	SModuleRMITwoEntityParams,		eNRT_ReliableOrdered );
	DECLARE_CLIENT_RMI_NOATTACH( ClModuleRMIEntityWithTime,	SModuleRMIEntityTimeParams,		eNRT_ReliableOrdered );
	DECLARE_SERVER_RMI_NOATTACH( SvModuleRMISingleEntity,		SModuleRMIEntityParams,							eNRT_ReliableOrdered );
	DECLARE_CLIENT_RMI_NOATTACH( ClVehicleDestroyed,					SVehicleDestroyedParams, eNRT_ReliableUnordered );

#ifdef INCLUDE_GAME_AI_RECORDER_NET
	DECLARE_SERVER_RMI_NOATTACH(SvRequestRecorderBookmark, NoParams, eNRT_ReliableUnordered);
	DECLARE_SERVER_RMI_NOATTACH(SvRequestRecorderComment, StringParams, eNRT_ReliableUnordered);
#endif //INCLUDE_GAME_AI_RECORDER_NET

	DECLARE_SERVER_RMI_NOATTACH(SvSuccessfulFlashBang, SSuccessfulFlashBangParams, eNRT_ReliableUnordered);

	DECLARE_CLIENT_RMI_NOATTACH(ClNetConsoleCommand, NetConsoleCommandParams, eNRT_ReliableUnordered);

	DECLARE_SERVER_RMI_NOATTACH(SvHostMigrationRequestSetup, SHostMigrationClientRequestParams, eNRT_ReliableUnordered);
	DECLARE_CLIENT_RMI_NOATTACH(ClHostMigrationFinished, NoParams, eNRT_ReliableOrdered);
	DECLARE_CLIENT_RMI_NOATTACH(ClMidMigrationJoin, SMidMigrationJoinParams, eNRT_ReliableOrdered);
	DECLARE_CLIENT_RMI_NOATTACH(ClHostMigrationPlayerJoined, EntityParams, eNRT_ReliableOrdered);

	DECLARE_CLIENT_RMI_NOATTACH(ClPredictionFailed, SPredictionParams, eNRT_ReliableUnordered);

	DECLARE_SERVER_RMI_NOATTACH(SvModuleRMIOnAction, SModuleRMISvClientActionParams, eNRT_ReliableOrdered);

	DECLARE_SERVER_RMI_NOATTACH(SvAfterMatchAwardsWorking, SAfterMatchAwardWorkingsParams, eNRT_ReliableUnordered);
	DECLARE_CLIENT_RMI_NOATTACH(ClAfterMatchAwards, SAfterMatchAwardsParams, eNRT_ReliableUnordered);

	DECLARE_CLIENT_RMI_NOATTACH(ClUpdateRespawnData, SRespawnUpdateParams, eNRT_ReliableUnordered);

	DECLARE_CLIENT_RMI_NOATTACH( ClTrackViewSynchAnimations, STrackViewParameters, eNRT_ReliableOrdered );
	DECLARE_SERVER_RMI_NOATTACH( SvTrackViewRequestAnimation, STrackViewRequestParameters, eNRT_ReliableOrdered );

	DECLARE_CLIENT_RMI_NOATTACH(ClPathFollowingAttachToPath, SPathFollowingAttachToPathParameters, eNRT_ReliableUnordered);

	DECLARE_CLIENT_RMI_NOATTACH(ClActivateHitIndicator, ActivateHitIndicatorParams, eNRT_ReliableUnordered);
	
#if USE_PC_PREMATCH
	DECLARE_CLIENT_RMI_NOATTACH(ClStartingPrematchCountDown, StartingPrematchCountDownParams, eNRT_ReliableUnordered);
#endif

	void AddGameRulesListener(SGameRulesListener* pRulesListener);
	void RemoveGameRulesListener(SGameRulesListener* pRulesListener);

	bool IsGameInProgress() const;
	bool HUDScoreElementTimerShouldCountDown() const;

	struct STeamScore
	{
		STeamScore() {};
		STeamScore(uint16 teamScore, uint16 roundTeamScore)
			: m_teamScore(teamScore), m_roundTeamScore(roundTeamScore), m_teamScoreRoundStart(teamScore) {}
		STeamScore(uint16 teamScore, uint16 roundTeamScore, uint16 teamScoreRoundStart )
			: m_teamScore(teamScore), m_roundTeamScore(roundTeamScore), m_teamScoreRoundStart(teamScoreRoundStart) {}
		uint16						m_teamScore;
		uint16						m_roundTeamScore;
		uint16						m_teamScoreRoundStart;
	};

	typedef std::map<int, EntityId>				TTeamIdEntityIdMap;
	typedef std::map<EntityId, int>				TEntityTeamIdMap;
	typedef std::map<int, TPlayers>				TPlayerTeamIdMap;
	typedef std::map<int, EntityId>				TChannelTeamIdMap;
	typedef std::map<string, int>				TTeamIdMap;
	typedef std::map<int, STeamScore>			TTeamScoresMap;
	typedef std::map<int, int>						THitMaterialMap;
	typedef std::vector<HitTypeInfo>			THitTypeVec;

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	typedef std::map<int, _smart_ptr<IVoiceGroup> >		TTeamIdVoiceGroupMap;
#endif

	struct SCollisionHitInfo
	{
		// Unless specified, every field not using the prefix "target" refers to the source
		Vec3		pos;
		Vec3		normal;
		Vec3		dir;
		Vec3		velocity;

		Vec3		target_velocity;
		float		target_mass;
		float		mass;
		EntityId	targetId;
		pe_type		target_type;

		int			materialId;
		int			partId;
		bool		dir_null;
		bool		backface;

		SCollisionHitInfo()
			: pos(ZERO),
			normal(ZERO),
			dir(ZERO),
			velocity(ZERO),
			target_velocity(ZERO),
			target_mass(0.f),
			mass(0.f),
			targetId(0),
			target_type(PE_NONE),
			materialId(0),
			partId(0),
			backface(false),
			dir_null(false)
		{}
	};

	struct SEntityRespawnData
	{
		SmartScriptTable	properties;
		Vec3							position;
		Quat							rotation;
		Vec3							scale;
		int								flags;
		IEntityClass			*pClass;

		CryHashStringId		m_nameHash;
		EntityId					m_currentEntityId;
		bool							m_bHasRespawned;

		void GetMemoryUsage( ICrySizer *pSizer ) const { /*nothing*/ }
	};

	struct SEntityRespawn
	{
		bool							unique;
		float							timer;

		void GetMemoryUsage( ICrySizer *pSizer ) const { /*nothing*/ }
	};

	struct SEntityRemovalData
	{
		float							timer;
		float							time;
		bool							visibility;

		void GetMemoryUsage( ICrySizer *pSizer ) const { /*nothing*/ }
	};

#if USE_PC_PREMATCH
	enum EPrematchState
	{
		ePS_Prematch = 0,
		ePS_PrematchWaitingForPlayers,
		ePS_Countdown,
		ePS_Match,
		ePS_None,
		ePS_Last = ePS_None
	};
#endif

	typedef std::vector<SEntityRespawnData>					TEntityRespawnDataVec;
	typedef std::map<EntityId, SEntityRespawn>			TEntityRespawnMap;
	typedef std::map<EntityId, SEntityRemovalData>	TEntityRemovalMap;

	typedef std::vector<IHitListener*> THitListenerVec;

	SEntityRespawnData *GetEntityRespawnData(EntityId entityId);
	SEntityRespawnData *GetEntityRespawnDataByHashId(CryHashStringId nameHashId);

protected:
	static void CmdDebugTeams(IConsoleCmdArgs *pArgs);
	static void CmdGiveScore(IConsoleCmdArgs *pArgs);

	bool NetSerializeTelemetry( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );

#if USE_PC_PREMATCH
public:
	void PrematchRespawn();
	void StartPrematch();
	void SkipPrematch();

private:
	void ChangePrematchState(EPrematchState newState);
	void ForceBalanceTeams();
#endif

public:
	static const char** S_GetGameModeNamesArray() { return s_gameModeNames; }
	ILINE EGameMode GetGameMode() const { return m_gameMode; }

	void CreateScriptHitInfo(SmartScriptTable &scriptHitInfo, const HitInfo &hitInfo) const;
	static void CreateHitInfoFromScript(const SmartScriptTable &scriptHitInfo, HitInfo &hitInfo);
	
	void SuccessfulFlashBang(const ExplosionInfo &explosionInfo, float time);

	void SetPendingLoadoutChange();

	void UpdateGameRulesCvars();
	void ClearEntityTeams();
	void ClearRemoveEntityEventListeners();

	void InitSessionStatistics();
	void SaveSessionStatistics(float delay = 0.f);

#ifdef _DEBUG
	static inline bool DbgSetAssertOnFailureToFindHitType(bool onOff)
	{
		bool old = s_dbgAssertOnFailureToFindHitType;
		s_dbgAssertOnFailureToFindHitType = onOff;
		return old;
	}
#endif

protected:
	void UpdateAffectedEntitiesSet(TExplosionAffectedEntities &affectedEnts, const pe_explosion& explosion);
	void AddOrUpdateAffectedEntity(TExplosionAffectedEntities &affectedEnts, IEntity* pEntity, float affected);
	void RemoveFriendlyAffectedEntities(const ExplosionInfo &explosionInfo, TExplosionAffectedEntities &affectedEntities);
	void ChatLog(EChatMessageType type, EntityId sourceId, EntityId targetId, const char *msg);
	void KnockBackPendingActors();
	
	bool	IsGamemodeScoringEvent(EGameRulesScoreType pointsType) const;

	void PrepCollision(int src, int trg, const SGameCollision& event, IEntity* pTarget, SCollisionHitInfo &result);

	void CallScript(IScriptTable *pScript, const char *name)
	{
		if (!pScript || pScript->GetValueType(name) != svtFunction)
			return;
		m_pScriptSystem->BeginCall(pScript, name); m_pScriptSystem->PushFuncParam(m_script);
		m_pScriptSystem->EndCall();
	};
	template<typename P1>
	void CallScript(IScriptTable *pScript, const char *name, const P1 &p1)
	{
		if (!pScript || pScript->GetValueType(name) != svtFunction)
			return;
		m_pScriptSystem->BeginCall(pScript, name); m_pScriptSystem->PushFuncParam(m_script);
		m_pScriptSystem->PushFuncParam(p1);
		m_pScriptSystem->EndCall();
	};
	template<typename P1, typename P2>
	void CallScript(IScriptTable *pScript, const char *name, const P1 &p1, const P2 &p2)
	{
		if (!pScript || pScript->GetValueType(name) != svtFunction)
			return;
		m_pScriptSystem->BeginCall(pScript, name); m_pScriptSystem->PushFuncParam(m_script);
		m_pScriptSystem->PushFuncParam(p1); m_pScriptSystem->PushFuncParam(p2);
		m_pScriptSystem->EndCall();
	};
	template<typename P1, typename P2, typename P3>
	void CallScript(IScriptTable *pScript, const char *name, const P1 &p1, const P2 &p2, const P3 &p3)
	{
		if (!pScript || pScript->GetValueType(name) != svtFunction)
			return;
		m_pScriptSystem->BeginCall(pScript, name); m_pScriptSystem->PushFuncParam(m_script);
		m_pScriptSystem->PushFuncParam(p1); m_pScriptSystem->PushFuncParam(p2); m_pScriptSystem->PushFuncParam(p3);
		m_pScriptSystem->EndCall();
	};
	template<typename P1, typename P2, typename P3, typename P4>
	void CallScript(IScriptTable *pScript, const char *name, const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4)
	{
		if (!pScript || pScript->GetValueType(name) != svtFunction)
			return;
		m_pScriptSystem->BeginCall(pScript, name); m_pScriptSystem->PushFuncParam(m_script);
		m_pScriptSystem->PushFuncParam(p1); m_pScriptSystem->PushFuncParam(p2); m_pScriptSystem->PushFuncParam(p3); m_pScriptSystem->PushFuncParam(p4);
		m_pScriptSystem->EndCall();
	};
	template<typename P1, typename P2, typename P3, typename P4, typename P5>
	void CallScript(IScriptTable *pScript, const char *name, const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4, const P5 &p5)
	{
		if (!pScript || pScript->GetValueType(name) != svtFunction)
			return;
		m_pScriptSystem->BeginCall(pScript, name); m_pScriptSystem->PushFuncParam(m_script);
		m_pScriptSystem->PushFuncParam(p1); m_pScriptSystem->PushFuncParam(p2); m_pScriptSystem->PushFuncParam(p3); m_pScriptSystem->PushFuncParam(p4); m_pScriptSystem->PushFuncParam(p5);
		m_pScriptSystem->EndCall();
	};
	template<typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
	void CallScript(IScriptTable *pScript, const char *name, P1 &p1, P2 &p2, P3 &p3, P4 &p4, P5 &p5, P6 &p6)
	{
		if (!pScript || pScript->GetValueType(name) != svtFunction)
			return;
		m_pScriptSystem->BeginCall(pScript, name); m_pScriptSystem->PushFuncParam(m_script);
		m_pScriptSystem->PushFuncParam(p1); m_pScriptSystem->PushFuncParam(p2); m_pScriptSystem->PushFuncParam(p3); m_pScriptSystem->PushFuncParam(p4); m_pScriptSystem->PushFuncParam(p5); m_pScriptSystem->PushFuncParam(p6);
		m_pScriptSystem->EndCall();
	};

public:
	static const char* s_reservedHitTypes[];
	static const char* s_hitTypeFlags[];

protected:
	static const char* s_gameModeNames[];
	EGameMode m_gameMode;

	IGameFramework			*m_pGameFramework;
	IGameplayRecorder		*m_pGameplayRecorder;
	ISystem							*m_pSystem;
	IActorSystem				*m_pActorSystem;
	IEntitySystem				*m_pEntitySystem;
	IScriptSystem				*m_pScriptSystem;
	IMaterialManager		*m_pMaterialManager;
	SmartScriptTable		m_script;
	SmartScriptTable		m_clientScript;
	SmartScriptTable		m_serverScript;
	SmartScriptTable		m_clientStateScript;
	SmartScriptTable		m_serverStateScript;

	INetChannel					*m_pClientNetChannel;

	std::vector<int>		m_channelIds;
	
	TTeamIdMap					m_teams;
	TTeamIdMap					m_teamAliases;
	TEntityTeamIdMap		m_entityteams;
	TPlayerTeamIdMap		m_playerteams;
	TChannelTeamIdMap		m_channelteams;
	TTeamScoresMap			m_teamscores;
	int									m_teamIdGen;

	THitTypeVec					m_hitTypes;
	int									m_hitTypeIdGen;
 
	SmartScriptTable		m_scriptClientHitInfo;

	typedef std::queue<SExplosionContainer*>	TExplosionPtrQueue;
	TExplosionPtrQueue		m_queuedExplosions;
	TExplosionPtrQueue		m_queuedExplosionsAwaitingRaycasts;
	SExplosionContainer		m_explosions[MAX_CONCURRENT_EXPLOSIONS];
	bool					m_explosionValidities[MAX_CONCURRENT_EXPLOSIONS];

	typedef std::queue<HitInfo> THitQueue;
	THitQueue						m_queuedHits;
	int									m_processingHit;

	TEntityRespawnDataVec	m_respawndata;
	TEntityRespawnMap			m_respawns;
	TEntityRemovalMap			m_removals;

	TSpawnGroupMap			m_spawnGroups;

	THitListenerVec     m_hitListeners;

	CTimeValue					m_gameStartedTime;	// time the game started at.
	CTimeValue					m_gameStartTime; // time for game start, <= 0 means game started already
	CTimeValue					m_gamePausedTime; // Time game went into postmatch - 0.0f if not happened yet. Set locally on game end, if time starts to be too inaccurate across client/server consider net syncing
	CTimeValue					m_cachedServerTime; // server time as of the last call to CGameRules::Update(...)
	CTimeValue					m_hostMigrationTimeSinceGameStarted;
	CTimeValue					m_timeLastShownUnbalancedTeamsWarning;
	float						m_timeLimit;
	int							m_scoreLimit;
	int							m_scoreLimitOverride;
	int							m_roundLimit;
	bool						m_votingEnabled;
	int							m_votingCooldown;
	int							m_votingMinVotes;
	float						m_votingRatio;

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	TTeamIdVoiceGroupMap	m_teamVoiceGroups;
#endif

  CVotingSystem       *m_pVotingSystem;
	CBattlechatter      *m_pBattlechatter;
	CAreaAnnouncer			*m_pAreaAnnouncer;
	CMiscAnnouncer			*m_pMiscAnnouncer;
	CExplosionGameEffect	*m_pExplosionGameEffect;

	CVTOLVehicleManager  *m_pVTOLVehicleManager;
	CCorpseManager*				m_pCorpseManager;
	CAnnouncer*						m_pAnnouncer;
	CClientHitEffectsMP*	m_pClientHitEffectsMP;
	CTeamVisualizationManager* m_pTeamVisualizationManager;

	CMPTrackViewManager*	m_mpTrackViewManager;
	CMPPathFollowingManager* m_mpPathFollowingManager;

	TGameRulesListenerVec	m_rulesListeners;

#ifdef _DEBUG
	static bool         s_dbgAssertOnFailureToFindHitType;
#endif

	EntityId					  m_ignoreEntityNextCollision;

	bool                m_timeOfDayInitialized;
	
	std::vector<EntityId> m_pendingActorsToBeKnockedDown;
 
  // constants used in the movie synq code
  enum 
  {	
    NUM_FRAMES_CHECKING_MOVIES_SYNQ = 20,       // how many frames the client keep checking for stalls after load
    FRAME_TIME_FOR_MOVIES_SYNQ_TRESHOLD = 400   // (ms). a frame with a delta time bigger than this makes the client to request a synq to the server
  };
    
	CCinematicInput			m_cinematicInput;
	
	// Used to store the pertinent details of migrating player entities so they
	// can be reconstructed as close as possible to their state prior to migration
	SMigratingPlayerInfo* m_pMigratingPlayerInfo;
	uint32 m_migratingPlayerMaxCount;

	static const int MAX_PLAYERS = MAX_PLAYER_LIMIT;
	TNetChannelID m_migratedPlayerChannels[MAX_PLAYERS];

	SHostMigrationClientRequestParams* m_pHostMigrationParams;
	SHostMigrationClientControlledParams* m_pHostMigrationClientParams;

	bool m_bPendingLoadoutChange;
	bool m_levelLoaded;
	bool m_hasWinningKill;
	bool m_sessionStatisticsSaved;
	bool m_bIsTeamGame;

	bool m_bClientKickVoteActive; // This is only used by clients to know whether to send voting messages
	bool m_bClientKickVoteSent;   // This is only used by clients to know whether to send voting messages
	bool m_bClientKickVotedFor;   // Remember what we voted for so it knows what to show when the server confirms voting
	CTimeValue m_ClientCooldownEndTime;

// Define pointers and accessor functions for each module type
#define GAMERULES_MODULE_LIST_FUNC(type, name, lowerCase, useInEditor) \
	protected:	\
		type *m_##lowerCase##Module;	\

	GAMERULES_MODULE_TYPES_LIST

#undef GAMERULES_MODULE_LIST_FUNC

public:
	IGameRulesTeamsModule*							GetTeamsModule()							{ return m_teamsModule; }
	IGameRulesStateModule*							GetStateModule()							{ return m_stateModule; }
	IGameRulesVictoryConditionsModule*	GetVictoryConditionsModule()	{ return m_victoryConditionsModule; }
	IGameRulesPlayerSetupModule*				GetPlayerSetupModule()				{ return m_playerSetupModule; }
	IGameRulesScoringModule*						GetScoringModule()						{ return m_scoringModule; }
	IGameRulesAssistScoringModule*			GetAssistScoringModule()			{ return m_assistScoringModule; }
	IGameRulesPlayerStatsModule*				GetPlayerStatsModule()				{ return m_playerStatsModule; }
	IGameRulesSpawningModule*						GetSpawningModule()						{ return m_spawningModule; }
	IGameRulesDamageHandlingModule*			GetDamageHandlingModule()			{ return m_damageHandlingModule; }
	IGameRulesActorActionModule*				GetActorActionModule()				{ return m_actorActionModule; }
	IGameRulesSpectatorModule*					GetSpectatorModule()					{ return m_spectatorModule; }
	IGameRulesObjectivesModule*					GetObjectivesModule()					{ return m_objectivesModule; }
	IGameRulesRoundsModule*							GetRoundsModule()							{ return m_roundsModule; }
	IGameRulesStatsRecording*						GetStatsRecordingModule()			{ return m_statsRecordingModule; }
	CMPTrackViewManager*								GetMPTrackViewManager()				{ return m_mpTrackViewManager; }
	CMPPathFollowingManager*						GetMPPathFollowingManager()		{ return m_mpPathFollowingManager; }

	typedef std::vector<EntityId> TEntityIdVec;
	TEntityIdVec m_entityEventDoneListeners;

protected:
	typedef std::vector<IGameRulesPickupListener*> TPickupListenersVec;
	typedef std::vector<IGameRulesClientConnectionListener*> TClientConnectionListenersVec;
	typedef std::vector<IGameRulesTeamChangedListener*> TTeamChangedListenersVec;
	typedef std::vector<IGameRulesKillListener*> TKillListenersVec;
	typedef std::vector<IGameRulesModuleRMIListener*> TModuleRMIListenersVec;
	typedef std::vector<IGameRulesRevivedListener*> TRevivedListenersVec;
	typedef std::vector<IGameRulesSurvivorCountListener*> TSurvivorCountListenersVec;
	typedef std::vector<IGameRulesPlayerStatsListener*> TPlayerStatsListenersVec;
	typedef std::vector<IGameRulesRoundsListener*> TRoundsListenersVec;
	typedef std::vector<IGameRulesClientScoreListener*> TClientScoreListenersVec;
	typedef std::vector<IGameRulesActorActionListener*> TActorActionListenersVec;
	TPickupListenersVec m_pickupListeners;
	TClientConnectionListenersVec m_clientConnectionListeners;
	TTeamChangedListenersVec m_teamChangedListeners;
	TKillListenersVec m_killListeners;
	TModuleRMIListenersVec m_moduleRMIListenersVec;
	TRevivedListenersVec m_revivedListenersVec;
	TSurvivorCountListenersVec m_survivorCountListenersVec;
	TPlayerStatsListenersVec m_playerStatsListenersVec;
	TRoundsListenersVec m_roundsListenersVec;
	TClientScoreListenersVec m_clientScoreListenersVec;
	TActorActionListenersVec m_actorActionListenersVec;
	float m_idleTime;
	CAudioSignalPlayer m_explosionFeedback;
	CAudioSignalPlayer m_explosionSoundmoodEnter;
	CAudioSignalPlayer m_explosionSoundmoodExit;
	
#if USE_PC_PREMATCH
	typedef std::vector<IGameRulesPrematchListener*> TPrematchListenersVec;
	TPrematchListenersVec m_prematchListenersVec;
#endif // USE_PC_PREMATCH

public:
	void RegisterPickupListener(IGameRulesPickupListener *pListener);
	void UnRegisterPickupListener(IGameRulesPickupListener *pListener);

	void RegisterClientConnectionListener(IGameRulesClientConnectionListener *pListener);
	void UnRegisterClientConnectionListener(IGameRulesClientConnectionListener *pListener);

	void RegisterTeamChangedListener(IGameRulesTeamChangedListener *pListener);
	void UnRegisterTeamChangedListener(IGameRulesTeamChangedListener *pListener);

	void RegisterRevivedListener( IGameRulesRevivedListener *pListener );
	void UnRegisterRevivedListener( IGameRulesRevivedListener *pListener );

	void RegisterSurvivorCountListener( IGameRulesSurvivorCountListener *pListener );
	void UnRegisterSurvivorCountListener( IGameRulesSurvivorCountListener *pListener );

	void RegisterPlayerStatsListener( IGameRulesPlayerStatsListener *pListener );
	void UnRegisterPlayerStatsListener( IGameRulesPlayerStatsListener *pListener );

	void RegisterRoundsListener( IGameRulesRoundsListener *pListener );
	void UnRegisterRoundsListener( IGameRulesRoundsListener *pListener );

#if USE_PC_PREMATCH
	void RegisterPrematchListener( IGameRulesPrematchListener *pListener );
	void UnRegisterPrematchListener( IGameRulesPrematchListener *pListener );
#endif // USE_PC_PREMATCH

	void RegisterClientScoreListener( IGameRulesClientScoreListener *pListener );
	void UnRegisterClientScoreListener( IGameRulesClientScoreListener *pListener );

	void RegisterActorActionListener( IGameRulesActorActionListener *pListener );
	void UnRegisterActorActionListener( IGameRulesActorActionListener *pListener );

	void RegisterKillListener(IGameRulesKillListener *pKillsListener);
	void UnRegisterKillListener(IGameRulesKillListener *pKillsListener);

	int RegisterModuleRMIListener(IGameRulesModuleRMIListener *pRMIListener);
	void UnRegisterModuleRMIListener(int index);

	void OwnClientConnected_NotifyListeners();

	void OnEntityKilledEarly( const HitInfo &hitInfo );
	void OnEntityKilled(const HitInfo &hitInfo);

	void CallEntityScriptFunction(EntityId entityId, const char *functionName)
	{
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(entityId);
		if (pEntity)
		{
			IScriptTable *pScript = pEntity->GetScriptTable();
			if ((pScript != NULL) && pScript->GetValueType(functionName) == svtFunction)
			{
				m_pScriptSystem->BeginCall(pScript, functionName);
				m_pScriptSystem->PushFuncParam(pScript);
				m_pScriptSystem->EndCall();
			}
		}
	};

	const char *GetEntityName(EntityId inEntityId) const
	{
		IEntity *entity = gEnv->pEntitySystem->GetEntity(inEntityId);
		if (entity)
		{
			return entity->GetName();
		}
		
		return "<NULL>";
	}
	
	const SHostMigrationClientControlledParams* GetHostMigrationClientParams()
	{
		return m_pHostMigrationClientParams;
	}
	
  // TODO TEMP until game modules can send/recv RMIs (or we decide how we're going to do networks)
	void SendRMI_SvRequestSpectatorMode(EntityId entityId, uint8 mode, EntityId target, bool reset, unsigned where, bool force = false)
	{
		SpectatorModeParams  params (entityId, mode, target, reset, force);
		GetGameObject()->InvokeRMIWithDependentObject(SvRequestSpectatorMode(), params, where, params.entityId);
	}	

	ILINE void SanityCheckHitInfo(const HitInfo &hitInfo, const char * funcName)
	{
		SanityCheckHitData(hitInfo.dir, hitInfo.shooterId, hitInfo.targetId, hitInfo.weaponId, hitInfo.type, funcName);
	}

#if !defined(_RELEASE)
	void SanityCheckHitData(const Vec3 & dir, EntityId shooter, EntityId target, EntityId weapon, uint16 hitType, const char * funcName);
#else
	ILINE void SanityCheckHitData(const Vec3 & dir, EntityId shooter, EntityId target, EntityId weapon, uint16 hitType, const char * funcName) {}
#endif

	void AddForbiddenArea(EntityId entityId);
	void RemoveForbiddenArea(EntityId entityId);
	bool IsInsideForbiddenArea(const Vec3& pos, bool doResetCheck, IEntity** ppArea);
	void OnLocalPlayerRevived();

	void PreCacheItemResources(const char* itemName);

	bool CanCalculateSkillRanking() const { return m_bCanUpdateSkillRanking; }
	float GetGameStartTime() const { return m_gameStartedTime.GetMilliSeconds(); }

	bool LevelNameCheckNeeded() const { return m_bLevelNameCheckNeeded; }
	void LevelNameCheckDone() { m_bLevelNameCheckNeeded = false; }

	EDisconnectionCause SvGetLastTeamDiscoCause(const int teamId) const;
	EDisconnectionCause SvGetLastDiscoCause() const;

	IItem* GetCurrentItemForActorWithStatus(IActor* pActor, bool* outIsUsing, bool* outIsDroppable);
	bool ActorShouldHideCurrentItemInsteadOfDroppingOnDeath(IActor* pActor);
	
	bool GameEndedByWinningKill() const { return m_hasWinningKill; }

	const TCryUserIdSet &GetParticipatingUsers() { return m_participatingUsers; }

#if USE_PC_PREMATCH
	ILINE bool HasGameActuallyStarted() const { return m_prematchState == ePS_Match; }
	ILINE bool IsPrematchCountDown() const {return m_prematchState == ePS_Countdown;}
	ILINE EPrematchState GetPrematchState() const {return m_prematchState;}
#else
	ILINE bool HasGameActuallyStarted() const { return true; }
	ILINE bool IsPrematchCountDown() const { return false; }
#endif

	bool IsRestarting() const {return m_isRestarting;}

	void CallOnForbiddenAreas(const char *pFuncName);

	private:

		struct SEquipmentLoadOutPreCacheCallback : public IEquipmentPackPreCacheCallback
		{
			virtual void PreCacheItemResources(const char* itemName);
		};
		SEquipmentLoadOutPreCacheCallback m_equipmentLoadOutPreCacheCallback;

		// For cases where even with no teams, entity can be a friend.
		bool GetThreatRatingWithoutTeams( const EntityId entityIdA, const EntityId entityIdB, eThreatRating& rThreat ) const;

		void ApplyLoadoutChange();

		XmlNodeRef LoadLevelXml();
		bool IsValidName(const char* name) const;

		void ProcessDeferredMaterialEffects();
		ILINE int GetFreeExplosionIndex();
		void CalculateExplosionAffectedEntities(const ExplosionInfo &explosionInfo, TExplosionAffectedEntities& affectedEntities);
		void PrecacheList(XmlNodeRef precacheListNode);

		void FinishMigrationForPlayer(int migratingIndex);
		void FakeDisconnectPlayer(EntityId playerId);

		bool SetTeam_Common(int teamId, EntityId entityId, bool& bIsPlayer);

		void HostMigrationFindDynamicEntities(TEntityIdVec &results);
		void HostMigrationRemoveDuplicateDynamicEntities();
		void HostMigrationRemoveNonchanneledPlayers();

		void AddEntityEventDoneListener(EntityId id);
		void RemoveEntityEventDoneListener(EntityId id);

		void SetupForbiddenAreaShapesHelpers();

		void RetrieveCurrentHealthAndDeathForTarget(const IEntity *pEntity, const IActor *pActor, const IVehicle* pVehicle, float* resultHealth, bool* resultDead) const;

		TEntityIdVec m_hostMigrationCachedEntities;

		TEntityIdVec m_forbiddenAreas;
		struct SForbiddenAreaHelper
		{
			SForbiddenAreaHelper(EntityId id, bool rev, bool resetObjects, EntityId parent)
			{
				shapeId = id;
				reversed = rev;
				resetsObjects = resetObjects;
				parentId = parent;
			}

			EntityId shapeId;
			EntityId parentId;
			bool reversed;
			bool resetsObjects;
		};
		std::vector<SForbiddenAreaHelper> m_forbiddenAreaHelpers;

		typedef _smart_ptr<ICharacterInstance> TCharacterInstancePtr;
		typedef std::vector<TCharacterInstancePtr> TCharacterInstancePtrVec;
		TCharacterInstancePtrVec m_cachedCharacterInstances;

		typedef _smart_ptr<IParticleEffect> TParticleEffectPtr;
		typedef std::vector<TParticleEffectPtr> TParticleEffectPtrVec;
		TParticleEffectPtrVec m_cachedParticleEffects;

		typedef _smart_ptr<ITexture> TTextureInstancePtr;
		typedef std::vector<TTextureInstancePtr> TTexturePtrVec;
		TTexturePtrVec m_cachedFlashTextures;

		int m_numLocalPlayerRevives;
		bool m_bHasCalledEnteredGame;
		bool m_bCanUpdateSkillRanking;
		bool m_bClientTeamInLead;
		bool m_bLevelNameCheckNeeded;

		bool m_isRestarting;
	
#if USE_PC_PREMATCH
		CryFixedStringT<64> m_waitingForPlayerMessage1;
		CryFixedStringT<64> m_waitingForPlayerMessage2;
		int m_numRequiredPlayers;
		int m_previousNumRequiredPlayers;
		float m_finishPrematchTime;
		EPrematchState m_prematchState;
		CAudioSignalPlayer m_prematchAudioSignalPlayer;
		CTimeValue m_timeStartedWaitingForBalancedGame;
#endif

		static IEntityClass* s_pC4Explosive;
		static IEntityClass* s_pSmartMineClass;
		static IEntityClass* s_pTurretClass;
				
		typedef std::map<string, XmlNodeRef> TXmlFilename2NodeRefMap;
		TXmlFilename2NodeRefMap m_cachedXmlNodesMap;

		EDisconnectionCause m_svLastTeamDiscoCause[2];
		TCryUserIdSet m_participatingUsers;
		uint32	m_uSecurity;
		bool		m_bSecurityInitialized;
		bool		m_bIntroSequenceRegistered;
		bool    m_bIntroCurrentlyPlaying;
		bool		m_gameStarted;
		bool		m_bIntroSequenceCompletedPlaying;
}; 

#endif //__GAMERULES_H__
