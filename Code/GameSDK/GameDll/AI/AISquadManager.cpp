// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AISquadManager.h"
#include <CryAISystem/IAISystem.h>
#include "Agent.h"
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include "GameCVars.h"
#include <CryAISystem/BehaviorTree/Decorator.h>
#include <CryAISystem/BehaviorTree/Action.h>
#include "GameAISystem.h"

namespace
{
	using namespace BehaviorTree;

	class SquadScope : public Decorator
	{
		typedef Decorator BaseClass;

	public:
		struct RuntimeData
		{
			bool isInScope;

			RuntimeData() : isInScope(false) {}
		};
		
		virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
		{
			const stack_string name = xml->getAttr("name");
			if (name.empty())
			{
				gEnv->pLog->LogError("Missing 'name' attribute for SquadScope behavior tree node, at line %d.", xml->getLine());
				return LoadFailure;
			}

			m_scopeID = SquadScopeID(name.c_str());

			m_allowedConcurrentUsers = 1;
			xml->getAttr("allowedConcurrentUsers", m_allowedConcurrentUsers);

			return LoadChildFromXml(xml, context);
		}

	protected:
		virtual Status Update(const UpdateContext& context) override
		{
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
			if(EnterScope(context.entityId, runtimeData))
			{
				return m_child->Tick(context);
			}
			return Failure;
		}

		virtual void OnInitialize(const UpdateContext& context) override
		{
			BaseClass::OnInitialize(context);
			
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
			runtimeData.isInScope = false;
		}

		virtual void OnTerminate(const UpdateContext& context) override
		{
			BaseClass::OnTerminate(context);

			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
			LeaveScope(context.entityId, runtimeData);
		}

	private:
		bool EnterScope(const EntityId entityId, RuntimeData& runtimeData)
		{
			if(!runtimeData.isInScope)
				runtimeData.isInScope = g_pGame->GetGameAISystem()->GetAISquadManager().EnterSquadScope(m_scopeID, entityId, m_allowedConcurrentUsers);

			return runtimeData.isInScope;
		}

		void LeaveScope(EntityId entityId, RuntimeData& runtimeData)
		{
			if(runtimeData.isInScope)
				g_pGame->GetGameAISystem()->GetAISquadManager().LeaveSquadScope(m_scopeID, entityId);
		}

		uint32 m_allowedConcurrentUsers;
		SquadScopeID m_scopeID;
	};

	class SendSquadEvent : public Action
	{
	public:
		struct RuntimeData
		{
		};
		
		virtual LoadResult LoadFromXml(const XmlNodeRef& node, const LoadContext& context) override
		{
			m_event = node->getAttr("name");
			return LoadSuccess;
		}

	protected:
		virtual Status Update(const UpdateContext& context) override
		{
			g_pGame->GetGameAISystem()->GetAISquadManager().SendSquadEvent(context.entityId, m_event);
			return Success;
		}

	private:
		string m_event;
	};

	class IfSquadCount : public Decorator
	{
	public:
		typedef Decorator BaseClass;

		struct RuntimeData
		{
			bool gateIsOpen;

			RuntimeData() : gateIsOpen(false) {}
		};
		
		virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
		{
			IF_UNLIKELY(xml->getNumAttributes() != 1)
			{
				gEnv->pLog->LogError("IfSquadCount nodes must have only one attribute. The node at line %d has %d.", xml->getLine(), xml->getNumAttributes());
				return LoadFailure;
			}

			if(xml->getAttr("isGreaterThan", m_value))
			{
				m_ifType = GreaterThan;
			}
			else if(xml->getAttr("isLesserThan", m_value))
			{
				m_ifType = LesserThan;
			}
			else if(xml->getAttr("equals", m_value))
			{
				m_ifType = Equals;
			}
			else
			{
				gEnv->pLog->LogError("IfSquadCount nodes must contain an attribute of type 'greaterThan', 'lesserThan' or 'equals'. The node at line %d has does not.", xml->getLine());
				return LoadFailure;
			}

			return LoadChildFromXml(xml, context);
		}

	protected:
		virtual void OnInitialize(const UpdateContext& context) override
		{
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
			
			runtimeData.gateIsOpen = false;

			AISquadManager& squadManager = g_pGame->GetGameAISystem()->GetAISquadManager();
			const SquadId squadId = squadManager.GetSquadIdForAgent(context.entityId);
			const uint32 squadCount = squadManager.GetSquadMemberCount(squadId);

			switch (m_ifType)
			{
			case GreaterThan:
				{
					if (squadCount > m_value)
						runtimeData.gateIsOpen = true;
				}
				break;
			case LesserThan:
				{
					if (squadCount < m_value)
						runtimeData.gateIsOpen = true;
				}
				break;
			case Equals:
				{
					if (squadCount == m_value)
						runtimeData.gateIsOpen = true;
				}
				break;
			}
		}

		virtual Status Update(const UpdateContext& context) override
		{
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

			if (runtimeData.gateIsOpen)
				return BaseClass::Update(context);
			else
				return Failure;
		}

	private:
		enum IfType
		{
			GreaterThan,
			LesserThan, 
			Equals,
		};

		uint32 m_value;
		IfType m_ifType;
	};

	void RegisterBehaviorTreeNodes_AISquad()
	{
		assert( gEnv->pAISystem->GetIBehaviorTreeManager() );

		BehaviorTree::IBehaviorTreeManager& manager = *gEnv->pAISystem->GetIBehaviorTreeManager();

		REGISTER_BEHAVIOR_TREE_NODE(manager, SquadScope);
		REGISTER_BEHAVIOR_TREE_NODE(manager, SendSquadEvent);
		REGISTER_BEHAVIOR_TREE_NODE(manager, IfSquadCount);
	}
}

AISquadManager::AISquadManager()
:m_timeAccumulator(g_pGameCVars->ai_SquadManager_UpdateTick)
,m_totalSquads(0)
{
	RegisterBehaviorTreeNodes_AISquad();
}

AISquadManager::~AISquadManager()
{
	stl::free_container(m_registeredAgents);
};

void AISquadManager::RegisterAgent( const EntityId entityID )
{
	if(m_registeredAgents.find(entityID) == m_registeredAgents.end())
		m_registeredAgents.insert(std::pair<EntityId, SquadAgent>(entityID, SquadAgent(entityID)));
}

void AISquadManager::UnregisterAgent( const EntityId entityID )
{
	m_registeredAgents.erase(entityID);
}

bool AISquadManager::EnterSquadScope( const SquadScopeID& scopeId, const EntityId entityID, const uint32 concurrentUsersAllowed )
{
	SquadAgentMap::iterator registeredAgentIt = m_registeredAgents.find(entityID);
	if(registeredAgentIt != m_registeredAgents.end())
	{ 
		SquadAgent& squadAgent = registeredAgentIt->second;
		const uint32 currentSquadMatesInScope = GetCurrentSquadMatesInScope(squadAgent.squadID, scopeId);
		if(currentSquadMatesInScope < concurrentUsersAllowed )
		{
			squadAgent.enteredScopes.push_back(scopeId);
			return true;
		}
	}
	return false;
}

uint32 AISquadManager::GetCurrentSquadMatesInScope(const SquadId squadId, const SquadScopeID& squadScopeId) const
{
	uint32 currentUsersInScopeForSquad = 0;
	SquadAgentMap::const_iterator registeredAgentIt = m_registeredAgents.begin();
	for(; registeredAgentIt != m_registeredAgents.end(); ++registeredAgentIt)
	{ 
		const SquadAgent& squadAgent = registeredAgentIt->second;
		if(squadAgent.squadID == squadId && squadAgent.IsInScope(squadScopeId))
		{
			++currentUsersInScopeForSquad;
		}
	}
	return currentUsersInScopeForSquad;
}

void AISquadManager::LeaveSquadScope( const SquadScopeID& squadScopeId, const EntityId entityID )
{
	SquadAgentMap::iterator registeredAgentIt = m_registeredAgents.find(entityID);
	IF_UNLIKELY(registeredAgentIt == m_registeredAgents.end())
	{
		CRY_ASSERT_MESSAGE(0, "AISquadManager : An unregistered entity tried to leave a scope.");
		return;
	}

	SquadAgent& squadAgent = registeredAgentIt->second;
	SquadAgent::EnteredScopes::iterator enteredScopeIt = std::find(squadAgent.enteredScopes.begin(), squadAgent.enteredScopes.end(), squadScopeId);
	IF_UNLIKELY(enteredScopeIt == squadAgent.enteredScopes.end())
	{
		CRY_ASSERT_MESSAGE(0, "AISquadManager : An entity tried to leave a scope which it had not entered.");
		return;
	}

	squadAgent.enteredScopes.erase(enteredScopeIt);
}

void AISquadManager::SendSquadEvent( const EntityId sourcEntityId, const char* eventName )
{
	SquadAgentMap::const_iterator sourceAgentIt = m_registeredAgents.find(sourcEntityId);
	if(sourceAgentIt != m_registeredAgents.end())
	{
		const SquadAgent& sourceAgent = sourceAgentIt->second;

		for(SquadAgentMap::const_iterator registeredAgentIt = m_registeredAgents.begin(); registeredAgentIt != m_registeredAgents.end(); ++registeredAgentIt)
		{
			const SquadAgent& squadAgent = registeredAgentIt->second;
			if(sourceAgent.squadID == squadAgent.squadID)
			{
				Agent agent(squadAgent.entityId);
				if(agent.IsValid())
					agent.SetSignal(SIGNALFILTER_SENDER, eventName);
			}
		}
	}
}

IEntity* AISquadManager::GetFormationLeaderForMySquad( const EntityId requesterId )
{
	const SquadId squadId = GetSquadIdForAgent(requesterId);
	for(SquadAgentMap::const_iterator registeredAgentIt = m_registeredAgents.begin(); registeredAgentIt != m_registeredAgents.end(); ++registeredAgentIt)
	{
		const SquadAgent& squadAgent = registeredAgentIt->second;
		if(squadAgent.squadID == squadId)
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(squadAgent.entityId);
			if(pEntity)
			{
				IAIObject* pIAIObject = pEntity->GetAI();
				if(pIAIObject && pIAIObject->HasFormation())
				{
					return pEntity;
				}
			}
		}
	}
	return NULL;
}

void AISquadManager::GetSquadMembers( const SquadId squadId, MemberIDArray& members ) const
{
	SquadAgentMap::const_iterator it = m_registeredAgents.begin();
	SquadAgentMap::const_iterator end = m_registeredAgents.end();

	for (; it != end; ++it)
	{
		const SquadAgent& agent = it->second;
		const EntityId entityId = it->first;

		if (agent.squadID == squadId)
		{
			members.push_back(entityId);
		}
	}
}

void AISquadManager::GetSquadMembersInScope( const SquadId squadId, const SquadScopeID& squadScopeID, MemberIDArray& members ) const
{
	SquadAgentMap::const_iterator it = m_registeredAgents.begin();
	SquadAgentMap::const_iterator end = m_registeredAgents.end();

	for (; it != end; ++it)
	{
		const SquadAgent& agent = it->second;
		const EntityId entityId = it->first;

		if (agent.squadID == squadId && agent.IsInScope(squadScopeID))
		{
			members.push_back(entityId);
		}
	}
}

uint32 AISquadManager::GetSquadMemberCount( const SquadId squadId ) const
{
	SquadAgentMap::const_iterator it = m_registeredAgents.begin();
	SquadAgentMap::const_iterator end = m_registeredAgents.end();

	uint32 count = 0;
	for (; it != end; ++it)
	{
		const SquadAgent& agent = it->second;
		if (agent.squadID == squadId)
			count++;
	}
	
	return count;
}

SquadId AISquadManager::GetSquadIdForAgent(const EntityId entityId) const
{
	SquadAgentMap::const_iterator registeredAgentIt = m_registeredAgents.find(entityId);
	if(registeredAgentIt != m_registeredAgents.end())
	{
		return registeredAgentIt->second.squadID;
	}
	return UnknownSquadId;
}

void AISquadManager::Reset()
{
	m_registeredAgents.clear();
	m_timeAccumulator = g_pGameCVars->ai_SquadManager_UpdateTick;
}

void AISquadManager::Update(float frameDeltaTime)
{
	m_timeAccumulator+= frameDeltaTime;
	if(m_timeAccumulator > g_pGameCVars->ai_SquadManager_UpdateTick)
	{
		RequestSquadsUpdate();
		m_timeAccumulator = .0f;
	}
	if(g_pGameCVars->ai_SquadManager_DebugDraw)
	{
		DebugDraw();
	}
}

void AISquadManager::RequestSquadsUpdate()
{
	assert(gEnv->pAISystem);
	if(m_registeredAgents.empty())
		return;

	if(gEnv->pAISystem)
	{
		IClusterDetector* pClusterDetector = gEnv->pAISystem->GetClusterDetector();
		if(!pClusterDetector)
			return;
		IClusterDetector::IClusterRequestPair requestPair = pClusterDetector->CreateNewRequest();
		IClusterRequest* pRequest = requestPair.second;
		for(AISquadManager::SquadAgentMap::const_iterator it = m_registeredAgents.begin();
			it != m_registeredAgents.end(); ++it)
		{
			const EntityId id = it->first;
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
			assert(pEntity);
			if(pEntity && pEntity->IsActivatedForUpdates())
			{
				pRequest->SetNewPointInRequest(id, pEntity->GetPos());
			} 
		}
		pRequest->SetCallback(functor(*this, &AISquadManager::ClusterDetectorCallback));
		pRequest->SetMaximumSqDistanceAllowedPerCluster(sqr(g_pGameCVars->ai_SquadManager_MaxDistanceFromSquadCenter));

		pClusterDetector->QueueRequest(requestPair.first);
	}
}

void AISquadManager::ClusterDetectorCallback(IClusterRequest* result)
{
	const size_t pointSize = result->GetNumberOfPoint();
	for(size_t i=0; i < pointSize; ++i)
	{
		const ClusterPoint* pPoint = result->GetPointAt(i);
		assert(pPoint);
		SquadAgentMap::iterator agentIt = m_registeredAgents.find(pPoint->pointId);
		if(agentIt != m_registeredAgents.end())
			agentIt->second.squadID = pPoint->clusterId;
	}
	m_totalSquads = result->GetTotalClustersNumber();
}

void AISquadManager::DebugDraw() const
{
	for(SquadAgentMap::const_iterator it = m_registeredAgents.begin(); it != m_registeredAgents.end(); ++it)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(it->first);
		assert(pEntity);
		if(pEntity)
		{
			const Vec3& pos = pEntity->GetPos();
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(pos,1.0f, GetColorForCluster(it->second.squadID));

		}
	}
}

ColorB AISquadManager::GetColorForCluster(SquadId squadID) const
{
	switch(squadID)
	{
		case 0:  return ColorB(Col_GreenYellow);
		case 1:  return ColorB(Col_IndianRed);
		case 2:  return ColorB(Col_Blue);
		case 3:  return ColorB(Col_Maroon);
		case 4:  return ColorB(Col_Khaki);
		case 5:  return ColorB(Col_FireBrick);
		case 6:  return ColorB(Col_Orange);
		case 7:  return ColorB(Col_Pink);
		case 8:  return ColorB(Col_Turquoise);
		case 9:  return ColorB(Col_Yellow);
		case 10: return ColorB(Col_DarkGreen);
		case 11: return ColorB(Col_DarkOrchid);
		case 12: return ColorB(Col_Gold);
		case 13: return ColorB(Col_CadetBlue);
		case 14: return ColorB(Col_MediumSpringGreen);
		case 15: return ColorB(Col_ForestGreen);
		default: return ColorB(Col_White);
	}
}