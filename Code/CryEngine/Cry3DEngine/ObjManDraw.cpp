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

#include "terrain.h"
#include "StatObj.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "terrain_sector.h"
#include "3dEngine.h"
#include "CCullThread.h"

bool CObjManager::IsBoxOccluded_HeightMap
(
  const AABB& objBox,
  float fDistance,
  EOcclusionObjectType eOcclusionObjectType,
  OcclusionTestClient* pOcclTestVars,
  const SRenderingPassInfo& passInfo
)
{
#ifdef SUPP_HMAP_OCCL
	// test occlusion by heightmap
	FUNCTION_PROFILER_3DENGINE;

	assert(pOcclTestVars);

	const Vec3 vTopMax(objBox.max);
	const Vec3 vTopMin(objBox.min.x, objBox.min.y, vTopMax.z);

	const int cMainID = passInfo.GetMainFrameID();

	//unlikely
	const bool cBoxTooLarge = ((vTopMax.x - vTopMin.x) > 10000) | ((vTopMax.y - vTopMin.y) > 10000);
	IF (cBoxTooLarge, false)
	{
		pOcclTestVars->nLastOccludedMainFrameID = cMainID;
		pOcclTestVars->nTerrainOccLastFrame = 1;
		return true;
	}

	const Vec3& vCamPos = passInfo.GetCamera().GetPosition();

	const bool cCamInsideBox = (vCamPos.x <= vTopMax.x) & (vCamPos.x >= vTopMin.x) & (vCamPos.y <= vTopMax.y) & (vCamPos.y >= vTopMin.y);
	IF (cCamInsideBox, false)
	{
		if ((eOcclusionObjectType != eoot_OCCLUDER && eOcclusionObjectType != eoot_OCCELL_OCCLUDER))
			pOcclTestVars->nLastVisibleMainFrameID = cMainID;
		pOcclTestVars->nTerrainOccLastFrame = 0;
		return false; // camera inside of box
	}

	CVisAreaManager* pVisAreaManager = GetVisAreaManager();
	int nMaxTestsToScip = (pVisAreaManager && pVisAreaManager->m_pCurPortal) ? 3 : 10000;

	// precision in meters for this object
	float fMaxStep = fDistance * GetFloatCVar(e_TerrainOcclusionCullingPrecision);

	CTerrain* const pTerrain = GetTerrain();

	const float cTerrainOccPrecRatio = GetFloatCVar(e_TerrainOcclusionCullingPrecisionDistRatio);
	if ((fMaxStep < (vTopMax.x - vTopMin.x) * cTerrainOccPrecRatio ||
	     fMaxStep < (vTopMax.y - vTopMin.y) * cTerrainOccPrecRatio) &&
	    objBox.min.x != objBox.max.x && objBox.min.y != objBox.max.y)
	{
		float dx = (vTopMax.x - vTopMin.x);
		while (dx > fMaxStep)
			dx *= 0.5f;

		float dy = (vTopMax.y - vTopMin.y);
		while (dy > fMaxStep)
			dy *= 0.5f;

		dy = max(dy, 0.001f);
		dx = max(dx, 0.001f);

		bool bCameraAbove = vCamPos.z > vTopMax.z;

		if (bCameraAbove && eOcclusionObjectType != eoot_TERRAIN_NODE)
		{
			for (float y = vTopMin.y; y <= vTopMax.y; y += dy)
				for (float x = vTopMin.x; x <= vTopMax.x; x += dx)
					if (!pTerrain->IntersectWithHeightMap(vCamPos, Vec3(x, y, vTopMax.z), fDistance, nMaxTestsToScip))
					{
						//handle post setup of call to IsBoxOccluded_HeightMap:
						if ((eOcclusionObjectType != eoot_OCCLUDER && eOcclusionObjectType != eoot_OCCELL_OCCLUDER))
							pOcclTestVars->nLastVisibleMainFrameID = cMainID;
						pOcclTestVars->nTerrainOccLastFrame = 0;
						return false;
					}
		}
		else
		{
			// test only needed edges, note: there are duplicated checks on the corners

			if ((vCamPos.x > vTopMin.x) == bCameraAbove) // test min x side
				for (float y = vTopMin.y; y <= vTopMax.y; y += dy)
					if (!pTerrain->IntersectWithHeightMap(vCamPos, Vec3(vTopMin.x, y, vTopMax.z), fDistance, nMaxTestsToScip))
					{
						//handle post setup of call to IsBoxOccluded_HeightMap:
						if ((eOcclusionObjectType != eoot_OCCLUDER && eOcclusionObjectType != eoot_OCCELL_OCCLUDER))
							pOcclTestVars->nLastVisibleMainFrameID = cMainID;
						pOcclTestVars->nTerrainOccLastFrame = 0;
						return false;
					}

			if ((vCamPos.x < vTopMax.x) == bCameraAbove) // test max x side
				for (float y = vTopMax.y; y >= vTopMin.y; y -= dy)
					if (!pTerrain->IntersectWithHeightMap(vCamPos, Vec3(vTopMax.x, y, vTopMax.z), fDistance, nMaxTestsToScip))
					{
						//handle post setup of call to IsBoxOccluded_HeightMap:
						if ((eOcclusionObjectType != eoot_OCCLUDER && eOcclusionObjectType != eoot_OCCELL_OCCLUDER))
							pOcclTestVars->nLastVisibleMainFrameID = cMainID;
						pOcclTestVars->nTerrainOccLastFrame = 0;
						return false;
					}

			if ((vCamPos.y > vTopMin.y) == bCameraAbove) // test min y side
				for (float x = vTopMax.x; x >= vTopMin.x; x -= dx)
					if (!pTerrain->IntersectWithHeightMap(vCamPos, Vec3(x, vTopMin.y, vTopMax.z), fDistance, nMaxTestsToScip))
					{
						//handle post setup of call to IsBoxOccluded_HeightMap:
						if ((eOcclusionObjectType != eoot_OCCLUDER && eOcclusionObjectType != eoot_OCCELL_OCCLUDER))
							pOcclTestVars->nLastVisibleMainFrameID = cMainID;
						pOcclTestVars->nTerrainOccLastFrame = 0;
						return false;
					}

			if ((vCamPos.y < vTopMax.y) == bCameraAbove) // test max y side
				for (float x = vTopMin.x; x <= vTopMax.x; x += dx)
					if (!pTerrain->IntersectWithHeightMap(vCamPos, Vec3(x, vTopMax.y, vTopMax.z), fDistance, nMaxTestsToScip))
					{
						//handle post setup of call to IsBoxOccluded_HeightMap:
						if ((eOcclusionObjectType != eoot_OCCLUDER && eOcclusionObjectType != eoot_OCCELL_OCCLUDER))
							pOcclTestVars->nLastVisibleMainFrameID = cMainID;
						pOcclTestVars->nTerrainOccLastFrame = 0;
						return false;
					}
		}
		pOcclTestVars->nLastOccludedMainFrameID = cMainID;
		pOcclTestVars->nTerrainOccLastFrame = 1;
		return true;
	}
	else
	{
		Vec3 vTopMid = (vTopMin + vTopMax) * 0.5f;
		if (pTerrain && pTerrain->IntersectWithHeightMap(vCamPos, vTopMid, fDistance, nMaxTestsToScip))
		{
			pOcclTestVars->nLastOccludedMainFrameID = cMainID;
			pOcclTestVars->nTerrainOccLastFrame = 1;
			return true;
		}
	}
	//handle post setup of call to IsBoxOccluded_HeightMap:
	if ((eOcclusionObjectType != eoot_OCCLUDER && eOcclusionObjectType != eoot_OCCELL_OCCLUDER))
		pOcclTestVars->nLastVisibleMainFrameID = cMainID;
	pOcclTestVars->nTerrainOccLastFrame = 0;
#endif
	return false;
}
