// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef GameAIHelpers_h
#define GameAIHelpers_h


#include "IGameAIModule.h"

// For an overview of the GameAISystem take a look in GameAISystem.cpp

struct IAISignalExtraData;



class CGameAIInstanceBase
{
public:
	CGameAIInstanceBase() : m_entityID(0) {}
	CGameAIInstanceBase(EntityId entityID);
	void Init(EntityId entityID);
	void Destroy() {}
	void Update(float frameTime) {}
	void SendSignal(const char* signal, IAISignalExtraData* data = NULL);
	void SendSignal(const char* signal, IAISignalExtraData* data, int nSignalID);
	IEntity* GetEntity() const { return gEnv->pEntitySystem->GetEntity(m_entityID); }
	EntityId GetEntityID() const { return m_entityID; }

#ifndef RELEASE
	const char* GetDebugEntityName() const { return m_debugEntityName.c_str(); }
	CGameAIInstanceBase(const CGameAIInstanceBase& rhs)
		: m_entityID(rhs.m_entityID)
		, m_debugEntityName(rhs.m_debugEntityName)
	{}
#else
	const char* GetDebugEntityName() const { return "NoNameInRelease"; }
	CGameAIInstanceBase(const CGameAIInstanceBase& rhs)
		: m_entityID(rhs.m_entityID)
	{}
#endif

protected:
	EntityId m_entityID;

#ifndef RELEASE
	string m_debugEntityName;
#endif
};



struct InstanceID
{
	InstanceID() : id(std::numeric_limits<uint16>::max()) {}
	InstanceID(const InstanceID& other) : id(other.id) {}

	operator uint16 () const { return id; }

	static InstanceID Invalid()
	{
		return InstanceID();
	}

	InstanceID& operator = (uint16 _id)
	{
		id = _id;
		return *this;
	}

	uint16 id;
};



template <class Instance>
struct InstanceInitContext
{
	InstanceInitContext(Instance* _instance, InstanceID _instanceID, IEntity* _entity, EntityId _entityID)
		: instance(*_instance)
		, instanceID(_instanceID)
		, entity(*_entity)
		, entityID(_entityID)
	{
		assert(_instance);
		assert(_instanceID != InstanceID::Invalid());
		assert(_entity);
		assert(_entityID != 0);
	}

	IEntity& entity;
	Instance& instance;
	EntityId entityID;
	InstanceID instanceID;
};



template <class Module, class Instance, uint32 NumPreallocatedInstances, uint32 GrowSize = 8>
class AIModule : public IGameAIModule
{
public:
	typedef AIModule<Module, Instance, NumPreallocatedInstances, GrowSize> BaseClass;
	typedef Instance InstanceType;
	typedef std::unordered_map<EntityId, InstanceID, stl::hash_uint32> Instances;

	AIModule()
	{
		m_pool.reserve(NumPreallocatedInstances * sizeof(Instance));
	}

	virtual void EntityEnter(EntityId entityID)
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);
		if (IEntity* entity = gEnv->pEntitySystem->GetEntity(entityID))
		{
			EntityLeave(entityID);


			Instance* instance = NULL;
			InstanceID instanceID = AllocateInstance(entityID, &instance);
			InitializeInstance(InstanceInitContext<Instance>(instance, instanceID, entity, entityID));
			m_running->insert(std::make_pair(entityID, instanceID));
		}
		else
		{
#ifndef RELEASE
			string message;
			message.Format(
				"GameAISystem: Entity with ID %d doesn't exist in the entity system and therefore failed to enter module '%s'",
				entityID, GetName());
			CRY_ASSERT_MESSAGE(0, message.c_str());
			GameWarning("%s", message.c_str());
#endif
		}
	}

	virtual void EntityLeave(EntityId entityID)
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

 		EntityLeaveFrom(*m_running, entityID);
 		EntityLeaveFrom(*m_paused, entityID);
	}

	virtual void EntityPause(EntityId entityID)
	{
		typename Instances::iterator it = m_running->find(entityID);
		if (it != m_running->end())
		{
			m_paused->insert(*it);
			m_running->erase(it);
		}
	}

	virtual void EntityResume(EntityId entityID)
	{
		typename Instances::iterator it = m_paused->find(entityID);
		if (it != m_paused->end())
		{
			m_running->insert(*it);
			m_paused->erase(it);
		}
	}

	virtual void Reset(bool bUnload)
	{
		LeaveAllInstances();
		stl::free_container(m_pool);

		if (bUnload)
		{
			m_running.reset();
			m_paused.reset();
			m_free.reset();
		}
		else
		{
			m_running.reset(new Instances());
			m_paused.reset(new Instances());
			m_free.reset(new FreeInstances());
		}
	}

	Instance* GetRunningInstance(EntityId entityID)
	{
		typename Instances::iterator it = m_running->find(entityID);

		if (it == m_running->end())
			return NULL;

		InstanceID instanceID = it->second;
		return &((Instance*)&m_pool[0])[instanceID];
	}

	Instance* GetInstanceFromID(InstanceID id)
	{
		uint32 offset = id * sizeof(Instance);
		if ((offset + sizeof(Instance)) <= m_pool.size())
		{
			return (Instance*)&m_pool[offset];
		}
		else
		{
			assert(0);
			return 0;
		}
	}

protected:
	std::unique_ptr<Instances> m_running;
	std::unique_ptr<Instances> m_paused;

private:
	AIModule(const AIModule<Module, Instance, NumPreallocatedInstances, GrowSize>&);
	AIModule<Module, Instance, NumPreallocatedInstances, GrowSize>& operator = (const AIModule<Module, Instance, NumPreallocatedInstances, GrowSize>&);

private:
	InstanceID AllocateInstance(EntityId entityID, Instance** instanceOut)
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		Instance* instance = NULL;
		InstanceID instanceID;

		if (m_free->empty())
		{
			if (m_pool.size() == m_pool.capacity())
			{
				size_t oldInstanceCount = m_pool.size() / sizeof(Instance);
				size_t newInstanceCount = oldInstanceCount + GrowSize;

				std::vector<char> newPool;
				newPool.reserve(newInstanceCount * sizeof(Instance));

				if (oldInstanceCount > 0)
				{
					newPool.resize(oldInstanceCount * sizeof(Instance));

					// Copy-construct elements in new pool with elements from old pool
					Instance* oldInstance = (Instance*)&m_pool[0];
					Instance* newInstance = (Instance*)&newPool[0];
					for (uint32 i = 0; i < oldInstanceCount; ++i)
					{
						new (newInstance++) Instance(*oldInstance);
						(oldInstance++)->~Instance();
					}
				}

				// Swap
				m_pool.swap(newPool);
			}

			instanceID = static_cast<uint16> (m_pool.size() / sizeof(Instance));
			m_pool.resize(m_pool.size() + sizeof(Instance));
		}
		else
		{
			instanceID = m_free->front();
			m_free->pop_front();
		}

		instance = GetInstanceFromID(instanceID);
		new (instance) Instance();

		if (instanceOut)
			*instanceOut = instance;

		return instanceID;
	}

	void DeallocateInstance(InstanceID instanceID)
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		Instance* instance = GetInstanceFromID(instanceID);

		assert(instance);

		if (instance)
		{
			instance->~Instance();
			m_free->push_back(instanceID);
		}
	}

	virtual void InitializeInstance(const InstanceInitContext<Instance>& context)
	{
		Instance* instance = GetInstanceFromID(context.instanceID);

		// In this default implementation of InitializeInstance we
		// call Init on the instance itself. This may be replaced
		// in any module that handles it differently.
		assert(instance);

		if (instance)
		{
			instance->Init(context.entityID);
		}
	}

	virtual void DeinitializeInstance(InstanceID instanceID)
	{
		Instance* instance = GetInstanceFromID(instanceID);

		// In this default implementation of InitializeInstance we
		// call Destroy on the instance itself. This may be replaced
		// in any module that handles it differently.
		assert(instance);

		if (instance)
		{
			instance->Destroy();
		}
	}

	void EntityLeaveFrom(Instances& instances, EntityId entityID)
	{
		typename Instances::iterator it = instances.find(entityID);
		if (it != instances.end())
		{
			InstanceID instanceID = it->second;
			DeinitializeInstance(instanceID);
			DeallocateInstance(instanceID);
			instances.erase(it);
		}
	}

	void LeaveAllInstances()
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		typename Instances::iterator it;
		typename Instances::iterator end;

		if (m_running.get())
		{
			it = m_running->begin();
			end = m_running->end();
			for ( ; it != end; ++it)
			{
				InstanceID instanceID = it->second;

				DeinitializeInstance(instanceID);
				DeallocateInstance(instanceID);
			}

			m_running->clear();
		}

		if (m_paused.get())
		{
			it = m_paused->begin();
			end = m_paused->end();
			for ( ; it != end; ++it)
			{
				InstanceID instanceID = it->second;

				DeinitializeInstance(instanceID);
				DeallocateInstance(instanceID);
			}

			m_paused->clear();
		}
	}

	typedef std::deque<InstanceID> FreeInstances;
	std::unique_ptr<FreeInstances> m_free;

	typedef std::vector<char> InstancePool;
	InstancePool m_pool;
};



template <class Module, class Instance, uint32 NumPreallocatedInstances, uint32 GrowSize = 8>
class AIModuleWithInstanceUpdate : public AIModule<Module, Instance, NumPreallocatedInstances, GrowSize>
{
public:
	typedef typename AIModule<Module, Instance, NumPreallocatedInstances, GrowSize>::BaseClass BaseClass;
	typedef typename BaseClass::Instances Instances;

protected:
	//	In, out:	The instance that is updated.
	//	In:			The amount of time that has elapsed since the last game loop update (>= 0.0)
	//				(in seconds).
	virtual void UpdateInstance(Instance& instance, float frameTime)
	{
		instance.Update(frameTime);
	}

private:
	virtual void Update(float frameTime)
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		if (BaseClass::m_running.get())
		{
			typename Instances::iterator it = BaseClass::m_running->begin();
			typename Instances::iterator end = BaseClass::m_running->end();

			for ( ; it != end; ++it)
			{
				InstanceID instanceID = it->second;

				Instance* instance = BaseClass::GetInstanceFromID(instanceID);

				assert(instance);

				if (instance)
				{
					UpdateInstance(*instance, frameTime);
				}
			}
		}
	}
};

#endif // GameAIHelpers_h
