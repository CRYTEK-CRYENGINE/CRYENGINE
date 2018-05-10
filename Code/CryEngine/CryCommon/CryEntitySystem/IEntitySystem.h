// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the CRYENTITYDLL_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// CRYENTITYDLL_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef CRYENTITYDLL_EXPORTS
	#define CRYENTITYDLL_API DLL_EXPORT
#else
	#define CRYENTITYDLL_API DLL_IMPORT
#endif

#define _DISABLE_AREASOLID_

#include <CryEntitySystem/IEntityComponent.h>
#include <CryEntitySystem/IEntity.h>
#include <CryEntitySystem/IEntityClass.h>
#include <CryEntitySystem/IEntityLayer.h>
#include <CryCore/smartptr.h>
#include <CryMath/Cry_Geo.h>
#include <CrySystem/XML/IXml.h>

// Forward declarations.
struct ISystem;
struct IEntitySystem;
class ICrySizer;
struct IEntity;
struct SpawnParams;
struct IPhysicalEntity;
struct IBreakEventListener;
struct SRenderNodeCloneLookup;
struct EventPhysRemoveEntityParts;
struct IBreakableManager;
struct SComponentRegisteredEvents;
class CEntity;

//! SpecType for entity layers.
//! Add new bits to update. Do not just rename, cause values are used for saving levels.
enum ESpecType
{
	eSpecType_PC      = BIT(0),
	eSpecType_XBoxOne = BIT(1),
	eSpecType_PS4     = BIT(2),
	eSpecType_All     = eSpecType_PC | eSpecType_XBoxOne | eSpecType_PS4
};

//! Area Manager Interface.
struct IArea
{
	// <interfuscator:shuffle>
	virtual ~IArea() = default;
	virtual size_t         GetEntityAmount() const = 0;
	virtual const EntityId GetEntityByIdx(size_t const index) const = 0;
	virtual int            GetGroup() const = 0;
	virtual int            GetPriority() const = 0;
	virtual int            GetID() const = 0;
	virtual AABB           GetAABB() const = 0;
	virtual float          GetExtent(EGeomForm eForm) = 0;
	virtual void           GetRandomPoints(Array<PosNorm> points, CRndGen seed, EGeomForm eForm) const = 0;
	virtual bool           IsPointInside(Vec3 const& pointToTest) const = 0;
	// </interfuscator:shuffle>
};

//! EventListener interface for the AreaManager.
struct IAreaManagerEventListener
{
	// <interfuscator:shuffle>
	virtual ~IAreaManagerEventListener(){}

	//! Callback event.
	virtual void OnAreaManagerEvent(EEntityEvent event, EntityId TriggerEntityID, IArea* pArea) = 0;
	// </interfuscator:shuffle>
};

//! Structure for AudioArea AreaManager queries.
struct SAudioAreaInfo
{
	SAudioAreaInfo()
		: amount(0.0f)
		, audioEnvironmentId(CryAudio::InvalidEnvironmentId)
		, envProvidingEntityId(INVALID_ENTITYID)
	{}

	explicit SAudioAreaInfo(
	  float const _amount,
	  CryAudio::EnvironmentId const _audioEnvironmentId,
	  EntityId const _envProvidingEntityId)
		: amount(_amount)
		, audioEnvironmentId(_audioEnvironmentId)
		, envProvidingEntityId(_envProvidingEntityId)
	{}

	float                   amount;
	CryAudio::EnvironmentId audioEnvironmentId;
	EntityId                envProvidingEntityId;
};

struct IBSPTree3D
{
	typedef DynArray<Vec3>  CFace;
	typedef DynArray<CFace> FaceList;

	virtual ~IBSPTree3D() {}
	virtual bool   IsInside(const Vec3& vPos) const = 0;
	virtual void   GetMemoryUsage(ICrySizer* pSizer) const = 0;

	virtual size_t WriteToBuffer(void* pBuffer) const = 0;
	virtual void   ReadFromBuffer(const void* pBuffer) = 0;
};

struct IAreaManager
{
	// <interfuscator:shuffle>
	virtual ~IAreaManager(){}
	virtual size_t             GetAreaAmount() const = 0;
	virtual IArea const* const GetArea(size_t const nAreaIndex) const = 0;
	virtual size_t             GetOverlappingAreas(const AABB& bb, PodArray<IArea*>& list) const = 0;

	//! Additional Query based on position. Returns only the areas that provide audio environments.
	//! Needs preallocated space to write numMaxResults to pResults.
	//! The number of found areas is returned in numResults.
	//! Note: This method is currently only called by the audio thread and not thread safe!
	//! \return True on success or false on error or if provided structure was too small.
	virtual bool QueryAudioAreas(Vec3 const& pos, SAudioAreaInfo* const pResults, size_t const numMaxResults, size_t& numResults) = 0;

	//! Query areas linked to other entities (these links are shape links)
	//! fills out a list of entities and sets outAndMaxResults to the size of results
	//! \return true if the array was large enough, or false if there was more then outAndMaxResults attached to the entity
	virtual bool GetLinkedAreas(EntityId linkedId, EntityId* pOutArray, int& outAndMaxResults) const = 0;

	virtual void DrawLinkedAreas(EntityId linkedId) const = 0;

	//! Registers EventListener to the AreaManager.
	virtual void AddEventListener(IAreaManagerEventListener* pListener) = 0;
	virtual void RemoveEventListener(IAreaManagerEventListener* pListener) = 0;

	//! Invokes a re-compilation of the entire area grid
	virtual void SetAreasDirty() = 0;

	//! Invokes a partial recomp of the grid (faster)
	virtual void SetAreaDirty(IArea* pArea) = 0;

	//! Passed in entity exits all areas. Intended for when players are killed.
	virtual void ExitAllAreas(EntityId const entityId) = 0;

	//! Puts the passed entity ID into the update list for the next update.
	virtual void MarkEntityForUpdate(EntityId const entityId) = 0;

	//! Forces an audio listener update against the passed area.
	virtual void TriggerAudioListenerUpdate(IArea const* const _pArea) = 0;
	// </interfuscator:shuffle>
};

//! Entity iterator interface. This interface is used to traverse trough all the entities in an entity system.
//! In a way, this iterator works a lot like a stl iterator.
struct IEntityIt
{
	// <interfuscator:shuffle>
	virtual ~IEntityIt(){}

	virtual void AddRef() = 0;

	//! Deletes this iterator and frees any memory it might have allocated.
	virtual void Release() = 0;

	//! Checks whether current iterator position is the end position.
	//! \return true if iterator at end position.
	virtual bool IsEnd() = 0;

	//! Retrieves next entity.
	//! \return Pointer to the entity that the iterator points to before it goes to the next.
	virtual IEntity* Next() = 0;

	//! Retrieves current entity.
	//! \return Entity that the iterator points to.
	virtual IEntity* This() = 0;

	//! Positions the iterator at the beginning of the entity list.
	virtual void MoveFirst() = 0;
	// </interfuscator:shuffle>
};

typedef _smart_ptr<IEntityIt> IEntityItPtr;

//! Callback interface for a class that wants to be aware when new entities are being spawned or removed.
//! A class that implements this interface will be called every time a new entity is spawned, removed, or when an entity container is to be spawned.
struct IEntitySystemSink
{
	// <interfuscator:shuffle>
	virtual ~IEntitySystemSink(){}

	//! This callback is called before this entity is spawned. The entity will be created and added to the list of entities,
	//! if this function returns true. Returning false will abort the spawning process.
	//! \param params Parameters that will be used to spawn the entity.
	virtual bool OnBeforeSpawn(SEntitySpawnParams& params) = 0;

	//! This callback is called when this entity has finished spawning.
	//! The entity has been created and added to the list of entities, but has not been initialized yet.
	//! \param pEntity Entity that was just spawned.
	//! \param params
	virtual void OnSpawn(IEntity* pEntity, SEntitySpawnParams& params) = 0;

	//! Called when an entity is being removed.
	//! \param pEntity Entity that is being removed. This entity is still fully valid.
	//! \return true to allow removal, false to deny.
	virtual bool OnRemove(IEntity* pEntity) = 0;

	//! Called when an entity has been reused. You should clean up when this is called.
	//! \param pEntity Entity that is being reused. This entity is still fully valid and with its new EntityId.
	//! \param params The new params this entity is using.
	virtual void OnReused(IEntity* pEntity, SEntitySpawnParams& params) = 0;

	//! Collect memory informations
	//! \param pSizer Sizer class used to collect the memory informations.
	virtual void GetMemoryUsage(class ICrySizer* pSizer) const {};
	// </interfuscator:shuffle>
};

//! Interface to the entity archetype.
struct IEntityArchetype
{
	// <interfuscator:shuffle>
	virtual ~IEntityArchetype(){}

	//! Retrieve entity class of the archetype.
	virtual IEntityClass* GetClass() const = 0;

	virtual const char*   GetName() const = 0;
	virtual IScriptTable* GetProperties() = 0;
	virtual XmlNodeRef    GetObjectVars() = 0;
	virtual void          LoadFromXML(XmlNodeRef& propertiesNode, XmlNodeRef& objectVarsNode) = 0;
	// </interfuscator:shuffle>
};

//! Interface entity archetype manager extension. Allows to react to archetype changes.
struct IEntityArchetypeManagerExtension
{
	CRY_DEPRECATED_ENTTIY_ARCHETYPE IEntityArchetypeManagerExtension() = default;

	virtual ~IEntityArchetypeManagerExtension() = default;

	//! Called when new archetype is added.
	virtual void OnArchetypeAdded(IEntityArchetype& archetype) = 0;
	//! Called when an archetype is about to be removed.
	virtual void OnArchetypeRemoved(IEntityArchetype& archetype) = 0;
	//! Called when all archetypes are about to be removed.
	virtual void OnAllArchetypesRemoved() = 0;

	//! Called during archetype serialization, allows to inject extra data.
	virtual void Serialize(IEntityArchetype& archetype, Serialization::IArchive& archive) = 0;
	//! Called to load archetype extension data from the XML.
	virtual void LoadFromXML(IEntityArchetype& archetype, XmlNodeRef& archetypeNode) = 0;
	//! Called to save archetype extension data to the XML.
	virtual void SaveToXML(IEntityArchetype& archetype, XmlNodeRef& archetypeNode) = 0;
};

struct IEntityLayerSetUpdateListener
{
	virtual void LayerEnablingEvent(const char* szLayerName, bool bEnabled, bool bSerialized) = 0;
protected:
	~IEntityLayerSetUpdateListener() {}
};

struct IEntityLayerListener
{
	virtual void LayerEnabled(bool bActivated) = 0;
protected:
	~IEntityLayerListener() {}
};

//! Structure used by proximity query in entity system.
struct SEntityProximityQuery
{
	//! Report entities within this bounding box.
	AABB          box;

	IEntityClass* pEntityClass;
	uint32        nEntityFlags;

	// Output.
	IEntity** pEntities;
	int       nCount;

	SEntityProximityQuery()
	{
		nCount = 0;
		pEntities = 0;
		pEntityClass = 0;
		nEntityFlags = 0;
	}
};

struct IEntitySystemEngineModule : public Cry::IDefaultModule
{
	CRYINTERFACE_DECLARE_GUID(IEntitySystemEngineModule, "bbbb58b0-ff97-49cf-bffe-254420cd166f"_cry_guid);
};

//! Interface to the system that manages the entities in the game.
//! Interface to the system that manages the entities in the game, their creation, deletion and upkeep.
//! The entities are kept in a map indexed by their unique entity ID. The entity system updates only unbound entities every frame (bound entities
//! are updated by their parent entities), and deletes the entities marked as garbage every frame before the update.
//! The entity system also keeps track of entities that have to be drawn last and with more zbuffer resolution.
struct IEntitySystem
{
	enum SinkEventSubscriptions : uint32
	{
		OnBeforeSpawn = BIT(0),
		OnSpawn       = BIT(1),
		OnRemove      = BIT(2),
		OnReused      = BIT(3),

		Last          = OnReused,
		Count         = 4,

		AllSinkEvents = ~0u,
	};

	//! Determines the state of simulation in the Editor, see OnEditorSimulationModeChanged
	enum class EEditorSimulationMode
	{
		//! User is in editing mode
		Editing,
		//! User is in game (ctrl + g / F5)
		InGame,
		// Entities are being simulated without player being active
		Simulation
	};

	// <interfuscator:shuffle>
	virtual ~IEntitySystem(){}

	//! Releases entity system.
	virtual void Release() = 0;

	//! Updates entity system and all entities before physics is called (or as early as possible after input in the multithreaded physics case).
	//! This function executes once per frame.
	virtual void PrePhysicsUpdate() = 0;

	//! Updates entity system and all entities. This function executes once a frame.
	virtual void Update() = 0;

	//! Resets whole entity system, and destroy all entities.
	virtual void Reset() = 0;

	//! Unloads whole entity system - should be called when level is unloaded.
	virtual void Unload() = 0;
	virtual void PurgeHeaps() = 0;

	//! Deletes any pending entities (which got marked for deletion).
	virtual void DeletePendingEntities() = 0;

	//! Retrieves the entity class registry interface.
	//! \return Pointer to the valid entity class registry interface.
	virtual IEntityClassRegistry* GetClassRegistry() = 0;

	//! Spawns a new entity according to the data in the Entity Descriptor.
	//! \param params		Entity descriptor structure that describes what kind of entity needs to be spawned.
	//! \param bAutoInit	If true, automatically initialize entity.
	//! \return If successful, the spawned entity, otherwise nullptr.
	//! \par Example
	//! \include CryEntitySystem/Examples/SpawnEntity.cpp
	virtual IEntity* SpawnEntity(SEntitySpawnParams& params, bool bAutoInit = true) = 0;

	//! Initialize entity if entity was spawned not initialized (with bAutoInit false in SpawnEntity).
	//! \note Used only by Editor, to setup properties & other things before initializing entity, do not use this directly.
	//! \param pEntity Pointer to just spawned entity object.
	//! \param params  Entity descriptor structure that describes what kind of entity needs to be spawned.
	//! \return true if successfully initialized entity.
	virtual bool InitEntity(IEntity* pEntity, SEntitySpawnParams& params) = 0;
	//! Retrieves entity from its unique id.
	//! \param id Unique ID of the entity required.
	//! \return Entity if one with such an ID exists, and NULL if no entity could be matched with the id
	//! \par Example
	//! \include CryEntitySystem/Examples/GetEntity.cpp
	virtual IEntity* GetEntity(EntityId id) const = 0;

	//! Find first entity with given name.
	//! \param sEntityName Name to look for.
	//! \return The entity if found, 0 if failed.
	//! \par Example
	//! \include CryEntitySystem/Examples/FindEntityByName.cpp
	virtual IEntity* FindEntityByName(const char* sEntityName) const = 0;

	//! \note Might be needed to call before loading of entities to be sure we get the requested IDs.
	//! \param id Must not be 0.
	virtual void ReserveEntityId(const EntityId id) = 0;

	//! Reserves a dynamic entity id.
	virtual EntityId ReserveUnknownEntityId() = 0;

	//! Removes an entity by ID.
	//! \param entity          Id of the entity to be removed.
	//! \param bForceRemoveNow If true, forces immediately delete of entity, overwise will delete entity on next update.
	virtual void RemoveEntity(EntityId entity, bool bForceRemoveNow = false) = 0;

	//! \return Number of stored in entity system.
	virtual uint32 GetNumEntities() const = 0;

	//! Gets a entity iterator.
	//! This iterator interface can be used to traverse all the entities in this entity system.
	//! \par Example
	//! \include CryEntitySystem/Examples/TraverseEntities.cpp
	virtual IEntityItPtr GetEntityIterator() = 0;

	//! Sends the same event to all entities in Entity System.
	//! \param event Event to send.
	virtual void SendEventToAll(SEntityEvent& event) = 0;

	//! Sent when game mode in Editor is changed
	virtual void OnEditorSimulationModeChanged(EEditorSimulationMode mode) = 0;

	//! Sent after the level has finished loading
	virtual void OnLevelLoaded() = 0;

	//! Sent when level is loaded and gameplay can start, triggers start of simulation for entities
	virtual void OnLevelGameplayStart() = 0;

	//! Get all entities within proximity of the specified bounding box.
	//! \note Query is not exact, entities reported can be a few meters away from the bounding box.
	virtual int QueryProximity(SEntityProximityQuery& query) = 0;

	//! Resizes the proximity grid.
	//! \note Call this when you know dimensions of the level, before entities are created.
	virtual void ResizeProximityGrid(int nWidth, int nHeight) = 0;

	//! Gets all entities in specified radius.
	//! \param origin
	//! \param radius
	//! \param pList
	//! \param physFlags Is one or more of PhysicalEntityFlag.
	virtual int GetPhysicalEntitiesInBox(const Vec3& origin, float radius, IPhysicalEntity**& pList, int physFlags = (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4)) const = 0;

	//! Retrieves host entity from the physical entity.
	virtual IEntity* GetEntityFromPhysics(IPhysicalEntity* pPhysEntity) const = 0;

	//! Adds the sink of the entity system. The sink is a class which implements IEntitySystemSink.
	//! \param sink Pointer to the sink, must not be 0.
	//! \param subscription - combination of SinkEventSubscriptions flags specifying which events to receive.
	virtual void AddSink(IEntitySystemSink* sink, std::underlying_type<SinkEventSubscriptions>::type subscriptions) = 0;

	//! Removes listening sink from the entity system. The sink is a class which implements IEntitySystemSink.
	//! \param sink Pointer to the sink, must not be 0.
	virtual void RemoveSink(IEntitySystemSink* sink) = 0;

	//! Pause all entity timers.
	//! \param bPause true to pause timer, false to resume.
	virtual void PauseTimers(bool bPause, bool bResume = false) = 0;

	//! Checks whether a given entity ID is already used.
	virtual bool IsIDUsed(EntityId nID) const = 0;

	//! Puts the memory statistics of the entities into the given sizer object according to the specifications in interface ICrySizer.
	virtual void GetMemoryStatistics(ICrySizer* pSizer) const = 0;

	//! Gets pointer to original ISystem.
	virtual ISystem* GetSystem() const = 0;

	//! Extract entity archetype load parameters from XML.
	//! \param entityNode XML archetype entity node.
	//! \param spawnParams Archetype entity spawn parameters.
	virtual bool ExtractArcheTypeLoadParams(XmlNodeRef& entityNode, SEntitySpawnParams& spawnParams) const = 0;

	//! Extract entity load parameters from XML.
	//! \param entityNode XML entity node.
	//! \param spawnParams Entity spawn parameters.
	virtual bool ExtractEntityLoadParams(XmlNodeRef& entityNode, SEntitySpawnParams& spawnParams) const = 0;

	//! Creates entity from XML. To ensure that entity links work, call BeginCreateEntities.
	//! Before using CreateEntity, then call EndCreateEntities when you're done to setup all the links.
	//! \param entityNode XML entity node.
	//! \param spawnParams Entity spawn parameters.
	//! \param outusingID Resulting ID.
	virtual void BeginCreateEntities(int amtToCreate) = 0;
	virtual bool CreateEntity(XmlNodeRef& entityNode, SEntitySpawnParams& pParams, EntityId& outUsingId) = 0;
	virtual void EndCreateEntities() = 0;

	//! Loads entities exported from Editor.
	//! bIsLoadingLevelFile indicates if the loaded entities come from the original level file.
	virtual void LoadEntities(XmlNodeRef& objectsNode, bool bIsLoadingLevelFile) = 0;
	virtual void LoadEntities(XmlNodeRef& objectsNode, bool bIsLoadingLevelFile, const Vec3& segmentOffest) = 0;

	//! Register entity layer listener
	virtual void AddEntityLayerListener(const char* szLayerName, IEntityLayerListener* pListener, const bool bCaseSensitive = true) = 0;
	virtual void RemoveEntityLayerListener(const char* szLayerName, IEntityLayerListener* pListener, const bool bCaseSensitive = true) = 0;

	// Entity GUIDs

	//! Finds entity by Entity GUID.
	virtual EntityId FindEntityByGuid(const EntityGUID& guid) const = 0;

	//! Gets a pointer to access to area manager.
	virtual IAreaManager* GetAreaManager() const = 0;

	//! \return The breakable manager interface.
	virtual IBreakableManager* GetBreakableManager() const = 0;

	//////////////////////////////////////////////////////////////////////////
	//! Entity archetypes.
	virtual IEntityArchetype*                 LoadEntityArchetype(XmlNodeRef oArchetype) = 0;
	virtual IEntityArchetype*                 LoadEntityArchetype(const char* sArchetype) = 0;
	virtual void                              UnloadEntityArchetype(const char* sArchetype) = 0;
	virtual IEntityArchetype*                 CreateEntityArchetype(IEntityClass* pClass, const char* sArchetype) = 0;
	virtual void                              RefreshEntityArchetypesInRegistry() = 0;
	virtual void                              SetEntityArchetypeManagerExtension(IEntityArchetypeManagerExtension* pEntityArchetypeManagerExtension) = 0;
	virtual IEntityArchetypeManagerExtension* GetEntityArchetypeManagerExtension() const = 0;
	//////////////////////////////////////////////////////////////////////////

	//! Serializes basic entity system members (timers etc. ) to/from a savegame;
	virtual void Serialize(TSerialize ser) = 0;

	//! Makes sure the next SpawnEntity will use the id provided (if it's in use, the current entity is deleted).
	virtual void SetNextSpawnId(EntityId id) = 0;

	//! Resets any area state for the specified entity.
	virtual void ResetAreas() = 0;

	//! Unloads any area state for the specified entity.
	virtual void UnloadAreas() = 0;

	//! Dumps entities in system.
	virtual void DumpEntities() = 0;

	//! Do not spawn any entities unless forced to.
	virtual void LockSpawning(bool lock) = 0;

	//! Handles entity-related loading of a level
	virtual bool OnLoadLevel(const char* szLevelPath) = 0;

	//! Add entity layer.
	virtual IEntityLayer* AddLayer(const char* szName, const char* szParent, uint16 id, bool bHasPhysics, int specs, bool bDefaultLoaded) = 0;

	//! Load layer infos.
	virtual void LoadLayers(const char* dataFile) = 0;

	//! Reorganize layer children.
	virtual void LinkLayerChildren() = 0;

	//! Add entity to entity layer.
	virtual void AddEntityToLayer(const char* layer, EntityId id) = 0;

	//! Remove entity from all layers.
	virtual void RemoveEntityFromLayers(EntityId id) = 0;

	//! Clear list of entity layers.
	virtual void ClearLayers() = 0;

	//! Enable all the default layers.
	virtual void EnableDefaultLayers(bool isSerialized = true) = 0;

	//! Enable entity layer.
	virtual void EnableLayer(const char* layer, bool isEnable, bool isSerialized = true) = 0;

	//! Enable entity layers specified in the layer set and hide all other known layers.
	virtual void EnableLayerSet(const char* const* pLayers, size_t layerCount, bool isSerialized = true, IEntityLayerSetUpdateListener* pListener = nullptr) = 0;

	//! Find a layer with a given name.
	virtual IEntityLayer* FindLayer(const char* szLayerName, const bool bCaseSensitive = true) const = 0;

	//! Is layer with given name enabled ?.
	virtual bool IsLayerEnabled(const char* layer, bool bMustBeLoaded, bool bCaseSensitive = true) const = 0;

	//! Returns true if entity is not in a layer or the layer is enabled/serialized.
	virtual bool ShouldSerializedEntity(IEntity* pEntity) = 0;

	//! Register callbacks from Physics System.
	virtual void RegisterPhysicCallbacks() = 0;
	virtual void UnregisterPhysicCallbacks() = 0;

	virtual void PurgeDeferredCollisionEvents(bool bForce = false) = 0;

	//! Resets physical simulation suppressed by LiveCreate during "edit mode".
	//! Called by LiveCreate subsystem when user resumed normal game mode.
	virtual void ResumePhysicsForSuppressedEntities(bool bWakeUp) = 0;

	//! Save/Load internal state (used mostly for LiveCreate).
	virtual void SaveInternalState(struct IDataWriteStream& writer) const = 0;
	virtual void LoadInternalState(struct IDataReadStream& reader) = 0;

	//! Get the layer index, -1 if not found.
	virtual int GetLayerId(const char* szLayerName) const = 0;

	//! Get the layer name, from an id.
	virtual const char* GetLayerName(int layerId) const = 0;

	//! Gets the number of children for a layer.
	virtual int GetLayerChildCount(const char* szLayerName) const = 0;

	//! Gets the name of the specified child layer.
	virtual const char* GetLayerChild(const char* szLayerName, int childIdx) const = 0;

	//! Get mask for visible layers, returns layer count.
	virtual int GetVisibleLayerIDs(uint8* pLayerMask, const uint32 maxCount) const = 0;

	//! Toggle layer visibility (in-game only), used by LiveCreate, non recursive.
	virtual void ToggleLayerVisibility(const char* layer, bool isEnabled, bool includeParent = true) = 0;

	//! Toggle layer visibility for all layers containing pSearchSubstring with the exception of any with pExceptionSubstring.
	virtual void        ToggleLayersBySubstring(const char* pSearchSubstring, const char* pExceptionSubstring, bool isEnable) = 0;

	virtual IBSPTree3D* CreateBSPTree3D(const IBSPTree3D::FaceList& faceList) = 0;
	virtual void        ReleaseBSPTree3D(IBSPTree3D*& pTree) = 0;
	// </interfuscator:shuffle>

	//! Registers Entity Event's listeners.
	inline void AddEntityEventListener(EntityId entityId, EEntityEvent event, IEntityEventListener* pListener)
	{
		if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId))
		{
			pEntity->AddEventListener(event, pListener);
		}
	}

	inline void RemoveEntityEventListener(EntityId entityId, EEntityEvent event, IEntityEventListener* pListener)
	{
		if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId))
		{
			pEntity->RemoveEventListener(event, pListener);
		}
	}
};

extern "C"
{
	CRYENTITYDLL_API struct IEntitySystem* CreateEntitySystem(ISystem* pISystem);
}

typedef struct IEntitySystem* (* PFNCREATEENTITYSYSTEM)(ISystem* pISystem);

#if !defined(_RELEASE) && CRY_PLATFORM_WINDOWS
	#define ENABLE_ENTITYEVENT_DEBUGGING 1
#else
	#define ENABLE_ENTITYEVENT_DEBUGGING 0
#endif

// entity event listener debug output macro
#if ENABLE_ENTITYEVENT_DEBUGGING
	#define ENTITY_EVENT_LISTENER_DEBUG                                      \
	  {                                                                      \
	    if (gEnv && gEnv->pConsole)                                          \
	    {                                                                    \
	      ICVar* pCvar = gEnv->pConsole->GetCVar("es_debugEntityListeners"); \
	      if (pCvar && pCvar->GetIVal())                                     \
	        CryLog("Entity Event listener %s '%p'", __FUNCTION__, this);     \
	    }                                                                    \
	  }

	#define ENTITY_EVENT_ENTITY_DEBUG(pEntity)                                          \
	  {                                                                                 \
	    if (gEnv && gEnv->pConsole)                                                     \
	    {                                                                               \
	      ICVar* pCvar = gEnv->pConsole->GetCVar("es_debugEntityListeners");            \
	      if (pCvar && pCvar->GetIVal())                                                \
	        CryLog("Event for entity '%s' pointer '%p'", pEntity->GetName(), pEntity);  \
	    }                                                                               \
	  }

	#define ENTITY_EVENT_ENTITY_LISTENER(pListener)                          \
	  {                                                                      \
	    if (gEnv && gEnv->pConsole)                                          \
	    {                                                                    \
	      ICVar* pCvar = gEnv->pConsole->GetCVar("es_debugEntityListeners"); \
	      if (pCvar && pCvar->GetIVal())                                     \
	        CryLog("Event listener '%p'", pListener);                        \
	    }                                                                    \
	  }

	#define ENTITY_EVENT_LISTENER_ADDED(pEntity, pListener)                                                                                 \
	  {                                                                                                                                     \
	    if (gEnv && gEnv->pConsole)                                                                                                         \
	    {                                                                                                                                   \
	      ICVar* pCvar = gEnv->pConsole->GetCVar("es_debugEntityListeners");                                                                \
	      if (pCvar && pCvar->GetIVal())                                                                                                    \
	        CryLog("Adding Entity Event listener for entity '%s' pointer '%p' for listener '%p'", pEntity->GetName(), pEntity, pListener);  \
	    }                                                                                                                                   \
	  }

	#define ENTITY_EVENT_LISTENER_REMOVED(nEntity, pListener)                                                                               \
	  {                                                                                                                                     \
	    if (gEnv && gEnv->pConsole)                                                                                                         \
	    {                                                                                                                                   \
	      ICVar* pCvar = gEnv->pConsole->GetCVar("es_debugEntityListeners");                                                                \
	      if (pCvar && pCvar->GetIVal())                                                                                                    \
	      {                                                                                                                                 \
	        CEntity* pCEntity = (CEntity*)GetEntity(nEntity);                                                                               \
	        CryLog("Removing Entity Event listener for entity '%s' for listener '%p'", pCEntity ? pCEntity->GetName() : "null", pListener); \
	      }                                                                                                                                 \
	    }                                                                                                                                   \
	  }

#else
	#define ENTITY_EVENT_LISTENER_DEBUG
	#define ENTITY_EVENT_ENTITY_DEBUG(pEntity)
	#define ENTITY_EVENT_ENTITY_LISTENER(pListener)
	#define ENTITY_EVENT_LISTENER_ADDED(pEntity, pListener)
	#define ENTITY_EVENT_LISTENER_REMOVED(nEntity, pListener)
#endif

template<class T>
inline IEntityClass* RegisterEntityClassWithDefaultComponent(
  const char* name,
  const CryGUID classGUID,           // This is a guid for the Entity Class, not for component
  const CryGUID componentUniqueGUID, // This is not a class type of component guid, but a unique guid of this unique component inside entity
  bool bIconOnTop = false,
  IFlowNodeFactory* pOptionalFlowNodeFactory = nullptr
  )
{
	const CEntityComponentClassDesc* pClassDesc = &Schematyc::GetTypeDesc<T>();

	IEntityClassRegistry::SEntityClassDesc clsDesc;
	clsDesc.sName = name;

	clsDesc.editorClassInfo.sCategory = pClassDesc->GetEditorCategory();
	clsDesc.editorClassInfo.sIcon = pClassDesc->GetIcon();
	clsDesc.editorClassInfo.bIconOnTop = bIconOnTop;
	clsDesc.pIFlowNodeFactory = pOptionalFlowNodeFactory;

	auto onSpawnLambda = [componentUniqueGUID, pClassDesc](IEntity& entity, SEntitySpawnParams& params) -> bool
	{
		string componentName = pClassDesc->GetName().c_str();
		IEntityComponent::SInitParams initParams(
		  &entity,
		  componentUniqueGUID,
		  componentName,
		  pClassDesc,
		  EEntityComponentFlags::None,
		  nullptr,
		  nullptr);
		entity.CreateComponentByInterfaceID(pClassDesc->GetGUID(), &initParams);
		return true;
	};
	clsDesc.onSpawnCallback = onSpawnLambda;

	return gEnv->pEntitySystem->GetClassRegistry()->RegisterStdClass(clsDesc);
}
