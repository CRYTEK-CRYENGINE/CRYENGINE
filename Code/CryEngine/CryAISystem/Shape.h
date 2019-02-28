// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   Shape.h
   $Id$
   Description: Polygon shapes used for different purposes in the AI system and various shape containers.

   The bin sort point-in-polygon check is based on:
   Graphics Gems IV: Point in Polygon Strategies by Eric Haines
   http://tog.acm.org/GraphicsGems/gemsiv/ptpoly_haines/

   -------------------------------------------------------------------------
   History:
   - 2007				: Created by Mikko Mononen
   - 2 Mar 2009	: Evgeny Adamenkov: Removed IRenderer

 *********************************************************************/

#ifndef _SHAPE_H_
#define _SHAPE_H_

#if _MSC_VER > 1000
	#pragma once
#endif

typedef std::vector<Vec3> ShapePointContainer;

class CAIShape
{
public:
	CAIShape();
	~CAIShape();

	CAIShape*                         DuplicateData(); // Duplicates data only, not acceleration structure.

	inline void                       SetName(const string& name) { m_name = name; }
	inline const string&              GetName() const             { return m_name; }

	void                              SetPoints(const std::vector<Vec3>& points);
	void                              BuildBins();
	void                              BuildAABB();

	bool                              IsPointInside(const Vec3& pt) const;
	bool                              IsPointOnEdge(const Vec3& pt, float tol, Vec3* outNormal = 0) const;
	bool                              IntersectLineSeg(const Vec3& start, const Vec3& end, float& tmin, Vec3* outClosestPoint, Vec3* outNormal = 0, bool bForceNormalOutwards = false) const;
	bool                              IntersectLineSeg(const Vec3& start, const Vec3& end, float radius) const;

	bool                              OverlapAABB(const AABB& aabb) const;

	inline ShapePointContainer&       GetPoints()       { return m_points; }
	inline const ShapePointContainer& GetPoints() const { return m_points; }
	inline const AABB&                GetAABB() const   { return m_aabb; }

	void                              DebugDraw();

	size_t                            MemStats() const;

private:

	struct Edge
	{
		inline Edge(unsigned short id, float minx, float maxx, bool fullCross) : id(id), fullCross(fullCross), minx(minx), maxx(maxx) {}
		inline bool operator<(const Edge& rhs) const { return minx < rhs.minx; }
		float          minx, maxx;
		unsigned short id;
		bool           fullCross;
	};

	struct Bin
	{
		inline void AddEdge(float vx0, float vx1, unsigned short id, bool fullCross)
		{
			if (vx0 > vx1)
				std::swap(vx0, vx1);
			// Update bin range
			if (minx > vx0)
				minx = vx0;
			if (maxx < vx1)
				maxx = vx1;
			// Insert edge
			edges.push_back(Edge(id, vx0, vx1, fullCross));
		}

		float             minx, maxx;
		std::vector<Edge> edges;
	};

	// Holds the bin sort acceleration structure for the polygon.
	struct BinSort
	{
		std::vector<Bin> bins;
		float            ydelta, invYdelta;
	};

	bool IsPointInsideSlow(const Vec3& pt) const;

	bool IsPointOnEdgeSlow(const Vec3& pt, float tol, Vec3* outNormal = 0) const;

	bool IntersectLineSegSlow(const Vec3& start, const Vec3& end, float& tmin,
	                          Vec3* outClosestPoint = 0, Vec3* outNormal = 0, bool bForceNormalOutwards = false) const;

	bool IntersectLineSegBin(const Bin& bin, float xrmin, float xrmax,
	                         const Vec3& start, const Vec3& end,
	                         float& tmin, const Vec3*& isa, const Vec3*& isb) const;

	float GetDrawZ(float x, float y);

	BinSort*            m_binSort;
	ShapePointContainer m_points;
	AABB                m_aabb;
	string              m_name;
};

#endif
