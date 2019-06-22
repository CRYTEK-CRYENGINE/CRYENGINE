// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IEntitySystem.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/ITimer.h>
#include "EntityComponentsVector.h"
#include "SaltBufferArray.h"
#include "EntityCVars.h"
#include <CryCore/StlUtils.h>
#include <CryMemory/STLPoolAllocator.h>
#include <CryMemory/STLGlobalAllocator.h>
#include <array>

#if !defined(INCLUDE_DEBUG_ENTITY_DRAWING) && !defined(RELEASE)
#define INCLUDE_DEBUG_ENTITY_DRAWING 1
#endif

//////////////////////////////////////////////////////////////////////////
// forward declarations.
//////////////////////////////////////////////////////////////////////////
namespace Cry {
namespace Entity {

struct IReflectionRegistry;

} // ~Entity namespace
} // ~Cry namespace

struct ICVar;
struct IEntityComponent;
struct IPhysicalEntity;
struct SEntityLayerGarbage;

class CAreaManager;
class CBreakableManager;
class CCharacterBoneAttachmentManager;
class CEntity;
class CEntityArchetypeManager;
class CEntityClassRegistry;
class CEntityComponentsCache;
class CEntityLayer;
class CEntityLoadManager;
class CGeomCacheAttachmentManager;
class CPartitionGrid;
class CPhysicsEventListener;
class CProximityTriggerSystem;
class CScriptBind_Entity;

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
typedef std::unordered_map<EntityId, CEntity*, stl::hash_uint32> EntityMap;
typedef std::vector<EntityId>                                    EntityIdVector;
typedef std::vector<EntityGUID>                                  EntityGuidVector;

//////////////////////////////////////////////////////////////////////////
struct SEntityTimerEvent
{
	ISimpleEntityEventListener* pListener;
	EntityId entityId;
	CryGUID  componentInstanceGUID;
	uint32   nTimerId;
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
	IEntitySystem::StaticEntityNetworkIdentifier networkIdentifier;
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
	struct SEntityArray final : public SSaltBufferArray
	{
		SEntityArray()
		{
			m_array.fill(nullptr);
		}

		// Note +1, reason being that currently m_array[0] is always null and an invalid index
		// This is consistent with INVALID_ENTITYID
		using Array = std::array<CEntity*, EntityArraySize>;
		using iterator = Array::iterator;
		using const_iterator = Array::const_iterator;

		//! Returns an iterator referring to the start of the entity array
		//! + 1 is used to skip past m_array[0], which is always nullptr as it is INVALID_ENTITYID
		iterator begin() { return m_array.begin() + 1; }
		//! Returns an iterator referring to the start of the entity array
		//! + 1 is used to skip past m_array[0], which is always nullptr as it is INVALID_ENTITYID
		const_iterator begin() const { return m_array.begin() + 1; }
		//! Returns an iterator referring to the past-the-end element of active entities
		//! This is not the "true" end of the array, as we check the maximum possible used index
		iterator end() { return begin() + GetMaxUsedEntityIndex(); }
		//! Returns an iterator referring to the past-the-end element of active entities
		//! This is not the "true" end of the array, as we check the maximum possible used index
		const_iterator end() const { return begin() + GetMaxUsedEntityIndex(); }

		CEntity*& operator[](const SEntityIdentifier id) { return m_array[id.GetIndex()]; }
		CEntity*  operator[](const SEntityIdentifier id) const { return m_array[id.GetIndex()]; }
		CEntity*& operator[](const EntityIndex index) { return m_array[index]; }
		CEntity*  operator[](const EntityIndex index) const { return m_array[index]; }

		void fill_nullptr()
		{
			m_array.fill(nullptr);
		}

	protected:
		Array m_array;
	};

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
	virtual EntityId                          ReserveNewEntityId() final;
	virtual void                              RemoveEntity(EntityId entity, bool bForceRemoveNow = false) final;
	virtual uint32                            GetNumEntities() const final;
	virtual IEntityItPtr                      GetEntityIterator() final;
	virtual void                              SendEventToAll(SEntityEvent& event) final;
#ifndef RELEASE
	virtual void                              OnEditorSimulationModeChanged(EEditorSimulationMode mode) final;
#endif
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
	virtual ISystem*                          GetSystem() const final { return m_pISystem; }
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
	virtual void                              EnableScopedLayerSet(const char* const* pLayers, size_t layerCount, const char* const* pScopeLayers, size_t scopeLayerCount, bool isSerialized = true, IEntityLayerSetUpdateListener* pListener = nullptr) final;
	// bCaseSensitive is set to false because it's not possible in Sandbox to create 2 layers with the same name but differs in casing
	// The issue was: when switching game data files and folders to different case, FlowGraph would still reference old layer names.
	virtual IEntityLayer*                     FindLayer(const char* szLayerName, const bool bCaseSensitive = false) const final;
	virtual bool                              IsLayerEnabled(const char* layer, bool bMustBeLoaded, bool bCaseSensitive = true) const final;
	virtual bool                              ShouldSerializedEntity(IEntity* pEntity) final;
	virtual void                              RegisterPhysicCallbacks() final;
	virtual void                              UnregisterPhysicCallbacks() final;
	// New Reflection System
	virtual Cry::Entity::IReflectionRegistry* GetReflectionRegistry() const final;

	CEntityLayer*                             GetLayerForEntity(EntityId id);
	void                                      EnableLayer(IEntityLayer* pLayer, bool bIsEnable, bool bIsSerialized, bool bAffectsChildren);

	bool                                      OnBeforeSpawn(SEntitySpawnParams& params);
	void                                      OnEntityReused(IEntity* pEntity, SEntitySpawnParams& params);

	// Sets new entity timer event.
	void AddTimerEvent(SEntityTimerEvent& event, CTimeValue startTime = gEnv->pTimer->GetFrameStartTime());
	void RemoveAllTimerEvents(ISimpleEntityEventListener* pListener);
	void RemoveAllTimerEvents(EntityId id);
	void RemoveTimerEvent(ISimpleEntityEventListener* pListener, int nTimerId);


	//////////////////////////////////////////////////////////////////////////
	// Load entities from XML.
	void         LoadEntities(XmlNodeRef& objectsNode, bool bIsLoadingLevelFile) final;
	void         LoadEntities(XmlNodeRef& objectsNode, bool bIsLoadingLevelFile, const Vec3& segmentOffset) final;

	virtual bool ExtractArcheTypeLoadParams(XmlNodeRef& entityNode, SEntitySpawnParams& spawnParams) const final;
	virtual bool ExtractEntityLoadParams(XmlNodeRef& entityNode, SEntitySpawnParams& spawnParams) const final;
	virtual void BeginCreateEntities(int nAmtToCreate) final;
	virtual bool CreateEntity(XmlNodeRef& entityNode, SEntitySpawnParams& pParams, EntityId& outUsingId) final;
	virtual void EndCreateEntities() final;

#ifndef PURE_CLIENT
	virtual StaticEntityNetworkIdentifier GetStaticEntityNetworkId(EntityId id) const final;
#endif

	virtual EntityId GetEntityIdFromStaticEntityNetworkId(StaticEntityNetworkIdentifier id) const final;

	IEntity*     SpawnPreallocatedEntity(CEntity* pPrecreatedEntity, SEntitySpawnParams& params, bool bAutoInit);

	// Access to class that binds script to entity functions.
	// Used by Script proxy.
	CScriptBind_Entity* GetScriptBindEntity() { return m_pEntityScriptBinding; }

	// Access to area manager.
	IAreaManager* GetAreaManager() const final { return (IAreaManager*)(m_pAreaManager); }

	// Access to breakable manager.
	virtual IBreakableManager*       GetBreakableManager() const final         { return m_pBreakableManager; }

	CEntityLoadManager*              GetEntityLoadManager() const              { return m_pEntityLoadManager; }

	CGeomCacheAttachmentManager*     GetGeomCacheAttachmentManager() const     { return m_pGeomCacheAttachmentManager; }
	CCharacterBoneAttachmentManager* GetCharacterBoneAttachmentManager() const { return m_pCharacterBoneAttachmentManager; }

	static EntityIndex         IdToIndex(const EntityId id) { return SEntityIdentifier::GetHandleFromId(id).GetIndex(); }
	static SEntityIdentifier   IdToHandle(const EntityId id) { return SEntityIdentifier::GetHandleFromId(id); }
	static EntityId            HandleToId(const SEntityIdentifier id) { return id.GetId(); }

	EntityId                         GenerateEntityId() { return HandleToId(m_entityArray.Insert()); }

	void                             RegisterEntityGuid(const EntityGUID& guid, EntityId id);
	void                             UnregisterEntityGuid(const EntityGUID& guid);

	CPartitionGrid*                  GetPartitionGrid()          { return m_pPartitionGrid; }
	CProximityTriggerSystem*         GetProximityTriggerSystem() { return m_pProximityTriggerSystem; }

	void                             ChangeEntityName(CEntity* pEntity, const char* sNewName);

	CEntity*                         GetEntityFromID(EntityId id) const;
	ILINE bool                       HasEntity(EntityId id) const { return GetEntityFromID(id) != 0; }

	virtual void                     PurgeDeferredCollisionEvents(bool bForce = false) final;

	virtual IBSPTree3D*              CreateBSPTree3D(const IBSPTree3D::FaceList& faceList) final;
	virtual void                     ReleaseBSPTree3D(IBSPTree3D*& pTree) final;

	void                             RemoveEntity(CEntity* pEntity, bool forceRemoveImmediately = false, bool ignoreSinks = false);
	// Restore an entity that was marked for deletion (CEntity::IsGarbage)
	void                             ResurrectGarbageEntity(CEntity* pEntity);
	
	void                             EnableComponentUpdates(IEntityComponent* pComponent, bool bEnable);
	void                             EnableComponentPrePhysicsUpdates(IEntityComponent* pComponent, bool bEnable);

	// Reserve space for the static entity identifiers, note +1 being due to game rules being considered a static entity
	void                             ReserveStaticEntityIds(size_t count) { m_staticEntityIds.resize(count + 1); }
	void                             AddStaticEntityId(EntityId id, StaticEntityNetworkIdentifier networkIdentifier);

private:
	bool ValidateSpawnParameters(SEntitySpawnParams& params);

	void UpdateEntityComponents(float fFrameTime);

	void DeleteEntity(CEntity* pEntity);
	void UpdateTimers();
	
#ifdef INCLUDE_DEBUG_ENTITY_DRAWING
	void DebugDraw();
	void DebugDrawBBox(const CEntity& entity, const CVar::EEntityDebugDrawType drawMode);
	void DebugDrawHierachies(const CEntity& entity);
	void DebugDrawEntityUsage();
	void DebugDrawLayerInfo();
	void DebugDrawEntityLinks(CEntity& entity);
	void DebugDrawComponents(const CEntity& entity);
#endif // ~INCLUDE_DEBUG_ENTITY_DRAWING

	void ClearEntityArray();

	void DumpEntity(CEntity* pEntity);

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
	SEntityArray          m_entityArray;
	// Vector containing entity ids of all static entities
	// This is guaranteed to be the same across clients connecting using the same level
	// Used to allow referencing static entities very quickly over the network (using an index)
	std::vector<EntityId> m_staticEntityIds;
	DeletedEntities       m_deletedEntities;
	std::vector<CEntity*> m_deferredUsedEntities;

	EntityNamesMap        m_mapEntityNames;            // Map entity name to entity ID.

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
	Cry::Entity::IReflectionRegistry* m_pReflectionRegistry = nullptr;

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

#ifndef RELEASE
	std::unique_ptr<CEntityComponentsCache> m_entityComponentsCache;
#endif

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

	struct SProfiledEntityEvent
	{
		int numEvents = 0;
		float totalCostMs = 0.f;
		int numListenerAdditions = 0;
		int numListenerRemovals = 0;

		struct SEntityInfo
		{
			SEntityInfo() = default;
			SEntityInfo(const IEntity& entity)
				: name(entity.GetName())
				, id(entity.GetId()) {}

			string name;
			EntityId id = INVALID_ENTITYID;
		};

		SEntityInfo mostExpensiveEntity;
		float mostExpensiveEntityCostMs = 0.f;
	};

	std::array<SProfiledEntityEvent, static_cast<size_t>(Cry::Entity::EEvent::Count)> m_profiledEvents;
#endif //ENABLE_PROFILING_CODE
};

CRYENTITYDLL_API extern CEntitySystem* g_pIEntitySystem;
ILINE CEntitySystem* GetIEntitySystem() { return g_pIEntitySystem; }

//////////////////////////////////////////////////////////////////////////
// Precache resources mode state.
//////////////////////////////////////////////////////////////////////////
extern bool gPrecacheResourcesMode;
