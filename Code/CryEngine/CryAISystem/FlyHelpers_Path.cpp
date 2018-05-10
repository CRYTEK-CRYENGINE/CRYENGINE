// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlyHelpers_Path.h"

namespace FlyHelpers
{

Path::Path()
{
}

Path::~Path()
{
}

void Path::Clear()
{
	m_points.clear();
	m_segmentDistances.clear();
	m_pathLengthsToPoint.clear();
}

void Path::AddPoint(const Vec3& point)
{
	if (m_points.empty())
	{
		m_points.push_back(point);
		m_pathLengthsToPoint.push_back(0);
	}
	else
	{
		const Vec3& previousPoint = m_points.back();
		Vec3 segment = point - previousPoint;
		const float segmentLength = segment.GetLength();
		if (0.01f < segmentLength)
		{
			m_points.push_back(point);
			m_segmentDistances.push_back(segmentLength);

			const float pathLengthToPreviousPoint = m_pathLengthsToPoint.back();
			const float pathLengthToPoint = pathLengthToPreviousPoint + segmentLength;
			m_pathLengthsToPoint.push_back(pathLengthToPoint);
		}
	}
}

const Vec3& Path::GetPoint(const size_t pointIndex) const
{
	assert(pointIndex < m_points.size());
	return m_points[pointIndex];
}

Lineseg Path::GetSegment(const size_t segmentIndex) const
{
	assert(segmentIndex + 1 < m_points.size());
	const Vec3& start = m_points[segmentIndex];
	const Vec3& end = m_points[segmentIndex + 1];
	return Lineseg(start, end);
}

float Path::GetSegmentLength(const size_t segmentIndex) const
{
	assert(segmentIndex < m_segmentDistances.size());
	return m_segmentDistances[segmentIndex];
}

size_t Path::GetPointCount() const
{
	return m_points.size();
}

size_t Path::GetSegmentCount() const
{
	return m_segmentDistances.size();
}

void Path::MakeLooping()
{
	if (m_points.empty())
	{
		return;
	}

	const Vec3 firstPoint = m_points.front();
	AddPoint(firstPoint);
}

float Path::GetPathDistanceToPoint(const size_t pointIndex) const
{
	assert(pointIndex < m_pathLengthsToPoint.size());
	return m_pathLengthsToPoint[pointIndex];
}

float Path::GetTotalPathDistance() const
{
	assert(!m_pathLengthsToPoint.empty());
	return m_pathLengthsToPoint.back();
}

}
