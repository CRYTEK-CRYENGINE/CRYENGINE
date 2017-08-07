// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   terrain_init.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: init
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "terrain.h"
#include "terrain_sector.h"
#include "StatObj.h"
#include "ObjMan.h"
#include "PolygonClipContext.h"
#include "terrain_water.h"
//#include "detail_grass.h"

float CTerrain::m_fUnitSize = 2;
float CTerrain::m_fInvUnitSize = 1.0f / 2.0f;
int CTerrain::m_nTerrainSize = 1024;
int CTerrain::m_nSectorSize = 64;
#ifndef SEG_WORLD
int CTerrain::m_nSectorsTableSize = 16;
#endif

void CTerrain::BuildSectorsTree(bool bBuildErrorsTable, int nSID)
{
	m_arrSecInfoPyramid[nSID].PreAllocate(TERRAIN_NODE_TREE_DEPTH, TERRAIN_NODE_TREE_DEPTH);
	int nCount = 0;
	for (int i = 0; i < TERRAIN_NODE_TREE_DEPTH; i++)
	{
		int nSectors = CTerrain::GetSectorsTableSize(nSID) >> i;
		m_arrSecInfoPyramid[nSID][i].Allocate(nSectors);
		nCount += nSectors * nSectors;
	}

	assert(m_pParentNodes[nSID] == 0);

	m_SSurfaceType[nSID].PreAllocate(SRangeInfo::e_max_surface_types, SRangeInfo::e_max_surface_types);

	float fStartTime = GetCurAsyncTimeSec();

	// Log() to use LogPlus() later
	PrintMessage(bBuildErrorsTable ? "Compiling %d terrain nodes (%.1f MB) " : "Constructing %d terrain nodes (%.1f MB) ",
	             nCount, float(sizeof(CTerrainNode) * nCount) / 1024.f / 1024.f);

	if (GetParentNode(nSID))
	{
		if (bBuildErrorsTable)
		{
			assert(0);
			//			GetParentNode(nSID)->UpdateErrors();
		}
	}
	else
	{
		int nNodeSize;
#ifdef SEG_WORLD
		AABB aabb;
		if (!GetSegmentBounds(nSID, aabb))
		{
			assert(0);
			nNodeSize = CTerrain::GetTerrainSize();
		}
		else
			nNodeSize = (int) (aabb.max.x - aabb.min.x);
#else
		nNodeSize = CTerrain::GetTerrainSize();
#endif
		m_pParentNodes.GetAt(nSID) = new CTerrainNode();
		m_pParentNodes.GetAt(nSID)->Init(0, 0, nNodeSize, NULL, bBuildErrorsTable, nSID);
	}

	if (Get3DEngine()->IsSegmentOperationInProgress())   // initialize sector AABBs
	{
		float unitSize = (float) CTerrain::GetHeightMapUnitSize();
		float z = GetWaterLevel();
		for (int iLayer = 0; iLayer < TERRAIN_NODE_TREE_DEPTH; ++iLayer)
		{
			Array2d<struct CTerrainNode*>& sectorLayer = m_arrSecInfoPyramid[nSID][iLayer];
			for (int sy = 0; sy < sectorLayer.GetSize(); ++sy)
			{
				float y1 = ((sy << m_nUnitsToSectorBitShift) << iLayer) * unitSize;
				float y2 = (((sy + 1) << m_nUnitsToSectorBitShift) << iLayer) * unitSize;
				for (int sx = 0; sx < sectorLayer.GetSize(); ++sx)
				{
					float x1 = ((sx << m_nUnitsToSectorBitShift) << iLayer) * unitSize;
					float x2 = (((sx + 1) << m_nUnitsToSectorBitShift) << iLayer) * unitSize;
					CTerrainNode* pTerrainNode = sectorLayer[sx][sy];
					if (!pTerrainNode)
						continue;
					pTerrainNode->m_boxHeigtmapLocal.min.Set(x1, y1, z);
					pTerrainNode->m_boxHeigtmapLocal.max.Set(x2, y2, z);
				}
			}
		}
	}

	if (Get3DEngine()->m_pObjectsTree[nSID])
		Get3DEngine()->m_pObjectsTree[nSID]->UpdateTerrainNodes();

	assert(nCount == GetTerrainNodesAmount(nSID));

#ifndef SW_STRIP_LOADING_MSG
	PrintMessagePlus(" done in %.2f sec", GetCurAsyncTimeSec() - fStartTime);
#endif
}

void CTerrain::ChangeOceanMaterial(IMaterial* pMat)
{
	if (m_pOcean)
	{
		m_pOcean->SetMaterial(pMat);
	}
}

void CTerrain::InitTerrainWater(IMaterial* pTerrainWaterShader, int nWaterBottomTexId)
{
	// make ocean surface
	SAFE_DELETE(m_pOcean);
	if (GetWaterLevel() > 0)
		m_pOcean = new COcean(pTerrainWaterShader);
}
