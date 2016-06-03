// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AIObjectManager.h"

//TODO should be removed so no knowledge of these are here:
#include "Puppet.h"
#include "AIVehicle.h"
#include "AIFlyingVehicle.h"
#include "AIPlayer.h"
#include "AIObjectIterators.h"//TODO get rid of this file totally!
#include "PerceptionManager.h"
#include "ObjectContainer.h"
#include "./TargetSelection/TargetTrackManager.h"

//////////////////////////////////////////////////////////////////////////
//	SAIObjectCreationHelper - helper for serializing AI objects
//	(used for both normal and bookmark serialization)
//////////////////////////////////////////////////////////////////////////
SAIObjectCreationHelper::SAIObjectCreationHelper(CAIObject* pObject)
{
	if (pObject)
	{
		name = pObject->GetName();
		objectId = pObject->GetAIObjectID();

		// Working from child to parent through the hierarchy is essential here - each object can be many types
		// This could all be much neater with a different type system to fastcast.
		if (pObject->CastToCAIVehicle())  aiClass = eAIC_AIVehicle;
		else if (pObject->CastToCAIFlyingVehicle())
			aiClass = eAIC_AIFlyingVehicle;
		else if (pObject->CastToCPuppet())
			aiClass = eAIC_Puppet;
		else if (pObject->CastToCPipeUser())
			aiClass = eAIC_PipeUser;
		else if (pObject->CastToCAIPlayer())
			aiClass = eAIC_AIPlayer;
		else if (pObject->CastToCLeader())
			aiClass = eAIC_Leader;
		else if (pObject->CastToCAIActor())
			aiClass = eAIC_AIActor;
		else /* CAIObject */ aiClass = eAIC_AIObject;
	}
	else
	{
		objectId = INVALID_AIOBJECTID;
		name = "Unset";
		aiClass = eAIC_Invalid;
	}
}

void SAIObjectCreationHelper::Serialize(TSerialize ser)
{
	ser.BeginGroup("ObjectHeader");
	ser.Value("name", name);  // debug mainly, could be removed
	ser.EnumValue("class", aiClass, eAIC_FIRST, eAIC_LAST);
	ser.Value("objectId", objectId);
	ser.EndGroup();
}

CAIObject* SAIObjectCreationHelper::RecreateObject(void* pAlloc /*=NULL*/)
{
	// skip unset ones (eg from a nil CStrongRef)
	if (objectId == INVALID_AIOBJECTID && aiClass == eAIC_Invalid)
		return NULL;

	// first verify it doesn't already exist
	// cppcheck-suppress assertWithSideEffect
	assert(gAIEnv.pAIObjectManager->GetAIObject(objectId) == NULL);

	CAIObject* pObject = NULL;
	switch (aiClass)
	{
	case eAIC_AIVehicle:
		pObject = pAlloc ? new(pAlloc) CAIVehicle : new CAIVehicle;
		break;
	case eAIC_Puppet:
		pObject = pAlloc ? new(pAlloc) CPuppet : new CPuppet;
		break;
	case eAIC_PipeUser:
		pObject = pAlloc ? new(pAlloc) CPipeUser : new CPipeUser;
		break;
	case eAIC_AIPlayer:
		pObject = pAlloc ? new(pAlloc) CAIPlayer : new CAIPlayer;
		break;
	case eAIC_Leader:
		pObject = pAlloc ? new(pAlloc) CLeader(0) : new CLeader(0);
		break;                                                                                // Groupid is reqd
	case eAIC_AIActor:
		pObject = pAlloc ? new(pAlloc) CAIActor : new CAIActor;
		break;
	case eAIC_AIObject:
		pObject = pAlloc ? new(pAlloc) CAIObject : new CAIObject;
		break;
	case eAIC_AIFlyingVehicle:
		pObject = pAlloc ? new(pAlloc) CAIFlyingVehicle : new CAIFlyingVehicle;
		break;
	default:
		assert(false);
	}

	return pObject;
}

//////////////////////////////////////////////////////////////////////////
//	CAIObjectManager
//////////////////////////////////////////////////////////////////////////

CAIObjectManager::CAIObjectManager()
	: m_pPoolAllocator(NULL)
	, m_serializingBookmark(false)
	, m_PoolBucketSize(0)
{
}

CAIObjectManager::~CAIObjectManager()
{
	SAFE_DELETE(m_pPoolAllocator);

	m_mapDummyObjects.clear();

	IEntityPoolManager* pPM = gEnv->pEntitySystem->GetIEntityPoolManager();
	if (pPM)
	{
		pPM->RemoveListener(this);
	}
}

void CAIObjectManager::Init()
{
	IEntityPoolManager* pPM = gEnv->pEntitySystem->GetIEntityPoolManager();
	if (pPM)
	{
		pPM->AddListener(this, "CAIObjectManager", EntityReturnedToPool | PoolDefinitionsLoaded | BookmarkEntitySerialize);
	}
}

void CAIObjectManager::Reset(bool includingPooled /*=true*/)
{
	m_mapDummyObjects.clear();

	if (includingPooled)
	{
		m_Objects.clear();
		stl::free_container(m_pooledObjects);
		if (m_pPoolAllocator)
		{
			gAIEnv.pObjectContainer->ReleaseDeregisteredObjects(false);
			m_pPoolAllocator->FreeMemoryIfEmpty();
			assert(m_pPoolAllocator->GetTotalMemory().nAlloc == 0);
		}
	}
	else
	{
		// remove all objects unless they are in the pooled list. This happens at the start of AI serialization (loading),
		//	when pooled objects have already been serialized by the entity pool manager. We need to leave those objects in
		//	the list since they won't be created/registered again.
		m_Objects.clear();

		for (TPooledAIObjectMap::iterator it = m_pooledObjects.begin(), end = m_pooledObjects.end(); it != end; ++it)
		{
			m_Objects.insert(AIObjectOwners::iterator::value_type(it->second->GetAIType(), it->second));
		}
	}
}

IAIObject* CAIObjectManager::CreateAIObject(const AIObjectParams& params)
{
	CCCPOINT(CreateAIObject);

	if (!GetAISystem()->IsEnabled())
		return 0;

	if (!GetAISystem()->m_bInitialized)
	{
		AIError("CAISystem::CreateAIObject called on an uninitialized AI system [Code bug]");
		return 0;
	}

	CAIObject* pObject = NULL;
	CLeader* pLeader = NULL;

	tAIObjectID idToUse = INVALID_AIOBJECTID;

	// first figure out if this AI object should be created from the pool,
	//	and (if so) attempt to get the object ID from there. This avoids creating a
	//	temporary object here, then a new one with the correct ID when the
	//	serialize-from-bookmark happens.
	bool pooled = false;
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(params.entityID);
	const bool isPooledEntity = pEntity ? pEntity->IsFromPool() : false;
	if (isPooledEntity)
	{
		pooled = true;

		IEntityPoolManager::SPreparingParams poolParams;
		if (gEnv->pEntitySystem->GetIEntityPoolManager()->IsPreparingEntity(poolParams))
		{
			assert(poolParams.entityId == params.entityID);
			idToUse = poolParams.aiObjectId;
		}
		else
		{
			// something wrong in the pool manager
			assert(false);
		}
	}

	uint16 type = params.type;

	void* pAlloc = NULL;
	if (pooled)
	{
		// allocate the object memory
		assert(m_pPoolAllocator);
		pAlloc = m_pPoolAllocator->Allocate();
	}

	switch (type)
	{
	case AIOBJECT_DUMMY:
		CRY_ASSERT_MESSAGE(false, "Creating dummy object through the AI object manager (use CAISystem::CreateDummyObject instead)");
		return 0;
	case AIOBJECT_ACTOR:
		type = AIOBJECT_ACTOR;
		pObject = pooled ? new(pAlloc) CPuppet : new CPuppet;
		break;
	case AIOBJECT_2D_FLY:
		type = AIOBJECT_ACTOR;
		pObject = pooled ? new(pAlloc) CPuppet : new CPuppet;
		pObject->SetSubType(CAIObject::STP_2D_FLY);
		break;
	case AIOBJECT_LEADER:
		{
			int iGroup = params.association ? static_cast<CPuppet*>(params.association)->GetGroupId() : -1;
			pLeader = pooled ? new(pAlloc) CLeader(iGroup) : new CLeader(iGroup);
			pObject = pLeader;
		}
		break;
	case AIOBJECT_BOAT:
		type = AIOBJECT_VEHICLE;
		pObject = pooled ? new(pAlloc) CAIVehicle : new CAIVehicle;
		pObject->SetSubType(CAIObject::STP_BOAT);
		//				pObject->m_bNeedsPathOutdoor = false;
		break;
	case AIOBJECT_CAR:
		type = AIOBJECT_VEHICLE;
		pObject = pooled ? new(pAlloc) CAIVehicle : new CAIVehicle;
		pObject->SetSubType(CAIObject::STP_CAR);
		break;
	case AIOBJECT_HELICOPTER:
		type = AIOBJECT_VEHICLE;
		pObject = pooled ? new(pAlloc) CAIVehicle : new CAIVehicle;
		pObject->SetSubType(CAIObject::STP_HELI);
		break;
	case AIOBJECT_INFECTED:
		type = AIOBJECT_ACTOR;
		pObject = pooled ? new(pAlloc) CAIActor : new CAIActor;
		break;
	case AIOBJECT_PLAYER:
		pObject = pooled ? new(pAlloc) CAIPlayer : new CAIPlayer; // just a dummy for the player
		break;
	case AIOBJECT_RPG:
	case AIOBJECT_GRENADE:
		pObject = pooled ? new(pAlloc) CAIObject : new CAIObject;
		break;
	case AIOBJECT_WAYPOINT:
	case AIOBJECT_HIDEPOINT:
	case AIOBJECT_SNDSUPRESSOR:
	case AIOBJECT_NAV_SEED:
		pObject = pooled ? new(pAlloc) CAIObject : new CAIObject;
		break;
	case AIOBJECT_ALIENTICK:
		type = AIOBJECT_ACTOR;
		pObject = pooled ? new(pAlloc) CPipeUser : new CPipeUser;
		break;
	case AIOBJECT_HELICOPTERCRYSIS2:
		type = AIOBJECT_ACTOR;
		pObject = pooled ? new(pAlloc) CAIFlyingVehicle : new CAIFlyingVehicle;
		pObject->SetSubType(CAIObject::STP_HELICRYSIS2);
		break;
	default:
		// try to create an object of user defined type
		pObject = pooled ? new(pAlloc) CAIObject : new CAIObject;
		break;
	}

	assert(pObject);
	assert(!pooled || (pAlloc == pObject));

	// Register the object
	CStrongRef<CAIObject> object;
	gAIEnv.pObjectContainer->RegisterObject(pObject, object, idToUse);

	CCountedRef<CAIObject> countedRef = object;

	if (pooled)
	{
		// store the details
		pObject->m_createdFromPool = true;
		m_pooledObjects[params.entityID] = countedRef;
	}

	// insert object into map under key type
	// this is a multimap
	m_Objects.insert(AIObjectOwners::iterator::value_type(type, countedRef));

	// Reset the object after registration, so other systems can reference back to it if needed
	pObject->SetType(type);
	pObject->SetEntityID(params.entityID);
	pObject->SetName(params.name);
	pObject->SetAssociation(GetWeakRef((CAIObject*)params.association));

	if (pEntity)
	{
		pObject->SetPos(pEntity->GetWorldPos());
	}

	if (CAIActor* actor = pObject->CastToCAIActor())
		actor->ParseParameters(params);

	// Non-puppets and players need to be updated at least once for delayed initialization (Reset sets this to false!)
	if (type != AIOBJECT_PLAYER && type != AIOBJECT_ACTOR)  //&& type != AIOBJECT_VEHICLE )
		pObject->m_bUpdatedOnce = true;

	if ((type == AIOBJECT_LEADER) && params.association && pLeader)
		GetAISystem()->AddToGroup(pLeader);

	pObject->Reset(AIOBJRESET_INIT);

	AILogComment("CAISystem::CreateAIObject %p of type %d", pObject, type);

	if (type == AIOBJECT_PLAYER)
		pObject->Event(AIEVENT_ENABLE, NULL);

	return pObject;
}

void CAIObjectManager::CreateDummyObject(CCountedRef<CAIObject>& ref, const char* name, CAIObject::ESubType type, tAIObjectID requiredID /*= INVALID_AIOBJECTID*/)
{
	CStrongRef<CAIObject> refTemp;
	CreateDummyObject(refTemp, name, type, requiredID);
	ref = refTemp;
}

void CAIObjectManager::CreateDummyObject(CStrongRef<CAIObject>& ref, const char* name /*=""*/, CAIObject::ESubType type /*= CAIObject::STP_NONE*/, tAIObjectID requiredID /*= INVALID_AIOBJECTID*/)
{
	CCCPOINT(CreateDummyObject);

	CAIObject* pObject = new CAIObject;
	gAIEnv.pObjectContainer->RegisterObject(pObject, ref, requiredID);

	pObject->SetType(AIOBJECT_DUMMY);
	pObject->SetSubType(type);

	pObject->SetAssociation(NILREF);
	if (name && *name)
		pObject->SetName(name);

	// check whether it was added before
	AIObjects::iterator itEntry = m_mapDummyObjects.find(type);
	while ((itEntry != m_mapDummyObjects.end()) && (itEntry->first == type))
	{
		if (itEntry->second == ref)
			return;
		++itEntry;
	}

	// make sure it is not in with another types already
	itEntry = m_mapDummyObjects.begin();
	for (; itEntry != m_mapDummyObjects.end(); ++itEntry)
	{
		if (itEntry->second == ref)
		{
			m_mapDummyObjects.erase(itEntry);
			break;
		}
	}

	// insert object into map under key type
	m_mapDummyObjects.insert(AIObjects::iterator::value_type(type, ref));

	AILogComment("CAIObjectManager::CreateDummyObject %s (%p)", pObject->GetName(), pObject);
}

void CAIObjectManager::RemoveObject(tAIObjectID objectID)
{
	EntityId entityId = 0;

	// Find the element in the owners list and erase it from there
	// This is strong, so will trigger removal/deregister/release in normal fashion
	// (MATT) Really not very efficient because the multimaps aren't very suitable for this.
	// I think a different container might be better as primary owner. {2009/05/22}
	AIObjectOwners::iterator it = m_Objects.begin();
	AIObjectOwners::iterator itEnd = m_Objects.end();
	for (; it != itEnd; ++it)
	{
		if (it->second.GetObjectID() == objectID)
		{
			entityId = it->second->GetEntityID();
			break;
		}
	}

	// Check we found one
	if (it == itEnd)
	{
		AIError("AI system asked to erase AI object with unknown AIObjectID");
		assert(false);
		return;
	}

	m_Objects.erase(it);

	// also remove from the pooled objects list
	if (entityId != 0)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);

		// Note: multiple AI objects may refer to the same entity
		//	(eg Group target dummy objects); in that case only remove this object
		//	from the pooled objects list if it is actually the main
		//	AI object associated with the entity.
		if ((pEntity != NULL) && (pEntity->GetAIObjectID() == objectID))
		{
			stl::member_find_and_erase(m_pooledObjects, entityId);
		}
	}

	// Because Action doesn't yet handle a delayed removal of the Proxies, we should perform cleanup immediately.
	// Note that this only happens when triggered externally, when an entity is refreshed/removed
	gAIEnv.pObjectContainer->ReleaseDeregisteredObjects(false);
}

// Get an AI object by it's AI object ID
IAIObject* CAIObjectManager::GetAIObject(tAIObjectID aiObjectID)
{
	return gAIEnv.pObjectContainer->GetAIObject(aiObjectID);
}

IAIObject* CAIObjectManager::GetAIObjectByName(unsigned short type, const char* pName) const
{
	AIObjectOwners::const_iterator ai;

	if (m_Objects.empty()) return 0;

	if (!type)
		return (IAIObject*) GetAIObjectByName(pName);

	if ((ai = m_Objects.find(type)) != m_Objects.end())
	{
		for (; ai != m_Objects.end(); )
		{
			if (ai->first != type)
				break;
			CAIObject* pObject = ai->second.GetAIObject();

			if (strcmp(pName, pObject->GetName()) == 0)
				return (IAIObject*) pObject;
			++ai;
		}
	}
	return 0;

}

CAIObject* CAIObjectManager::GetAIObjectByName(const char* pName) const
{
	AIObjectOwners::const_iterator ai = m_Objects.begin();
	while (ai != m_Objects.end())
	{
		CAIObject* pObject = ai->second.GetAIObject();

		if ((pObject != NULL) && (strcmp(pName, pObject->GetName()) == 0))
			return pObject;
		++ai;
	}

	// Try dummy object map as well
	AIObjects::const_iterator aiDummy = m_mapDummyObjects.begin();
	while (aiDummy != m_mapDummyObjects.end())
	{
		CAIObject* pObject = aiDummy->second.GetAIObject();

		if ((pObject != NULL) && (strcmp(pName, pObject->GetName()) == 0))
			return pObject;
		++aiDummy;
	}

	return 0;
}

IAIObjectIter* CAIObjectManager::GetFirstAIObject(EGetFirstFilter filter, short n)
{
	if (filter == OBJFILTER_GROUP)
	{
		return SAIObjectMapIterOfType<CWeakRef>::Allocate(n, GetAISystem()->m_mapGroups.find(n), GetAISystem()->m_mapGroups.end());
	}
	else if (filter == OBJFILTER_FACTION)
	{
		return SAIObjectMapIterOfType<CWeakRef>::Allocate(n, GetAISystem()->m_mapFaction.find(n), GetAISystem()->m_mapFaction.end());
	}
	else if (filter == OBJFILTER_DUMMYOBJECTS)
	{
		return SAIObjectMapIterOfType<CWeakRef>::Allocate(n, m_mapDummyObjects.find(n), m_mapDummyObjects.end());
	}
	else
	{
		if (n == 0)
			return SAIObjectMapIter<CCountedRef>::Allocate(m_Objects.begin(), m_Objects.end());
		else
			return SAIObjectMapIterOfType<CCountedRef>::Allocate(n, m_Objects.find(n), m_Objects.end());
	}
}

IAIObjectIter* CAIObjectManager::GetFirstAIObjectInRange(EGetFirstFilter filter, short n, const Vec3& pos, float rad, bool check2D)
{
	if (filter == OBJFILTER_GROUP)
	{
		return SAIObjectMapIterOfTypeInRange<CWeakRef>::Allocate(n, GetAISystem()->m_mapGroups.find(n), GetAISystem()->m_mapGroups.end(), pos, rad, check2D);
	}
	else if (filter == OBJFILTER_FACTION)
	{
		return SAIObjectMapIterOfTypeInRange<CWeakRef>::Allocate(n, GetAISystem()->m_mapFaction.find(n), GetAISystem()->m_mapFaction.end(), pos, rad, check2D);
	}
	else if (filter == OBJFILTER_DUMMYOBJECTS)
	{
		return SAIObjectMapIterOfTypeInRange<CWeakRef>::Allocate(n, m_mapDummyObjects.find(n), m_mapDummyObjects.end(), pos, rad, check2D);
	}
	else //	if(filter == OBJFILTER_TYPE)
	{
		if (n == 0)
			return SAIObjectMapIterInRange<CCountedRef>::Allocate(m_Objects.begin(), m_Objects.end(), pos, rad, check2D);
		else
			return SAIObjectMapIterOfTypeInRange<CCountedRef>::Allocate(n, m_Objects.find(n), m_Objects.end(), pos, rad, check2D);
	}
}

void CAIObjectManager::OnObjectRemoved(CAIObject* pObject)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);
	CCCPOINT(OnObjectRemoved);

	if (!pObject)
		return;

	RemoveObjectFromAllOfType(AIOBJECT_ACTOR, pObject);
	RemoveObjectFromAllOfType(AIOBJECT_VEHICLE, pObject);
	RemoveObjectFromAllOfType(AIOBJECT_ATTRIBUTE, pObject);
	RemoveObjectFromAllOfType(AIOBJECT_LEADER, pObject);

	// (MATT) Remove from player - especially as attention target {2009/02/05}
	CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetAISystem()->GetPlayer());
	if (pPlayer)
		pPlayer->OnObjectRemoved(pObject);

	for (CAISystem::AIGroupMap::iterator it = GetAISystem()->m_mapAIGroups.begin(); it != GetAISystem()->m_mapAIGroups.end(); ++it)
		it->second->OnObjectRemoved(pObject);

	AIObjects::iterator oi;
	for (oi = GetAISystem()->m_mapFaction.begin(); oi != GetAISystem()->m_mapFaction.end(); ++oi)
	{
		if (oi->second == pObject)
		{
			GetAISystem()->m_mapFaction.erase(oi);
			break;
		}
	}

	for (oi = GetAISystem()->m_mapGroups.begin(); oi != GetAISystem()->m_mapGroups.end(); ++oi)
	{
		if (oi->second == pObject)
		{
			GetAISystem()->m_mapGroups.erase(oi);
			break;
		}
	}

	for (oi = m_mapDummyObjects.begin(); oi != m_mapDummyObjects.end(); ++oi)
	{
		if (oi->second == pObject)
		{
			m_mapDummyObjects.erase(oi);
			break;
		}
	}

	CLeader* pLeader = pObject->CastToCLeader();
	if (pLeader)
	{
		if (CAIGroup* pAIGroup = pLeader->GetAIGroup())
		{
			pAIGroup->SetLeader(0);
		}
	}
	CAISystem::FormationMap::iterator fi;
	for (fi = GetAISystem()->m_mapActiveFormations.begin(); fi != GetAISystem()->m_mapActiveFormations.end(); ++fi)
		fi->second->OnObjectRemoved(pObject);

	//remove this object from any pending paths generated for him
	CAIActor* pActor = pObject->CastToCAIActor();
	if (pActor)
	{
		pActor->CancelRequestedPath(true);
	}

	// (MATT) Try to do implicitly {2009/02/05}
	/*
	   // check if this object owned any beacons and remove them if so
	   if (!m_mapBeacons.empty())
	   {
	   BeaconMap::iterator bi,biend = m_mapBeacons.end();
	   for (bi=m_mapBeacons.begin();bi!=biend;)
	   {
	    if ((bi->second).pOwner == pObject)
	    {
	      BeaconMap::iterator eraseme = bi;
	   ++bi;
	      RemoveObject((eraseme->second).pBeacon);
	      m_mapBeacons.erase(bi);
	    }
	    else
	   ++bi;
	   }
	   }
	 */

	if (gAIEnv.pTargetTrackManager)
		gAIEnv.pTargetTrackManager->OnObjectRemoved(pObject);

	CPuppet* pPuppet = pObject->CastToCPuppet();
	if (pPuppet)
	{
		for (unsigned i = 0; i < GetAISystem()->m_delayedExpAccessoryUpdates.size(); )
		{
			if (GetAISystem()->m_delayedExpAccessoryUpdates[i].pPuppet = pPuppet)
			{
				GetAISystem()->m_delayedExpAccessoryUpdates[i] = GetAISystem()->m_delayedExpAccessoryUpdates.back();
				GetAISystem()->m_delayedExpAccessoryUpdates.pop_back();
			}
			else
				++i;
		}
	}
}

// it removes all references to this object from all objects of the specified type
//
//-----------------------------------------------------------------------------------------------------------
void CAIObjectManager::RemoveObjectFromAllOfType(int nType, CAIObject* pRemovedObject)
{
	AIObjectOwners::iterator ai = m_Objects.lower_bound(nType);
	AIObjectOwners::iterator end = m_Objects.end();
	for (; ai != end && ai->first == nType; ++ai)
		ai->second.GetAIObject()->OnObjectRemoved(pRemovedObject);
}

//-----------------------------------------------------------------------------------------------------------
void CAIObjectManager::ReleasePooledObject(CAIObject* pObject)
{
	assert(pObject && pObject->IsFromPool());

	// destruct and free pooled memory
	pObject->~CAIObject();
	m_pPoolAllocator->Deallocate(pObject);
}

//-----------------------------------------------------------------------------------------------------------
void CAIObjectManager::OnPoolDefinitionsLoaded(size_t num)
{
	Reset(true);
	SAFE_DELETE(m_pPoolAllocator);

	// ensure that the pool is big enough for any type of AI object
	STATIC_CHECK(sizeof(TPooledAIObject) >= sizeof(CPuppet), Change_TPooledAIObject_To_CPuppet);
	STATIC_CHECK(sizeof(TPooledAIObject) >= sizeof(CAIActor), Change_TPooledAIObject_To_CAIActor);
	STATIC_CHECK(sizeof(TPooledAIObject) >= sizeof(CPipeUser), Change_TPooledAIObject_To_CPipeUser);
	STATIC_CHECK(sizeof(TPooledAIObject) >= sizeof(CAIObject), Change_TPooledAIObject_To_CAIObject);
	STATIC_CHECK(sizeof(TPooledAIObject) >= sizeof(CAIVehicle), Change_TPooledAIObject_To_CAIVehicle);
	STATIC_CHECK(sizeof(TPooledAIObject) >= sizeof(CLeader), Change_TPooledAIObject_To_CLeader);
	STATIC_CHECK(sizeof(TPooledAIObject) >= sizeof(CAIPlayer), Change_TPooledAIObject_To_CAIPlayer);
	STATIC_CHECK(sizeof(TPooledAIObject) >= sizeof(CAIFlyingVehicle), Change_TPooledAIObject_To_CAIFlyingVehicle);

	m_pPoolAllocator = new stl::TPoolAllocator<TPooledAIObject>(stl::FHeap().PageSize(num));
	m_PoolBucketSize = num;
}

//-----------------------------------------------------------------------------------------------------------
void CAIObjectManager::OnEntityPreparedFromPool(EntityId entityId, IEntity* pEntity)
{
	// not necessary to do anything here; the AI object will be created when the
	//	bookmark is serialized
}

//-----------------------------------------------------------------------------------------------------------
void CAIObjectManager::OnEntityReturnedToPool(EntityId entityId, IEntity* pEntity)
{
	// the check for pEntity->IsFromPool fixes the editor, where 'pooled' entities are actually
	//	just disabled
	if (pEntity && pEntity->HasAI() && pEntity->IsFromPool())
	{
		tAIObjectID idToRemove = INVALID_AIOBJECTID;
		CAIObject* pObject = NULL;
		bool serializing = (gEnv->pSystem->IsSerializingFile() != 0);

		// if we're serializing the AI object might have already been removed by the ai system Flush
		{
			CCountedRef<CAIObject> objectref = stl::find_in_map(m_pooledObjects, entityId, CCountedRef<CAIObject>());
			assert(serializing || !objectref.IsNil());

			idToRemove = objectref.GetObjectID();
			pObject = objectref.GetAIObject();

			// Marcio: This should probably not be here but should perhaps be called by the AIProxyFactory in CryAction
			if (pObject)
			{
				if (IAIActorProxy* proxy = pObject->GetProxy())
					proxy->Reset(AIOBJRESET_SHUTDOWN);
			}

			// the entity and the ai object should both hold the id of the other
			assert(serializing || idToRemove == pEntity->GetAIObjectID());
			assert(serializing || ((pObject != NULL) && (pObject->GetEntityID() == entityId)));
		}

		// this will remove the object from the m_Objects map,
		// meaning the ref count will be zero and the object should be deleted
		//	(CAIObject::Release will call ReleasePooledObject above, to actually free the memory)
		if (idToRemove != INVALID_AIOBJECTID)
		{
			RemoveObject(idToRemove);

			// add a temporary NULL object to make sure no other AI object is created using the same id
			gAIEnv.pObjectContainer->ReserveID(idToRemove);
		}

		// Entity should now forget about the AI object, since it will be removed.
		//	On reactivating from the pool, the object ID is saved and used to recreate
		//	the AI object; the ID will be reset on the entity at that point.
		pEntity->SetAIObjectID(INVALID_AIOBJECTID);
	}
}

//-----------------------------------------------------------------------------------------------------------
void CAIObjectManager::OnBookmarkEntitySerialize(TSerialize serialize, void* pVEntity)
{
	// In the editor nothing is truly pooled; entities are just hidden instead.
	//	So no need to serialize anything to/from the bookmark
	if (gEnv->IsEditor())
		return;

	m_serializingBookmark = true;

	IEntity* pEntity = reinterpret_cast<IEntity*>(pVEntity);

	// not all pooled entities have AI: don't write anything if so (meaning this will all be skipped on loading as well)
	if (serialize.BeginOptionalGroup("BookmarkedAIObject", pEntity->HasAI()))
	{
		if (serialize.IsWriting())
		{
			CCountedRef<CAIObject> objectref = stl::find_in_map(m_pooledObjects, pEntity->GetId(), CCountedRef<CAIObject>());
			CAIObject* pObject = objectref.GetAIObject();
			if (pObject)
			{
				SAIObjectCreationHelper objHeader(pObject);
				objHeader.Serialize(serialize);

				pObject->Serialize(serialize);
			}
		}
		else
		{
			CCountedRef<CAIObject> objectref = stl::find_in_map(m_pooledObjects, pEntity->GetId(), CCountedRef<CAIObject>());

			SAIObjectCreationHelper objHeader(NULL);

			objHeader.Serialize(serialize);
			assert(objHeader.aiClass != eAIC_Invalid);
			assert(objHeader.objectId != INVALID_AIOBJECTID);

			CAIObject* pObject = objectref.GetAIObject();

			if (!objectref.IsNil() && objHeader.objectId != objectref.GetObjectID())
			{
				assert(false);

				// deregister + remove old object

				RemoveObject(objectref.GetObjectID());
				objectref.Release();

				gAIEnv.pObjectContainer->ReleaseDeregisteredObjects(true);

				// verify that it's been removed
				// cppcheck-suppress assertWithSideEffect
				assert(GetAIObject(objHeader.objectId) == NULL);
			}

			// now recreate object with new ai object ID
			if (objectref.IsNil())
			{
				void* pAlloc = m_pPoolAllocator->Allocate();

				pObject = objHeader.RecreateObject(pAlloc);

				assert(pAlloc == pObject);
				assert(pObject);
				pObject->m_createdFromPool = true;

				// reregister the object with both the object container and our own maps.
				//	NB: requesting a specific ID here will allow us to take it from the
				//	'reserved ids' list in the object container
				CStrongRef<CAIObject> ref;
				gAIEnv.pObjectContainer->RegisterObject(pObject, ref, objHeader.objectId);
				objectref = ref;
				m_Objects.insert(AIObjectOwners::iterator::value_type(pObject->GetAIType(), objectref));
				m_pooledObjects[pEntity->GetId()] = objectref;
			}

			// set this before serializing the AI object
			pEntity->SetAIObjectID(objHeader.objectId);

			// serialize into the existing object
			pObject->Serialize(serialize);
			pObject->PostSerialize();
		}

		serialize.EndGroup(); // BookmarkedAIObject
	}

	m_serializingBookmark = false;
}
