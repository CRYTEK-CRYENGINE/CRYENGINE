// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef AIBattleFront_h
#define AIBattleFront_h

#include <CryEntitySystem/IEntity.h>
#include "GameAIHelpers.h"

struct SBattleFrontMember
{
	SBattleFrontMember(EntityId _entityID);

	bool operator < (const SBattleFrontMember& rhs) const 
	{	
		return entityID < rhs.entityID;
	}

	EntityId entityID;
	mutable bool paused;
};

class CAIBattleFrontGroup
{
public:
	typedef int GroupID;

	CAIBattleFrontGroup();
	CAIBattleFrontGroup(GroupID groupID);
	void AddEntity(EntityId entityID);
	void RemoveEntity(EntityId entityID);
	void PauseEntity(EntityId entityID);
	void ResumeEntity(EntityId entityID);
	void Update();
	void SetDesignerControlledBattleFrontAt(const Vec3& position);
	void DisableDesignerControlledBattleFront();
	void DrawDebugInfo();

	const Vec3& GetBattleFrontPosition() const
	{
		return m_battleFrontPosition;
	}

	const Vec3& GetAveragePosition() const
	{
		return m_averagePosition;
	}

	unsigned int GetMemberCount() const
	{
		return m_members.size();
	}

	bool IsEmpty() const
	{
		return m_members.empty();
	}

private:
	enum BattleFrontMode
	{
		Dynamic,
		DesignerControlled
	};

	void CalculateAveragePositionOfGroupMembers();
	void SetPause(EntityId entityID, bool paused);

	std::set<SBattleFrontMember> m_members;
	GroupID m_groupID;
	Vec3 m_battleFrontPosition;
	Vec3 m_averagePosition;
	BattleFrontMode m_battleFrontMode;
};

class CAIBattleFrontModule : public IGameAIModule
{
public:
	virtual void EntityEnter(EntityId entityID);
	virtual void EntityLeave(EntityId entityID);
	virtual void EntityPause(EntityId entityID);
	virtual void EntityResume(EntityId entityID);
	virtual void Reset(bool bUnload);
	virtual void Update(float dt);
	virtual const char* GetName() const;
	CAIBattleFrontGroup* GetGroupForEntity(EntityId entityID);
	CAIBattleFrontGroup* GetGroupByID(CAIBattleFrontGroup::GroupID groupID);

private:
	std::map<CAIBattleFrontGroup::GroupID, CAIBattleFrontGroup> m_battleFrontGroups;
};

#endif // AIBattleFront_h
