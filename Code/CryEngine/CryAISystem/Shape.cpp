// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   Shape.cpp
   $Id$
   Description: Polygon shapes used for different purposes in the AI system and various shape containers.

   The bin sort point-in-polygon check is based on:
   Graphics Gems IV: Point in Polygon Strategies by Eric Haines
   http://tog.acm.org/GraphicsGems/gemsiv/ptpoly_haines/

   -------------------------------------------------------------------------
   History:
   - 2007				: Created by Mikko Mononen
   - 2 Mar 2009	: Evgeny Adamenkov: Replaced IRenderer with CDebugDrawContext

 *********************************************************************/

#include "StdAfx.h"
#include "Shape.h"
#include "DebugDrawContext.h"

static const float BIN_WIDTH = 3.0f;

//====================================================================
// DistancePointLinesegSq
//====================================================================
static float DistancePointLinesegSq(const Vec3& p, const Vec3& start, const Vec3& end, float& t)
{
	Vec2 diff(p.x - start.x, p.y - start.y);
	Vec2 dir(end.x - start.x, end.y - start.y);
	t = diff.Dot(dir);

	if (t <= 0.0f)
	{
		t = 0.0f;
	}
	else
	{
		float sqrLen = dir.GetLength2();
		if (t >= sqrLen)
		{
			t = 1.0f;
			diff -= dir;
		}
		else
		{
			t /= sqrLen;
			diff -= t * dir;
		}
	}

	return diff.GetLength2();
}

//====================================================================
// IntersectLinesegLineseg
//====================================================================
static bool IntersectLinesegLineseg(const Vec3& startA, const Vec3& endA,
                                    const Vec3& startB, const Vec3& endB,
                                    float& tA, float& tB)
{
	// used for parallel testing
	static const float epsilon = 0.0000001f;

	Vec2 delta(startB.x - startA.x, startB.y - startA.y);
	Vec2 lineADir(endA.x - startA.x, endA.y - startA.y);
	Vec2 lineBDir(endB.x - startB.x, endB.y - startB.y);
	float crossD = lineADir.x * lineBDir.y - lineADir.y * lineBDir.x;
	float crossDelta1 = delta.x * lineBDir.y - delta.y * lineBDir.x;
	float crossDelta2 = delta.x * lineADir.y - delta.y * lineADir.x;

	if (fabs(crossD) > epsilon)
	{
		// intersection
		tA = crossDelta1 / crossD;
		tB = crossDelta2 / crossD;
	}
	else
	{
		// parallel - maybe should really test for lines overlapping each other?
		tA = tB = 0.5f;
		return false;
	}

	if (tA > 1.0f || tA < 0.0f || tB > 1.0f || tB < 0.0f)
		return false;

	return true;
}

//====================================================================
// CAIShape
//====================================================================
CAIShape::CAIShape() :
	m_binSort(0),
	m_aabb(AABB::RESET)
{
}

//====================================================================
// ~CAIShape
//====================================================================
CAIShape::~CAIShape()
{
	delete m_binSort;
}

//====================================================================
// Duplicate
//====================================================================
CAIShape* CAIShape::DuplicateData()
{
	CAIShape* pShape = new CAIShape;
	pShape->m_name = m_name;
	pShape->m_aabb = m_aabb;
	pShape->m_points = m_points;
	return pShape;
}

//====================================================================
// SetPoints
//====================================================================
void CAIShape::SetPoints(const std::vector<Vec3>& points)
{
	m_points.resize(points.size());
	for (unsigned i = 0, ni = points.size(); i < ni; ++i)
		m_points[i] = points[i];
	BuildAABB();
}

//====================================================================
// BuildAABB
//====================================================================
void CAIShape::BuildAABB()
{
	// Build AABB
	m_aabb.Reset();
	for (unsigned i = 0, ni = m_points.size(); i < ni; ++i)
		m_aabb.Add(m_points[i]);

	// add a little to the bounds to ensure everything falls inside area
	const float EPSILON = 0.00001f;
	float rangex = m_aabb.max.x - m_aabb.min.x;
	float rangey = m_aabb.max.y - m_aabb.min.y;
	m_aabb.min.x -= EPSILON * rangex;
	m_aabb.max.x += EPSILON * rangex;
	m_aabb.min.y -= EPSILON * rangey;
	m_aabb.max.y += EPSILON * rangey;
}

//====================================================================
// BuildBins
//====================================================================
void CAIShape::BuildBins()
{
	if (m_binSort)
	{
		delete m_binSort;
		m_binSort = 0;
	}

	// Do not build hte bins if too few points.
	if (m_points.size() < 15)
		return;

	const float minx = m_aabb.min.x;
	const float maxx = m_aabb.max.x;
	const float miny = m_aabb.min.y;
	const float maxy = m_aabb.max.y;

	unsigned bins = (unsigned)ceilf((maxy - miny) / BIN_WIDTH);
	if (bins < 2)
		return;

	// Limit the size of bins so that the acceleration structure does not take more memory than the points.
	if (bins > m_points.size() / 2)
		bins = m_points.size() / 2;

	m_binSort = new BinSort;

	m_binSort->bins.resize(bins);

	m_binSort->ydelta = (maxy - miny) / (float)bins;
	m_binSort->invYdelta = 1.0f / m_binSort->ydelta;

	// find how many locations to allocate for each bin
	std::vector<unsigned> binTot(bins);
	for (unsigned i = 0, ni = binTot.size(); i < ni; ++i)
		binTot[i] = 0;

	Vec3* p0 = 0;
	Vec3* p1 = 0;

	p0 = &m_points.back();
	for (unsigned i = 0, ni = m_points.size(); i < ni; ++i)
	{
		p1 = &m_points[i];

		// skip if Y's identical (edge has no effect)
		if (p0->y != p1->y)
		{
			Vec3* pa = p0;
			Vec3* pb = p1;
			if (pa->y > pb->y)
				std::swap(pa, pb);

			float fba = (pa->y - miny) * m_binSort->invYdelta;
			int ba = (int)fba;
			float fbb = (pb->y - miny) * m_binSort->invYdelta;
			int bb = (int)fbb;
			// if high vertex ends on a boundary, don't go into next boundary
			if (fbb == (float)bb)
				bb--;

			if (ba < 0) ba = 0;
			if (bb >= (int)bins) bb = (int)bins - 1;

			//			AIAssert(ba >= 0 && ba < (int)bins);
			//			AIAssert(bb >= 0 && bb < (int)bins);

			// mark the bins with this edge
			for (int j = ba; j <= bb; j++)
				binTot[j]++;
		}

		p0 = p1;
	}

	// allocate the bin contents and fill in some basics
	for (unsigned i = 0, ni = m_binSort->bins.size(); i < ni; ++i)
	{
		Bin& bin = m_binSort->bins[i];
		bin.edges.reserve(binTot[i]);
		// start these off at some awful values; refined below
		bin.minx = maxx;
		bin.maxx = minx;
	}

	// now go through list yet again, putting edges in bins
	p0 = &m_points.back();
	unsigned short id = (unsigned short)(m_points.size() - 1);
	for (unsigned i = 0, ni = m_points.size(); i < ni; ++i)
	{
		p1 = &m_points[i];

		// skip if Y's identical (edge has no effect)
		if (p0->y != p1->y)
		{
			Vec3* pa = p0;
			Vec3* pb = p1;
			if (pa->y > pb->y)
				std::swap(pa, pb);

			float fba = (pa->y - miny) * m_binSort->invYdelta;
			int ba = (int)fba;
			float fbb = (pb->y - miny) * m_binSort->invYdelta;
			int bb = (int)fbb;
			// if high vertex ends on a boundary, don't go into it
			if (bb > ba && fbb == (float)bb)
				bb--;

			if (ba < 0) ba = 0;
			if (bb >= (int)bins) bb = (int)bins - 1;

			//			AIAssert(ba >= 0 && ba < (int)bins);
			//			AIAssert(bb >= 0 && bb < (int)bins);

			float vx0 = pa->x;
			float dy = pb->y - pa->y;
			float slope = m_binSort->ydelta * (pb->x - pa->x) / dy;

			// set vx1 in case loop is not entered
			float vx1 = vx0;
			bool fullCross = false;
			for (int j = ba; j < bb; j++, vx0 = vx1)
			{
				// could increment vx1, but for greater accuracy recompute it
				vx1 = pa->x + ((float)(j + 1) - fba) * slope;
				m_binSort->bins[j].AddEdge(vx0, vx1, id, fullCross);
				fullCross = true;
			}

			// at last bin - fill as above, but with vx1 = p1->x
			vx0 = vx1;
			vx1 = pb->x;
			m_binSort->bins[bb].AddEdge(vx0, vx1, id, false); // the last bin is never a full crossing
		}

		id = (unsigned short)i;
		p0 = p1;
	}

	// finally, sort the bins' contents by minx
	for (unsigned i = 0, ni = m_binSort->bins.size(); i < ni; ++i)
		std::sort(m_binSort->bins[i].edges.begin(), m_binSort->bins[i].edges.end());
}

//====================================================================
// IsPointInsideSlow
//====================================================================
bool CAIShape::IsPointInsideSlow(const Vec3& pt) const
{
	return Overlap::Point_Polygon2D(pt, m_points, &m_aabb);
}

//====================================================================
// IsPointInside
//====================================================================
bool CAIShape::IsPointInside(const Vec3& pt) const
{
	if (!m_binSort)
	{
		return IsPointInsideSlow(pt);
	}

	bool insideFlag = false;

	// first, is point inside bounding rectangle?
	//	if (!Overlap::Point_AABB2D(pt, m_aabb))
	//		return false;

	// what bin are we in?
	int b = (int)((pt.y - m_aabb.min.y) * m_binSort->invYdelta);

	// Outside the bin range, outside the shape.
	if (b < 0 || b >= (int)m_binSort->bins.size())
		return false;

	const Bin& bin = m_binSort->bins[b];
	// find if we're inside this bin's bounds
	if (pt.x < bin.minx || pt.x > bin.maxx)
		return false;

	// now search bin for crossings
	const unsigned npts = m_points.size();
	for (unsigned j = 0, nj = bin.edges.size(); j < nj; ++j)
	{
		const Edge* edge = &bin.edges[j];
		if (pt.x < edge->minx)
		{
			// all remaining edges are to right of point, so test them
			do
			{
				if (edge->fullCross)
				{
					insideFlag = !insideFlag;
				}
				else
				{
					unsigned id = edge->id;
					if ((pt.y <= m_points[id].y) != (pt.y <= m_points[(id + 1) % npts].y))
					{
						// point crosses edge in Y, so must cross.
						insideFlag = !insideFlag;
					}
				}
				edge++;
			}
			while (++j < nj);

			return insideFlag;
		}
		else if (pt.x < edge->maxx)
		{
			// edge is overlapping point in X, check it
			unsigned id = edge->id;
			const Vec3& vtx0 = m_points[id];
			const Vec3& vtx1 = m_points[(id + 1) % npts];

			if (edge->fullCross || (pt.y <= vtx0.y) != (pt.y <= vtx1.y))
			{
				// edge crosses in Y, so have to do full crossings test
				if ((vtx0.x - (vtx0.y - pt.y) * (vtx1.x - vtx0.x) / (vtx1.y - vtx0.y)) >= pt.x)
					insideFlag = !insideFlag;
			}
		} // else edge is to left of point, ignore it
	}

	return insideFlag;
}

//====================================================================
// IsPointOnEdgeSlow
//====================================================================
bool CAIShape::IsPointOnEdgeSlow(const Vec3& pt, float tol, Vec3* outNormal) const
{
	float tolSq = sqr(tol);

	unsigned j = m_points.size() - 1;
	for (unsigned i = 0, ni = m_points.size(); i < ni; j = i, ++i)
	{
		const Vec3& sa = m_points[j];
		const Vec3& sb = m_points[i];
		float t;
		if (DistancePointLinesegSq(pt, sa, sb, t) < tolSq)
		{
			Vec3 polySeg = sb - sa;
			Vec3 intersectionPoint = sa + t * polySeg;
			Vec3 intSeg = (intersectionPoint - pt);

			Vec3 normal(polySeg.y, -polySeg.x, 0.0f);
			normal.Normalize();
			// returns the normal towards the start point of the intersecting segment
			if ((intSeg.Dot(normal)) > 0.0f)
			{
				normal.x = -normal.x;
				normal.y = -normal.y;
			}

			if (outNormal)
				*outNormal = normal;

			return true;
		}
	}

	return false;
}

//====================================================================
// IsPointOnEdge
//====================================================================
bool CAIShape::IsPointOnEdge(const Vec3& pt, float tol, Vec3* outNormal) const
{
	if (!Overlap::Sphere_AABB2D(Sphere(pt, tol), m_aabb))
		return false;

	if (!m_binSort)
	{
		return IsPointOnEdgeSlow(pt, tol, outNormal);
	}

	const unsigned bins = m_binSort->bins.size();

	// Calculate x and y ranges.
	const float xrmin = pt.x - tol;
	const float xrmax = pt.x + tol;
	const float yrmin = pt.y - tol;
	const float yrmax = pt.y + tol;

	// Check all bins that overlap the circle.
	float fba = (yrmin - m_aabb.min.y) * m_binSort->invYdelta;
	int ba = (int)fba;
	float fbb = (yrmax - m_aabb.min.y) * m_binSort->invYdelta;
	int bb = (int)fbb;
	// if high vertex ends on a boundary, don't go into it
	if (bb > ba && fbb == (float)bb)
		bb--;

	// Sanity check for index ranges.
	if (ba < 0) ba = 0;
	if (bb >= (int)bins) bb = (int)bins - 1;

	float tolSq = sqr(tol);

	for (int i = ba; i <= bb; i++)
	{
		const Bin& bin = m_binSort->bins[i];

		// Check the bins.
		const unsigned npts = m_points.size();
		for (unsigned j = 0, nj = bin.edges.size(); j < nj; ++j)
		{
			const Edge* edge = &bin.edges[j];
			// Skip if the X range does not overlap.
			if (xrmin > edge->maxx || xrmax < edge->minx)
				continue;

			unsigned id = edge->id;
			const Vec3& sa = m_points[id];
			const Vec3& sb = m_points[(id + 1) % npts];

			float t;
			if (DistancePointLinesegSq(pt, sa, sb, t) < tolSq)
			{
				Vec3 polySeg = sb - sa;
				Vec3 intersectionPoint = sa + t * polySeg;
				Vec3 intSeg = (intersectionPoint - pt);

				Vec3 normal(polySeg.y, -polySeg.x, 0.0f);
				normal.Normalize();
				// returns the normal towards the start point of the intersecting segment
				if ((intSeg.Dot(normal)) > 0.0f)
				{
					normal.x = -normal.x;
					normal.y = -normal.y;
				}

				if (outNormal)
					*outNormal = normal;

				return true;
			}
		}
	}

	return false;
}

//====================================================================
// IntersectLineSegSlow
//====================================================================
bool CAIShape::IntersectLineSegSlow(const Vec3& start, const Vec3& end, float& tmin,
                                    Vec3* outClosestPoint, Vec3* outNormal, bool bForceNormalOutwards) const
{
	const Vec3* isa = 0;
	const Vec3* isb = 0;

	tmin = 1.0f;
	bool intersect = false;

	size_t pointCount = m_points.size();

	for (size_t i = 1; i <= pointCount; ++i)
	{
		const Vec3 sa = m_points[i - 1];
		const Vec3 sb = m_points[i % pointCount];

		float s, t;
		if (IntersectLinesegLineseg(start, end, sa, sb, s, t))
		{
			if (s < 0.00001f || s > 0.99999f || t < 0.00001f || t > 0.99999f)
				continue;

			if (s < tmin)
			{
				tmin = s;
				intersect = true;
				isa = &sa;
				isb = &sb;
			}
		}
	}

	if (intersect && outClosestPoint)
		*outClosestPoint = start + tmin * (end - start);

	if (intersect && outNormal)
	{
		Vec3 polyseg = *isb - *isa;
		Vec3 intSeg = end - start;
		outNormal->x = polyseg.y;
		outNormal->y = -polyseg.x;
		outNormal->z = 0;
		outNormal->Normalize();
		// returns the normal towards the start point of the intersecting segment (if it's not forced to be outwards)
		if (!bForceNormalOutwards && intSeg.Dot(*outNormal) > 0)
		{
			outNormal->x = -outNormal->x;
			outNormal->y = -outNormal->y;
		}
	}

	return intersect;
}

//====================================================================
// IntersectLineSegBin
//====================================================================
bool CAIShape::IntersectLineSegBin(const Bin& bin, float xrmin, float xrmax,
                                   const Vec3& start, const Vec3& end,
                                   float& tmin, const Vec3*& isa, const Vec3*& isb) const
{
	if (xrmin > xrmax)
		std::swap(xrmin, xrmax);

	const float EPSILON = 0.00001f;
	xrmin -= EPSILON;
	xrmax += EPSILON;

	bool intersect = false;

	// Check the bins.
	const unsigned npts = m_points.size();
	for (unsigned j = 0, nj = bin.edges.size(); j < nj; ++j)
	{
		const Edge* edge = &bin.edges[j];
		// Skip if the X range does not overlap.
		if (xrmin > edge->maxx || xrmax < edge->minx)
			continue;

		unsigned id = edge->id;
		const Vec3& sa = m_points[id];
		const Vec3& sb = m_points[(id + 1) % npts];

		float s, t;
		if (IntersectLinesegLineseg(start, end, sa, sb, s, t))
		{
			if (s < 0.00001f || s > 0.99999f || t < 0.00001f || t > 0.99999f)
				continue;
			if (s < tmin)
			{
				tmin = s;
				intersect = true;
				isa = &sa;
				isb = &sb;
			}
		}
	}

	return intersect;
}

//====================================================================
// IntersectLineSeg
//====================================================================
bool CAIShape::IntersectLineSeg(const Vec3& start, const Vec3& end, float& tmin,
                                Vec3* outClosestPoint, Vec3* outNormal, bool bForceNormalOutwards) const
{
	tmin = 1.0f;
	if (!Overlap::Lineseg_AABB2D(Lineseg(start, end), m_aabb))
		return false;

	if (!m_binSort)
	{
		return IntersectLineSegSlow(start, end, tmin, outClosestPoint, outNormal, bForceNormalOutwards);
	}

	const unsigned bins = m_binSort->bins.size();

	const Vec3* isa = 0;
	const Vec3* isb = 0;

	bool intersect = false;

	Vec3 pa(start);
	Vec3 pb(end);

	// skip if Y's identical (edge has no effect)
	if (pa.y > pb.y)
		std::swap(pa, pb);

	float fba = (pa.y - m_aabb.min.y) * m_binSort->invYdelta;
	int ba = (int)fba;
	float fbb = (pb.y - m_aabb.min.y) * m_binSort->invYdelta;
	int bb = (int)fbb;
	// if high vertex ends on a boundary, don't go into it
	if (bb > ba && fbb == (float)bb)
		bb--;

	if (ba < 0) ba = 0;
	if (bb >= (int)bins) bb = (int)bins - 1;

	float vx0 = pa.x;
	float dy = pb.y - pa.y;
	float slope = dy < 0.000001f ? 0.0f : m_binSort->ydelta * (pb.x - pa.x) / dy;

	// set vx1 in case loop is not entered
	float vx1 = vx0;
	for (int i = ba; i < bb; i++, vx0 = vx1)
	{
		const Bin& bin = m_binSort->bins[i];

		// could increment vx1, but for greater accuracy recompute it
		vx1 = pa.x + ((float)(i + 1) - fba) * slope;

		if (IntersectLineSegBin(bin, vx0, vx1, start, end, tmin, isa, isb))
			intersect = true;
	}

	// at last bin - fill as above, but with vx1 = p1->x
	if (IntersectLineSegBin(m_binSort->bins[bb], vx1, pb.x, start, end, tmin, isa, isb))
		intersect = true;

	if (intersect && outClosestPoint)
		*outClosestPoint = start + tmin * (end - start);

	if (intersect && outNormal)
	{
		Vec3 polyseg = *isb - *isa;
		Vec3 intSeg = end - start;
		outNormal->x = polyseg.y;
		outNormal->y = -polyseg.x;
		outNormal->z = 0;
		outNormal->Normalize();
		// returns the normal towards the start point of the intersecting segment (if it's not forced to be outwards)
		/*			if (!bForceNormalOutwards && intSeg.Dot(Vec2(outNormal->x, outNormal->y)) > 0)
		   {
		   outNormal->x = -outNormal->x;
		   outNormal->y = -outNormal->y;
		   }*/
	}

	return intersect;
}

//====================================================================
// IntersectLineSeg
//====================================================================
bool CAIShape::IntersectLineSeg(const Vec3& start, const Vec3& end, float radius) const
{
	AABB aabb(m_aabb);
	aabb.Expand(Vec3(radius));

	Lineseg line(start, end);

	if (!Overlap::Lineseg_AABB2D(line, aabb))
		return false;

	size_t pointCount = m_points.size();
	float radiusSq = sqr(radius);

	for (size_t i = 1; i <= pointCount; ++i)
	{
		float distanceSq = Distance::Lineseg_Lineseg2DSq<float>(line, Lineseg(m_points[i - 1], m_points[i % pointCount]));

		if (distanceSq <= radiusSq)
			return true;
	}

	return false;
}

//====================================================================
// DebugDraw
//====================================================================
bool CAIShape::OverlapAABB(const AABB& aabb) const
{
	if (!Overlap::AABB_AABB2D(aabb, m_aabb))
		return false;

	const ShapePointContainer& pts = m_points;
	const unsigned npts = m_points.size();

	// Trivial reject if all points are outside any of the edges of the AABB.
	unsigned cxmin = 0, cxmax = 0, cymin = 0, cymax = 0;
	for (unsigned i = 0; i < npts; ++i)
	{
		const Vec3& v = pts[i];
		unsigned inside = 0;
		if (v.x < aabb.min.x)
			cxmin++;
		else if (v.x > aabb.max.x)
			cxmax++;
		else
			inside++;

		if (v.y < aabb.min.y)
			cymin++;
		else if (v.y > aabb.max.y)
			cymax++;
		else
			inside++;

		// The vertex is inside the AABB, there is be overlap.
		if (inside == 2)
			return true;
	}
	if (cxmin == npts || cxmax == npts || cymin == npts || cymax == npts)
		return false;

	// If any edge intersects the aabb, return true.
	for (unsigned i = 0; i < npts; ++i)
	{
		unsigned ine = i + 1;
		if (ine >= npts) ine = 0;
		if (OverlapLinesegAABB2D(pts[i], pts[ine], aabb))
			return true;
	}

	// If any aabb edge is inside the poly, return true.
	if (IsPointInside(Vec3(aabb.min.x, aabb.min.y, 0)))
		return true;
	if (IsPointInside(Vec3(aabb.max.x, aabb.min.y, 0)))
		return true;
	if (IsPointInside(Vec3(aabb.max.x, aabb.max.y, 0)))
		return true;
	if (IsPointInside(Vec3(aabb.min.x, aabb.max.y, 0)))
		return true;

	return false;
}

//====================================================================
// DebugDraw
//====================================================================
void CAIShape::DebugDraw()
{
	CDebugDrawContext dc;

	AABB bounds(m_aabb);

	// Draw shape
	dc->DrawPolyline(&m_points[0], m_points.size(), true, ColorB(255, 255, 255), 3.0f);

	dc->DrawAABB(bounds, false, ColorB(255, 255, 255, 128), eBBD_Faceted);

	// Draw bins
	if (m_binSort)
	{
		AABB aabb;
		const unsigned npts = m_points.size();
		for (unsigned i = 0, ni = m_binSort->bins.size(); i < ni; ++i)
		{
			const Bin& bin = m_binSort->bins[i];

			float y = bounds.min.y + i * m_binSort->ydelta;
			dc->DrawLine(Vec3(bin.minx, y, bounds.min.z), ColorB(255, 0, 0, 128),
			             Vec3(bin.maxx, y, bounds.min.z), ColorB(255, 0, 0, 128));

			aabb.min.y = y;
			aabb.max.y = y + m_binSort->ydelta;

			// Draw edge aabbs
			for (unsigned j = 0, nj = bin.edges.size(); j < nj; ++j)
			{
				const Edge& e = bin.edges[j];
				const Vec3& va = m_points[e.id];
				const Vec3& vb = m_points[(e.id + 1) % npts];

				aabb.min.x = e.minx;
				aabb.min.z = min(va.z, vb.z);

				aabb.max.x = e.maxx;
				aabb.max.z = max(va.z, vb.z);

				dc->DrawAABB(aabb, true, ColorB(255, 0, 0, 128), eBBD_Faceted);
			}
		}
	}
}

//====================================================================
// GetDrawZ
//====================================================================
float CAIShape::GetDrawZ(float x, float y)
{
	I3DEngine* pEngine = gEnv->p3DEngine;
	float terrainZ = pEngine->GetTerrainElevation(x, y);
	Vec3 pt(x, y, 0);
	float waterZ = pEngine->GetWaterLevel(&pt);
	return max(terrainZ, waterZ);
}

//====================================================================
// MemStats
//====================================================================
size_t CAIShape::MemStats() const
{
	size_t size = sizeof(*this);
	size += m_points.capacity() * sizeof(ShapePointContainer::value_type);
	if (m_binSort)
	{
		size += sizeof(*m_binSort);
		size += m_binSort->bins.capacity() * sizeof(Bin);
		for (unsigned i = 0, ni = m_binSort->bins.size(); i < ni; ++i)
			size += m_binSort->bins[i].edges.capacity() * sizeof(Edge);
	}
	return size;
}
