// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef SearchModule_h
#define SearchModule_h

#include "GameAISystem.h"
#include <CryAISystem/IVisionMap.h>

struct SearchSpotQuery
{
	float closenessToAgentWeight;
	float closenessToTargetWeight;
	float closenessToTargetCurrentPosWeight;
	float minDistanceFromAgent;
	Vec3 result;
};

struct SearchActor
{
	SearchActor()
		: entityID(0)
	{}

	SearchActor(EntityId _entityID)
		: entityID(_entityID)
	{}

	EntityId entityID;
	VisionID visionID;

	void Serialize(TSerialize ser)
	{
		ser.Value("entityID", entityID);
	}
};

enum SearchSpotStatus
{
	NotSearchedYet,
	BeingSearchedRightAboutNow,
	Searched,
	Unreachable,
	SearchedTimingOut,
};

class SearchSpot
{
public:
	SearchSpot();
	~SearchSpot();

	void Init(const Vec3& pos,bool isTargetSpot = false);
	void DebugDraw(float searchTimeOut);
	void SetStatus(SearchSpotStatus newStatus) { m_status = newStatus; }
	SearchSpotStatus GetStatus() const { return m_status; }
	bool HasBeenSearched() const { return m_status == Searched; }
	bool IsBeingSearched() const { return m_status == BeingSearchedRightAboutNow; }
	const Vec3& GetPos() const { return m_pos; }
	operator const VisionID& () const { return m_visionID; }
	bool IsAssignedTo(EntityId entityID) const;
	void AssignTo(EntityId entityID);
	void MarkAsSearchedBy(SearchActor& participant, float timeout);
	void MarkAsUnreachable();
	bool IsTimingOut() const { return m_status == SearchedTimingOut; }
	void UpdateSearchedTimeout(float frameTime);
	void Serialize(TSerialize ser);

//private:
	Vec3 m_pos;
	SearchSpotStatus m_status;
	VisionID m_visionID;
	EntityId m_assigneeID;
	float m_searchTimeoutLeft;
	bool m_isTargetSearchSpot;

	CTimeValue m_lastTimeObserved;
};

class SearchGroup
{
	friend class SearchModule;

public:
	void Init(int groupID, const Vec3& targetPos, EntityId targetID, float searchSpotTimeout);
	void Destroy();
	void Update();
	bool GetNextSearchPoint(EntityId entityID, SearchSpotQuery* query);
	void MarkAssignedSearchSpotAsUnreachable(EntityId entityID);
	void AddEnteredEntity(const EntityId entityID);
	void RemoveEnteredEntity(const EntityId entityID);
	bool IsEmpty() const;

private:
	void StoreActors(int groupID);
	void GenerateSearchSpots();
	float CalculateScore(SearchSpot& searchSpot, EntityId entityID, SearchSpotQuery* query, Vec3& targetCurrentPos) const;
	SearchSpot* FindBestSearchSpot(EntityId entityID, SearchSpotQuery* query);
	SearchSpot* GetAssignedSearchSpot(EntityId entityID);
	void InitActors();
	void InitSearchSpots();

private:
	typedef std::vector<SearchSpot> SearchSpots;
	typedef std::vector<SearchSpot>::iterator SearchSpotIter;

	typedef std::vector<SearchActor> SearchActors;
	typedef std::vector<SearchActor>::iterator SearchActorIter;

	SearchActors m_actors;
	SearchSpots m_searchSpots;
	Vec3 m_targetPos;
	EntityId m_targetID;
	float m_searchSpotTimeout;
	std::vector<EntityId> m_enteredEntities;
};

class SearchModule : public IGameAIModule
{
public:
	virtual void EntityEnter(EntityId entityID);
	virtual void EntityLeave(EntityId entityID);
	virtual void EntityPause(EntityId entityID) { EntityLeave(entityID); }
	virtual void EntityResume(EntityId entityID) { EntityEnter(entityID); }
	virtual void Reset(bool bUnload);
	virtual void Update(float dt);
	virtual void Serialize(TSerialize ser);
	virtual void PostSerialize();
	virtual const char* GetName() const { return "SearchModule"; }

	void GroupEnter(int groupID, const Vec3& targetPos, EntityId targetID, float searchSpotTimeout);
	void GroupLeave(int groupID);
	bool GetNextSearchPoint(EntityId entityID, SearchSpotQuery* query);
	void MarkAssignedSearchSpotAsUnreachable(EntityId entityID);
	bool GroupExist(int groupID) const;

private:
	SearchGroup* GetGroup(int groupID);
	void DebugDraw() const;

private:
	typedef int GroupID;
	typedef std::map<GroupID, SearchGroup> SearchGroupContainer;
	typedef std::map<GroupID, SearchGroup>::iterator SearchGroupIter;

	SearchGroupContainer m_groups;
};

#endif // SearchModule_h
