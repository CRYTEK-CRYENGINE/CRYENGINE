// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "CTriangulator.h"
#include "AILog.h"
#include <math.h>
#include <algorithm>
#include <limits>
#include <CrySystem/ISystem.h>

#ifdef AI_FP_FAST
	#pragma float_control(precise, on)
	#pragma fenv_access(on)
//#else
//#error "!!!AISystem FP optimizations turned off - if you do want this, remove this #error from CTriangulator.cpp"
#endif

//====================================================================
// CTriangulator
//====================================================================
CTriangulator::CTriangulator()
{
	m_vtxBBoxMin.x = 3000;
	m_vtxBBoxMin.y = 3000;
	m_vtxBBoxMin.z = 0;

	m_vtxBBoxMax.x = 0;
	m_vtxBBoxMax.y = 0;
	m_vtxBBoxMax.z = 0;

	m_curCutIdx = 0;
}

//====================================================================
// CTriangulator
//====================================================================
CTriangulator::~CTriangulator()
{
}

//====================================================================
// DoesVertexExist2D
//====================================================================
bool CTriangulator::DoesVertexExist2D(real x, real y, real tol) const
{
	VARRAY::const_iterator i;
	for (i = m_vVertices.begin(); i != m_vVertices.end(); ++i)
	{
		const Vtx& v = *i;
		if ((fabs(x - v.x) < tol) && (fabs(y - v.y) < tol))
			return true;
	}
	return false;
}

//====================================================================
// AddVertex
//====================================================================
int CTriangulator::AddVertex(real x, real y, real z, bool bCollidable, bool bHideable)
{
	Vtx newvertex;
	newvertex.x = x;
	newvertex.y = y;
	newvertex.z = z;
	newvertex.bCollidable = bCollidable;
	newvertex.bHideable = bHideable;

	if (newvertex.x < m_vtxBBoxMin.x)
		m_vtxBBoxMin.x = newvertex.x;
	if (newvertex.x > m_vtxBBoxMax.x)
		m_vtxBBoxMax.x = newvertex.x;

	if (newvertex.y < m_vtxBBoxMin.y)
		m_vtxBBoxMin.y = newvertex.y;
	if (newvertex.y > m_vtxBBoxMax.y)
		m_vtxBBoxMax.y = newvertex.y;

	VARRAY::iterator i;
	i = std::find(m_vVertices.begin(), m_vVertices.end(), newvertex);

	if (i != m_vVertices.end())
		return -1;

	m_vVertices.push_back(newvertex);

	return (m_vVertices.size() - 1);
}

//====================================================================
// AddSegment
//====================================================================
void CTriangulator::AddSegment(const Vec3r& p1, const Vec3r& p2)
{
	m_cuts.push_back(SSegment(p1, p2));
	m_curCutIdx = 0;
}

//====================================================================
// GetSegment
//====================================================================
bool CTriangulator::GetSegment(Vec3r& p1, Vec3r& p2)
{
	if (m_curCutIdx >= m_cuts.size())
		return false;
	p1 = m_cuts[m_curCutIdx].m_p1;
	p2 = m_cuts[m_curCutIdx].m_p2;
	++m_curCutIdx;
	return true;
}

//====================================================================
// Triangulate
//====================================================================
bool CTriangulator::Triangulate()
{

	if (m_vVertices.empty()) return true;

	// init supertriangle and structures
	if (!PrepForTriangulation())
		return false;

	// perform triangulation on any new vertices
	if (!TriangulateNew())
		return false;

	return true;
}

//====================================================================
// GetVertices
//====================================================================
VARRAY CTriangulator::GetVertices()
{
	return m_vProcessed;
}

//====================================================================
// GetTriangles
//====================================================================
TARRAY CTriangulator::GetTriangles()
{
	return m_vTriangles;
}

//====================================================================
// PushUnique
//====================================================================
void CTriangulator::PushUnique(int a, int b)
{
	MYPOINT newpoint, oldpoint;
	newpoint.x = a;
	newpoint.y = b;
	oldpoint.x = b;
	oldpoint.y = a;

	tUniquePts::iterator i = std::find(m_uniquePts.begin(), m_uniquePts.end(), oldpoint);
	if (i == m_uniquePts.end())
		m_uniquePts.push_back(newpoint);
	else
		m_uniquePts.erase(i);
}

//====================================================================
// IsAntiClockwise
//====================================================================
bool CTriangulator::IsAntiClockwise(Tri* who)
{
	Vtx v1, v2, v3;

	v1 = m_vProcessed[who->v[0]];
	v2 = m_vProcessed[who->v[1]];
	v3 = m_vProcessed[who->v[2]];

	Vtx vec1, vec2;

	vec1.x = v1.x - v2.x;
	vec1.y = v1.y - v2.y;

	vec2.x = v3.x - v2.x;
	vec2.y = v3.y - v2.y;

	real f = vec1.x * vec2.y - vec2.x * vec1.y;

	if (f > 0) return true;

	return false;
}

//====================================================================
// Calculate
//====================================================================
bool CTriangulator::Calculate(Tri* pTri)
{
	const Vtx& v1 = m_vProcessed[pTri->v[0]];
	const Vtx& v2 = m_vProcessed[pTri->v[1]];
	const Vtx& v3 = m_vProcessed[pTri->v[2]];

	if (!IsPerpendicular(v1, v2, v3))
		CalcCircle(v1, v2, v3, pTri);
	else if (!IsPerpendicular(v1, v3, v2))
		CalcCircle(v1, v3, v2, pTri);
	else if (!IsPerpendicular(v2, v1, v3))
		CalcCircle(v2, v1, v3, pTri);
	else if (!IsPerpendicular(v2, v3, v1))
		CalcCircle(v2, v3, v1, pTri);
	else if (!IsPerpendicular(v3, v2, v1))
		CalcCircle(v3, v2, v1, pTri);
	else if (!IsPerpendicular(v3, v1, v2))
		CalcCircle(v3, v1, v2, pTri);
	else
	{
		// should not get here. However, we do sometimes... but by setting the radius to very
		// large this "triangle" can still get broken down and hopefully triangulated better.
		// Don't issue a warning because there's nothing the level designer can do, and
		// this case can occur a lot.
		//		AIWarning("These points are colinear: (%.2f,%.2f,%.2f) - (%.2f,%.2f,%.2f) - (%.2f,%.2f,%.2f)",v1.x,v1.y,v1.z,v2.x,v2.y,v2.z,v3.x,v3.y,v3.z);
		pTri->radiusSq = std::numeric_limits<real>::max();
		return true;
	}
	return true;
}

//====================================================================
// IsPerpendicular
//====================================================================
bool CTriangulator::IsPerpendicular(const Vtx& v1, const Vtx& v2, const Vtx& v3)
{

	double yDelta_a = v2.y - v1.y;
	double xDelta_a = v2.x - v1.x;
	double yDelta_b = v3.y - v2.y;
	double xDelta_b = v3.x - v2.x;

	// checking whether the line of the two pts are vertical
	if (fabs(xDelta_a) <= 0.000000001 && fabs(yDelta_b) <= 0.000000001)
		return false;

	if (fabs(yDelta_a) <= 0.0000001)
		return true;
	else if (fabs(yDelta_b) <= 0.0000001)
		return true;
	else if (fabs(xDelta_a) <= 0.000000001)
		return true;
	else if (fabs(xDelta_b) <= 0.000000001)
		return true;
	else
		return false;

}

//====================================================================
// CalcCircle
//====================================================================
void CTriangulator::CalcCircle(const Vtx& v1, const Vtx& v2, const Vtx& v3, Tri* pTri)
{
	double yDelta_a = v2.y - v1.y;
	double xDelta_a = v2.x - v1.x;
	double yDelta_b = v3.y - v2.y;
	double xDelta_b = v3.x - v2.x;

	if (fabs(xDelta_a) <= 0.000000001 && fabs(yDelta_b) <= 0.000000001)
	{

		pTri->center.x = 0.5f * (v2.x + v3.x);
		pTri->center.y = 0.5f * (v1.y + v2.y);
		pTri->center.z = v1.z;
		pTri->radiusSq = square(pTri->center.x - v1.x) + square(pTri->center.y - v1.y);
		return;
	}

	// IsPerpendicular() assure that xDelta(s) are not zero
	AIAssert(xDelta_a != 0.0);
	AIAssert(xDelta_b != 0.0);
	double aSlope = yDelta_a / xDelta_a; //
	double bSlope = yDelta_b / xDelta_b;
	if (fabs(aSlope - bSlope) <= 0.000000001)
	{
		// checking whether the given points are colinear.
		/*		char which[1024];
		   cry_sprintf(which,"vertices (%.2f,%.2f,%.2f),(%.2f,%.2f,%.2f),(%.2f,%.2f,%.2f) caused assert \n",v1.x,v1.y,v1.z,v2.x,v2.y,v2.z,v3.x,v3.y,v3.z);
		   OutputDebugString(&which[0]);
		   DEBUG_BREAK; // we should never get here!!!
		 */
		AIError("CTriangulator::CalcCircle vertices (%.2f,%.2f,%.2f),(%.2f,%.2f,%.2f),(%.2f,%.2f,%.2f) caused problems [Code bug]", v1.x, v1.y, v1.z, v2.x, v2.y, v2.z, v3.x, v3.y, v3.z);
		return;
	}

	// calc center
	AIAssert(2.f * (bSlope - aSlope) != 0);
	pTri->center.x = (real) ((aSlope * bSlope * (v1.y - v3.y) + bSlope * (v1.x + v2.x) - aSlope * (v2.x + v3.x)) / (2.f * (bSlope - aSlope)));
	AIAssert(aSlope != 0);
	pTri->center.y = (real) (-(pTri->center.x - (v1.x + v2.x) / 2.f) / aSlope + (v1.y + v2.y) / 2.f);
	pTri->center.z = v1.z;

	pTri->radiusSq = square(pTri->center.x - v1.x) + square(pTri->center.y - v1.y);
}

//====================================================================
// PrepForTriangulation
//====================================================================
bool CTriangulator::PrepForTriangulation(void)
{
	m_vProcessed.clear();
	m_vProcessed.reserve(m_vVertices.size());

	// calculate super-triangle
	VARRAY::iterator i;
	Vec3r min(100000, 100000, 0);
	Vec3r max(-100000, -100000, 0);

	// bounding rectangle
	for (i = m_vVertices.begin(); i != m_vVertices.end(); ++i)
	{
		const Vtx& current = (*i);
		if (current.x < min.x) min.x = current.x;
		if (current.y < min.y) min.y = current.y;
		if (current.x > max.x) max.x = current.x;
		if (current.y > max.y) max.y = current.y;
	}

	// Add 4 corner points that are a little bit outside min/max - then
	// generate 2 triangles to cover this
	Vec3r offset(10, 10, 0);

	Vtx v00(min - offset);
	Vtx v11(max + offset);
	Vtx v10(v11.x, v00.y, 0);
	Vtx v01(v00.x, v11.y, 0);

	m_vProcessed.push_back(v00);
	m_vProcessed.push_back(v10);
	m_vProcessed.push_back(v11);
	m_vProcessed.push_back(v01);

	Tri* tri0 = new Tri(0, 1, 3);
	Tri* tri1 = new Tri(1, 2, 3);

	m_vTriangles.push_back(tri0);
	m_vTriangles.push_back(tri1);

	if (!Calculate(tri0))
		return false;
	if (!Calculate(tri1))
		return false;

	return true;
}

//====================================================================
// VertSorter
// Can use this in TriangulateNew. Sorting reduces the number of flops, but
// doesn't generally speed up much.
//====================================================================
inline bool VertSorter(const Vtx& v1, const Vtx& v2)
{
	if (v1.x < v2.x)
		return true;
	else if (v1.x == v2.x)
		return v1.y < v2.y;
	else
		return false;
}

//====================================================================
// TriangulateNew
//====================================================================
bool CTriangulator::TriangulateNew(void)
{
	// triangulation goes bad (very slow) if the vertices aren't added
	// in approximate order, apart from the first 4 which define the area
	VARRAY::iterator i = m_vVertices.begin();
	//  if (m_vVertices.size() > 4)
	//  {
	//    i += 4;
	//    std::sort(i, m_vVertices.end(), VertSorter);
	//  }

	int vertCounter = 0;
	// for every vertex
	for (i = m_vVertices.begin(); i != m_vVertices.end(); ++i, ++vertCounter)
	{
		const Vtx& current = (*i);

		if (0 == (vertCounter % 500))
		{
			AILogAlways("Now on vertex %d of %" PRISIZE_T " (%5.2f percent)\n",
			            vertCounter, m_vVertices.size(), 100.0f * vertCounter / m_vVertices.size());
		}

		m_uniquePts.resize(0);
		// find enclosing circles
		TARRAY::iterator ti;

		for (ti = m_vTriangles.begin(); ti != m_vTriangles.end(); )
		{
			Tri* triangle = (*ti);

			real distSq = square(current.x - triangle->center.x) + square(current.y - triangle->center.y);

			if (distSq <= triangle->radiusSq)
			{
				PushUnique(triangle->v[0], triangle->v[1]);
				PushUnique(triangle->v[1], triangle->v[2]);
				PushUnique(triangle->v[2], triangle->v[0]);
				m_vTriangles.erase(ti++);
				delete triangle;
			}
			else
			{
				++ti;
			}
		}

		// add new triangles
		int pos = m_vProcessed.size();
		m_vProcessed.push_back(current);

		tUniquePts::iterator ui;

		for (ui = m_uniquePts.begin(); ui != m_uniquePts.end(); ++ui)
		{
			MYPOINT curr = *ui;

			Tri* newone = new Tri;
			newone->v[0] = curr.x;
			newone->v[1] = curr.y;
			newone->v[2] = pos;

			if (!IsAntiClockwise(newone))
			{
				newone->v[0] = curr.y;
				newone->v[1] = curr.x;
			}

			if (!Calculate(newone))
				return false;

			m_vTriangles.push_back(newone);
		}
	}
	m_vVertices.clear();

	return true;
}

#ifdef AI_FP_FAST
	#pragma fenv_access(off)
	#pragma float_control(precise, off)
#endif
