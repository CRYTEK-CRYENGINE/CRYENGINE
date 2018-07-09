// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ClusterDetector.h"

ClusterRequest::ClusterRequest()
	: m_state(eRequestState_Created)
	, m_maxDistanceSq(FLT_MAX)
	, m_totalClustersNumber(0)
{
	m_points.reserve(16);
}

void ClusterRequest::SetNewPointInRequest(const uint32 pointId, const Vec3& location)
{
	m_points.push_back(ClusterPoint(pointId, location));
}

size_t ClusterRequest::GetNumberOfPoint() const
{
	return m_points.size();
}

void ClusterRequest::SetCallback(Callback callback)
{
	m_callback = callback;
}

void ClusterRequest::SetMaximumSqDistanceAllowedPerCluster(float maxDistanceSq)
{
	m_maxDistanceSq = maxDistanceSq;
}

const ClusterPoint* ClusterRequest::GetPointAt(const size_t pointIndex) const
{
	return (pointIndex < m_points.size()) ? &(m_points[pointIndex]) : NULL;
}

size_t ClusterRequest::GetTotalClustersNumber() const
{
	return m_totalClustersNumber;
}
