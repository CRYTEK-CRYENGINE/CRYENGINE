// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   terrain_render.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: draw vis sectors
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "terrain.h"
#include "terrain_sector.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "PolygonClipContext.h"

struct CTerrain__Cmp_Sectors
{
	CTerrain__Cmp_Sectors(const SRenderingPassInfo& state) : passInfo(state) {}

	bool __cdecl operator()(CTerrainNode* p1, CTerrainNode* p2)
	{
		int nRecursiveLevel = passInfo.GetRecursiveLevel();

		// if same - give closest sectors higher priority
		if (p1->m_arrfDistance[nRecursiveLevel] > p2->m_arrfDistance[nRecursiveLevel])
			return false;
		else if (p1->m_arrfDistance[nRecursiveLevel] < p2->m_arrfDistance[nRecursiveLevel])
			return true;

		return false;
	}
private:
	const SRenderingPassInfo passInfo;
};

float GetPointToBoxDistance(Vec3 vPos, AABB bbox);

int   CTerrain::GetDetailTextureMaterials(IMaterial* materials[])
{
	int materialNumber = 0;
	//vector<IMaterial*> materials;

	uchar szProj[] = "XYZ";

	for (int s = 0; s < SRangeInfo::e_hole; s++)
	{
		SSurfaceType* pSurf = &m_SSurfaceType[s];

		if (pSurf->HasMaterial())
		{
			for (int p = 0; p < 3; p++)
			{
				if (IMaterial* pMat = pSurf->GetMaterialOfProjection(szProj[p]))
				{
					if (materials)
					{
						materials[materialNumber] = pMat;
					}
					materialNumber++;
				}
			}
		}
	}

	return materialNumber;
}

void CTerrain::DrawVisibleSectors(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!passInfo.RenderTerrain() || !Get3DEngine()->m_bShowTerrainSurface)
		return;

	// sort to get good texture streaming priority
	if (m_lstVisSectors.Count() > 0)
		std::sort(m_lstVisSectors.GetElements(), m_lstVisSectors.GetElements() + m_lstVisSectors.Count(), CTerrain__Cmp_Sectors(passInfo));

	for (CTerrainNode* pNode : m_lstVisSectors)
	{
		if (!pNode->RenderNodeHeightmap(passInfo))
		{
			m_pTerrainUpdateDispatcher->QueueJob(pNode, passInfo);
		}
	}
}

void CTerrain::UpdateSectorMeshes(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	m_pTerrainUpdateDispatcher->SyncAllJobs(false, passInfo);
}
