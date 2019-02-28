// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IEntitySystem.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/ITimer.h>
#include "EntityComponentsVector.h"
#include "SaltBufferArray.h"          // SaltBufferArray<>
#include <CryCore/StlUtils.h>
#include <CryMemory/STLPoolAllocator.h>
#include <CryMemory/STLGlobalAllocator.h>
#include <array>

//////////////////////////////////////////////////////////////////////////
// forward declarations.
//////////////////////////////////////////////////////////////////////////
class CEntity;
struct ICVar;
struct IPhysicalEntity;
struct IEntityComponent;
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
class CEntitiesComponentPropertyCache;
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
	int        flags;
	string     target;

	SEntityAttachment() : child(0), parent(0), pos(ZERO), rot(ZERO), scale(ZERO), flags(0) {}
};

//////////////////////////////////////////////////////////////////////////
// Structure for extended information about loading an entity
//	Supports reusing an entity container if specified
struct SEntityLoadParams
{
	SEntitySpawnParams spawnParams;
	size_t             allocationSize;

	SEntityLoadParams();
	~SEntityLoadParams();

	SEntityLoadParams& operator=(const SEntityLoadParams& other) = delete;
	SEntityLoadParams(const SEntityLoadParams& other) = delete;
	SEntityLoadParams& operator=(SEntityLoadParams&& other) = default;
	SEntityLoadParams(SEntityLoadParams&& other) = default;

private:
	void        AddRef();
	void        RemoveRef();

	static bool CloneXmlNode(const XmlNodeRef sourceNode, XmlNodeRef destNode);
};
typedef std::vector<SEntityLoadParams>              TEntityLoadParamsContainer;

//////////////////////////////////////////////////////////////////////
class CEntitySystem final : public IEntitySystem
{
public:
	explicit CEntitySystem(ISystem* pSystem);
	~CEntitySystem();

	bool Init(ISystem* pSystem);

	// interface IEntitySystem ------------------------------------------------------
	virtual void                              Release() final;
	virtual void                              Update() final;
	virtual void                              DeletePendingEntities() final;
	virtual void                              PrePhysicsUpdate() final;
	virtual void                              Reset() final;
	virtual void                              Unload() final;
	virtual void                              PurgeHeaps() final;
	virtual IEntityClassRegistry*             GetClassRegistry() final;
	virtual IEntity*                          SpawnEntity(SEntitySpawnParams& params, bool bAutoInit = true) final;
	virtual bool                              InitEntity(IEntity* pEntity, SEntitySpawnParams& params) final;
	virtual IEntity*                          GetEntity(EntityId id) const final;
	virtual IEntity*                          FindEntityByName(const char* sEntityName) const final;
	virtual void                              ReserveEntityId(const EntityId id) final;
	virtual EntityId                          ReserveUnknownEntityId() final;
	virtual void                              RemoveEntity(EntityId entity, bool bForceRemoveNow = false) final;
	virtual uint32                            GetNumEntities() const final;
	virtual IEntityItPtr                      GetEntityIterator() final;
	virtual void                              SendEventToAll(SEntityEvent& event) final;
	virtual void                              OnEditorSimulationModeChanged(EEditorSimulationMode mode) final;
	virtual void                              OnLevelLoaded() final;
	virtual void                              OnLevelGameplayStart() final;
	virtual int                               QueryProximity(SEntityProximityQuery& query) final;
	virtual void                              ResizeProximityGrid(int nWidth, int nHeight) final;
	virtual int                               GetPhysicalEntitiesInBox(const Vec3& origin, float radius, IPhysicalEntity**& pList, int physFlags) const final;
	virtual IEntity*                          GetEntityFromPhysics(IPhysicalEntity* pPhysEntity) const final;
	virtual void                              AddSink(IEntitySystemSink* pSink, std::underlying_type<SinkEventSubscriptions>::type subscriptions) final;
	virtual void                              RemoveSink(IEntitySystemSink* pSink) final;
	virtual void                              PauseTimers(bool bPause, bool bResume = false) final;
	virtual bool                              IsIDUsed(EntityId nID) const final;
	virtual void                              GetMemoryStatistics(ICrySizer* pSizer) const final;
	virtual ISystem*                          GetSystem() const final { return m_pISystem; };
	virtual void                              SetNextSpawnId(EntityId id) final;
	virtual void                              ResetAreas() final;
	virtual void                              UnloadAreas() final;

	virtual void                              AddEntityLayerListener(const char* szLayerName, IEntityLayerListener* pListener, const bool bCaseSensitive = true) final;
	virtual void                              RemoveEntityLayerListener(const char* szLayerName, IEntityLayerListener* pListener, const bool bCaseSensitive = true) final;

	virtual EntityId                          FindEntityByGuid(const EntityGUID& guid) const final;

	virtual IEntityArchetype*                 LoadEntityArchetype(XmlNodeRef oArchetype) final;
	virtual IEntityArchetype*                 LoadEntityArchetype(const char* sArchetype) final;
	virtual void                              UnloadEntityArchetype(const char* sArchetype) final;
	virtual IEntityArchetype*                 CreateEntityArchetype(IEntityClass* pClass, const char* sArchetype) final;
	virtual void                              RefreshEntityArchetypesInRegistry() final;
	virtual void                              SetEntityArchetypeManagerExtension(IEntityArchetypeManagerExtension* pEntityArchetypeManagerExtension) final;
	virtual IEntityArchetypeManagerExtension* GetEntityArchetypeManagerExtension() const final;

	virtual void                              Serialize(TSerialize ser) final;

	virtual void                              DumpEntities() final;

	virtual void                              ResumePhysicsForSuppressedEntities(bool bWakeUp) final;
	virtual void                              SaveInternalState(struct IDataWriteStream& writer) const final;
	virtual void                              LoadInternalState(struct IDataReadStream& reader) final;

	virtual int                               GetLayerId(const char* szLayerName) const final;
	virtual const char*                       GetLayerName(int layerId) const final;
	virtual int                               GetLayerChildCount(const char* szLayerName) const final;
	virtual const char*                       GetLayerChild(const char* szLayerName, int childIdx) const final;

	virtual int                               GetVisibleLayerIDs(uint8* pLayerMask, const uint32 maxCount) const final;

	virtual void                              ToggleLayerVisibility(const char* layer, bool isEnabled, bool includeParent = true) final;

	virtual void                              ToggleLayersBySubstring(const char* pSearchSubstring, const char* pExceptionSubstring, bool isEnable) final;

	virtual void                              LockSpawning(bool lock) final { m_bLocked = lock; }

	virtual bool                              OnLoadLevel(const char* szLevelPath) final;

	virtual IEntityLayer*                     AddLayer(const char* szName, const char* szParent, uint16 id, bool bHasPhysics, int specs, bool bDefaultLoaded) final;
	virtual void                              LoadLayers(const char* dataFile) final;
	virtual void                              LinkLayerChildren() final;
	virtual void                              AddEntityToLayer(const char* layer, EntityId id) final;
	virtual void                              RemoveEntityFromLayers(EntityId id) final;
	virtual void                              ClearLayers() final;
	virtual void                              EnableDefaultLayers(bool isSerialized = true) final;
	virtual void                              EnableLayer(const char* layer, bool isEnable, bool isSerialized = true) final;
	virtual void                              EnableLayerSet(const char* const* pLayers, size_t layerCount, bool bIsSerialized = true, IEntityLayerSetUpdateListener* pListener = nullptr) final;
	// bCaseSensitive is set to false because it's not possible in Sandbox to create 2 layers with the same name but differs in casing
	// The issue was: when switching game data files and folders to different case, FlowGraph would still reference old layer names.
	virtual IEntityLayer*                     FindLayer(const char* szLayerName, const bool bCaseSensitive = false) const final;
	virtual bool                              IsLayerEnabled(const char* layer, bool bMustBeLoaded, bool bCaseSensitive = true) const final;
	virtual bool                              ShouldSerializedEntity(IEntity* pEntity) final;
	virtual void                              RegisterPhysicCallbacks() final;
	virtual void                              UnregisterPhysicCallbacks() final;

	CEntityLayer*                             GetLayerForEntity(EntityId id);
	void                                      EnableLayer(IEntityLayer* pLayer, bool bIsEnable, bool bIsSerialized, bool bAffectsChildren);

	bool                                      OnBeforeSpawn(SEntitySpawnParams& params);
	void                                      OnEntityReused(IEntity* pEntity, SEntitySpawnParams& params);

	// Sets new entity timer event.
	void AddTimerEvent(SEntityTimerEvent& event, CTimeValue startTime = gEnv->pTimer->GetFrameStartTime());

	//////////////////////////////////////////////////////////////////////////
	// Load entities from XML.
	void         LoadEntities(XmlNodeRef& objectsNode, bool bIsLoadingLevelFile) final;
	void         LoadEntities(XmlNodeRef& objectsNode, bool bIsLoadingLevelFile, const Vec3& segmentOffset) final;

	virtual bool ExtractArcheTypeLoadParams(XmlNodeRef& entityNode, SEntitySpawnParams& spawnParams) const final;
	virtual bool ExtractEntityLoadParams(XmlNodeRef& entityNode, SEntitySpawnParams& spawnParams) const final;
	virtual void BeginCreateEntities(int nAmtToCreate) final;
	virtual bool CreateEntity(XmlNodeRef& entityNode, SEntitySpawnParams& pParams, EntityId& outUsingId) final;
	virtual void EndCreateEntities() final;

	IEntity*     SpawnPreallocatedEntity(CEntity* pPrecreatedEntity, SEntitySpawnParams& params, bool bAutoInit);

	//////////////////////////////////////////////////////////////////////////
	// Called from CEntity implementation.
	//////////////////////////////////////////////////////////////////////////
	void RemoveTimerEvent(EntityId id, int nTimerId);
	bool HasTimerEvent(EntityId id, int nTimerId);

	// Access to class that binds script to entity functions.
	// Used by Script proxy.
	CScriptBind_Entity* GetScriptBindEntity() { return m_pEntityScriptBinding; };

	// Access to area manager.
	IAreaManager* GetAreaManager() const final { return (IAreaManager*)(m_pAreaManager); }

	// Access to breakable manager.
	virtual IBreakableManager*       GetBreakableManager() const final         { return m_pBreakableManager; };

	CEntityLoadManager*              GetEntityLoadManager() const              { return m_pEntityLoadManager; }

	CGeomCacheAttachmentManager*     GetGeomCacheAttachmentManager() const     { return m_pGeomCacheAttachmentManager; }
	CCharacterBoneAttachmentManager* GetCharacterBoneAttachmentManager() const { return m_pCharacterBoneAttachmentManager; }

	static EntityIndex         IdToIndex(const EntityId id) { return CSaltHandle::GetHandleFromId(id).GetIndex(); }
	static CSaltHandle         IdToHandle(const EntityId id) { return CSaltHandle::GetHandleFromId(id); }
	static EntityId            HandleToId(const CSaltHandle id) { return id.GetId(); }

	EntityId                         GenerateEntityId(bool bStaticId);

	void                             RegisterEntityGuid(const EntityGUID& guid, EntityId id);
	void                             UnregisterEntityGuid(const EntityGUID& guid);

	CPartitionGrid*                  GetPartitionGrid()          { return m_pPartitionGrid; }
	CProximityTriggerSystem*         GetProximityTriggerSystem() { return m_pProximityTriggerSystem; }

	void                             ChangeEntityName(CEntity* pEntity, const char* sNewName);

	CEntity*                         GetEntityFromID(EntityId id) const;
	ILINE bool                       HasEntity(EntityId id) const { return GetEntityFromID(id) != 0; };

	virtual void                     PurgeDeferredCollisionEvents(bool bForce = false) final;

	virtual IBSPTree3D*              CreateBSPTree3D(const IBSPTree3D::FaceList& faceList) final;
	virtual void                     ReleaseBSPTree3D(IBSPTree3D*& pTree) final;

	void                             RemoveEntity(CEntity* pEntity, bool forceRemoveImmediately = false, bool ignoreSinks = false);
	// Restore an entity that was marked for deletion (CEntity::IsGarbage)
	void                             ResurrectGarbageEntity(CEntity* pEntity);
	
	void                             EnableComponentUpdates(IEntityComponent* pComponent, bool bEnable);
	void                             EnableComponentPrePhysicsUpdates(IEntityComponent* pComponent, bool bEnable);

private:
	bool ValidateSpawnParameters(SEntitySpawnParams& params);

	void UpdateEntityComponents(float fFrameTime);

	void DeleteEntity(CEntity* pEntity);
	void UpdateTimers();
	void DebugDraw(const CEntity* const pEntity, float fUpdateTime);

	void DebugDrawEntityUsage();
	void DebugDrawLayerInfo();

	void ClearEntityArray();

	void DumpEntity(CEntity* pEntity);

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

	typedef std::vector<OnEventSink, stl::STLGlobalAllocator<OnEventSink>>                                                                                                                              EntitySystemOnEventSinks;
	typedef std::vector<CEntity*>                                                                                                                                                                       DeletedEntities;
	typedef std::multimap<CTimeValue, SEntityTimerEvent, std::less<CTimeValue>, stl::STLPoolAllocator<std::pair<const CTimeValue, SEntityTimerEvent>, stl::PoolAllocatorSynchronizationSinglethreaded>> EntityTimersMap;
	typedef std::multimap<const char*, EntityId, stl::less_stricmp<const char*>>                                                                                                                        EntityNamesMap;
	typedef std::vector<SEntityTimerEvent>                                                                                                                                                              EntityTimersVector;

	std::array<std::vector<IEntitySystemSink*>, (size_t)SinkEventSubscriptions::Count> m_sinks;

	ISystem*              m_pISystem;
	std::array<CEntity*, CSaltBufferArray::GetTSize()> m_EntityArray;                    // [id.GetIndex()]=CEntity
	DeletedEntities       m_deletedEntities;
	std::vector<CEntity*> m_deferredUsedEntities;

	EntityNamesMap        m_mapEntityNames;            // Map entity name to entity ID.

	CSaltBufferArray    m_EntitySaltBuffer;               // used to create new entity ids (with uniqueid=salt)
	//////////////////////////////////////////////////////////////////////////

	CEntityComponentsVector<SMinimalEntityComponentRecord> m_updatedEntityComponents;
	CEntityComponentsVector<SMinimalEntityComponentRecord> m_prePhysicsUpdatedEntityComponents;

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

	typedef std::unordered_map<EntityGUID, EntityId> EntityGuidMap;
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
	bool m_bLocked;
	bool m_bSupportLegacy64bitGuids = false;

	friend class CEntityItMap;

	typedef std::map<string, CEntityLayer*>  TLayers;
	typedef std::vector<SEntityLayerGarbage> THeaps;

	TLayers m_layers;
	THeaps  m_garbageLayerHeaps;

	std::unique_ptr<CEntitiesComponentPropertyCache> m_entitiesPropertyCache;

public:
	std::unique_ptr<class CEntityObjectDebugger> m_pEntityObjectDebugger;

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
