// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "MNMPathfinder.h"
#include "Navigation/MNM/NavMesh.h"
#include "Navigation/NavigationSystem/NavigationSystem.h"
#include "SmartObjectOffMeshNavigation.h"
#include "DebugDrawContext.h"
#include "AIBubblesSystem/AIBubblesSystem.h"
#include <CryThreading/IJobManager_JobDelegator.h>
#include <CryAISystem/NavigationSystem/INavMeshQueryManager.h>

//#pragma optimize("", off)
//#pragma inline_depth(0)

inline Vec3 TriangleCenter(const Vec3& a, const Vec3& b, const Vec3& c)
{
	return (a + b + c) / 3.f;
}

//////////////////////////////////////////////////////////////////////////

void MNM::PathfinderUtils::QueuedRequest::SetupDangerousLocationsData()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	dangerousAreas.clear();
	if (requestParams.dangersToAvoidFlags == eMNMDangers_None)
		return;

	if (requesterEntityId)
	{
		// The different danger types need a different setup
		if (requestParams.dangersToAvoidFlags & eMNMDangers_AttentionTarget)
		{
			SetupAttentionTargetDanger();
		}

		if (requestParams.dangersToAvoidFlags & eMNMDangers_Explosive)
		{
			SetupExplosiveDangers();
		}

		if (requestParams.dangersToAvoidFlags & eMNMDangers_GroupMates)
		{
			SetupGroupMatesAvoidance();
		}
	}
}

void MNM::PathfinderUtils::QueuedRequest::SetupAttentionTargetDanger()
{
	if (dangerousAreas.size() >= MNM::max_danger_amount)
		return;

	CRY_ASSERT(requesterEntityId != INVALID_ENTITYID);

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(requesterEntityId);
	CAIActor* pActor = pEntity ? CastToCAIActorSafe(pEntity->GetAI()) : nullptr;
	if (pActor)
	{
		IAIObject* pAttTarget = pActor->GetAttentionTarget();
		if (pAttTarget)
		{
			const float effectRange = 0.0f; // Effect over all the world
			const Vec3& dangerPosition = pEntity ? pEntity->GetPos() : pAttTarget->GetPos();
			MNM::DangerAreaConstPtr info;
			info.reset(new MNM::DangerAreaT<MNM::eWCT_Direction>(dangerPosition, effectRange, gAIEnv.CVars.pathfinder.PathfinderDangerCostForAttentionTarget));
			dangerousAreas.push_back(info);
		}
	}
}
// Predicate to find if an explosive danger is already in the list of the stored dangers
struct DangerAlreadyStoredInRangeFromPositionPred
{
	DangerAlreadyStoredInRangeFromPositionPred(const Vec3& _pos, const float _rangeSq)
		: pos(_pos)
		, rangeSq(_rangeSq)
	{}

	bool operator()(const MNM::DangerAreaConstPtr dangerInfo)
	{
		return Distance::Point_PointSq(dangerInfo->GetLocation(), pos) < rangeSq;
	}

private:
	const Vec3  pos;
	const float rangeSq;
};
#undef GetObject
void MNM::PathfinderUtils::QueuedRequest::SetupExplosiveDangers()
{
	CRY_ASSERT(requesterEntityId != INVALID_ENTITYID);

	const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(requesterEntityId);
	
	const float maxDistanceSq = sqr(gAIEnv.CVars.pathfinder.PathfinderExplosiveDangerMaxThreatDistance);
	const float maxDistanceSqToMergeExplosivesThreat = 3.0f;

	std::vector<Vec3> grenadesLocations;
	AutoAIObjectIter grenadesIterator(gAIEnv.pAIObjectManager->GetFirstAIObject(OBJFILTER_TYPE, AIOBJECT_GRENADE));

	while (dangerousAreas.size() < MNM::max_danger_amount && grenadesIterator->GetObject())
	{
		const Vec3& pos = grenadesIterator->GetObject()->GetPos();
		if (Distance::Point_Point2DSq(pos, pEntity->GetPos()) < maxDistanceSq)
		{
			MNM::DangerousAreasList::const_iterator it = std::find_if(dangerousAreas.begin(), dangerousAreas.end(),
			                                                          DangerAlreadyStoredInRangeFromPositionPred(pos, maxDistanceSqToMergeExplosivesThreat));
			if (it == dangerousAreas.end())
			{
				MNM::DangerAreaConstPtr info;
				info.reset(new MNM::DangerAreaT<MNM::eWCT_Range>(pos, gAIEnv.CVars.pathfinder.PathfinderExplosiveDangerRadius,
				                                                 gAIEnv.CVars.pathfinder.PathfinderDangerCostForExplosives));
				dangerousAreas.push_back(info);
			}
		}

		grenadesIterator->Next();
	}
}

void MNM::PathfinderUtils::QueuedRequest::SetupGroupMatesAvoidance()
{
	CRY_ASSERT(requesterEntityId != INVALID_ENTITYID);

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(requesterEntityId);
	IAIObject* requesterAIObject = pEntity ? pEntity->GetAI() : nullptr;
	
	IF_UNLIKELY (!requesterAIObject)
	{
		return;
	}

	const int requesterGroupId = requesterAIObject->GetGroupId();
	AutoAIObjectIter groupMemberIterator(gEnv->pAISystem->GetAIObjectManager()->GetFirstAIObject(OBJFILTER_GROUP, requesterGroupId));
	while (dangerousAreas.size() < MNM::max_danger_amount && groupMemberIterator->GetObject())
	{
		IAIObject* groupMemberAIObject = groupMemberIterator->GetObject();
		if (requesterEntityId != groupMemberAIObject->GetEntityID())
		{
			MNM::DangerAreaConstPtr dangerArea;
			dangerArea.reset(new MNM::DangerAreaT<MNM::eWCT_Range>(
			                   groupMemberAIObject->GetPos(),
			                   gAIEnv.CVars.pathfinder.PathfinderGroupMatesAvoidanceRadius,
			                   gAIEnv.CVars.pathfinder.PathfinderAvoidanceCostForGroupMates
			                   ));
			dangerousAreas.push_back(dangerArea);
		}
		groupMemberIterator->Next();
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
#undef max
#undef min

void ProcessPathRequestJob(MNM::PathfinderUtils::ProcessingContext* pProcessingContext)
{
	gAIEnv.pMNMPathfinder->ProcessPathRequest(*pProcessingContext);
}
DECLARE_JOB("PathProcessing", PathfinderProcessingJob, ProcessPathRequestJob);

void ConstructPathIfWayWasFoundJob(MNM::PathfinderUtils::ProcessingContext* pProcessingContext)
{
	gAIEnv.pMNMPathfinder->ConstructPathIfWayWasFound(*pProcessingContext);
}
DECLARE_JOB("PathConstruction", PathConstructionJob, ConstructPathIfWayWasFoundJob);

CMNMPathfinder::CMNMPathfinder()
{
	m_pathfindingFailedEventsToDispatch.reserve(gAIEnv.CVars.pathfinder.MNMPathfinderConcurrentRequests);
	m_pathfindingCompletedEventsToDispatch.reserve(gAIEnv.CVars.pathfinder.MNMPathfinderConcurrentRequests);
}

CMNMPathfinder::~CMNMPathfinder()
{
}

void CMNMPathfinder::Reset()
{
	WaitForJobsToFinish();

	// send failed to all requested paths before we clear this queue and leave all our callers with dangling ids that will be reused!
	while (!m_requestedPathsQueue.empty())
	{
		PathRequestFailed(m_requestedPathsQueue.front_id(), m_requestedPathsQueue.front(), EMNMPathResult::PathfinderReset);
		m_requestedPathsQueue.pop_front();
	}

	m_processingContextsPool.Reset();
	m_pathfindingFailedEventsToDispatch.clear();
	m_pathfindingCompletedEventsToDispatch.clear();
}

MNM::QueuedPathID CMNMPathfinder::RequestPathTo(const EntityId requesterEntityId, const MNMPathRequest& request)
{
	const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(requesterEntityId);
	
	//////////////////////////////////////////////////////////////////////////
	// Validate requester entity
	if (!pEntity)
	{
		AIWarning("[CMNMPathfinder::RequestPathTo] Request entity id %d is invalid.", requesterEntityId);
		return MNM::Constants::eQueuedPathID_InvalidID;
	}

	//////////////////////////////////////////////////////////////////////////
	// Validate Agent Type
	const char* actorName = pEntity->GetName();
	if (!request.agentTypeID)
	{
		AIWarning("[CMNMPathfinder::RequestPathTo] Request from agent %s has no NavigationType defined.", actorName);
		return MNM::Constants::eQueuedPathID_InvalidID;
	}

	//////////////////////////////////////////////////////////////////////////
	// Validate callback
	if (!request.resultCallback)
	{
		AIWarning("[CMNMPathfinder::RequestPathTo] Agent %s does not provide a result Callback", actorName);
		return 0;
	}

	//////////////////////////////////////////////////////////////////////////
	// Validate start/end locations
	const NavigationMeshID meshID = gAIEnv.pNavigationSystem->GetEnclosingMeshID(request.agentTypeID, request.startLocation);

	if (!meshID)
	{
		AIQueueBubbleMessage("CMNMPathfinder::RequestPathTo-NoMesh", requesterEntityId,
		                     "I'm not inside a Navigation area for my navigation type.",
		                     eBNS_LogWarning);
		return 0;
	}
	return m_requestedPathsQueue.push_back(MNM::PathfinderUtils::QueuedRequest(requesterEntityId, request.agentTypeID, request));
}

MNM::QueuedPathID CMNMPathfinder::RequestPathTo(const MNMPathRequest& request)
{
	const IEntity* pEntity = request.requesterEntityId != INVALID_ENTITYID ? gEnv->pEntitySystem->GetEntity(request.requesterEntityId) : nullptr;
	const char* actorName = pEntity ? pEntity->GetName() : "'missing entity'";
	
	//////////////////////////////////////////////////////////////////////////
	// Validate Agent Type
	if (!request.agentTypeID)
	{
		AIWarning("[CMNMPathfinder::RequestPathTo] Request from agent %s has no NavigationType defined.", actorName);
		return MNM::Constants::eQueuedPathID_InvalidID;
	}

	//////////////////////////////////////////////////////////////////////////
	// Validate callback
	if (!request.resultCallback)
	{
		AIWarning("[CMNMPathfinder::RequestPathTo] Agent %s does not provide a result Callback", actorName);
		return 0;
	}

	//////////////////////////////////////////////////////////////////////////
	// Validate start/end locations
	const NavigationMeshID meshID = gAIEnv.pNavigationSystem->GetEnclosingMeshID(request.agentTypeID, request.startLocation);

	if (!meshID)
	{
		if (request.requesterEntityId != INVALID_ENTITYID)
		{
			AIQueueBubbleMessage("CMNMPathfinder::RequestPathTo-NoMesh", request.requesterEntityId,
				"I'm not inside a Navigation area for my navigation type.",
				eBNS_LogWarning);
		}		
		return 0;
	}
	return m_requestedPathsQueue.push_back(MNM::PathfinderUtils::QueuedRequest(request.requesterEntityId, request.agentTypeID, request));
}

void CMNMPathfinder::CancelPathRequest(const MNM::QueuedPathID requestId)
{
	m_processingContextsPool.CancelPathRequest(requestId);

	CancelResultDispatchingForRequest(requestId);

	//Check if in the queue
	if (m_requestedPathsQueue.has(requestId))
	{
		m_requestedPathsQueue.erase(requestId);
	}
}

void CMNMPathfinder::CancelResultDispatchingForRequest(MNM::QueuedPathID requestId)
{
	m_pathfindingFailedEventsToDispatch.try_remove_and_erase_if(MNM::PathfinderUtils::IsPathfindingFailedEventRelatedToRequest(requestId));
	m_pathfindingCompletedEventsToDispatch.try_remove_and_erase_if(MNM::PathfinderUtils::IsPathfindingCompletedEventRelatedToRequest(requestId));
}

bool CMNMPathfinder::CheckIfPointsAreOnStraightWalkableLine(const NavigationMeshID& meshID, const Vec3& source, const Vec3& destination, float heightOffset) const
{
	return CheckIfPointsAreOnStraightWalkableLine(meshID, source, destination, nullptr, heightOffset);
}

bool CMNMPathfinder::CheckIfPointsAreOnStraightWalkableLine(const NavigationMeshID& meshID, const Vec3& source, const Vec3& destination, const INavMeshQueryFilter* pFilter, float heightOffset) const
{
	if (meshID == 0)
		return false;

	const NavigationMesh& mesh = gAIEnv.pNavigationSystem->GetMesh(meshID);
	const MNM::CNavMesh& navMesh = mesh.navMesh;

	const Vec3 raiseUp(0.0f, 0.0f, heightOffset);
	const Vec3 raisedSource = source + raiseUp;

	const MNM::vector3_t startMeshLoc = navMesh.ToMeshSpace(raisedSource);
	const MNM::vector3_t endMeshLoc = navMesh.ToMeshSpace(destination);

	const MNM::real_t verticalRange(2.0f);
	const MNM::TriangleID triStart =  navMesh.QueryTriangleAt(startMeshLoc, verticalRange, verticalRange, MNM::ENavMeshQueryOverlappingMode::BoundingBox_Partial, pFilter);
	const MNM::TriangleID triEnd = navMesh.QueryTriangleAt(endMeshLoc, verticalRange, verticalRange, MNM::ENavMeshQueryOverlappingMode::BoundingBox_Partial, pFilter);

	if (!triStart || !triEnd)
		return false;

	MNM::CNavMesh::RayCastRequest<512> raycastRequest;
	if (navMesh.RayCast(startMeshLoc, triStart, endMeshLoc, triEnd, raycastRequest, pFilter) != MNM::ERayCastResult::NoHit)
		return false;

	return true;
}

void CMNMPathfinder::SetupNewValidPathRequests()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	
	const size_t freeAvailableSlotsToProcessNewRequests = m_processingContextsPool.GetFreeSlotsCount();

	for (size_t i = 0; (i < freeAvailableSlotsToProcessNewRequests) && !m_requestedPathsQueue.empty(); ++i)
	{
		MNM::QueuedPathID idQueuedRequest = m_requestedPathsQueue.front_id();
		MNM::PathfinderUtils::QueuedRequest& requestToServe = m_requestedPathsQueue.front();

		MNM::PathfinderUtils::ProcessingContextId id = m_processingContextsPool.GetFirstAvailableContextId();
		if (id != MNM::PathfinderUtils::kInvalidProcessingContextId)
		{
			MNM::PathfinderUtils::ProcessingContext& processingContext = m_processingContextsPool.GetContextAtPosition(id);
			assert(processingContext.status == MNM::PathfinderUtils::ProcessingContext::Reserved);
			processingContext.workingSet.aStarNodesList.SetFrameTimeQuota(gAIEnv.CVars.pathfinder.MNMPathFinderQuota);

			const EMNMPathResult pathResult = SetupForNextPathRequest(idQueuedRequest, requestToServe, processingContext);
			if (pathResult != EMNMPathResult::Success)
			{
				m_processingContextsPool.ReleaseContext(id);

				MNM::PathfinderUtils::PathfindingFailedEvent failedEvent(idQueuedRequest, requestToServe, pathResult);
				m_pathfindingFailedEventsToDispatch.push_back(failedEvent);
			}
			else
			{
				processingContext.status = MNM::PathfinderUtils::ProcessingContext::InProgress;
			}
		}
		else
		{
			CRY_ASSERT_MESSAGE(0, "Trying to setup new requests while not having available slots in the pool.");
		}

		m_requestedPathsQueue.pop_front();
	}
}

void CMNMPathfinder::SpawnJobs()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	const bool isPathfinderMultithreaded = gAIEnv.CVars.pathfinder.MNMPathfinderMT != 0;
	if (isPathfinderMultithreaded)
	{
		m_processingContextsPool.ExecuteFunctionOnElements(functor(*this, &CMNMPathfinder::SpawnAppropriateJobIfPossible));
	}
	else
	{
		m_processingContextsPool.ExecuteFunctionOnElements(functor(*this, &CMNMPathfinder::ExecuteAppropriateOperationIfPossible));
	}
}

void CMNMPathfinder::SpawnAppropriateJobIfPossible(MNM::PathfinderUtils::ProcessingContext& processingContext)
{
	switch (processingContext.status)
	{
	case MNM::PathfinderUtils::ProcessingContext::InProgress:
		{
			SpawnPathfinderProcessingJob(processingContext);
			break;
		}
	case MNM::PathfinderUtils::ProcessingContext::FindWayCompleted:
		{
			SpawnPathConstructionJob(processingContext);
			break;
		}
	}
}

void CMNMPathfinder::ExecuteAppropriateOperationIfPossible(MNM::PathfinderUtils::ProcessingContext& processingContext)
{
	switch (processingContext.status)
	{
	case MNM::PathfinderUtils::ProcessingContext::InProgress:
		{
			ProcessPathRequest(processingContext);
			break;
		}
	case MNM::PathfinderUtils::ProcessingContext::FindWayCompleted:
		{
			ConstructPathIfWayWasFound(processingContext);
			break;
		}
	}
}

void CMNMPathfinder::SpawnPathfinderProcessingJob(MNM::PathfinderUtils::ProcessingContext& processingContext)
{
	CRY_ASSERT_MESSAGE(processingContext.status == MNM::PathfinderUtils::ProcessingContext::InProgress, "PathfinderProcessingJob is spawned even if the process is not 'InProgress'.");

	CRY_ASSERT_MESSAGE(!processingContext.jobState.IsRunning(), "The job is still running and we are spawning a new one.");

	PathfinderProcessingJob job(&processingContext);
	job.RegisterJobState(&processingContext.jobState);
	job.SetPriorityLevel(JobManager::eRegularPriority);
	job.Run();
}

void CMNMPathfinder::WaitForJobsToFinish()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	m_processingContextsPool.ExecuteFunctionOnElements(functor(*this, &CMNMPathfinder::WaitForJobToFinish));
}

void CMNMPathfinder::WaitForJobToFinish(MNM::PathfinderUtils::ProcessingContext& processingContext)
{
	gEnv->GetJobManager()->WaitForJob(processingContext.jobState);
}

void CMNMPathfinder::DispatchResults()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	if(m_pathfindingFailedEventsToDispatch.size())
	{
		MNM::PathfinderUtils::PathfindingFailedEvent failedEvent;
		while (m_pathfindingFailedEventsToDispatch.try_pop_back(failedEvent))
		{
			PathRequestFailed(failedEvent.requestId, failedEvent.request, failedEvent.result);
		}
	}

	if(m_pathfindingCompletedEventsToDispatch.size())
	{
		MNM::PathfinderUtils::PathfindingCompletedEvent succeeded;
		while (m_pathfindingCompletedEventsToDispatch.try_pop_back(succeeded))
		{
			succeeded.callback(succeeded.requestId, succeeded.eventData);
		}
	}
}

void CMNMPathfinder::Update()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (gAIEnv.CVars.pathfinder.MNMPathFinderDebug)
	{
		DebugAllStatistics();
	}

	DispatchResults();

	m_processingContextsPool.CleanupFinishedRequests();

	SetupNewValidPathRequests();

	SpawnJobs();
}

void CMNMPathfinder::OnNavigationMeshChanged(const NavigationMeshID meshId, const MNM::TileID tileId)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	for (size_t i = 0; i < m_processingContextsPool.GetMaxSlots(); ++i)
	{
		MNM::PathfinderUtils::ProcessingContext& processingContext = m_processingContextsPool.GetContextAtPosition(i);
		if (processingContext.status == MNM::PathfinderUtils::ProcessingContext::Invalid)
			continue;

		MNM::PathfinderUtils::ProcessingRequest& processingRequest = processingContext.processingRequest;
		if (!processingRequest.IsValid() || processingRequest.meshID != meshId)
			continue;

		if (!processingContext.workingSet.aStarNodesList.TileWasVisited(tileId))
		{
			bool neighbourTileWasVisited = false;

			const NavigationMesh& mesh = gAIEnv.pNavigationSystem->GetMesh(meshId);
			const MNM::vector3_t meshCoordinates = mesh.navMesh.GetTileContainerCoordinates(tileId);
			for (size_t side = 0; side < MNM::NavMesh::SideCount; ++side)
			{
				const MNM::TileID neighbourTileId = mesh.navMesh.GetNeighbourTileID(meshCoordinates.x.as_int(), meshCoordinates.y.as_int(), meshCoordinates.z.as_int(), side);

				if (processingContext.workingSet.aStarNodesList.TileWasVisited(neighbourTileId))
				{
					neighbourTileWasVisited = true;
					break;
				}
			}

			if (!neighbourTileWasVisited)
				continue;
		}

		//////////////////////////////////////////////////////////////////////////
		/// Re-start current request for next update

		// If the request was already completed, we don't want to dispatch it because it contains old data
		if (processingContext.status == MNM::PathfinderUtils::ProcessingContext::Completed)
		{
			CancelResultDispatchingForRequest(processingRequest.queuedID);
		}

		// Copy onto the stack to call function to avoid self delete.
		MNM::QueuedPathID requestId = processingRequest.queuedID;
		MNM::PathfinderUtils::QueuedRequest queuedRequest = processingRequest.data;

		const EMNMPathResult result = SetupForNextPathRequest(requestId, queuedRequest, processingContext);
		if (result != EMNMPathResult::Success)
		{
			// Context is released automatically because we set the context as invalid
			// Processing contexts with a invalid status are considered free by the pool, so they can be reused again
			processingContext.status = MNM::PathfinderUtils::ProcessingContext::Invalid;
			MNM::PathfinderUtils::PathfindingFailedEvent failedEvent(requestId, queuedRequest, result);
			m_pathfindingFailedEventsToDispatch.push_back(failedEvent);
		}
		else
		{
			processingContext.status = MNM::PathfinderUtils::ProcessingContext::InProgress;
		}
	}
}

EMNMPathResult CMNMPathfinder::SetupForNextPathRequest(MNM::QueuedPathID requestID, const MNM::PathfinderUtils::QueuedRequest& request, MNM::PathfinderUtils::ProcessingContext& processingContext)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	
	MNM::PathfinderUtils::ProcessingRequest& processingRequest = processingContext.processingRequest;
	processingRequest.Reset();

	//////////////////////////////////////////////////////////////////////////
	// Validate start/end locations
	const INavMeshQueryFilter* pFilter = request.pFilter.get();
	
	NavigationMeshID meshId;
	MNM::TriangleID startTriangleId;
	Vec3 safeStartLocation;

	if (!gAIEnv.pNavigationSystem->SnapToNavMesh(request.agentTypeID, request.requestParams.startLocation, request.requestParams.snappingMetrics, pFilter, &safeStartLocation, &startTriangleId, &meshId))
	{
		if (!meshId.IsValid())
		{
			const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(request.requesterEntityId);
			AIWarning("[CMNMPathfinder::SetupForNextPathRequest] Agent %s start position is not near a navigation volume.", pEntity ? pEntity->GetName() : "'missing entity'");
			return EMNMPathResult::FailedAgentOutsideNavigationVolume;
		}
		
		const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(request.requesterEntityId);
		AIWarning("Navigation system couldn't find NavMesh triangle at path start point (%.2f, %2f, %2f) for agent '%s'.",
			request.requestParams.startLocation.x, request.requestParams.startLocation.y, request.requestParams.startLocation.z,
			pEntity ? pEntity->GetName() : "'missing entity'");
		return EMNMPathResult::FailedToSnapStartPoint;
	}

	// Try to snap end location on the same NavMesh
	const MNM::CNavMesh& navMesh = gAIEnv.pNavigationSystem->GetMesh(meshId).navMesh;

	MNM::TriangleID endTriangleId;
	MNM::vector3_t snappedEndPosition;
	if (!navMesh.SnapPosition(navMesh.ToMeshSpace(request.requestParams.endLocation), request.requestParams.snappingMetrics, pFilter, &snappedEndPosition, &endTriangleId))
	{
		const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(request.requesterEntityId);
		AIWarning("Navigation system couldn't find NavMesh triangle at path destination point (%.2f, %.2f, %.2f) for agent '%s'.",
			request.requestParams.endLocation.x, request.requestParams.endLocation.y, request.requestParams.endLocation.z,
			pEntity ? pEntity->GetName() : "'missing entity'");
		return EMNMPathResult::FailedToSnapEndPoint;
	}

	const Vec3 safeEndLocation = navMesh.ToWorldSpace(snappedEndPosition).GetVec3();

	// The data for MNM are good until this point so we can set up the path finding
	processingRequest.meshID = meshId;
	processingRequest.fromTriangleID = startTriangleId;
	processingRequest.toTriangleID = endTriangleId;
	processingRequest.queuedID = requestID;

	processingRequest.data = request;
	processingRequest.data.requestParams.startLocation = safeStartLocation;
	processingRequest.data.requestParams.endLocation = safeEndLocation;

	const MNM::real_t startToEndDist = (MNM::vector3_t(safeEndLocation) - MNM::vector3_t(safeStartLocation)).lenNoOverflow();
	processingContext.workingSet.aStarNodesList.SetUpForPathSolving(navMesh.GetTriangleCount(), startTriangleId, request.requestParams.startLocation, startToEndDist);

	return EMNMPathResult::Success;
}

void CMNMPathfinder::ProcessPathRequest(MNM::PathfinderUtils::ProcessingContext& processingContext)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (processingContext.status != MNM::PathfinderUtils::ProcessingContext::InProgress)
		return;

	processingContext.workingSet.aStarNodesList.ResetConsumedTimeDuringCurrentFrame();

	MNM::PathfinderUtils::ProcessingRequest& processingRequest = processingContext.processingRequest;
	assert(processingRequest.IsValid());

	const NavigationMesh& mesh = gAIEnv.pNavigationSystem->GetMesh(processingRequest.meshID);
	const MNM::CNavMesh& navMesh = mesh.navMesh;
	const OffMeshNavigationManager* offMeshNavigationManager = gAIEnv.pNavigationSystem->GetOffMeshNavigationManager();
	assert(offMeshNavigationManager);
	const MNM::OffMeshNavigation& meshOffMeshNav = offMeshNavigationManager->GetOffMeshNavigationForMesh(processingRequest.meshID);

	MNM::CNavMesh::SWayQueryRequest inputParams(
		processingRequest.data.requesterEntityId, processingRequest.fromTriangleID,
		navMesh.ToMeshSpace(processingRequest.data.requestParams.startLocation), processingRequest.toTriangleID,
		navMesh.ToMeshSpace(processingRequest.data.requestParams.endLocation), meshOffMeshNav, *offMeshNavigationManager,
		processingRequest.data.GetDangersInfos(),
		processingRequest.data.pFilter.get(),
		processingRequest.data.requestParams.pCustomPathCostComputer);

	if (navMesh.FindWay(inputParams, processingContext.workingSet, processingContext.queryResult) == MNM::CNavMesh::eWQR_Continuing)
		return;

	processingContext.status = MNM::PathfinderUtils::ProcessingContext::FindWayCompleted;
	processingContext.workingSet.aStarNodesList.PathSolvingDone();

	return;
}

bool CMNMPathfinder::ConstructPathFromFoundWay(
  const MNM::SWayQueryResult& way,
  const MNM::CNavMesh& navMesh,
  const OffMeshNavigationManager* pOffMeshNavigationManager,
  const Vec3& startLocation,
  const Vec3& endLocation,
  CPathHolder<PathPointDescriptor>& outputPath)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	
	const MNM::WayTriangleData* pWayData = way.GetWayData();
	const size_t waySize = way.GetWaySize();

	// NOTE: waypoints are in reverse order
	for (size_t i = 0; i < waySize; ++i)
	{
		// Using the edge-midpoints of adjacent triangles to build the path.
		if (i > 0)
		{
			Vec3 edgeMidPoint;
			if (navMesh.CalculateMidEdge(pWayData[i - 1].triangleID, pWayData[i].triangleID, edgeMidPoint))
			{
				PathPointDescriptor pathPoint(IAISystem::NAV_UNSET, navMesh.ToWorldSpace(edgeMidPoint).GetVec3());
				pathPoint.iTriId = pWayData[i].triangleID;
				outputPath.PushFront(pathPoint);
			}
		}

		if (pWayData[i].offMeshLinkID)
		{
			// Grab off-mesh link object
			const MNM::IOffMeshLink* pOffMeshLink = pOffMeshNavigationManager->GetOffMeshLink(pWayData[i].offMeshLinkID);
			if (pOffMeshLink == nullptr)
			{
				// Link can no longer be found; this path is now invalid
				return false;
			}

			const bool isOffMeshLinkSmartObject = pOffMeshLink->IsTypeOf<OffMeshLink_SmartObject>();
			IAISystem::ENavigationType type = isOffMeshLinkSmartObject ? IAISystem::NAV_SMARTOBJECT : IAISystem::NAV_CUSTOM_NAVIGATION;

			// Add Entry/Exit points
			PathPointDescriptor pathPoint(IAISystem::NAV_UNSET);

			// Add Exit point
			pathPoint.vPos = pOffMeshLink->GetEndPosition();
			CRY_ASSERT_MESSAGE(i > 0, "Path contains offmesh link without exit waypoint");
			if (i > 0)
			{
				pathPoint.iTriId = pWayData[i - 1].triangleID;
			}
			outputPath.PushFront(pathPoint);

			// Add Entry point
			pathPoint.navType = type;
			pathPoint.vPos = pOffMeshLink->GetStartPosition();
			pathPoint.offMeshLinkData.offMeshLinkID = pWayData[i].offMeshLinkID;
			pathPoint.iTriId = pWayData[i].triangleID;
			outputPath.PushFront(pathPoint);
		}
	}

	PathPointDescriptor navPathStart(IAISystem::NAV_UNSET, startLocation);
	PathPointDescriptor navPathEnd(IAISystem::NAV_UNSET, endLocation);

	// Assign triangleID of start and end points (waypoints are in reverse order)
	navPathEnd.iTriId = pWayData[0].triangleID;
	navPathStart.iTriId = pWayData[waySize - 1].triangleID;

	//Insert start/end locations in the path
	outputPath.PushBack(navPathEnd);
	outputPath.PushFront(navPathStart);

	return true;
}

void CMNMPathfinder::ConstructPathIfWayWasFound(MNM::PathfinderUtils::ProcessingContext& processingContext)
{
	if (processingContext.status != MNM::PathfinderUtils::ProcessingContext::FindWayCompleted)
		return;

	CRY_PROFILE_FUNCTION_ARG(PROFILE_AI, CryStringUtils::toString(static_cast<unsigned>(processingContext.queryResult.GetWaySize())));

	MNM::PathfinderUtils::ProcessingRequest& processingRequest = processingContext.processingRequest;

	const NavigationMesh& mesh = gAIEnv.pNavigationSystem->GetMesh(processingRequest.meshID);
	const MNM::CNavMesh& navMesh = mesh.navMesh;
	const OffMeshNavigationManager* offMeshNavigationManager = gAIEnv.pNavigationSystem->GetOffMeshNavigationManager();
	assert(offMeshNavigationManager);

	const bool bPathFound = (processingContext.queryResult.GetWaySize() != 0);
	bool bPathConstructed = false;
	CPathHolder<PathPointDescriptor> outputPath;
	if (bPathFound)
	{
		++processingContext.constructedPathsCount;

		CTimeValue timeBeforePathConstruction;
		if (gAIEnv.CVars.pathfinder.MNMPathFinderDebug)
		{
			timeBeforePathConstruction = gEnv->pTimer->GetAsyncCurTime();
		}

		bPathConstructed = ConstructPathFromFoundWay(
		  processingContext.queryResult,
		  navMesh,
		  offMeshNavigationManager,
		  processingRequest.data.requestParams.startLocation,
		  processingRequest.data.requestParams.endLocation,
		  *&outputPath);

		if (gAIEnv.CVars.pathfinder.MNMPathFinderDebug)
		{
			CTimeValue timeAfterPathConstruction = gEnv->pTimer->GetAsyncCurTime();
			const float constructionPathTime = timeAfterPathConstruction.GetDifferenceInSeconds(timeBeforePathConstruction) * 1000.0f;
			processingContext.totalTimeConstructPath += constructionPathTime;
			processingContext.peakTimeConstructPath = max(processingContext.peakTimeConstructPath, constructionPathTime);
		}

		if (bPathConstructed)
		{
			if (processingRequest.data.requestParams.beautify && gAIEnv.CVars.pathfinder.BeautifyPath)
			{
				CTimeValue timeBeforeBeautifyPath;
				if (gAIEnv.CVars.pathfinder.MNMPathFinderDebug)
				{
					timeBeforeBeautifyPath = gEnv->pTimer->GetAsyncCurTime();
				}

				outputPath.PullPathOnNavigationMesh(navMesh, gAIEnv.CVars.pathfinder.PathStringPullingIterations, processingRequest.data.requestParams.pCustomPathCostComputer.get());

				if (gAIEnv.CVars.pathfinder.MNMPathFinderDebug)
				{
					CTimeValue timeAfterBeautifyPath = gEnv->pTimer->GetAsyncCurTime();
					const float beautifyPathTime = timeAfterBeautifyPath.GetDifferenceInSeconds(timeBeforeBeautifyPath) * 1000.0f;
					processingContext.totalTimeBeautifyPath += beautifyPathTime;
					processingContext.peakTimeBeautifyPath = max(processingContext.peakTimeBeautifyPath, beautifyPathTime);
				}
			}
		}
	}

	MNM::PathfinderUtils::PathfindingCompletedEvent successEvent;
	MNMPathRequestResult& resultData = successEvent.eventData;
	if (bPathConstructed)
	{
		INavPathPtr navPath = resultData.pPath;
		resultData.result = EMNMPathResult::Success;
		navPath->Clear("CMNMPathfinder::ProcessPathRequest");

		outputPath.FillNavPath(*navPath);

		const MNMPathRequest& requestParams = processingRequest.data.requestParams;
		SNavPathParams pparams(requestParams.startLocation, requestParams.endLocation, Vec3(0.0f, 0.0f, 0.0f), requestParams.endDirection, requestParams.forceTargetBuildingId,
		                       requestParams.allowDangerousDestination, requestParams.endDistance);
		pparams.meshID = processingRequest.meshID;

		navPath->SetParams(pparams);
		navPath->SetEndDir(requestParams.endDirection);
	}
	else
	{
		resultData.result = bPathFound ? EMNMPathResult::FailedToConstructPath : EMNMPathResult::FailedNoPathFound;
	}

	successEvent.callback = processingRequest.data.requestParams.resultCallback;
	successEvent.requestId = processingRequest.queuedID;

	m_pathfindingCompletedEventsToDispatch.push_back(successEvent);
	processingContext.status = MNM::PathfinderUtils::ProcessingContext::Completed;
	return;
}

void CMNMPathfinder::SpawnPathConstructionJob(MNM::PathfinderUtils::ProcessingContext& processingContext)
{
	CRY_ASSERT_MESSAGE(processingContext.status == MNM::PathfinderUtils::ProcessingContext::FindWayCompleted, "PathConstructionJob spawned even if the status is not 'FindWayCompleted'");

	CRY_ASSERT_MESSAGE(!processingContext.jobState.IsRunning(), "The job is still running and we are spawning a new one.");

	PathConstructionJob job(&processingContext);
	job.RegisterJobState(&processingContext.jobState);
	job.SetPriorityLevel(JobManager::eRegularPriority);
	job.Run();
}

void CMNMPathfinder::PathRequestFailed(MNM::QueuedPathID requestID, const MNM::PathfinderUtils::QueuedRequest& request, const EMNMPathResult result)
{
	MNMPathRequestResult requestResult;
	requestResult.result = result;
	request.requestParams.resultCallback(requestID, requestResult);
}

void CMNMPathfinder::DebugAllStatistics()
{
	float y = 40.0f;
	IRenderAuxText::Draw2dLabel(100.0f, y, 1.4f, Col_White, false, "Currently we have %" PRISIZE_T " queued requests in the MNMPathfinder", m_requestedPathsQueue.size());

	y += 50.0f;
	const size_t maximumAmountOfSlotsToUpdate = m_processingContextsPool.GetMaxSlots();
	for (size_t i = 0; i < maximumAmountOfSlotsToUpdate; ++i)
	{
		MNM::PathfinderUtils::ProcessingContext& processingContext = m_processingContextsPool.GetContextAtPosition(i);
		DebugStatistics(processingContext, i, (y + (i * 120.0f)));
	}
}

void CMNMPathfinder::DebugStatistics(MNM::PathfinderUtils::ProcessingContext& processingContext, const int contextNumber, const float textY)
{
	stack_string text;

	MNM::AStarContention::ContentionStats stats = processingContext.workingSet.aStarNodesList.GetContentionStats();

	const float avgTimeConstructPath = processingContext.constructedPathsCount > 0 ? processingContext.totalTimeConstructPath / processingContext.constructedPathsCount : 0.0f;
	const float avgTimeBeautifyPath = processingContext.constructedPathsCount > 0 ? processingContext.totalTimeBeautifyPath / processingContext.constructedPathsCount : 0.0f;

	text.Format(
	  "MNMPathFinder Context %d - Frame time quota (%f ms) - EntityId: %d AgentTypeId: %d - Status: %s\n"
	  "\tPath count:              %d\n"
	  "\tAStar steps:             Avg - %d / Max - %d\n"
	  "\tAStar time:              Avg - %2.5f ms / Max - %2.5f ms\n"
	  "\tConstruction time:       Avg - %2.5f ms / Max - %2.5f ms\n"
	  "\tBeautify time:           Avg - %2.5f ms / Max - %2.5f ms",
	  contextNumber,
	  stats.frameTimeQuota,
	  processingContext.processingRequest.data.requestParams.requesterEntityId,
	  (uint32)processingContext.processingRequest.data.requestParams.agentTypeID,
	  processingContext.GetStatusAsString(),
	  processingContext.constructedPathsCount,
	  stats.averageSearchSteps,
	  stats.peakSearchSteps,
	  stats.averageSearchTime,
	  stats.peakSearchTime,
	  avgTimeConstructPath,
	  processingContext.peakTimeConstructPath,
	  avgTimeBeautifyPath,
	  processingContext.peakTimeBeautifyPath
	);

	IRenderAuxText::Draw2dLabel(100.f, textY, 1.4f, Col_White, false, "%s", text.c_str());
}
