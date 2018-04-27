// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ObjectContainer.h"

#include "AIEntityComponent.h"

// Annoying...
#include "AIVehicle.h"
#include "AIPlayer.h"

int CObjectContainer::m_snObjectsRegistered = 0;
int CObjectContainer::m_snObjectsDeregistered = 0;

const char* GetNameFromType(EAIClass type)
{
	switch (type)
	{
	case eAIC_AIVehicle:
		return "CAIVehicle";
	case eAIC_Puppet:
		return "CPuppet";
	case eAIC_PipeUser:
		return "CPipeUser";
	case eAIC_AIPlayer:
		return "CAIPlayer";
	case eAIC_Leader:
		return "CLeader";
	case eAIC_AIActor:
		return "CAIActor";
	case eAIC_AIObject:
		return "CAIObject";
	case eAIC_AIFlyingVehicle:
		return "CAIFlyingVehicle";
	default:
		assert(false);
		return "<UNKNOWN>";
	}
}

CObjectContainer::CObjectContainer(void)
	: m_objects(MAX_AI_OBJECTS)
{
}

void CObjectContainer::Reset()
{
	// unreserve all IDs first
	for (size_t i = 0; i < m_reservedIDs.size(); ++i)
	{
		m_objects.erase(m_reservedIDs[i]);
		m_snObjectsDeregistered++;
	}
	stl::free_container(m_reservedIDs);

	const int numRegistered = GetNumRegistered();
	if (numRegistered != 0)
	{
		DumpRegistered();
	}
	CRY_ASSERT_MESSAGE(numRegistered == 0, "Something has leaked AI objects, and we're about to create dangling pointers! Check the log for details");

	m_objects.clear();

	stl::free_container(m_DeregisteredBuffer);
	stl::free_container(m_DeregisteredWorkingBuffer);
}

// pObject is essential - have to register something of course
// ref is to set a strong reference if we have one, otherwise we accept it is unowned
// inId is optional, if specified will attempt to register the object under that ID. Mostly this is used for serialization.
bool CObjectContainer::RegisterObject(CAIObject* pObject, CStrongRef<CAIObject>& ref, tAIObjectID inId /*=INVALID_AIOBJECTID*/)
{
	CCCPOINT(RegisterObjectUntyped);

	// First, check and release the reference if it is already used
	// Recreating an object like this usually isn't necessary but the semantics make sense
	if (!ref.IsNil())
	{
		ref.Release();
	}

	m_snObjectsRegistered++;

#if _DEBUG
	if (pObject)
	{
		uint16 capacity = static_cast<uint16>(m_objects.capacity());

		tAIObjectID prevID = 0;
		for (uint16 i = 0; i < capacity; ++i)
		{
			if (!m_objects.index_free(i) && (m_objects.get_index(i) == pObject))
			{
				prevID = m_objects.get_index_id(i);
				break;
			}
		}

		assert(!prevID);
		if (prevID)
		{
			gEnv->pLog->LogError("AI: CObjectContainer::RegisterObjectUntyped - Object already registered - %p @%6d \"%s\" ",
			                     (pObject), prevID, pObject->GetName());
			// intentionally not calling pObject->SetSelfReference(ref); here, since that would be bad.
			return false;
		}
	}
#endif

	tAIObjectID id = inId;
	if (id != INVALID_AIOBJECTID)
	{
		// Registering an object with a specified ID. This usually means the object was serialized out
		//	with a specific ID (eg to a pool bookmark) and now needs to be recreated using the same ID.

		// In this case the ID should have been reserved earlier, and there should be a null object
		//	in the object map. UnreserveID will remove that, so we can add the new object in its place.
		UnreserveID(id);

		m_objects.insert(id, pObject);
	}
	else
	{
		id = m_objects.insert(pObject);
	}

	AIAssert(id != INVALID_AIOBJECTID);

	ref.Assign(id);

	AILogComment("Registered object %p @%6d \"%s\" ", pObject, id, pObject ? pObject->GetName() : "NULL");

	if (pObject)
	{
		pObject->SetSelfReference(ref);
	}

	return true;
}

bool CObjectContainer::DeregisterObjectUntyped(CAIObject* pObject)
{
	CWeakRef<CAIObject> ref(GetWeakRef(pObject));
	return DeregisterObjectUntyped(&ref);
}

// Only strong refs should ever be passed in
bool CObjectContainer::DeregisterObjectUntyped(CAbstractUntypedRef* ref)
{
	CCCPOINT(DeregisterObjectUntyped);

	// (MATT) Checks for double-deregister in debug might be helpful here - but if the mechanisms are enforced it shouldn't be possible {2009/03/30}

	// (MATT) Perhaps this isn't the right place to increment - they are only pushed on a list, after all {2009/04/07}
	m_snObjectsDeregistered++;
	assert(m_snObjectsRegistered >= m_snObjectsDeregistered);

	tAIObjectID id = ref->GetObjectID();
	bool validID = m_objects.validate(id);

	CRY_ASSERT_TRACE(validID, ("Multiple AI objects with id %i, dangling pointers or corruption imminent", id));

	if (validID)
	{
		CRY_ASSERT_MESSAGE(!stl::find(m_DeregisteredBuffer, id), "Double deregistering object!");
		m_DeregisteredBuffer.push_back(id);
		ref->Assign(INVALID_AIOBJECTID);

#ifdef _DEBUG
		const CAIObject* object = m_objects[id];
		const char* const name = object ? object->GetName() : "<NULL OBJECT>";

		AILogComment("Deregistered object %p @%6d \"%s\" ", (object), id, name);
#endif
		return true;
	}
	else
	{
		int prevIndex = m_objects.get_index_for_id(id);
		CAIObject* pObject = m_objects.get_index(prevIndex);
		CRY_ASSERT_TRACE(false, ("Previous object was %s (%d)", pObject ? pObject->GetName() : "<NULL OBJECT>", pObject ? pObject->GetAIObjectID() : 0));

		return false;
	}
}

// This implictly has to mark something for deletion
// Or, at least, it does usually....
// No point returning the pointer because that encourages people to delete it themselves, which is no use

// We (should!) call this at the end of each AI frame
int CObjectContainer::ReleaseDeregisteredObjects(bool checkForLeaks)
{
	CCCPOINT(ReleaseDeregisteredObjects);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	int nReleased = 0;

	// We use a double-buffer approach
	// Currently, deleting objects currently triggers deregistration of any sub-objects
	// It could be made to work with one vector but this seems more debuggable
	int loopLimit = 100;
	while (!m_DeregisteredBuffer.empty() && loopLimit)
	{
		assert(--loopLimit > 0);
		m_DeregisteredBuffer.swap(m_DeregisteredWorkingBuffer);

		TVecAIObjects::iterator itO = m_DeregisteredWorkingBuffer.begin();
		TVecAIObjects::iterator itOEnd = m_DeregisteredWorkingBuffer.end();
		for (; itO != itOEnd; ++itO)
		{
			const tAIObjectID& id = *itO;
			const bool validHandle = m_objects.validate(id);

			assert(validHandle);
			if (validHandle)
			{
				CAIObject* object = m_objects[id];

#ifdef _DEBUG
				// Before we start removing the object check for Proxy and release if necessary
				// This allows us to prepare for any proxy queries during remove procedure, such as checking health
				const CAIActor* actor = object ? object->CastToCAIActor() : NULL;
				const IAIActorProxy* proxy = actor ? actor->GetProxy() : NULL;
				const char* const name = object ? object->GetName() : "<NULL OBJECT>";

				AILogComment("Releasing object %p @%6d proxy %p \"%s\" ",
				             object, id, proxy, name);
#endif
				if (object)
				{

					// For transitional purposes (at least) call the remove code now
					gAIEnv.pAIObjectManager->OnObjectRemoved(object);

					// Delete the object. Past this point, weak refs will still function and you can still fetch them for a given pointer, but the object itself is gone
					// It might be better to put off that delete until we wipe pointers from the stubs, but if GetWeakRef disappeared, so would the need for all that.
					object->Release();
					//m_DeregisteredObjects.push_back(*itO);
				}

				m_objects.erase(id);

				// Special case: if this ID is in the reserve list, we now need to add a null object to reserve the ID.
				// This is because the object's ID has been reserved while the object still existed.
				if (stl::find(m_reservedIDs, id))
				{
					m_objects.insert(id, NULL);
					m_snObjectsRegistered++;
				}

				nReleased++;
			}
		}
		m_DeregisteredWorkingBuffer.clear();
	}

#ifdef CRYAISYSTEM_DEBUG
	if (checkForLeaks)
	{
		size_t totalObjects = GetNumRegistered();
		size_t count = m_objects.size();

		if (count != totalObjects)
			DumpRegistered();

		CRY_ASSERT_MESSAGE(count == totalObjects, "Something has leaked AI objects! Check the log for details");
	}
#endif

	return nReleased;
}

CWeakRef<CAIObject> CObjectContainer::GetWeakRef(tAIObjectID id)
{
	if (m_objects.validate(id))
		return CWeakRef<CAIObject>(id);
	return NILREF;
}

void CObjectContainer::DumpRegistered()
{
#ifdef _DEBUG
	size_t count = 0;
	gEnv->pLog->Log("Listing all current AI objects:");
	for (uint16 i = 0; i < static_cast<uint16>(m_objects.capacity()); ++i)
	{
		if (!m_objects.index_free(i))
		{
			CAIObject* object = m_objects.get_index(i);
			CWeakRef<CAIObject> weakRef = GetWeakRef(object);
			gEnv->pLog->Log("Slot %" PRISIZE_T ": Object %p @%6d \"%s\" ", i, (object), weakRef.GetObjectID(), object ? object->GetName() : "NULL");
			++count;
		}
	}

	gEnv->pLog->Log("Total object count %" PRISIZE_T, count);
#endif
}

void CObjectContainer::Serialize(TSerialize ser)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Object Container serialization");

	CAISystem* pAISystem = GetAISystem();

	// Deal with the deregistration list as it makes no sense to serialize those objects
	ReleaseDeregisteredObjects(true);

	ser.BeginGroup("ObjectContainer");

	const bool bReading = ser.IsReading();

	uint32 totalObjects = (uint32)GetNumRegistered();
	uint32 capacity = m_objects.capacity();

	if (ser.IsWriting())
	{
		for (uint32 i = 0; i < capacity; ++i)
		{
			if (!m_objects.index_free(i))
			{
				CAIObject* object = m_objects.get_index(i);

				// AI objects associated with pooled entities are serialized as
				//	 part of the entity bookmark: skip them here
				if (!object || object->ShouldSerialize() == false)
				{
					if (object)
					{
						AILogComment("Serialization skipping no-save object %p @%6d \"%s\" ", object, object->GetAIObjectID(), object->GetName());
					}

					totalObjects--;
					continue;
				}
			}
		}
	}

	ser.Value("total", totalObjects);

	for (uint32 i = 0, totalSerialised = 0; totalSerialised < totalObjects && i < capacity; ++i)
	{
		CAIObject* object = 0;

		if (ser.IsWriting())
		{
			// First, is there a valid object here?
			if (m_objects.index_free(i))
				continue;

			object = m_objects.get_index(i);

			// AI objects associated with pooled entities are serialized as
			//	 part of the entity bookmark: skip them here
			if (!object || !object->ShouldSerialize())
				continue;
		}

		tAIObjectID id = INVALID_AIOBJECTID;

		if (ser.IsWriting())
			id = m_objects.get_index_id(i);

		// If writing, we've established this is a valid index
		// If reading, we will read the next valid index and skip ahead
		ser.BeginGroup("Entry");
		ser.Value("index", i);    // Reading may change the loop iterator
		ser.Value("id", id);

		// Serialize basic data (enough to allow recreation of the object)
		SAIObjectCreationHelper objHeader(object);
		objHeader.Serialize(ser);
		assert(id == objHeader.objectId);

		bool bHadObject = object != nullptr && m_objects.get(id) != nullptr;

		if (bReading)
		{
			//Read type for creation, skipping if the object already exists
			if (!bHadObject)
			{
				object = objHeader.RecreateObject();

				// all IDs for objects to serialize should have been reserved earlier
				UnreserveID(id);

				m_objects.insert(id, object);
				m_snObjectsRegistered++;
			}
			else
			{
				object = m_objects.get(id);
			}
		}

		if (object)
		{
			MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "AI object (%s) serialization", GetNameFromType(objHeader.aiClass));

			// (MATT) Note that this call may reach CAIActor, which currently handles serialising the proxies {2009/04/30}
			object->Serialize(ser);

			if (bReading && !bHadObject)
			{
				if (IEntity* pEntity = object->GetEntity())
				{
					// Re-create the associated AI entity component
					pEntity->GetOrCreateComponentClass<CAIEntityComponent>(GetWeakRef(object));
				}
			}
		}
		totalSerialised++;

		// Simple back-and-forth test, should be valid at this point
		// cppcheck-suppress assertWithSideEffect
		assert(GetWeakRef(object).IsValid());

		if (bReading)
			AILogComment("Serialisation created object %p @%6d \"%s\" ", object, id, object ? object->GetName() : "");

		ser.EndGroup();
	}

	ser.EndGroup();
}

void CObjectContainer::SerializeObjectIDs(TSerialize ser)
{
	if (ser.IsReading())
	{
		// AI flush should have reset everything by this point
		assert(m_objects.size() == 0);

		// read in the reserved object IDs and the used IDs
		//	before the rest of the system is serialized

		// add NULL objects for each of the reserved IDs
		ser.Value("reservedObjects", m_reservedIDs);
		for (size_t i = 0; i < m_reservedIDs.size(); ++i)
		{
			m_objects.insert(m_reservedIDs[i], NULL);
			m_snObjectsRegistered++;
		}

		ser.BeginGroup("existingObjects");
		uint32 objectCount;
		ser.Value("count", objectCount);
		for (uint32 i = 0; i < objectCount; ++i)
		{
			ser.BeginGroup("object");
			tAIObjectID id = INVALID_AIOBJECTID;
			ser.Value("id", id);

			//string name = object->GetName();
			//ser.Value("name", name);

			// again, add a NULL object for each one.
			ReserveID(id);

			ser.EndGroup();
		}
		ser.EndGroup();
	}
	else
	{
		// write out:
		//	- the list of reserved object IDs
		//	- a list of all currently registered objects
		ser.Value("reservedObjects", m_reservedIDs);

		ser.BeginGroup("existingObjects");
		uint32 objectCount = 0;
		uint32 totalObjects = (uint32)m_objects.size();
		uint32 capacity = (uint32)m_objects.capacity();
		for (uint32 i = 0; i < capacity && objectCount < totalObjects; ++i)
		{
			if (!m_objects.index_free(i))
			{
				CAIObject* object = m_objects.get_index(i);
				if (object)
				{
					++objectCount;

					ser.BeginGroup("object");
					tAIObjectID id = object->GetAIObjectID();
					ser.Value("id", id);

					// useful for debugging
					//string name = object->GetName();
					//ser.Value("name", name);

					ser.EndGroup();
				}
			}
		}
		ser.Value("count", objectCount);
		ser.EndGroup();
	}
}

void CObjectContainer::PostSerialize()
{
	size_t capacity = m_objects.capacity();
	for (size_t i = 0; i < capacity; ++i)
	{
		uint16 i16 = static_cast<uint16>(i);
		if (!m_objects.index_free(i16))
		{
			CAIObject* object = m_objects.get_index(i16);
			if (object)
			{
				object->PostSerialize();
			}
			else
			{
				// NULL object in m_objects map:
				//	verify that this object is on the reserved list
				assert(stl::find(m_reservedIDs, m_objects.get_index_id(i16)));
			}
		}
	}
}

void CObjectContainer::RebuildObjectMaps(std::multimap<short, CCountedRef<CAIObject>>& objectMap, std::multimap<short, CWeakRef<CAIObject>>& dummyMap)
{
	size_t capacity = m_objects.capacity();

	for (size_t i = 0; i < capacity; ++i)
	{
		uint16 i16 = static_cast<uint16>(i);
		if (!m_objects.index_free(i16))
		{
			CAIObject* object = m_objects.get_index(i16);
			if (!object)
				continue;

			unsigned short type = object->GetType();

			bool bIsDummy = false;
			if (type == AIOBJECT_DUMMY)
				bIsDummy = true;
			else if (type == AIOBJECT_WAYPOINT && object->GetSubType() == IAIObject::STP_BEACON)
				bIsDummy = true;

			if (bIsDummy)
			{
				CWeakRef<CAIObject> ref = GetWeakRef(object->GetAIObjectID());
				dummyMap.insert(std::multimap<short, CWeakRef<CAIObject>>::iterator::value_type(object->GetSubType(), ref));
			}
			else
			{
				CStrongRef<CAIObject> ref;
				ref.Assign(object->GetAIObjectID());
				objectMap.insert(std::multimap<short, CCountedRef<CAIObject>>::iterator::value_type(object->GetType(), ref));
			}
		}
	}
}

void CObjectContainer::ReserveID(tAIObjectID id)
{
	assert(id != INVALID_AIOBJECTID);

	// Objects may already be in the reserve list - this happens for instance when an AI is
	//	active at a checkpoint save and then is later deactivated (returned to pool): both will
	//	cause a serialize-to-bookmark, which will reserve the ID. So just ignore the second reserve.
	stl::push_back_unique(m_reservedIDs, id);

	// If an object still exists using this ID, leave the existing one there.
	//	When removed, ReleaseDeregisteredObjects will add the NULL entry.
	if (m_objects.free(id))
	{
		m_objects.insert(id, NULL);

		m_snObjectsRegistered++;  // prevents asserts about leaking objects
	}
}

void CObjectContainer::UnreserveID(tAIObjectID id)
{
	assert(id != INVALID_AIOBJECTID);
	assert(stl::find(m_reservedIDs, id));
	assert(m_objects.get(id) == NULL);

	if (m_objects.get(id) != NULL)
	{
		// this would suggest a code error: something was added to the reserve list, which should have then placed a NULL
		//	AI object in m_objects. That should mean that no other object can take that slot.
		CAIObject* pPrevObject = m_objects[id];
		AILogAlways("Error: Trying to unreserve existing AI Object id %d, object is %s", id, pPrevObject ? pPrevObject->GetName() : "NULL");

		// Returning now probably means the request won't be able to register a new object using this ID.
		// That's bad, but probably better than removing some other object.
		return;
	}

	stl::find_and_erase(m_reservedIDs, id);
	m_objects.erase(id);
	m_snObjectsDeregistered++;
}
