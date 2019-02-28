// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   MNMPathfinder.h
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   - 06.05.2011 : Created by Francesco Roccucci

 *********************************************************************/

#ifndef _MNMPATHFINDER_H_
#define _MNMPATHFINDER_H_

#include "Navigation/MNM/MNM.h"
#include "Navigation/MNM/NavMesh.h"
#include "NavPath.h"
#include "Navigation/PathHolder.h"
#include <CryPhysics/AgePriorityQueue.h>
#include <CryAISystem/INavigationSystem.h>
#include <CryAISystem/IPathfinder.h>
#include <CryAISystem/NavigationSystem/INavigationQuery.h>

//////////////////////////////////////////////////////////////////////////

namespace MNM
{
namespace PathfinderUtils
{
struct QueuedRequest
{
	QueuedRequest()
		: requesterEntityId(INVALID_ENTITYID)
		, agentTypeID(0)
		, pFilter(nullptr)
	{
		//don't need to setup dangerous areas in default constructor
	}

	~QueuedRequest()
	{
		pFilter = nullptr;
	}

	QueuedRequest(EntityId _requesterEntityId, NavigationAgentTypeID _agentTypeID, const MNMPathRequest& _request)
		: requesterEntityId(_requesterEntityId)
		, agentTypeID(_agentTypeID)
		, requestParams(_request)
	{
		// Make a copy of the navigation query filter
		if (_request.pFilter)
		{
			requestParams.pFilter = _request.pFilter->Clone();
			pFilter = INavMeshQueryFilterConstPtr(requestParams.pFilter, [](const INavMeshQueryFilter* pFilter)
			{
				pFilter->Release();
			});
		}
		
		SetupDangerousLocationsData();
	}

	EntityId                 requesterEntityId;
	NavigationAgentTypeID    agentTypeID;
	MNMPathRequest           requestParams;
	INavMeshQueryFilterConstPtr pFilter;

	const MNM::DangerousAreasList& GetDangersInfos() { return dangerousAreas; }

private:
	void SetupDangerousLocationsData();
	// Setup different danger types
	void SetupAttentionTargetDanger();
	void SetupExplosiveDangers();
	void SetupGroupMatesAvoidance();

	MNM::DangerousAreasList dangerousAreas;
};

struct ProcessingRequest
{
	ProcessingRequest()
		: meshID(0)
		, fromTriangleID(0)
		, toTriangleID(0)
		, data()
		, queuedID(0)
	{

	}

	ILINE void Reset()         { (*this) = ProcessingRequest(); }
	ILINE bool IsValid() const { return (queuedID != 0); };

	NavigationMeshID    meshID;
	MNM::TriangleID     fromTriangleID;
	MNM::TriangleID     toTriangleID;

	QueuedRequest       data;
	MNM::QueuedPathID   queuedID;
};

typedef size_t ProcessingContextId;
const static ProcessingContextId kInvalidProcessingContextId = ~0ul;

struct ProcessingContext
{
	enum EProcessingStatus
	{
		Invalid,
		Reserved,
		InProgress,
		FindWayCompleted,
		Completed,
	};

	ProcessingContext(const size_t maxWaySize)
		: queryResult(maxWaySize)
		, status(Invalid)
	{
	}
	
	ProcessingContext(ProcessingContext&&) = default;
	ProcessingContext(const ProcessingContext&) = delete;

	void Reset()
	{
		processingRequest.Reset();
		queryResult.Reset();
		workingSet.Reset();

		status = Invalid;
		jobState = JobManager::SJobState();
	}

	const char* GetStatusAsString() const
	{
		switch (status)
		{
		case Invalid:
			return "Invalid";
		case Reserved:
			return "Reserved";
		case InProgress:
			return "InProgress";
		case FindWayCompleted:
			return "FindWayCompleted";
		case Completed:
			return "Completed";
		default:
			assert(0);
			return "Error. Unknown status";
		}
	}

	ProcessingContext& operator=(ProcessingContext&&) = default;
	ProcessingContext& operator=(const ProcessingContext&) = delete;

	ProcessingRequest            processingRequest;
	CNavMesh::WayQueryResult     queryResult;
	CNavMesh::WayQueryWorkingSet workingSet;

	volatile EProcessingStatus   status;
	JobManager::SJobState        jobState;
};

struct IsProcessingRequestRelatedToQueuedPathId
{
	IsProcessingRequestRelatedToQueuedPathId(MNM::QueuedPathID _idToCheck)
		: idToCheck(_idToCheck)
	{}

	bool operator()(const ProcessingContext& requestToEvaluate)
	{
		return requestToEvaluate.processingRequest.queuedID == idToCheck;
	}

private:
	MNM::QueuedPathID idToCheck;
};

class ProcessingContextsPool
{
public:
	typedef std::vector<ProcessingContext> Pool;
	typedef Pool::iterator                 PoolIterator;
	typedef Pool::const_iterator           PoolConstIterator;
	typedef Functor1<ProcessingContext&>   ProcessingOperation;

	ProcessingContextsPool(const size_t maxAmountOfTrianglesToCalculateWay = 512)
		: m_maxAmountOfTrianglesToCalculateWay(maxAmountOfTrianglesToCalculateWay)
	{
		Reset();
	}

	struct IsProcessCompleted
	{
		bool operator()(const ProcessingContext& context) { return context.status == ProcessingContext::Completed; }
	};

	struct IsProcessInvalid
	{
		bool operator()(const ProcessingContext& context) { return context.status == ProcessingContext::Invalid; }
	};

	size_t              GetFreeSlotsCount() const     { return std::count_if(m_pool.begin(), m_pool.end(), IsProcessInvalid()); }
	size_t              GetOccupiedSlotsCount() const { return std::count_if(m_pool.begin(), m_pool.end(), IsProcessCompleted()); }
	size_t              GetMaxSlots() const           { return m_pool.size(); }

	ProcessingContextId GetFirstAvailableContextId()
	{
		PoolIterator freeElement = std::find_if(m_pool.begin(), m_pool.end(), IsProcessInvalid());
		if (freeElement != m_pool.end())
		{
			const ProcessingContextId newIndex = freeElement - m_pool.begin();
			freeElement->status = ProcessingContext::Reserved;
			return newIndex;
		}

		return kInvalidProcessingContextId;
	}

	ProcessingContext& GetContextAtPosition(ProcessingContextId processingContextId)
	{
		CRY_ASSERT_MESSAGE(processingContextId >= 0 && processingContextId < m_pool.size(), "ProcessingContextsPool::GetContextAtPosition Trying to access an invalid element of the pool.");
		return m_pool[processingContextId];
	}

	void ReleaseContext(ProcessingContextId processingContextIdToRemove)
	{
		if (processingContextIdToRemove != kInvalidProcessingContextId && processingContextIdToRemove < m_pool.size())
		{
			m_pool[processingContextIdToRemove].status = ProcessingContext::Invalid;
		}
	}

	void ExecuteFunctionOnElements(const ProcessingOperation& operation)
	{
		std::for_each(m_pool.begin(), m_pool.end(), operation);
	}

	struct InvalidateCompletedProcess
	{
		void operator()(ProcessingContext& context) { if (context.status == ProcessingContext::Completed) context.Reset(); }
	};

	void CleanupFinishedRequests()
	{
		std::for_each(m_pool.begin(), m_pool.end(), InvalidateCompletedProcess());
	}

	struct IsProcessRelatedToPathId
	{
		IsProcessRelatedToPathId(const MNM::QueuedPathID _requestIdToSearch)
			: requestIdToSearch(_requestIdToSearch)
		{}

		bool operator()(const ProcessingContext& context) { return context.processingRequest.queuedID == requestIdToSearch; }
	private:
		MNM::QueuedPathID requestIdToSearch;
	};

	void CancelPathRequest(MNM::QueuedPathID requestId)
	{
		PoolIterator end = m_pool.end();
		PoolIterator element = std::find_if(m_pool.begin(), end, IsProcessRelatedToPathId(requestId));
		if (element != end)
		{
			gEnv->GetJobManager()->WaitForJob(element->jobState);
			element->status = ProcessingContext::Invalid;
		}
	}

	void Reset()
	{
		const int poolSize = gAIEnv.CVars.MNMPathfinderConcurrentRequests;

		m_pool.clear();
		m_pool.reserve(poolSize);

		for(int i = 0; i < poolSize; ++i)
		{
			m_pool.emplace_back(m_maxAmountOfTrianglesToCalculateWay);
		}
	}

private:
	Pool   m_pool;
	size_t m_maxAmountOfTrianglesToCalculateWay;
};

struct PathfindingFailedEvent
{
	PathfindingFailedEvent()
		: requestId(MNM::Constants::eQueuedPathID_InvalidID)
		, request()
	{
	}

	PathfindingFailedEvent(const MNM::QueuedPathID& _requestId, const MNM::PathfinderUtils::QueuedRequest& _request)
		: requestId(_requestId)
		, request(_request)
	{}

	MNM::QueuedPathID                   requestId;
	MNM::PathfinderUtils::QueuedRequest request;
};

struct IsPathfindingFailedEventRelatedToRequest
{
	IsPathfindingFailedEventRelatedToRequest(const MNM::QueuedPathID requestId) : requestIdToCheck(requestId) {}

	const bool operator()(PathfindingFailedEvent& event) { return event.requestId == requestIdToCheck; }

	MNM::QueuedPathID requestIdToCheck;
};

struct PathfindingCompletedEvent
{
	PathfindingCompletedEvent()
	{
		eventData.pPath.reset(new CNavPath());
	}

	CNavPath                 eventNavPath;
	MNMPathRequestResult     eventResults;
	MNMPathRequestResult     eventData;
	MNMPathRequest::Callback callback;
	MNM::QueuedPathID        requestId;
};

struct IsPathfindingCompletedEventRelatedToRequest
{
	IsPathfindingCompletedEventRelatedToRequest(const MNM::QueuedPathID requestId) : requestIdToCheck(requestId) {}

	const bool operator()(PathfindingCompletedEvent& event) { return event.requestId == requestIdToCheck; }

	MNM::QueuedPathID requestIdToCheck;
};

typedef CryMT::vector<PathfindingFailedEvent>    PathfinderFailedEventQueue;
typedef CryMT::vector<PathfindingCompletedEvent> PathfinderCompletedEventQueue;
}
}

// -----------------------------------------------------------------------------

class CMNMPathfinder : public IMNMPathfinder
{
public:

	CMNMPathfinder();
	~CMNMPathfinder();

	void                      Reset();

	virtual MNM::QueuedPathID RequestPathTo(const EntityId requesterEntityId, const MNMPathRequest& request) override;
	virtual void              CancelPathRequest(MNM::QueuedPathID requestId) override;

	void                      WaitForJobsToFinish();

	bool                      CheckIfPointsAreOnStraightWalkableLine(const NavigationMeshID& meshID, const Vec3& source, const Vec3& destination, float heightOffset = 0.2f) const override;
	bool                      CheckIfPointsAreOnStraightWalkableLine(const NavigationMeshID& meshID, const Vec3& source, const Vec3& destination, const INavMeshQueryFilter* pFilter, float heightOffset = 0.2f) const override;

	void                      Update();

	void                      SetupNewValidPathRequests();
	void                      DispatchResults();

	size_t                    GetRequestQueueSize() const { return m_requestedPathsQueue.size(); }

	void                      OnNavigationMeshChanged(NavigationMeshID meshId, MNM::TileID tileId);

	// Utility function, which takes a triangles way found by MNM::CNavMesh::FindWay() and converts it into a way-point path.
	static bool ConstructPathFromFoundWay(
	  const MNM::CNavMesh::WayQueryResult& way,
	  const MNM::CNavMesh& navMesh,
	  const OffMeshNavigationManager* pOffMeshNavigationManager,
	  const Vec3& startLocation,
	  const Vec3& endLocation,
	  CPathHolder<PathPointDescriptor>& outputPath);

private:

	void              SpawnJobs();
	void              SpawnAppropriateJobIfPossible(MNM::PathfinderUtils::ProcessingContext& processingContext);
	void              ExecuteAppropriateOperationIfPossible(MNM::PathfinderUtils::ProcessingContext& processingContext);
	void              SpawnPathfinderProcessingJob(MNM::PathfinderUtils::ProcessingContext& processingContext);
	void              SpawnPathConstructionJob(MNM::PathfinderUtils::ProcessingContext& processingContext);
	void              WaitForJobToFinish(MNM::PathfinderUtils::ProcessingContext& processingContext);
	void              ProcessPathRequest(MNM::PathfinderUtils::ProcessingContext& processingContext);
	void              ConstructPathIfWayWasFound(MNM::PathfinderUtils::ProcessingContext& processingContext);

	bool              SetupForNextPathRequest(MNM::QueuedPathID requestID, const MNM::PathfinderUtils::QueuedRequest& request, MNM::PathfinderUtils::ProcessingContext& processingContext);
	void              PathRequestFailed(MNM::QueuedPathID requestID, const MNM::PathfinderUtils::QueuedRequest& request);

	void              CancelResultDispatchingForRequest(MNM::QueuedPathID requestId);

	void              DebugAllStatistics();
	void              DebugStatistics(MNM::PathfinderUtils::ProcessingContext& processingContext, const float textY);

	friend void       ProcessPathRequestJob(MNM::PathfinderUtils::ProcessingContext* pProcessingContext);
	friend void       ConstructPathIfWayWasFoundJob(MNM::PathfinderUtils::ProcessingContext* pProcessingContext);

	typedef AgePriorityQueue<MNM::PathfinderUtils::QueuedRequest> RequestQueue;
	RequestQueue m_requestedPathsQueue;

	MNM::PathfinderUtils::ProcessingContextsPool        m_processingContextsPool;
	MNM::PathfinderUtils::PathfinderFailedEventQueue    m_pathfindingFailedEventsToDispatch;
	MNM::PathfinderUtils::PathfinderCompletedEventQueue m_pathfindingCompletedEventsToDispatch;
};

#endif // _MNMPATHFINDER_H_
