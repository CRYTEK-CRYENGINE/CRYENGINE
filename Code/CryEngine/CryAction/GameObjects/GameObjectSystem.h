// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GAMEOBJECTSYSTEM_H__
#define __GAMEOBJECTSYSTEM_H__

#pragma once

// FIXME: Cell SDK GCC bug workaround.
#ifndef __IGAMEOBJECTSYSTEM_H__
	#include "IGameObjectSystem.h"
#endif

#include "GameObjectDispatch.h"
#include <vector>
#include <map>

class CGameObjectSystem : public IGameObjectSystem
{
public:
	bool                                               Init();

	IGameObjectSystem::ExtensionID                     GetID(const char* name) override;
	const char*                                        GetName(IGameObjectSystem::ExtensionID id) override;
	uint32                                             GetExtensionSerializationPriority(IGameObjectSystem::ExtensionID id) override;
	IGameObjectExtension*                              Instantiate(IGameObjectSystem::ExtensionID id, IGameObject* pObject) override;
	virtual void                                       RegisterExtension(const char* szName, IGameObjectExtensionCreatorBase* pCreator, IEntityClassRegistry::SEntityClassDesc* pClsDesc) override;
	virtual void                                       RegisterSchedulingProfile(const char* szEntityClassName, const char* szNormalPolicy, const char* szOwnedPolicy) override;
	virtual void                                       DefineProtocol(bool server, IProtocolBuilder* pBuilder) override;
	virtual void                                       BroadcastEvent(const SGameObjectEvent& evt) override;

	virtual void                                       RegisterEvent(uint32 id, const char* name) override;
	virtual uint32                                     GetEventID(const char* name) override;
	virtual const char*                                GetEventName(uint32 id) override;

	virtual IGameObject*                               CreateGameObjectForEntity(EntityId entityId) override;
	virtual IEntityComponent*                          CreateGameObjectEntityProxy(IEntity& entity, IGameObject** pGameObject = NULL) override;

	virtual void                                       PostUpdate(float frameTime) override;
	virtual void                                       SetPostUpdate(IGameObject* pGameObject, bool enable) override;

	virtual void                                       Reset() override;

	const SEntitySchedulingProfiles*                   GetEntitySchedulerProfiles(IEntity* pEnt);

	void                                               RegisterFactories(IGameFramework* pFW);

	IEntity*                                           CreatePlayerProximityTrigger();
	ILINE IEntityClass*                                GetPlayerProximityTriggerClass() { return m_pClassPlayerProximityTrigger; }
	ILINE std::vector<IGameObjectSystem::ExtensionID>* GetActivatedExtensionsTop()      { return &m_activatedExtensions_top; }

	void                                               GetMemoryUsage(ICrySizer* s) const;

	virtual void                                       AddSink(IGameObjectSystemSink* pSink) override;
	virtual void                                       RemoveSink(IGameObjectSystemSink* pSink) override;
private:
	void                                               LoadSerializationOrderFile();
	
	std::map<string, ExtensionID> m_nameToID;

	struct SExtensionInfo
	{
		string                           name;
		uint32                           serializationPriority; // lower values is higher priority
		IGameObjectExtensionCreatorBase* pFactory;

		void                             GetMemoryUsage(ICrySizer* pSizer) const
		{
			pSizer->AddObject(name);
		}
	};
	std::vector<SExtensionInfo> m_extensionInfo;

	static IEntityComponent* CreateGameObjectWithPreactivatedExtension(
	  IEntity* pEntity, SEntitySpawnParams& params, void* pUserData);

	CGameObjectDispatch       m_dispatch;

	std::vector<IGameObject*> m_postUpdateObjects;
	bool                      m_isPostUpdating = false;

	IEntityClass*             m_pClassPlayerProximityTrigger;

	typedef std::map<string, SEntitySchedulingProfiles> TSchedulingProfiles;
	TSchedulingProfiles       m_schedulingParams;
	SEntitySchedulingProfiles m_defaultProfiles;

	// event ID management
	std::map<string, uint32> m_eventNameToID;
	std::map<uint32, string> m_eventIDToName;
	//
	typedef std::list<IGameObjectSystemSink*> SinkList;
	SinkList                                    m_lstSinks; // registered sinks get callbacks

	std::vector<IGameObjectSystem::ExtensionID> m_activatedExtensions_top;
	std::vector<string>                         m_serializationOrderList; // defines serialization order for extensions
};

#endif
