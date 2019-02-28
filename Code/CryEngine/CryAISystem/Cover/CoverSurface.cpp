// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CoverSurface.h"

#include "DebugDrawContext.h"

CoverSurface::CoverSurface()
	: m_flags(0)
	, m_aabb(AABB::RESET)
{
}

CoverSurface::CoverSurface(const ICoverSystem::SurfaceInfo& surfaceInfo)
{
	m_flags = surfaceInfo.flags;
	m_samples.assign(surfaceInfo.samples, surfaceInfo.samples + surfaceInfo.sampleCount);

	Generate();
}

void CoverSurface::Swap(CoverSurface& other)
{
	m_samples.swap(other.m_samples);
	m_segments.swap(other.m_segments);
	m_locations.swap(other.m_locations);

	std::swap(m_aabb, other.m_aabb);
	std::swap(m_flags, other.m_flags);
}

bool CoverSurface::IsValid() const
{
	return m_samples.size() > 1;
}

void CoverSurface::Clear()
{
	m_samples.clear();
	m_segments.clear();
	m_aabb.Reset();
}

uint32 CoverSurface::GetSampleCount() const
{
	return m_samples.size();
}

bool CoverSurface::GetSurfaceInfo(ICoverSystem::SurfaceInfo* surfaceInfo) const
{
	if (m_samples.empty())
		return false;

	if (surfaceInfo)
	{
		surfaceInfo->sampleCount = m_samples.size();
		if (surfaceInfo->sampleCount)
			surfaceInfo->samples = &m_samples.front();
		surfaceInfo->flags = m_flags;
	}

	return true;
}

uint32 CoverSurface::GetSegmentCount() const
{
	return m_segments.size();
}

CoverSurface::Segment& CoverSurface::GetSegment(uint16 segmentIdx)
{
	return m_segments[segmentIdx];
}

const CoverSurface::Segment& CoverSurface::GetSegment(uint16 segmentIdx) const
{
	return m_segments[segmentIdx];
}

const AABB& CoverSurface::GetAABB() const
{
	return m_aabb;
}

const uint32 CoverSurface::GetFlags() const
{
	return m_flags;
}

void CoverSurface::ReserveSamples(uint32 sampleCount)
{
	m_samples.reserve(sampleCount);
}

void CoverSurface::AddSample(const Sample& sample)
{
	m_samples.push_back(sample);
}

void CoverSurface::Generate()
{
	uint32 sampleCount = m_samples.size();

	if (sampleCount < 2)
		return;

	m_segments.clear();
	m_segments.reserve(sampleCount - 1);

	m_aabb = AABB(AABB::RESET);

	for (uint32 i = 0; i < sampleCount - 1; ++i)
	{
		const Sample& left = m_samples[i];
		const Sample& right = m_samples[i + 1];

		Vec3 rightToLeft = left.position - right.position;

		Segment segment(CoverUp.Cross(rightToLeft).normalized(), rightToLeft.GetLength(), i, i + 1, 0);

		m_aabb.Add(left.position);
		m_aabb.Add(left.position + Vec3(0.0f, 0.0f, left.GetHeight()));

		m_aabb.Add(right.position);
		m_aabb.Add(right.position + Vec3(0.0f, 0.0f, right.GetHeight()));

		if ((left.flags & ICoverSampler::Sample::Dynamic) && (right.flags & ICoverSampler::Sample::Dynamic))
		{
			m_flags |= ICoverSystem::SurfaceInfo::Dynamic;

			segment.flags |= Segment::Dynamic;
		}

		m_segments.push_back(segment);
	}

	if (m_flags & ICoverSystem::SurfaceInfo::Looped)
	{
		const Sample& left = m_samples.back();
		const Sample& right = m_samples.front();

		Vec3 rightToLeft = left.position - right.position;

		Segment segment(CoverUp.Cross(rightToLeft).normalized(), rightToLeft.GetLength(), static_cast<uint16>(m_samples.size() - 1), 0, 0);

		if ((left.flags & ICoverSampler::Sample::Dynamic) && (right.flags & ICoverSampler::Sample::Dynamic))
		{
			m_flags |= ICoverSystem::SurfaceInfo::Dynamic;

			segment.flags |= Segment::Dynamic;
		}

		m_segments.push_back(segment);
	}

	GenerateLocations();
}

// this can later move into the file
// once all the calculation are more stable
void CoverSurface::GenerateLocations()
{
	const float MinLocationSeparation = 0.5f;
	const float IntermediateLocationSeparation = 1.5f;
	const float RightSnapThreshold = 0.125f;

	bool looped = (m_flags& ICoverSystem::SurfaceInfo::Looped) != 0;

	uint32 segmentCount = m_segments.size();
	float spanRight = 0.0f;
	float spanLeft = 0.0f;
	float lastLocation = 0.0f;

	for (uint32 i = 0; i < segmentCount; ++i)
	{
		uint32 segmentIdx = i;
		Segment& segment = m_segments[segmentIdx];
		Sample& left = m_samples[segment.leftIdx];
		Sample& right = m_samples[segment.rightIdx];

		float segmentStart = spanRight;
		spanRight += segment.length;

		uint32 segmentLocationCount = 0;

		if (m_locations.empty())
		{
			m_locations.push_back(Location(segmentIdx, 0.0f, left.GetHeight()));
			m_locations.back().flags |= Location::LeftEdge;

			if (spanRight < MinLocationSeparation)
				continue;

			++segmentLocationCount;
		}

		float invSegLength = 1.0f / segment.length;

		while ((spanRight - spanLeft) >= MinLocationSeparation)
		{
			if ((segmentLocationCount < 3) ||
			    ((spanLeft - lastLocation) >= IntermediateLocationSeparation) ||
			    ((spanRight - spanLeft) < ((3.0f * MinLocationSeparation) + 0.015f)))
			{
				float offset = ((spanLeft + MinLocationSeparation) - segmentStart) * invSegLength;

				assert((offset >= 0.0f) && (offset <= 1.0f));

				float leftHeight = left.GetHeight();
				float rightHeight = right.GetHeight();

				float height = leftHeight + offset * (rightHeight - leftHeight);

				m_locations.push_back(Location(segmentIdx, offset, height));
				lastLocation = spanLeft + MinLocationSeparation;
			}

			spanLeft += MinLocationSeparation;

			++segmentLocationCount;
		}

		bool isLast = (i == (segmentCount - 1)) && !looped;
		if (isLast)
		{
			Location& back = m_locations.back();

			Location* last = 0;

			float distanceSq = (GetLocation(static_cast<uint16>(m_locations.size() - 1)) - right.position).len2();
			if (distanceSq < sqr(RightSnapThreshold))
				last = &back;
			else
			{
				m_locations.push_back(Location());
				last = &m_locations.back();
			}

			last->segmentIdx = segmentIdx;
			last->SetOffset(1.0f);
			last->SetHeight(right.GetHeight());

			if (isLast)
				last->flags |= Location::RightEdge;
		}
	}
}

uint32 CoverSurface::GetLocationCount() const
{
	return m_locations.size();
}

Vec3 CoverSurface::GetLocation(uint16 location, float offset, float* height, Vec3* normal) const
{
	assert(location < m_locations.size());

	if (location < m_locations.size())
	{
		const Location& loc = m_locations[location];
		const Segment& segment = m_segments[loc.segmentIdx];
		const Vec3& left = m_samples[segment.leftIdx].position;
		const Vec3& right = m_samples[segment.rightIdx].position;

		if (height)
			*height = loc.GetHeight();

		if (normal)
			*normal = segment.normal;

		const float tmpOffset = static_cast<float>(__fsel(offset - 0.001f, offset, 0.0f));
		return (left + loc.GetOffset() * (right - left)) + segment.normal * tmpOffset;
	}

	return Vec3Constants<float>::fVec3_Zero;
}

bool CoverSurface::IsPointInCover(const Vec3& eye, const Vec3& point) const
{
	uint32 segmentCount = m_segments.size();

	if (segmentCount < 1)
		return false;

	Vec3 observer(eye);

	for (uint32 i = 0; i < segmentCount; ++i)
	{
		const Segment& segment = m_segments[i];

		if (((segment.flags & Segment::Disabled) == 0) && IsPointBehindSegment(observer, point, segment))
			return true;
	}

	return false;
}

bool CoverSurface::IsCircleInCover(const Vec3& eye, const Vec3& center, float radius) const
{
	uint32 segmentCount = m_segments.size();

	if (segmentCount < 1)
		return false;

	Vec3 observer(eye);

	enum
	{
		Left = 0,
		Center,
		Right,
	};

	uint8 result = 0;

	// loop center only first for early out
	for (uint32 i = 0; i < segmentCount; ++i)
	{
		const Segment& segment = m_segments[i];

		if (((segment.flags & Segment::Disabled) != 0) || !IsPointBehindSegment(observer, center, segment))
			continue;

		result |= 1 << Center;
		break;
	}

	if ((result & (1 << Center)) == 0)
		return false;

	Vec3 dirRight = (observer - center).Cross(CoverUp).normalized();
	Vec3 left = center - (dirRight * radius);

	for (uint32 i = 0; i < segmentCount; ++i)
	{
		const Segment& segment = m_segments[i];

		if (((segment.flags & Segment::Disabled) != 0) || !IsPointBehindSegment(observer, left, segment))
			continue;

		result |= 1 << Left;
		break;
	}

	if ((result & (1 << Left)) == 0)
		return false;

	Vec3 right = center + (dirRight * radius);

	for (uint32 i = 0; i < segmentCount; ++i)
	{
		const Segment& segment = m_segments[i];

		if (((segment.flags & Segment::Disabled) != 0) || !IsPointBehindSegment(observer, right, segment))
			continue;

		result |= 1 << Right;
		break;
	}

	return ((result & (1 << Right)) != 0);
}

bool CoverSurface::GetCoverOcclusionAt(const Vec3& eye, const Vec3& point, float* heightSq) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	uint32 segmentCount = m_segments.size();

	if (segmentCount < 1)
		return false;

	Vec3 observer(eye);

	for (uint32 i = 0; i < segmentCount; ++i)
	{
		const Segment& segment = m_segments[i];

		if (((segment.flags & Segment::Disabled) == 0) && GetOcclusionBehindSegment(observer, point, segment, heightSq))
			return true;
	}

	return false;
}

bool CoverSurface::GetCoverOcclusionAt(const Vec3& eye, const Vec3& center, float radius, float* heightSq) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	uint32 segmentCount = m_segments.size();

	if (segmentCount < 1)
		return false;

	Vec3 observer(eye);

	enum
	{
		Left = 0,
		Center,
		Right,
	};

	float heightSqArr[3];
	uint8 result = 0;

	// If we store the index we continue to the last one
	// evaluated instead of re-iterating through segments
	// that were previously excluded
	uint32 segmentIndex = 0;

	Vec3 dirRight = (observer - center).Cross(CoverUp).normalized();
	Vec3 left = center - (dirRight * radius);

	for (; segmentIndex < segmentCount; ++segmentIndex)
	{
		const Segment& segment = m_segments[segmentIndex];

		if (((segment.flags & Segment::Disabled) != 0) || !GetOcclusionBehindSegment(observer, left, segment, &heightSqArr[Left]))
			continue;

		result |= 1 << Left;
		break;
	}

	if ((result & (1 << Left)) == 0)
		return false;

	for (; segmentIndex < segmentCount; ++segmentIndex)
	{
		const Segment& segment = m_segments[segmentIndex];

		if (((segment.flags & Segment::Disabled) != 0) || !GetOcclusionBehindSegment(observer, center, segment, &heightSqArr[Center]))
			continue;

		result |= 1 << Center;
		break;
	}

	if ((result & (1 << Center)) == 0)
		return false;

	Vec3 right = center + (dirRight * radius);

	for (; segmentIndex < segmentCount; ++segmentIndex)
	{
		const Segment& segment = m_segments[segmentIndex];

		if (((segment.flags & Segment::Disabled) != 0) || !GetOcclusionBehindSegment(observer, right, segment, &heightSqArr[Right]))
			continue;

		result |= 1 << Right;
		break;
	}

	if ((result & (1 << Right)) != 0)
	{
		uint32 lowest = (heightSqArr[Left] < heightSqArr[Right]) ?
		                ((heightSqArr[Left] < heightSqArr[Center]) ? Left : Center) :
		                ((heightSqArr[Right] < heightSqArr[Center]) ? Right : Center);

		*heightSq = heightSqArr[lowest];

		return true;
	}

	return false;
}

bool CoverSurface::GenerateCoverPath(float distanceToCover, CoverPath* path, bool skipSimplify) const
{
	uint32 segmentCount = m_segments.size();

	if (segmentCount < 1)
		return false;

	if (segmentCount == 1)
	{
		const Segment& segment = m_segments[0];

		Vec3 extrusion = segment.normal * distanceToCover;

		Vec3 left = m_samples[segment.leftIdx].position + extrusion;
		Vec3 right = m_samples[segment.rightIdx].position + extrusion;

		path->AddPoint(left);
		path->AddPoint(right);

		return true;
	}

	Points points;

	const float ExtraPointMinAngleCos = cos_tpl(DEG2RAD(20.0f));

	const Segment& firstSegment = m_segments[0];
	Vec3 extrusionLeft = firstSegment.normal * distanceToCover;
	points.push_back(m_samples[firstSegment.leftIdx].position + extrusionLeft);

	for (uint32 i = 1; i < segmentCount; ++i)
	{
		const Segment& prevSegment = m_segments[i - 1];
		const Segment& segment = m_segments[i];

		const float crossProduct = prevSegment.normal.Cross(segment.normal).z;
		if (fabsf(crossProduct) > FLT_EPSILON)
		{
			Vec3 prevExtrusion = prevSegment.normal * distanceToCover;
			points.push_back(m_samples[prevSegment.rightIdx].position + prevExtrusion);

			Vec3 currentExtrusion = segment.normal * distanceToCover;

			float angleCos = prevSegment.normal.Dot(segment.normal);
			if ((angleCos <= ExtraPointMinAngleCos) && ((prevExtrusion - currentExtrusion).len2() > sqr(0.35f)))
			{
				Vec3 normalAvg = ((prevSegment.normal + segment.normal) * 0.5f).normalized();

				float thetaCos = prevSegment.normal.Dot(normalAvg);
				if (thetaCos < 0.01f)
					thetaCos = 0.5f;

				float extrusionOffset = distanceToCover / thetaCos;

				points.push_back(m_samples[segment.leftIdx].position + (normalAvg * extrusionOffset));
			}

			if (crossProduct > 0.0f)
			{
				points.push_back(m_samples[segment.leftIdx].position + currentExtrusion);
			}
		}
	}

	const Segment& lastSegment = m_segments[segmentCount - 1];
	Vec3 extrusionRight = lastSegment.normal * distanceToCover;
	points.push_back(m_samples[lastSegment.rightIdx].position + extrusionRight);

	if (!skipSimplify)
		SimplifyCoverPath(points);

	Points::const_iterator it = points.begin();
	Points::const_iterator itEnd = points.end();

	bool looped = m_flags & ICoverSystem::SurfaceInfo::Looped;
	path->Reserve(looped ? (points.size() + 1) : points.size());

	for (; it != itEnd; ++it)
		path->AddPoint(*it);

	if (looped)
	{
		path->AddPoint(points.front());
		path->SetFlags(path->GetFlags() | CoverPath::Looped);
	}

	return true;
}

CoverSurface::Points CoverSurface::s_simplifiedPoints;

void CoverSurface::SimplifyCoverPath(Points& points) const
{
	uint32 pointCount = points.size();
	if (pointCount < 3)
		return;

	s_simplifiedPoints.reserve(pointCount);
	s_simplifiedPoints.clear();

	Points::iterator rightIt = points.begin();
	Points::iterator leftIt = rightIt++;
	Points::iterator midIt = rightIt++;

	s_simplifiedPoints.push_back(*leftIt);

	for (; rightIt != points.end(); )
	{
		const Vec3& left = *leftIt;
		const Vec3& mid = *midIt;
		const Vec3& right = *rightIt;

		float t;
		Lineseg line(left, right);

		if (Distance::Point_LinesegSq(mid, line, t) < sqr(0.015f))
		{
			midIt = rightIt++;
			continue;
		}

		s_simplifiedPoints.push_back(*midIt);

		leftIt = midIt;
		midIt = rightIt++;
	}

	s_simplifiedPoints.push_back(*midIt);

	s_simplifiedPoints.swap(points);
}

bool CoverSurface::CalculatePathCoverage(const Vec3& eye, const CoverPath& coverPath,
                                         CoverInterval* interval) const
{
	const CoverPath::Points& pathPoints = coverPath.GetPoints();

	const uint32 pathPointCount = pathPoints.size();

	if (pathPointCount < 2)
		return false;

	Plane leftPlane;
	Plane rightPlane;

	Vec3 eyeLoc = eye;
	eyeLoc.z = 0.0f;

	FindCoverPlanes(eyeLoc, leftPlane, rightPlane);

	if (gAIEnv.CVars.DebugDrawCoverPlanes != 0)
	{
		CDebugDrawContext dc;

		Vec3 leftFwd = leftPlane.n.Cross(Vec3Constants<float>::fVec3_OneZ).normalized();
		Vec3 rightFwd = rightPlane.n.Cross(Vec3Constants<float>::fVec3_OneZ).normalized();

		dc->DrawLine(eye - leftFwd * 50.0f, Col_MediumSeaGreen, eye + leftFwd * 50.0f, Col_MediumSeaGreen, 10.0f);
		dc->DrawLine(eye - leftFwd * 2.0f, Col_MediumSeaGreen, eye - leftFwd * 2.0f + leftPlane.n * 0.5f, Col_MediumSeaGreen, 5.0f);
		dc->DrawLine(eye + leftFwd * 2.0f, Col_MediumSeaGreen, eye + leftFwd * 2.0f + leftPlane.n * 0.5f, Col_MediumSeaGreen, 5.0f);
		dc->DrawLine(eye - rightFwd * 50.0f, Col_Orange, eye + rightFwd * 50.0f, Col_Orange, 10.0f);
		dc->DrawLine(eye - rightFwd * 2.0f, Col_Orange, eye - rightFwd * 2.0f + rightPlane.n * 0.5f, Col_Orange, 5.0f);
		dc->DrawLine(eye + rightFwd * 2.0f, Col_Orange, eye + rightFwd * 2.0f + rightPlane.n * 0.5f, Col_Orange, 5.0f);
	}

	float leftCoverage = -1.0f;
	float leftClosestSq = FLT_MAX;

	float rightCoverage = -1.0f;
	float rightClosestSq = FLT_MAX;

	for (uint32 i = 0; i < (pathPointCount - 1); ++i)
	{
		const CoverPath::Point& leftPoint = pathPoints[i];
		const CoverPath::Point& rightPoint = pathPoints[i + 1];

		float t;
		Vec3 point;

		Vec3 dir = leftPoint.position - rightPoint.position;
		dir.z = 0.0f;

		Vec3 left = leftPoint.position;
		Vec3 right = rightPoint.position;

		if (IntersectCoverPlane(right, dir, leftPlane, &point, &t))
		{
			float distSq = (eyeLoc - point).len2();
			if (distSq < leftClosestSq)
			{
				leftClosestSq = distSq;
				leftCoverage = rightPoint.distance - (dir.len() * t);
			}
		}

		if (IntersectCoverPlane(left, -dir, rightPlane, &point, &t))
		{
			float distSq = (eyeLoc - point).len2();
			if (distSq < rightClosestSq)
			{
				rightClosestSq = distSq;
				rightCoverage = leftPoint.distance + (dir.len() * t);
			}
		}
	}

	if (m_flags & ICoverSystem::SurfaceInfo::Looped)
	{
		if ((leftCoverage < 0.0f) || (rightCoverage < 0.0f))
			return false;

		if (leftCoverage > rightCoverage)
			rightCoverage += coverPath.GetLength();
	}
	else
	{
		if ((leftCoverage < 0.0f) && (rightCoverage > 0.0f))
			leftCoverage = 0.0f;
		else if ((rightCoverage < 0.0f) && (leftCoverage > 0.0f))
			rightCoverage = coverPath.GetLength();
	}

	*interval = CoverInterval(leftCoverage, rightCoverage);

	return true;
}

bool CoverSurface::IntersectCoverPlane(const Vec3& origin, const Vec3& length, const Plane& plane, Vec3* point, float* t) const
{
	float planeNormalDotSegment = plane.n | length;

	if ((planeNormalDotSegment == 0.0f) || (planeNormalDotSegment > 0.0f))
		return false;

	float distanceToStart = plane.DistFromPlane(origin);
	float scale = -distanceToStart / planeNormalDotSegment;

	if ((scale < 0.0f) || (scale > 1.0f))
		return false;

	*point = origin + length * scale;
	*t = scale;

	return true;
}

void CoverSurface::FindCoverPlanes(const Vec3& eye, Plane& left, Plane& right) const
{
	const uint32 coverSampleCount = m_samples.size();

	if (coverSampleCount < 2)
		return;

	Vec3 leftToEye = eye - m_samples[m_segments.front().leftIdx].position;
	leftToEye.z = 0.0f;
	Vec3 leftNormal = leftToEye.Cross(CoverUp).normalized();
	left.SetPlane(leftNormal, eye);

	Vec3 rightToEye = eye - m_samples[m_segments.back().rightIdx].position;
	rightToEye.z = 0.0f;
	Vec3 rightNormal = CoverUp.Cross(rightToEye).normalized();
	right.SetPlane(rightNormal, eye);

	for (uint32 i = 1; i < coverSampleCount - 1; ++i)
	{
		const Sample& sample = m_samples[i];

		if (left.DistFromPlane(sample.position) < 0.0f)
		{
			leftToEye = eye - sample.position;
			leftToEye.z = 0.0f;
			leftNormal = leftToEye.Cross(CoverUp).normalized();
			left.SetPlane(leftNormal, eye);
		}
		else if (right.DistFromPlane(sample.position) < 0.0f)
		{
			rightToEye = eye - sample.position;
			rightToEye.z = 0.0f;
			rightNormal = CoverUp.Cross(rightToEye).normalized();
			right.SetPlane(rightNormal, eye);
		}
	}
}

void CoverSurface::DebugDraw() const
{
	uint32 coverSampleCount = m_samples.size();
	uint32 segmentCount = m_segments.size();

	if (coverSampleCount < 2)
		return;

	static std::vector<Vec3> coverSurface;
	coverSurface.resize(2 * coverSampleCount * 6);

	const float offset = -0.05f;
	const float floorOffset = 0.005f;

	bool looped = (m_flags& ICoverSystem::SurfaceInfo::Looped) != 0;

	CDebugDrawContext dc;
	dc->SetBackFaceCulling(false);
	dc->SetDepthWrite(false);

	// Low Cover
	uint32 k = 0;
	bool wasDynamic = false;
	for (uint32 i = 0; i < segmentCount; ++i)
	{
		const Segment& segment = m_segments[i];

		if (segment.flags & Segment::Disabled)
			continue;

		const Sample& left = m_samples[segment.leftIdx];
		const Sample& right = m_samples[segment.rightIdx];

		bool isDynamic = (left.flags & ICoverSampler::Sample::Dynamic) && (right.flags & ICoverSampler::Sample::Dynamic);

		if (isDynamic != wasDynamic)
		{
			if (k > 0)
			{
				ColorB color(wasDynamic ? Col_Yellow : Col_DarkGreen);
				color.a = 128;

				dc->DrawTriangles(&coverSurface[0], k * 6, color);
				k = 0;
			}

			wasDynamic = isDynamic;
		}

		Vec3 leftNormal = (i > 0) ?
		                  (segment.normal + m_segments[i - 1].normal).normalized() : segment.normal;
		Vec3 rightNormal = (i < segmentCount - 1) ?
		                   (segment.normal + m_segments[i + 1].normal).normalized() : segment.normal;

		Vec3 bottomLeft(left.position - (leftNormal * offset));
		Vec3 bottomRight(right.position - (rightNormal * offset));

		coverSurface[k * 6 + 0] = bottomLeft;
		coverSurface[k * 6 + 1] = bottomRight + Vec3(0.0f, 0.0f, min(right.GetHeight(), LowCoverMaxHeight));
		coverSurface[k * 6 + 2] = bottomRight;
		coverSurface[k * 6 + 3] = bottomLeft;
		coverSurface[k * 6 + 4] = bottomLeft + Vec3(0.0f, 0.0f, min(left.GetHeight(), LowCoverMaxHeight));
		coverSurface[k * 6 + 5] = bottomRight + Vec3(0.0f, 0.0f, min(right.GetHeight(), LowCoverMaxHeight));

		++k;
	}

	if (k > 0)
	{
		ColorB color(wasDynamic ? Col_Yellow : Col_DarkGreen);
		color.a = 128;

		dc->DrawTriangles(&coverSurface[0], k * 6, color);
	}

	if (looped)
	{
		ColorB color(Col_SlateBlue);
		color.a = 128;

		dc->DrawTriangles(&coverSurface[(segmentCount - 1) * 6], 6, color);
	}

	// High Cover
	k = 0;
	wasDynamic = false;
	for (uint32 i = 0; i < segmentCount; ++i)
	{
		const Segment& segment = m_segments[i];

		if (segment.flags & Segment::Disabled)
			continue;

		const Sample& left = m_samples[segment.leftIdx];
		const Sample& right = m_samples[segment.rightIdx];

		bool isDynamic = (left.flags & ICoverSampler::Sample::Dynamic) && (right.flags & ICoverSampler::Sample::Dynamic);

		if (isDynamic != wasDynamic)
		{
			if (k > 0)
			{
				ColorB color(wasDynamic ? Col_YellowGreen : Col_MediumSeaGreen);
				color.a = 128;

				dc->DrawTriangles(&coverSurface[0], k * 6, color);
				k = 0;
			}

			wasDynamic = isDynamic;
		}

		if ((left.GetHeight() <= LowCoverMaxHeight) && (right.GetHeight() <= LowCoverMaxHeight))
			continue;

		Vec3 leftNormal = (i > 0) ?
		                  (segment.normal + m_segments[i - 1].normal).normalized() : segment.normal;
		Vec3 rightNormal = (i < segmentCount - 1) ?
		                   (segment.normal + m_segments[i + 1].normal).normalized() : segment.normal;

		Vec3 bottomLeft(left.position - (leftNormal * offset));
		bottomLeft += Vec3(0.0f, 0.0f, min(left.GetHeight(), LowCoverMaxHeight));
		Vec3 bottomRight(right.position - (rightNormal * offset));
		bottomRight += Vec3(0.0f, 0.0f, min(right.GetHeight(), LowCoverMaxHeight));

		coverSurface[k * 6 + 0] = bottomLeft;
		coverSurface[k * 6 + 1] = bottomRight + Vec3(0.0f, 0.0f, max(right.GetHeight() - LowCoverMaxHeight, 0.0f));
		coverSurface[k * 6 + 2] = bottomRight;
		coverSurface[k * 6 + 3] = bottomLeft;
		coverSurface[k * 6 + 4] = bottomLeft + Vec3(0.0f, 0.0f, max(left.GetHeight() - LowCoverMaxHeight, 0.0f));
		coverSurface[k * 6 + 5] = bottomRight + Vec3(0.0f, 0.0f, max(right.GetHeight() - LowCoverMaxHeight, 0.0f));

		++k;
	}

	if (k > 0)
	{
		{
			ColorB color(wasDynamic ? Col_YellowGreen : Col_MediumSeaGreen);
			color.a = 128;

			uint32 drawCount = looped ? k - 1 : k;
			dc->DrawTriangles(&coverSurface[0], drawCount * 6, color);
		}

		if (looped)
		{
			ColorB color(Col_SteelBlue);
			color.a = 128;

			dc->DrawTriangles(&coverSurface[(k - 1) * 6], 6, color);
		}
	}

	// Segments
	for (uint32 i = 0; i < segmentCount; ++i)
	{
		const Segment& segment = m_segments[i];

		Vec3 leftNormal = (i > 0) ?
		                  (0.5f * (segment.normal + m_segments[i - 1].normal)) : segment.normal;
		Vec3 rightNormal = (i < segmentCount - 1) ?
		                   (0.5f * (segment.normal + m_segments[i + 1].normal)) : segment.normal;

		const Vec3& left = m_samples[segment.leftIdx].position - (leftNormal * offset) + (CoverUp * floorOffset);
		const Vec3& right = m_samples[segment.rightIdx].position - (rightNormal * offset) + (CoverUp * floorOffset);

		Vec3 segmentCenter = (left + right) * 0.5f;

		bool loopSegment = looped && (i == (segmentCount - 1));

		dc->DrawLine(left, loopSegment ? Col_SlateBlue : Col_DarkWood, right, loopSegment ? Col_SlateBlue : Col_DarkWood, 10.0f);
		dc->DrawLine(segmentCenter, loopSegment ? Col_SlateBlue : Col_Goldenrod, segmentCenter + segment.normal * 0.15f,
		             loopSegment ? Col_SlateBlue : Col_Goldenrod);
		dc->DrawCone(segmentCenter + segment.normal * 0.15f, segment.normal, 0.025f, 0.05f,
		             loopSegment ? Col_SlateBlue : Col_Goldenrod);
	}

	// Samples
	{
		for (uint32 i = 0; i < segmentCount; ++i)
		{
			const Segment& segment = m_segments[i];

			Vec3 leftNormal = (i > 0) ?
			                  (0.5f * (segment.normal + m_segments[i - 1].normal)) : segment.normal;

			dc->DrawSphere(m_samples[segment.leftIdx].position - (leftNormal * offset) + (CoverUp * floorOffset),
			               0.035f, ColorB(Col_FireBrick), true);
		}

		// Last Sample
		const Segment& segment = m_segments[segmentCount - 1];
		Vec3 rightNormal = segment.normal;
		dc->DrawSphere(m_samples[segment.rightIdx].position - (rightNormal * offset) + (CoverUp * floorOffset),
		               0.035f, ColorB(Col_FireBrick), true);
	}

	// Locations
	if (gAIEnv.CVars.DebugDrawCoverLocations)
	{
		uint32 locationCount = m_locations.size();
		for (uint32 i = 0; i < locationCount; ++i)
		{
			const Location& location = m_locations[i];

			Vec3 pos = GetLocation(i, 0.5f);

			const float height = 0.05f;
			dc->DrawCylinder(pos + CoverUp * height * 0.5f, CoverUp, 0.45f, height, ColorB(Col_Tan, 0.35f));
		}
	}
}
