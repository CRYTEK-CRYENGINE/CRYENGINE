// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ClusterDetector.h"

#define MIN_ACCEPTABLE_DISTANCE_DIFFERENCE_SQ sqr(0.1f)

/* **********************
   ClusterInternalState
 ** ********************** */

void ClusterDetector::CurrentRequestState::Reset()
{
	pCurrentRequest = NULL;
	stl::free_container(clusters);
	aabbCenter.zero();
	spawningPositionForNextCluster.zero();
}

/* *********************
    ClusterDetector
 ** ********************* */

ClusterDetector::ClusterDetector()
	: m_nextUniqueRequestID(0)
	, m_status(eStatus_Waiting)
{
}

IClusterDetector::IClusterRequestPair ClusterDetector::CreateNewRequest()
{
	ClusterRequestID uniqueId = GenerateUniqueClusterRequestId();
	m_requests.push_back(ClusterRequestPair(uniqueId, ClusterRequest()));
	return std::make_pair(uniqueId, &(m_requests.back().second));
}

void ClusterDetector::QueueRequest(const ClusterRequestID requestId)
{
	RequestsList::iterator it = m_requests.begin();
	while (it != m_requests.end())
	{
		if (it->first == requestId)
		{
			if (it->second.GetNumberOfPoint() != 0)
			{
				it->second.m_state = ClusterRequest::eRequestState_ReadyToBeProcessed;
			}
			else
			{
				// If an empty request is queued, let's remove it
				it = m_requests.erase(it);
			}

			return;
		}
		++it;
	}

}

void ClusterDetector::Reset()
{
	m_internalState.Reset();
	m_requests.clear();
	m_nextUniqueRequestID = 0;
	m_status = eStatus_Waiting;
}

ClusterDetector::ClusterRequestID ClusterDetector::GenerateUniqueClusterRequestId()
{
	return ++m_nextUniqueRequestID;
}

void ClusterDetector::Update(float frameDeltaTime)
{
	if (m_requests.empty())
		return;

	if (m_status == eStatus_Waiting)
	{
		bool hasRequestReadyToProcess = InitializeNextRequest();
		if (!hasRequestReadyToProcess)
			return;

		m_status = eStatus_ClusteringInProgress;
	}
	// At this point a current request should always be set
	assert(m_internalState.pCurrentRequest);

	if (m_status == eStatus_ClusteringInProgress)
	{
		/*
		   The clustering algorithm used is K-Mean.
		   The algorithm steps are the following:
		   1) K-cluster centers are placed in the world (we place them spread
		     over a circle shape in the center of the bounding box containing
		     the points we want to cluster)
		   2) Per each step:
		   a) We calculate the distance of each point from each center and we
		      assign a point to the closest cluster center
		   b) We calculate the new position of each cluster center as the middle
		      point of the points that belongs to that cluster
		   3) We iterate until no point is assigned to a different cluster.

		   As additional step for our needs we also try to calculate the optimal
		   clusters number. To do that we assign a maximum distance we allow for
		   a point to be considered part of the cluster.
		   If after each refinement we have point in a group that is at a further
		   distance than the maximum allowed we add a new cluster center and we
		   proceed to re-cluster the points.
		 */

		while (true)
		{
			bool isFurtherRefinementNeeded = false;
			while (true)
			{
				ResetClustersElements();
				isFurtherRefinementNeeded = ExecuteClusteringStep();
				UpdateClustersCenter();
				if (!isFurtherRefinementNeeded)
					break;
			}

			if (IsNewClusterNeeded())
				AddNewCluster();
			else
				break;
		}

		m_status = eStatus_ClusteringCompleted;
	}

	if (m_status == eStatus_ClusteringCompleted)
	{
		m_internalState.pCurrentRequest->m_totalClustersNumber = m_internalState.clusters.size();
		m_internalState.pCurrentRequest->m_callback(m_internalState.pCurrentRequest);

		m_internalState.Reset();
		m_requests.pop_front();

		m_status = eStatus_Waiting;
	}
}

bool ClusterDetector::IsNewClusterNeeded()
{
	bool isNewClusterNeeded = false;
	Cluster::DistancePointPair furtherPointFromAClusterCenter(FLT_MIN, Vec3(ZERO));
	ClusterDetector::ClustersArray::iterator it = m_internalState.clusters.begin();
	for (; it != m_internalState.clusters.end(); ++it)
	{
		if (it->pointAtMaxDistanceSqFromCenter.first > m_internalState.pCurrentRequest->m_maxDistanceSq &&
		    it->pointAtMaxDistanceSqFromCenter.first > furtherPointFromAClusterCenter.first)
		{
			furtherPointFromAClusterCenter = it->pointAtMaxDistanceSqFromCenter;
			isNewClusterNeeded = true;
		}
	}
	m_internalState.spawningPositionForNextCluster = furtherPointFromAClusterCenter.second;
	return isNewClusterNeeded;
}

bool ClusterDetector::ExecuteClusteringStep()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	bool isFurtherRefinementNeeded = false;
	ClusterRequest::PointsList& points = m_internalState.pCurrentRequest->m_points;
	ClusterRequest::PointsList::iterator pointIt = points.begin();
	for (; pointIt != points.end(); ++pointIt)
	{
		float minDistanceSq = FLT_MAX;
		size_t bestClusterId = ~0;
		ClusterDetector::ClustersArray::iterator clusterIt = m_internalState.clusters.begin();
		for (; clusterIt != m_internalState.clusters.end(); ++clusterIt)
		{
			const Vec3& clusterCenter = clusterIt->centerPos;
			float curDistanceSq = Distance::Point_PointSq(clusterCenter, pointIt->pos);

			if ((minDistanceSq - curDistanceSq) > MIN_ACCEPTABLE_DISTANCE_DIFFERENCE_SQ)
			{
				minDistanceSq = curDistanceSq;
				bestClusterId = clusterIt - m_internalState.clusters.begin();
			}
		}
		if (pointIt->clusterId != bestClusterId)
		{
			isFurtherRefinementNeeded = true;
			pointIt->clusterId = bestClusterId;
		}
		assert(pointIt->clusterId < m_internalState.clusters.size());
		Cluster& clusterForCurrentPoint = m_internalState.clusters[pointIt->clusterId];
		if ((minDistanceSq - clusterForCurrentPoint.pointAtMaxDistanceSqFromCenter.first) >= .0f)
		{
			clusterForCurrentPoint.pointAtMaxDistanceSqFromCenter = Cluster::DistancePointPair(minDistanceSq, pointIt->pos);
		}

		++(clusterForCurrentPoint.numberOfElements);
	}

	return isFurtherRefinementNeeded;
}

ClusterDetector::ClusterRequestPair* ClusterDetector::ChooseRequestToServe()
{
	RequestsList::iterator it = m_requests.begin();
	for (; it != m_requests.end(); ++it)
	{
		if (it->second.m_state == ClusterRequest::eRequestState_ReadyToBeProcessed)
		{
			return &(*it);
		}
	}
	return NULL;
}

size_t ClusterDetector::CalculateRuleOfThumbClusterSize(const size_t numberOfPoints)
{
	const size_t halfPoints = numberOfPoints >> 1;
	const float approxSqrtHalfPoints = sqrt_fast_tpl(static_cast<float>(halfPoints));
	return static_cast<size_t>(approxSqrtHalfPoints + .5f);
}

bool ClusterDetector::InitializeNextRequest()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	m_internalState.Reset();

	ClusterDetector::ClusterRequestPair* pCurrentRequestPair = ChooseRequestToServe();
	if (pCurrentRequestPair == NULL)
		return false;

	m_internalState.currentRequestId = pCurrentRequestPair->first;
	m_internalState.pCurrentRequest = &(pCurrentRequestPair->second);

	//Calculating the center of the aabb containing all the points of the request

	const ClusterRequest::PointsList& points = m_internalState.pCurrentRequest->m_points;
	const float positionsWeight = 1.0f / points.size();
	ClusterRequest::PointsList::const_iterator pointIt = points.begin();
	for (; pointIt != points.end(); ++pointIt)
	{
		m_internalState.aabbCenter += pointIt->pos;
	}
	m_internalState.aabbCenter *= positionsWeight;

	const size_t startupClusterNumber = std::max(CalculateRuleOfThumbClusterSize(m_internalState.pCurrentRequest->m_points.size()), size_t(1));
	m_internalState.clusters.reserve(2 * startupClusterNumber);
	m_internalState.clusters = ClustersArray(startupClusterNumber);

	InitializeClusters();
	return true;
}

void ClusterDetector::InitializeClusters()
{
	float spreadAngleUnit = (2.0f * gf_PI) / m_internalState.clusters.size();
	ClusterDetector::ClustersArray::iterator it = m_internalState.clusters.begin();
	for (; it != m_internalState.clusters.end(); ++it)
	{
		size_t clusterIndex = it - m_internalState.clusters.begin();
		const Vec3& pos = CalculateInitialClusterPosition(clusterIndex, spreadAngleUnit);
		it->centerPos = pos;
	}
}

Vec3 ClusterDetector::CalculateInitialClusterPosition(const size_t clusterIndex, const float spreadAngleUnit) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	Vec3 offset(2.0f, 0.0f, 0.0f);
	offset = offset.GetRotated(Vec3Constants<float>::fVec3_OneZ, spreadAngleUnit * clusterIndex);
	return m_internalState.aabbCenter + offset;
}

void ClusterDetector::UpdateClustersCenter()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	ClusterDetector::ClustersArray::iterator it = m_internalState.clusters.begin();
	for (; it != m_internalState.clusters.end(); ++it)
	{
		IF_UNLIKELY (it->numberOfElements == 0)
		{
			// This should not happen anymore but if it does, for specific
			// of layout the points to be clustered, it's still gracefully
			// handled and it won't cause any issue
			continue;
		}
		it->centerPos.zero();
		it->pointsWeight = 1.0f / it->numberOfElements;
	}

	const ClusterRequest::PointsList& points = m_internalState.pCurrentRequest->m_points;
	ClusterRequest::PointsList::const_iterator pointIt = points.begin();
	for (; pointIt != points.end(); ++pointIt)
	{
		Cluster& c = m_internalState.clusters[pointIt->clusterId];
		c.centerPos += pointIt->pos * c.pointsWeight;
	}
}

void ClusterDetector::ResetClustersElements()
{
	ClusterDetector::ClustersArray::iterator it = m_internalState.clusters.begin();
	for (; it != m_internalState.clusters.end(); ++it)
	{
		it->numberOfElements = 0;
		it->pointsWeight = 1.0f;
		it->pointAtMaxDistanceSqFromCenter = Cluster::DistancePointPair(FLT_MIN, Vec3(ZERO));
	}
}

bool ClusterDetector::AddNewCluster()
{
	m_internalState.clusters.push_back(Cluster());
	if (!m_internalState.spawningPositionForNextCluster.IsZero())
	{
		Cluster& c = m_internalState.clusters.back();
		c.centerPos = m_internalState.spawningPositionForNextCluster;
	}
	else
	{
		InitializeClusters();
	}
	return true;
}
