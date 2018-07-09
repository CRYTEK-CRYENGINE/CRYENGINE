// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _CAISYSTEM_H_
#define _CAISYSTEM_H_

#if _MSC_VER > 1000
	#pragma once
#endif

#include <map>
#include "AI/NavDataGeneration/AILog.h"

class CShapeMask
{
public:
	CShapeMask();
	~CShapeMask();

	/// Builds this mask according to pShape down to the granularity
	void Build(const struct SShape* pShape, float granularity);

	/// categorisation of a point according to this mask
	enum EType {TYPE_IN, TYPE_EDGE, TYPE_OUT};
	EType  GetType(const Vec3 pt) const;

	size_t MemStats() const;
private:
	/// used just during building
	EType GetType(const struct SShape* pShape, const Vec3& min, const Vec3& max) const;
	/// gets the min/max values of the 2D box indicated by i and j
	void  GetBox(Vec3& min, Vec3& max, int i, int j);
	AABB     m_aabb;
	unsigned m_nx, m_ny;
	float    m_dx, m_dy; // number of pixels

	// container bit-pair is indexed by x + y * m_bytesPerRow
	unsigned   m_bytesPerRow;
	typedef std::vector<unsigned char> TContainer;
	TContainer m_container;
};

typedef std::vector<Vec3> ListPositions;

struct SShape
{
	SShape() :
		navType(IAISystem::NAV_UNSET), type(0), devalueTime(0.0f), height(0.0f),
		temporary(false), aabb(AABB::RESET), enabled(true), shapeMask(0), lightLevel(AILL_NONE), closed(false)
	{
	}

	explicit SShape(const ListPositions& shape_, const AABB& aabb_, IAISystem::ENavigationType navType = IAISystem::NAV_UNSET,
	                int type = 0, bool closed_ = false, float height_ = 0.0f, EAILightLevel lightLevel_ = AILL_NONE, bool temp = false) :
		shape(shape_), aabb(aabb_), navType(IAISystem::NAV_UNSET), type(0), devalueTime(0), height(height_),
		temporary(temp), enabled(true), shapeMask(0), lightLevel(lightLevel_), closed(closed_)
	{
	}

	explicit SShape(const ListPositions& shape_, bool allowMask = false, IAISystem::ENavigationType navType_ = IAISystem::NAV_UNSET,
	                int type_ = 0, bool closed_ = false, float height_ = 0.0f, EAILightLevel lightLevel_ = AILL_NONE, bool temp = false);

	~SShape()
	{
		ReleaseMask();
	}

	void RecalcAABB()
	{
		aabb.Reset();
		aabb.min.z = 10000.0f;  // avoid including limit
		aabb.max.z = -10000.0f;
		for (ListPositions::const_iterator it = shape.begin(); it != shape.end(); ++it)
			aabb.Add(*it);
	}

	/// Builds a mask with a specified granularity
	void BuildMask(float granularity);

	void ReleaseMask()
	{
		delete shapeMask;
		shapeMask = 0;
	}

	ListPositions::const_iterator NearestPointOnPath(const Vec3& pos, float& dist, Vec3& nearestPt, float* distAlongPath = 0, Vec3* segmentDir = 0) const
	{
		if (shape.empty())
			return shape.end();
		ListPositions::const_iterator nearest(shape.begin());
		dist = FLT_MAX;
		ListPositions::const_iterator cur = shape.begin();
		ListPositions::const_iterator next(cur);
		++next;
		float pathLen = 0.0f;
		while (next != shape.end())
		{
			Lineseg seg(*cur, *next);
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
				nearest = next;
			}
			pathLen += Distance::Point_Point(seg.start, seg.end);
			cur = next;
			++next;
		}
		return nearest;
	}

	// Populate an array with intersection distances
	// Can combine with IsPointInsideShape to establish entry/exit where appropriate
	// If testHeight is true, will take the height of sides into account
	// If testTopBottom is true, will test for intersection on top and bottom sides of a closed shape
	int GetIntersectionDistances(const Vec3& start, const Vec3& end, float intersectDistArray[], int maxIntersections, bool testHeight = false, bool testTopBottom = false) const
	{
		Lineseg ray(start, end);
		float rayLength = sqrt(start.GetSquaredDistance2D(end));

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
				//float h = abs(ray.GetPoint(tA).z - seg.GetPoint(tB).z);
				//if (h > aabb.GetSize().z) //height)
				//	hit = false;
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
			norm = Vec3(0, 0, (end.z > start.z) ? -1 : 1); // because plane test is one sided
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

	Vec3 GetPointAlongPath(float dist) const
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

	bool IsPointInsideShape(const Vec3& pos, bool checkHeight) const
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

	bool ConstrainPointInsideShape(Vec3& pos, bool checkHeight) const
	{
		if (!IsPointInsideShape(pos, checkHeight))
		{
			// Target is outside the territory, find closest point on the edge of the territory.
			float dist = 0;
			Vec3 nearest;
			NearestPointOnPath(pos, dist, nearest);
			// adjust height.
			nearest.z = pos.z;
			if (checkHeight && height > 0.00001f)
				nearest.z = clamp_tpl(nearest.z, aabb.min.z, aabb.min.z + height);
			pos = nearest;
			return true;
		}
		return false;
	}

	size_t MemStats() const
	{
		size_t size = sizeof(*this) + shape.size() * sizeof(Vec3);
		if (shapeMask.Get())
			size += shapeMask->MemStats();
		return size;
	}

	ListPositions              shape;       /// Points describing the shape.
	AABB                       aabb;        /// Bounding box of the shape points.
	IAISystem::ENavigationType navType;     /// Navigation type associated with AIPaths
	int                        type;        /// AIobject/anchor type associated with an AIshape
	float                      height;      /// Height of the shape. The shape volume is described as [aabb.minz, aabb.minz+height]
	float                      devalueTime; /// Timeout for occupied AIpaths
	bool                       temporary;   /// Flag indicating if the path is temporary and should be deleted upon reset.
	bool                       enabled;     /// Flag indicating if the shape is enabled. Disabled shapes are excluded from queries.
	EAILightLevel              lightLevel;  /// The light level modifier of the shape.
	bool                       closed;      /// Flag indicating if the shape is a closed volume
	class ShapeMaskPtr
	{
	public:
		ShapeMaskPtr(CShapeMask* ptr = 0) : m_p(ptr) {}
		ShapeMaskPtr(const ShapeMaskPtr& other) { m_p = 0; if (other.m_p) { m_p = new CShapeMask; * m_p = *other.m_p; } }
		CShapeMask*       operator->()       { return m_p; }
		const CShapeMask* operator->() const { return m_p; }
		CShapeMask&       operator*()        { return *m_p; }
		const CShapeMask& operator*() const  { return *m_p; }
		operator CShapeMask*() { return m_p; }
		CShapeMask*       Get()              { return m_p; }
		const CShapeMask* Get() const        { return m_p; }
	private:
		CShapeMask*       m_p;
	};
	ShapeMaskPtr shapeMask;             /// Optional mask for this shape that speeds up 2D in/out/edge tests
};

// used to determine if a forbidden area should be rasterised
static float criticalForbiddenSize = 30.0f;
static float forbiddenMaskGranularity = 2.0f;
static unsigned forbiddenMaskMaxDimension = 256; // max side of the mask (256->16KB)
static bool useForbiddenMask = false;

//===================================================================
// SShape
//===================================================================
inline SShape::SShape(const ListPositions& shape_, bool allowMask, IAISystem::ENavigationType navType_,
                      int type_, bool closed_, float height_, EAILightLevel lightLevel_, bool temp) :
	shape(shape_), navType(navType_), type(type_), devalueTime(0), height(height_),
	temporary(temp), enabled(true), shapeMask(0), lightLevel(lightLevel_), closed(closed_)
{
	RecalcAABB();
	if (allowMask)
		BuildMask(forbiddenMaskGranularity);
}

//===================================================================
// BuildMask
//===================================================================
inline void SShape::BuildMask(float granularity)
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

//===================================================================
// CShapeMask
//===================================================================
inline CShapeMask::CShapeMask() : m_nx(0), m_ny(0), m_dx(0), m_dy(0)
{
}

//===================================================================
// CShapeMask
//===================================================================
inline CShapeMask::~CShapeMask()
{
}

//===================================================================
// GetBox
//===================================================================
inline void CShapeMask::GetBox(Vec3& min, Vec3& max, int i, int j)
{
	min = m_aabb.min + Vec3(i * m_dx, j * m_dy, 0.0f);
	max = min + Vec3(m_dx, m_dy, 0.0f);
}

//===================================================================
// GetType
//===================================================================
inline CShapeMask::EType CShapeMask::GetType(const struct SShape* pShape, const Vec3& min, const Vec3& max) const
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
// MemStats
//===================================================================
inline size_t CShapeMask::MemStats() const
{
	size_t size = sizeof(*this);
	size += m_container.capacity() * sizeof(unsigned char);
	return size;
}

//===================================================================
// Build
//===================================================================
inline void CShapeMask::Build(const struct SShape* pShape, float granularity)
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
inline CShapeMask::EType CShapeMask::GetType(const Vec3 pt) const
{
	if (!useForbiddenMask)
		return TYPE_EDGE;
	int i = (int) ((pt.x - m_aabb.min.x) / m_dx);
	if (i < 0 || i >= m_nx)
		return TYPE_OUT;
	int j = (int) ((pt.x - m_aabb.min.y) / m_dx);
	if (j < 0 || j >= m_ny)
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

struct SPerceptionModifierShape : SShape
{
	SPerceptionModifierShape(const ListPositions& shape, float reductionPerMetre, float reductionMax, float fHeight, bool isClosed)
		: SShape(shape)
	{
		height = fHeight;
		closed = isClosed;
		fReductionPerMetre = CLAMP(reductionPerMetre, 0.0f, 1.0f);
		fReductionMax = CLAMP(reductionMax, 0.0f, 1.0f);

		// Recalc AABB
		aabb.Reset();
		for (ListPositions::const_iterator it = shape.begin(); it != shape.end(); ++it)
			aabb.Add(*it);
		aabb.max.z = aabb.min.z + height;
	}

	float fReductionPerMetre;
	float fReductionMax;
};

typedef std::map<string, SShape>                   ShapeMap;

typedef std::map<string, SPerceptionModifierShape> PerceptionModifierShapeMap;

#endif // _CAISYSTEM_H_

