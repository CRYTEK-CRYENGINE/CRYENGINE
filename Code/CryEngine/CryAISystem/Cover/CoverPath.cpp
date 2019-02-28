// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CoverPath.h"

#include "DebugDrawContext.h"

CoverPath::CoverPath()
	: m_flags(0)
{
}

const CoverPath::Points& CoverPath::GetPoints() const
{
	return m_points;
}

bool CoverPath::Empty() const
{
	return m_points.size() < 2;
}

void CoverPath::Clear()
{
	m_flags = 0;
	m_points.clear();
}

void CoverPath::Reserve(uint32 pointCount)
{
	m_points.reserve(pointCount);
}

void CoverPath::AddPoint(const Vec3& point)
{
	if (!m_points.empty())
	{
		float distance = m_points.back().distance + (m_points.back().position - point).len();
		m_points.push_back(Point(point, distance));
	}
	else
		m_points.push_back(Point(point, 0.0f));
}

Vec3 CoverPath::GetPointAt(float distance) const
{
	uint32 pointCount = m_points.size();
	uint32 current = 0;

	if ((m_flags & Looped) && (distance > GetLength()))
		distance -= GetLength();

	while (current < pointCount - 1)
	{
		const Point& left = m_points[current];
		const Point& right = m_points[current + 1];

		if ((distance >= left.distance) && (distance < right.distance))
			break;

		++current;
	}

	if (current != pointCount)
	{
		const Point& left = m_points[current];
		const Point& right = m_points[current + 1];

		float frac = (distance - left.distance) / (right.distance - left.distance);

		return left.position + (right.position - left.position) * frac;
	}

	return Vec3(ZERO);
}

Vec3 CoverPath::GetClosestPoint(const Vec3& point, float* distanceToPath, float* distanceOnPath) const
{
	uint32 pointCount = m_points.size();

	float shortestSq = FLT_MAX;
	float shortestT = FLT_MAX;
	uint32 shortestLeft = -1;

	for (uint32 i = 0; i < pointCount - 1; ++i)
	{
		const Point& left = m_points[i];
		const Point& right = m_points[i + 1];

		Lineseg pathSegment(left.position, right.position);

		float t;
		float distToSegmentSq = Distance::Point_Lineseg2DSq(point, pathSegment, t);

		if (distToSegmentSq <= shortestSq)
		{
			shortestSq = distToSegmentSq;
			shortestT = t;
			shortestLeft = i;
		}
	}

	const Point& left = m_points[shortestLeft];
	const Point& right = m_points[shortestLeft + 1];

	if (distanceToPath)
		*distanceToPath = sqrt_tpl(shortestSq);

	if (distanceOnPath)
		*distanceOnPath = left.distance + shortestT * (right.distance - left.distance);

	return left.position + (right.position - left.position) * shortestT;
}

float CoverPath::GetDistanceAt(const Vec3& point, float tolerance) const
{
	uint32 pointCount = m_points.size();

	float lowestDistToSegmentSq = FLT_MAX;
	float best = -1.0f;

	for (uint32 i = 0; i < pointCount - 1; ++i)
	{
		const Point& left = m_points[i];
		const Point& right = m_points[i + 1];

		Lineseg pathSegment(left.position, right.position);

		float t;
		float distToSegmentSq = Distance::Point_Lineseg2DSq(point, pathSegment, t);

		if (distToSegmentSq <= sqr(tolerance))
		{
			if (distToSegmentSq < lowestDistToSegmentSq)
			{
				best = left.distance + t * (right.distance - left.distance);
				lowestDistToSegmentSq = distToSegmentSq;
			}
		}
	}

	return best;
}

Vec3 CoverPath::GetNormalAt(const Vec3& point) const
{
	uint32 pointCount = m_points.size();
	if (pointCount < 2)
		return ZERO;

	float lowestDistToSegmentSq = FLT_MAX;
	uint32 best = ~0u;

	for (uint32 i = 0; i < pointCount - 1; ++i)
	{
		const Point& left = m_points[i];
		const Point& right = m_points[i + 1];

		Lineseg pathSegment(left.position, right.position);

		float t;
		float distToSegmentSq = Distance::Point_Lineseg2DSq(point, pathSegment, t);

		if (distToSegmentSq < lowestDistToSegmentSq)
		{
			best = i;
			lowestDistToSegmentSq = distToSegmentSq;
		}
	}

	const Point& left = m_points[best];
	const Point& right = m_points[best + 1];

	return (left.position - right.position).Cross(CoverUp).normalized();
}

float CoverPath::GetLength() const
{
	return !m_points.empty() ? m_points.back().distance : 0.0f;
}

uint32 CoverPath::GetFlags() const
{
	return m_flags;
}

void CoverPath::SetFlags(uint32 flags)
{
	m_flags = flags;
}

bool CoverPath::Intersect(const Vec3& origin, const Vec3& dir, float* distance, Vec3* point) const
{
	uint32 pointCount = m_points.size();

	Lineseg ray(origin, origin + dir);

	int closestLeftIdx = -1;
	float closestA = FLT_MAX;
	float closestB = FLT_MAX;

	for (uint32 i = 0; i < pointCount - 1; ++i)
	{
		const Point& left = m_points[i];
		const Point& right = m_points[i + 1];

		Lineseg pathSegment(left.position, right.position);

		float a, b;

		Vec3 delta = ray.start - pathSegment.start;
		Vec3 segmentDir = pathSegment.end - pathSegment.start;

		float crossD = segmentDir.x * dir.y - segmentDir.y * dir.x;
		float crossDelta1 = delta.x * dir.y - delta.y * dir.x;
		float crossDelta2 = delta.x * segmentDir.y - delta.y * segmentDir.x;

		if (fabs_tpl(crossD) > 0.0001f)
		{
			a = crossDelta1 / crossD;
			b = crossDelta2 / crossD;

			if (a > 1.0f || a < 0.0f || b > 1.0f || b < 0.0f)
				continue;

			if (b < closestB)
			{
				closestA = a;
				closestB = b;
				closestLeftIdx = i;
			}
		}
	}

	if (closestLeftIdx != -1)
	{
		const Point& left = m_points[closestLeftIdx];
		const Point& right = m_points[closestLeftIdx + 1];

		if (distance)
			*distance = left.distance + closestA * (right.distance - left.distance);

		if (point)
			*point = left.position + closestA * (right.position - left.position);

		return true;
	}

	return false;
}

void CoverPath::DebugDraw()
{
	uint32 pointCount = m_points.size();

	if (pointCount < 2)
		return;

	CDebugDrawContext dc;
	dc->SetDepthWrite(false);

	Points::const_iterator rightIt = m_points.begin();
	Points::const_iterator leftIt = rightIt++;

	bool looped = (m_flags & Looped) != 0;

	const Vec3 floorOffset = (CoverUp * 0.005f);

	for (uint32 i = 0; i < pointCount - 1; ++i)
	{
		const Vec3& left = floorOffset + (*leftIt++).position;
		const Vec3& right = floorOffset + (*rightIt++).position;

		bool loopSegment = looped && (i == (pointCount - 2));

		dc->DrawLine(left, loopSegment ? Col_SlateBlue : Col_White, right, loopSegment ? Col_SlateBlue : Col_White, 10.0f);
	}

	Points::const_iterator pointIt = m_points.begin();
	for (uint32 i = 0; i < pointCount; ++i)
		dc->DrawSphere((CoverUp * 0.025f) + (*pointIt++).position, 0.035f, ColorB(Col_Black), true);
}
