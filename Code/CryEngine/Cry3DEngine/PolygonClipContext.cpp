// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   PolygonClipContext.cpp
//  Created:
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "PolygonClipContext.h"

CPolygonClipContext::CPolygonClipContext()
{
}

void CPolygonClipContext::Reset()
{
	stl::free_container(m_lstPolygonA);
	stl::free_container(m_lstPolygonB);
}

int CPolygonClipContext::ClipEdge(const Vec3& v1, const Vec3& v2, const Plane& ClipPlane, Vec3& vRes1, Vec3& vRes2)
{
	float d1 = -ClipPlane.DistFromPlane(v1);
	float d2 = -ClipPlane.DistFromPlane(v2);
	if (d1 < 0 && d2 < 0)
		return 0; // all clipped = do not add any vertices

	if (d1 >= 0 && d2 >= 0)
	{
		vRes1 = v2;
		return 1; // both not clipped - add second vertex
	}

	// calculate new vertex
	Vec3 vIntersectionPoint = v1 + (v2 - v1) * (abs(d1) / (abs(d2) + abs(d1)));

#ifdef _DEBUG
	float fNewDist = -ClipPlane.DistFromPlane(vIntersectionPoint);
	assert(abs(fNewDist) < 0.01f);
#endif

	if (d1 >= 0 && d2 < 0)
	{
		// from vis to no vis
		vRes1 = vIntersectionPoint;
		return 1;
	}
	else if (d1 < 0 && d2 >= 0)
	{
		// from not vis to vis
		vRes1 = vIntersectionPoint;
		vRes2 = v2;
		return 2;
	}

	assert(0);
	return 0;
}

void CPolygonClipContext::ClipPolygon(PodArray<Vec3>& PolygonOut, const PodArray<Vec3>& pPolygon, const Plane& ClipPlane)
{
	PolygonOut.Clear();
	// clip edges, make list of new vertices
	for (int i = 0; i < pPolygon.Count(); i++)
	{
		Vec3 vNewVert1(0, 0, 0), vNewVert2(0, 0, 0);
		if (int nNewVertNum = ClipEdge(pPolygon.GetAt(i), pPolygon.GetAt((i + 1) % pPolygon.Count()), ClipPlane, vNewVert1, vNewVert2))
		{
			PolygonOut.Add(vNewVert1);
			if (nNewVertNum > 1)
				PolygonOut.Add(vNewVert2);
		}
	}

	// check result
	for (int i = 0; i < PolygonOut.Count(); i++)
	{
		float d1 = -ClipPlane.DistFromPlane(PolygonOut.GetAt(i));
		assert(d1 >= -0.01f);
	}

	assert(PolygonOut.Count() == 0 || PolygonOut.Count() >= 3);
}

const PodArray<Vec3>& CPolygonClipContext::Clip(const PodArray<Vec3>& poly, const Plane* planes, size_t numPlanes)
{
	m_lstPolygonA.Clear();
	m_lstPolygonB.Clear();

	m_lstPolygonA.AddList(poly);

	PodArray<Vec3>* src = &m_lstPolygonA, * dst = &m_lstPolygonB;

	for (size_t i = 0; i < numPlanes && src->Count() >= 3; std::swap(src, dst), i++)
		ClipPolygon(*dst, *src, planes[i]);

	return *src;
}

const PodArray<Vec3>& CPolygonClipContext::Clip(const Vec3& a, const Vec3& b, const Vec3& c, const Plane* planes, size_t numPlanes)
{
	m_lstPolygonA.Clear();
	m_lstPolygonB.Clear();

	m_lstPolygonA.Add(a);
	m_lstPolygonA.Add(b);
	m_lstPolygonA.Add(c);

	PodArray<Vec3>* src = &m_lstPolygonA, * dst = &m_lstPolygonB;

	for (size_t i = 0; i < numPlanes && src->Count() >= 3; std::swap(src, dst), i++)
		ClipPolygon(*dst, *src, planes[i]);

	return *src;
}

void CPolygonClipContext::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(m_lstPolygonA);
	pSizer->AddObject(m_lstPolygonB);
}
