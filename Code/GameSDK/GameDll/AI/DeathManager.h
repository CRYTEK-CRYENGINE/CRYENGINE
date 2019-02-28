// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef DeathManager_h
#define DeathManager_h

#include <CryEntitySystem/IEntity.h>
#include <CryAISystem/IVisionMap.h>
#include <CryAISystem/IAISystemComponent.h>

class Agent;

namespace GameAI
{
	// Contains information about a dead body that can be spotted by
	// other team members.  It is registered as an observable in the
	// vision map.
	struct DeadBody
	{
		DeadBody()
			: position(ZERO)
			, groupID(0)
			, entityID(0)
			, killerID(0)
		{
		}

		Vec3 position; // Entity position when killed, lifted up slightly from the ground
		int groupID;
		EntityId entityID;
		EntityId killerID;
		VisionID visionID;
	};

	// At the very moment a group member gets killed we do not yet
	// have all the data we need to pick the best reaction to it.
	// We must wait for stimulus to propagate through the systems
	// out to the agents.  Therefore we wait a few frames before
	// we gather data and react.
	struct DeferredDeathReaction
	{
		typedef std::multimap<float, EntityId> PotentialWitnesses;
		PotentialWitnesses potentialWitnesses;

		enum State
		{
			NotReady          = 0,
			DataCollected     = 1 << 0,
			ExtraFramesPassed = 1 << 1,
			ReadyForDispatch  = DataCollected | ExtraFramesPassed,
		};

		DeferredDeathReaction()
			: deathPos(ZERO)
			, victimID(0)
			, closestID(0)
			, witnessID(0)
			, killerID(0)
			, groupID(0)
			, timeStamp(0.0f)
			, state(NotReady)
		{
		}

		Vec3 deathPos;
		EntityId victimID;
		EntityId closestID;
		EntityId witnessID;
		EntityId killerID;
		int groupID;
		float timeStamp;
		uint8 state;
	};

	// When an agent dies, this manager gets notified. It then gathers
	// the necessary information about the situation and dispatches
	// this to the group so it can react accordingly.
	class DeathManager : public IAISystemComponent
	{
	public:
		DeathManager();
		virtual ~DeathManager();
		void GameUpdate();
		void OnAgentGrabbedByPlayer(const EntityId agentID);

	private:
		void ProcessDeferredDeathReactions();
		void AddDeferredDeathReactionFor(const Agent& deadAgent, const EntityId killerID);
		void DispatchDeferredDeathReaction(const DeferredDeathReaction& pdr);
		void AddDeadBodyFor(const Agent& deadMember, EntityId killerID);
		void CheckDeadBodyVisibilityFor(Agent& agent);
		void ClearDeadBodiesForGroup(int groupID);
		void ClearDeferredDeathReactionsForGroup(int groupID);
		void QueueNextPotentialWitnessRay(DeferredDeathReaction& ddr);
		void InitDeadBody(DeadBody& deadBody, const Agent& agent, EntityId killerID);
		void DestroyDeadBody(DeadBody& deadBody);
		void PotentialWitnessRayComplete(const QueuedRayID& rayID, const RayCastResult& result);
		void InjectDeadGroupMemberDataIntoScriptTable(IScriptTable* scriptTable, const EntityId victimID, const EntityId killerID, const Vec3& victimPosition);
		void OnAgentDeath(EntityId deadEntityID, EntityId killerID);

		// IAISystemComponent
		virtual void OnActorUpdate(IAIObject* pAIObject, IAIObject::EUpdateType type, float frameDelta);
		virtual bool WantActorUpdates(IAIObject::EUpdateType type) override { return type == IAIObject::Full; }
		// ~IAISystemComponent

	private:
		typedef std::vector<DeferredDeathReaction> DeferredDeathReactions;
		DeferredDeathReactions m_deferredDeathReactions;

		typedef std::vector<DeadBody> DeadBodies;
		DeadBodies m_unseenDeadBodies;

		AsyncState m_asyncState;
		QueuedRayID m_rayID;
	};
}

#endif // DeathManager_h
