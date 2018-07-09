// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AIObjectManager.h"

//TODO should be removed so no knowledge of these are here:
#include "Puppet.h"
#include "AIVehicle.h"
#include "AIFlyingVehicle.h"
#include "AIPlayer.h"
#include "AIObjectIterators.h"//TODO get rid of this file totally!
#include "ObjectContainer.h"
#include "./TargetSelection/TargetTrackManager.h"
#include "AIEntityComponent.h"

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
{
}

CAIObjectManager::~CAIObjectManager()
{
	m_mapDummyObjects.clear();
}

void CAIObjectManager::Init()
{
}

void CAIObjectManager::Reset(bool includingPooled /*=true*/)
{
	m_mapDummyObjects.clear();
	m_Objects.clear();
}

IAIObject* CAIObjectManager::CreateAIObject(const AIObjectParams& params)
{
	CCCPOINT(CreateAIObject);
	CRY_ASSERT(params.type != 0);

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

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(params.entityID);
	uint16 type = params.type;

	// Attempt to remove already existing AI object for this entity
	RemoveObjectByEntityId(params.entityID);

	switch (type)
	{
	case AIOBJECT_DUMMY:
		CRY_ASSERT_MESSAGE(false, "Creating dummy object through the AI object manager (use CAISystem::CreateDummyObject instead)");
		return 0;
	case AIOBJECT_ACTOR:
		type = AIOBJECT_ACTOR;
		pObject = new CPuppet;
		break;
	case AIOBJECT_2D_FLY:
		type = AIOBJECT_ACTOR;
		pObject = new CPuppet;
		pObject->SetSubType(CAIObject::STP_2D_FLY);
		break;
	case AIOBJECT_LEADER:
		{
			int iGroup = params.association ? static_cast<CPuppet*>(params.association)->GetGroupId() : -1;
			pLeader = new CLeader(iGroup);
			pObject = pLeader;
		}
		break;
	case AIOBJECT_BOAT:
		type = AIOBJECT_VEHICLE;
		pObject = new CAIVehicle;
		pObject->SetSubType(CAIObject::STP_BOAT);
		//				pObject->m_bNeedsPathOutdoor = false;
		break;
	case AIOBJECT_CAR:
		type = AIOBJECT_VEHICLE;
		pObject = new CAIVehicle;
		pObject->SetSubType(CAIObject::STP_CAR);
		break;
	case AIOBJECT_HELICOPTER:
		type = AIOBJECT_VEHICLE;
		pObject = new CAIVehicle;
		pObject->SetSubType(CAIObject::STP_HELI);
		break;
	case AIOBJECT_INFECTED:
		type = AIOBJECT_ACTOR;
		pObject = new CAIActor;
		break;
	case AIOBJECT_PLAYER:
		pObject = new CAIPlayer; // just a dummy for the player
		break;
	case AIOBJECT_RPG:
	case AIOBJECT_GRENADE:
		pObject = new CAIObject;
		break;
	case AIOBJECT_WAYPOINT:
	case AIOBJECT_HIDEPOINT:
	case AIOBJECT_SNDSUPRESSOR:
	case AIOBJECT_NAV_SEED:
		pObject = new CAIObject;
		break;
	case AIOBJECT_ALIENTICK:
		type = AIOBJECT_ACTOR;
		pObject = new CPipeUser;
		break;
	case AIOBJECT_HELICOPTERCRYSIS2:
		type = AIOBJECT_ACTOR;
		pObject = new CAIFlyingVehicle;
		pObject->SetSubType(CAIObject::STP_HELICRYSIS2);
		break;
	default:
		// try to create an object of user defined type
		pObject = new CAIObject;
		break;
	}

	assert(pObject);

	// Register the object
	CStrongRef<CAIObject> object;
	gAIEnv.pObjectContainer->RegisterObject(pObject, object, idToUse);

	CCountedRef<CAIObject> countedRef = object;

	// insert object into map under key type
	// this is a multimap
	m_Objects.insert(AIObjectOwners::iterator::value_type(type, countedRef));

	// Create an associated AI entity component
	pEntity->GetOrCreateComponentClass<CAIEntityComponent>(countedRef.GetWeakRef());

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

	GetAISystem()->OnAIObjectCreated(pObject);

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

void CAIObjectManager::RemoveObject(const tAIObjectID objectID)
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
		return;
	}

	m_Objects.erase(it);

	// Because Action doesn't yet handle a delayed removal of the Proxies, we should perform cleanup immediately.
	// Note that this only happens when triggered externally, when an entity is refreshed/removed
	gAIEnv.pObjectContainer->ReleaseDeregisteredObjects(false);
}

void CAIObjectManager::RemoveObjectByEntityId(const EntityId entityId)
{
	AIObjectOwners::iterator it = m_Objects.begin();
	AIObjectOwners::iterator itEnd = m_Objects.end();
	for (; it != itEnd; ++it)
	{
		if (it->second->GetEntityID() == entityId)
		{
			RemoveObject(it->second.GetObjectID());
			return;
		}
	}
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
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	CCCPOINT(OnObjectRemoved);

	if (!pObject)
		return;

	RemoveObjectFromAllOfType(AIOBJECT_ACTOR, pObject);
	RemoveObjectFromAllOfType(AIOBJECT_VEHICLE, pObject);
	RemoveObjectFromAllOfType(AIOBJECT_ATTRIBUTE, pObject);
	RemoveObjectFromAllOfType(AIOBJECT_LEADER, pObject);

	for (AIObjects::iterator oi = m_mapDummyObjects.begin(); oi != m_mapDummyObjects.end(); ++oi)
	{
		if (oi->second == pObject)
		{
			m_mapDummyObjects.erase(oi);
			break;
		}
	}

	GetAISystem()->OnAIObjectRemoved(pObject);
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
