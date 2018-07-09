// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef AISquadManager_h
#define AISquadManager_h

#include <CryAISystem/IClusterDetector.h>

typedef ClusterId SquadId;
#define UnknownSquadId SquadId(~0)

class SquadScopeID
{
public:
	SquadScopeID()
		: m_nameCRC(0)
	{
	}

	SquadScopeID(const char* name)
	{
#ifdef CRYAISYSTEM_DEBUG
		m_debugName = name;
#endif
		m_nameCRC = CCrc32::ComputeLowercase(name);
	}

	bool operator == (const SquadScopeID& rhs) const
	{
		return m_nameCRC == rhs.m_nameCRC;
	}

private:
#ifdef CRYAISYSTEM_DEBUG
	stack_string m_debugName;
#endif
	unsigned int m_nameCRC;
};

struct SquadAgent
{
	SquadAgent(const EntityId _entityId)
	:entityId(_entityId)
	,squadID(~0)
	{
	}

	bool IsInScope(const SquadScopeID& scopeId) const
	{
		return std::find(enteredScopes.begin(),enteredScopes.end(),scopeId) != enteredScopes.end();
	}

	EntityId entityId;
	typedef std::vector<SquadScopeID> EnteredScopes;
	EnteredScopes enteredScopes;
	SquadId squadID;
};

class AISquadManager
{
public:
	AISquadManager();
	~AISquadManager();

	void RegisterAgent( const EntityId entityId );
	void UnregisterAgent( const EntityId entityId );

	bool EnterSquadScope( const SquadScopeID& scopeId, const EntityId entityId, const uint32 concurrentUserAllowed );
	void LeaveSquadScope( const SquadScopeID& scopeId, const EntityId entityId );

	void SendSquadEvent( const EntityId sourcEntityId, const char* eventName);

	typedef std::vector<EntityId> MemberIDArray;
	void GetSquadMembers( const SquadId squadId, MemberIDArray& members ) const;
	void GetSquadMembersInScope( const SquadId squadId, const SquadScopeID& squadScopeID, MemberIDArray& members ) const;
	uint32 GetSquadMemberCount( const SquadId squadId ) const;

	SquadId GetSquadIdForAgent(const EntityId entityId) const;

	void Reset();
	void RequestSquadsUpdate();
	
	void Init();	
	void Update(float frameDeltaTime);

	// Utility functions for a specific game purpose
	IEntity* GetFormationLeaderForMySquad(const EntityId requesterId);

private:

	void ClusterDetectorCallback(IClusterRequest* result);

	uint32 GetCurrentSquadMatesInScope(const SquadId squadId, const SquadScopeID& squadScopeId) const;

	void DebugDraw() const;
	ColorB GetColorForCluster(SquadId squadID) const;
	typedef std::map<EntityId, SquadAgent> SquadAgentMap;
	SquadAgentMap m_registeredAgents;
	float m_timeAccumulator; // Needs to be change to avoid hit-store penalty
	size_t m_totalSquads;
};

#endif //AISquadManager_h