// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DeathManager.h"
#include "Agent.h"
#include "GameCVars.h"
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IAIGroupProxy.h>
#include <CryAISystem/ITargetTrackManager.h>
#include <CryAISystem/VisionMapTypes.h>
#include <CryCore/functor.h>

//
//   Victim Detection
//
//   When an agent dies the DeathManager will collect group members that
//   witnessed the mate dying.  DeathManager does this by performing
//   deferred raycasts to see if any of them saw this happening.
//   It goes through, from closest to furthest, the members that have the
//   victim in their field-of-view.
//

#ifndef _RELEASE_
# define BUILD_WITH_DEATH_MANAGER_DEBUG_INFORMATION
#endif

namespace GameAI
{
	DeathManager::DeathManager()
		: m_asyncState(AsyncReady)
		, m_rayID(0)
	{
		assert(gEnv->pAISystem);
		gEnv->pAISystem->RegisterSystemComponent(this);

		gEnv->pAISystem->Callbacks().AgentDied().Add(functor(*this, &DeathManager::OnAgentDeath));
	}

	DeathManager::~DeathManager()
	{
		if (gEnv->pAISystem)
		{
			gEnv->pAISystem->Callbacks().AgentDied().Remove(functor(*this, &DeathManager::OnAgentDeath));
			gEnv->pAISystem->UnregisterSystemComponent(this);
		}
	}

	void DeathManager::GameUpdate()
	{
		ProcessDeferredDeathReactions();
	}

	void DeathManager::OnAgentGrabbedByPlayer(const EntityId agentID)
	{
		if (IEntity* pObjectEntity = gEnv->pEntitySystem->GetEntity(agentID))
		{
			if (IAIObject* pAI = pObjectEntity->GetAI())
			{
				if (IAIActor* pAIActor = pAI->CastToIAIActor())
				{
					gEnv->pAISystem->SendSignal(SIGNALFILTER_GROUPONLY_EXCEPT, AISIGNAL_DEFAULT, "GroupMemberGrabbedByPlayer", pAI);
				}
			}
		}
	}

	void DeathManager::AddDeferredDeathReactionFor(const Agent& deadAgent, const EntityId killerID)
	{
		IAISystem& aiSystem = *gEnv->pAISystem;

		EntityId closestMemberID = 0;
		EntityId closestWitnessMemberID = 0;
		float closestDistSq = FLT_MAX;
		float closestWitnessDistSq = FLT_MAX;

		int groupID = deadAgent.GetGroupID();
		int memberCount = aiSystem.GetGroupCount(groupID);
		if (memberCount == 0)
			return;

		DeferredDeathReaction deferredDeathReaction;

		// Find potential witness
		DeferredDeathReaction::PotentialWitnesses& potentialWitnesses = deferredDeathReaction.potentialWitnesses;

		const float idleWitnessRangeSq = square(20.0f);
		const float alertedWitnessRangeSq = square(80.0f);

		for (int memberIndex = 0; memberIndex < memberCount; ++memberIndex)
		{
			Agent member(aiSystem.GetGroupMember(groupID, memberIndex));

			if (member)
			{
				const float distSq = SquaredDistance(member, deadAgent);

				if (member.IsPointInFOV(deadAgent.GetPos()))
				{
					if (distSq < (member.GetAlertness() == 0 ? idleWitnessRangeSq : alertedWitnessRangeSq))
					{
						potentialWitnesses.insert(std::make_pair(distSq, member.GetEntityID()));
					}
				}

				if (distSq < closestDistSq)
				{
					closestDistSq = distSq;
					closestMemberID = member.GetEntityID();
				}
			}
		}

		deferredDeathReaction.groupID = groupID;
		deferredDeathReaction.timeStamp = gEnv->pTimer->GetCurrTime();
		deferredDeathReaction.deathPos = deadAgent.GetPos();
		deferredDeathReaction.victimID = deadAgent.GetEntityID();
		deferredDeathReaction.closestID = closestMemberID;
		deferredDeathReaction.witnessID = closestWitnessMemberID;
		deferredDeathReaction.killerID = killerID;
	
		m_deferredDeathReactions.push_back(deferredDeathReaction);
	}

	void DeathManager::ProcessDeferredDeathReactions()
	{
		DeferredDeathReactions::iterator it = m_deferredDeathReactions.begin();
		DeferredDeathReactions::iterator end = m_deferredDeathReactions.end();

		for ( ; it != end; ++it)
		{
			DeferredDeathReaction& ddr = *it;

			if (!(ddr.state & DeferredDeathReaction::ExtraFramesPassed))
			{
				// The extra frames was useful for detecting if the kill would
				// lead to the group getting a target. In Crysis 2 that meant
				// that a different search behavior would be used.
				// For Crysis 3, however, the reactions are governed by the
				// context of the behavior tree. I'll leave it here for a few
				// weeks to remind you that it can be done. /Jonas 2012-04-24

				//const float necessaryTimeInQueue = 1.0f;
				//float timeInQueue = gEnv->pTimer->GetCurrTime() - ddr.timeStamp;
				//if (timeInQueue > necessaryTimeInQueue)
					ddr.state = ddr.state | DeferredDeathReaction::ExtraFramesPassed;
			}

			if (ddr.state == DeferredDeathReaction::ReadyForDispatch)
			{
				DispatchDeferredDeathReaction(ddr);

				std::swap(*it, m_deferredDeathReactions.back());
				m_deferredDeathReactions.pop_back();
				return;
			}
		}

		if (m_asyncState == AsyncReady && !m_deferredDeathReactions.empty())
		{
			DeferredDeathReaction& ddr = m_deferredDeathReactions.front();

			if ((ddr.state & DeferredDeathReaction::DataCollected) == 0)
				QueueNextPotentialWitnessRay(ddr);
		}
	}

	void DeathManager::DispatchDeferredDeathReaction(const DeferredDeathReaction& ddr)
	{
		IAISystem& aiSystem = *gEnv->pAISystem;

		//
		// Trigger death reaction
		//

		// This arbitrary member is just as good as any in the group.
		// It's just used to get access to group information.
		Agent arbitraryMember(ddr.closestID);
		if (arbitraryMember)
		{
			if (IAIGroupProxy* group = aiSystem.GetAIGroupProxy(ddr.groupID))
			{
				InjectDeadGroupMemberDataIntoScriptTable(group->GetScriptTable(), ddr.victimID, ddr.killerID, ddr.deathPos);
				aiSystem.SendSignal(SIGNALFILTER_GROUPONLY, AISIGNAL_DEFAULT, "GroupMemberDied", arbitraryMember.GetAIObject());
			}
		}

		//
		// Feed information to the witness
		//

		Agent witness(ddr.witnessID);
		if (witness)
		{
			InjectDeadGroupMemberDataIntoScriptTable(witness.GetScriptTable(), ddr.victimID, ddr.killerID, ddr.deathPos);
			witness.SetSignal(AISIGNAL_DEFAULT, "WatchedMateDie");

			#ifdef BUILD_WITH_DEATH_MANAGER_DEBUG_INFORMATION
			if (g_pGameCVars->ai_DebugDeferredDeath)
			{
				gEnv->pLog->Log("DeathManager: Letting '%s' know he watched his mate die.", witness.GetName());
			}
			#endif

			// Let the witness sense the killer's presence
			Agent killer(ddr.killerID);
			if (killer)
			{
				#ifdef BUILD_WITH_DEATH_MANAGER_DEBUG_INFORMATION
				if (g_pGameCVars->ai_DebugDeferredDeath)
				{
					IPersistantDebug* debug = g_pGame->GetIGameFramework()->GetIPersistantDebug();
					debug->Begin("DebugDeferredDeath", false);
					debug->AddLine(witness.GetPos(), killer.GetPos(), Col_Orange, 4.0f);
					gEnv->pLog->Log("DeathManager: Letting '%s' sense mate's killer. See orange line.", witness.GetName());
				}
				#endif
			}
		}

		//
		// Add dead body if there was no witness
		//

		if (ddr.witnessID == 0)
		{
			Agent victim(ddr.victimID);

			if (victim)
			{
				AddDeadBodyFor(victim, ddr.killerID);
			}
		}
	}

	void DeathManager::AddDeadBodyFor(const Agent& deadMember, EntityId killerID)
	{
		m_unseenDeadBodies.push_back(DeadBody());
		InitDeadBody(m_unseenDeadBodies.back(), deadMember, killerID);
	}

	void DeathManager::CheckDeadBodyVisibilityFor(Agent& agent)
	{
		DeadBodies::iterator it = m_unseenDeadBodies.begin();
		DeadBodies::iterator end = m_unseenDeadBodies.end();

		for ( ; it != end; ++it)
		{
			DeadBody& deadBody = (*it);
			AgentParameters  agentParameters = agent.GetAIActor()->GetParameters();
			if(agent.GetPos().GetDistance(deadBody.position) > agentParameters.m_PerceptionParams.minDistanceToSpotDeadBodies)
				continue;

			if (agent.CanSee(deadBody.visionID) && deadBody.groupID == agent.GetGroupID())
			{
				// Notify
				#ifdef BUILD_WITH_DEATH_MANAGER_DEBUG_INFORMATION
				if (g_pGameCVars->ai_DebugDeferredDeath)
				{
					gEnv->pLog->Log("DeathManager: Letting '%s' know he spotted a dead body.", agent.GetName());
				}
				#endif

				InjectDeadGroupMemberDataIntoScriptTable(agent.GetScriptTable(), deadBody.entityID, deadBody.killerID, deadBody.position);
				agent.SetSignal(0, "SpottedDeadGroupMember");

				// Stimulate finder so that he gets scared of the body (with killer's ID)
				// This only happens if the agent doesn't have an attention target.
				// It simplifies things if we can assume there's an attention
				// target whenever we spot a dead body.
				if (!agent.GetAttentionTarget())
				{
					Agent killer(deadBody.killerID);
					if (killer)
					{
						#ifdef BUILD_WITH_DEATH_MANAGER_DEBUG_INFORMATION
						if (g_pGameCVars->ai_DebugDeferredDeath)
						{
							gEnv->pLog->Log("DeathManager: Letting '%s' get scared of the mate's body, but with the killer's ID.", agent.GetName());
						}
						#endif
					}
				}

				// Remove body
				DestroyDeadBody(deadBody);
				std::swap(*it, m_unseenDeadBodies.back());
				m_unseenDeadBodies.pop_back();
				return;
			}
		}
	}

	void DeathManager::ClearDeadBodiesForGroup(int groupID)
	{
		for (size_t i = 0; i < m_unseenDeadBodies.size(); )
		{
			DeadBody& deadBody = m_unseenDeadBodies[i];

			if (deadBody.groupID == groupID)
			{
				DestroyDeadBody(deadBody);
				std::swap(m_unseenDeadBodies[i], m_unseenDeadBodies.back());
				m_unseenDeadBodies.pop_back();
			}
			else
			{
				++i;
			}
		}
	}

	void DeathManager::ClearDeferredDeathReactionsForGroup(int groupID)
	{
		for (size_t i = 0; i < m_deferredDeathReactions.size(); )
		{
			const DeferredDeathReaction& ddr = m_deferredDeathReactions[i];

			if (ddr.groupID == groupID)
			{
				std::swap(m_deferredDeathReactions[i], m_deferredDeathReactions.back());
				m_deferredDeathReactions.pop_back();

				// Cancel deferred raycast result
				if (i == 0 && m_asyncState == AsyncInProgress)
				{
					assert(m_rayID != 0);

					m_asyncState = AsyncReady;
					g_pGame->GetRayCaster().Cancel(m_rayID);
					m_rayID = 0;

					if(!m_deferredDeathReactions.empty())
						QueueNextPotentialWitnessRay(m_deferredDeathReactions.front());
				}
			}
			else
			{
				++i;
			}
		}
	}

	void DeathManager::QueueNextPotentialWitnessRay(DeferredDeathReaction& ddr)
	{
		assert(m_rayID == 0);
		assert(m_asyncState == AsyncReady);

		DeferredDeathReaction::PotentialWitnesses& potentialWitnesses = ddr.potentialWitnesses;

		while (!potentialWitnesses.empty())
		{
			DeferredDeathReaction::PotentialWitnesses::iterator it = potentialWitnesses.begin();
			Agent agent(it->second);

			if (agent.IsValid())
			{
				PhysSkipList skipList;
				agent.GetPhysicalSkipEntities(skipList);

				const Vec3& from = agent.GetPos();
				const Vec3& to = ddr.deathPos;

				m_asyncState = AsyncInProgress;

				m_rayID = g_pGame->GetRayCaster().Queue(
					RayCastRequest::HighPriority,
					RayCastRequest(from, to - from,
					ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid,
					((geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any | (10 & rwi_pierceability_mask)),
					&skipList[0], skipList.size()),
					functor(*this, &DeathManager::PotentialWitnessRayComplete));

				if (g_pGameCVars->ai_DebugDeferredDeath)
				{
					IPersistantDebug* debug = g_pGame->GetIGameFramework()->GetIPersistantDebug();
					debug->Begin("DebugDeferredDeath", false);
					debug->AddLine(from, to, Col_Grey, 4.0f);
				}

				break;
			}
			potentialWitnesses.erase(it);
		}

		if (potentialWitnesses.empty())
		{
			ddr.state |= ddr.DataCollected;
		}
	}

	void DeathManager::InitDeadBody(DeadBody& deadBody, const Agent& agent, EntityId killerID)
	{
		const Vec3 deadBodyPosition = agent.GetEntityPos() + Vec3(0.0f, 0.0f, 0.5f);

		IVisionMap& visionMap = *gEnv->pAISystem->GetVisionMap();

		deadBody.position = deadBodyPosition;
		deadBody.groupID = agent.GetGroupID();
		deadBody.entityID = agent.GetEntityID();
		deadBody.killerID = killerID;
		deadBody.visionID = visionMap.CreateVisionID(agent.GetName());
	
		ObservableParams observableParams;
		observableParams.faction = 0;
		observableParams.typeMask = DeadAgent;
		observableParams.observablePositionsCount = 1;
		observableParams.observablePositions[0] = deadBodyPosition;
		visionMap.RegisterObservable(deadBody.visionID, observableParams);
	}

	void DeathManager::DestroyDeadBody(DeadBody& deadBody)
	{
		if (deadBody.visionID)
		{
			assert(gEnv->pAISystem);
			IVisionMap& visionMap = *gEnv->pAISystem->GetVisionMap();
			visionMap.UnregisterObservable(deadBody.visionID);
		}
	}

	void DeathManager::OnAgentDeath(EntityId deadEntityID, EntityId killerID)
	{
		Agent deadAgent(deadEntityID);
		
		if (deadAgent)
		{
			const bool lastOneInGroupJustDied = (gEnv->pAISystem->GetGroupCount(deadAgent.GetGroupID()) == 0);
			if (lastOneInGroupJustDied)
			{
				ClearDeadBodiesForGroup(deadAgent.GetGroupID());
				ClearDeferredDeathReactionsForGroup(deadAgent.GetGroupID());
			}
			else
			{
				AddDeferredDeathReactionFor(deadAgent, killerID);
			}
		}
	}

	void DeathManager::OnActorUpdate(IAIObject* pAIObject, IAIObject::EUpdateType type, float frameDelta)
	{
		Agent agent(pAIObject);
		if (agent)
		{
			CheckDeadBodyVisibilityFor(agent);
		}
	}

	void DeathManager::PotentialWitnessRayComplete(const QueuedRayID& rayID, const RayCastResult& result)
	{
		assert(m_asyncState == AsyncInProgress);
		assert(m_rayID == rayID);
		assert(!m_deferredDeathReactions.empty());

		m_asyncState = AsyncReady;
		m_rayID = 0;

		if (!m_deferredDeathReactions.empty())
		{
			DeferredDeathReaction& ddr = m_deferredDeathReactions.front();
			if(!ddr.potentialWitnesses.empty()) // May be possible that this has been cleared before this callback is fired
			{
				DeferredDeathReaction::PotentialWitnesses::iterator it = ddr.potentialWitnesses.begin();

				bool agentWitnessedDeath = !result;

				if (agentWitnessedDeath)
				{
					ddr.witnessID = it->second;
					ddr.state |= DeferredDeathReaction::DataCollected;
				}
				else
				{
					ddr.potentialWitnesses.erase(it);
					QueueNextPotentialWitnessRay(ddr);
				}
			}
		}
	}

	void DeathManager::InjectDeadGroupMemberDataIntoScriptTable(IScriptTable* scriptTable, const EntityId victimID, const EntityId killerID, const Vec3& victimPosition)
	{
		if (scriptTable)
		{
			IScriptTable* deadGroupMemberData = gEnv->pScriptSystem->CreateTable();
			deadGroupMemberData->AddRef();

			deadGroupMemberData->SetValue("killerID", ScriptHandle(killerID));
			deadGroupMemberData->SetValue("victimID", ScriptHandle(victimID));
			deadGroupMemberData->SetValue("deathPosition", victimPosition);
			deadGroupMemberData->SetValue("currentBodyPosition", victimPosition);
			
			scriptTable->SetValue("deadGroupMemberData", deadGroupMemberData);
			deadGroupMemberData->Release();
		}
	}
}
