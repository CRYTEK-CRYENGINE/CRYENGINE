// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ObjectContainer.h
//  Created:     11/02/2008 by Matthew
//  Description: Manages the stubs and pointers to all AI objects
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __AIOBJECTCONTAINER
#define __AIOBJECTCONTAINER

#pragma once

#include <vector>
#include <CryCore/Containers/IDMap.h>

#include "Reference.h"
#include <CryAISystem/IAIObjectManager.h>

struct IAIObject;
class CAIObject;
class CAbstractUntypedRef;
template<class T> class CAbstractRef;
template<class T> class CCountedRef;
template<class T> class CStrongRef;
template<class T> class CWeakRef;

#define MAX_AI_OBJECTS (1 << 13)

/**
 * Container for AI Objects.
 * All AI objects should be registered through the the ObjectContainer, which is the only way strong references
 * can be assigned. The container manages the objects themselves, their stubs, their validity status
 * (though the use of salts) and the destruction of the objects at the end of an AI frame.
 */
class CObjectContainer
{
public:
	typedef std::vector<IAIObject*> TAIObjectVec;
	CObjectContainer(void);

	void       Reset();

	ILINE bool IsValid(const CAbstractUntypedRef& ref) const
	{
		return (GetAIObject(ref) != NULL);
	}

	ILINE const CAIObject* GetAIObject(const CAbstractUntypedRef& ref) const
	{
		tAIObjectID objectID = ref.GetObjectID();

		return m_objects.validate(objectID) ? m_objects[objectID] : 0;
	}

	ILINE CAIObject* GetAIObject(const CAbstractUntypedRef& ref)
	{
		tAIObjectID objectID = ref.GetObjectID();

		return m_objects.validate(objectID) ? m_objects[objectID] : 0;
	}

	ILINE CAIObject* GetAIObject(tAIObjectID objectID)
	{
		return m_objects.validate(objectID) ? m_objects[objectID] : 0;
	}

	// Add a new AI Object to the manager, which will then own it
	// In theory, this might fail due to lack of buffer space
	// Might need to pass in an existing stub...
	// Also, the Object should keep a pointer to its stub... hmm...
	// Allows registration of objects with a specified ID (mostly for use after serialization.
	//  If specified, the ID to be used should already be in the reserved list)
	bool RegisterObject(CAIObject* pObject, CStrongRef<CAIObject>& ref, tAIObjectID inId = INVALID_AIOBJECTID);

	// Remove an object from the manager via its handle
	// Returns false if no object was associated with that handle
	template<class T> bool DeregisterObject(CStrongRef<T>& ref)
	{
		// The templating is really just a wrapper, so call an untyped private function to do the real work
		return DeregisterObjectUntyped(&ref);
	}

	int                           ReleaseDeregisteredObjects(bool checkForLeaks);

	template<class T> CWeakRef<T> GetWeakRef(T* pObject)
	{
		return pObject ? GetWeakRef(pObject->GetAIObjectID()) : CWeakRef<T>();
	}

	// Get a weak reference corresponding to a handle
	CWeakRef<CAIObject> GetWeakRef(tAIObjectID nID);

	int                 GetNumRegistered() { return m_snObjectsRegistered - m_snObjectsDeregistered; }

	void                DumpRegistered();

	// Serialization: the Object Container and all the objects it contains are handled in
	//	the normal Serialize function. SerializeObjectIDs is used to read and write the
	//	full list of required object IDs, to prevent ID collisions. PostSerialize
	//	just allows objects to fix up things relating to other entities / ai objects
	void SerializeObjectIDs(TSerialize ser);
	void Serialize(TSerialize ser);
	void PostSerialize();

	void RebuildObjectMaps(std::multimap<short, CCountedRef<CAIObject>>& objectMap, std::multimap<short, CWeakRef<CAIObject>>& dummyMap);

	void ReserveID(tAIObjectID id);
	void UnreserveID(tAIObjectID id);

protected:
	// Perform much of the work of deregistering an object ignoring the type info
	bool DeregisterObjectUntyped(CAIObject* pObject);
	bool DeregisterObjectUntyped(CAbstractUntypedRef* pRef);

	id_map<tAIObjectID, CAIObject*> m_objects;

	typedef std::vector<tAIObjectID> TVecAIObjects;
	TVecAIObjects m_DeregisteredBuffer;                 // When deregistered, CAIObjects are added here
	TVecAIObjects m_DeregisteredWorkingBuffer;          // Working buffer during deletion (consider re-entrant deletes)
	static int    m_snObjectsRegistered, m_snObjectsDeregistered;

	// certain objects (eg those for pooled entities) may be removed temporarily during gameplay,
	// yet need to be recreated later using the same object ID. In those cases we keep the ID reserved
	// by adding a NULL entry to m_objects, so nothing else can take an ID corresponding
	//	to the same index in the buffer.
	TVecAIObjects m_reservedIDs;
};

#include "Reference.inl"

#endif // __AIOBJECTCONTAINER
