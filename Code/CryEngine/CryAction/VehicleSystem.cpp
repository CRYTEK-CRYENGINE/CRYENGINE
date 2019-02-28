// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Vehicle System

   -------------------------------------------------------------------------
   History:
   - 05:10:2004   11:55 : Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include <CryScriptSystem/IScriptSystem.h>
#include <CrySystem/File/ICryPak.h>
#include "IGameObjectSystem.h"
#include <CryAISystem/IAIActorProxy.h>
#include <CryAISystem/IAIObject.h>

#include "CryAction.h"
#include "VehicleSystem.h"
#include "VehicleSystem/Vehicle.h"
#include "VehicleSystem/VehiclePartEntity.h"
#include "VehicleSystem/VehiclePartDetachedEntity.h"
#include "VehicleSystem/VehicleDamagesTemplateRegistry.h"
#include "VehicleSystem/VehicleCVars.h"

const char* gScriptPath = "Scripts/Entities/Vehicles/Implementations/";
const char* gXmlPath = "Scripts/Entities/Vehicles/Implementations/Xml/";

#if ENABLE_VEHICLE_DEBUG
CVehicleSystem::TVehicleClassCount CVehicleSystem::s_classInstanceCounts;
#endif

//------------------------------------------------------------------------
CVehicleSystem::CVehicleSystem(ISystem* pSystem, IEntitySystem* pEntitySystem)
	: m_nextObjectId (InvalidVehicleObjectId),
	m_pDamagesTemplateRegistry(NULL),
	m_pVehicleClient(NULL),
	m_pInitializingSeat(NULL),
	m_pCurrentClientVehicle(NULL)
{
	m_pDamagesTemplateRegistry = new CVehicleDamagesTemplateRegistry;
	m_pCVars = new CVehicleCVars;
}

// Iterators now have their destructors called before they enter the pool - so we only need to free the memory here {2008/12/09}
void DeleteVehicleIterator(IVehicleIterator* ptr) { operator delete(ptr); }

//------------------------------------------------------------------------
CVehicleSystem::~CVehicleSystem()
{
	std::for_each(m_iteratorPool.begin(), m_iteratorPool.end(), DeleteVehicleIterator);

	SAFE_RELEASE(m_pDamagesTemplateRegistry);
	SAFE_DELETE(m_pCVars);
}

//------------------------------------------------------------------------
bool CVehicleSystem::Init()
{
	m_nextObjectId = InvalidVehicleObjectId + 1;

	LoadDamageTemplates();
	return true;
}

//------------------------------------------------------------------------
void CVehicleSystem::LoadDamageTemplates()
{
	m_pDamagesTemplateRegistry->Init("Scripts/Entities/Vehicles/DamagesTemplates/");
}

//------------------------------------------------------------------------
void CVehicleSystem::ReloadSystem()
{
	LoadDamageTemplates();
}

//------------------------------------------------------------------------
IVehicle* CVehicleSystem::CreateVehicle(uint16 channelId, const char* name, const char* vehicleClass, const Vec3& pos, const Quat& rot, const Vec3& scale, EntityId id)
{
	// get the entity class
	IEntityClass* pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(vehicleClass);

	if (!pEntityClass)
	{
		CRY_ASSERT(pEntityClass);

		return 0;
	}

	// if a channel is specified and already has a vehicle,
	// use that entity id

	if (channelId)
	{
		IVehicle* pExistingProxy = GetVehicleByChannelId(channelId);

		if (pExistingProxy)
		{
			id = pExistingProxy->GetEntityId();
		}
	}

	SSpawnUserData userData(vehicleClass, channelId);
	SEntitySpawnParams params;
	params.id = id;
	params.pUserData = (void*)&userData;
	params.sName = name;
	params.vPosition = pos;
	params.qRotation = rot;
	params.vScale = scale;
	params.pClass = pEntityClass;

	IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(params, true);
	CRY_ASSERT(pEntity);

	if (!pEntity)
	{
		return 0;
	}

	return GetVehicle(pEntity->GetId());
}

//------------------------------------------------------------------------
IVehicle* CVehicleSystem::GetVehicle(EntityId entityId)
{
	TVehicleMap::iterator it = m_vehicles.find(entityId);

	if (it != m_vehicles.end())
	{
		return it->second;
	}

	return 0;
}

//------------------------------------------------------------------------
IVehicle* CVehicleSystem::GetVehicleByChannelId(uint16 channelId)
{
	for (TVehicleMap::iterator it = m_vehicles.begin(); it != m_vehicles.end(); ++it)
	{
		if (it->second->GetChannelId() == channelId)
		{
			return it->second;
		}
	}

	return 0;
}

//------------------------------------------------------------------------
bool CVehicleSystem::IsVehicleClass(const char* name) const
{
	TVehicleClassMap::const_iterator it = m_classes.find(CONST_TEMP_STRING(name));
	return (it != m_classes.end());
}

//------------------------------------------------------------------------
void CVehicleSystem::RegisterVehicleClass(const char* name, IGameFramework::IVehicleCreator* pCreator, bool isAI)
{
	IEntityClassRegistry::SEntityClassDesc vehicleClass;

	// Allow the name to contain relative path, but use only the name part as class name.
	string className(PathUtil::GetFile(name));
	vehicleClass.sName = className.c_str();
	vehicleClass.sScriptFile = "Scripts/Entities/Vehicles/VehiclePool.lua";

	CCryAction::GetCryAction()->GetIGameObjectSystem()->RegisterExtension(name, pCreator, &vehicleClass);

	m_classes.insert(TVehicleClassMap::value_type(name, pCreator));
}

//------------------------------------------------------------------------
void CVehicleSystem::AddVehicle(EntityId entityId, IVehicle* pProxy)
{
	m_vehicles.insert(TVehicleMap::value_type(entityId, pProxy));

#if ENABLE_VEHICLE_DEBUG
	string vehicleName = "Vehicle:";
	vehicleName += pProxy->GetEntity()->GetClass()->GetName();
	vehicleName += ":";
	vehicleName += pProxy->GetModification();
	s_classInstanceCounts[vehicleName]++;
#endif
}

//------------------------------------------------------------------------
void CVehicleSystem::RemoveVehicle(EntityId entityId)
{
	stl::member_find_and_erase(m_vehicles, entityId);
}

//------------------------------------------------------------------------
void CVehicleSystem::RegisterVehicleMovement(const char* name, IVehicleMovement*(*func)(), bool isAI)
{
	m_movementClasses.insert(TVehicleMovementClassMap::value_type(name, func));
}

//------------------------------------------------------------------------
void CVehicleSystem::RegisterVehicleView(const char* name, IVehicleView*(*func)(), bool isAI)
{
	m_viewClasses.insert(TVehicleViewClassMap::value_type(name, func));
}

//------------------------------------------------------------------------
void CVehicleSystem::RegisterVehiclePart(const char* name, IVehiclePart*(*func)(), bool isAI)
{
	m_partClasses.insert(TVehiclePartClassMap::value_type(name, func));
}

//------------------------------------------------------------------------
void CVehicleSystem::RegisterVehicleDamageBehavior(const char* name, IVehicleDamageBehavior*(*func)(), bool isAI)
{
	m_damageBehaviorClasses.insert(TVehicleDamageBehaviorClassMap::value_type(name, func));
}

//------------------------------------------------------------------------
void CVehicleSystem::RegisterVehicleSeatAction(const char* name, IVehicleSeatAction*(*func)(), bool isAI)
{
	m_seatActionClasses.insert(TVehicleSeatActionClassMap::value_type(name, func));
}

//------------------------------------------------------------------------
void CVehicleSystem::RegisterVehicleAction(const char* name, IVehicleAction*(*func)(), bool isAI)
{
	m_actionClasses.insert(TVehicleActionClassMap::value_type(name, func));
}

//------------------------------------------------------------------------
IVehicleMovement* CVehicleSystem::CreateVehicleMovement(const string& name)
{
	CRY_ASSERT(!name.empty());

	TVehicleMovementClassMap::iterator ite = m_movementClasses.find(name);

	if (ite == m_movementClasses.end())
	{
		GameWarning("Unknown Vehicle movement: <%s>", name.c_str());
		return NULL;
	}

#if ENABLE_VEHICLE_DEBUG
	string movementName = "Movement:";
	movementName += name;
	s_classInstanceCounts[movementName]++;
#endif

	return (*ite->second)();
}

//------------------------------------------------------------------------
IVehicleView* CVehicleSystem::CreateVehicleView(const string& name)
{
	CRY_ASSERT(!name.empty());

	TVehicleViewClassMap::iterator ite = m_viewClasses.find(name);

	if (ite == m_viewClasses.end())
	{
		GameWarning("Unknown Vehicle view: <%s>", name.c_str());
		return NULL;
	}

#if ENABLE_VEHICLE_DEBUG
	string viewName = "View:";
	viewName += name;
	s_classInstanceCounts[viewName]++;
#endif

	return (*ite->second)();
}

//------------------------------------------------------------------------
IVehiclePart* CVehicleSystem::CreateVehiclePart(const string& name)
{
	CRY_ASSERT(!name.empty());

	TVehiclePartClassMap::iterator ite = m_partClasses.find(name);

	if (ite == m_partClasses.end())
	{
		GameWarning("Unknown Vehicle part: <%s>", name.c_str());
		return NULL;
	}

#if ENABLE_VEHICLE_DEBUG
	string partName = "Part:";
	partName += name;
	s_classInstanceCounts[partName]++;
#endif

	return (*ite->second)();
}

//------------------------------------------------------------------------
IVehicleDamageBehavior* CVehicleSystem::CreateVehicleDamageBehavior(const string& name)
{
	CRY_ASSERT(!name.empty());

	TVehicleDamageBehaviorClassMap::iterator ite = m_damageBehaviorClasses.find(name);

	if (ite == m_damageBehaviorClasses.end())
	{
		GameWarning("Unknown Vehicle damage behavior: <%s>", name.c_str());
		return NULL;
	}

#if ENABLE_VEHICLE_DEBUG
	string behaviorName = "DamageBehavior:";
	behaviorName += name;
	s_classInstanceCounts[behaviorName]++;
#endif

	return (*ite->second)();
}

//------------------------------------------------------------------------
IVehicleSeatAction* CVehicleSystem::CreateVehicleSeatAction(const string& name)
{
	CRY_ASSERT(!name.empty());

	TVehicleSeatActionClassMap::iterator ite = m_seatActionClasses.find(name);

	if (ite == m_seatActionClasses.end())
	{
		GameWarning("Unknown Vehicle seat action: <%s>", name.c_str());
		return NULL;
	}

#if ENABLE_VEHICLE_DEBUG
	string seatActionName = "SeatAction:";
	seatActionName += name;
	s_classInstanceCounts[seatActionName]++;
#endif

	return (*ite->second)();
}

//------------------------------------------------------------------------
IVehicleAction* CVehicleSystem::CreateVehicleAction(const string& name)
{
	CRY_ASSERT(!name.empty());

	TVehicleActionClassMap::iterator ite = m_actionClasses.find(name);

	if (ite == m_actionClasses.end())
	{
		GameWarning("Unknown Vehicle action: <%s>", name.c_str());
		return NULL;
	}

#if ENABLE_VEHICLE_DEBUG
	string actionName = "Action:";
	actionName += name;
	s_classInstanceCounts[actionName]++;
#endif

	return (*ite->second)();
}

//------------------------------------------------------------------------
void CVehicleSystem::GetVehicleImplementations(SVehicleImpls& impls)
{
	ICryPak* pack = gEnv->pCryPak;
	_finddata_t fd;
	int res;
	intptr_t handle;
	std::set<string> setVehicles;

	string mask(gXmlPath + string("*.xml"));

	if ((handle = pack->FindFirst(mask.c_str(), &fd)) != -1)
	{
		do
		{
			//PathUtil::RemoveExtension(vehicleName);
			if (XmlNodeRef root = GetISystem()->LoadXmlFromFile(gXmlPath + string(fd.name)))
			{
				const char* name = root->getAttr("name");
				if (name[0])
				{
					bool show = true;
					root->getAttr("show", show);
					if (show || VehicleCVars().v_show_all)
					{
						//CryLog("GetVehicleImpls: adding <%s>", name);
						std::pair<std::set<string>::iterator, bool> result = setVehicles.insert(string(name));
						if (result.second)
							impls.Add(name);
						else
							CryLog("Vehicle <%s> already registered (duplicate vehicle .xml files?)", name);
					}
				}
				else
					CryLog("VehicleSystem: %s is missing 'name' attribute, skipping", fd.name);
			}
			res = pack->FindNext(handle, &fd);
		}
		while (res >= 0);
		pack->FindClose(handle);
	}
}

//------------------------------------------------------------------------
bool CVehicleSystem::GetOptionalScript(const char* vehicleName, char* buf, size_t len)
{
	_finddata_t fd;
	intptr_t handle;

	cry_sprintf(buf, len, "%s%s.lua", gScriptPath, vehicleName);

	if ((handle = gEnv->pCryPak->FindFirst(buf, &fd)) != -1)
	{
		gEnv->pCryPak->FindClose(handle);

		return true;
	}

	return false;
}

//------------------------------------------------------------------------
TVehicleObjectId CVehicleSystem::AssignVehicleObjectId()
{
	return m_nextObjectId++;
}

//------------------------------------------------------------------------
TVehicleObjectId CVehicleSystem::AssignVehicleObjectId(const string& className)
{
	TVehicleObjectId id = AssignVehicleObjectId();
	RegisterVehicleObject(className, id);
	return id;
}

//------------------------------------------------------------------------
TVehicleObjectId CVehicleSystem::GetVehicleObjectId(const string& className) const
{
	TVehicleObjectIdMap::const_iterator cit = m_objectIds.find(className);
	bool objectIdFound = (cit != m_objectIds.end());
	if (objectIdFound)
	{
		return cit->second;
	}
	else
	{
		return InvalidVehicleObjectId;
	}
}

//------------------------------------------------------------------------
void CVehicleSystem::RegisterVehicleObject(const string& className, TVehicleObjectId id)
{
	m_objectIds.insert(TVehicleObjectIdMap::value_type(className, id));
}

//------------------------------------------------------------------------
void CVehicleSystem::Reset()
{
	if (GetISystem()->IsSerializingFile() == 1)
	{
		TVehicleMap::iterator it = m_vehicles.begin();
		IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
		for (; it != m_vehicles.end(); )
		{
			EntityId id = it->first;
			IEntity* pEntity = pEntitySystem->GetEntity(id);
			if (pEntity != NULL)
			{
				++it;
			}
			else
			{
				TVehicleMap::iterator eraseIt = it++;
				m_vehicles.erase(eraseIt);
			}
		}
	}

#if ENABLE_VEHICLE_DEBUG
	stl::free_container(s_classInstanceCounts);
#endif
	std::for_each(m_iteratorPool.begin(), m_iteratorPool.end(), DeleteVehicleIterator);
	stl::free_container(m_iteratorPool);
}

//------------------------------------------------------------------------
IVehicleIteratorPtr CVehicleSystem::CreateVehicleIterator()
{
	if (m_iteratorPool.empty())
	{
		return new CVehicleIterator(this);
	}
	else
	{
		CVehicleIterator* pIter = m_iteratorPool.back();
		m_iteratorPool.pop_back();
		new(pIter) CVehicleIterator(this);
		return pIter;
	}
}

//------------------------------------------------------------------------
void CVehicleSystem::RegisterVehicleClient(IVehicleClient* pVehicleClient)
{
	m_pVehicleClient = pVehicleClient;
}

template<class T>
static void AddFactoryMapTo(const std::map<string, T>& m, ICrySizer* s)
{
	s->AddContainer(m);
	for (typename std::map<string, T>::const_iterator iter = m.begin(); iter != m.end(); ++iter)
	{
		s->Add(iter->first);
	}
}

//------------------------------------------------------------------------
void CVehicleSystem::GetMemoryStatistics(ICrySizer* s)
{

	s->AddContainer(m_vehicles);
	AddFactoryMapTo(m_classes, s);
	AddFactoryMapTo(m_movementClasses, s);
	AddFactoryMapTo(m_viewClasses, s);
	AddFactoryMapTo(m_partClasses, s);
	AddFactoryMapTo(m_seatActionClasses, s);
	AddFactoryMapTo(m_actionClasses, s);
	AddFactoryMapTo(m_damageBehaviorClasses, s);
	AddFactoryMapTo(m_objectIds, s);
	s->AddContainer(m_iteratorPool);
	for (size_t i = 0; i < m_iteratorPool.size(); i++)
		s->Add(*m_iteratorPool[i]);

	// TODO: VehicleDamagesTemplateRegistry
}

//------------------------------------------------------------------------
void CVehicleSystem::RegisterVehicleUsageEventListener(const EntityId playerId, IVehicleUsageEventListener* pEventListener)
{
	TVehicleUsageEventListenerList::iterator it_playersListeners = m_eventListeners.find(playerId);
	if (it_playersListeners == m_eventListeners.end())
	{
		m_eventListeners[playerId] = TVehicleUsageListenerVec();
	}
	stl::push_back_unique(m_eventListeners[playerId], pEventListener);
}

//------------------------------------------------------------------------
void CVehicleSystem::UnregisterVehicleUsageEventListener(const EntityId playerId, IVehicleUsageEventListener* pEventListener)
{
	TVehicleUsageEventListenerList::iterator it_playersListeners = m_eventListeners.find(playerId);
	if (it_playersListeners == m_eventListeners.end())
	{
		return;
	}

	TVehicleUsageListenerVec& playerListener = it_playersListeners->second;

	stl::find_and_erase(playerListener, pEventListener);
	if (playerListener.empty())
	{
		m_eventListeners.erase(it_playersListeners);
	}
}

//------------------------------------------------------------------------
void CVehicleSystem::BroadcastVehicleUsageEvent(const EVehicleEvent eventId, const EntityId playerId, IVehicle* pVehicle)
{
	TVehicleUsageEventListenerList::iterator playerListeners_it = m_eventListeners.find(playerId);
	if (m_eventListeners.end() == playerListeners_it)
	{
		return;
	}

	TVehicleUsageListenerVec& playerListeners = playerListeners_it->second;
	TVehicleUsageListenerVec::iterator listenerInfo = playerListeners.begin();
	TVehicleUsageListenerVec::const_iterator end = playerListeners.end();
	switch (eventId)
	{
	case eVE_PassengerEnter:
		for (; listenerInfo != end; ++listenerInfo)
		{
			(*listenerInfo)->OnStartUse(playerId, pVehicle);
		}
		break;
	case eVE_PassengerExit:
		for (; listenerInfo != end; ++listenerInfo)
		{
			(*listenerInfo)->OnEndUse(playerId, pVehicle);
		}
		break;
	}
}

//------------------------------------------------------------------------
void CVehicleSystem::Update(float deltaTime)
{
#if ENABLE_VEHICLE_DEBUG
	if (VehicleCVars().v_debug_mem > 0)
	{
		float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		ICrySizer* pSizer = gEnv->pSystem->CreateSizer();

		GetMemoryStatistics(pSizer);
		for (TVehicleMap::iterator it = m_vehicles.begin(); it != m_vehicles.end(); ++it)
		{
			if (it->second)
				it->second->GetMemoryUsage(pSizer);
		}

		IRenderAuxText::Draw2dLabel(15.0f, 10.0f, 2.0f, color, false, "Vehicle system takes %" PRISIZE_T " bytes for its c++ side.", pSizer->GetTotalSize());

		pSizer->Release();
	}
	if (VehicleCVars().v_debug_flip_over)
	{
		for (TVehicleMap::iterator it = m_vehicles.begin(); it != m_vehicles.end(); ++it)
		{
			IVehicle* pVehicle = it->second;
			if (pVehicle->IsPlayerDriving())
			{
				static_cast<CVehicle*>(pVehicle)->DebugFlipOver();
			}
		}
		CVehicleCVars::Get().v_debug_flip_over = 0;
	}
	if (VehicleCVars().v_debug_reorient)
	{
		static ICVar* pDebugVehicle = gEnv->pConsole->GetCVar("v_debugVehicle");

		for (TVehicleMap::iterator it = m_vehicles.begin(); it != m_vehicles.end(); ++it)
		{
			IVehicle* pVehicle = it->second;
			if (pVehicle->IsPlayerDriving() || 0 == strcmpi(pDebugVehicle->GetString(), pVehicle->GetEntity()->GetName()))
			{
				static_cast<CVehicle*>(pVehicle)->DebugReorient();
			}
		}
		CVehicleCVars::Get().v_debug_reorient = 0;
	}
#endif
	IActor* pClientActor = CCryAction::GetCryAction()->GetClientActor();
	if (pClientActor)
	{
		m_pCurrentClientVehicle = pClientActor->GetLinkedVehicle();
	}
	else
	{
		m_pCurrentClientVehicle = NULL;
	}
}

//------------------------------------------------------------------------
// NOTE: This function must be thread-safe.
void CVehicleSystem::OnPrePhysicsTimeStep(float deltaTime)
{
	CryAutoCriticalSection lock(m_currentVehicleLock);
	CVehicle* pVehicle = (CVehicle*)m_pCurrentClientVehicle;
	if (pVehicle)
	{
		pVehicle->OnPrePhysicsTimeStep(deltaTime);
	}
}

#if ENABLE_VEHICLE_DEBUG
void CVehicleSystem::DumpClasses(IConsoleCmdArgs* pArgs)
{
	CVehicleSystem* pVS = static_cast<CVehicleSystem*>(CCryAction::GetCryAction()->GetIVehicleSystem());
	CryLogAlways("Vehicle classes and instance counts:");
	for (TVehicleClassCount::const_iterator it = s_classInstanceCounts.begin(); it != s_classInstanceCounts.end(); ++it)
	{
		CryLogAlways("%s : %d", it->first.c_str(), it->second);
	}
}
#endif
