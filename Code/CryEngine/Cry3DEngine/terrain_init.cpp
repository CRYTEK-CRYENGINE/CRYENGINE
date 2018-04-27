// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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
int CTerrain::m_nSectorsTableSize = 16;

void CTerrain::BuildSectorsTree(bool bBuildErrorsTable)
{
	m_arrSecInfoPyramid.PreAllocate(TERRAIN_NODE_TREE_DEPTH, TERRAIN_NODE_TREE_DEPTH);
	int nCount = 0;
	for (int i = 0; i < TERRAIN_NODE_TREE_DEPTH; i++)
	{
		int nSectors = CTerrain::GetSectorsTableSize() >> i;
		m_arrSecInfoPyramid[i].Allocate(nSectors);
		nCount += nSectors * nSectors;
	}

	assert(m_pParentNode == 0);

	m_SSurfaceType.PreAllocate(SRangeInfo::e_max_surface_types, SRangeInfo::e_max_surface_types);

	float fStartTime = GetCurAsyncTimeSec();

	// Log() to use LogPlus() later
	PrintMessage(bBuildErrorsTable ? "Compiling %d terrain nodes (%.1f MB) " : "Constructing %d terrain nodes (%.1f MB) ",
	             nCount, float(sizeof(CTerrainNode) * nCount) / 1024.f / 1024.f);

	if (GetParentNode())
	{
		if (bBuildErrorsTable)
		{
			assert(0);
			//			GetParentNode()->UpdateErrors();
		}
	}
	else
	{
		int nNodeSize;
		nNodeSize = CTerrain::GetTerrainSize();
		m_pParentNode = new CTerrainNode();
		m_pParentNode->Init(0, 0, nNodeSize, NULL, bBuildErrorsTable);
	}

	{
		float unitSize = (float) CTerrain::GetHeightMapUnitSize();
		float z = GetWaterLevel();
		for (int iLayer = 0; iLayer < TERRAIN_NODE_TREE_DEPTH; ++iLayer)
		{
			Array2d<struct CTerrainNode*>& sectorLayer = m_arrSecInfoPyramid[iLayer];
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

	if (Get3DEngine()->m_pObjectsTree)
		Get3DEngine()->m_pObjectsTree->UpdateTerrainNodes();

	assert(nCount == GetTerrainNodesAmount());

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
