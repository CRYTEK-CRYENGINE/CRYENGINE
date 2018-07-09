// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SearchModule.h"
#include <CryAISystem/IAgent.h>
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IAIObject.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryAISystem/ICoverSystem.h>
#include <CryAISystem/VisionMapTypes.h>
#include "Agent.h"
#include "GameCVars.h"

SearchSpot::SearchSpot()
: m_pos(ZERO)
, m_status(NotSearchedYet)
, m_assigneeID(0)
, m_searchTimeoutLeft(0.0f)
, m_isTargetSearchSpot(false)
, m_lastTimeObserved(0.0f)
{

}

SearchSpot::~SearchSpot()
{
	if (m_visionID)
	{
		assert(gEnv->pAISystem);
		IVisionMap& visionMap = *gEnv->pAISystem->GetVisionMap();
		visionMap.UnregisterObservable(m_visionID);
		m_visionID = VisionID();
	}
}

void SearchSpot::Init(const Vec3& pos, bool isTargetSpot /*= false*/ )
{
	assert(pos.IsValid());

	m_pos = pos;
	m_status = NotSearchedYet;
	m_isTargetSearchSpot = isTargetSpot;

	assert(!m_visionID);
	if (!m_visionID)
	{
		assert(gEnv->pAISystem);
		IVisionMap& visionMap = *gEnv->pAISystem->GetVisionMap();

		static unsigned int searchSpotCounter = 0;
		string visionName;
		visionName.Format("SearchSpot%d", searchSpotCounter++);
		m_visionID = visionMap.CreateVisionID(visionName);

		ObservableParams observableParams;
		observableParams.observablePositionsCount = 1;
		observableParams.observablePositions[0] = pos;
		observableParams.typeMask = SearchSpots;
		observableParams.faction = 31;
		visionMap.RegisterObservable(m_visionID, observableParams);
	}
}

void SearchSpot::DebugDraw(float searchTimeOut)
{
	ColorB spotColor;

	switch (m_status)
	{
	case NotSearchedYet:
		spotColor = ColorB(0, 0, 255);
		break;
	case BeingSearchedRightAboutNow:
		spotColor = ColorB(255, 255, 0);
		break;
	case Searched:
		spotColor = ColorB(0, 255, 0);
		break;
	case Unreachable:
		spotColor = ColorB(255, 0, 0);
		break;
	case SearchedTimingOut:
		if(searchTimeOut)
		{
			uint8 green = (uint8)(255 * clamp_tpl( (m_searchTimeoutLeft / (searchTimeOut / 2.0f)), 0.0f, 1.0f));
			uint8 blue = (uint8)(255 * clamp_tpl(((searchTimeOut - m_searchTimeoutLeft) / (searchTimeOut / 2.0f)), 0.0f, 1.0f));
			spotColor = ColorB(0, green, blue);
		}
		break;
	}

	IRenderAuxGeom* pDebugRenderer = gEnv->pRenderer->GetIRenderAuxGeom();
	pDebugRenderer->DrawSphere(m_pos, 0.3f, spotColor);

	if (m_assigneeID)
	{
		Agent agent(m_assigneeID);
		if (agent)
			pDebugRenderer->DrawLine(agent.GetPos(), ColorB(255, 255, 0), m_pos, ColorB(255, 255, 0), 2.0f);
	}
}

bool SearchSpot::IsAssignedTo(EntityId entityID) const
{
	return m_assigneeID == entityID;
}

void SearchSpot::AssignTo(EntityId entityID)
{
	m_assigneeID = entityID;
}

void SearchSpot::MarkAsSearchedBy(SearchActor& participant, float timeout)
{
	if(m_assigneeID)
	{
		if (IsAssignedTo(participant.entityID))
		{
			Agent agent(participant.entityID);
			if (agent)
			{
				if(m_isTargetSearchSpot)
					agent.SetSignal(0, "OnTargetSearchSpotSeen");

				agent.SetSignal(0, "OnAssignedSearchSpotSeen");
			}
		}
		else
		{
			Agent agent(m_assigneeID);
			if (agent)
			{
				agent.SetSignal(0, "OnAssignedSearchSpotSeenBySomeoneElse");
			}
		}
		m_assigneeID = 0;
	}

	if(timeout > 0.0f)
	{
		m_searchTimeoutLeft = timeout;
		m_status = SearchedTimingOut;
	}
	else
		m_status = Searched;

	m_lastTimeObserved = gEnv->pTimer->GetFrameStartTime();
}

void SearchSpot::MarkAsUnreachable()
{
	m_status = Unreachable;
	m_assigneeID = 0;
}

void SearchSpot::UpdateSearchedTimeout(float frameTime)
{
	m_searchTimeoutLeft -= frameTime;
	if(m_searchTimeoutLeft < 0.0f)
		m_status = NotSearchedYet;
}

void SearchSpot::Serialize(TSerialize ser)
{
	// VisionID is not serialized. It will be created in InitSearchSpots.

	ser.Value("pos", m_pos);
	ser.Value("assigneeID", m_assigneeID);
	ser.Value("searchTimeoutLeft", m_searchTimeoutLeft);
	ser.Value("isTargetSearchSpot", m_isTargetSearchSpot);

	uint8 intStatus = (uint8)m_status;
	ser.Value("status", intStatus);

	if (ser.IsReading())
		m_status = (SearchSpotStatus)intStatus;
}

void SearchGroup::Init(int groupID, const Vec3& targetPos, EntityId targetID, float searchSpotTimeout)
{
	m_targetPos = targetPos;
	m_targetID = targetID;
	m_searchSpotTimeout = searchSpotTimeout;

	StoreActors(groupID);
	GenerateSearchSpots();
}

void SearchGroup::Destroy()
{
	IVisionMap& visionMap = *gEnv->pAISystem->GetVisionMap();

	SearchActorIter it = m_actors.begin();
	SearchActorIter end = m_actors.end();

	for ( ; it != end; ++it)
	{
		SearchActor& actor = *it;
		visionMap.UnregisterObserver(actor.visionID);
	}

	m_actors.clear();
}

void SearchGroup::StoreActors(int groupID)
{
	assert(m_actors.empty());

	IAISystem& aiSystem = *gEnv->pAISystem;

	uint groupSize = (uint)aiSystem.GetGroupCount(groupID);
	m_actors.reserve(groupSize);

	for (uint i = 0; i < groupSize; ++i)
	{
		if (IAIObject* pAI = aiSystem.GetGroupMember(groupID, i))
			m_actors.push_back(SearchActor(pAI->GetEntityID()));
	}

	InitActors();
}

void SearchGroup::GenerateSearchSpots()
{
	assert(m_searchSpots.empty());

	const uint32 MaxCoverLocationsPerSurface = 3;
	const uint32 MaxCoverLocations = 128;
	Vec3 coverLocations[MaxCoverLocations];

	const uint32 MaxEyeCount = 16;

	Vec3 eyes[MaxEyeCount];
	uint32 eyeCount = 0;

	std::vector<SearchActor>::iterator actorIt = m_actors.begin();
	std::vector<SearchActor>::iterator actorEnd = m_actors.end();

// 	for ( ; (eyeCount < MaxEyeCount) && (actorIt != actorEnd); ++actorIt)
// 	{
// 		SearchActor& actor = (*actorIt);
// 
// 		Agent agent(actor.entityID);
// 		if(!agent.IsValid())
// 			continue;
// 
// 		eyes[eyeCount++] = agent.GetPos();
// 	}

	IAISystem& aiSystem = *gEnv->pAISystem;
	uint32 locationCount = 
		aiSystem.GetCoverSystem()->GetCover(m_targetPos, 50.0f, &eyes[0], eyeCount, 0.45f, &coverLocations[0],
		MaxCoverLocations, MaxCoverLocationsPerSurface);

	// Filter out search spots too close to each other
	for (uint32 i = 0; i < locationCount; ++i)
	{
		for (uint32 j = i + 1; j < locationCount;)
		{
			const float squaredDistance = coverLocations[i].GetSquaredDistance(coverLocations[j]);
			if (squaredDistance < sqr(3.0f))
			{
				// Too close, remove j
				coverLocations[j] = coverLocations[locationCount - 1];
				--locationCount;
			}
			else
				++j;
		}
	}


	// Initialize search spots
	m_searchSpots.resize(locationCount + 1);

	for (uint32 i = 0; i < locationCount; ++i)
	{
		SearchSpot& spot = m_searchSpots[i];
		spot.Init(coverLocations[i] + Vec3(0.0f, 0.0f, 0.5f));
	}

	// Adding targetSearchSpot to the searchSpots as well
	m_searchSpots.back().Init(m_targetPos + Vec3(0.0f, 0.0f, 0.2f), true);
}

float SearchGroup::CalculateScore(SearchSpot& searchSpot, EntityId entityID, SearchSpotQuery* query, Vec3& targetCurrentPos) const
{
	assert(query);

	Agent agent(entityID);
	if (agent.IsValid())
	{
		const Vec3 spotPosition = searchSpot.GetPos();
		const Vec3 agentPosition = agent.GetPos();

		const float agentToSpotDistance = agentPosition.GetDistance(spotPosition);
		if (agentToSpotDistance < query->minDistanceFromAgent)
		{
			return -100.0f;
		}

		float closenessToAgentScore = 1.0f - clamp_tpl(agentToSpotDistance, 1.0f, 50.0f) / 50.0f;
		const float closenessToTargetScore = 1.0f - clamp_tpl(spotPosition.GetDistance(m_targetPos),    1.0f, 50.0f) / 50.0f;

		float closenessToTargetCurrentPosScore = 0.0f;
		if (!targetCurrentPos.IsZeroFast())
		{
			closenessToTargetCurrentPosScore = 1.0f - clamp_tpl(spotPosition.GetDistance(targetCurrentPos), 1.0f, 50.0f) / 50.0f;
		}

		return
			closenessToAgentScore  * query->closenessToAgentWeight +
			closenessToTargetScore * query->closenessToTargetWeight +
			closenessToTargetCurrentPosScore * query->closenessToTargetCurrentPosWeight;
	}
	else
	{
		return -100.0f;
	}
}

void SearchGroup::Update()
{
	IVisionMap& visionMap = *gEnv->pAISystem->GetVisionMap();

	// Update vision
	{
		std::vector<SearchActor>::iterator actorIt = m_actors.begin();
		std::vector<SearchActor>::iterator actorEnd = m_actors.end();

		for ( ; actorIt != actorEnd; ++actorIt)
		{
			SearchActor& actor = (*actorIt);

			Agent agent(actor.entityID);
			if(!agent.IsValid())
				continue;

			ObserverParams observerParams;
			observerParams.eyePosition = agent.GetPos();
			observerParams.eyeDirection = agent.GetViewDir();

			visionMap.ObserverChanged(actor.visionID, observerParams, eChangedPosition | eChangedOrientation);
		}
	}

	// Debug draw target pos
	if (g_pGameCVars->ai_DebugSearch)
	{
		IRenderAuxGeom* pDebugRenderer = gEnv->pRenderer->GetIRenderAuxGeom();
		pDebugRenderer->DrawSphere(m_targetPos, 0.6f, ColorB(255, 255, 255, 128));
	}

	const float frameTime = gEnv->pTimer->GetFrameTime();

	std::vector<SearchSpot>::iterator spotIt = m_searchSpots.begin();
	std::vector<SearchSpot>::iterator spotEnd = m_searchSpots.end();

	for ( ; spotIt != spotEnd; ++spotIt)
	{
		SearchSpot& searchSpot = (*spotIt);

		if (g_pGameCVars->ai_DebugSearch)
			searchSpot.DebugDraw(m_searchSpotTimeout);

		if(searchSpot.IsTimingOut())
			searchSpot.UpdateSearchedTimeout(frameTime);

		if (searchSpot.HasBeenSearched())
			continue;

		// Naive Implementation!
		// Go through all the actors and see
		// if they see any of the search spots.
		// Later on, use a callback for this!

		SearchActorIter actorIt = m_actors.begin();
		std::vector<SearchActor>::iterator actorEnd = m_actors.end();

		for ( ; actorIt != actorEnd; ++actorIt)
		{
			SearchActor& actor = *actorIt;
			if (visionMap.IsVisible(actor.visionID, searchSpot))
			{
				searchSpot.MarkAsSearchedBy(actor, m_searchSpotTimeout);
				break;
			}
		}
	}
}

bool SearchGroup::GetNextSearchPoint(EntityId entityID, SearchSpotQuery* query)
{
	if (SearchSpot* best = FindBestSearchSpot(entityID, query))
	{
		SearchSpot* assignedSearchSpot = GetAssignedSearchSpot(entityID);
		if(assignedSearchSpot)
		{
			assignedSearchSpot->m_assigneeID = 0;
			assignedSearchSpot->m_status = NotSearchedYet;
		}

		assert(query);
		query->result = best->GetPos();
		best->m_status = BeingSearchedRightAboutNow;
		best->m_assigneeID = entityID;
		return true;
	}

	return false;
}

void SearchGroup::MarkAssignedSearchSpotAsUnreachable(EntityId entityID)
{
	SearchSpot* assignedSpot = GetAssignedSearchSpot(entityID);

	if (assignedSpot)
	{
		assignedSpot->m_status = Unreachable;
		assignedSpot->m_assigneeID = 0;
	}
}

void SearchGroup::AddEnteredEntity(const EntityId entityID)
{
	stl::push_back_unique(m_enteredEntities, entityID);
}

void SearchGroup::RemoveEnteredEntity(const EntityId entityID)
{
	stl::find_and_erase(m_enteredEntities, entityID);
}

bool SearchGroup::IsEmpty() const
{
	return m_enteredEntities.empty();
}

SearchSpot* SearchGroup::FindBestSearchSpot(EntityId entityID, SearchSpotQuery* query)
{
	SearchSpotIter it = m_searchSpots.begin();
	SearchSpotIter end = m_searchSpots.end();

	SearchSpot* bestScoredSearchSpot = NULL;
	SearchSpot* lastSeenSearchSpot = NULL;
	float bestScore = FLT_MIN;

	Vec3 targetCurrentPos(ZERO);
	if(m_targetID)
	{
		Agent agent(m_targetID);
		if(agent.IsValid())
			targetCurrentPos = agent.GetPos();
	}

	for ( ; it != end; ++it)
	{
		SearchSpot& searchSpot = (*it);

		if (searchSpot.GetStatus() == NotSearchedYet)
		{
			float score = CalculateScore(searchSpot, entityID, query, targetCurrentPos);
			if (score > bestScore)
			{
				bestScoredSearchSpot = &searchSpot;
				bestScore = score;
			}
		}

		if(searchSpot.GetStatus() != Unreachable && searchSpot.GetStatus() != BeingSearchedRightAboutNow)
		{
			if(!lastSeenSearchSpot || searchSpot.m_lastTimeObserved.GetValue() < lastSeenSearchSpot->m_lastTimeObserved.GetValue())
			{
				lastSeenSearchSpot = &searchSpot;
			}
		}
	}

	return bestScoredSearchSpot ? bestScoredSearchSpot : lastSeenSearchSpot;
}

SearchSpot* SearchGroup::GetAssignedSearchSpot(EntityId entityID)
{
	// TODO: Fix this! It is implemented in a suuuuuper naive way.
	// It's going through all search spots and sees if it
	// has the agent as an assignee.  We should have an internal
	// representation of the agents so we can store this reference back.

	SearchSpotIter it = m_searchSpots.begin();
	SearchSpotIter end = m_searchSpots.end();

	for ( ; it != end; ++it)
	{
		SearchSpot& spot = (*it);

		if (spot.m_assigneeID == entityID)
			return &spot;
	}

	return NULL;
}

void SearchGroup::InitActors()
{
	IVisionMap& visionMap = *gEnv->pAISystem->GetVisionMap();

	SearchActors::iterator actorIt = m_actors.begin();
	SearchActors::iterator actorEnd = m_actors.end();

	for ( ; actorIt != actorEnd; ++actorIt)
	{
		SearchActor& actor = *actorIt;

		Agent agent(actor.entityID);
		assert(agent.IsValid());
		if (agent.IsValid())
		{
			actor.visionID = visionMap.CreateVisionID(agent.GetName());

			ObserverParams observerParams;
			observerParams.entityId = actor.entityID;
			observerParams.factionsToObserveMask = 0xFFFFFFFF;
			observerParams.typeMask = ::SearchSpots;
			observerParams.eyePosition = agent.GetPos();
			observerParams.eyeDirection = agent.GetViewDir();

			observerParams.fovCos = cosf(120.0f);
			observerParams.sightRange = 8.0f;

			IScriptTable* entityTable = agent.GetScriptTable();
			SmartScriptTable properties;
			if (entityTable->GetValue("SearchModule", properties))
				properties->GetValue("sightRange", observerParams.sightRange);

			visionMap.RegisterObserver(actor.visionID, observerParams);
		}
	}
}

void SearchGroup::InitSearchSpots()
{
	SearchSpots::iterator it = m_searchSpots.begin();
	SearchSpots::iterator end = m_searchSpots.end();

	for ( ; it != end; ++it)
	{
		SearchSpot& spot = *it;
		spot.Init(spot.m_pos, spot.m_isTargetSearchSpot);
	}
}

void SearchModule::EntityEnter(EntityId entityID)
{
	Agent agent(entityID);
	assert(agent.IsValid());
	if (agent.IsValid())
	{
		if (SearchGroup* group = GetGroup(agent.GetGroupID()))
		{
			group->AddEnteredEntity(entityID);
		}
	}
}

void SearchModule::EntityLeave(EntityId entityID)
{
	Agent agent(entityID);
	if (agent.IsValid())
	{
		const int groupID = agent.GetGroupID();
		if (SearchGroup* group = GetGroup(groupID))
		{
			group->RemoveEnteredEntity(entityID);

			if (group->IsEmpty())
			{
				GroupLeave(groupID);
			}
		}
	}
}

void SearchModule::Reset(bool bUnload)
{
	SearchGroupIter it = m_groups.begin();
	SearchGroupIter end = m_groups.end();

	for ( ; it != end; ++it)
	{
		SearchGroup& group = it->second;
		group.Destroy();
	}

	m_groups.clear();
}

void SearchModule::Update(float dt)
{
	SearchGroupIter it = m_groups.begin();
	SearchGroupIter end = m_groups.end();

	for ( ; it != end; ++it)
	{
		SearchGroup& group = it->second;
		group.Update();
	}
}

void SearchModule::Serialize(TSerialize ser)
{
	if (ser.IsReading())
	{
		Reset(false);
	}
}

void SearchModule::PostSerialize()
{

}

void SearchModule::GroupEnter(int groupID, const Vec3& targetPos, EntityId targetID, float searchSpotTimeout)
{
	if(GroupExist(groupID))
		GroupLeave(groupID);

	SearchGroup& group = m_groups[groupID];
	group.Init(groupID, targetPos, targetID, searchSpotTimeout);
}

void SearchModule::GroupLeave(int groupID)
{
	SearchGroupIter it = m_groups.find(groupID);

	if (it != m_groups.end())
	{
		SearchGroup& group = it->second;
		group.Destroy();
		m_groups.erase(it);
	}
}

bool SearchModule::GetNextSearchPoint(EntityId entityID, SearchSpotQuery* query)
{
	Agent agent(entityID);
	if (agent)
	{
		SearchGroup* group = GetGroup(agent.GetGroupID());
		if (group)
			return group->GetNextSearchPoint(entityID, query);
	}

	return false;
}

void SearchModule::MarkAssignedSearchSpotAsUnreachable(EntityId entityID)
{
	Agent agent(entityID);
	if (agent)
	{
		SearchGroup* group = GetGroup(agent.GetGroupID());
		if (group)
			return group->MarkAssignedSearchSpotAsUnreachable(entityID);
	}
}

bool SearchModule::GroupExist(int groupID) const
{
	return m_groups.find(groupID) != m_groups.end();
}

SearchGroup* SearchModule::GetGroup(int groupID)
{
	SearchGroupIter it = m_groups.find(groupID);

	if (it != m_groups.end())
		return &it->second;

	return NULL;
}