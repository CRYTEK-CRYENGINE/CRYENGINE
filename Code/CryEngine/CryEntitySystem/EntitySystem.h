// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   EntitySystem.h
//  Version:     v1.00
//  Created:     24/5/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __EntitySystem_h__
#define __EntitySystem_h__
#pragma once

#include <CryEntitySystem/IEntitySystem.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/ITimer.h>
#include "SaltBufferArray.h"          // SaltBufferArray<>
#include "EntityTimeoutList.h"
#include <CryCore/StlUtils.h>
#include <CryMemory/STLPoolAllocator.h>
#include <CryMemory/STLGlobalAllocator.h>

//////////////////////////////////////////////////////////////////////////
// forward declarations.
//////////////////////////////////////////////////////////////////////////
class CEntity;
struct ICVar;
struct IPhysicalEntity;
class IComponent;
class CComponentEventDistributer;
class CEntityClassRegistry;
class CScriptBind_Entity;
class CPhysicsEventListener;
class CAreaManager;
class CBreakableManager;
class CEntityArchetypeManager;
class CPartitionGrid;
class CProximityTriggerSystem;
class CEntityLayer;
class CEntityLoadManager;
struct SEntityLayerGarbage;
class CGeomCacheAttachmentManager;
class CCharacterBoneAttachmentManager;
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
typedef std::unordered_map<EntityId, CEntity*, stl::hash_uint32> EntityMap;
typedef std::vector<EntityId>                                    EntityIdVector;
typedef std::vector<EntityGUID>                                  EntityGuidVector;

//////////////////////////////////////////////////////////////////////////
struct SEntityTimerEvent
{
	EntityId entityId;
	int      nTimerId;
	int      nMilliSeconds;
};

//////////////////////////////////////////////////////////////////////////
struct SEntityAttachment
{
	EntityId   child;
	EntityId   parent;
	EntityGUID parentGuid;
	Vec3       pos;
	Quat       rot;
	Vec3       scale;
	bool       guid;
	int        flags;
	string     target;

	SEntityAttachment() : child(0), parent(0), parentGuid(0), pos(ZERO), rot(ZERO), scale(ZERO), guid(false), flags(0) {}
};

//////////////////////////////////////////////////////////////////////////
// Structure for extended information about loading an entity
//	Supports reusing an entity container if specified
struct SEntityLoadParams
{
	SEntitySpawnParams spawnParams;
	CEntity*           pReuseEntity;
	bool               bCallInit;
	int                clonedLayerId;

	SEntityLoadParams();
	SEntityLoadParams(CEntity* pReuseEntity, SEntitySpawnParams& resetParams);
	~SEntityLoadParams();

	SEntityLoadParams& operator=(const SEntityLoadParams& other);
	SEntityLoadParams(const SEntityLoadParams& other) { *this = other; }
	void               UseClonedEntityNode(const XmlNodeRef sourceEntityNode, XmlNodeRef parentNode);

private:
	void        AddRef();
	void        RemoveRef();

	static bool CloneXmlNode(const XmlNodeRef sourceNode, XmlNodeRef destNode);
};
typedef std::vector<SEntityLoadParams>              TEntityLoadParamsContainer;

typedef CSaltHandle<unsigned short, unsigned short> CEntityHandle;

//////////////////////////////////////////////////////////////////////
class CEntitySystem : public IEntitySystem
{
public:
	CEntitySystem(ISystem* pSystem);
	~CEntitySystem();

	bool Init(ISystem* pSystem);

	// interface IEntitySystem ------------------------------------------------------
	virtual void                  RegisterCharactersForRendering() override;
	virtual void                  Release() override;
	virtual void                  Update() override;
	virtual void                  DeletePendingEntities() override;
	virtual void                  PrePhysicsUpdate() override;
	virtual void                  Reset() override;
	virtual void                  Unload() override;
	virtual void                  PurgeHeaps() override;
	virtual IEntityClassRegistry* GetClassRegistry() override;
	virtual IEntity*              SpawnEntity(SEntitySpawnParams& params, bool bAutoInit = true) override;
	virtual bool                  InitEntity(IEntity* pEntity, SEntitySpawnParams& params) override;
	virtual IEntity*              GetEntity(EntityId id) const override;
	virtual EntityId              GetClonedEntityId(EntityId origId, EntityId refId) const override;
	virtual IEntity*              FindEntityByName(const char* sEntityName) const override;
	virtual void                  ReserveEntityId(const EntityId id) override;
	virtual EntityId              ReserveUnknownEntityId() override;
	virtual void                  RemoveEntity(EntityId entity, bool bForceRemoveNow = false) override;
	virtual uint32                GetNumEntities() const override;
	virtual IEntityIt*            GetEntityIterator() override;
	virtual void                  SendEventViaEntityEvent(IEntity* piEntity, SEntityEvent& event) override;
	virtual void                  SendEventToAll(SEntityEvent& event) override;
	virtual int                   QueryProximity(SEntityProximityQuery& query) override;
	virtual void                  ResizeProximityGrid(int nWidth, int nHeight) override;
	virtual int                   GetPhysicalEntitiesInBox(const Vec3& origin, float radius, IPhysicalEntity**& pList, int physFlags) const override;
	virtual IEntity*              GetEntityFromPhysics(IPhysicalEntity* pPhysEntity) const override;
	virtual void                  AddSink(IEntitySystemSink* pSink, uint32 subscriptions, uint64 onEventSubscriptions) override;
	virtual void                  RemoveSink(IEntitySystemSink* pSink) override;
	virtual void                  PauseTimers(bool bPause, bool bResume = false) override;
	virtual bool                  IsIDUsed(EntityId nID) const override;
	virtual void                  GetMemoryStatistics(ICrySizer* pSizer) const override;
	virtual ISystem*              GetSystem() const override { return m_pISystem; };
	virtual void                  SetNextSpawnId(EntityId id) override;
	virtual void                  ResetAreas() override;
	virtual void                  UnloadAreas() override;

	virtual void                  AddEntityEventListener(EntityId nEntity, EEntityEvent event, IEntityEventListener* pListener) override;
	virtual void                  RemoveEntityEventListener(EntityId nEntity, EEntityEvent event, IEntityEventListener* pListener) override;

	virtual EntityId              FindEntityByGuid(const EntityGUID& guid) const override;
	virtual EntityId              FindEntityByEditorGuid(const char* pGuid) const override;

	virtual EntityId              GenerateEntityIdFromGuid(const EntityGUID& guid) override;

	virtual IEntityArchetype*     LoadEntityArchetype(XmlNodeRef oArchetype) override;
	virtual IEntityArchetype*     LoadEntityArchetype(const char* sArchetype) override;
	virtual void                  UnloadEntityArchetype(const char* sArchetype) override;
	virtual IEntityArchetype*     CreateEntityArchetype(IEntityClass* pClass, const char* sArchetype) override;
	virtual void                  RefreshEntityArchetypesInRegistry() override;

	virtual void                  Serialize(TSerialize ser) override;

	virtual void                  DumpEntities() override;

	virtual void                  ResumePhysicsForSuppressedEntities(bool bWakeUp) override;
	virtual void                  SaveInternalState(struct IDataWriteStream& writer) const override;
	virtual void                  LoadInternalState(struct IDataReadStream& reader) override;

	virtual int                   GetLayerId(const char* szLayerName) const override;
	virtual const char*           GetLayerName(int layerId) const override;
	virtual int                   GetLayerChildCount(const char* szLayerName) const override;
	virtual const char*           GetLayerChild(const char* szLayerName, int childIdx) const override;

	virtual int                   GetVisibleLayerIDs(uint8* pLayerMask, const uint32 maxCount) const override;

	virtual void                  ToggleLayerVisibility(const char* layer, bool isEnabled, bool includeParent = true) override;

	virtual void                  ToggleLayersBySubstring(const char* pSearchSubstring, const char* pExceptionSubstring, bool isEnable) override;

	virtual void                  LockSpawning(bool lock) override { m_bLocked = lock; }

	virtual bool                  OnLoadLevel(const char* szLevelPath) override;
	void                          OnLevelLoadStart();

	virtual IEntityLayer*         AddLayer(const char* szName, const char* szParent, uint16 id, bool bHasPhysics, int specs, bool bDefaultLoaded) override;
	virtual void                  LoadLayers(const char* dataFile) override;
	virtual void                  LinkLayerChildren() override;
	virtual void                  AddEntityToLayer(const char* layer, EntityId id) override;
	virtual void                  RemoveEntityFromLayers(EntityId id) override;
	virtual void                  ClearLayers() override;
	virtual void                  EnableDefaultLayers(bool isSerialized = true) override;
	virtual void                  EnableLayer(const char* layer, bool isEnable, bool isSerialized = true) override;
	virtual IEntityLayer*         FindLayer(const char* szLayer) const override;
	virtual bool                  IsLayerEnabled(const char* layer, bool bMustBeLoaded) const override;
	virtual bool                  ShouldSerializedEntity(IEntity* pEntity) override;
	virtual void                  RegisterPhysicCallbacks() override;
	virtual void                  UnregisterPhysicCallbacks() override;

	// ------------------------------------------------------------------------

	CEntityLayer* GetLayerForEntity(EntityId id);

	bool          OnBeforeSpawn(SEntitySpawnParams& params);
	void          OnEntityReused(IEntity* pEntity, SEntitySpawnParams& params);

	// Sets new entity timer event.
	void AddTimerEvent(SEntityTimerEvent& event, CTimeValue startTime = gEnv->pTimer->GetFrameStartTime());

	//////////////////////////////////////////////////////////////////////////
	// Load entities from XML.
	void                              LoadEntities(XmlNodeRef& objectsNode, bool bIsLoadingLevelFile) override;
	void                              LoadEntities(XmlNodeRef& objectsNode, bool bIsLoadingLevelFile, const Vec3& segmentOffset, std::vector<IEntity*>* outGlobalEntityIds, std::vector<IEntity*>* outLocalEntityIds) override;

	virtual void                      HoldLayerEntities(const char* pLayerName) override;
	virtual void                      CloneHeldLayerEntities(const char* pLayerName, const Vec3& localOffset, const Matrix34& l2w, const char** pExcludeLayers = NULL, int numExcludeLayers = 0) override;
	virtual void                      ReleaseHeldEntities() override;

	ILINE CComponentEventDistributer* GetEventDistributer() { return m_pEventDistributer; }

	virtual bool                      ExtractArcheTypeLoadParams(XmlNodeRef& entityNode, SEntitySpawnParams& spawnParams) const override;
	virtual bool                      ExtractEntityLoadParams(XmlNodeRef& entityNode, SEntitySpawnParams& spawnParams) const override;
	virtual void                      BeginCreateEntities(int nAmtToCreate) override;
	virtual bool                      CreateEntity(XmlNodeRef& entityNode, SEntitySpawnParams& pParams, EntityId& outUsingId) override;
	virtual void                      EndCreateEntities() override;

	//////////////////////////////////////////////////////////////////////////
	// Called from CEntity implementation.
	//////////////////////////////////////////////////////////////////////////
	void RemoveTimerEvent(EntityId id, int nTimerId);

	// Puts entity into active list.
	void ActivateEntity(CEntity* pEntity, bool bActivate);
	void ActivatePrePhysicsUpdateForEntity(CEntity* pEntity, bool bActivate);
	bool IsPrePhysicsActive(CEntity* pEntity);
	void OnEntityEvent(CEntity* pEntity, SEntityEvent& event);

	// Access to class that binds script to entity functions.
	// Used by Script proxy.
	CScriptBind_Entity* GetScriptBindEntity() { return m_pEntityScriptBinding; };

	// Access to area manager.
	IAreaManager* GetAreaManager() const override { return (IAreaManager*)(m_pAreaManager); }

	// Access to breakable manager.
	virtual IBreakableManager*       GetBreakableManager() const override      { return m_pBreakableManager; };

	CEntityLoadManager*              GetEntityLoadManager() const              { return m_pEntityLoadManager; }

	CGeomCacheAttachmentManager*     GetGeomCacheAttachmentManager() const     { return m_pGeomCacheAttachmentManager; }
	CCharacterBoneAttachmentManager* GetCharacterBoneAttachmentManager() const { return m_pCharacterBoneAttachmentManager; }

	CEntityTimeoutList*              GetTimeoutList()                          { return &m_entityTimeoutList; }

	static ILINE uint16              IdToIndex(const EntityId id)              { return id & 0xffff; }
	static ILINE CSaltHandle<>       IdToHandle(const EntityId id)             { return CSaltHandle<>(id >> 16, id & 0xffff); }
	static ILINE EntityId            HandleToId(const CSaltHandle<> id)        { return (((uint32)id.GetSalt()) << 16) | ((uint32)id.GetIndex()); }

	EntityId                         GenerateEntityId(bool bStaticId);
	bool                             ResetEntityId(CEntity* pEntity, EntityId newEntityId);
	void                             RegisterEntityGuid(const EntityGUID& guid, EntityId id);
	void                             UnregisterEntityGuid(const EntityGUID& guid);

	CPartitionGrid*                  GetPartitionGrid()          { return m_pPartitionGrid; }
	CProximityTriggerSystem*         GetProximityTriggerSystem() { return m_pProximityTriggerSystem; }

	void                             ChangeEntityName(CEntity* pEntity, const char* sNewName);

	void                             RemoveEntityEventListeners(CEntity* pEntity);

	CEntity*                         GetEntityFromID(EntityId id) const;
	ILINE bool                       HasEntity(EntityId id) const { return GetEntityFromID(id) != 0; };

	virtual void                     PurgeDeferredCollisionEvents(bool bForce = false) override;

	void                             ComponentRegister(EntityId id, IComponentPtr pComponent, const int flags);
	virtual void                     ComponentEnableEvent(const EntityId id, const int eventID, const bool enable) override;

	virtual void                     DebugDraw() override;

	virtual bool                     EntitiesUseGUIDs() const override                { return m_bEntitiesUseGUIDs; }
	virtual void                     SetEntitiesUseGUIDs(const bool bEnable) override { m_bEntitiesUseGUIDs = bEnable; }

	virtual IBSPTree3D*              CreateBSPTree3D(const IBSPTree3D::FaceList& faceList) override;
	virtual void                     ReleaseBSPTree3D(IBSPTree3D*& pTree) override;

private: // -----------------------------------------------------------------
	void DoPrePhysicsUpdate();
	void DoPrePhysicsUpdateFast();
	void DoUpdateLoop(float fFrameTime);

	void DeleteEntity(CEntity* pEntity);
	void UpdateDeletedEntities();
	void RemoveEntityFromActiveList(CEntity* pEntity);
	void UpdateNotSeenTimeouts();
	void UpdateEngineCVars();
	void UpdateTimers();
	void DebugDraw(CEntity* pEntity, float fUpdateTime);

	void DebugDrawEntityUsage();
	void DebugDrawLayerInfo();
	void DebugDrawProximityTriggers();

	void ClearEntityArray();

	void DumpEntity(IEntity* pEntity);

	void UpdateTempActiveEntities();

	// slow - to find specific problems
	void CheckInternalConsistency() const;

	//////////////////////////////////////////////////////////////////////////
	// Variables.
	//////////////////////////////////////////////////////////////////////////
	struct OnEventSink
	{
		OnEventSink(uint64 _subscriptions, IEntitySystemSink* _pSink)
			: subscriptions(_subscriptions)
			, pSink(_pSink)
		{
		}

		uint64             subscriptions;
		IEntitySystemSink* pSink;
	};

	typedef std::vector<OnEventSink, stl::STLGlobalAllocator<OnEventSink>>                                                                                                                        EntitySystemOnEventSinks;
	typedef std::vector<IEntitySystemSink*, stl::STLGlobalAllocator<IEntitySystemSink*>>                                                                                                          EntitySystemSinks;
	typedef std::vector<CEntity*>                                                                                                                                                                 DeletedEntities;
	typedef std::multimap<CTimeValue, SEntityTimerEvent, std::less<CTimeValue>, stl::STLPoolAllocator<std::pair<CTimeValue, SEntityTimerEvent>, stl::PoolAllocatorSynchronizationSinglethreaded>> EntityTimersMap;
	typedef std::multimap<const char*, EntityId, stl::less_stricmp<const char*>>                                                                                                                  EntityNamesMap;
	typedef std::map<EntityId, CEntity*>                                                                                                                                                          EntitiesMap;
	typedef std::set<EntityId>                                                                                                                                                                    EntitiesSet;
	typedef std::vector<SEntityTimerEvent>                                                                                                                                                        EntityTimersVector;

	EntitySystemSinks           m_sinks[SinkMaxEventSubscriptionCount];     // registered sinks get callbacks for creation and removal
	EntitySystemOnEventSinks    m_onEventSinks;

	ISystem*                    m_pISystem;
	std::vector<CEntity*>       m_EntityArray;              // [id.GetIndex()]=CEntity
	DeletedEntities             m_deletedEntities;
	std::vector<CEntity*>       m_deferredUsedEntities;
	EntitiesMap                 m_mapActiveEntities;        // Map of currently active entities (All entities that need per frame update).
	bool                        m_tempActiveEntitiesValid;  // must be set to false whenever m_mapActiveEntities is changed
	EntitiesSet                 m_mapPrePhysicsEntities;    // map of entities requiring pre-physics activation

	EntityNamesMap              m_mapEntityNames;      // Map entity name to entity ID.

	CSaltBufferArray<>          m_EntitySaltBuffer;         // used to create new entity ids (with uniqueid=salt)
	std::vector<EntityId>       m_tempActiveEntities;       // Temporary array of active entities.

	CComponentEventDistributer* m_pEventDistributer;
	//////////////////////////////////////////////////////////////////////////

	// Entity timers.
	EntityTimersMap    m_timersMap;
	EntityTimersVector m_currentTimers;
	bool               m_bTimersPause;
	CTimeValue         m_nStartPause;

	// Binding entity.
	CScriptBind_Entity* m_pEntityScriptBinding;

	// Entity class registry.
	CEntityClassRegistry*  m_pClassRegistry;
	CPhysicsEventListener* m_pPhysicsEventListener;

	CAreaManager*          m_pAreaManager;

	CEntityLoadManager*    m_pEntityLoadManager;

	// There`s a map of entity id to event listeners for each event.
	typedef std::multimap<EntityId, IEntityEventListener*> EventListenersMap;
	EventListenersMap m_eventListeners[ENTITY_EVENT_LAST];

	typedef std::map<EntityGUID, EntityId> EntityGuidMap;
	EntityGuidMap                    m_guidMap;
	EntityGuidMap                    m_genIdMap;

	IBreakableManager*               m_pBreakableManager;
	CEntityArchetypeManager*         m_pEntityArchetypeManager;
	CGeomCacheAttachmentManager*     m_pGeomCacheAttachmentManager;
	CCharacterBoneAttachmentManager* m_pCharacterBoneAttachmentManager;

	// Partition grid used by the entity system
	CPartitionGrid*          m_pPartitionGrid;
	CProximityTriggerSystem* m_pProximityTriggerSystem;

	EntityId                 m_idForced;

	//don't spawn any entities without being forced to
	bool               m_bLocked;

	CEntityTimeoutList m_entityTimeoutList;

	friend class CEntityItMap;
	class CCompareEntityIdsByClass;

	// helper to satisfy GCC
	static IEntityClass* GetClassForEntity(CEntity*);

	typedef std::map<string, CEntityLayer*>  TLayers;
	typedef std::vector<SEntityLayerGarbage> THeaps;

	TLayers m_layers;
	THeaps  m_garbageLayerHeaps;
	bool    m_bEntitiesUseGUIDs;
	int     m_nGeneratedFromGuid;

	//////////////////////////////////////////////////////////////////////////
	// Pool Allocators.
	//////////////////////////////////////////////////////////////////////////
public:
	bool m_bReseting;
	//////////////////////////////////////////////////////////////////////////

#ifdef ENABLE_PROFILING_CODE
public:
	struct SLayerProfile
	{
		float         fTimeMS;
		float         fTimeOn;
		bool          isEnable;
		CEntityLayer* pLayer;
	};

	std::vector<SLayerProfile> m_layerProfiles;
#endif //ENABLE_PROFILING_CODE
};

//////////////////////////////////////////////////////////////////////////
// Precache resources mode state.
//////////////////////////////////////////////////////////////////////////
extern bool gPrecacheResourcesMode;

#endif // __EntitySystem_h__
