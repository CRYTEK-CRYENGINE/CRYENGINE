// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   Shape2.h
   $Id$
   Description: Temp file holding code extracted from CAISystem.h/cpp

   -------------------------------------------------------------------------
   History:

 *********************************************************************/

#ifndef _SHAPE2_H_
#define _SHAPE2_H_

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

struct SShape
{
	SShape();

	explicit SShape(
	  const ListPositions& shape_
	  , const AABB& aabb_
	  , IAISystem::ENavigationType navType = IAISystem::NAV_UNSET
	  , int type = 0
	  , bool closed_ = false
	  , float height_ = 0.0f
	  , EAILightLevel lightLevel_ = AILL_NONE
	  , bool temp = false
	  );

	explicit SShape(
	  const ListPositions& shape_
	  , bool allowMask = false
	  , IAISystem::ENavigationType navType_ = IAISystem::NAV_UNSET
	  , int type_ = 0
	  , bool closed_ = false
	  , float height_ = 0.0f
	  , EAILightLevel lightLevel_ = AILL_NONE
	  , bool temp = false);

	~SShape();

	void RecalcAABB();

	/// Builds a mask with a specified granularity
	void BuildMask(float granularity);

	void ReleaseMask();

	// offsets this shape(called when segmented world shifts)
	void                          OffsetShape(const Vec3& offset);

	ListPositions::const_iterator NearestPointOnPath(const Vec3& pos, bool forceLoop, float& dist, Vec3& nearestPt, float* distAlongPath = 0,
	                                                 Vec3* segmentDir = 0, uint32* segmentStartIndex = 0, float* pathLength = 0, float* segmentFraction = 0) const;

	// Populate an array with intersection distances
	// Can combine with IsPointInsideShape to establish entry/exit where appropriate
	// If testHeight is true, will take the height of sides into account
	// If testTopBottom is true, will test for intersection on top and bottom sides of a closed shape
	int    GetIntersectionDistances(const Vec3& start, const Vec3& end, float intersectDistArray[], int maxIntersections, bool testHeight = false, bool testTopBottom = false) const;

	Vec3   GetPointAlongPath(float dist) const;

	bool   IsPointInsideShape(const Vec3& pos, bool checkHeight) const;

	bool   ConstrainPointInsideShape(Vec3& pos, bool checkHeight) const;

	size_t MemStats() const;

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
		ShapeMaskPtr(const ShapeMaskPtr& other) : m_p(0) { if (other.m_p) { m_p = new CShapeMask; * m_p = *other.m_p; } }
		~ShapeMaskPtr() { SAFE_DELETE(m_p); }
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

typedef std::map<string, SShape> ShapeMap;

struct SPerceptionModifierShape : SShape
{
	SPerceptionModifierShape(const ListPositions& shape, float reductionPerMetre, float reductionMax, float fHeight, bool isClosed);

	float fReductionPerMetre;
	float fReductionMax;
};

typedef std::map<string, SPerceptionModifierShape> PerceptionModifierShapeMap;

#endif
