// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ActorLookUp_h__
#define __ActorLookUp_h__

#pragma once

#include <CryAISystem/IActorLookUp.h>
#include "AIActor.h"

template<> struct ActorLookUpCastHelper<CAIObject>
{
	static CAIObject* Cast(IAIActor* ptr) { return static_cast<CAIActor*>(ptr); }
};

template<> struct ActorLookUpCastHelper<CAIActor>
{
	static CAIActor* Cast(IAIActor* ptr) { return  static_cast<CAIActor*>(ptr); }
};

template<> struct ActorLookUpCastHelper<CPipeUser>
{
	static CPipeUser* Cast(IAIActor* ptr) { return static_cast<CAIActor*>(ptr)->CastToCPipeUser(); }
};

template<> struct ActorLookUpCastHelper<CPuppet>
{
	static CPuppet* Cast(IAIActor* ptr) { return static_cast<CAIActor*>(ptr)->CastToCPuppet(); }
};

class ActorLookUp : public IActorLookUp
{
public:
	virtual size_t GetActiveCount() const override
	{
		return m_actors.size();
	}

	virtual IAIActorProxy* GetProxy(uint32 index) const override
	{
		return m_proxies[index];
	}

	virtual const Vec3& GetPosition(uint32 index) const override
	{
		return m_positions[index];
	}

	virtual EntityId GetEntityID(uint32 index) const override
	{
		return m_entityIDs[index];
	}

	virtual void UpdatePosition(CAIActor* actor, const Vec3& position) override
	{
		size_t activeActorCount = GetActiveCount();

		for (size_t i = 0; i < activeActorCount; ++i)
		{
			if (m_actors[i] == actor)
			{
				m_positions[i] = position;
				return;
			}
		}
	}

	virtual void UpdateProxy(CAIActor* actor) override
	{
		size_t activeActorCount = GetActiveCount();

		for (size_t i = 0; i < activeActorCount; ++i)
		{
			if (m_actors[i] == actor)
			{
				m_proxies[i] = actor->GetProxy();
				return;
			}
		}
	}

	virtual void Prepare(uint32 lookUpFields) override
	{
		if (!m_actors.empty())
		{
			CryPrefetch(&m_actors[0]);

			if (lookUpFields & Proxy)
			{
				CryPrefetch(&m_proxies[0]);
			}

			if (lookUpFields & Position)
			{
				CryPrefetch(&m_positions[0]);
			}

			if (lookUpFields & EntityID)
			{
				CryPrefetch(&m_entityIDs[0]);
			}
		}
	}

	virtual void AddActor(CAIActor* actor) override
	{
		assert(actor);

		size_t activeActorCount = GetActiveCount();

		for (uint32 i = 0; i < activeActorCount; ++i)
		{
			if (m_actors[i] == actor)
			{
				m_proxies[i] = actor->GetProxy();
				m_positions[i] = actor->GetPos();
				m_entityIDs[i] = actor->GetEntityID();

				return;
			}
		}

		m_actors.push_back(actor);
		m_proxies.push_back(actor->GetProxy());
		m_positions.push_back(actor->GetPos());
		m_entityIDs.push_back(actor->GetEntityID());
	}

	virtual void RemoveActor(CAIActor* actor) override
	{
		assert(actor);

		size_t activeActorCount = GetActiveCount();

		for (size_t i = 0; i < activeActorCount; ++i)
		{
			if (m_actors[i] == actor)
			{
				std::swap(m_actors[i], m_actors.back());
				m_actors.pop_back();

				std::swap(m_proxies[i], m_proxies.back());
				m_proxies.pop_back();

				std::swap(m_positions[i], m_positions.back());
				m_positions.pop_back();

				std::swap(m_entityIDs[i], m_entityIDs.back());
				m_entityIDs.pop_back();

				return;
			}
		}
	}

private:
	virtual IAIActor* GetActorInternal(uint32 index) const override
	{
		if (index >= m_actors.size())
			return nullptr;

		return m_actors[index];
	}

	typedef std::vector<CAIActor*> Actors;
	Actors m_actors;

	typedef std::vector<IAIActorProxy*> Proxies;
	Proxies m_proxies;

	typedef std::vector<Vec3> Positions;
	Positions m_positions;

	typedef std::vector<EntityId> EntityIDs;
	EntityIDs m_entityIDs;
};

#endif
