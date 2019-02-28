// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Shape2.h"

static bool useForbiddenMask = false;
static unsigned forbiddenMaskMaxDimension = 256; // max side of the mask (256->16KB)
static float forbiddenMaskGranularity = 2.0f;
static float criticalForbiddenSize = 30.0f;

//===================================================================
// CShapeMask
//===================================================================
CShapeMask::CShapeMask() : m_nx(0), m_ny(0), m_dx(0), m_dy(0), m_bytesPerRow(0)
{
}

//===================================================================
// CShapeMask
//===================================================================
CShapeMask::~CShapeMask()
{
}

//===================================================================
// Build
//===================================================================
void CShapeMask::Build(const struct SShape* pShape, float granularity)
{
	AIAssert(pShape);
	AIAssert(granularity > 0.0f);
	m_aabb = pShape->aabb;
	Vec3 delta = m_aabb.max - m_aabb.min;
	m_nx = 1 + (unsigned)(delta.x / granularity);
	m_ny = 1 + (unsigned)(delta.y / granularity);

	if (m_nx > forbiddenMaskMaxDimension)
		m_nx = forbiddenMaskMaxDimension;
	if (m_ny > forbiddenMaskMaxDimension)
		m_ny = forbiddenMaskMaxDimension;

	m_dx = delta.x / m_nx;
	m_dy = delta.y / m_ny;

	m_bytesPerRow = (m_nx + 3) >> 2;

	m_container.resize(m_ny * m_bytesPerRow);

	for (unsigned j = 0; j != m_ny; ++j)
	{
		for (unsigned i = 0; i != m_nx; ++i)
		{
			unsigned index = (i >> 2) + m_bytesPerRow * j;
			assert(index < m_container.size());
			unsigned char& byte = m_container[index];

			unsigned offset = (i & 3) * 2;

			Vec3 min, max;
			GetBox(min, max, i, j);

			unsigned char value = GetType(pShape, min, max) << offset;
			unsigned char mask = 3 << offset;
			byte = (byte & ~mask) + value;
		}
	}
}

//===================================================================
// GetType
//===================================================================
CShapeMask::EType CShapeMask::GetType(const Vec3 pt) const
{
	if (!useForbiddenMask)
		return TYPE_EDGE;
	int i = (int) ((pt.x - m_aabb.min.x) / m_dx);
	if (i < 0 || i >= (int)m_nx)
		return TYPE_OUT;
	int j = (int) ((pt.x - m_aabb.min.y) / m_dx);
	if (j < 0 || j >= (int)m_ny)
		return TYPE_OUT;

	unsigned index = (i >> 2) + m_bytesPerRow * j;
	assert(index < m_container.size());
	const unsigned char& byte = m_container[index];

	unsigned offset = (i & 2) * 2;
	unsigned char mask = 3 << offset;

	unsigned char value = byte & mask;
	value = value >> offset;
	return (EType) value;
}

//===================================================================
// MemStats
//===================================================================
size_t CShapeMask::MemStats() const
{
	size_t size = sizeof(*this);
	size += m_container.capacity() * sizeof(unsigned char);
	return size;
}

//===================================================================
// GetType
//===================================================================
CShapeMask::EType CShapeMask::GetType(const struct SShape* pShape, const Vec3& min, const Vec3& max) const
{
	Vec3 delta = max - min;
	if (Overlap::Lineseg_Polygon2D(Lineseg(min, min + Vec3(delta.x, 0.0f, 0.0f)), pShape->shape, &pShape->aabb))
		return TYPE_EDGE;
	if (Overlap::Lineseg_Polygon2D(Lineseg(min + Vec3(0.0f, delta.y, 0.0f), min + Vec3(delta.x, delta.y, 0.0f)), pShape->shape, &pShape->aabb))
		return TYPE_EDGE;
	if (Overlap::Lineseg_Polygon2D(Lineseg(min, min + Vec3(0.0f, delta.y, 0.0f)), pShape->shape, &pShape->aabb))
		return TYPE_EDGE;
	if (Overlap::Lineseg_Polygon2D(Lineseg(min + Vec3(delta.x, 0.0f, 0.0f), min + Vec3(delta.x, delta.y, 0.0f)), pShape->shape, &pShape->aabb))
		return TYPE_EDGE;

	Vec3 mid = 0.5f * (min + max);
	if (Overlap::Point_Polygon2D(mid, pShape->shape, &pShape->aabb))
		return TYPE_IN;
	else
		return TYPE_OUT;
}

//===================================================================
// GetBox
//===================================================================
void CShapeMask::GetBox(Vec3& min, Vec3& max, int i, int j)
{
	min = m_aabb.min + Vec3(i * m_dx, j * m_dy, 0.0f);
	max = min + Vec3(m_dx, m_dy, 0.0f);
}

//===================================================================
// SShape
//===================================================================
SShape::SShape(const ListPositions& shape_, bool allowMask, IAISystem::ENavigationType navType_, int type_,
               bool closed_, float height_, EAILightLevel lightLevel_, bool temp) :
	shape(shape_), navType(navType_), type(type_), devalueTime(0), height(height_),
	temporary(temp), enabled(true), shapeMask(0), lightLevel(lightLevel_), closed(closed_)
{
	RecalcAABB();
	if (allowMask)
		BuildMask(forbiddenMaskGranularity);
}

SShape::SShape() : navType(IAISystem::NAV_UNSET)
	, type(0)
	, devalueTime(0.0f)
	, height(0.0f)
	, temporary(false)
	, aabb(AABB::RESET)
	, enabled(true)
	, shapeMask(0)
	, lightLevel(AILL_NONE)
	, closed(false)
{

}

SShape::SShape(const ListPositions& shape_, const AABB& aabb_, IAISystem::ENavigationType navType /*= IAISystem::NAV_UNSET */,
               int type /*= 0 */, bool closed_ /*= false */, float height_ /*= 0.0f */, EAILightLevel lightLevel_ /*= AILL_NONE */,
               bool temp /*= false */)
	: shape(shape_)
	, aabb(aabb_)
	, navType(IAISystem::NAV_UNSET)
	, type(0)
	, devalueTime(0)
	, height(height_)
	, temporary(temp)
	, enabled(true)
	, shapeMask(0)
	, lightLevel(lightLevel_)
	, closed(closed_)
{

}
//===================================================================
// BuildMask
//===================================================================
void SShape::BuildMask(float granularity)
{
	Vec3 delta = aabb.max - aabb.min;
	if (useForbiddenMask && (delta.x > criticalForbiddenSize || delta.y > criticalForbiddenSize))
	{
		if (!shapeMask)
			shapeMask = new CShapeMask;
		shapeMask->Build(this, forbiddenMaskGranularity);
	}
	else
	{
		ReleaseMask();
	}
}

SShape::~SShape()
{
	ReleaseMask();
}

void SShape::RecalcAABB()
{
	aabb.Reset();
	aabb.min.z = 10000.0f;  // avoid including limit
	aabb.max.z = -10000.0f;
	for (ListPositions::const_iterator it = shape.begin(); it != shape.end(); ++it)
		aabb.Add(*it);
}

void SShape::ReleaseMask()
{
	delete shapeMask;
	shapeMask = 0;
}

//
// offsets this shape(called when segmented world shifts)
void SShape::OffsetShape(const Vec3& offset)
{
	for (ListPositions::iterator it = shape.begin(); it != shape.end(); ++it)
	{
		(*it) += offset;
	}
	aabb.Move(offset);
}

ListPositions::const_iterator SShape::NearestPointOnPath(const Vec3& pos, bool forceLoop, float& dist, Vec3& nearestPt,
                                                         float* distAlongPath /*= 0*/, Vec3* segmentDir /*= 0*/,
                                                         uint32* segmentStartIndex, float* pathLength, float* segmentFraction) const
{
	if (shape.empty())
		return shape.end();

	dist = FLT_MAX;

	size_t nearest = 0;
	size_t cur = 0;
	size_t next = 1;
	size_t size = shape.size();
	size_t loopEnd = size;

	if (forceLoop && !closed)
		loopEnd += 1;

	float pathLen = 0.0f;

	while (next != loopEnd)
	{
		Lineseg seg(shape[cur], shape[next % size]);
		float t;
		float d = Distance::Point_Lineseg(pos, seg, t);
		if (d < dist)
		{
			dist = d;
			nearestPt = seg.GetPoint(t);
			if (distAlongPath)
				*distAlongPath = pathLen + Distance::Point_Point(seg.start, nearestPt);
			if (segmentDir)
				*segmentDir = seg.end - seg.start;
			if (segmentStartIndex)
				*segmentStartIndex = cur;
			if (segmentFraction)
				*segmentFraction = t;
			nearest = next;
		}
		pathLen += Distance::Point_Point(seg.start, seg.end);
		cur = next;
		++next;
	}

	if (pathLength)
		*pathLength = pathLen;

	return shape.begin() + nearest;
}

int SShape::GetIntersectionDistances(const Vec3& start, const Vec3& end, float intersectDistArray[], int maxIntersections, bool testHeight /*=false*/, bool testTopBottom /*=false*/) const
{
	Lineseg ray(start, end);
	float rayLength = sqrtf(start.GetSquaredDistance2D(end));

	int intersects = 0;
	float tA, tB;  // Intersection parameters as distances along each line

	ListPositions::const_iterator cur = shape.begin();
	ListPositions::const_iterator next = cur;
	++next;
	while (next != shape.end())
	{
		Lineseg seg(*cur, *next);
		bool hit = Intersect::Lineseg_Lineseg2D(ray, seg, tA, tB);

		// Test height
		if (hit && testHeight)
		{
			// Treat all sides as being the full AABB height, so we don't get any gaps
			// between sides and top and bottom planes
			float z(ray.GetPoint(tA).z);
			if (z < aabb.min.z || z > aabb.max.z)
				hit = false;
		}

		if (hit)
		{
			intersectDistArray[intersects++] = tA * rayLength;
			if (intersects == maxIntersections) break;
		}
		++next;
		++cur;
	}

	// Test top and bottom sides of shape for intersection
	// Note that we cheat a bit and use horizontal shapes at AABB min and max heights,
	// rather than making them properly conform to heights at each shape vertex
	// Should be good enough for most shapes in practice, and is a lot cheaper
	if (testTopBottom)
	{
		Vec3 pt, norm;

		// test top
		norm = Vec3(0.f, 0.f, (end.z > start.z) ? -1.f : 1.f); // because plane test is one sided
		if (Intersect::Line_Plane(Line(start, end), Plane::CreatePlane(norm, aabb.max), pt) &&
		    IsPointInsideShape(pt, false) &&
		    intersects < maxIntersections)
			intersectDistArray[intersects++] = (pt - start).len();

		// test bottom
		norm.z *= -1;
		if (Intersect::Line_Plane(Line(start, end), Plane::CreatePlane(norm, aabb.min), pt) &&
		    IsPointInsideShape(pt, false) &&
		    intersects < maxIntersections)
			intersectDistArray[intersects++] = (pt - start).len();
	}

	std::sort(&intersectDistArray[0], &intersectDistArray[intersects]);

	return intersects;
}

Vec3 SShape::GetPointAlongPath(float dist) const
{
	if (shape.empty())
		return ZERO;

	if (dist < 0.0f)
		return shape.front();

	ListPositions::const_iterator cur = shape.begin();
	ListPositions::const_iterator next(cur);
	++next;

	float d = 0.0f;
	while (next != shape.end())
	{
		Vec3 delta = *next - *cur;
		float len = delta.GetLength();
		if (len > 0.0f && dist >= d && dist < d + len)
		{
			float t = (dist - d) / len;
			return *cur + delta * t;
		}
		d += len;
		cur = next;
		++next;
	}
	return *cur;
}

bool SShape::IsPointInsideShape(const Vec3& pos, bool checkHeight) const
{
	// Check height.
	if (checkHeight && height > 0.01f)
	{
		float h = pos.z - aabb.min.z;
		if (h < 0.0f || h > height)
			return false;
	}
	// Is the request point inside the shape.
	if (Overlap::Point_Polygon2D(pos, shape, &aabb))
		return true;
	return false;
}

bool SShape::ConstrainPointInsideShape(Vec3& pos, bool checkHeight) const
{
	if (!IsPointInsideShape(pos, checkHeight))
	{
		// Target is outside the territory, find closest point on the edge of the territory.
		float dist = 0;
		Vec3 nearest;
		NearestPointOnPath(pos, false, dist, nearest);
		// adjust height.
		nearest.z = pos.z;
		if (checkHeight && height > 0.00001f)
			nearest.z = clamp_tpl(nearest.z, aabb.min.z, aabb.min.z + height);
		pos = nearest;
		return true;
	}
	return false;
}

size_t SShape::MemStats() const
{
	size_t size = sizeof(*this) + shape.size() * sizeof(Vec3);
	if (shapeMask.Get())
		size += shapeMask->MemStats();
	return size;
}

SPerceptionModifierShape::SPerceptionModifierShape(const ListPositions& shape, float reductionPerMetre, float reductionMax, float fHeight, bool isClosed) : SShape(shape)
{
	height = fHeight;
	closed = isClosed;
	fReductionPerMetre = clamp_tpl(reductionPerMetre, 0.0f, 1.0f);
	fReductionMax = clamp_tpl(reductionMax, 0.0f, 1.0f);

	// Recalc AABB
	aabb.Reset();
	for (ListPositions::const_iterator it = shape.begin(); it != shape.end(); ++it)
		aabb.Add(*it);
	aabb.max.z = aabb.min.z + height;
}
