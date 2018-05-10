// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ACTIONGAME_H__
#define __ACTIONGAME_H__

#pragma once

#define MAX_CACHED_EFFECTS 8

#include "IGameRulesSystem.h"
#include <CryAction/IMaterialEffects.h>
#include <CryGame/IGameFramework.h>

struct SGameStartParams;
struct SGameContextParams;
struct INetNub;
class CGameClientNub;
class CGameServerNub;
class CGameContext;
class CGameClientChannel;
class CGameServerChannel;
struct IGameObject;
struct INetContext;
struct INetwork;
struct IActor;
struct IGameTokenSystem;
struct IScriptTable;
class CScriptRMI;
class CGameStats;

//////////////////////////////////////////////////////////////////////////
struct SProcBrokenObjRec
{
	int          itype;
	IRenderNode* pBrush;
	EntityId     idEnt;
	int          islot;
	IStatObj*    pStatObjOrg;
	float        mass;

	void         GetMemoryUsage(ICrySizer* pSizer) const {}
};

//////////////////////////////////////////////////////////////////////////
enum EBreakEventState
{
	eBES_Generated,
	eBES_Processed,
	eBES_Max,
	eBES_Invalid = eBES_Max
};

//////////////////////////////////////////////////////////////////////////
struct SBreakEvent
{
	SBreakEvent()
		: itype(0)
		, bFirstBreak(0)
		, idEnt()
		, eventPos(ZERO)
		, hash(0)
		, pos(ZERO)
		, rot(ZERO)
		, scale(ZERO)
		, pt(ZERO)
		, n(ZERO)
		, penetration(0.0f)
		, energy(0.0f)
		, radius(0.0f)
		, time(0.0f)
		, iBrokenObjectIndex(-1)
		, iState(-1)
		, seed(0)
	{
		vloc[0] = vloc[1] = ZERO;
		mass[0] = mass[1] = ZERO;
		partid[0] = partid[1] = -1;
		idmat[0] = idmat[1] = -1;
		iPrim[0] = iPrim[1] = -1;
	}

	SBreakEvent& operator=(const SBreakEvent& in)
	{
		// Stop floating-point exceptions when copying this class around
		memcpy(this, &in, sizeof(in));
		return *this;
	}

	void Serialize(TSerialize ser)
	{
		ser.Value("type", itype);
		ser.Value("bFirstBreak", bFirstBreak);
		ser.Value("idEnt", idEnt);
		ser.Value("eventPos", eventPos);
		ser.Value("hash", hash);
		ser.Value("pos", pos);
		ser.Value("rot", rot);
		ser.Value("scale", scale);
		ser.Value("pt", pt);
		ser.Value("n", n);
		ser.Value("vloc0", vloc[0]);
		ser.Value("vloc1", vloc[1]);
		ser.Value("mass0", mass[0]);
		ser.Value("mass1", mass[1]);
		ser.Value("partid0", partid[0]);
		ser.Value("partid1", partid[1]);
		ser.Value("idmat0", idmat[0]);
		ser.Value("idmat1", idmat[1]);
		ser.Value("iPrim1", iPrim[1]);
		ser.Value("penetration", penetration);
		ser.Value("seed", seed);
		ser.Value("energy", energy);
		ser.Value("radius", radius);
	}

	void GetMemoryUsage(ICrySizer* pSizer) const {}

	int16    itype;
	int16    bFirstBreak; // For plane breaks record whether this is the first break
	EntityId idEnt;
	// For Static Entities
	Vec3     eventPos;
	uint32   hash;

	Vec3     pos;
	Quat     rot;
	Vec3     scale;

	Vec3     pt;
	Vec3     n;
	Vec3     vloc[2];
	float    mass[2];
	int      partid[2];
	int      idmat[2];
	float    penetration;
	float    energy;
	float    radius;
	float    time;

	int16    iPrim[2];
	int16    iBrokenObjectIndex;
	int16    iState;
	int      seed;
};

struct SBrokenEntPart
{
	void Serialize(TSerialize ser)
	{
		ser.Value("idSrcEnt", idSrcEnt);
		ser.Value("idNewEnt", idNewEnt);
	}
	void GetMemoryUsage(ICrySizer* pSizer) const {}

	EntityId idSrcEnt;
	EntityId idNewEnt;
};

struct SBrokenVegPart
{
	void Serialize(TSerialize ser)
	{
		ser.Value("pos", pos);
		ser.Value("volume", volume);
		ser.Value("idNewEnt", idNewEnt);
	}
	void GetMemoryUsage(ICrySizer* pSizer) const {}

	Vec3     pos;
	float    volume;
	EntityId idNewEnt;
};

struct SEntityCollHist
{
	SEntityCollHist* pnext;
	float            velImpact, velSlide2, velRoll2;
	int              imatImpact[2], imatSlide[2], imatRoll[2];
	float            timeRolling, timeNotRolling;
	float            rollTimeout, slideTimeout;
	float            mass;

	void             GetMemoryUsage(ICrySizer* pSizer) const { /*nothing*/ }
};

struct SEntityHits
{
	SEntityHits* pnext;
	Vec3*        pHits, hit0;
	int*         pnHits, nhits0;
	int          nHits, nHitsAlloc;
	float        hitRadius;
	int          hitpoints;
	int          maxdmg;
	int          nMaxHits;
	float        timeUsed, lifeTime;

	void         GetMemoryUsage(ICrySizer* pSizer) const
	{
		//TODO
	}
};

struct STreeBreakInst;
struct STreePieceThunk
{
	IPhysicalEntity* pPhysEntNew;
	STreePieceThunk* pNext;
	STreeBreakInst*  pParent;
};

struct STreeBreakInst
{
	IPhysicalEntity* pPhysEntSrc;
	IPhysicalEntity* pPhysEntNew0;
	STreePieceThunk* pNextPiece;
	STreeBreakInst*  pThis;
	STreeBreakInst*  pNext;
	IStatObj*        pStatObj;
	float            cutHeight, cutSize;
};

struct SBrokenMeshSize
{
	SBrokenMeshSize()
		: pent(nullptr)
		, partid(0)
		, size(0)
		, timeout(0.0f)
		, fractureFX(nullptr)
	{
	}
	SBrokenMeshSize(IPhysicalEntity* _pent, int _size, int _partid, float _timeout, const char* _fractureFX)
	{ (pent = _pent)->AddRef(); size = _size; partid = _partid; timeout = _timeout; fractureFX = _fractureFX; }
	SBrokenMeshSize(const SBrokenMeshSize& src) { if (pent = src.pent) pent->AddRef(); partid = src.partid; size = src.size; timeout = src.timeout; fractureFX = src.fractureFX; }
	~SBrokenMeshSize() { if (pent) pent->Release(); }
	void Serialize(TSerialize ser)
	{
		int id;
		if (ser.IsReading())
		{
			ser.Value("id", id);
			pent = gEnv->pPhysicalWorld->GetPhysicalEntityById(id);
		}
		else
			ser.Value("id", id = gEnv->pPhysicalWorld->GetPhysicalEntityId(pent));
		ser.Value("partid", partid);
		ser.Value("size", size);
	}
	IPhysicalEntity* pent;
	int              partid;
	int              size;
	float            timeout;
	const char*      fractureFX;
};

//////////////////////////////////////////////////////////////////////////
class CActionGame : public IHitListener, public CMultiThreadRefCount, public IHostMigrationEventListener
{
public:
	CActionGame(CScriptRMI*);
	~CActionGame();

	bool          Init(const SGameStartParams*);
	void          ServerInit(const SGameStartParams* pGameStartParams, bool* io_ok, bool* io_hasPbSvStarted);
	void          ClientInit(const SGameStartParams* pGameStartParams, bool* io_ok, bool* io_hasPbSvStarted, bool* io_requireBlockingConnection);

	void          PostInit(const SGameStartParams* pGameStartParams, bool* io_ok, bool* io_requireBlockingConnection);

	bool          IsInited(void)  { return (m_initState == eIS_InitDone); }
	bool          IsIniting(void) { return (m_initState > eIS_Uninited) && (m_initState < eIS_InitDone); }
	bool          ChangeGameContext(const SGameContextParams*);
	bool          BlockingSpawnPlayer();

	const string& GetLevelName() const { return m_levelName; }

	void          UnloadLevel();
	void          UnloadPhysicsData();
	void          FixBrokenObjects(bool bRestoreBroken);

	void          CloneBrokenObjectsByIndex(uint16* pBreakEventIndices, int32& iNumBreakEvents, IRenderNode** outClonedNodes, int32& iNumClonedNodes, SRenderNodeCloneLookup& nodeLookup);
	void          HideBrokenObjectsByIndex(uint16* pObjectIndicies, int32 iNumObjectIndices);
	void          UnhideBrokenObjectsByIndex(uint16* pObjectIndicies, int32 iNumObjectIndices);
	void          ApplySingleProceduralBreakFromEventIndex(uint16 uBreakEventIndex, const SRenderNodeCloneLookup& renderNodeLookup);
	void          ApplyBreaksUntilTimeToObjectList(int iFirstBreakEventIndex, const SRenderNodeCloneLookup& renderNodeLookup);

	SBreakEvent&  StoreBreakEvent(const SBreakEvent& breakEvent);

	// CryLobby
	//void InitCryLobby( void );
	void            CryLobbyServerInit(const SGameStartParams* pGameStartParams);

	bool            IsServer() const   { return m_pServerNub != 0; }
	bool            IsClient() const   { return m_pClientNub != 0; }
	CGameServerNub* GetGameServerNub() { return m_pGameServerNub; }
	CGameClientNub* GetGameClientNub() { return m_pGameClientNub; }
	CGameContext*   GetGameContext()   { return m_pGameContext; }
	IActor*         GetClientActor();

	// returns true if should be let go
	bool Update();

	void AddGlobalPhysicsCallback(int event, void (*)(const EventPhys*, void*), void*);
	void RemoveGlobalPhysicsCallback(int event, void (*)(const EventPhys*, void*), void*);

	void     Serialize(TSerialize ser);
	void     SerializeBreakableObjects(TSerialize ser);
	void     FlushBreakableObjects();
	void     ClearBreakHistory();

	void     OnBreakageSpawnedEntity(IEntity* pEntity, IPhysicalEntity* pPhysEntity, IPhysicalEntity* pSrcPhysEntity);

	void     InitImmersiveness();
	void     UpdateImmersiveness();

	void     OnEditorSetGameMode(bool bGameMode);

	INetNub* GetServerNetNub() { return m_pServerNub; }
	INetNub* GetClientNetNub() { return m_pClientNub; }

	void     DumpStats();

	void     GetMemoryUsage(ICrySizer* s) const;

	//
	void                ReleaseGameStats();

	void                FreeBrokenMeshesForEntity(IPhysicalEntity* pEntity);

	static CActionGame* Get() { return s_this; }

	static void         RegisterCVars();

	// helper functions
	static IGameObject* GetEntityGameObject(IEntity* pEntity);
	static IGameObject* GetPhysicalEntityGameObject(IPhysicalEntity* pPhysEntity);

	static void PerformPlaneBreak(const EventPhysCollision &epc, SBreakEvent * pRecordedEvent, int flags, class CDelayedPlaneBreak * pDelayedTask);

public:
	enum EPeformPlaneBreakFlags
	{
		ePPB_EnableParticleEffects = 0x1,    // Used to force particles for recorded/delayed/network events
		ePPB_PlaybackEvent         = 0x2,    // Used for event playbacks from the netork etc and to distinguish these as not events from serialisation/loading
	};
	static float g_glassAutoShatterMinArea;

private:
	static IEntity* GetEntity(int i, void* p)
	{
		if (i == PHYS_FOREIGN_ID_ENTITY)
			return (IEntity*)p;
		return NULL;
	}

	void         EnablePhysicsEvents(bool enable);

	void         CreateGameStats();
	void         ApplyBreakToClonedObjectFromEvent(const SRenderNodeCloneLookup& renderNodeLookup, int iBrokenObjIndex, int i);

	void         LogModeInformation(const bool isMultiplayer, const char* hostname) const;
	static int   OnBBoxOverlap(const EventPhys* pEvent);
	static int   OnCollisionLogged(const EventPhys* pEvent);
	static int   OnPostStepLogged(const EventPhys* pEvent);
	static int   OnStateChangeLogged(const EventPhys* pEvent);
	static int   OnCreatePhysicalEntityLogged(const EventPhys* pEvent);
	static int   OnUpdateMeshLogged(const EventPhys* pEvent);
	static int   OnRemovePhysicalEntityPartsLogged(const EventPhys* pEvent);
	static int   OnPhysEntityDeleted(const EventPhys* pEvent);

	static int   OnCollisionImmediate(const EventPhys* pEvent);
	static int   OnPostStepImmediate(const EventPhys* pEvent);
	static int   OnStateChangeImmediate(const EventPhys* pEvent);
	static int   OnCreatePhysicalEntityImmediate(const EventPhys* pEvent);
	static int   OnUpdateMeshImmediate(const EventPhys* pEvent);

	virtual void OnHit(const HitInfo&)                   {}
	virtual void OnExplosion(const ExplosionInfo&);
	virtual void OnServerExplosion(const ExplosionInfo&) {}

	static void  OnCollisionLogged_MaterialFX(const EventPhys* pEvent);
	static void  OnCollisionLogged_Breakable(const EventPhys* pEvent);
	static void  OnPostStepLogged_MaterialFX(const EventPhys* pEvent);
	static void  OnStateChangeLogged_MaterialFX(const EventPhys* pEvent);

	// IHostMigrationEventListener
	virtual EHostMigrationReturn OnInitiate(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual EHostMigrationReturn OnDisconnectClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual EHostMigrationReturn OnDemoteToClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual EHostMigrationReturn OnPromoteToServer(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual EHostMigrationReturn OnReconnectClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual EHostMigrationReturn OnFinalise(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual void                 OnComplete(SHostMigrationInfo& hostMigrationInfo) {}
	virtual EHostMigrationReturn OnTerminate(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual EHostMigrationReturn OnReset(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	// ~IHostMigrationEventListener

	bool         ProcessHitpoints(const Vec3& pt, IPhysicalEntity* pent, int partid, ISurfaceType* pMat, int iDamage = 1);
	SBreakEvent& RegisterBreakEvent(const EventPhysCollision* pColl, float energy);
	int          ReuseBrokenTrees(const EventPhysCollision* pCEvent, float size, int flags);
	EntityId     UpdateEntityIdForBrokenPart(EntityId idSrc);
	EntityId     UpdateEntityIdForVegetationBreak(IRenderNode* pVeg);
	void         RegisterEntsForBreakageReuse(IPhysicalEntity* pPhysEnt, int partid, IPhysicalEntity* pPhysEntNew, float h, float size);
	void         RemoveEntFromBreakageReuse(IPhysicalEntity* pEntity, int bRemoveOnlyIfSecondary);
	void         ClearTreeBreakageReuseLog();
	int          FreeBrokenMesh(IPhysicalEntity* pent, SBrokenMeshSize& bm);
	void         RegisterBrokenMesh(IPhysicalEntity*, IGeometry*, int partid = 0, IStatObj* pStatObj = 0, IGeometry* pSkel = 0, float timeout = 0.0f, const char* fractureFX = 0);
	void         DrawBrokenMeshes();
	static void  AddBroken2DChunkId(int id);
	void         UpdateBrokenMeshes(float dt);
	void         UpdateFadeEntities(float dt);

	bool         ConditionHavePlayer(CGameClientChannel*);
	bool         ConditionHaveConnection(CGameClientChannel*);
	bool         ConditionInGame(CGameClientChannel*);
	typedef bool (CActionGame::* BlockingConditionFunction)(CGameClientChannel*);
	bool BlockingConnect(BlockingConditionFunction, bool requireClientChannel, const char* conditionText);

	enum eInitTaskState
	{
		eITS_InProgress,
		eITS_Done,
		eITS_Error
	};

	eInitTaskState NonBlockingConnect(BlockingConditionFunction, bool requireClientChannel, const char* conditionText);

	void CallOnEditorSetGameMode(IEntity* pEntity, bool bGameMode);

	bool IsStale();

	void AddProtectedPath(const char* root);

	enum EProceduralBreakType
	{
		ePBT_Normal = 0x10,
		ePBT_Glass  = 0x01,
	};
	enum EProceduralBreakFlags
	{
		ePBF_ObeyCVar     = 0x01,
		ePBF_AllowGlass   = 0x02,
		ePBF_DefaultAllow = 0x08,
	};
	uint8 m_proceduralBreakFlags;
	bool AllowProceduralBreaking(uint8 proceduralBreakType);

	static CActionGame* s_this;

	IEntitySystem*      m_pEntitySystem;
	INetwork*           m_pNetwork;
	INetNub*            m_pClientNub;
	INetNub*            m_pServerNub;
	CGameClientNub*     m_pGameClientNub;
	CGameServerNub*     m_pGameServerNub;
	CGameContext*       m_pGameContext;
	IGameTokenSystem*   m_pGameTokenSystem;
	IPhysicalWorld*     m_pPhysicalWorld;

	typedef std::pair<void (*)(const EventPhys*, void*), void*> TGlobalPhysicsCallback;
	typedef std::set<TGlobalPhysicsCallback>                    TGlobalPhysicsCallbackSet;
	struct SPhysCallbacks
	{
		TGlobalPhysicsCallbackSet collision[2];
		TGlobalPhysicsCallbackSet postStep[2];
		TGlobalPhysicsCallbackSet stateChange[2];
		TGlobalPhysicsCallbackSet createEntityPart[2];
		TGlobalPhysicsCallbackSet updateMesh[2];

	}                                            m_globalPhysicsCallbacks;

	std::vector<SProcBrokenObjRec>               m_brokenObjs;
	std::vector<SBreakEvent>                     m_breakEvents;
	std::vector<SBrokenEntPart>                  m_brokenEntParts;
	std::vector<SBrokenVegPart>                  m_brokenVegParts;
	std::vector<EntityId>                        m_broken2dChunkIds;
	std::map<EntityId, int>                      m_entPieceIdx;
	std::map<IPhysicalEntity*, STreeBreakInst*>  m_mapBrokenTreesByPhysEnt;
	std::map<IStatObj*, STreeBreakInst*>         m_mapBrokenTreesByCGF;
	std::map<IPhysicalEntity*, STreePieceThunk*> m_mapBrokenTreesChunks;
	std::map<int, SBrokenMeshSize>               m_mapBrokenMeshes;
	std::vector<int>                             m_brokenMeshRemovals;
	std::vector<CDelayedPlaneBreak>              m_pendingPlaneBreaks;
	bool                                         m_bLoading;
	int                                          m_iCurBreakEvent;
	int                                          m_totBreakageSize;
	int                                          m_inDeleteEntityCallback;

	CGameStats*                                  m_pGameStats;

	SEntityCollHist*                             m_pCHSlotPool, * m_pFreeCHSlot0;
	std::map<int, SEntityCollHist*>              m_mapECH;

	SEntityHits*                                 m_pEntHits0;
	std::map<int, SEntityHits*>                  m_mapEntHits;
	typedef struct SVegCollisionStatus
	{
		IRenderNode* rn;
		IStatObj*    pStat;
		int          branchNum;
		SVegCollisionStatus()
		{
			branchNum = -1;
			rn = 0;
			pStat = 0;
		}
		void GetMemoryUsage(ICrySizer* pSizer) const { /*nothing*/ }

	} SVegCollisionStatus;
	std::map<EntityId, Vec3>                 m_vegStatus;
	std::map<EntityId, SVegCollisionStatus*> m_treeStatus;
	std::map<EntityId, SVegCollisionStatus*> m_treeBreakStatus;

	int                     m_nEffectCounter;
	SMFXRunTimeEffectParams m_lstCachedEffects[MAX_CACHED_EFFECTS];
	static int              g_procedural_breaking;
	static int              g_joint_breaking;
	static float            g_tree_cut_reuse_dist;
	static int              g_no_secondary_breaking;
	static int              g_no_breaking_by_objects;
	static int              g_breakage_mem_limit;
	static int              g_breakage_debug;

	static int              s_waterMaterialId;

	enum eDisconnectState
	{
		eDS_Disconnect,
		eDS_Disconnecting,
		eDS_Disconnected
	};

	enum eReconnectState
	{
		eRS_Reconnect,
		eRS_Reconnecting,
		eRS_Terminated,
		eRS_Reconnected
	};

	enum eInitState
	{
		eIS_Uninited,
		eIS_Initing,
		eIS_WaitForConnection,
		eIS_WaitForPlayer,
		eIS_WaitForInGame,
		eIS_InitDone,
		eIS_InitError
	};

	eInitState m_initState;

	struct SEntityFadeState
	{
		EntityId entId;       // Entity ID
		float    time;        // Time since spawned
		int      bCollisions; // Are collisions on
	};
	DynArray<SEntityFadeState> m_fadeEntities;

	void BackupGameStartParams(const SGameStartParams* pGameStartParams);

	SGameContextParams m_gameContextParams;
	SGameStartParams   m_startGameParams;

	string             m_levelName;
	string             m_gameRules;
	string             m_demoRecorderFilename;
	string             m_demoPlaybackFilename;
	string             m_hostname;
	string             m_connectionString;

	uint32             m_lastDynPoolSize;

	struct SBreakageThrottling
	{
		int16 m_numGlassEvents;
		int16 m_brokenTreeCounter;
	};

	SBreakageThrottling m_throttling;

#ifndef _RELEASE
	float        m_timeToPromoteToServer;
	static float g_hostMigrationServerDelay;
#endif
};

#endif
