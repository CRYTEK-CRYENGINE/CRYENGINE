// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   statobjmandraw.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Draw static objects (vegetations)
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#ifdef SUPP_HMAP_OCCL

	#include "terrain.h"
	#include "StatObj.h"
	#include "ObjMan.h"
	#include "VisAreas.h"
	#include "terrain_sector.h"
	#include "3dEngine.h"
	#include "PolygonClipContext.h"

bool CHeightMap::IsBoxOccluded
(
  const AABB& objBox,
  float fDistance,
  bool bTerrainNode,
  OcclusionTestClient* const __restrict pOcclTestVars,
  const SRenderingPassInfo& passInfo
)
{
	// test occlusion by heightmap
	//	FUNCTION_PROFILER_3DENGINE;
	assert(pOcclTestVars);

	// define top face of the box
	Vec3 vTopMax = objBox.max;
	Vec3 vTopMin = objBox.min;
	vTopMin.z = vTopMax.z;

	// skip testing huge boxes
	IF ((vTopMax.x - vTopMin.x) > 100000 || (vTopMax.y - vTopMin.y) > 100000, false)
	{
		pOcclTestVars->nTerrainOccLastFrame = 0;
		return false;
	}

	const Vec3 vCamPos = passInfo.GetCamera().GetPosition();

	// return 'not occluded' if camera is inside of box
	IF (vCamPos.x <= vTopMax.x && vCamPos.x >= vTopMin.x && vCamPos.y <= vTopMax.y && vCamPos.y >= vTopMin.y, false)
	{
		pOcclTestVars->nTerrainOccLastFrame = 0;
		return false;
	}

	// when camera is underground - number of steps allowed to skip
	int nMaxTestsToScip = (GetVisAreaManager()->m_pCurPortal) ? 3 : 10000;

	CVars* const __restrict pCVars = GetCVars();

	// density of tests in meters for this object
	float fMaxStep = fDistance * GetFloatCVar(e_TerrainOcclusionCullingPrecision);

	// if object was visible last time - try last known visible point first
	if (!pOcclTestVars->vLastVisPoint.IsZero())
	{
		Vec3 vTmp(0, 0, 0);
		if (!Intersect(vCamPos, pOcclTestVars->vLastVisPoint, fDistance, nMaxTestsToScip, vTmp))
		{
			pOcclTestVars->nTerrainOccLastFrame = 0;
			return false;
		}
		else
			pOcclTestVars->vLastVisPoint.zero();
	}

	// do many rays for near objects and only one for far/small objects
	if ((fMaxStep < (vTopMax.x - vTopMin.x) * GetFloatCVar(e_TerrainOcclusionCullingPrecisionDistRatio) ||
	     fMaxStep < (vTopMax.y - vTopMin.y) * GetFloatCVar(e_TerrainOcclusionCullingPrecisionDistRatio)) &&
	    objBox.min.x != objBox.max.x && objBox.min.y != objBox.max.y)
	{
		// split fox top surface into round number of parts
		float dx = (vTopMax.x - vTopMin.x);
		while (dx > fMaxStep)
			dx *= 0.5f;
		float dy = (vTopMax.y - vTopMin.y);
		while (dy > fMaxStep)
			dy *= 0.5f;

		// avoid dead loops
		dy = max(dy, 0.001f);
		dx = max(dx, 0.001f);

		bool bCameraAbove = vCamPos.z > vTopMax.z;

		if (bCameraAbove && !bTerrainNode)
		{
			for (float y = vTopMin.y; y <= vTopMax.y; y += dy)
				for (float x = vTopMin.x; x <= vTopMax.x; x += dx)
					if (!Intersect(vCamPos, Vec3(x, y, vTopMax.z), fDistance, nMaxTestsToScip, pOcclTestVars->vLastVisPoint))
					{
						pOcclTestVars->nTerrainOccLastFrame = 0;
						return false;
					}
		}
		else
		{
			// test only needed edges, note: there are duplicated checks on the corners

			if ((vCamPos.x > vTopMin.x) == bCameraAbove) // test min x side
				for (float y = vTopMin.y; y <= vTopMax.y; y += dy)
					if (!Intersect(vCamPos, Vec3(vTopMin.x, y, vTopMax.z), fDistance, nMaxTestsToScip, pOcclTestVars->vLastVisPoint))
					{
						pOcclTestVars->nTerrainOccLastFrame = 0;
						return false;
					}

			if ((vCamPos.x < vTopMax.x) == bCameraAbove) // test max x side
				for (float y = vTopMax.y; y >= vTopMin.y; y -= dy)
					if (!Intersect(vCamPos, Vec3(vTopMax.x, y, vTopMax.z), fDistance, nMaxTestsToScip, pOcclTestVars->vLastVisPoint))
					{
						pOcclTestVars->nTerrainOccLastFrame = 0;
						return false;
					}

			if ((vCamPos.y > vTopMin.y) == bCameraAbove) // test min y side
				for (float x = vTopMax.x; x >= vTopMin.x; x -= dx)
					if (!Intersect(vCamPos, Vec3(x, vTopMin.y, vTopMax.z), fDistance, nMaxTestsToScip, pOcclTestVars->vLastVisPoint))
					{
						pOcclTestVars->nTerrainOccLastFrame = 0;
						return false;
					}

			if ((vCamPos.y < vTopMax.y) == bCameraAbove) // test max y side
				for (float x = vTopMin.x; x <= vTopMax.x; x += dx)
					if (!Intersect(vCamPos, Vec3(x, vTopMax.y, vTopMax.z), fDistance, nMaxTestsToScip, pOcclTestVars->vLastVisPoint))
					{
						pOcclTestVars->nTerrainOccLastFrame = 0;
						return false;
					}
		}
		pOcclTestVars->nLastOccludedMainFrameID = passInfo.GetMainFrameID();
		pOcclTestVars->nTerrainOccLastFrame = 1;
		return true;
	}
	else
	{
		// select random point on top surface, once visible point is found - it will be stored in vLastVisPoint and used for future tests
		Vec3 vTopMid(0, 0, vTopMin.z);
		float t;
		t = cry_random(0.0f, 1.0f);
		vTopMid.x = vTopMin.x * t + vTopMax.x * (1.f - t);
		t = cry_random(0.0f, 1.0f);
		vTopMid.y = vTopMin.y * t + vTopMax.y * (1.f - t);

		if (Intersect(vCamPos, vTopMid, fDistance, nMaxTestsToScip, pOcclTestVars->vLastVisPoint))
		{
			pOcclTestVars->nLastOccludedMainFrameID = passInfo.GetMainFrameID();
			pOcclTestVars->nTerrainOccLastFrame = 1;
			return true;
		}
	}
	pOcclTestVars->nTerrainOccLastFrame = 0;
	return false;
}

bool CHeightMap::Intersect(Vec3 vStartPoint, Vec3 vStopPoint, float _fDist, int nMaxTestsToScip, Vec3& vLastVisPoint)
{
	//  FUNCTION_PROFILER_3DENGINE;

	// convert x and y into heightmap space, keep z in world space
	float heightMapSize = (float)CTerrain::GetTerrainSize() * CTerrain::GetHeightMapUnitSizeInverted();
	float invUnitSize = CTerrain::GetInvUnitSize();
	vStopPoint.x *= invUnitSize;
	vStopPoint.y *= invUnitSize;
	vStartPoint.x *= invUnitSize;
	vStartPoint.y *= invUnitSize;

	// clamp start
	if (vStartPoint.x < 0 || vStartPoint.y < 0 || vStartPoint.x >= heightMapSize || vStartPoint.y >= heightMapSize)
	{
		AABB boxHM(Vec3(0, 0, 0), Vec3(heightMapSize, heightMapSize, heightMapSize));
		Lineseg ls(vStartPoint, vStopPoint);
		Vec3 vRes;
		if (Intersect::Lineseg_AABB(ls, boxHM, vRes) == 0x01)
			vStartPoint = vRes;
		else
		{
			vLastVisPoint.Set(vStopPoint.x / invUnitSize, vStopPoint.y / invUnitSize, vStopPoint.z);
			return false;
		}
	}

	// clamp end
	if (vStopPoint.x < 0 || vStopPoint.y < 0 || vStopPoint.x >= heightMapSize || vStopPoint.y >= heightMapSize)
	{
		AABB boxHM(Vec3(0, 0, 0), Vec3(heightMapSize, heightMapSize, heightMapSize));
		Lineseg ls(vStopPoint, vStartPoint);
		Vec3 vRes;
		if (Intersect::Lineseg_AABB(ls, boxHM, vRes) == 0x01)
			vStopPoint = vRes;
		else
		{
			vLastVisPoint.Set(vStopPoint.x / invUnitSize, vStopPoint.y / invUnitSize, vStopPoint.z);
			return false;
		}
	}

	float unitSizeRatio = 2.f * CTerrain::GetInvUnitSize();

	CVars* const __restrict pCVars = GetCVars();

	float initStepSize = pCVars->e_TerrainOcclusionCullingStepSize * unitSizeRatio;

	float stepSize = initStepSize;
	Vec3 vDir = (vStopPoint - vStartPoint);
	float fFullDist = vDir.GetLength();
	vDir.Normalize();
	float fPos = 0;

	float fMaxUndegroundDist = min(fFullDist, (float)nMaxTestsToScip * initStepSize);

	for (; fPos < fMaxUndegroundDist; fPos += stepSize)
	{
		Vec3 vPos = vStartPoint + vDir * fPos;
		if (!IsPointUnderGround(fastround_positive(vPos.x), fastround_positive(vPos.y), vPos.z))
			break;
	}

	float fMaxEndUnitsToSkip = min((float)nMaxTestsToScip, 4.f * initStepSize);

	fFullDist -= fMaxEndUnitsToSkip;

	if (fFullDist > pCVars->e_TerrainOcclusionCullingMaxDist * unitSizeRatio)
		fFullDist = pCVars->e_TerrainOcclusionCullingMaxDist * unitSizeRatio;

	for (; fPos < fFullDist; fPos += stepSize)
	{
		Vec3 vPos = vStartPoint + vDir * fPos;
		if (IsPointUnderGround(fastround_positive(vPos.x), fastround_positive(vPos.y), vPos.z))
		{
			vLastVisPoint.Set(0, 0, 0);
			return true;
		}

		stepSize *= GetFloatCVar(e_TerrainOcclusionCullingStepSizeDelta);
	}

	vLastVisPoint.Set(vStopPoint.x / invUnitSize, vStopPoint.y / invUnitSize, vStopPoint.z);
	return false;
}
#endif// SUPP_HMAP_OCCL
