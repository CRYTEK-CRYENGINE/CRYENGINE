// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "EntitySystem.h"
#include "EntityIt.h"

#include "Entity.h"
#include "EntityClassRegistry.h"
#include "ReflectionRegistry.h"
#include "ScriptBind_Entity.h"
#include "PhysicsEventListener.h"
#include "AreaManager.h"
#include "AreaProxy.h"
#include "BreakableManager.h"
#include "EntityArchetype.h"
#include "PartitionGrid.h"
#include "ProximityTriggerSystem.h"
#include "EntitySlot.h"
#include <CryNetwork/ISerialize.h>
#include "NetEntity.h"
#include "EntityLayer.h"
#include "EntityLoadManager.h"
#include <CrySystem/Profilers/IStatoscope.h>
#include <CryEntitySystem/IBreakableManager.h>
#include "GeomCacheAttachmentManager.h"
#include "CharacterBoneAttachmentManager.h"
#include <CryCore/TypeInfo_impl.h>  // CRY_ARRAY_COUNT
#include "BSPTree3D.h"

#include <CryCore/StlUtils.h>
#include <CryRenderer/IRenderer.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CrySystem/ILog.h>
#include <CrySystem/ITimer.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/ConsoleRegistration.h>
#include <CryPhysics/IPhysics.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryLiveCreate/ILiveCreateHost.h>
#include <CryMath/Cry_Camera.h>
#include <CrySystem/File/IResourceManager.h>
#include <CryPhysics/IDeferredCollisionEvent.h>
#include <CryNetwork/IRemoteCommand.h>
#include <CryGame/IGameFramework.h>

#include "EntityComponentsCache.h"

#include "Schematyc/EntityObjectDebugger.h"

stl::PoolAllocatorNoMT<sizeof(CEntitySlot), 16>* g_Alloc_EntitySlot = 0;

namespace
{
static inline bool LayerActivationPriority(
  const SPostSerializeLayerActivation& a
  , const SPostSerializeLayerActivation& b)
{
	if (a.enable && !b.enable)
		return true;
	if (!a.enable && b.enable)
		return false;
	return false;
}
}

//////////////////////////////////////////////////////////////////////////
void OnRemoveEntityCVarChange(ICVar* pArgs)
{
	if (g_pIEntitySystem != nullptr)
	{
		const char* szEntity = pArgs->GetString();
		if (CEntity* pEnt = static_cast<CEntity*>(g_pIEntitySystem->FindEntityByName(szEntity)))
		{
			g_pIEntitySystem->RemoveEntity(pEnt);
		}
	}
}

//////////////////////////////////////////////////////////////////////
SEntityLoadParams::SEntityLoadParams()
	: allocationSize(sizeof(CEntity))
{
	AddRef();
}

//////////////////////////////////////////////////////////////////////
SEntityLoadParams::~SEntityLoadParams()
{
	RemoveRef();
}

//////////////////////////////////////////////////////////////////////
bool SEntityLoadParams::CloneXmlNode(const XmlNodeRef sourceNode, XmlNodeRef destNode)
{
	assert(sourceNode);
	assert(destNode);

	if (destNode)
	{
		bool bResult = true;

		destNode->setContent(sourceNode->getContent());

		const int attrCount = sourceNode->getNumAttributes();
		for (int attr = 0; attr < attrCount; ++attr)
		{
			const char* key = 0;
			const char* value = 0;
			if (sourceNode->getAttributeByIndex(attr, &key, &value))
			{
				destNode->setAttr(key, value);
			}
		}

		const int childCount = sourceNode->getChildCount();
		for (int child = 0; child < childCount; ++child)
		{
			XmlNodeRef pSourceChild = sourceNode->getChild(child);
			if (pSourceChild)
			{
				XmlNodeRef pChildClone = destNode->newChild(pSourceChild->getTag());
				bResult &= CloneXmlNode(pSourceChild, pChildClone);
			}
		}

		return bResult;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////
void SEntityLoadParams::AddRef()
{
	if (spawnParams.pPropertiesTable)
		spawnParams.pPropertiesTable->AddRef();

	if (spawnParams.pPropertiesInstanceTable)
		spawnParams.pPropertiesInstanceTable->AddRef();
}

//////////////////////////////////////////////////////////////////////
void SEntityLoadParams::RemoveRef()
{
	SAFE_RELEASE(spawnParams.pPropertiesTable);
	SAFE_RELEASE(spawnParams.pPropertiesInstanceTable);
}

//////////////////////////////////////////////////////////////////////
CEntitySystem::CEntitySystem(ISystem* pSystem)
	: m_pPartitionGrid(nullptr)
	, m_pProximityTriggerSystem(nullptr)
{
	// Assign allocators.
	g_Alloc_EntitySlot = new stl::PoolAllocatorNoMT<sizeof(CEntitySlot), 16>(stl::FHeap().FreeWhenEmpty(true));

	for (std::vector<IEntitySystemSink*>& sinkListeners : m_sinks)
	{
		sinkListeners.reserve(8);
	}

	m_pISystem = pSystem;
	m_pClassRegistry = 0;
	m_pEntityScriptBinding = NULL;

	CVar::Init();

	m_bTimersPause = false;
	m_nStartPause.SetSeconds(-1.0f);

	m_pAreaManager = new CAreaManager();
	m_pBreakableManager = new CBreakableManager();
	m_pEntityArchetypeManager = new CEntityArchetypeManager;

	m_pEntityLoadManager = new CEntityLoadManager();

	if (CVar::es_UseProximityTriggerSystem)
	{
		m_pPartitionGrid = new CPartitionGrid;
		m_pProximityTriggerSystem = new CProximityTriggerSystem;
	}

#if defined(USE_GEOM_CACHES)
	m_pGeomCacheAttachmentManager = new CGeomCacheAttachmentManager;
#endif
	m_pCharacterBoneAttachmentManager = new CCharacterBoneAttachmentManager;

	m_idForced = 0;

	if (gEnv->pConsole != 0)
	{
		REGISTER_STRING_CB("es_removeEntity", "", VF_CHEAT, "Removes an entity", OnRemoveEntityCVarChange);
	}

	m_pEntityObjectDebugger.reset(new CEntityObjectDebugger);

#ifndef RELEASE
	if (gEnv->IsEditor())
	{
		m_entityComponentsCache.reset(new CEntityComponentsCache);
	}
#endif
}

//////////////////////////////////////////////////////////////////////
CEntitySystem::~CEntitySystem()
{
	Unload();

	SAFE_DELETE(m_pClassRegistry);
	SAFE_DELETE(m_pReflectionRegistry);

	SAFE_DELETE(m_pCharacterBoneAttachmentManager);
#if defined(USE_GEOM_CACHES)
	SAFE_DELETE(m_pGeomCacheAttachmentManager);
#endif
	SAFE_DELETE(m_pAreaManager);
	SAFE_DELETE(m_pEntityArchetypeManager);
	SAFE_DELETE(m_pEntityScriptBinding);
	SAFE_DELETE(m_pEntityLoadManager);

	SAFE_DELETE(m_pPhysicsEventListener);

	SAFE_DELETE(m_pProximityTriggerSystem);
	SAFE_DELETE(m_pPartitionGrid);
	SAFE_DELETE(m_pBreakableManager);

	SAFE_DELETE(g_Alloc_EntitySlot);
	//ShutDown();
}

//////////////////////////////////////////////////////////////////////
bool CEntitySystem::Init(ISystem* pSystem)
{
	MEMSTAT_FUNCTION_CONTEXT(EMemStatContextType::Other);
	if (!pSystem)
		return false;

	m_pISystem = pSystem;
	ClearEntityArray();

	m_pClassRegistry = new CEntityClassRegistry;
	m_pClassRegistry->InitializeDefaultClasses();

	m_pReflectionRegistry = new Cry::Entity::CReflectionRegistry();

	//////////////////////////////////////////////////////////////////////////
	// Initialize entity script bindings.
	m_pEntityScriptBinding = new CScriptBind_Entity(pSystem->GetIScriptSystem(), pSystem);

	// Initialize physics events handler.
	if (pSystem->GetIPhysicalWorld())
		m_pPhysicsEventListener = new CPhysicsEventListener(pSystem->GetIPhysicalWorld());

	if (CVar::es_UseProximityTriggerSystem)
	{
		//////////////////////////////////////////////////////////////////////////
		// Should reallocate grid if level size change.
		m_pPartitionGrid->AllocateGrid(4096, 4096);
	}

	m_bLocked = false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
IEntityClassRegistry* CEntitySystem::GetClassRegistry()
{
	return m_pClassRegistry;
}

//////////////////////////////////////////////////////////////////////////
Cry::Entity::IReflectionRegistry* CEntitySystem::GetReflectionRegistry() const
{
	return m_pReflectionRegistry;
}

//////////////////////////////////////////////////////////////////////
void CEntitySystem::Release()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::RegisterPhysicCallbacks()
{
	if (m_pPhysicsEventListener)
		m_pPhysicsEventListener->RegisterPhysicCallbacks();
#if !defined(_RELEASE)
	CEntityPhysics::EnableValidation();
#endif
}

void CEntitySystem::UnregisterPhysicCallbacks()
{
	if (m_pPhysicsEventListener)
		m_pPhysicsEventListener->UnregisterPhysicCallbacks();
#if !defined(_RELEASE)
	CEntityPhysics::DisableValidation();
#endif
}

//////////////////////////////////////////////////////////////////////
void CEntitySystem::Unload()
{
	MEMSTAT_FUNCTION_CONTEXT(EMemStatContextType::Other);
	if (gEnv->p3DEngine)
	{
		IDeferredPhysicsEventManager* pDeferredPhysicsEventManager = gEnv->p3DEngine->GetDeferredPhysicsEventManager();
		if (pDeferredPhysicsEventManager)
			pDeferredPhysicsEventManager->ClearDeferredEvents();
	}

	Reset();

	UnloadAreas();

	stl::free_container(m_currentTimers);
	m_pEntityArchetypeManager->Reset();

	stl::free_container(m_deferredUsedEntities);
}

void CEntitySystem::PurgeHeaps()
{
	for (THeaps::iterator it = m_garbageLayerHeaps.begin(); it != m_garbageLayerHeaps.end(); ++it)
	{
		if (it->pHeap)
			it->pHeap->Release();
	}

	stl::free_container(m_garbageLayerHeaps);
}

void CEntitySystem::Reset()
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	if (CVar::es_UseProximityTriggerSystem)
	{
		m_pPartitionGrid->BeginReset();
		m_pProximityTriggerSystem->BeginReset();
	}

	// Flush the physics linetest and events queue
	if (gEnv->pPhysicalWorld)
	{
		gEnv->pPhysicalWorld->TracePendingRays(0);
		gEnv->pPhysicalWorld->ClearLoggedEvents();
	}

	PurgeDeferredCollisionEvents(true);

	ClearLayers();
	m_mapEntityNames.clear();

	m_genIdMap.clear();

	m_updatedEntityComponents.Clear();
	m_prePhysicsUpdatedEntityComponents.Clear();

	// Delete entities that have already been added to the delete list.
	DeletePendingEntities();

	for(CEntity* pEntity : m_entityArray)
	{
		if (pEntity == nullptr)
			continue;

		if (!pEntity->IsGarbage())
		{
			pEntity->m_flags &= ~ENTITY_FLAG_UNREMOVABLE;
			pEntity->m_keepAliveCounter = 0;
			RemoveEntity(pEntity, false, true);
		}
		else
		{
			stl::push_back_unique(m_deletedEntities, pEntity);
		}
	}
	// Delete entity that where added to delete list.
	DeletePendingEntities();

	ClearEntityArray();

	stl::free_container(m_deletedEntities);
	m_guidMap.clear();

	// Delete broken objects after deleting entities
	GetBreakableManager()->ResetBrokenObjects();

	ResetAreas();

	m_entityArray.Reset();
	m_staticEntityIds.clear();

	// Always reserve the legacy game rules and local player entity id's
	ReserveEntityId(1);
	// Game rules is always considered as a static entity
	m_staticEntityIds.push_back(1);

	ReserveEntityId(LOCAL_PLAYER_ENTITY_ID);

	m_timersMap.clear();

	if (CVar::es_UseProximityTriggerSystem)
	{
		m_pProximityTriggerSystem->Reset();
		m_pPartitionGrid->Reset();
	}

	m_pEntityLoadManager->Reset();
}

//////////////////////////////////////////////////////////////////////
void CEntitySystem::AddSink(IEntitySystemSink* pSink, std::underlying_type<SinkEventSubscriptions>::type subscriptions)
{
	assert(pSink);

	if (pSink)
	{
		for (uint i = 0; i < m_sinks.size(); ++i)
		{
			if ((subscriptions & (1 << i)) && (i != ((uint)SinkEventSubscriptions::Last - 1)))
			{
				assert(!stl::find(m_sinks[i], pSink));
				m_sinks[i].push_back(pSink);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::RemoveSink(IEntitySystemSink* pSink)
{
	assert(pSink);

	for (std::vector<IEntitySystemSink*>& sinkListeners : m_sinks)
	{
		stl::find_and_erase(sinkListeners, pSink);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntitySystem::OnBeforeSpawn(SEntitySpawnParams& params)
{
	const std::vector<IEntitySystemSink*>& sinks = m_sinks[stl::static_log2 < (size_t)IEntitySystem::OnBeforeSpawn > ::value];

	for (IEntitySystemSink* pSink : sinks)
	{
		if (!pSink->OnBeforeSpawn(params))
			return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::OnEntityReused(IEntity* pEntity, SEntitySpawnParams& params)
{
	const std::vector<IEntitySystemSink*>& sinks = m_sinks[stl::static_log2 < (size_t)IEntitySystem::OnReused > ::value];

	for (IEntitySystemSink* pSink : sinks)
	{
		pSink->OnReused(pEntity, params);
	}
}

bool CEntitySystem::ValidateSpawnParameters(SEntitySpawnParams& params)
{
#ifdef INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE
	if (params.sName)
	{
		unsigned int countIllegalCharacters = 0;

		for (const char* checkEachChar = params.sName; *checkEachChar; ++checkEachChar)
		{
			const uint8 c = static_cast<uint8>(*checkEachChar);
			countIllegalCharacters += (c < ' ') || (c == 127);
		}

		if (countIllegalCharacters)
		{
			CryWarning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, "!Name '%s' contains %u illegal character(s) while creating an instance of '%s'", params.sName, countIllegalCharacters, params.pClass ? params.pClass->GetName() : "no class specified");
		}
	}
#endif

	// Check if the entity class is archetype if yes get it from the system
	if (params.pClass && !params.pArchetype && params.pClass->GetFlags() & ECLF_ENTITY_ARCHETYPE)
	{
		params.pArchetype = LoadEntityArchetype(params.pClass->GetName());
	}

	// If entity spawns from archetype take class from archetype
	if (params.pArchetype)
		params.pClass = params.pArchetype->GetClass();

	if (params.pClass == nullptr)
	{
		params.pClass = m_pClassRegistry->GetDefaultClass();
	}

	if (m_bLocked)
	{
		if (!params.bIgnoreLock)
		{
			CryWarning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, "Spawning entity %s of class %s refused due to spawn lock! Spawning entities is not allowed at this time.", params.sName, params.pClass->GetName());
			return false;
		}
	}

	return true;
}

IEntity* CEntitySystem::SpawnPreallocatedEntity(CEntity* pPrecreatedEntity, SEntitySpawnParams& params, bool bAutoInit)
{
	CRY_PROFILE_FUNCTION_ARG(PROFILE_ENTITY, (params.pClass ? params.pClass->GetName() : "Unknown"));

	if (!ValidateSpawnParameters(params))
	{
		params.spawnResult = EEntitySpawnResult::Error;
		return nullptr;
	}

	MEMSTAT_CONTEXT_FMT(EMemStatContextType::Other, "SpawnEntity %s", params.pClass->GetName());

	if (!OnBeforeSpawn(params))
	{
		params.spawnResult = EEntitySpawnResult::Skipped;
		return nullptr;
	}

	if (m_idForced)
	{
		params.id = m_idForced;
		m_idForced = 0;
	}

	CEntity* pEntity = nullptr;

	// Most common case, entity id was not provided
	if (params.id == INVALID_ENTITYID)
	{
		// get entity id and mark it
		params.id = GenerateEntityId();

		if (!params.id)
		{
			EntityWarning("CEntitySystem::SpawnEntity Failed, Can't spawn entity %s. ID range is full (internal error)", params.sName);
			params.spawnResult = EEntitySpawnResult::Error;
			return nullptr;
		}
	}
	else
	{
		if (m_entityArray.IsUsed(IdToHandle(params.id).GetIndex()))
		{
			// was reserved or was existing already

			pEntity = m_entityArray[IdToHandle(params.id)];

			if (pEntity)
				EntityWarning("Entity with id=%d, %s already spawned on this client...override", pEntity->GetId(), pEntity->GetEntityTextDescription().c_str());
			else
				m_entityArray.InsertKnownHandle(IdToHandle(params.id));
		}
		else
		{
			m_entityArray.InsertKnownHandle(IdToHandle(params.id));
		}
	}

	if (params.guid.IsNull())
	{
		params.guid = CryGUID::Create();
	}

	if (pEntity == nullptr || pPrecreatedEntity != nullptr)
	{
		if (pPrecreatedEntity != nullptr)
		{
			pEntity = pPrecreatedEntity;
			pEntity->PreInit(params);
			pEntity->SetInternalFlag(CEntity::EInternalFlag::PreallocatedComponents, true);
		}
		else if (pEntity == nullptr)
		{
			// Makes new entity.
			pEntity = new CEntity();
			pEntity->PreInit(params);
		}

		// put it into the entity map
		m_entityArray[IdToHandle(params.id)] = pEntity;

		RegisterEntityGuid(params.guid, params.id);

		if (bAutoInit)
		{
			if (!InitEntity(pEntity, params))   // calls DeleteEntity() on failure
			{
				params.spawnResult = EEntitySpawnResult::Error;
				return nullptr;
			}
		}
	}

	if (CVar::es_debugEntityLifetime)
	{
		CryLog("CEntitySystem::SpawnEntity %s %s 0x%x", pEntity ? pEntity->GetClass()->GetName() : "null", pEntity ? pEntity->GetName() : "null", pEntity ? pEntity->GetId() : 0);
	}

	params.spawnResult = EEntitySpawnResult::Success;
	return pEntity;
}

IEntity* CEntitySystem::SpawnEntity(SEntitySpawnParams& params, bool bAutoInit)
{
	return SpawnPreallocatedEntity(nullptr, params, bAutoInit);
}

//////////////////////////////////////////////////////////////////////////
bool CEntitySystem::InitEntity(IEntity* pEntity, SEntitySpawnParams& params)
{
	INDENT_LOG_DURING_SCOPE(true, "Initializing entity '%s' of class '%s' (layer='%s')", params.sName, params.pClass->GetName(), params.sLayerName);

	assert(pEntity);

	CRY_DEFINE_ASSET_SCOPE("Entity", params.sName);

	CEntity* pCEntity = static_cast<CEntity*>(pEntity);

	// initialize entity
	if (!pCEntity->Init(params))
	{
		// The entity may have already be scheduled for deletion
		if (!pCEntity->IsGarbage() || std::find(m_deletedEntities.begin(), m_deletedEntities.end(), pCEntity) == m_deletedEntities.end())
		{
			DeleteEntity(pCEntity);
		}

		return false;
	}

#ifndef RELEASE
	if (gEnv->IsEditor() && gEnv->IsGameOrSimulation()  &&!(pCEntity->GetFlags() & ENTITY_FLAG_UNREMOVABLE))
	{
		m_entityComponentsCache->OnEntitySpawnedDuringGameMode(pEntity->GetId());
	}
#endif

	const std::vector<IEntitySystemSink*>& sinks = m_sinks[stl::static_log2 < (size_t)IEntitySystem::OnSpawn > ::value];

	for (IEntitySystemSink* pSink : sinks)
	{
		pSink->OnSpawn(pEntity, params);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::DeleteEntity(CEntity* pEntity)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	if (CVar::es_debugEntityLifetime)
	{
		CryLog("CEntitySystem::DeleteEntity %s %s 0x%x",
		       pEntity ? pEntity->GetClass()->GetName() : "null",
		       pEntity ? pEntity->GetName() : "null",
		       pEntity ? pEntity->GetId() : 0);
	}

	if (pEntity)
	{
		// Make sure 3dengine does not keep references to this entity
		gEnv->p3DEngine->OnEntityDeleted(pEntity);

		if (gEnv->pGameFramework)
		{
			INetContext* pContext = gEnv->pGameFramework->GetNetContext();

			if (pContext)
			{
				if (pContext->IsBound(pEntity->GetId()))
				{
					// If the network is still there and this entity is still bound to it force an unbind.
					pContext->UnbindObject(pEntity->GetId());
				}
			}
		}

		pEntity->ShutDown();

		m_entityArray[IdToHandle(pEntity->GetId())] = 0;
		m_entityArray.Remove(IdToHandle(pEntity->GetId()));

		if (!pEntity->m_guid.IsNull())
			UnregisterEntityGuid(pEntity->m_guid);

		if (!pEntity->HasInternalFlag(CEntity::EInternalFlag::PreallocatedComponents))
		{
			delete pEntity;
		}
		else
		{
			pEntity->~CEntity();
		}
	}
}

//////////////////////////////////////////////////////////////////////
void CEntitySystem::ClearEntityArray()
{
#if defined(USE_CRY_ASSERT)
	for(const CEntity* const pEntity : m_entityArray)
	{
		CRY_ASSERT_TRACE(pEntity == nullptr, ("About to \"leak\" entity id %d (%s)", pEntity->GetId(), pEntity->GetName()));
	}
#endif

	m_entityArray.fill_nullptr();
	m_bSupportLegacy64bitGuids = false;
}

//////////////////////////////////////////////////////////////////////
void CEntitySystem::RemoveEntity(EntityId entity, bool forceRemoveImmediately)
{
	assert((bool)IdToHandle(entity));

	if (CEntity* pEntity = GetEntityFromID(entity))
	{
		if (pEntity->GetId() != entity)
		{
			CRY_ASSERT(false);

			EntityWarning("Trying to remove entity with mismatching salts. id1=%d id2=%d", entity, pEntity->GetId());

			if (ICVar* pVar = gEnv->pConsole->GetCVar("net_assertlogging"))
			{
				if (pVar->GetIVal())
				{
					gEnv->pSystem->debug_LogCallStack();
				}
			}
			return;
		}

		RemoveEntity(pEntity, forceRemoveImmediately);
	}
}

void CEntitySystem::RemoveEntity(CEntity* pEntity, bool forceRemoveImmediately, bool ignoreSinks)
{
	ENTITY_PROFILER

	if (CVar::es_debugEntityLifetime)
	{
		CryLog("CEntitySystem::RemoveEntity %s %s 0x%x", pEntity ? pEntity->GetClass()->GetName() : "null", pEntity ? pEntity->GetName() : "null", pEntity ? pEntity->GetId() : 0);
	}

	if (pEntity)
	{
		if (m_bLocked)
		{
			CryWarning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, "Removing entity during system lock : %s with id %i", pEntity->GetName(), pEntity->GetId());
		}

		if (pEntity->IsGarbage())
		{
			// Entity was already queued for deletion
			// Check if we request immediate removal, and if so delete immediately if it was in the pending deletion queue
			if (forceRemoveImmediately && stl::find_and_erase(m_deletedEntities, pEntity))
			{
				DeleteEntity(pEntity);
			}
		}
		else
		{
			const std::vector<IEntitySystemSink*>& sinks = m_sinks[stl::static_log2 < (size_t)IEntitySystem::OnRemove > ::value];

			for (IEntitySystemSink* pSink : sinks)
			{
				if (!pSink->OnRemove(pEntity) && !ignoreSinks)
				{
					// basically unremovable... but hide it anyway to be polite
					pEntity->Hide(true);
					return;
				}
			}

#ifndef RELEASE
			if (gEnv->IsEditor() && gEnv->IsGameOrSimulation() && !(pEntity->GetFlags() & ENTITY_FLAG_UNREMOVABLE))
			{
				m_entityComponentsCache->OnEntityRemovedDuringGameMode(pEntity->GetId());
			}
#endif

			// Mark the entity for deletion before sending the ENTITY_EVENT_DONE event
			// This protects against cases where the event results in another deletion request for the same entity
			pEntity->SetInternalFlag(CEntity::EInternalFlag::MarkedForDeletion, true);

			pEntity->GetPhysicalProxy()->PrepareForDeletion();

			// Send deactivate event.
			SEntityEvent entevent;
			entevent.event = ENTITY_EVENT_DONE;
			entevent.nParam[0] = pEntity->GetId();
			pEntity->SendEventInternal(entevent);

			pEntity->PrepareForDeletion();

			if (!(pEntity->m_flags & ENTITY_FLAG_UNREMOVABLE) && pEntity->m_keepAliveCounter == 0)
			{
				if (forceRemoveImmediately)
				{
					DeleteEntity(pEntity);
				}
				else
				{
					// add entity to deleted list, and actually delete entity on next update.
					stl::push_back_unique(m_deletedEntities, pEntity);
				}
			}
			else
			{
				// Unremovable entities. are hidden and deactivated.
				pEntity->Hide(true);

				// remember kept alive entities to get rid of them as soon as they are no longer needed
				if (pEntity->m_keepAliveCounter > 0 && !(pEntity->m_flags & ENTITY_FLAG_UNREMOVABLE))
				{
					m_deferredUsedEntities.push_back(pEntity);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////
void CEntitySystem::ResurrectGarbageEntity(CEntity* pEntity)
{
	CRY_ASSERT(pEntity->HasInternalFlag(CEntity::EInternalFlag::MarkedForDeletion));
	pEntity->SetInternalFlag(CEntity::EInternalFlag::MarkedForDeletion, false);

	// Entity may have been queued for deletion
	stl::find_and_erase(m_deletedEntities, pEntity);
}

//////////////////////////////////////////////////////////////////////
IEntity* CEntitySystem::GetEntity(EntityId id) const
{
	return GetEntityFromID(id);
}

CEntity* CEntitySystem::GetEntityFromID(EntityId id) const
{
	const EntityId maxUsedEntityIndex = m_entityArray.GetMaxUsedEntityIndex();
	EntityId index = static_cast<EntityId>(IdToIndex(id));

	EntityId hd1cond = index <= maxUsedEntityIndex;
	using SignedIndexType = std::make_signed<EntityId>::type;
	hd1cond = static_cast<EntityId>(static_cast<SignedIndexType>(hd1cond) | -static_cast<SignedIndexType>(hd1cond));  //0 for hdl>dwMaxUsed, 0xFFFFFFFF for hdl<=dwMaxUsed
	index = hd1cond & index;

	if (CEntity* const pEntity = m_entityArray[index])
	{
		// An entity at a specific index is considered identical if the salt (use count) matches
		if (IdToHandle(pEntity->GetId()).GetSalt() == IdToHandle(id).GetSalt())
		{
			return pEntity;
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
IEntity* CEntitySystem::FindEntityByName(const char* sEntityName) const
{
	if (!sEntityName || !sEntityName[0])
		return nullptr; // no entity name specified

	if (CVar::es_DebugFindEntity != 0)
	{
		CryLog("FindEntityByName: %s", sEntityName);
		if (CVar::es_DebugFindEntity == 2)
			GetISystem()->debug_LogCallStack();
	}

	// Find first object with this name.
	EntityNamesMap::const_iterator it = m_mapEntityNames.lower_bound(sEntityName);
	if (it != m_mapEntityNames.end())
	{
		if (stricmp(it->first, sEntityName) == 0)
			return GetEntityFromID(it->second);
	}

	return nullptr;   // name not found
}

//////////////////////////////////////////////////////////////////////
uint32 CEntitySystem::GetNumEntities() const
{
	uint32 dwRet = 0;

	for(const CEntity* const pEntity : m_entityArray)
	{
		if (pEntity == nullptr)
			continue;

		++dwRet;
	}

	return dwRet;
}

//////////////////////////////////////////////////////////////////////////
IEntity* CEntitySystem::GetEntityFromPhysics(IPhysicalEntity* pPhysEntity) const
{
	CRY_ASSERT(pPhysEntity);
	if (pPhysEntity != nullptr)
	{
		return reinterpret_cast<CEntity*>(pPhysEntity->GetForeignData(PHYS_FOREIGN_ID_ENTITY));
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////
int CEntitySystem::GetPhysicalEntitiesInBox(const Vec3& origin, float radius, IPhysicalEntity**& pList, int physFlags) const
{
	assert(m_pISystem);

	Vec3 bmin = origin - Vec3(radius, radius, radius);
	Vec3 bmax = origin + Vec3(radius, radius, radius);

	return m_pISystem->GetIPhysicalWorld()->GetEntitiesInBox(bmin, bmax, pList, physFlags);
}

//////////////////////////////////////////////////////////////////////////
int CEntitySystem::QueryProximity(SEntityProximityQuery& query)
{
	if (CVar::es_UseProximityTriggerSystem)
	{
		SPartitionGridQuery q;
		q.aabb = query.box;
		q.nEntityFlags = query.nEntityFlags;
		q.pEntityClass = query.pEntityClass;
		m_pPartitionGrid->GetEntitiesInBox(q);
		query.pEntities = 0;
		query.nCount = (int)q.pEntities->size();
		if (q.pEntities && query.nCount > 0)
		{
			query.pEntities = q.pEntities->data();
		}
	}
	return query.nCount;
}

//////////////////////////////////////////////////////////////////////
void CEntitySystem::PrePhysicsUpdate()
{
	CRY_PROFILE_SECTION(PROFILE_ENTITY, "EntitySystem::PrePhysicsUpdate");
	MEMSTAT_FUNCTION_CONTEXT(EMemStatContextType::Other);

	SEntityEvent event(ENTITY_EVENT_PREPHYSICSUPDATE);
	event.fParam[0] = gEnv->pTimer->GetFrameTime();

#ifdef ENABLE_PROFILING_CODE
	const uint8 eventIndex = CEntity::GetEntityEventIndex(event.event);

	if (CVar::es_profileComponentUpdates != 0)
	{
		const CTimeValue timeBeforePrePhysicsUpdate = gEnv->pTimer->GetAsyncTime();

		m_prePhysicsUpdatedEntityComponents.ForEach([&](const SMinimalEntityComponentRecord& rec) -> EComponentIterationResult
		{
			const CTimeValue timeBeforeComponentUpdate = gEnv->pTimer->GetAsyncTime();

			// Cache entity info before sending the event, as the entity may be removed by the event
			const SProfiledEntityEvent::SEntityInfo componentEntityInfo(*rec.pComponent->GetEntity());

			rec.pComponent->ProcessEvent(event);

			const CTimeValue timeAfterComponentUpdate = gEnv->pTimer->GetAsyncTime();
			const float componentUpdateTime = (timeAfterComponentUpdate - timeBeforeComponentUpdate).GetMilliSeconds();
			if (componentUpdateTime > m_profiledEvents[eventIndex].mostExpensiveEntityCostMs)
			{
				m_profiledEvents[eventIndex].mostExpensiveEntityCostMs = componentUpdateTime;
				m_profiledEvents[eventIndex].mostExpensiveEntity = componentEntityInfo;
			}

			return EComponentIterationResult::Continue;
		});

		const CTimeValue timeAfterPrePhysicsUpdate = gEnv->pTimer->GetAsyncTime();

		const float componentsUpdateTime = (timeAfterPrePhysicsUpdate - timeBeforePrePhysicsUpdate).GetMilliSeconds();
		m_profiledEvents[eventIndex].numEvents = m_prePhysicsUpdatedEntityComponents.Size();
		m_profiledEvents[eventIndex].totalCostMs = componentsUpdateTime;
	}
	else
#endif
	m_prePhysicsUpdatedEntityComponents.ForEach([&](const SMinimalEntityComponentRecord& rec) -> EComponentIterationResult
	{
		rec.pComponent->ProcessEvent(event);

		return EComponentIterationResult::Continue;
	});
}

//update the entity system
//////////////////////////////////////////////////////////////////////
void CEntitySystem::Update()
{
	CRY_PROFILE_SECTION(PROFILE_ENTITY, "EntitySystem::Update");
	MEMSTAT_FUNCTION_CONTEXT(EMemStatContextType::Other);

	const float fFrameTime = gEnv->pTimer->GetFrameTime();
	if (fFrameTime > FLT_EPSILON)
	{
		UpdateTimers();

#if defined(USE_GEOM_CACHES)
		m_pGeomCacheAttachmentManager->Update();
#endif
		m_pCharacterBoneAttachmentManager->Update();

		UpdateEntityComponents(fFrameTime);

		if (CVar::es_UseProximityTriggerSystem)
		{
			// Update info on proximity triggers.
			m_pProximityTriggerSystem->Update();
		}

		// Now update area manager to send enter/leave events from areas.
		m_pAreaManager->Update();

		// Delete entities that must be deleted.
		if (!m_deletedEntities.empty())
		{
			DeletePendingEntities();
		}

		{
			CRY_PROFILE_SECTION(PROFILE_ENTITY, "Delete garbage layer heaps");
			for (THeaps::iterator it = m_garbageLayerHeaps.begin(); it != m_garbageLayerHeaps.end(); )
			{
				SEntityLayerGarbage& lg = *it;
				if (lg.pHeap->Cleanup())
				{
					lg.pHeap->Release();
					it = m_garbageLayerHeaps.erase(it);
				}
				else
				{
					++lg.nAge;
					if (lg.nAge == 32)
					{
						CryWarning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, "Layer '%s', heap %p, has gone for %i frames without releasing its heap. Possible leak?", lg.layerName.c_str(), lg.pHeap, lg.nAge);
					}

					++it;
				}
			}
		}

		PurgeDeferredCollisionEvents();

		// Doesn't depend on anything, previously called in CGameObject::PostUpdate()
		CNetEntity::UpdateSchedulingProfiles();
	}

#ifdef INCLUDE_DEBUG_ENTITY_DRAWING
	DebugDraw();
#endif // ~INCLUDE_DEBUG_ENTITY_DRAWING

	if (NULL != gEnv->pLiveCreateHost)
	{
		gEnv->pLiveCreateHost->DrawOverlay();
	}

	m_pEntityObjectDebugger->Update();
}

//////////////////////////////////////////////////////////////////////////

#ifdef ENABLE_PROFILING_CODE

namespace
{
void DrawText(const float x, const float y, ColorF c, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	IRenderAuxText::DrawText(Vec3(x, y, 1.0f), 1.2f, c, eDrawText_FixedSize | eDrawText_2D | eDrawText_Monospace, format, args);
	va_end(args);
}
}

#endif

#ifdef ENABLE_PROFILING_CODE
#define DEF_ENTITY_EVENT_NAME(x) #x

std::array<const char*, static_cast<size_t>(Cry::Entity::EEvent::Count)> s_eventNames =
{ {
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_XFORM),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_XFORM_FINISHED_EDITOR),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_INIT),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_DONE),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_RESET),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ATTACH),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ATTACH_THIS),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_DETACH),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_DETACH_THIS),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_LINK),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_DELINK),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_HIDE),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_UNHIDE),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_LAYER_HIDE),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_LAYER_UNHIDE),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ENABLE_PHYSICS),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_PHYSICS_CHANGE_STATE),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_SCRIPT_EVENT),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ENTERAREA),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_LEAVEAREA),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ENTERNEARAREA),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_LEAVENEARAREA),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_MOVEINSIDEAREA),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_MOVENEARAREA),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_PHYS_POSTSTEP),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_PHYS_BREAK),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_COLLISION),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_COLLISION_IMMEDIATE),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_RENDER_VISIBILITY_CHANGE),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_LEVEL_LOADED),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_START_LEVEL),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_START_GAME),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ENTER_SCRIPT_STATE),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_LEAVE_SCRIPT_STATE),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_PRE_SERIALIZE),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_POST_SERIALIZE),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_INVISIBLE),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_VISIBLE),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_MATERIAL),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ANIM_EVENT),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_SCRIPT_REQUEST_COLLIDERMODE),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ACTIVATE_FLOW_NODE_OUTPUT),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_RELOAD_SCRIPT),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ACTIVATED),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_DEACTIVATED),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_SET_AUTHORITY),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_NET_BECOME_LOCAL_PLAYER),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_SET_NAME),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_AUDIO_TRIGGER_STARTED),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_AUDIO_TRIGGER_ENDED),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_SLOT_CHANGED),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_PHYSICAL_TYPE_CHANGED),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_SPAWNED_REMOTELY),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_PREPHYSICSUPDATE),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_UPDATE),
		DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_TIMER)
	} };
#endif

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::UpdateEntityComponents(float fFrameTime)
{
	CRY_PROFILE_SECTION(PROFILE_ENTITY, "EntitySystem::UpdateEntityComponents");

	SEntityUpdateContext ctx = { fFrameTime, gEnv->pTimer->GetCurrTime(), gEnv->nMainFrameID };

	SEntityEvent event;
	event.event = ENTITY_EVENT_UPDATE;
	event.nParam[0] = (INT_PTR)&ctx;
	event.fParam[0] = ctx.fFrameTime;

#ifdef INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE
	if (CVar::pUpdateEntities->GetIVal() == 0)
	{
		return;
	}

	enum class EComponentProfilingType
	{
		Disabled,
		// Displays information of component costs, but no details on what is slow
		Simple,
		// Breaks down cost of component update based on component type
		TypeCostBreakdown,
		// Breaks down cost of component events
		EventCostBreakdown,
	};

	const float positionX = 50;
	float positionY = 20;
	const float yOffset = 10.6f;
	const float fontSize = 1.f;
	const float textColor[] = { 1, 1, 1, 1 };

	switch (CVar::es_profileComponentUpdates)
	{
	case (int)EComponentProfilingType::Disabled:
		{
#endif
			m_updatedEntityComponents.ForEach([&](const SMinimalEntityComponentRecord& rec) -> EComponentIterationResult
			{
				rec.pComponent->ProcessEvent(event);
				return EComponentIterationResult::Continue;
			});

#ifdef INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE
		}
		break;
	case(int)EComponentProfilingType::Simple:
		{
		const CTimeValue timeBeforeComponentUpdate = gEnv->pTimer->GetAsyncTime();

			m_updatedEntityComponents.ForEach([&](const SMinimalEntityComponentRecord& rec) -> EComponentIterationResult
			{
				rec.pComponent->ProcessEvent(event);
				return EComponentIterationResult::Continue;
			});

			const CTimeValue timeAfterComponentUpdate = gEnv->pTimer->GetAsyncTime();

			std::set<CEntity*> updatedEntities;

			EntityId renderedEntityCount = 0;
			EntityId physicalizedEntityCount = 0;

			m_updatedEntityComponents.ForEach([&](const SMinimalEntityComponentRecord& rec) -> EComponentIterationResult
			{
				CEntity* pEntity = static_cast<CEntity*>(rec.pComponent->GetEntity());

				if (updatedEntities.find(pEntity) == updatedEntities.end())
				{
					updatedEntities.insert(pEntity);

					if (pEntity->GetEntityRender()->GetSlotCount() > 0)
					{
						renderedEntityCount++;
					}
					if (pEntity->GetPhysicalEntity() != nullptr)
					{
						physicalizedEntityCount++;
					}
				}

				return EComponentIterationResult::Continue;
			});

			IRenderAuxGeom* pRenderAuxGeom = gEnv->pAuxGeomRenderer;

			pRenderAuxGeom->Draw2dLabel(positionX, positionY, fontSize, textColor, false, "Number of Entities: %i", GetNumEntities());
			positionY += yOffset;
			pRenderAuxGeom->Draw2dLabel(positionX, positionY, fontSize, textColor, false, "Number of Updated Entities: %" PRISIZE_T, updatedEntities.size());
			positionY += yOffset;
			pRenderAuxGeom->Draw2dLabel(positionX, positionY, fontSize, textColor, false, "Number of Rendered Entities: %i", renderedEntityCount);
			positionY += yOffset;
			pRenderAuxGeom->Draw2dLabel(positionX, positionY, fontSize, textColor, false, "Number of Physicalized Entities: %i", renderedEntityCount);
			positionY += yOffset;
			pRenderAuxGeom->Draw2dLabel(positionX, positionY, fontSize, textColor, false, "Number of Updated Entity Components: %" PRISIZE_T, m_updatedEntityComponents.Size());
			positionY += yOffset;
			pRenderAuxGeom->Draw2dLabel(positionX, positionY, fontSize, textColor, false, "Number of Pre-physics Updated Entity Components: %" PRISIZE_T, m_prePhysicsUpdatedEntityComponents.Size());
			positionY += yOffset * 2;

			const float componentUpdateTime = (timeAfterComponentUpdate - timeBeforeComponentUpdate).GetMilliSeconds();
			pRenderAuxGeom->Draw2dLabel(positionX, positionY, fontSize, textColor, false, "Entity Components: %f ms", componentUpdateTime);
		}
		break;
	case(int)EComponentProfilingType::TypeCostBreakdown:
		{
			struct SComponentTypeInfo
			{
				const char* szName;
				float       totalCostMs;
			};

			std::unordered_map<CryGUID, SComponentTypeInfo> componentTypeCostMap;

			const CTimeValue timeBeforeComponentsUpdate = gEnv->pTimer->GetAsyncTime();

			m_updatedEntityComponents.ForEach([&](const SMinimalEntityComponentRecord& rec) -> EComponentIterationResult
			{
				const CTimeValue timeBeforeComponentUpdate = gEnv->pTimer->GetAsyncTime();
				rec.pComponent->ProcessEvent(event);
				const CTimeValue timeAfterComponentUpdate = gEnv->pTimer->GetAsyncTime();

				CryGUID typeGUID;
				const char* szName;

				// The component may have been removed during its own update
				if (rec.pComponent != nullptr)
				{
					typeGUID = rec.pComponent->GetClassDesc().GetGUID();
					szName = rec.pComponent->GetClassDesc().GetLabel();

					if (szName == nullptr || szName[0] == '\0')
					{
						szName = rec.pComponent->GetClassDesc().GetName().c_str();
					}

					if (typeGUID.IsNull() && rec.pComponent->GetFactory() != nullptr)
					{
						typeGUID = rec.pComponent->GetFactory()->GetClassID();
						szName = rec.pComponent->GetFactory()->GetName();
					}
				}
				else
				{
					szName = nullptr;
				}

				if (szName == nullptr || szName[0] == '\0')
				{
					szName = "Unknown Entity Component";
				}

				auto it = componentTypeCostMap.find(typeGUID);

				if (it == componentTypeCostMap.end())
				{
					componentTypeCostMap.emplace(typeGUID, SComponentTypeInfo{ szName, (timeAfterComponentUpdate - timeBeforeComponentUpdate).GetMilliSeconds() });
				}
				else
				{
					it->second.totalCostMs += (timeAfterComponentUpdate - timeBeforeComponentUpdate).GetMilliSeconds();
				}

				return EComponentIterationResult::Continue;
			});

			const CTimeValue timeAfterComponentsUpdate = gEnv->pTimer->GetAsyncTime();

			IRenderAuxGeom* pRenderAuxGeom = gEnv->pAuxGeomRenderer;

			pRenderAuxGeom->Draw2dLabel(positionX, positionY, fontSize, textColor, false, "Number of Entities: %i", GetNumEntities());
			positionY += yOffset;
			pRenderAuxGeom->Draw2dLabel(positionX, positionY, fontSize, textColor, false, "Number of Updated Entity Components: %" PRISIZE_T, m_updatedEntityComponents.Size());
			positionY += yOffset * 2;

			const float componentUpdateTime = (timeAfterComponentsUpdate - timeBeforeComponentsUpdate).GetMilliSeconds();
			pRenderAuxGeom->Draw2dLabel(positionX, positionY, fontSize, textColor, false, "Entity Components: %f ms", componentUpdateTime);
			positionY += yOffset;
			pRenderAuxGeom->Draw2dLabel(positionX, positionY, fontSize, textColor, false, "------------------------");
			positionY += yOffset;
			pRenderAuxGeom->Draw2dLabel(positionX, positionY, fontSize, textColor, false, "Entity Component Type Cost Breakdown:");
			positionY += yOffset * 1.5f;

			for (const std::pair<CryGUID, SComponentTypeInfo>& componentTypeCostPair : componentTypeCostMap)
			{
				pRenderAuxGeom->Draw2dLabel(positionX, positionY, fontSize, textColor, false, "%s: %f ms", componentTypeCostPair.second.szName, componentTypeCostPair.second.totalCostMs);
				positionY += yOffset;
			}
		}
		break;
	case (int)EComponentProfilingType::EventCostBreakdown:
		{
			const CTimeValue timeBeforeComponentsUpdate = gEnv->pTimer->GetAsyncTime();
			const uint8 eventIndex = CEntity::GetEntityEventIndex(event.event);

			m_updatedEntityComponents.ForEach([&](const SMinimalEntityComponentRecord& rec) -> EComponentIterationResult
			{
				// Cache entity info before sending the event, as the entity may be removed by the event
				const SProfiledEntityEvent::SEntityInfo componentEntityInfo(*rec.pComponent->GetEntity());

				rec.pComponent->ProcessEvent(event);

				const CTimeValue timeAfterComponentsUpdate = gEnv->pTimer->GetAsyncTime();
				const float componentUpdateTime = (timeAfterComponentsUpdate - timeBeforeComponentsUpdate).GetMilliSeconds();
				if (componentUpdateTime > m_profiledEvents[eventIndex].mostExpensiveEntityCostMs)
				{
					m_profiledEvents[eventIndex].mostExpensiveEntityCostMs = componentUpdateTime;
					m_profiledEvents[eventIndex].mostExpensiveEntity = componentEntityInfo;
				}

				return EComponentIterationResult::Continue;
			});

			const CTimeValue timeAfterComponentsUpdate = gEnv->pTimer->GetAsyncTime();

			IRenderAuxGeom* pRenderAuxGeom = gEnv->pAuxGeomRenderer;

			const float componentsUpdateTime = (timeAfterComponentsUpdate - timeBeforeComponentsUpdate).GetMilliSeconds();
			m_profiledEvents[eventIndex].numEvents = m_updatedEntityComponents.Size();
			m_profiledEvents[eventIndex].totalCostMs = componentsUpdateTime;

			const float eventNameColumnPositionX = positionX;
			pRenderAuxGeom->Draw2dLabel(eventNameColumnPositionX, positionY, fontSize, textColor, false, "Event Name");

			const float numEventsColumnPositionX = eventNameColumnPositionX + 250;
			pRenderAuxGeom->Draw2dLabel(numEventsColumnPositionX, positionY, fontSize, textColor, false, "| Count");

			const float totalCostColumnPositionX = numEventsColumnPositionX + 50;
			pRenderAuxGeom->Draw2dLabel(totalCostColumnPositionX, positionY, fontSize, textColor, false, "| Total Cost (ms)");

			const float numListenerAdditionsPositionX = totalCostColumnPositionX + 125;
			pRenderAuxGeom->Draw2dLabel(numListenerAdditionsPositionX, positionY, fontSize, textColor, false, "| Added Listeners");

			const float numListenerRemovalsPositionX = numListenerAdditionsPositionX + 100;
			pRenderAuxGeom->Draw2dLabel(numListenerRemovalsPositionX, positionY, fontSize, textColor, false, "| Removed Listeners");

			const float mostExpensiveEntityPositionX = numListenerRemovalsPositionX + 100;
			pRenderAuxGeom->Draw2dLabel(mostExpensiveEntityPositionX, positionY, fontSize, textColor, false, "| Most Expensive Entity (ms)");

			positionY += yOffset;
			
			for(uint8 i = 0; i < static_cast<uint8>(Cry::Entity::EEvent::Count); ++i)
			{
				const SProfiledEntityEvent& profiledEvent = m_profiledEvents[i];
				const char* szEventName = s_eventNames[i];

				pRenderAuxGeom->Draw2dLabel(eventNameColumnPositionX, positionY, fontSize, textColor, false, szEventName);
				pRenderAuxGeom->Draw2dLabel(numEventsColumnPositionX, positionY, fontSize, textColor, false, "| %i", profiledEvent.numEvents);
				pRenderAuxGeom->Draw2dLabel(totalCostColumnPositionX, positionY, fontSize, textColor, false, "| %f", profiledEvent.totalCostMs);
				pRenderAuxGeom->Draw2dLabel(numListenerAdditionsPositionX, positionY, fontSize, textColor, false, "| %i", profiledEvent.numListenerAdditions);
				pRenderAuxGeom->Draw2dLabel(numListenerRemovalsPositionX, positionY, fontSize, textColor, false, "| %i", profiledEvent.numListenerRemovals);

				pRenderAuxGeom->Draw2dLabel(mostExpensiveEntityPositionX, positionY, fontSize, textColor, false, "| %s (%u): %f ms", profiledEvent.mostExpensiveEntity.name.c_str(), profiledEvent.mostExpensiveEntity.id, profiledEvent.mostExpensiveEntityCostMs);

				positionY += yOffset;
			}

			// Zero next frame's data
			m_profiledEvents.fill(SProfiledEntityEvent());
		}
		break;
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
IEntityItPtr CEntitySystem::GetEntityIterator()
{
	return IEntityItPtr(new CEntityItMap(m_entityArray));
}

//////////////////////////////////////////////////////////////////////////
bool CEntitySystem::IsIDUsed(EntityId nID) const
{
	return m_entityArray.IsUsed(nID);
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::SendEventToAll(SEntityEvent& event)
{
	for (CEntity* const pEntity : m_entityArray)
	{
		if (pEntity == nullptr)
			continue;

		pEntity->SendEvent(event);
	}
}

//////////////////////////////////////////////////////////////////////////
#ifndef RELEASE
void CEntitySystem::OnEditorSimulationModeChanged(EEditorSimulationMode mode)
{
	const bool isSimulating = mode != EEditorSimulationMode::Editing;

	if (isSimulating && m_entityComponentsCache)
	{
		m_entityComponentsCache->StoreEntities();
	}
	if (!isSimulating && m_entityComponentsCache)
	{
		m_entityComponentsCache->RestoreEntities();
	}

	for (CEntity* const pEntity : m_entityArray)
	{
		if (pEntity == nullptr)
			continue;

		pEntity->OnEditorGameModeChanged(isSimulating);
	}

	SEntityEvent event;
	event.event = ENTITY_EVENT_RESET;
	event.nParam[0] = isSimulating ? 1 : 0;
	SendEventToAll(event);

	if (isSimulating)
	{
		event.event = ENTITY_EVENT_START_GAME;
		SendEventToAll(event);
	}
}
#endif

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::OnLevelLoaded()
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	const SEntityEvent event(ENTITY_EVENT_LEVEL_LOADED);

	for (CEntity* const pEntity : m_entityArray)
	{
		if (pEntity == nullptr)
			continue;

		// After level load we set the simulation mode but don't start simulation yet. Mean we
		// fully prepare the object for the simulation to start. Simulation will be started with
		// the ENTITY_EVENT_START_GAME event.
		pEntity->SetSimulationMode(gEnv->IsEditor() ? EEntitySimulationMode::Editor : EEntitySimulationMode::Game);

		pEntity->SendEvent(event);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::OnLevelGameplayStart()
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	const SEntityEvent event(ENTITY_EVENT_START_GAME);

	for (CEntity* const pEntity : m_entityArray)
	{
		if (pEntity == nullptr)
			continue;

		pEntity->SetSimulationMode(gEnv->IsEditor() ? EEntitySimulationMode::Editor : EEntitySimulationMode::Game);

		pEntity->SendEvent(event);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::GetMemoryStatistics(ICrySizer* pSizer) const
{
	pSizer->AddContainer(m_guidMap);
	pSizer->AddObject(m_pClassRegistry);
	pSizer->AddContainer(m_mapEntityNames);

	m_updatedEntityComponents.GetMemoryStatistics(pSizer);
	m_prePhysicsUpdatedEntityComponents.GetMemoryStatistics(pSizer);

	{
		SIZER_COMPONENT_NAME(pSizer, "Entities");
		pSizer->AddContainer(m_deletedEntities);

	}

	for (const std::vector<IEntitySystemSink*> sinkListeners : m_sinks)
	{
		pSizer->AddContainer(sinkListeners);
	}

	pSizer->AddObject(m_pPartitionGrid);
	pSizer->AddObject(m_pProximityTriggerSystem);
	pSizer->AddObject(this, sizeof(*this));

	{
		SIZER_COMPONENT_NAME(pSizer, "EntityLoad");
		m_pEntityLoadManager->GetMemoryStatistics(pSizer);
	}

#ifndef _LIB // Only when compiling as dynamic library
	{
		//SIZER_COMPONENT_NAME(pSizer,"Strings");
		//pSizer->AddObject( (this+1),string::_usedMemory(0) );
	}
	{
		SIZER_COMPONENT_NAME(pSizer, "STL Allocator Waste");
		CryModuleMemoryInfo meminfo;
		ZeroStruct(meminfo);
		CryGetMemoryInfoForModule(&meminfo);
		pSizer->AddObject((this + 2), (size_t)meminfo.STL_wasted);
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::AddTimerEvent(SEntityTimerEvent& event, CTimeValue startTime)
{
	CTimeValue millis;
	millis.SetMilliSeconds(event.nMilliSeconds);
	CTimeValue nTriggerTime = startTime + millis;
	m_timersMap.emplace(nTriggerTime, event);
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::RemoveAllTimerEvents(ISimpleEntityEventListener* pListener)
{
	for (EntityTimersMap::const_iterator it = m_timersMap.begin(); it != m_timersMap.end();)
	{
		if (pListener == it->second.pListener)
		{
			// Remove this item.
			it = m_timersMap.erase(it);
		}
		else
		{
			++it;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CEntitySystem::RemoveAllTimerEvents(EntityId id)
{
	for (EntityTimersMap::const_iterator it = m_timersMap.begin(); it != m_timersMap.end();)
	{
		if (it->second.entityId == id)
		{
			// Remove this item.
			it = m_timersMap.erase(it);
		}
		else
		{
			++it;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::RemoveTimerEvent(ISimpleEntityEventListener* pListener, int nTimerId)
{
	for (EntityTimersMap::const_iterator it = m_timersMap.begin(), end = m_timersMap.end(); it != end; ++it)
	{
		if (pListener == it->second.pListener && nTimerId == it->second.nTimerId)
		{
			// Remove this item.
			m_timersMap.erase(it);
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::PauseTimers(bool bPause, bool bResume)
{
	m_bTimersPause = bPause;
	if (bResume)
	{
		m_nStartPause.SetSeconds(-1.0f);
		return; // just allow timers to be updated next time
	}

	if (bPause)
	{
		// record when timers pause was called
		m_nStartPause = gEnv->pTimer->GetFrameStartTime();
	}
	else if (m_nStartPause > CTimeValue(0.0f))
	{
		// increase the timers by adding the delay time passed since when
		// it was paused
		CTimeValue nCurrTimeMillis = gEnv->pTimer->GetFrameStartTime();
		CTimeValue nAdditionalTriggerTime = nCurrTimeMillis - m_nStartPause;

		EntityTimersMap::iterator it;
		EntityTimersMap lstTemp;

		for (it = m_timersMap.begin(); it != m_timersMap.end(); ++it)
		{
			lstTemp.insert(EntityTimersMap::value_type(it->first, it->second));
		} //it

		m_timersMap.clear();

		for (it = lstTemp.begin(); it != lstTemp.end(); ++it)
		{
			CTimeValue nUpdatedTimer = nAdditionalTriggerTime + it->first;
			m_timersMap.emplace(nUpdatedTimer, it->second);
		} //it

		m_nStartPause.SetSeconds(-1.0f);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::UpdateTimers()
{
	if (m_timersMap.empty())
		return;

	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	const CTimeValue currentTime = gEnv->pTimer->GetFrameStartTime();

	// Iterate through all matching timers.
	const EntityTimersMap::const_iterator first = m_timersMap.begin();
	const EntityTimersMap::const_iterator last = m_timersMap.upper_bound(currentTime);
	if (last != first)
	{
		{
			CRY_PROFILE_SECTION(PROFILE_ENTITY, "Split");
			// Make a separate list, because OnTrigger call can modify original timers map.
			m_currentTimers.clear();
			m_currentTimers.reserve(std::distance(first, last));

			for (EntityTimersMap::const_iterator it = first; it != last; ++it)
			{
				m_currentTimers.emplace_back(it->second);
			}

			// Delete these items from map.
			m_timersMap.erase(first, last);

			//////////////////////////////////////////////////////////////////////////
			// Execute OnTimer events.
		}

		{
			CRY_PROFILE_SECTION(PROFILE_ENTITY, "SendEvent");
			SEntityEvent entityEvent;
			entityEvent.event = ENTITY_EVENT_TIMER;

#ifdef ENABLE_PROFILING_CODE
			if (CVar::es_profileComponentUpdates != 0)
			{
				const CTimeValue timeBeforeTimerEvents = gEnv->pTimer->GetAsyncTime();
				const uint8 eventIndex = CEntity::GetEntityEventIndex(entityEvent.event);

				for (const SEntityTimerEvent& event : m_currentTimers)
				{
					// Send Timer event to the entity.
					entityEvent.nParam[0] = event.nTimerId;
					entityEvent.nParam[1] = event.nMilliSeconds;

					const CTimeValue timeBeforeTimerEvent = gEnv->pTimer->GetAsyncTime();

					const CEntity* const pEntity = GetEntityFromID(event.entityId);
					// Cache entity info before sending the event, as the entity may be removed by the event
					const SProfiledEntityEvent::SEntityInfo listenerEntityInfo = pEntity != nullptr ? SProfiledEntityEvent::SEntityInfo(*pEntity) : SProfiledEntityEvent::SEntityInfo();

					event.pListener->ProcessEvent(entityEvent);

					const CTimeValue timeAfterTimerEvent = gEnv->pTimer->GetAsyncTime();
					const float timerEventCostMs = (timeAfterTimerEvent - timeBeforeTimerEvent).GetMilliSeconds();

					if (timerEventCostMs > m_profiledEvents[eventIndex].mostExpensiveEntityCostMs)
					{
						m_profiledEvents[eventIndex].mostExpensiveEntityCostMs = timerEventCostMs;
						m_profiledEvents[eventIndex].mostExpensiveEntity = listenerEntityInfo;
					}
				}

				const CTimeValue timeAfterTimerEvents = gEnv->pTimer->GetAsyncTime();

				m_profiledEvents[eventIndex].numEvents = m_currentTimers.size();
				m_profiledEvents[eventIndex].totalCostMs = (timeAfterTimerEvents - timeBeforeTimerEvents).GetMilliSeconds();
			}
			else
#endif
			for (const SEntityTimerEvent& event : m_currentTimers)
			{
				// Send Timer event to the entity.
				entityEvent.nParam[0] = event.nTimerId;
				entityEvent.nParam[1] = event.nMilliSeconds;

				event.pListener->ProcessEvent(entityEvent);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::ReserveEntityId(const EntityId id)
{
	assert(id);

	const SEntityIdentifier hdl = IdToHandle(id);
	if (m_entityArray.IsUsed(hdl.GetIndex())) // Do not reserve if already used.
		return;
	//assert(m_EntitySaltBuffer.IsUsed(hdl.GetIndex()) == false);	// don't reserve twice or overriding in used one

	m_entityArray.InsertKnownHandle(hdl);
}

//////////////////////////////////////////////////////////////////////////
EntityId CEntitySystem::ReserveNewEntityId()
{
	return GenerateEntityId();
}

//////////////////////////////////////////////////////////////////////////
bool CEntitySystem::OnLoadLevel(const char* szLevelPath)
{
	int nTerrainSize = gEnv->p3DEngine ? gEnv->p3DEngine->GetTerrainSize() : 0;
	ResizeProximityGrid(nTerrainSize, nTerrainSize);
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::LoadEntities(XmlNodeRef& objectsNode, bool bIsLoadingLevelFile)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_FUNCTION_CONTEXT(EMemStatContextType::Other);

	//Update loading screen and important tick functions
	SYNCHRONOUS_LOADING_TICK();

	gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_ENTITIES, 0, 0);

	assert(m_pEntityLoadManager);
	if (!m_pEntityLoadManager->LoadEntities(objectsNode, bIsLoadingLevelFile))
	{
		EntityWarning("CEntitySystem::LoadEntities : Not all entities were loaded correctly.");
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::LoadEntities(XmlNodeRef& objectsNode, bool bIsLoadingLevelFile, const Vec3& segmentOffset)
{
	gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_ENTITIES, 0, 0);

	assert(m_pEntityLoadManager);
	if (!m_pEntityLoadManager->LoadEntities(objectsNode, bIsLoadingLevelFile, segmentOffset))
	{
		EntityWarning("CEntitySystem::LoadEntities : Not all entities were loaded correctly.");
	}
}

//////////////////////////////////////////////////////////////////////////
IEntityLayer* CEntitySystem::FindLayer(const char* szLayerName, const bool bCaseSensitive) const
{
	CEntityLayer* pResult = nullptr;
	if (szLayerName)
	{
		if (bCaseSensitive)
		{
			TLayers::const_iterator it = m_layers.find(CONST_TEMP_STRING(szLayerName));
			if (it != m_layers.end())
			{
				pResult = it->second;
			}
		}
		else
		{
			for (const TLayers::value_type& layerPair : m_layers)
			{
				if (0 == stricmp(layerPair.first.c_str(), szLayerName))
				{
					pResult = layerPair.second;
					break;
				}
			}
		}
	}
	return pResult;
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::AddEntityLayerListener(const char* szLayerName, IEntityLayerListener* pListener, const bool bCaseSensitive)
{
	if (CEntityLayer* pLayer = static_cast<CEntityLayer*>(FindLayer(szLayerName, bCaseSensitive)))
	{
		pLayer->AddListener(pListener);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::RemoveEntityLayerListener(const char* szLayerName, IEntityLayerListener* pListener, const bool bCaseSensitive)
{
	if (CEntityLayer* pLayer = static_cast<CEntityLayer*>(FindLayer(szLayerName, bCaseSensitive)))
	{
		pLayer->RemoveListener(pListener);
	}
}

#ifdef INCLUDE_DEBUG_ENTITY_DRAWING
//////////////////////////////////////////////////////////////////////////
void CEntitySystem::DebugDraw()
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	const CVar::EEntityDebugDrawType drawMode = CVar::es_EntityDebugDraw;

	// Check if we have to iterate through all entities to debug draw information.
	if (drawMode != CVar::EEntityDebugDrawType::Off)
	{
		for (CEntity* const pEntity : m_entityArray)
		{
			if (pEntity == nullptr)
				continue;

			switch (drawMode)
			{
			// All cases for bounding boxes
			case CVar::EEntityDebugDrawType::BoundingBoxWithName:
			case CVar::EEntityDebugDrawType::BoundingBoxWithPositionPhysics:
			case CVar::EEntityDebugDrawType::BoundingBoxWithEntityId:
				DebugDrawBBox(*pEntity, drawMode);
				break;
			case CVar::EEntityDebugDrawType::Hierarchies:
				DebugDrawHierachies(*pEntity);
				break;
			case CVar::EEntityDebugDrawType::Links:
				DebugDrawEntityLinks(*pEntity);
				break;
			case CVar::EEntityDebugDrawType::Components:
				DebugDrawComponents(*pEntity);
				break;
			}
		}
	}

	if (CVar::pDrawAreas->GetIVal() != 0 || CVar::pDrawAreaDebug->GetIVal() != 0 || CVar::pLogAreaDebug->GetIVal() != 0)
	{
		m_pAreaManager->DrawAreas(gEnv->pSystem);
	}

	if (CVar::pDrawAreaGrid->GetIVal() != 0)
	{
		m_pAreaManager->DrawGrid();
	}

	if (CVar::es_DebugEntityUsage > 0)
	{
		DebugDrawEntityUsage();
	}

	if (CVar::es_LayerDebugInfo > 0)
	{
		DebugDrawLayerInfo();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::DebugDrawHierachies(const CEntity& entity)
{
	IRenderAuxGeom* pRenderAux = gEnv->pAuxGeomRenderer;

	const int entityChildCount = entity.GetChildCount();
	for (int i = 0; i < entityChildCount; ++i)
	{
		if (const CEntity* pChild = static_cast<CEntity*>(entity.GetChild(i)))
		{
			pRenderAux->DrawLine(entity.GetWorldPos(), Col_Red, pChild->GetWorldPos(), Col_Blue, 1.5f);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::DebugDrawBBox(const CEntity& entity, const CVar::EEntityDebugDrawType drawMode)
{
	IRenderAuxGeom* pRenderAux = gEnv->pAuxGeomRenderer;

	Vec3 entityTranslation = entity.GetWorldTM().GetTranslation();

	if (entityTranslation.IsZero())
		return;

	ColorB boundsColor = ColorB(255, 255, 0, 255);
	
	if (drawMode == CVar::EEntityDebugDrawType::BoundingBoxWithName) // draw just the name of the entity
	{
		float colors[4] = { 1, 1, 1, 1 };
		IRenderAuxText::DrawLabelEx(entityTranslation, 1.2f, colors, true, true, entity.GetEntityTextDescription().c_str());
	}
	if (drawMode == CVar::EEntityDebugDrawType::BoundingBoxWithPositionPhysics) // draw information about physics, position, and textual representation of the entity
	{
		char szProfInfo[256];
		float colors[4] = { 1, 1, 1, 1 };

		if (entity.GetPhysicalProxy() && entity.GetPhysicalProxy()->GetPhysicalEntity())
		{
			pe_type type = entity.GetPhysics()->GetType();

			pe_status_pos pe;
			entity.GetPhysicalProxy()->GetPhysicalEntity()->GetStatus(&pe);
			cry_sprintf(szProfInfo, "Physics: %8.5f %8.5f %8.5f", pe.pos.x, pe.pos.y, pe.pos.z);

			switch (type)
			{
			case PE_STATIC:
				cry_strcat(szProfInfo, " (PE_STATIC)");
				break;
			case PE_RIGID:
				cry_strcat(szProfInfo, " (PE_RIGID)");
				break;
			case PE_WHEELEDVEHICLE:
				cry_strcat(szProfInfo, " (PE_WHEELEDVEHICLE)");
				break;
			case PE_LIVING:
				cry_strcat(szProfInfo, " (PE_LIVING)");
				break;
			case PE_PARTICLE:
				cry_strcat(szProfInfo, " (PE_PARTICLE)");
				break;
			case PE_ARTICULATED:
				cry_strcat(szProfInfo, " (PE_ARTICULATED)");
				break;
			case PE_ROPE:
				cry_strcat(szProfInfo, " (PE_ROPE)");
				break;
			case PE_SOFT:
				cry_strcat(szProfInfo, " (PE_SOFT)");
				break;
			case PE_AREA:
				cry_strcat(szProfInfo, " (PE_AREA)");
				break;
			case PE_GRID:
				cry_strcat(szProfInfo, " (PE_GRID)");
				break;
			case PE_WALKINGRIGID:
				cry_strcat(szProfInfo, " (PE_WALKINGRIGID)");
				break;
			}

			entityTranslation.z -= 0.1f;
			IRenderAuxText::DrawLabelEx(entityTranslation, 1.1f, colors, true, true, szProfInfo);
		}

		Vec3 entPos = entity.GetPos();
		cry_sprintf(szProfInfo, "Entity:  %8.5f %8.5f %8.5f", entPos.x, entPos.y, entPos.z);
		entityTranslation.z -= 0.1f;
		IRenderAuxText::DrawLabelEx(entityTranslation, 1.1f, colors, true, true, szProfInfo);
	}
	if(drawMode == CVar::EEntityDebugDrawType::BoundingBoxWithEntityId) // draw only the EntityId (no other information, like position, physics status, etc.)
	{
		Vec3 wp = entity.GetWorldTM().GetTranslation();

		static const ColorF colorsToChooseFrom[] =
		{
			Col_Green,
			Col_Blue,
			Col_Red,
			Col_Yellow,
			Col_Gray,
			Col_Orange,
			Col_Pink,
			Col_Cyan,
			Col_Magenta
		};

		char textToRender[256];
		cry_sprintf(textToRender, "EntityId: %u", entity.GetId());
		const ColorF& color = colorsToChooseFrom[entity.GetId() % CRY_ARRAY_COUNT(colorsToChooseFrom)];

		// Draw text.
		float colors[4] = { color.r, color.g, color.b, 1.0f };
		IRenderAuxText::DrawLabelEx(wp, 1.2f, colors, true, true, textToRender);

		// specify color for drawing bounds below
		boundsColor.set((uint8)(color.r * 255.0f), (uint8)(color.g * 255.0f), (uint8)(color.b * 255.0f), 255);
	}

	// Draw bounds.
	AABB box;
	entity.GetLocalBounds(box);
	if (box.min == box.max)
	{
		box.min = entityTranslation - Vec3(0.1f, 0.1f, 0.1f);
		box.max = entityTranslation + Vec3(0.1f, 0.1f, 0.1f);
	}

	pRenderAux->DrawAABB(box, entity.GetWorldTM(), false, boundsColor, eBBD_Extremes_Color_Encoded);
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::DebugDrawEntityUsage()
{
	static float fLastUpdate = 0.0f;
	float fCurrTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();

	struct SEntityClassDebugInfo
	{
		SEntityClassDebugInfo() = default;
		explicit SEntityClassDebugInfo(IEntityClass* pEntityClass) : pClass(pEntityClass) {}

		IEntityClass* pClass;

		size_t        memoryUsage = 0;
		uint32        numEntities = 0;
		uint32        numActiveEntities = 0;
		uint32        numNotHiddenEntities = 0;

		// Amount of memory used by hidden entities
		size_t hiddenMemoryUsage = 0;
	};

	static std::vector<SEntityClassDebugInfo> debuggedEntityClasses;

	enum class EEntityUsageSortMode
	{
		None = 0,
		ActiveInstances,
		MemoryUsage
	};

	EEntityUsageSortMode sortMode = (EEntityUsageSortMode)CVar::es_DebugEntityUsageSortMode;

	ICrySizer* pSizer = gEnv->pSystem->CreateSizer();

	if (fCurrTime - fLastUpdate >= 0.001f * max(CVar::es_DebugEntityUsage, 1000))
	{
		fLastUpdate = fCurrTime;

		string sFilter = CVar::es_DebugEntityUsageFilter;
		sFilter.MakeLower();

		debuggedEntityClasses.clear();
		debuggedEntityClasses.reserve(GetClassRegistry()->GetClassCount());

		for (const CEntity* const pEntity : m_entityArray)
		{
			if (pEntity == nullptr)
				continue;

			IEntityClass* pEntityClass = pEntity->GetClass();

			if (!sFilter.empty())
			{
				string szName = pEntityClass->GetName();
				szName.MakeLower();
				if (szName.find(sFilter) == string::npos)
					continue;
			}

			auto debuggedEntityClassIterator = std::find_if(debuggedEntityClasses.begin(), debuggedEntityClasses.end(), [pEntityClass](const SEntityClassDebugInfo& classInfo)
			{
				return classInfo.pClass == pEntityClass;
			});

			if (debuggedEntityClassIterator == debuggedEntityClasses.end())
			{
				debuggedEntityClasses.emplace_back(pEntityClass);
				debuggedEntityClassIterator = --debuggedEntityClasses.end();
			}

			// Calculate memory usage
			const uint32 prevMemoryUsage = pSizer->GetTotalSize();
			pEntity->GetMemoryUsage(pSizer);
			const uint32 uMemoryUsage = pSizer->GetTotalSize() - prevMemoryUsage;
			pSizer->Reset();

			debuggedEntityClassIterator->memoryUsage += uMemoryUsage;
			debuggedEntityClassIterator->numEntities++;

			if (pEntity->IsActivatedForUpdates())
			{
				debuggedEntityClassIterator->numActiveEntities++;
			}

			if (pEntity->IsHidden())
			{
				debuggedEntityClassIterator->hiddenMemoryUsage += uMemoryUsage;
			}
			else
			{
				debuggedEntityClassIterator->numNotHiddenEntities++;
			}
		}

		if (sortMode == EEntityUsageSortMode::ActiveInstances)
		{
			std::sort(debuggedEntityClasses.begin(), debuggedEntityClasses.end(), [](const SEntityClassDebugInfo& classInfoLeft, const SEntityClassDebugInfo& classInfoRight)
			{
				return classInfoLeft.numActiveEntities > classInfoRight.numActiveEntities;
			});
		}
		else if (sortMode == EEntityUsageSortMode::MemoryUsage)
		{
			std::sort(debuggedEntityClasses.begin(), debuggedEntityClasses.end(), [](const SEntityClassDebugInfo& classInfoLeft, const SEntityClassDebugInfo& classInfoRight)
			{
				return classInfoLeft.memoryUsage > classInfoRight.memoryUsage;
			});
		}
	}

	pSizer->Release();

	if (!debuggedEntityClasses.empty())
	{
		string title = "Entity Class";
		if (CVar::es_DebugEntityUsageFilter && CVar::es_DebugEntityUsageFilter[0])
		{
			title.append(" [");
			title.append(CVar::es_DebugEntityUsageFilter);
			title.append("]");
		}

		float columnY = 11.0f;
		const float colWhite[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		const float colGreen[] = { 0.0f, 1.0f, 0.0f, 1.0f };

		const float columnX_Class = 50.0f;
		const float columnX_ActiveCount = 350.0f;
		const float columnX_NotHiddenCount = 450.0f;
		const float columnX_MemoryUsage = 550.0f;
		const float columnX_MemoryHidden = 650.0f;

		IRenderAuxText::Draw2dLabel(columnX_Class, columnY, 1.2f, colWhite, false, "%s", title.c_str());
		IRenderAuxText::Draw2dLabel(columnX_ActiveCount, columnY, 1.2f, colWhite, true, "Active");
		IRenderAuxText::Draw2dLabel(columnX_NotHiddenCount, columnY, 1.2f, colWhite, true, "Not Hidden");
		IRenderAuxText::Draw2dLabel(columnX_MemoryUsage, columnY, 1.2f, colWhite, true, "Memory Usage");
		IRenderAuxText::Draw2dLabel(columnX_MemoryHidden, columnY, 1.2f, colWhite, true, "[Only Hidden]");
		columnY += 15.0f;

		for (const SEntityClassDebugInfo& debugEntityClassInfo : debuggedEntityClasses)
		{
			const char* szName = debugEntityClassInfo.pClass->GetName();

			IRenderAuxText::Draw2dLabel(columnX_Class, columnY, 1.0f, colWhite, false, "%s", szName);
			IRenderAuxText::Draw2dLabel(columnX_ActiveCount, columnY, 1.0f, colWhite, true, "%u", debugEntityClassInfo.numActiveEntities);
			IRenderAuxText::Draw2dLabel(columnX_NotHiddenCount, columnY, 1.0f, colWhite, true, "%u", debugEntityClassInfo.numNotHiddenEntities);
			IRenderAuxText::Draw2dLabel(columnX_MemoryUsage, columnY, 1.0f, colWhite, true, "%u (%uKb)", debugEntityClassInfo.memoryUsage, debugEntityClassInfo.memoryUsage / 1000);
			IRenderAuxText::Draw2dLabel(columnX_MemoryHidden, columnY, 1.0f, colGreen, true, "%u (%uKb)", debugEntityClassInfo.hiddenMemoryUsage, debugEntityClassInfo.hiddenMemoryUsage / 1000);
			columnY += 12.0f;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

#ifdef ENABLE_PROFILING_CODE
namespace
{
	struct SLayerDesc
	{
		SLayerDesc(CEntityLayer* pLayer)
			: pLayer(pLayer)
		{}
		typedef std::vector<SLayerDesc> TLayerDescs;
		CEntityLayer* pLayer;
		TLayerDescs childLayers;
	};

	typedef std::vector<SLayerDesc> TParentLayers;
	typedef std::vector<CEntityLayer*> TLayerPtrs;

	SLayerDesc* FindLayerDesc(SLayerDesc& inLayerDesc, CEntityLayer* pLayer)
	{
		SLayerDesc* pOutLayer = nullptr;
		if (pLayer == inLayerDesc.pLayer)
		{
			pOutLayer = &inLayerDesc;
		}
		else
		{
			for (SLayerDesc& layerDesc : inLayerDesc.childLayers)
			{
				pOutLayer = FindLayerDesc(layerDesc, pLayer);
				if (pOutLayer)
					break;
			}
		}
		return pOutLayer;
	}

	SLayerDesc* FindLayerDesc(TParentLayers& parentLayers, CEntityLayer* pLayer)
	{
		SLayerDesc* pOutLayer = nullptr;
		for (SLayerDesc& layerDesc : parentLayers)
		{
			pOutLayer = FindLayerDesc(layerDesc, pLayer);
			if (pOutLayer)
				break;
		}
		return pOutLayer;
	}

	// lambda contract: [](const SLayerDesc& layerDesc, size_t level) -> bool
	// return true to stop the loop
	template <typename TLayerDescFunctor>
	bool ForEachLayerDesc(const SLayerDesc& inLayerDesc, TLayerDescFunctor func, size_t level = 0)
	{
		if (func(inLayerDesc, level))
			return true;
		
		for (const SLayerDesc& layerDesc : inLayerDesc.childLayers)
		{
			if (ForEachLayerDesc(layerDesc, func, level + 1))
				return true;
		}
		return false;
	}

	template <typename TLayerDescFunctor>
	bool ForEachLayerDesc(const TParentLayers& parentLayers, TLayerDescFunctor func)
	{
		for (const SLayerDesc& layerDesc : parentLayers)
		{
			if (ForEachLayerDesc(layerDesc, func))
				return true;
		}
		return false;
	}
}
#endif

void CEntitySystem::DebugDrawLayerInfo()
{
#ifdef ENABLE_PROFILING_CODE
	bool shouldRenderAllLayerStats = CVar::es_LayerDebugInfo >= 2;
	bool shouldRenderMemoryStats = CVar::es_LayerDebugInfo == 3;
	bool shouldRenderWithoutFolder = CVar::es_LayerDebugInfo == 4;
	bool shouldShowLayerActivation = CVar::es_LayerDebugInfo == 5;

	const float xstep = 480.f;
	const float ystep = 12.0f;
	const float ybegin = 30.f;
	float tx = 0.f;
	float ty = ybegin;
	ColorF clText(0, 1, 1, 1);
	IRenderAuxGeom* pRenderAux = IRenderAuxGeom::GetAux();
	const float screenHeight = float(pRenderAux->GetCamera().GetViewSurfaceZ());

	auto AdjustTextPos = [=](float& tx, float& ty)
	{
		if (!shouldRenderMemoryStats && (ty + ystep * 2.f) > screenHeight)
		{
			tx += xstep;
			ty = ybegin;
		}
	};

	if (shouldShowLayerActivation) // Show which layer was switched on or off
	{
		const float fShowTime = 10.0f; // 10 seconds
		float fCurTime = gEnv->pTimer->GetCurrTime();
		float fPrevTime = 0.f;
		std::vector<SLayerProfile>::iterator ppClearProfile = m_layerProfiles.end();
		for (std::vector<SLayerProfile>::iterator ppProfiles = m_layerProfiles.begin(); ppProfiles != m_layerProfiles.end(); ++ppProfiles)
		{
			SLayerProfile& profile = (*ppProfiles);
			CEntityLayer* pLayer = profile.pLayer;

			ColorF clTextProfiledTime(0, 1, 1, 1);
			if (profile.fTimeMS > 50.f)  // Red color for more then 50 ms
				clTextProfiledTime = ColorF(1, 0.3f, 0.3f, 1);
			else if (profile.fTimeMS > 10.f)    // Yellow color for more then 10 ms
				clTextProfiledTime = ColorF(1, 1, 0.3f, 1);

			if (!profile.isEnable)
				clTextProfiledTime -= ColorF(0.3f, 0.3f, 0.3f, 0);

			float xindent = 0.0f;
			if (strlen(pLayer->GetParentName()) > 0)
			{
				xindent += 20.f;
				const IEntityLayer* const pParentLayer = FindLayer(pLayer->GetParentName());
				if (pParentLayer && strlen(pParentLayer->GetParentName()) > 0)
					xindent += 20.f;
			}
			if (profile.fTimeOn != fPrevTime)
			{
				ty += 10.0f;
				fPrevTime = profile.fTimeOn;
			}

			AdjustTextPos(tx, ty);
			DrawText(tx + xindent, ty += ystep, clTextProfiledTime, "%.1f ms: %s (%s)", profile.fTimeMS, pLayer->GetName(), profile.isEnable ? "On" : "Off");
			if (ppClearProfile == m_layerProfiles.end() && fCurTime - profile.fTimeOn > fShowTime)
				ppClearProfile = ppProfiles;
		}
		if (ppClearProfile != m_layerProfiles.end())
			m_layerProfiles.erase(ppClearProfile, m_layerProfiles.end());
		return;
	}

	SLayerPakStats layerPakStats;
	layerPakStats.m_MaxSize = 0;
	layerPakStats.m_UsedSize = 0;
	IResourceManager* pResMan = gEnv->pSystem->GetIResourceManager();
	if (pResMan)
		pResMan->GetLayerPakStats(layerPakStats, shouldShowLayerActivation);

	if (shouldShowLayerActivation)
	{
		AdjustTextPos(tx, ty);
		DrawText(tx, ty += ystep, clText, "Layer Pak Stats: %1.1f MB / %1.1f MB)",
			(float)layerPakStats.m_UsedSize / (1024.f * 1024.f),
			(float)layerPakStats.m_MaxSize / (1024.f * 1024.f));

		for (SLayerPakStats::TEntries::iterator it = layerPakStats.m_entries.begin(); it != layerPakStats.m_entries.end(); ++it)
		{
			SLayerPakStats::SEntry& entry = *it;

			AdjustTextPos(tx, ty);
			DrawText(tx, ty += ystep, clText, "  %20s: %1.1f MB - %s)", entry.name.c_str(),
				(float)entry.nSize / (1024.f * 1024.f), entry.status.c_str());
		}
		AdjustTextPos(tx, ty);
		DrawText(tx, ty += ystep, clText, "All Layers:");
	}
	else
	{
		string tmp = "Active Brush Layers";
		if (shouldRenderAllLayerStats)
			tmp = "All Layers";

		AdjustTextPos(tx, ty);
		DrawText(tx, ty += ystep, clText, "%s (PakInfo: %1.1f MB / %1.1f MB):", tmp.c_str(),
			(float)layerPakStats.m_UsedSize / (1024.f * 1024.f),
			(float)layerPakStats.m_MaxSize / (1024.f * 1024.f));
	}

	ty += ystep;

	ICrySizer* pSizer = 0;
	if (shouldRenderMemoryStats)
		pSizer = gEnv->pSystem->CreateSizer();

	std::vector<bool> layerUsed;
	TLayerPtrs layerPtrs;
	TParentLayers parentLayers;

	// Populate temporary flat layer storage
	// This has some unnecessary expense per each frame
	layerPtrs.reserve(m_layers.size());
	layerUsed.reserve(m_layers.size());
	for (const TLayers::value_type& layerPair : m_layers)
	{
		layerPtrs.push_back(layerPair.second);
		layerUsed.push_back(false);
	}

	const size_t layersCount = layerPtrs.size();
	const size_t layersLast = layersCount - 1;
	size_t layersUsed = 0;

	// Populate hierarchy of layers
	for (size_t i = 0; layersUsed != layersCount; ++i)
	{
		if (i > layersLast)
			i = 0;

		if (layerUsed[i])
			continue;

		CEntityLayer* pLayer = layerPtrs[i];
		CEntityLayer* pParentLayer = static_cast<CEntityLayer*>(FindLayer(pLayer->GetParentName()));

		if (pParentLayer)
		{
			SLayerDesc* pLayerDesc = FindLayerDesc(parentLayers, pParentLayer);
			if (pLayerDesc)
			{
				pLayerDesc->childLayers.emplace_back(pLayer);
				layerUsed[i] = true;
				layersUsed++;
			}
		}
		else
		{
			parentLayers.emplace_back(pLayer);
			layerUsed[i] = true;
			layersUsed++;
		}
	}

	for (const SLayerDesc& layerDesc : parentLayers)
	{
		CEntityLayer* pParent = layerDesc.pLayer;
		const char* szParentName = pParent->GetName();

		bool bIsEnabled = false;
		if (shouldRenderAllLayerStats)
		{
			bIsEnabled = true;
		}
		else
		{
			ForEachLayerDesc(layerDesc, [&](const SLayerDesc& inLayerDesc, size_t level)
			{
				if (inLayerDesc.pLayer->IsEnabledBrush())
				{
					bIsEnabled = true;
					return true;
				}	
				return false;
			});
		}

		if (!bIsEnabled)
			continue;

		SLayerPakStats::SEntry* pLayerPakEntry = 0;
		for (SLayerPakStats::TEntries::iterator it2 = layerPakStats.m_entries.begin();
			it2 != layerPakStats.m_entries.end(); ++it2)
		{
			if (it2->name == szParentName)
			{
				pLayerPakEntry = &(*it2);
				break;
			}
		}

		if (pLayerPakEntry)
		{
			AdjustTextPos(tx, ty);
			DrawText(tx, ty += ystep, clText, "%s (PakInfo - %1.1f MB - %s)", szParentName,
				(float)pLayerPakEntry->nSize / (1024.f * 1024.f), pLayerPakEntry->status.c_str());
		}

		ForEachLayerDesc(layerDesc, [&](const SLayerDesc& inLayerDesc, size_t level)
		{
			if (pLayerPakEntry && level == 0)
				return false;

			if (shouldRenderWithoutFolder && !inLayerDesc.childLayers.empty())
				return false;

			stack_string levelStr(level, '.');
			CEntityLayer* pChild = inLayerDesc.pLayer;
			const char* szChildName = pChild->GetName();

			if (shouldRenderAllLayerStats)
			{
				const char* state = "enabled";
				ColorF clTextState(0, 1, 1, 1);
				if (pChild->IsEnabled() && !pChild->IsEnabledBrush())
				{
					// a layer was not disabled by Flowgraph in time when level is starting
					state = "was not disabled";
					clTextState = ColorF(1, 0.3f, 0.3f, 1);
				}
				else if (!pChild->IsEnabled())
				{
					state = "disabled";
					clTextState = ColorF(0.7f, 0.7f, 0.7f, 1);
				}
				AdjustTextPos(tx, ty);
				DrawText(tx, ty += ystep, clTextState, "%s%s (%s)", levelStr.c_str(), szChildName, state);

				if (shouldRenderMemoryStats && pSizer)
				{
					pSizer->Reset();

					int numBrushes, numDecals;
					gEnv->p3DEngine->GetLayerMemoryUsage(pChild->GetId(), pSizer, &numBrushes, &numDecals);

					int numEntities;
					pChild->GetMemoryUsage(pSizer, &numEntities);
					const float memorySize = float(pSizer->GetTotalSize()) / 1024.f;
					const int kColumnPos = 350;

					if (numDecals)
						DrawText(tx + kColumnPos, ty, clTextState, "Brushes: %d, Decals: %d, Entities: %d; Mem: %.2f Kb", numBrushes, numDecals, numEntities, memorySize);
					else
						DrawText(tx + kColumnPos, ty, clTextState, "Brushes: %d, Entities: %d; Mem: %.2f Kb", numBrushes, numEntities, memorySize);
				}
			}
			else if (pChild->IsEnabledBrush())
			{
				AdjustTextPos(tx, ty);
				DrawText(tx, ty += ystep, clText, "%s%s", levelStr.c_str(), szChildName);
			}

			return false;
		});
	}

	SAFE_RELEASE(pSizer);

#endif // ~ENABLE_PROFILING_CODE
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::DebugDrawEntityLinks(CEntity& entity)
{
	IRenderAuxGeom* pRenderAux = gEnv->pAuxGeomRenderer;

	IEntityLink* pLink = entity.GetEntityLinks();
	while (pLink != nullptr)
	{
		if (const CEntity* pTarget = GetEntityFromID(pLink->entityId))
		{
			pRenderAux->DrawLine(entity.GetWorldPos(), ColorF(0.0f, 1.0f, 0.0f), pTarget->GetWorldPos(), ColorF(0.0f, 1.0f, 0.0f));

			Vec3 pos = 0.5f * (entity.GetWorldPos() + pTarget->GetWorldPos());

			Vec3 camDir = pRenderAux->GetCamera().GetPosition() - entity.GetWorldPos();
			float camDist = camDir.GetLength();

			const float maxDist = 100.0f;
			if (camDist < maxDist)
			{
				float rangeRatio = 1.0f;
				float range = maxDist / 2.0f;
				if (camDist > range)
				{
					rangeRatio = 1.0f * (1.0f - (camDist - range) / range);
				}

				IRenderAuxText::DrawLabelEx(pos, 1.2f, ColorF(0.4f, 1.0f, 0.0f, rangeRatio), true, false, pLink->name);
			}
		}

		pLink = pLink->next;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::DebugDrawComponents(const CEntity& entity)
{
	IRenderAuxGeom* pRenderAux = gEnv->pAuxGeomRenderer;

	Vec3 entityTranslation = entity.GetWorldTM().GetTranslation();
	if (entityTranslation.IsZero())
		return;

	const Vec3 camDir = pRenderAux->GetCamera().GetPosition() - entityTranslation;
	const float maxDist = 100.0f;

	if (camDir.GetLength() < maxDist)
	{
		ColorF color = Col_White;
		IRenderAuxText::DrawLabelEx(entityTranslation, 1.2f, color, true, true, entity.GetEntityTextDescription().c_str());
		entityTranslation.z -= 0.1f;

		DynArray<IEntityComponent*> components;
		entity.GetComponents(components);

		for (const IEntityComponent* const pEntityComponent : components)
		{
			bool getsUpdated = pEntityComponent->GetEventMask().Check(ENTITY_EVENT_UPDATE);
			bool getsPrePhysics = pEntityComponent->GetEventMask().Check(ENTITY_EVENT_PREPHYSICSUPDATE);

			if (getsUpdated)
				color = Col_Red;
			else if(getsPrePhysics)
				color = Col_Yellow;
			else
				color = Col_White;

			IRenderAuxText::DrawLabelEx(entityTranslation, 1.2f, color, true, false, pEntityComponent->GetClassDesc().GetName().c_str());
			entityTranslation.z -= 0.1f;
		}
	}
}

#endif // ~INCLUDE_DEBUG_ENTITY_DRAWING

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::RegisterEntityGuid(const EntityGUID& guid, EntityId id)
{
	if (CVar::es_DebugFindEntity != 0)
	{
		CryLog("RegisterEntityGuid: %s, [%d]", guid.ToDebugString(), id);
	}

	if (guid.lopart == 0)
	{
		m_bSupportLegacy64bitGuids = true;
	}

	m_guidMap.insert(EntityGuidMap::value_type(guid, id));
	CEntity* pCEntity = GetEntityFromID(id);
	if (pCEntity)
		pCEntity->m_guid = guid;
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::UnregisterEntityGuid(const EntityGUID& guid)
{
	if (CVar::es_DebugFindEntity != 0)
	{
		CryLog("UnregisterEntityGuid: %s", guid.ToDebugString());
	}

	m_guidMap.erase(guid);
}

//////////////////////////////////////////////////////////////////////////
EntityId CEntitySystem::FindEntityByGuid(const EntityGUID& guid) const
{
	if (CVar::es_DebugFindEntity != 0)
	{
		CryLog("FindEntityByGuid: %s", guid.ToDebugString());
	}
	EntityId result = stl::find_in_map(m_guidMap, guid, INVALID_ENTITYID);

	if (result == INVALID_ENTITYID)
	{
		if (guid.hipart != 0 && guid.lopart == 0)
		{
			// A special case of Legacy 64bit GUID
			for (auto& item : m_guidMap)
			{
				if (item.first.hipart == guid.hipart)
				{
					// GUID found even when not a full match
					result = item.second;
					break;
				}
			}
		}
		else if (m_bSupportLegacy64bitGuids)
		{
			// A special case when loading old not-reexported maps where Entity GUIDs where 64bit
			CryGUID shortGuid = guid;
			shortGuid.lopart = 0;
			result = stl::find_in_map(m_guidMap, shortGuid, INVALID_ENTITYID);
		}
	}
	return result;
}

//////////////////////////////////////////////////////////////////////////
IEntityArchetype* CEntitySystem::LoadEntityArchetype(const char* sArchetype)
{
	return m_pEntityArchetypeManager->LoadArchetype(sArchetype);
}

//////////////////////////////////////////////////////////////////////////
IEntityArchetype* CEntitySystem::LoadEntityArchetype(XmlNodeRef oArchetype)
{
	IEntityArchetype* piArchetype(NULL);

	if (!oArchetype)
	{
		return NULL;
	}

	if (oArchetype->isTag("EntityPrototype"))
	{
		const char* name = oArchetype->getAttr("Name");
		const char* library = oArchetype->getAttr("Library");

		string strArchetypename;

		strArchetypename.Format("%s.%s", library, name);
		piArchetype = m_pEntityArchetypeManager->FindArchetype(strArchetypename);

		if (!piArchetype)
		{
			const char* className = oArchetype->getAttr("Class");

			IEntityClass* pClass = m_pClassRegistry->FindClass(className);
			if (!pClass)
			{
				// No such entity class.
				EntityWarning("EntityArchetype %s references unknown entity class %s", strArchetypename.c_str(), className);
				return NULL;
			}

			piArchetype = m_pEntityArchetypeManager->CreateArchetype(pClass, strArchetypename);
		}
	}

	if (piArchetype != NULL)
	{
		// We need to pass the properties node to the LoadFromXML
		XmlNodeRef propNode = oArchetype->findChild("Properties");
		XmlNodeRef objectVarsNode = oArchetype->findChild("ObjectVars");
		if (propNode != NULL)
		{
			piArchetype->LoadFromXML(propNode, objectVarsNode);
		}

		if (IEntityArchetypeManagerExtension* pExtension = m_pEntityArchetypeManager->GetEntityArchetypeManagerExtension())
		{
			pExtension->LoadFromXML(*piArchetype, oArchetype);
		}
	}

	return piArchetype;
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::UnloadEntityArchetype(const char* sArchetype)
{
	m_pEntityArchetypeManager->UnloadArchetype(sArchetype);
}

//////////////////////////////////////////////////////////////////////////
IEntityArchetype* CEntitySystem::CreateEntityArchetype(IEntityClass* pClass, const char* sArchetype)
{
	return m_pEntityArchetypeManager->CreateArchetype(pClass, sArchetype);
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::RefreshEntityArchetypesInRegistry()
{
	m_pClassRegistry->LoadArchetypes("Libs/EntityArchetypes", true);
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::SetEntityArchetypeManagerExtension(IEntityArchetypeManagerExtension* pEntityArchetypeManagerExtension)
{
	m_pEntityArchetypeManager->SetEntityArchetypeManagerExtension(pEntityArchetypeManagerExtension);
}

IEntityArchetypeManagerExtension* CEntitySystem::GetEntityArchetypeManagerExtension() const
{
	return m_pEntityArchetypeManager->GetEntityArchetypeManagerExtension();
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::Serialize(TSerialize ser)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		ser.BeginGroup("Timers");

		ser.Value("Paused", m_bTimersPause);
		ser.Value("PauseStart", m_nStartPause);

		int count = m_timersMap.size();
		ser.Value("timerCount", count);

		if (ser.IsWriting())
		{
			EntityTimersMap::iterator it;
			for (it = m_timersMap.begin(); it != m_timersMap.end(); ++it)
			{
				SEntityTimerEvent tempEvent = it->second;
				if (tempEvent.componentInstanceGUID.IsNull())
				{
					// We cannot deserialize without knowing which component instance to map to, skip.
					continue;
				}

				ser.BeginGroup("Timer");
				ser.Value("entityId", tempEvent.entityId);
				ser.Value("componentInstanceGUIDHipart", tempEvent.componentInstanceGUID.hipart);
				ser.Value("componentInstanceGUIDLopart", tempEvent.componentInstanceGUID.lopart);
				ser.Value("eventTime", tempEvent.nMilliSeconds);
				ser.Value("timerID", tempEvent.nTimerId);
				CTimeValue start = it->first;
				ser.Value("startTime", start);
				ser.EndGroup();
			}
		}
		else
		{
			m_timersMap.clear();

			CTimeValue start;
			for (int c = 0; c < count; ++c)
			{
				SEntityTimerEvent tempEvent;

				ser.BeginGroup("Timer");
				ser.Value("entityId", tempEvent.entityId);
				ser.Value("componentInstanceGUIDHipart", tempEvent.componentInstanceGUID.hipart);
				ser.Value("componentInstanceGUIDLopart", tempEvent.componentInstanceGUID.lopart);
				ser.Value("eventTime", tempEvent.nMilliSeconds);
				ser.Value("timerID", tempEvent.nTimerId);
				ser.Value("startTime", start);
				ser.EndGroup();
				start.SetMilliSeconds((int64)(start.GetMilliSeconds() - tempEvent.nMilliSeconds));

				if (CEntity* pEntity = GetEntityFromID(tempEvent.entityId))
				{
					if (tempEvent.pListener = pEntity->GetComponentByGUID(tempEvent.componentInstanceGUID))
					{
						AddTimerEvent(tempEvent, start);
					}
					else
					{
						EntityWarning("Deserialized timer event with id %i for entity %u, but component with instance GUID %s did not exist!", tempEvent.nTimerId, tempEvent.entityId, tempEvent.componentInstanceGUID.ToDebugString());
					}
				}
				else
				{
					EntityWarning("Deserialized timer event with id %i for entity %u, but entity did not exist!", tempEvent.nTimerId, tempEvent.entityId);
				}
			}
		}

		ser.EndGroup();

		if (gEnv->pScriptSystem)
		{
			gEnv->pScriptSystem->SerializeTimers(GetImpl(ser));
		}

		ser.BeginGroup("Layers");
		TLayerActivationOpVec deferredLayerActivations;

		for (TLayers::const_iterator iLayer = m_layers.begin(), iEnd = m_layers.end(); iLayer != iEnd; ++iLayer)
			iLayer->second->Serialize(ser, deferredLayerActivations);

		if (ser.IsReading() && deferredLayerActivations.size())
		{
			std::sort(deferredLayerActivations.begin(), deferredLayerActivations.end(), LayerActivationPriority);

			for (std::size_t i = deferredLayerActivations.size(); i > 0; i--)
			{
				SPostSerializeLayerActivation& op = deferredLayerActivations[i - 1];
				assert(op.m_layer && op.m_func);
				(op.m_layer->*op.m_func)(op.enable);
			}
		}
		else if (deferredLayerActivations.size())
			CryFatalError("added a deferred layer activation on writing !?");

		ser.EndGroup();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::ResizeProximityGrid(int nWidth, int nHeight)
{
	if (m_pPartitionGrid)
		m_pPartitionGrid->AllocateGrid((float)nWidth, (float)nHeight);
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::SetNextSpawnId(EntityId id)
{
	if (id)
		RemoveEntity(id, true);
	m_idForced = id;
}

void CEntitySystem::ResetAreas()
{
	m_pAreaManager->ResetAreas();
	CEntityComponentArea::ResetTempState();
}

void CEntitySystem::UnloadAreas()
{
	ResetAreas();
	m_pAreaManager->UnloadAreas();
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::DumpEntities()
{
	IEntityItPtr it = GetEntityIterator();
	it->MoveFirst();

	int count = 0;

	CryLogAlways("--------------------------------------------------------------------------------");
	while (CEntity* pEntity = static_cast<CEntity*>(it->Next()))
	{
		DumpEntity(pEntity);

		++count;
	}
	CryLogAlways("--------------------------------------------------------------------------------");
	CryLogAlways(" %d entities (%" PRISIZE_T " active components)", count, m_updatedEntityComponents.Size());
	CryLogAlways("--------------------------------------------------------------------------------");
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::ResumePhysicsForSuppressedEntities(bool bWakeUp)
{
	IEntityItPtr it = GetEntityIterator();
	it->MoveFirst();

	while (CEntity* pEntity = static_cast<CEntity*>(it->Next()))
	{
		if (pEntity->CheckFlags(ENTITY_FLAG_IGNORE_PHYSICS_UPDATE))
		{
			pEntity->ClearFlags(ENTITY_FLAG_IGNORE_PHYSICS_UPDATE);

			// Recreate the physics object
			pEntity->EnablePhysics(false);
			pEntity->EnablePhysics(true);

			IPhysicalEntity* pPhysEnt = pEntity->GetPhysics();
			if (NULL != pPhysEnt)
			{
				// Sync position of the entity with physical entity
				pe_params_pos params;
				params.pos = pEntity->GetPos();
				params.q = pEntity->GetRotation();
				pPhysEnt->SetParams(&params);

				// Only need to wake up sleeping entities
				if (bWakeUp)
				{
					pe_status_awake status;
					if (pPhysEnt->GetStatus(&status) == 0)
					{
						pe_action_awake wake;
						wake.bAwake = 1;
						pPhysEnt->Action(&wake);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::SaveInternalState(IDataWriteStream& writer) const
{
	// entities are exported in CObjectManager in editor
}

void CEntitySystem::LoadInternalState(IDataReadStream& reader)
{
	// get layers in the level
	std::vector<CEntityLayer*> allLayers;
	for (TLayers::iterator it = m_layers.begin();
	     it != m_layers.end(); ++it)
	{
		CEntityLayer* pLayer = it->second;
		allLayers.push_back(pLayer);
	}

	// enable layers that are explicitly listed
	const uint32 numLayers = reader.ReadUint32();
	for (uint32 i = 0; i < numLayers; ++i)
	{
		const string layerName = reader.ReadString();

		// find local layer
		CEntityLayer* pLayer = NULL;
		for (TLayers::iterator it = m_layers.begin();
		     it != m_layers.end(); ++it)
		{
			if (it->second->GetName() == layerName)
			{
				pLayer = it->second;
				break;
			}
		}

		// No layer found
		if (NULL == pLayer)
		{
			gEnv->pLog->LogAlways("Layer with name '%s' not found in local data",
			                      layerName.c_str());
			continue;
		}

		// remove from the "all layers" list - what's left will be hidden
		std::vector<CEntityLayer*>::iterator it = std::find(allLayers.begin(), allLayers.end(), pLayer);
		if (it != allLayers.end())
		{
			allLayers.erase(it);
		}

		pLayer->Enable(true, false, false); // NON RECURSIVE

		// show/hide the objects also
		if (pLayer->GetId() != 0)
		{
			gEnv->p3DEngine->ActivateObjectsLayer(pLayer->GetId(), true, true, true, true, pLayer->GetName(), NULL, false);
		}
	}

	// Hide rest of the layers
	for (std::vector<CEntityLayer*>::const_iterator it = allLayers.begin();
	     it != allLayers.end(); ++it)
	{
		CEntityLayer* pLayer = *it;
		pLayer->Enable(false, false, false); // NON RECURSIVE

		// show/hide the objects also
		if (pLayer->GetId() != 0)
		{
			gEnv->p3DEngine->ActivateObjectsLayer(pLayer->GetId(), false, true, true, true, pLayer->GetName(), NULL, false);
		}
	}

	// Load entities data
	{
		// get current entities on the level
		typedef std::set<EntityGUID> TEntitySet;
		TEntitySet currentEntities;
		for (const CEntity* const pEntity : m_entityArray)
		{
			if (pEntity == nullptr)
				continue;

			gEnv->pLog->LogAlways("ExistingEntity: '%s' '%s' (ID=%d, GUID=%s, Flags=0x%X) %i",
				pEntity->GetName(), pEntity->GetClass()->GetName(),
				pEntity->GetId(), pEntity->GetGuid().ToDebugString(),
				pEntity->GetFlags(), pEntity->IsLoadedFromLevelFile());

			currentEntities.insert(pEntity->GetGuid());
		}

		// create update packet
		XmlNodeRef xmlRoot = gEnv->pSystem->CreateXmlNode("Entities");

		// parse the update packet
		const uint32 numEntities = reader.ReadUint32();
		for (uint32 i = 0; i < numEntities; ++i)
		{
			// load entity ID
			const EntityGUID entityGUID = CryGUID::FromString(reader.ReadString().c_str());

			// get existing entity with that ID
			TEntitySet::iterator it = currentEntities.find(entityGUID);
			if (it != currentEntities.end())
			{
				currentEntities.erase(it);
			}

			// find entity in the current file
			EntityId entityId = FindEntityByGuid(entityGUID);
			if (CEntity* pEntity = GetEntityFromID(entityId))
			{
				// this is not an entity loaded from LevelFile, we cant override it!
				if (!pEntity->IsLoadedFromLevelFile())
				{
					// entity sill there :)
					gEnv->pLog->LogAlways("Could not override entity '%s' '%s' (ID=%d, GUID=%s, Flags=0x%X). Original entity is non from Level.",
					                      pEntity->GetName(), pEntity->GetClass()->GetName(),
					                      pEntity->GetId(), pEntity->GetGuid().ToDebugString(), pEntity->GetFlags());

					reader.SkipString();
					continue;
				}

				// entity does already exist, delete it (we will be restoring a new version)
				pEntity->ClearFlags(ENTITY_FLAG_UNREMOVABLE);
				RemoveEntity(pEntity);
			}

			// load this entity
			const string xmlString = reader.ReadString();
			XmlNodeRef xml = gEnv->pSystem->LoadXmlFromBuffer(xmlString.c_str(), xmlString.length());
			if (xml != nullptr)
			{
				xmlRoot->addChild(xml);
			}
		}

		// delete entities that were not in the sync packet
		if (!currentEntities.empty())
		{
			for (TEntitySet::iterator it = currentEntities.begin();
			     it != currentEntities.end(); ++it)
			{
				const EntityGUID entityGUID = *it;

				const EntityId entityId = FindEntityByGuid(entityGUID);
				if (CEntity* pEntity = GetEntityFromID(entityId))
				{
					// we can't delete local player
					if (pEntity->CheckFlags(ENTITY_FLAG_LOCAL_PLAYER))
					{
						continue;
					}

					// we can't delete entities that were not loaded from file
					if (!pEntity->IsLoadedFromLevelFile())
					{
						continue;
					}

					// special case for GameRules...
					if (0 == strcmp(pEntity->GetName(), "GameRules"))
					{
						continue;
					}

					gEnv->pLog->LogAlways("Deleting entity: '%s' '%s' (ID=%d, GUID=%s, Flags=0x%X", pEntity->GetName(), pEntity->GetClass()->GetName(),
					                      pEntity->GetId(), pEntity->GetGuid().ToDebugString(), pEntity->GetFlags());

					// Remove the entity
					pEntity->ClearFlags(ENTITY_FLAG_UNREMOVABLE);
					RemoveEntity(pEntity);
				}
			}
		}

		// finish entity deletions
		DeletePendingEntities();

		// load new entities
		LoadEntities(xmlRoot, true); // when loading LiveCreate entities we assume that they come from level
	}
}

//////////////////////////////////////////////////////////////////////////
int CEntitySystem::GetLayerId(const char* szLayerName) const
{
	std::map<string, CEntityLayer*>::const_iterator found = m_layers.find(CONST_TEMP_STRING(szLayerName));
	if (found != m_layers.end())
	{
		return found->second->GetId();
	}
	else
	{
		return -1;
	}
}

//////////////////////////////////////////////////////////////////////////
const char* CEntitySystem::GetLayerName(int layerId) const
{
	std::map<string, CEntityLayer*>::const_iterator it = m_layers.cbegin();
	for (; it != m_layers.cend(); ++it)
	{
		if (it->second->GetId() == layerId)
			return it->first.c_str();
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
int CEntitySystem::GetLayerChildCount(const char* szLayerName) const
{
	std::map<string, CEntityLayer*>::const_iterator found = m_layers.find(CONST_TEMP_STRING(szLayerName));
	if (found != m_layers.end())
	{
		return found->second->GetNumChildren();
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
const char* CEntitySystem::GetLayerChild(const char* szLayerName, int childIdx) const
{
	std::map<string, CEntityLayer*>::const_iterator found = m_layers.find(CONST_TEMP_STRING(szLayerName));
	if (found != m_layers.end())
	{
		CEntityLayer* pChildLayer = found->second->GetChild(childIdx);
		if (pChildLayer != NULL)
			return pChildLayer->GetName();
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::ToggleLayerVisibility(const char* layer, bool isEnabled, bool includeParent)
{
	IEntityLayer* pLayer = FindLayer(layer);
	if (pLayer)
	{
		const bool bChange = pLayer->IsEnabled() != isEnabled;
		if (bChange)
		{
			// call the Enable() with a special set of parameters for faster operation (LiveCreate specific)
			pLayer->Enable(isEnabled, false, false);

			// load/unload layer paks
			IResourceManager* pResMan = gEnv->pSystem->GetIResourceManager();
			if (pResMan)
			{
				if (isEnabled)
				{
					if (includeParent && (strlen(pLayer->GetParentName()) > 0))
					{
						pResMan->LoadLayerPak(pLayer->GetParentName());
					}
					else
					{
						pResMan->LoadLayerPak(pLayer->GetName());
					}
				}
				else
				{
					if (includeParent && strlen(pLayer->GetParentName()) > 0)
					{
						pResMan->UnloadLayerPak(pLayer->GetParentName());
					}
					else
					{
						pResMan->UnloadLayerPak(pLayer->GetName());
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::ToggleLayersBySubstring(const char* pSearchSubstring, const char* pExceptionSubstring, bool isEnable)
{
	// This toggles visibility of all layers containing pSearchSubstring with the exception of any containing pExceptionSubstring.
	// This does not modify parent layers by passing false to ToggleLayerVisibility
	string exceptionSubstring = pExceptionSubstring;
	string disableSubstring = pSearchSubstring;

	exceptionSubstring.MakeLower();
	disableSubstring.MakeLower();

	for (TLayers::iterator it = m_layers.begin(); it != m_layers.end(); ++it)
	{
		CEntityLayer* pLayer = it->second;
		string pLayerName = pLayer->GetName();
		pLayerName.MakeLower();

		if (strstr(pLayerName, disableSubstring) != NULL)
		{
			if (exceptionSubstring.empty() || strstr(pLayerName, exceptionSubstring) == NULL)
			{
				ToggleLayerVisibility(pLayer->GetName(), isEnable, false);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
int CEntitySystem::GetVisibleLayerIDs(uint8* pLayerMask, const uint32 maxCount) const
{
	memset(pLayerMask, 0, maxCount / 8);

	int maxLayerId = 0;
	for (std::map<string, CEntityLayer*>::const_iterator it = m_layers.begin();
	     it != m_layers.end(); ++it)
	{
		CEntityLayer* pLayer = it->second;
		if (pLayer->IsEnabled())
		{
			const int id = pLayer->GetId();
			if (id < (int)maxCount)
			{
				pLayerMask[id >> 3] |= (1 << (id & 7));
			}

			if (id > maxLayerId)
			{
				maxLayerId = id;
			}
		}
	}

	return maxLayerId;
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::DumpEntity(CEntity* pEntity)
{
	static struct SFlagData
	{
		unsigned mask;
		const char* name;
	} flagData[] = {
		{ ENTITY_FLAG_CLIENT_ONLY,         "Client"      },
		{ ENTITY_FLAG_SERVER_ONLY,         "Server"      },
		{ ENTITY_FLAG_UNREMOVABLE,         "NoRemove"    },
		{ ENTITY_FLAG_CLIENTSIDE_STATE,    "ClientState" },
		{ ENTITY_FLAG_MODIFIED_BY_PHYSICS, "PhysMod"     },
	};

	string name(pEntity->GetName());
	name += string("[$9") + pEntity->GetClass()->GetName() + string("$o]");
#if !defined(EXCLUDE_NORMAL_LOG)
	Vec3 pos(pEntity->GetWorldPos());
#endif
	const char* sStatus = pEntity->IsActivatedForUpdates() ? "[$3Active$o]" : "[$9Inactive$o]";
	if (pEntity->IsHidden())
		sStatus = "[$9Hidden$o]";

	std::set<string> allFlags;
	for (size_t i = 0; i < sizeof(flagData) / sizeof(*flagData); ++i)
	{
		if ((flagData[i].mask & pEntity->GetFlags()) == flagData[i].mask)
			allFlags.insert(flagData[i].name);
	}
	string flags;
	for (std::set<string>::iterator iter = allFlags.begin(); iter != allFlags.end(); ++iter)
	{
		flags += ' ';
		flags += *iter;
	}

	CryLogAlways("%5u: Salt:%u  %-42s  %-14s  pos: (%.2f %.2f %.2f) %u %s",
	             pEntity->GetId() & 0xffff, pEntity->GetId() >> 16,
	             name.c_str(), sStatus, pos.x, pos.y, pos.z, pEntity->GetId(), flags.c_str());
}

void CEntitySystem::DeletePendingEntities()
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	while (!m_deletedEntities.empty())
	{
		CEntity* pEntity = m_deletedEntities.back();
		m_deletedEntities.pop_back();
		DeleteEntity(pEntity);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::ChangeEntityName(CEntity* pEntity, const char* sNewName)
{
	if (!pEntity->m_name.empty())
	{
		// Remove old name.
		EntityNamesMap::iterator it = m_mapEntityNames.lower_bound(pEntity->m_name.c_str());
		for (; it != m_mapEntityNames.end(); ++it)
		{
			if (it->second == pEntity->m_id)
			{
				m_mapEntityNames.erase(it);
				break;
			}
			if (stricmp(it->first, pEntity->m_name.c_str()) != 0)
				break;
		}
	}

	if (sNewName[0] != 0)
	{
		pEntity->m_name = sNewName;
		// Insert new name into the map.
		m_mapEntityNames.insert(EntityNamesMap::value_type(pEntity->m_name.c_str(), pEntity->m_id));
	}
	else
	{
		pEntity->m_name = sNewName;
	}

	SEntityEvent event(ENTITY_EVENT_SET_NAME);
	pEntity->SendEvent(event);
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::ClearLayers()
{
	for (std::map<string, CEntityLayer*>::iterator it = m_layers.begin(); it != m_layers.end(); ++it)
	{
		CEntityLayer* pLayer = it->second;
		delete pLayer;
	}
	m_layers.clear();

#ifdef ENABLE_PROFILING_CODE
	g_pIEntitySystem->m_layerProfiles.clear();
#endif //ENABLE_PROFILING_CODE
}

//////////////////////////////////////////////////////////////////////////
IEntityLayer* CEntitySystem::AddLayer(const char* szName, const char* szParent,
                                      uint16 id, bool bHasPhysics, int specs, bool bDefaultLoaded)
{
	CEntityLayer* pLayer = new CEntityLayer(szName, id, bHasPhysics, specs, bDefaultLoaded, m_garbageLayerHeaps);
	if (szParent && *szParent)
	{
		pLayer->SetParentName(szParent);
	}
	m_layers[szName] = pLayer;
	return pLayer;
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::LinkLayerChildren()
{
	// make sure parent - child relation is valid
	for (TLayers::iterator it = m_layers.begin(); it != m_layers.end(); ++it)
	{
		CEntityLayer* pLayer = it->second;
		if (strlen(pLayer->GetParentName()) == 0)
			continue;

		IEntityLayer* pParent = FindLayer(pLayer->GetParentName());
		if (pParent)
			pParent->AddChild(pLayer);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::LoadLayers(const char* dataFile)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	ClearLayers();
	XmlNodeRef root = GetISystem()->LoadXmlFromFile(dataFile);
	if (root)
	{
		XmlNodeRef layersNode = root->findChild("Layers");
		if (layersNode)
		{
			bool bClearSkippedLayers = true;
			for (int i = 0; i < layersNode->getChildCount(); i++)
			{
				XmlNodeRef layerNode = layersNode->getChild(i);
				if (layerNode->isTag("Layer"))
				{
					string name = layerNode->getAttr("Name");
					string parent = layerNode->getAttr("Parent");
					uint16 id;
					layerNode->getAttr("Id", id);
					int nHavePhysics = 1;
					layerNode->getAttr("Physics", nHavePhysics);
					int specs = eSpecType_All;
					layerNode->getAttr("Specs", specs);
					int nDefaultLayer = 0;
					layerNode->getAttr("DefaultLoaded", nDefaultLayer);
					IEntityLayer* pLayer = AddLayer(name, parent, id, nHavePhysics ? true : false, specs, nDefaultLayer ? true : false);

					if (pLayer->IsSkippedBySpec())
					{
						gEnv->p3DEngine->SkipLayerLoading(id, bClearSkippedLayers);
						bClearSkippedLayers = false;
					}
				}
			}
		}
	}
	LinkLayerChildren();
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::AddEntityToLayer(const char* layer, EntityId id)
{
	IEntityLayer* pLayer = FindLayer(layer);
	if (pLayer)
		pLayer->AddObject(id);
}

///////////////////////////////////////// /////////////////////////////////
void CEntitySystem::RemoveEntityFromLayers(EntityId id)
{
	for (TLayers::iterator it = m_layers.begin(); it != m_layers.end(); ++it)
	{
		(it->second)->RemoveObject(id);
	}
}

//////////////////////////////////////////////////////////////////////////
CEntityLayer* CEntitySystem::GetLayerForEntity(EntityId id)
{
	TLayers::const_iterator it = m_layers.begin();
	TLayers::const_iterator end = m_layers.end();
	for (; it != end; ++it)
	{
		if (it->second->IncludesEntity(id))
			return it->second;
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::EnableDefaultLayers(bool isSerialized)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	if (!gEnv->p3DEngine->IsAreaActivationInUse())
		return;
	
	for (TLayers::iterator it = m_layers.begin(); it != m_layers.end(); ++it)
	{
		CEntityLayer* pLayer = it->second;
		EnableLayer(pLayer, pLayer->IsDefaultLoaded(), isSerialized, true);
	}
}

void CEntitySystem::EnableLayer(const char* szLayer, bool isEnable, bool isSerialized)
{
	CRY_PROFILE_FUNCTION_ARG(PROFILE_LOADING_ONLY, szLayer);
	if (!gEnv->p3DEngine->IsAreaActivationInUse())
		return;
	IEntityLayer* pLayer = FindLayer(szLayer);
	if (pLayer)
	{
		EnableLayer(pLayer, isEnable, isSerialized, true);
	}
}

void CEntitySystem::EnableLayer(IEntityLayer* pLayer, bool bIsEnable, bool bIsSerialized, bool bAffectsChildren)
{
	CRY_PROFILE_FUNCTION_ARG(PROFILE_LOADING_ONLY, pLayer->GetName());
	const bool bEnableChange = pLayer->IsEnabledBrush() != bIsEnable;

#if ENABLE_STATOSCOPE
	if (gEnv->pStatoscope && (pLayer->IsEnabled() != bIsEnable))
	{
		stack_string userMarker = "LayerSwitching: ";
		userMarker += bIsEnable ? "Enable " : "Disable ";
		userMarker += pLayer->GetName();
		gEnv->pStatoscope->AddUserMarker("LayerSwitching", userMarker.c_str());
	}
#endif

	pLayer->Enable(bIsEnable, bIsSerialized, bAffectsChildren);

	IResourceManager* pResMan = gEnv->pSystem->GetIResourceManager();
	if (bEnableChange && pResMan)
	{
		if (bIsEnable)
		{
			if (strlen(pLayer->GetParentName()) > 0)
				pResMan->LoadLayerPak(pLayer->GetParentName());
			else
				pResMan->LoadLayerPak(pLayer->GetName());
		}
		else
		{
			if (strlen(pLayer->GetParentName()) > 0)
				pResMan->UnloadLayerPak(pLayer->GetParentName());
			else
				pResMan->UnloadLayerPak(pLayer->GetName());
		}
	}
}

void CEntitySystem::EnableLayerSet(const char* const* pLayers, size_t layerCount, bool bIsSerialized, IEntityLayerSetUpdateListener* pListener)
{
	if (!gEnv->p3DEngine->IsAreaActivationInUse())
		return;

	const char* const* pVisibleLayersBegin = pLayers;
	const char* const* pVisibleLayersEnd = pLayers + layerCount;
	for (TLayers::iterator it = m_layers.begin(), itEnd = m_layers.end(); it != itEnd; ++it)
	{
		const string& searchedString = it->first;
		const bool bEnabled = std::find_if(pVisibleLayersBegin, pVisibleLayersEnd, [&searchedString](const char* szLayerName)
		{
			return searchedString == szLayerName;
		}) != pVisibleLayersEnd;

		CEntityLayer* pLayer = it->second;
		if (bEnabled != pLayer->IsEnabled())
		{
			EnableLayer(pLayer, bEnabled, bIsSerialized, false);
		}

		if (pListener)
		{
			pListener->LayerEnablingEvent(searchedString.c_str(), bEnabled, bIsSerialized);
		}
	}
}

void CEntitySystem::EnableScopedLayerSet(const char* const* pLayers, size_t layerCount, const char* const* pScopeLayers, size_t scopeLayerCount, bool isSerialized, IEntityLayerSetUpdateListener* pListener)
{
	if (!gEnv->p3DEngine->IsAreaActivationInUse())
		return;

	const char* const* const pVisibleLayersBegin = pLayers;
	const char* const* const pVisibleLayersEnd = pLayers + layerCount;

	const char* const* const pScopeLayersBegin = pScopeLayers;
	const char* const* const pScopeLayersEnd = pScopeLayers + scopeLayerCount;

	const TLayers::iterator layersEndIt = m_layers.end();

	for(const char* const* pScopeLayersIt = pScopeLayersBegin; pScopeLayersIt != pScopeLayersEnd; ++pScopeLayersIt)
	{
		const char* const scopeLayerName = *pScopeLayersIt;
		const bool shouldBeEnabled = std::find_if(pVisibleLayersBegin, pVisibleLayersEnd, [scopeLayerName](const char* szLayerName)
		{
			return 0 == strcmp(scopeLayerName, szLayerName);;
		}) != pVisibleLayersEnd;

		TLayers::iterator it = m_layers.find(CONST_TEMP_STRING(scopeLayerName));
		if(layersEndIt != it)
		{
			CEntityLayer* pLayer = it->second;
			if(shouldBeEnabled != pLayer->IsEnabled())
			{
				EnableLayer(pLayer, shouldBeEnabled, isSerialized, false);
			}

			if(pListener)
			{
				pListener->LayerEnablingEvent(scopeLayerName, shouldBeEnabled, isSerialized);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntitySystem::IsLayerEnabled(const char* layer, bool bMustBeLoaded, bool bCaseSensitive) const
{
	const IEntityLayer* pLayer = FindLayer(layer, bCaseSensitive);
	if (pLayer)
	{
		if (pLayer->IsEnabled())
		{
			return pLayer->IsSerialized() || !bMustBeLoaded;
		}
	}

	return false;
}

bool CEntitySystem::ShouldSerializedEntity(IEntity* pEntity)
{
	if (!pEntity)
		return false;

	//entity flag
	if (pEntity->GetFlags() & ENTITY_FLAG_NO_SAVE)
		return false;

	//lua flag
	if (CVar::es_SaveLoadUseLUANoSaveFlag != 0)
	{
		IScriptTable* pEntityScript = pEntity->GetScriptTable();
		SmartScriptTable props;
		if (pEntityScript && pEntityScript->GetValue("Properties", props))
		{
			bool bSerialize = true;
			if (props->GetValue("bSerialize", bSerialize) && (bSerialize == false))
				return false;
		}
	}

	// layer settings
	int iSerMode = CVar::es_LayerSaveLoadSerialization;
	if (iSerMode == 0)
		return true;

	CEntityLayer* pLayer = GetLayerForEntity(pEntity->GetId());
	if (!pLayer)
		return true;

	if (iSerMode == 1)
		return pLayer->IsEnabled();

	return pLayer->IsSerialized();
}

//////////////////////////////////////////////////////////////////////////
bool CEntitySystem::ExtractArcheTypeLoadParams(XmlNodeRef& entityNode, SEntitySpawnParams& spawnParams) const
{
	return m_pEntityLoadManager->ExtractArcheTypeLoadParams(entityNode, spawnParams);
}

//////////////////////////////////////////////////////////////////////////
bool CEntitySystem::ExtractEntityLoadParams(XmlNodeRef& entityNode, SEntitySpawnParams& spawnParams) const
{
	return(m_pEntityLoadManager->ExtractEntityLoadParams(entityNode, spawnParams));
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::BeginCreateEntities(int nAmtToCreate)
{
	m_pEntityLoadManager->PrepareBatchCreation(nAmtToCreate);
}

//////////////////////////////////////////////////////////////////////////
bool CEntitySystem::CreateEntity(XmlNodeRef& entityNode, SEntitySpawnParams& pParams, EntityId& outUsingId)
{
	return(m_pEntityLoadManager->CreateEntity(entityNode, pParams, outUsingId));
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::EndCreateEntities()
{
	m_pEntityLoadManager->OnBatchCreationCompleted();
}

#ifndef PURE_CLIENT
CEntitySystem::StaticEntityNetworkIdentifier CEntitySystem::GetStaticEntityNetworkId(const EntityId id) const
{
	CRY_ASSERT_MESSAGE(gEnv->bServer, "Static entity network identifier should only be queried by the server!");

	// Look up index for the specified static (loaded from level) entity in the vector
	// The entry has to be contained here, or it shows a severe issue in the entity system
	auto it = std::find(m_staticEntityIds.begin(), m_staticEntityIds.end(), id);
	CRY_ASSERT_MESSAGE(it != m_staticEntityIds.end(), "Static entity was not present in the static entity vector! This indicates a severe system error!");

	return static_cast<StaticEntityNetworkIdentifier>(std::distance(m_staticEntityIds.begin(), it));
}
#endif

EntityId CEntitySystem::GetEntityIdFromStaticEntityNetworkId(const StaticEntityNetworkIdentifier id) const
{
	CRY_ASSERT_MESSAGE(gEnv->IsClient(), "Retrieving entity id from static entity network identifier should only be done by the client!");
	CRY_ASSERT_MESSAGE(id < m_staticEntityIds.size(), "Static entity identifier exceeded range! Probable level mismatch detected!");
	if (id < m_staticEntityIds.size())
	{
		return m_staticEntityIds[id];
	}

	// Note: If this is triggered, chances are we need to find a better way of detecting level mismatches and disconnecting + alerting player early.
	EntityWarning("Attempted to access static entity id from network with out of range index! This is a sign of a probable level mismatch between server and client.");
	return INVALID_ENTITYID;
}

void CEntitySystem::AddStaticEntityId(const EntityId id, StaticEntityNetworkIdentifier networkIdentifier)
{
	CRY_ASSERT_MESSAGE(m_staticEntityIds.empty() || m_staticEntityIds.back() < id || id == INVALID_ENTITYID, "Static entity identifiers must be added sequentially!");
	CRY_ASSERT(networkIdentifier < m_staticEntityIds.size());

	// Static entity identifiers start at index 1, 0 being game rules
	m_staticEntityIds[networkIdentifier + 1] = id;
}

void CEntitySystem::PurgeDeferredCollisionEvents(bool force)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	for (std::vector<CEntity*>::iterator it = m_deferredUsedEntities.begin(); it != m_deferredUsedEntities.end();)
	{
		CEntity* pEntity = *it;

		if (pEntity->m_keepAliveCounter == 0 || force)
		{
			CRY_ASSERT_MESSAGE(pEntity->IsGarbage(), "Entity must have been marked as removed to be deferred deleted!");
			stl::push_back_unique(m_deletedEntities, pEntity);
			it = m_deferredUsedEntities.erase(it);
		}
		else
		{
			++it;
		}
	}
}

IBSPTree3D* CEntitySystem::CreateBSPTree3D(const IBSPTree3D::FaceList& faceList)
{
	return new CBSPTree3D(faceList);
}

void CEntitySystem::ReleaseBSPTree3D(IBSPTree3D*& pTree)
{
	SAFE_DELETE(pTree);
}

void CEntitySystem::EnableComponentUpdates(IEntityComponent* pComponent, bool bEnable)
{
	CEntity* pEntity = static_cast<CEntity*>(pComponent->GetEntity());

	if (bEnable)
	{
		SMinimalEntityComponentRecord record;
		record.pComponent = pComponent;

		m_updatedEntityComponents.SortedEmplace(std::move(record));

		if (!pEntity->HasInternalFlag(CEntity::EInternalFlag::InActiveList))
		{
			pEntity->SetInternalFlag(CEntity::EInternalFlag::InActiveList, true);
			SEntityEvent event(ENTITY_EVENT_ACTIVATED);
			pEntity->SendEvent(event);
		}
	}
	else
	{
		// Invalid the component entry so it can be cleaned up next iteration.
		m_updatedEntityComponents.Remove(pComponent);

		bool bRequiresUpdate = false;

		// Check if any other components need the update event
		pEntity->m_components.NonRecursiveForEach([pComponent, &bRequiresUpdate](const SEntityComponentRecord& otherComponentRecord) -> EComponentIterationResult
		{
			if (otherComponentRecord.pComponent.get() != pComponent && otherComponentRecord.registeredEventsMask.Check(ENTITY_EVENT_UPDATE))
			{
				bRequiresUpdate = true;
				return EComponentIterationResult::Break;
			}

			return EComponentIterationResult::Continue;
		});

		if (!bRequiresUpdate)
		{
			pEntity->SetInternalFlag(CEntity::EInternalFlag::InActiveList, false);
			SEntityEvent event(ENTITY_EVENT_DEACTIVATED);
			pEntity->SendEvent(event);
		}
	}
}

void CEntitySystem::EnableComponentPrePhysicsUpdates(IEntityComponent* pComponent, bool bEnable)
{
	if (bEnable)
	{
		SMinimalEntityComponentRecord record;
		record.pComponent = pComponent;

		m_prePhysicsUpdatedEntityComponents.SortedEmplace(std::move(record));
	}
	else
	{
		m_prePhysicsUpdatedEntityComponents.Remove(pComponent);
	}
}
