// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "MNMPathfinder.h"
#include "Navigation/NavigationSystem/NavigationSystem.h"
#include "DebugDrawContext.h"
#include "AIBubblesSystem/AIBubblesSystem.h"
#include "Navigation/PathHolder.h"
#include <CryThreading/IJobManager_JobDelegator.h>

//#pragma optimize("", off)
//#pragma inline_depth(0)

inline Vec3 TriangleCenter(const Vec3& a, const Vec3& b, const Vec3& c)
{
	return (a + b + c) / 3.f;
}

//////////////////////////////////////////////////////////////////////////

void MNM::PathfinderUtils::QueuedRequest::SetupDangerousLocationsData()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_AI);

	dangerousAreas.clear();
	if (requestParams.dangersToAvoidFlags == eMNMDangers_None)
		return;

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

void MNM::PathfinderUtils::QueuedRequest::SetupAttentionTargetDanger()
{
	if (dangerousAreas.size() >= MNM::max_danger_amount)
		return;

	if (pRequester)
	{
		CAIActor* pActor = CastToCAIActorSafe(pRequester->GetPathAgentEntity()->GetAI());
		if (pActor)
		{
			IAIObject* pAttTarget = pActor->GetAttentionTarget();
			if (pAttTarget)
			{
				const IEntity* pEntity = pAttTarget->GetEntity();
				const float effectRange = 0.0f; // Effect over all the world
				const Vec3& dangerPosition = pEntity ? pEntity->GetPos() : pAttTarget->GetPos();
				MNM::DangerAreaConstPtr info;
				info.reset(new MNM::DangerAreaT<MNM::eWCT_Direction>(dangerPosition, effectRange, gAIEnv.CVars.PathfinderDangerCostForAttentionTarget));
				dangerousAreas.push_back(info);
			}
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
	const float maxDistanceSq = sqr(gAIEnv.CVars.PathfinderExplosiveDangerMaxThreatDistance);
	const float maxDistanceSqToMergeExplosivesThreat = 3.0f;

	std::vector<Vec3> grenadesLocations;
	AutoAIObjectIter grenadesIterator(gAIEnv.pAIObjectManager->GetFirstAIObject(OBJFILTER_TYPE, AIOBJECT_GRENADE));

	while (dangerousAreas.size() < MNM::max_danger_amount && grenadesIterator->GetObject())
	{
		const Vec3& pos = grenadesIterator->GetObject()->GetPos();
		if (Distance::Point_Point2DSq(pos, pRequester->GetPathAgentEntity()->GetPos()) < maxDistanceSq)
		{
			MNM::DangerousAreasList::const_iterator it = std::find_if(dangerousAreas.begin(), dangerousAreas.end(),
			                                                          DangerAlreadyStoredInRangeFromPositionPred(pos, maxDistanceSqToMergeExplosivesThreat));
			if (it == dangerousAreas.end())
			{
				MNM::DangerAreaConstPtr info;
				info.reset(new MNM::DangerAreaT<MNM::eWCT_Range>(pos, gAIEnv.CVars.PathfinderExplosiveDangerRadius,
				                                                 gAIEnv.CVars.PathfinderDangerCostForExplosives));
				dangerousAreas.push_back(info);
			}
		}

		grenadesIterator->Next();
	}
}

void MNM::PathfinderUtils::QueuedRequest::SetupGroupMatesAvoidance()
{
	IF_UNLIKELY (!pRequester)
	{
		return;
	}

	IAIObject* requesterAIObject = pRequester->GetPathAgentEntity()->GetAI();
	IF_UNLIKELY (!requesterAIObject)
	{
		return;
	}

	const int requesterGroupId = requesterAIObject->GetGroupId();
	AutoAIObjectIter groupMemberIterator(gEnv->pAISystem->GetAIObjectManager()->GetFirstAIObject(OBJFILTER_GROUP, requesterGroupId));
	while (dangerousAreas.size() < MNM::max_danger_amount && groupMemberIterator->GetObject())
	{
		IAIObject* groupMemberAIObject = groupMemberIterator->GetObject();
		if (requesterAIObject->GetEntityID() != groupMemberAIObject->GetEntityID())
		{
			MNM::DangerAreaConstPtr dangerArea;
			dangerArea.reset(new MNM::DangerAreaT<MNM::eWCT_Range>(
			                   groupMemberAIObject->GetPos(),
			                   gAIEnv.CVars.PathfinderGroupMatesAvoidanceRadius,
			                   gAIEnv.CVars.PathfinderAvoidanceCostForGroupMates
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
	m_pathfindingFailedEventsToDispatch.reserve(gAIEnv.CVars.MNMPathfinderConcurrentRequests);
	m_pathfindingCompletedEventsToDispatch.reserve(gAIEnv.CVars.MNMPathfinderConcurrentRequests);
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
		PathRequestFailed(m_requestedPathsQueue.front_id(), m_requestedPathsQueue.front());
		m_requestedPathsQueue.pop_front();
	}

	m_processingContextsPool.Reset();
	m_pathfindingFailedEventsToDispatch.clear();
	m_pathfindingCompletedEventsToDispatch.clear();
}

MNM::QueuedPathID CMNMPathfinder::RequestPathTo(const IAIPathAgent* pRequester, const MNMPathRequest& request)
{
	//////////////////////////////////////////////////////////////////////////
	// Validate requester
	if (!pRequester || (pRequester->GetPathAgentType() != AIOBJECT_ACTOR && pRequester->GetPathAgentType() != AIOBJECT_VEHICLE))
	{
		AIWarning("[CMNMPathfinder::QueuePathRequest] No agent specified. A NavigationTypeID is needed to find the appropriate mesh.");
		return MNM::Constants::eQueuedPathID_InvalidID;
	}

	//////////////////////////////////////////////////////////////////////////
	// Validate Agent Type
	const char* actorName = pRequester->GetPathAgentName();

	const IEntity* pEntity = pRequester->GetPathAgentEntity();
	EntityId entityId = pEntity ? pEntity->GetId() : 0;

	if (!request.agentTypeID)
	{
		AIWarning("[CMNMPathfinder::QueuePathRequest] Request from agent %s has no NavigationType defined.", actorName);
		return MNM::Constants::eQueuedPathID_InvalidID;
	}

	//////////////////////////////////////////////////////////////////////////
	// Validate callback
	if (!request.resultCallback)
	{
		AIWarning("[CMNMPathfinder::QueuePathRequest] Agent %s does not provide a result Callback", actorName);
		return 0;
	}

	//////////////////////////////////////////////////////////////////////////
	// Validate start/end locations
	const NavigationMeshID meshID = gAIEnv.pNavigationSystem->GetEnclosingMeshID(request.agentTypeID, request.startLocation);

	if (!meshID)
	{
		AIQueueBubbleMessage("CMNMPathfinder::RequestPathTo-NoMesh", entityId,
		                     "I m not inside a Navigation area for my navigation type.",
		                     eBNS_LogWarning);
		return 0;
	}

	return m_requestedPathsQueue.push_back(MNM::PathfinderUtils::QueuedRequest(pRequester, request.agentTypeID, request));
}

void CMNMPathfinder::CancelPathRequest(MNM::QueuedPathID requestId)
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
	if (meshID == 0)
		return false;

	const NavigationMesh& mesh = gAIEnv.pNavigationSystem->GetMesh(meshID);
	const MNM::MeshGrid& grid = mesh.grid;

	const Vec3 raiseUp(0.0f, 0.0f, heightOffset);
	Vec3 raisedSource = source + raiseUp;

	MNM::vector3_t startLoc = MNM::vector3_t(MNM::real_t(raisedSource.x), MNM::real_t(raisedSource.y), MNM::real_t(raisedSource.z));
	MNM::vector3_t endLoc = MNM::vector3_t(MNM::real_t(destination.x), MNM::real_t(destination.y), MNM::real_t(destination.z));

	const MNM::real_t verticalRange(2.0f);
	MNM::TriangleID triStart = grid.GetTriangleAt(startLoc, verticalRange, verticalRange);
	MNM::TriangleID triEnd = grid.GetTriangleAt(endLoc, verticalRange, verticalRange);

	if (!triStart || !triEnd)
		return false;

	MNM::MeshGrid::RayCastRequest<512> raycastRequest;

	if (grid.RayCast(startLoc, triStart, endLoc, triEnd, raycastRequest) != MNM::MeshGrid::eRayCastResult_NoHit)
		return false;

	return true;
}

void CMNMPathfinder::SetupNewValidPathRequests()
{
	const size_t freeAvailableSlotsToProcessNewRequests = m_processingContextsPool.GetFreeSlotsCount();

	for (size_t i = 0; (i < freeAvailableSlotsToProcessNewRequests) && !m_requestedPathsQueue.empty(); ++i)
	{
		MNM::QueuedPathID idQueuedRequest = m_requestedPathsQueue.front_id();
		MNM::PathfinderUtils::QueuedRequest requestToServe = m_requestedPathsQueue.front();
		m_requestedPathsQueue.pop_front();

		MNM::PathfinderUtils::ProcessingContextsPool::PoolIterator pProcessingContext;
		MNM::PathfinderUtils::ProcessingContextId id;
		id = m_processingContextsPool.GetFirstAvailableContextId();
		if (id != MNM::PathfinderUtils::kInvalidProcessingContextId)
		{
			MNM::PathfinderUtils::ProcessingContext& processingContext = m_processingContextsPool.GetContextAtPosition(id);
			assert(processingContext.status == MNM::PathfinderUtils::ProcessingContext::Reserved);
			processingContext.workingSet.aStarOpenList.SetFrameTimeQuota(gAIEnv.CVars.MNMPathFinderQuota);

			bool hasSetupSucceeded = SetupForNextPathRequest(idQueuedRequest, requestToServe, processingContext);
			if (!hasSetupSucceeded)
			{
				m_processingContextsPool.ReleaseContext(id);

				MNM::PathfinderUtils::PathfindingFailedEvent failedEvent(idQueuedRequest, requestToServe);
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
	}
}

void CMNMPathfinder::SpawnJobs()
{
	const bool isPathfinderMultithreaded = gAIEnv.CVars.MNMPathfinderMT != 0;
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
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);
	m_processingContextsPool.ExecuteFunctionOnElements(functor(*this, &CMNMPathfinder::WaitForJobToFinish));
}

void CMNMPathfinder::WaitForJobToFinish(MNM::PathfinderUtils::ProcessingContext& processingContext)
{
	gEnv->GetJobManager()->WaitForJob(processingContext.jobState);
}

void CMNMPathfinder::DispatchResults()
{
	{
		MNM::PathfinderUtils::PathfindingFailedEvent failedEvent;
		while (m_pathfindingFailedEventsToDispatch.try_pop_back(failedEvent))
		{
			PathRequestFailed(failedEvent.requestId, failedEvent.request);
		}
	}

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
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

	if (gAIEnv.CVars.MNMPathFinderDebug)
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
	const size_t maximumAmountOfSlotsToUpdate = m_processingContextsPool.GetOccupiedSlotsCount();
	for (size_t i = 0; i < maximumAmountOfSlotsToUpdate; ++i)
	{
		MNM::PathfinderUtils::ProcessingContext& processingContext = m_processingContextsPool.GetContextAtPosition(i);
		MNM::PathfinderUtils::ProcessingRequest& processingRequest = processingContext.processingRequest;

		if (!processingRequest.IsValid())
			return;

		if (processingRequest.meshID != meshId)
			return;

		if (!processingContext.workingSet.aStarOpenList.TileWasVisited(tileId))
		{
			bool neighbourTileWasVisited = false;

			const NavigationMesh& mesh = gAIEnv.pNavigationSystem->GetMesh(meshId);
			const MNM::vector3_t meshCoordinates = mesh.grid.GetTileContainerCoordinates(tileId);

			for (size_t side = 0; side < MNM::MeshGrid::SideCount; ++side)
			{
				const MNM::TileID neighbourTileId = mesh.grid.GetNeighbourTileID(meshCoordinates.x.as_int(), meshCoordinates.y.as_int(), meshCoordinates.z.as_int(), side);

				if (processingContext.workingSet.aStarOpenList.TileWasVisited(neighbourTileId))
				{
					neighbourTileWasVisited = true;
					break;
				}
			}

			if (!neighbourTileWasVisited)
				return;
		}

		//////////////////////////////////////////////////////////////////////////
		/// Re-start current request for next update

		// Copy onto the stack to call function to avoid self delete.
		MNM::QueuedPathID requestId = processingRequest.queuedID;
		MNM::PathfinderUtils::QueuedRequest requestParams = processingRequest.data;

		if (!SetupForNextPathRequest(requestId, requestParams, processingContext))
		{
			PathRequestFailed(requestId, requestParams);
		}
	}
}

bool CMNMPathfinder::SetupForNextPathRequest(MNM::QueuedPathID requestID, MNM::PathfinderUtils::QueuedRequest& request, MNM::PathfinderUtils::ProcessingContext& processingContext)
{
	MNM::PathfinderUtils::ProcessingRequest& processingRequest = processingContext.processingRequest;
	processingRequest.Reset();

	//////////////////////////////////////////////////////////////////////////
	// Validate start/end locations
	const NavigationMeshID meshID = gAIEnv.pNavigationSystem->GetEnclosingMeshID(request.agentTypeID, request.requestParams.startLocation);

	if (!meshID)
	{
		AIWarning("[CMNMPathfinder::SetupForNextPathRequest] Agent %s is not inside a navigation volume.", request.pRequester->GetPathAgentName());
		return false;
	}

	const NavigationMesh& mesh = gAIEnv.pNavigationSystem->GetMesh(meshID);
	const MNM::MeshGrid& grid = mesh.grid;

	const MNM::MeshGrid::Params& gridParams = grid.GetParams();
	const MNM::vector3_t origin = MNM::vector3_t(MNM::real_t(gridParams.origin.x), MNM::real_t(gridParams.origin.y), MNM::real_t(gridParams.origin.z));

	const uint16 agentRadiusUnits = gAIEnv.pNavigationSystem->GetAgentRadiusInVoxelUnits(request.agentTypeID);
	const uint16 agentHeightUnits = gAIEnv.pNavigationSystem->GetAgentHeightInVoxelUnits(request.agentTypeID);

	const MNM::vector3_t startLocation(MNM::real_t(request.requestParams.startLocation.x), MNM::real_t(request.requestParams.startLocation.y),
	                                   MNM::real_t(request.requestParams.startLocation.z));
	const MNM::vector3_t endLocation(MNM::real_t(request.requestParams.endLocation.x), MNM::real_t(request.requestParams.endLocation.y),
	                                 MNM::real_t(request.requestParams.endLocation.z));
	const Vec3 voxelSize = grid.GetParams().voxelSize;
	const MNM::real_t horizontalRange = MNMUtils::CalculateMinHorizontalRange(agentRadiusUnits, voxelSize.x);
	const MNM::real_t verticalRange = MNMUtils::CalculateMinVerticalRange(agentHeightUnits, voxelSize.z);

	AgentType agentTypeProperties;
	const bool arePropertiesValid = gAIEnv.pNavigationSystem->GetAgentTypeProperties(request.agentTypeID, agentTypeProperties);
	assert(arePropertiesValid);
	const uint16 zOffsetMultiplier = min(uint16(2), agentTypeProperties.settings.heightVoxelCount);
	const MNM::real_t verticalUpwardRange = arePropertiesValid ? MNM::real_t(zOffsetMultiplier * agentTypeProperties.settings.voxelSize.z) : MNM::real_t(.0f);

	Vec3 safeStartLocation(request.requestParams.startLocation);
	MNM::TriangleID triangleStartID;
	if (!(triangleStartID = grid.GetTriangleAt(startLocation - origin, verticalRange, verticalUpwardRange)))
	{
		MNM::vector3_t closest;
		if (!(triangleStartID = grid.GetClosestTriangle(startLocation - origin, verticalRange, horizontalRange, NULL, &closest)))
		{
			AIWarning("Navigation system couldn't find NavMesh triangle at path start point (%.2f, %2f, %2f) for agent '%s'.",
			          request.requestParams.startLocation.x, request.requestParams.startLocation.y, request.requestParams.startLocation.z,
			          request.pRequester->GetPathAgentName());
			return false;
		}
		else
		{
			safeStartLocation = closest.GetVec3();
		}
	}

	Vec3 safeEndLocation(request.requestParams.endLocation);
	MNM::TriangleID triangleEndID = grid.GetTriangleAt(endLocation - origin, verticalRange, verticalRange);
	if (!triangleEndID)
	{
		MNM::vector3_t closest;
		triangleEndID = grid.GetClosestTriangle(endLocation - origin, verticalRange, horizontalRange, NULL, &closest);
		if (triangleEndID)
		{
			safeEndLocation = closest.GetVec3();
		}
		else
		{
			AIWarning("Navigation system couldn't find NavMesh triangle at path destination point (%.2f, %.2f, %.2f) for agent '%s'.",
			          request.requestParams.endLocation.x, request.requestParams.endLocation.y, request.requestParams.endLocation.z,
			          request.pRequester->GetPathAgentName());
			return false;
		}
	}

	// The data for MNM are good until this point so we can set up the path finding

	processingRequest.pRequester = request.pRequester;
	processingRequest.meshID = meshID;
	processingRequest.fromTriangleID = triangleStartID;
	processingRequest.toTriangleID = triangleEndID;
	processingRequest.queuedID = requestID;

	processingRequest.data = request;
	processingRequest.data.requestParams.startLocation = safeStartLocation;
	processingRequest.data.requestParams.endLocation = safeEndLocation;

	const MNM::real_t startToEndDist = (endLocation - startLocation).lenNoOverflow();
	processingContext.workingSet.aStarOpenList.SetUpForPathSolving(mesh.grid.GetTriangleCount(), triangleStartID, startLocation, startToEndDist);

	return true;
}

void CMNMPathfinder::ProcessPathRequest(MNM::PathfinderUtils::ProcessingContext& processingContext)
{
	if (processingContext.status != MNM::PathfinderUtils::ProcessingContext::InProgress)
		return;

	processingContext.workingSet.aStarOpenList.ResetConsumedTimeDuringCurrentFrame();

	MNM::PathfinderUtils::ProcessingRequest& processingRequest = processingContext.processingRequest;
	assert(processingRequest.IsValid());

	const NavigationMesh& mesh = gAIEnv.pNavigationSystem->GetMesh(processingRequest.meshID);
	const MNM::MeshGrid& grid = mesh.grid;
	const MNM::MeshGrid::Params& gridParams = grid.GetParams();
	const OffMeshNavigationManager* offMeshNavigationManager = gAIEnv.pNavigationSystem->GetOffMeshNavigationManager();
	assert(offMeshNavigationManager);
	const MNM::OffMeshNavigation& meshOffMeshNav = offMeshNavigationManager->GetOffMeshNavigationForMesh(processingRequest.meshID);

	MNM::MeshGrid::WayQueryRequest inputParams(processingRequest.pRequester, processingRequest.fromTriangleID,
	                                           processingRequest.data.requestParams.startLocation - gridParams.origin, processingRequest.toTriangleID,
	                                           processingRequest.data.requestParams.endLocation - gridParams.origin, meshOffMeshNav, *offMeshNavigationManager,
	                                           processingRequest.data.GetDangersInfos());

	if (grid.FindWay(inputParams, processingContext.workingSet, processingContext.queryResult) == MNM::MeshGrid::eWQR_Continuing)
		return;

	processingContext.status = MNM::PathfinderUtils::ProcessingContext::FindWayCompleted;
	return;
}

void CMNMPathfinder::ConstructPathIfWayWasFound(MNM::PathfinderUtils::ProcessingContext& processingContext)
{
	if (processingContext.status != MNM::PathfinderUtils::ProcessingContext::FindWayCompleted)
		return;

	MNM::PathfinderUtils::ProcessingRequest& processingRequest = processingContext.processingRequest;

	const NavigationMesh& mesh = gAIEnv.pNavigationSystem->GetMesh(processingRequest.meshID);
	const MNM::MeshGrid& grid = mesh.grid;
	const MNM::MeshGrid::Params& gridParams = grid.GetParams();
	const OffMeshNavigationManager* offMeshNavigationManager = gAIEnv.pNavigationSystem->GetOffMeshNavigationManager();
	assert(offMeshNavigationManager);

	CPathHolder<PathPointDescriptor> outputPath;

	PathPointDescriptor navPathStart(IAISystem::NAV_UNSET, processingRequest.data.requestParams.startLocation);
	PathPointDescriptor navPathEnd(IAISystem::NAV_UNSET, processingRequest.data.requestParams.endLocation);

	const MNM::vector3_t origin = MNM::vector3_t(MNM::real_t(gridParams.origin.x), MNM::real_t(gridParams.origin.y), MNM::real_t(gridParams.origin.z));

	// NOTE: waypoints are in reverse order
	size_t waySize = processingContext.queryResult.GetWaySize();
	MNM::WayTriangleData* outputWay = processingContext.queryResult.GetWayData();
	for (size_t i = 0; i < waySize; ++i)
	{
		// Using the edge-midpoints of adjacent triangles to build the path.
		if (i > 0)
		{
			Vec3 edgeMidPoint;
			if (grid.CalculateMidEdge(outputWay[i - 1].triangleID, outputWay[i].triangleID, edgeMidPoint))
			{
				PathPointDescriptor pathPoint(IAISystem::NAV_UNSET, edgeMidPoint + origin.GetVec3());
				pathPoint.iTriId = outputWay[i].triangleID;
				outputPath.PushFront(pathPoint);
			}
		}

		if (outputWay[i].offMeshLinkID)
		{
			// Grab off-mesh link object
			const MNM::OffMeshLink* pOffMeshLink = offMeshNavigationManager->GetOffMeshLink(outputWay[i].offMeshLinkID);
			if (pOffMeshLink == NULL)
			{
				// Link can no longer be found; this path is now invalid
				processingContext.queryResult.Clear();
				break;
			}

			const bool isOffMeshLinkSmartObject = pOffMeshLink->GetLinkType() == MNM::OffMeshLink::eLinkType_SmartObject;
			IAISystem::ENavigationType type = isOffMeshLinkSmartObject ? IAISystem::NAV_SMARTOBJECT : IAISystem::NAV_CUSTOM_NAVIGATION;

			// Add Entry/Exit points
			PathPointDescriptor pathPoint(IAISystem::NAV_UNSET);

			// Add Exit point
			pathPoint.vPos = pOffMeshLink->GetEndPosition();
			outputPath.PushFront(pathPoint);

			// Add Entry point
			pathPoint.navType = type;
			pathPoint.vPos = pOffMeshLink->GetStartPosition();
			pathPoint.offMeshLinkData.offMeshLinkID = outputWay[i].offMeshLinkID;
			pathPoint.iTriId = outputWay[i].triangleID;
			outputPath.PushFront(pathPoint);
		}
	}
	const bool pathFound = (processingContext.queryResult.GetWaySize() != 0);

	MNM::PathfinderUtils::PathfindingCompletedEvent successEvent;
	MNMPathRequestResult& resultData = successEvent.eventData;
	if (pathFound)
	{
		INavPathPtr navPath = resultData.pPath;
		resultData.result = eMNMPR_Success;
		navPath->Clear("CMNMPathfinder::ProcessPathRequest");
		//Insert start/end locations in the path
		outputPath.PushBack(navPathEnd);
		outputPath.PushFront(navPathStart);

		if (processingRequest.data.requestParams.beautify && gAIEnv.CVars.BeautifyPath)
		{
			outputPath.PullPathOnNavigationMesh(processingRequest.meshID, gAIEnv.CVars.PathStringPullingIterations, &outputWay[0], processingContext.queryResult.GetWaySize());
		}

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
		resultData.result = eMNMPR_NoPathFound;
	}

	successEvent.callback = processingRequest.data.requestParams.resultCallback;
	successEvent.requestId = processingRequest.queuedID;

	processingRequest.Reset();
	processingContext.workingSet.aStarOpenList.PathSolvingDone();

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

void CMNMPathfinder::PathRequestFailed(MNM::QueuedPathID requestID, const MNM::PathfinderUtils::QueuedRequest& request)
{
	MNMPathRequestResult result;
	request.requestParams.resultCallback(requestID, result);
}

void CMNMPathfinder::DebugAllStatistics()
{
	float y = 40.0f;
	gEnv->pRenderer->Draw2dLabel(100.f, y, 1.4f, Col_White, false, "Currently we have %" PRISIZE_T " queued requests in the MNMPathfinder", m_requestedPathsQueue.size());

	y += 100.0f;
	const size_t maximumAmountOfSlotsToUpdate = m_processingContextsPool.GetMaxSlots();
	for (size_t i = 0; i < maximumAmountOfSlotsToUpdate; ++i)
	{
		MNM::PathfinderUtils::ProcessingContext& processingContext = m_processingContextsPool.GetContextAtPosition(i);
		DebugStatistics(processingContext, (y + (i * 100.f)));
	}
}

void CMNMPathfinder::DebugStatistics(MNM::PathfinderUtils::ProcessingContext& processingContext, const float textY)
{
	stack_string text;

	MNM::AStarContention::ContentionStats stats = processingContext.workingSet.aStarOpenList.GetContentionStats();

	text.Format(
	  "MNMPathFinder - Frame time quota (%f ms) - EntityId: %d - Status: %s\n"
	  "---------\n"
	  "AStar steps: Average - %d / Maximum - %d\n"
	  "AStar time:  Average - %.4f ms / Maximum - %.4f ms",
	  stats.frameTimeQuota,
	  (uint32)processingContext.processingRequest.data.requestParams.agentTypeID,
	  processingContext.GetStatusAsString(),
	  stats.averageSearchSteps,
	  stats.peakSearchSteps,
	  stats.averageSearchTime,
	  stats.peakSearchTime);

	gEnv->pRenderer->Draw2dLabel(100.f, textY, 1.4f, Col_White, false, "%s", text.c_str());
}
