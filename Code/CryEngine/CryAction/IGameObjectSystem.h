// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IGAMEOBJECTSYSTEM_H__
#define __IGAMEOBJECTSYSTEM_H__

#pragma once

#include <CryEntitySystem/IEntityClass.h>
#include <CryGame/IGameFramework.h>
#include <CryNetwork/INetwork.h>

enum EGameObjectEventFlags
{
	eGOEF_ToScriptSystem     = 0x0001,
	eGOEF_ToGameObject       = 0x0002,
	eGOEF_ToExtensions       = 0x0004,
	eGOEF_LoggedPhysicsEvent = 0x0008, // was this a logged or immediate physics event

	eGOEF_ToAll              = eGOEF_ToScriptSystem | eGOEF_ToGameObject | eGOEF_ToExtensions
};

struct IGameObject;
struct SGameObjectEvent;

struct IGameObjectBoxListener
{
	virtual ~IGameObjectBoxListener(){}
	virtual void OnEnter(int id, EntityId entity) = 0;
	virtual void OnLeave(int id, EntityId entity) = 0;
	virtual void OnRemoveParent() = 0;
};

// Description:
//		A callback interface for a class that wants to be aware when new game objects are being spawned. A class that implements
//		this interface will be called every time a new game object is spawned.
struct IGameObjectSystemSink
{
	// This callback is called after this game object is initialized.
	virtual void OnAfterInit(IGameObject* object) = 0;
	virtual ~IGameObjectSystemSink(){}
};

struct IGameObjectSystem
{
	virtual ~IGameObjectSystem(){}
	// If this is set as the user data for a GameObject with Preactivated Extension
	// spawn, then it will be called back to provide additional initialization.
	struct SEntitySpawnParamsForGameObjectWithPreactivatedExtension
	{
		// If the user wants to extend this spawn parameters using this as a base class,
		// make sure to override 'm_type' member with your own typeID starting at 'eSpawnParamsType_Custom'
		enum EType
		{
			eSpawnParamsType_Default = 0,
			eSpawnParamsType_Custom,
		};

		bool  (* hookFunction)(IEntity* pEntity, IGameObject*, void* pUserData);
		void* pUserData;

		SEntitySpawnParamsForGameObjectWithPreactivatedExtension()
			: m_type(eSpawnParamsType_Default)
			, pUserData(nullptr)
			, hookFunction(nullptr)
		{
		}

		uint32 IsOfType(const uint32 type) const { return (m_type == type); };

	protected:
		uint32 m_type;
	};

	typedef uint16                  ExtensionID;
	static const ExtensionID InvalidExtensionID = ~ExtensionID(0);
	typedef IGameObjectExtension*(* GameObjectExtensionFactory)();

	virtual IGameObjectSystem::ExtensionID GetID(const char* name) = 0;
	virtual const char*                    GetName(IGameObjectSystem::ExtensionID id) = 0;
	virtual uint32                         GetExtensionSerializationPriority(IGameObjectSystem::ExtensionID id) = 0;
	virtual IGameObjectExtension*          Instantiate(IGameObjectSystem::ExtensionID id, IGameObject* pObject) = 0;
	virtual void                           BroadcastEvent(const SGameObjectEvent& evt) = 0;

	static const uint32                    InvalidEventID = ~uint32(0);
	virtual void              RegisterEvent(uint32 id, const char* name) = 0;
	virtual uint32            GetEventID(const char* name) = 0;
	virtual const char*       GetEventName(uint32 id) = 0;

	virtual IGameObject*      CreateGameObjectForEntity(EntityId entityId) = 0;
	virtual IEntityComponent* CreateGameObjectEntityProxy(IEntity& entity, IGameObject** ppGameObject = NULL) = 0;

	virtual void            RegisterExtension(const char* szName, IGameObjectExtensionCreatorBase* pCreator, IEntityClassRegistry::SEntityClassDesc* pEntityCls) = 0;
	virtual void            RegisterSchedulingProfile(const char* szEntityClassName, const char* szNormalPolicy, const char* szOwnedPolicy) = 0;
	virtual void            DefineProtocol(bool server, IProtocolBuilder* pBuilder) = 0;

	virtual void              PostUpdate(float frameTime) = 0;
	virtual void              SetPostUpdate(IGameObject* pGameObject, bool enable) = 0;

	virtual void              Reset() = 0;

	virtual void              AddSink(IGameObjectSystemSink* pSink) = 0;
	virtual void              RemoveSink(IGameObjectSystemSink* pSink) = 0;
};

// Summary
//   Structure used to define a game object event
struct SGameObjectEvent
{
	SGameObjectEvent(uint32 event, uint16 flags, IGameObjectSystem::ExtensionID target = IGameObjectSystem::InvalidExtensionID, void* _param = 0)
	{
		this->event = event;
		this->target = target;
		this->flags = flags;
		this->ptr = 0;
		this->param = _param;
	}
	uint32                         event;
	IGameObjectSystem::ExtensionID target;
	uint16                         flags;
	void*                          ptr;
	// optional parameter of event (ugly)
	union
	{
		void* param;
		bool  paramAsBool;
	};
};

#endif
