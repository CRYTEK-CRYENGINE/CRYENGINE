// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef AIOBJECTMANAGER
#define AIOBJECTMANAGER

#if _MSC_VER > 1000
	#pragma once
#endif

#include "AIObject.h"

#include <CryAISystem/IAIObjectManager.h>
#include <CryMemory/PoolAllocator.h>

typedef std::multimap<short, CCountedRef<CAIObject>> AIObjectOwners;
typedef std::multimap<short, CWeakRef<CAIObject>>    AIObjects;

enum EAIClass
{
	eAIC_Invalid         = 0,

	eAIC_FIRST           = 1,

	eAIC_AIObject        = 1,
	eAIC_AIActor         = 2,
	eAIC_Leader          = 3,
	eAIC_AIPlayer        = 4,
	eAIC_PipeUser        = 5,
	eAIC_Puppet          = 6,
	eAIC_AIVehicle       = 7,
	eAIC_AIFlyingVehicle = 8,

	eAIC_LAST            = eAIC_AIVehicle
};

struct SAIObjectCreationHelper
{
	SAIObjectCreationHelper(CAIObject* pObject);
	void       Serialize(TSerialize ser);
	CAIObject* RecreateObject(void* alloc = NULL);

	string      name;
	EAIClass    aiClass;
	tAIObjectID objectId;
};

class CAIObjectManager : public IAIObjectManager
{
public:

	CAIObjectManager();
	virtual ~CAIObjectManager();

	void Init();
	void Reset(bool includingPooled = true);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//IAIObjectManager/////////////////////////////////////////////////////////////////////////////////////////////

	virtual IAIObject* CreateAIObject(const AIObjectParams& params);
	virtual void       RemoveObject(const tAIObjectID objectID);
	virtual void       RemoveObjectByEntityId(const EntityId entityId);

	virtual IAIObject* GetAIObject(tAIObjectID aiObjectID);
	virtual IAIObject* GetAIObjectByName(unsigned short type, const char* pName) const;

	// Returns AIObject iterator for first match, see EGetFirstFilter for the filter options.
	// The parameter 'n' specifies the type, group id or species based on the selected filter.
	// It is possible to iterate over all objects by setting the filter to OBJFILTER_TYPE
	// passing zero to 'n'.
	virtual IAIObjectIter* GetFirstAIObject(EGetFirstFilter filter, short n);
	// Iterates over AI objects within specified range.
	// Parameter 'pos' and 'rad' specify the enclosing sphere, for other parameters see GetFirstAIObject.
	virtual IAIObjectIter* GetFirstAIObjectInRange(EGetFirstFilter filter, short n, const Vec3& pos, float rad, bool check2D);

	//IAIObjectManager/////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// callback from CObjectContainer: notify the rest of the system that the object is disappearing
	void OnObjectRemoved(CAIObject* pObject);

	//// it removes all references to this object from all objects of the specified type
	//// (MATT) Note that is does not imply deletion - vehicles use it when they lose their driver {2009/02/05}
	void       RemoveObjectFromAllOfType(int nType, CAIObject* pRemovedObject);

	void       CreateDummyObject(CCountedRef<CAIObject>& ref, const char* name = "", CAIObject::ESubType type = CAIObject::STP_NONE, tAIObjectID requiredID = INVALID_AIOBJECTID);
	void       CreateDummyObject(CStrongRef<CAIObject>& ref, const char* name = "", CAIObject::ESubType type = CAIObject::STP_NONE, tAIObjectID requiredID = INVALID_AIOBJECTID);

	CAIObject* GetAIObjectByName(const char* pName) const;

	// todo: ideally not public
	AIObjectOwners m_Objects;// m_RootObjects or EntityObjects might be better names
	AIObjects      m_mapDummyObjects;
};

#endif
