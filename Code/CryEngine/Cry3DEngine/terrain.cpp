// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   terrain.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: check vis
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
#include "VisAreas.h"
#include "Vegetation.h"
#include "RoadRenderNode.h"

namespace TerrainSectorRenderTempData {
void GetMemoryUsage(ICrySizer* pSizer);
}

void CTerrain::AddVisSector(CTerrainNode* newsec)
{
	assert(newsec->m_cNewGeomMML < m_nUnitsToSectorBitShift);
	m_lstVisSectors.Add(newsec);
}

void CTerrain::CheckVis(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (passInfo.IsGeneralPass())
		m_fDistanceToSectorWithWater = OCEAN_IS_VERY_FAR_AWAY;

	m_lstVisSectors.Clear();

	for (int nSID = 0; nSID < m_pParentNodes.Count(); nSID++)
		if (Get3DEngine()->IsSegmentSafeToUse(nSID) && GetParentNode(nSID))
		{
			// reopen texture file if needed, texture pack may be randomly closed by editor so automatic reopening used
			if (!m_arrBaseTexInfos[nSID].m_nDiffTexIndexTableSize && m_bEditor)
				OpenTerrainTextureFile(m_arrBaseTexInfos[nSID].m_hdrDiffTexHdr, m_arrBaseTexInfos[nSID].m_hdrDiffTexInfo,
				                       m_arrSegmentPaths[nSID] + COMPILED_TERRAIN_TEXTURE_FILE_NAME, m_arrBaseTexInfos[nSID].m_ucpDiffTexTmpBuffer, m_arrBaseTexInfos[nSID].m_nDiffTexIndexTableSize, nSID);

			GetParentNode(nSID)->CheckVis(false, (GetCVars()->e_CoverageBufferTerrain != 0) && (GetCVars()->e_CoverageBuffer != 0), m_arrSegmentOrigns[nSID], passInfo);
		}

	if (passInfo.IsGeneralPass())
	{
		m_bOceanIsVisible = (int)((m_fDistanceToSectorWithWater != OCEAN_IS_VERY_FAR_AWAY) || !m_lstVisSectors.Count());

		if (m_fDistanceToSectorWithWater < 0)
			m_fDistanceToSectorWithWater = 0;

		if (!m_lstVisSectors.Count())
			m_fDistanceToSectorWithWater = 0;

		m_fDistanceToSectorWithWater = max(m_fDistanceToSectorWithWater, (passInfo.GetCamera().GetPosition().z - m_fOceanWaterLevel));
	}
}

int __cdecl CmpTerrainNodesImportance(const void* v1, const void* v2)
{
	CTerrainNode* p1 = *(CTerrainNode**)v1;
	CTerrainNode* p2 = *(CTerrainNode**)v2;

	// process textures in progress first
	bool bInProgress1 = p1->m_eTexStreamingStatus == ecss_InProgress;
	bool bInProgress2 = p2->m_eTexStreamingStatus == ecss_InProgress;
	if (bInProgress1 > bInProgress2)
		return -1;
	else if (bInProgress1 < bInProgress2)
		return 1;

	// process recently requested textures first
	if (p1->m_nNodeTextureLastUsedSec4 > p2->m_nNodeTextureLastUsedSec4)
		return -1;
	else if (p1->m_nNodeTextureLastUsedSec4 < p2->m_nNodeTextureLastUsedSec4)
		return 1;

	// move parents first
	float f1 = (float)p1->m_nTreeLevel;
	float f2 = (float)p2->m_nTreeLevel;
	if (f1 > f2)
		return -1;
	else if (f1 < f2)
		return 1;

	// move closest first
	f1 = (p1->m_arrfDistance[0]);
	f2 = (p2->m_arrfDistance[0]);
	if (f1 > f2)
		return 1;
	else if (f1 < f2)
		return -1;

	return 0;
}

int __cdecl CmpTerrainNodesDistance(const void* v1, const void* v2)
{
	CTerrainNode* p1 = *(CTerrainNode**)v1;
	CTerrainNode* p2 = *(CTerrainNode**)v2;

	if (p1->m_nSetLodFrameId < p2->m_nSetLodFrameId)
		return 1;
	else if (p1->m_nSetLodFrameId > p2->m_nSetLodFrameId)
		return -1;

	float f1 = (p1->m_arrfDistance[0]);
	float f2 = (p2->m_arrfDistance[0]);
	if (f1 > f2)
		return 1;
	else if (f1 < f2)
		return -1;

	return 0;
}

void CTerrain::ActivateNodeTexture(CTerrainNode* pNode, const SRenderingPassInfo& passInfo)
{
	if (pNode->m_nNodeTextureOffset < 0 || passInfo.IsRecursivePass())
		return;

	pNode->m_nNodeTextureLastUsedSec4 = (uint16)(GetCurTimeSec() / 4.f);

	if (m_lstActiveTextureNodes.Find(pNode) < 0)
	{
		if (!pNode->m_nNodeTexSet.nTex1 || !pNode->m_nNodeTexSet.nTex0)
		{
			m_lstActiveTextureNodes.Add(pNode);
		}
	}
}

void CTerrain::ActivateNodeProcObj(CTerrainNode* pNode)
{
	if (m_lstActiveProcObjNodes.Find(pNode) < 0)
	{
		m_lstActiveProcObjNodes.Add(pNode);
	}
}

int CTerrain::GetNotReadyTextureNodesCount()
{
	int nRes = 0;
	for (int i = 0; i < m_lstActiveTextureNodes.Count(); i++)
		if (m_lstActiveTextureNodes[i]->m_eTexStreamingStatus != ecss_Ready)
			nRes++;
	return nRes;
}

void CTerrain::CheckNodesGeomUnload(int nSID, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!Get3DEngine()->m_bShowTerrainSurface || !GetParentNode(nSID))
		return;

	for (int n = 0; n < 32; n++)
	{
		static uint32 nOldSectorsX = ~0, nOldSectorsY = ~0, nTreeLevel = ~0;

		if (nTreeLevel > (uint32) GetParentNode(nSID)->m_nTreeLevel)
			nTreeLevel = GetParentNode(nSID)->m_nTreeLevel;

		uint32 nTableSize = CTerrain::GetSectorsTableSize(nSID) >> nTreeLevel;
		assert(nTableSize);

		// x/y cycle
		nOldSectorsY++;
		if (nOldSectorsY >= nTableSize)
		{
			nOldSectorsY = 0;
			nOldSectorsX++;
		}

		if (nOldSectorsX >= nTableSize)
		{
			nOldSectorsX = 0;
			nOldSectorsY = 0;
			nTreeLevel++;
		}

		if (nTreeLevel > (uint32)GetParentNode(nSID)->m_nTreeLevel)
			nTreeLevel = 0;

		if (CTerrainNode* pNode = m_arrSecInfoPyramid[nSID][nTreeLevel][nOldSectorsX][nOldSectorsY])
			pNode->CheckNodeGeomUnload(passInfo);
	}
}

void CTerrain::UpdateNodesIncrementaly(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

#if defined(FEATURE_SVO_GI)
	// make sure top most node is always ready
	if (GetParentNode(0) && GetCVars()->e_svoTI_Apply)
		ActivateNodeTexture(GetParentNode(0), passInfo);
#endif

	// process textures
	if (m_lstActiveTextureNodes.Count() && m_texCache[0].Update())
	{
		m_texCache[1].Update();
		m_texCache[2].Update();

		for (int i = 0; i < m_lstActiveTextureNodes.Count(); i++)
			m_lstActiveTextureNodes[i]->UpdateDistance(passInfo);

		// sort by importance
		qsort(m_lstActiveTextureNodes.GetElements(), m_lstActiveTextureNodes.Count(),
		      sizeof(m_lstActiveTextureNodes[0]), CmpTerrainNodesImportance);

		// release unimportant textures and make sure at least one texture is free for possible loading
		while (m_lstActiveTextureNodes.Count() > m_texCache[0].GetPoolItemsNum() - 1)
		{
			m_lstActiveTextureNodes.Last()->UnloadNodeTexture(false);
			m_lstActiveTextureNodes.DeleteLast();
		}

		// load a few sectors if needed
		int nMaxNodesToLoad = 6;
		assert(m_texCache[0].m_FreeTextures.Count());
		for (int i = 0; i < m_lstActiveTextureNodes.Count() && nMaxNodesToLoad; i++)
			if (!m_lstActiveTextureNodes[i]->CheckUpdateDiffuseMap())
				if (!Get3DEngine()->IsTerrainSyncLoad())
					--nMaxNodesToLoad;

		if (GetCVars()->e_TerrainTextureStreamingDebug >= 2)
		{
			for (int i = 0; i < m_lstActiveTextureNodes.Count(); i++)
			{
				CTerrainNode* pNode = m_lstActiveTextureNodes[i];
				if (pNode->m_nTreeLevel <= GetCVars()->e_TerrainTextureStreamingDebug - 2)
					switch (pNode->m_eTexStreamingStatus)
					{
					case ecss_NotLoaded:
						DrawBBox(pNode->GetBBoxVirtual(), Col_Red);
						break;
					case ecss_InProgress:
						DrawBBox(pNode->GetBBoxVirtual(), Col_Green);
						break;
					case ecss_Ready:
						DrawBBox(pNode->GetBBoxVirtual(), Col_Blue);
						break;
					}
			}
		}
	}

	// process procedural objects
	if (m_lstActiveProcObjNodes.Count())
	{
		static int paramsCheckSumm = 0;
		if (!CTerrainNode::GetProcObjChunkPool() || paramsCheckSumm != (MAX_PROC_OBJ_CHUNKS_NUM + MAX_PROC_SECTORS_NUM + GetCVars()->e_ProcVegetationMaxObjectsInChunk))
		{
			while (m_lstActiveProcObjNodes.Count() > 0)
			{
				m_lstActiveProcObjNodes.Last()->RemoveProcObjects(false);
				m_lstActiveProcObjNodes.DeleteLast();
			}

			delete CTerrainNode::GetProcObjChunkPool();
			CTerrainNode::SetProcObjChunkPool(new SProcObjChunkPool(MAX_PROC_OBJ_CHUNKS_NUM));

			delete CTerrainNode::GetProcObjPoolMan();
			CTerrainNode::SetProcObjPoolMan(new CProcVegetPoolMan(MAX_PROC_SECTORS_NUM));
			
			paramsCheckSumm = (MAX_PROC_OBJ_CHUNKS_NUM + MAX_PROC_SECTORS_NUM + GetCVars()->e_ProcVegetationMaxObjectsInChunk);
		}

		// make sure distances are correct
		for (int i = 0; i < m_lstActiveProcObjNodes.Count(); i++)
			m_lstActiveProcObjNodes[i]->UpdateDistance(passInfo);

		// sort by distance
		qsort(m_lstActiveProcObjNodes.GetElements(), m_lstActiveProcObjNodes.Count(),
		      sizeof(m_lstActiveProcObjNodes[0]), CmpTerrainNodesDistance);

		// release unimportant sectors
		static int nMaxSectors = MAX_PROC_SECTORS_NUM;
		while (m_lstActiveProcObjNodes.Count() > (GetCVars()->e_ProcVegetation ? nMaxSectors : 0))
		{
			m_lstActiveProcObjNodes.Last()->RemoveProcObjects(false);
			m_lstActiveProcObjNodes.DeleteLast();
		}

		while (1)
		{
			// release even more if we are running out of memory
			while (m_lstActiveProcObjNodes.Count())
			{
				int nAll = 0;
				int nUsed = CTerrainNode::GetProcObjChunkPool()->GetUsedInstancesCount(nAll);
				if (nAll - nUsed > (MAX_PROC_OBJ_CHUNKS_NUM / MAX_PROC_SECTORS_NUM)) // make sure at least X chunks are free and ready to be used in this frame
					break;

				m_lstActiveProcObjNodes.Last()->RemoveProcObjects(false);
				m_lstActiveProcObjNodes.DeleteLast();

				nMaxSectors = min(nMaxSectors, m_lstActiveProcObjNodes.Count());
			}

			// build most important not ready sector
			bool bAllDone = true;
			for (int i = 0; i < m_lstActiveProcObjNodes.Count(); i++)
			{
				if (m_lstActiveProcObjNodes[i]->CheckUpdateProcObjects(passInfo))
				{
					bAllDone = false;
					break;
				}
			}

			if (!Get3DEngine()->IsTerrainSyncLoad() || bAllDone)
				break;
		}

		//    if(m_lstActiveProcObjNodes.Count())
		//    m_lstActiveProcObjNodes[0]->RemoveProcObjects(false);

		IF (GetCVars()->e_ProcVegetation == 2, 0)
			for (int i = 0; i < m_lstActiveProcObjNodes.Count(); i++)
				DrawBBox(m_lstActiveProcObjNodes[i]->GetBBoxVirtual(),
				         m_lstActiveProcObjNodes[i]->IsProcObjectsReady() ? Col_Green : Col_Red);
	}
}

int CTerrain::UpdateOcean(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!m_pOcean || !passInfo.RenderWaterOcean())
		return 0;

	m_pOcean->Update(passInfo);

	return 0;
}

int CTerrain::RenderOcean(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!m_pOcean || !passInfo.RenderWaterOcean())
		return 0;

	m_pOcean->Render(passInfo);

	m_pOcean->SetLastFov(passInfo.GetCamera().GetFov());

	return 0;
}

void CTerrain::ApplyForceToEnvironment(Vec3 vPos, float fRadius, float fAmountOfForce)
{
	if (fRadius <= 0)
		return;

	if (!_finite(vPos.x) || !_finite(vPos.y) || !_finite(vPos.z))
	{
		PrintMessage("Error: 3DEngine::ApplyForceToEnvironment: Undefined position passed to the function");
		return;
	}

	if (fRadius > 50.f)
		fRadius = 50.f;

	if (fAmountOfForce > 1.f)
		fAmountOfForce = 1.f;

	if ((vPos.GetDistance(gEnv->p3DEngine->GetRenderingCamera().GetPosition()) > 50.f + fRadius * 2.f) || // too far
	    vPos.z < (GetZApr(vPos.x, vPos.y, GetDefSID()) - 1.f))                                            // under ground
		return;

	Get3DEngine()->AddForcedWindArea(vPos, fAmountOfForce, fRadius);
}

Vec3 CTerrain::GetTerrainSurfaceNormal_Int(int x, int y, int nSID)
{
	FUNCTION_PROFILER_3DENGINE;

	const int nTerrainSize = CTerrain::GetTerrainSize();
	const int nRange = GetHeightMapUnitSize();

	float sx;
	if ((x + nRange) < nTerrainSize && x >= nRange)
		sx = GetZ(x + nRange, y, nSID) - GetZ(x - nRange, y, nSID);
	else
		sx = 0;

	float sy;
	if ((y + nRange) < nTerrainSize && y >= nRange)
		sy = GetZ(x, y + nRange, nSID) - GetZ(x, y - nRange, nSID);
	else
		sy = 0;

	Vec3 vNorm(-sx, -sy, nRange * 2.0f);

	return vNorm.GetNormalized();
}

void CTerrain::GetTerrainAlignmentMatrix(const Vec3& vPos, const float amount, Matrix33& matrix33)
{
	//All paths from GetTerrainSurfaceNormal() return a normalized vector
	Vec3 vTerrainNormal = GetTerrainSurfaceNormal(vPos, 0.5f);
	vTerrainNormal = LERP(Vec3(0, 0, 1.f), vTerrainNormal, amount);
	vTerrainNormal.Normalize();
	Vec3 vDir = Vec3(-1, 0, 0).Cross(vTerrainNormal);

	matrix33 = matrix33.CreateOrientation(vDir, -vTerrainNormal, 0);
}

void CTerrain::GetMemoryUsage(class ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));

	{
		SIZER_COMPONENT_NAME(pSizer, "NodesTree");
		for (int nSID = 0; nSID < m_pParentNodes.Count(); nSID++)
			if (m_pParentNodes[nSID])
				m_pParentNodes[nSID]->GetMemoryUsage(pSizer);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "SecInfoTable");
		pSizer->AddObject(m_arrSecInfoPyramid);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "ProcVeget");
		pSizer->AddObject(CTerrainNode::GetProcObjPoolMan());
	}

	pSizer->AddObject(m_lstVisSectors);
	pSizer->AddObject(m_lstActiveTextureNodes);
	pSizer->AddObject(m_lstActiveProcObjNodes);
	pSizer->AddObject(m_pOcean);
	pSizer->AddObject(m_arrBaseTexInfos);

	{
		SIZER_COMPONENT_NAME(pSizer, "TerrainSectorRenderTemp Data");
		if (m_pTerrainUpdateDispatcher)
			m_pTerrainUpdateDispatcher->GetMemoryUsage(pSizer);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "StaticIndices");
		for (int i = 0; i < SRangeInfo::e_max_surface_types; ++i)
			for (int j = 0; j < 4; ++j)
				pSizer->AddObject(CTerrainNode::m_arrIndices[i][j]);
	}
}

void CTerrain::GetObjects(PodArray<SRNInfo>* pLstObjects)
{
}

int CTerrain::GetTerrainNodesAmount(int nSID)
{
	//	((N pow l)-1)/(N-1)
#if defined(__GNUC__)
	uint64 amount = (uint64)0xaaaaaaaaaaaaaaaaULL;
#else
	uint64 amount = (uint64)0xaaaaaaaaaaaaaaaa;
#endif
	amount >>= (65 - (GetParentNode(nSID)->m_nTreeLevel + 1) * 2);
	return (int)amount;
}

void CTerrain::GetVisibleSectorsInAABB(PodArray<struct CTerrainNode*>& lstBoxSectors, const AABB& boxBox)
{
	lstBoxSectors.Clear();
	for (int i = 0; i < m_lstVisSectors.Count(); i++)
	{
		CTerrainNode* pNode = m_lstVisSectors[i];
		if (pNode->GetBBox().IsIntersectBox(boxBox))
			lstBoxSectors.Add(pNode);
	}
}

void CTerrain::IntersectWithShadowFrustum(PodArray<IShadowCaster*>* plstResult, ShadowMapFrustum* pFrustum, int nSID, const SRenderingPassInfo& passInfo)
{
#ifdef SEG_WORLD
	if (nSID < 0)
	{
		for (nSID = 0; nSID < m_pParentNodes.Count(); ++nSID)
			IntersectWithShadowFrustum(plstResult, pFrustum, nSID);
		return;
	}
#endif

	if (Get3DEngine()->IsSegmentSafeToUse(nSID) && GetParentNode(nSID))
	{
		float fHalfGSMBoxSize = 0.5f / (pFrustum->fFrustrumSize * Get3DEngine()->m_fGsmRange);

		if (pFrustum->pLightOwner == Get3DEngine()->GetSunEntity())
		{
			// move near plane closer to the light source, this will include all casters located between player and sun, shadow-gen shader is also modified to render casters outside of near plane
			ShadowMapFrustum tmpFrustum = *pFrustum;
			CCamera& cam0 = tmpFrustum.FrustumPlanes[0];
			cam0.SetFrustum(cam0.GetViewSurfaceX(), cam0.GetViewSurfaceZ(), cam0.GetFov(), 1.f, cam0.GetFarPlane());
			CCamera& cam1 = tmpFrustum.FrustumPlanes[1];
			cam1.SetFrustum(cam1.GetViewSurfaceX(), cam1.GetViewSurfaceZ(), cam1.GetFov(), 1.f, cam1.GetFarPlane());
			GetParentNode(nSID)->IntersectWithShadowFrustum(false, plstResult, &tmpFrustum, fHalfGSMBoxSize, passInfo);
		}
		else
		{
			GetParentNode(nSID)->IntersectWithShadowFrustum(false, plstResult, pFrustum, fHalfGSMBoxSize, passInfo);
		}
	}
}

void CTerrain::IntersectWithBox(const AABB& aabbBox, PodArray<CTerrainNode*>* plstResult, int nSID)
{
#ifdef SEG_WORLD
	if (nSID < 0)
	{
		for (nSID = 0; nSID < m_pParentNodes.Count(); ++nSID)
			IntersectWithBox(aabbBox, plstResult, nSID);
		return;
	}
#endif

	if (Get3DEngine()->IsSegmentSafeToUse(nSID) && GetParentNode(nSID))
		GetParentNode(nSID)->IntersectWithBox(aabbBox, plstResult);
}

void CTerrain::MarkAllSectorsAsUncompiled(int nSID)
{
#ifdef SEG_WORLD
	if (nSID < 0)
	{
		for (nSID = 0; nSID < m_pParentNodes.Count(); ++nSID)
			MarkAllSectorsAsUncompiled(nSID);
		return;
	}
#endif

	if (!Get3DEngine()->IsSegmentSafeToUse(nSID))
		return;

	if (GetParentNode(nSID))
		GetParentNode(nSID)->RemoveProcObjects(true);

	if (nSID < Get3DEngine()->m_pObjectsTree.Count() && Get3DEngine()->m_pObjectsTree[nSID])
		if (Get3DEngine()->m_pObjectsTree[nSID])
			Get3DEngine()->m_pObjectsTree[nSID]->MarkAsUncompiled();
}

void CTerrain::SetHeightMapMaxHeight(float fMaxHeight)
{
	for (int nSID = 0; nSID < m_pParentNodes.Count(); nSID++)
		if (Get3DEngine()->IsSegmentSafeToUse(nSID) && m_pParentNodes[nSID])
			InitHeightfieldPhysics(nSID);
}

void SetTerrain(CTerrain& rTerrain);

void CTerrain::SerializeTerrainState(TSerialize ser)
{
	ser.BeginGroup("TerrainState");

	m_StoredModifications.SerializeTerrainState(ser);

	ser.EndGroup();
}

int CTerrain::GetTerrainLightmapTexId(Vec4& vTexGenInfo, int nSID)
{
	assert(nSID >= 0);
	AABB nearWorldBox;
	float fBoxSize = 512;
	nearWorldBox.min = gEnv->p3DEngine->GetRenderingCamera().GetPosition() - Vec3(fBoxSize, fBoxSize, fBoxSize);
	nearWorldBox.max = gEnv->p3DEngine->GetRenderingCamera().GetPosition() + Vec3(fBoxSize, fBoxSize, fBoxSize);

	CTerrainNode* pNode = GetParentNode(nSID)->FindMinNodeContainingBox(nearWorldBox);

	if (pNode)
		pNode = pNode->GetTexuringSourceNode(0, ett_LM);
	else
		pNode = GetParentNode(nSID);

	vTexGenInfo.x = (float)pNode->m_nOriginX;
	vTexGenInfo.y = (float)pNode->m_nOriginY;
	vTexGenInfo.z = (float)(CTerrain::GetSectorSize() << pNode->m_nTreeLevel);
	vTexGenInfo.w = 512.0f;
	return (pNode && pNode->m_nNodeTexSet.nTex1 > 0) ? pNode->m_nNodeTexSet.nTex1 : 0;
}

void CTerrain::GetAtlasTexId(int& nTex0, int& nTex1, int& nTex2, int nSID = 0)
{
	nTex0 = m_texCache[0].m_nPoolTexId; // RGB
	nTex1 = m_texCache[1].m_nPoolTexId; // Normal
	nTex2 = m_texCache[2].m_nPoolTexId; // Height
}

void CTerrain::GetResourceMemoryUsage(ICrySizer* pSizer, const AABB& crstAABB, int nSID)
{
	assert(nSID >= 0);
	CTerrainNode* poTerrainNode = FindMinNodeContainingBox(crstAABB, nSID);

	if (poTerrainNode)
	{
		poTerrainNode->GetResourceMemoryUsage(pSizer, crstAABB);
	}
}

void CTerrain::ReleaseInactiveSegments()
{
	for (int nDel = 0; nDel < m_arrDeletedSegments.Count(); nDel++)
	{
		int nSID = m_arrDeletedSegments[nDel];

		//UpdateSectorMeshes();

		ReleaseHeightmapGeometryAroundSegment(nSID);

		// terrain texture file info
		CloseTerrainTextureFile(nSID);
		SAFE_DELETE_ARRAY(m_arrBaseTexInfos[nSID].m_ucpDiffTexTmpBuffer);
		ZeroStruct(m_arrBaseTexInfos[nSID]);

		// surface types
		for (int i = 0; i < m_SSurfaceType[nSID].Count(); i++)
		{
			m_SSurfaceType[nSID][i].lstnVegetationGroups.Reset();
			m_SSurfaceType[nSID][i].pLayerMat = NULL;
		}

		m_SSurfaceType[nSID].Reset();
		ZeroStruct(m_SSurfaceType[nSID]);

		// terrain nodes tree
		SAFE_DELETE(m_pParentNodes[nSID]);

		// terrain nodes pyramid
		int cnt = m_arrSecInfoPyramid[nSID].Count();
		assert(!cnt || cnt == TERRAIN_NODE_TREE_DEPTH);
		for (int i = 0; i < cnt; i++)
			m_arrSecInfoPyramid[nSID][i].Reset();
		m_arrSecInfoPyramid[nSID].Reset();

		// position in the world
		m_arrSegmentOrigns[nSID].Set(0, 0, 0);
		m_arrSegmentSizeUnits[nSID] = Vec2i(0, 0);

		SAFE_DELETE(Get3DEngine()->m_pObjectsTree[nSID]);

		Get3DEngine()->m_safeToUseSegments[nSID] = 0;

		m_arrLoadStatuses[nSID].Clear();
	}
	m_arrDeletedSegments.Clear();
}

int CTerrain::CreateSegment(Vec3 vSegmentSize, Vec3 vSegmentOrigin, const char* pcPath)
{
	int nSID;

	// try to find existing empty slot
	for (nSID = 0; nSID < m_pParentNodes.Count(); nSID++)
	{
		if (!Get3DEngine()->m_pObjectsTree[nSID])
			break;
	}

	if (nSID >= m_pParentNodes.Count())
	{
		// terrain nodes tree
		m_pParentNodes.PreAllocate(nSID + 1, nSID + 1);

		// terrain nodes pyramid
		m_arrSecInfoPyramid.PreAllocate(nSID + 1, nSID + 1);

		// terrain surface types
		m_SSurfaceType.PreAllocate(nSID + 1, nSID + 1);

		// terrain texture file info
		m_arrBaseTexInfos.PreAllocate(nSID + 1, nSID + 1);

		// position in the world
		m_arrSegmentOrigns.PreAllocate(nSID + 1, nSID + 1);

		//segment size in units
		m_arrSegmentSizeUnits.PreAllocate(nSID + 1, nSID + 1);

		// segment paths
		assert(nSID == m_arrSegmentPaths.Count());
		m_arrSegmentPaths.push_back(string());

		// objects tree
		Get3DEngine()->m_pObjectsTree.PreAllocate(nSID + 1, nSID + 1);

		// safe to use segments array
		Get3DEngine()->m_safeToUseSegments.PreAllocate(nSID + 1, nSID + 1);

		// load streaming statuses
		m_arrLoadStatuses.PreAllocate(nSID + 1, nSID + 1);

		// statobj properties
		GetObjManager()->m_lstStaticTypes.PreAllocate(nSID + 1, nSID + 1);
	}

	assert(!m_pParentNodes[nSID]);

	m_SSurfaceType[nSID].PreAllocate(SRangeInfo::e_max_surface_types, SRangeInfo::e_max_surface_types);

	m_arrSegmentOrigns[nSID] = vSegmentOrigin;

	m_arrSegmentSizeUnits[nSID] = Vec2i(((int) vSegmentSize.x) >> m_nBitShift, ((int) vSegmentSize.y) >> m_nBitShift);

	m_arrSegmentPaths[nSID] = pcPath ? pcPath : "";

	m_arrSecInfoPyramid[nSID] = 0;

	Get3DEngine()->m_safeToUseSegments[nSID] = 1; // safe by default

	assert(!Get3DEngine()->m_pObjectsTree[nSID]);
	Get3DEngine()->m_pObjectsTree[nSID] = COctreeNode::Create(nSID, AABB(vSegmentOrigin, vSegmentOrigin + vSegmentSize), NULL);

	m_arrLoadStatuses[nSID].Clear();

	GetObjManager()->m_lstStaticTypes[nSID].Reset();

	m_arrDeletedSegments.Clear();

	return nSID;
}

bool CTerrain::SetSegmentPath(int nSID, const char* pcPath)
{
	if (nSID < 0 || nSID >= m_arrSegmentPaths.Count())
		return false;
	m_arrSegmentPaths[nSID] = pcPath ? pcPath : "";
	return true;
}

const char* CTerrain::GetSegmentPath(int nSID)
{
	if (nSID < 0 || nSID >= m_arrSegmentPaths.Count())
		return NULL;
	return m_arrSegmentPaths[nSID];
}

bool CTerrain::SetSegmentOrigin(int nSID, Vec3 vSegmentOrigin, bool callOffsetPosition)
{
	if (nSID < 0 || nSID >= m_arrSegmentOrigns.Count())
		return false;

	Vec3 offset = vSegmentOrigin - m_arrSegmentOrigns[nSID];
	m_arrSegmentOrigns[nSID] = vSegmentOrigin;
	if (callOffsetPosition)
	{
		Get3DEngine()->m_pObjectsTree[nSID]->OffsetObjects(offset);
		if (m_pParentNodes[nSID])
			m_pParentNodes[nSID]->OffsetPosition(offset);
	}

	return true;
}

const Vec3& CTerrain::GetSegmentOrigin(int nSID) const
{
	if (nSID < 0 || nSID >= m_arrSegmentOrigns.Count())
	{
		static Vec3 vInvalid(std::numeric_limits<float>::quiet_NaN());
		return vInvalid;
	}
	return m_arrSegmentOrigns[nSID];
}

void CTerrain::ReleaseHeightmapGeometryAroundSegment(int nSID)
{
	AABB box;
	GetSegmentBounds(nSID, box);
	box.Expand(Vec3(1, 1, 1));
	box.min.z = -FLT_MAX;
	box.max.z = +FLT_MAX;
	for (int nSID2 = 0; nSID2 < m_pParentNodes.Count(); ++nSID2)
	{
		if (nSID == nSID2) continue;
		if (m_pParentNodes[nSID2])
			m_pParentNodes[nSID2]->ReleaseHeightMapGeometry(true, &box);
	}
}

void CTerrain::ResetHeightmapGeometryAroundSegment(int nSID)
{
	AABB box;
	GetSegmentBounds(nSID, box);
	box.Expand(Vec3(1, 1, 1));
	box.min.z = -FLT_MAX;
	box.max.z = +FLT_MAX;
	for (int nSID2 = 0; nSID2 < m_pParentNodes.Count(); ++nSID2)
	{
		if (nSID == nSID2) continue;
		if (m_pParentNodes[nSID2])
			m_pParentNodes[nSID2]->ResetHeightMapGeometry(true, &box);
	}
}

bool CTerrain::DeleteSegment(int nSID, bool bDeleteNow)
{
	if (nSID < 0 || nSID >= m_pParentNodes.Count())
		return false;

	ResetHeightmapGeometryAroundSegment(nSID);
	Get3DEngine()->m_safeToUseSegments[nSID] = 0;
	m_arrDeletedSegments.push_back(nSID);

	if (bDeleteNow)
		ReleaseInactiveSegments();

	return true;
}

bool CTerrain::Recompile_Modified_Incrementaly_RoadRenderNodes()
{
	PodArray<CRoadRenderNode*>& rList = Get3DEngine()->m_lstRoadRenderNodesForUpdate;

	while (rList.Count())
	{
		rList[0]->Compile();
		rList.Delete(0);

		if (!gEnv->IsEditor())
			break;
	}

	return rList.Count() > 0;
}

int CTerrain::FindSegment(Vec3 vPt)
{
	if (Get3DEngine()->m_pSegmentsManager)
	{
		int nSID;
		if (Get3DEngine()->m_pSegmentsManager->FindSegment(this, vPt, nSID))
			return nSID;
	}
	for (int nSID = 0; nSID < GetMaxSegmentsCount(); ++nSID)
	{
		AABB aabb;
		if (!GetSegmentBounds(nSID, aabb))
			continue;
		if (!Overlap::Point_AABB2D(vPt, aabb)) //!!! 2D?
			continue;
		return nSID;
	}
	return -1;
}

int CTerrain::FindSegment(int x, int y)
{
	Vec3 v((float) (x << m_nBitShift), (float) (y << m_nBitShift), 0);
	int nSID = FindSegment(v);
	return nSID;
}

int CTerrain::GetMaxSegmentsCount() const
{
	return Get3DEngine()->m_pObjectsTree.Count();
}

bool CTerrain::GetSegmentBounds(int nSID, AABB& bbox)
{
	if (nSID < 0 || nSID >= Get3DEngine()->m_pObjectsTree.Count())
		return false;
	if (!Get3DEngine()->m_pObjectsTree[nSID])
		return false;
	bbox = Get3DEngine()->m_pObjectsTree[nSID]->GetNodeBox();
	return true;
}

int CTerrain::WorldToSegment(Vec3& vPt, int nSID)
{
	if (nSID >= 0)
	{
		AABB aabb;
		if (!GetSegmentBounds(nSID, aabb))
			return -1;
		if (Overlap::Point_AABB2D(vPt + aabb.min, aabb))
			return nSID;
		vPt += aabb.min;
	}
	nSID = FindSegment(vPt);
	if (nSID < 0)
		return nSID;
	AABB aabb;
	if (!GetSegmentBounds(nSID, aabb))
	{
		assert(!"Internal world segmentation error");
		return -1;
	}
	vPt -= aabb.min;
	return nSID;
}

int CTerrain::WorldToSegment(int& x, int& y, int nBitShift, int nSID)
{
	Vec3 v;
	if (nSID >= 0)
	{
		if (x >= 0 && y >= 0 && x < m_arrSegmentSizeUnits[nSID].x && y < m_arrSegmentSizeUnits[nSID].y)
			return nSID;
		v = Vec3(m_arrSegmentOrigns[nSID].x + (x << nBitShift), m_arrSegmentOrigns[nSID].y + (y << nBitShift), 0);
		nSID = -1;
	}
	else
		v = Vec3((float) (x << nBitShift), (float) (y << nBitShift), 0);
	int n = WorldToSegment(v, nSID);
	if (n == nSID || n < 0)
		return n;
	x = ((int) v.x) >> nBitShift;
	y = ((int) v.y) >> nBitShift;
	return n;
}

Vec3 CTerrain::GetSegmentOrigin(int nSID)
{
	if (nSID < 0 || nSID >= (int)m_arrSegmentOrigns.size())
		return Vec3(0, 0, 0);
	else
		return m_arrSegmentOrigns[nSID];
}

void CTerrain::SplitWorldRectToSegments(const Rectf& worldRect, PodArray<TSegmentRect>& segmentRects)
{
	segmentRects.Clear();

	for (int nSID = 0; nSID < GetMaxSegmentsCount(); ++nSID)
	{
		AABB aabb;
		if (!GetSegmentBounds(nSID, aabb))
			continue;

		Rectf commonRect(worldRect);
		commonRect.DoIntersect(Rectf(aabb.min.x, aabb.min.y, aabb.max.x, aabb.max.y));
		if (commonRect.IsEmpty())
			continue;

		TSegmentRect sr;
		sr.localRect.Min.x = commonRect.Min.x - aabb.min.x;
		sr.localRect.Min.y = commonRect.Min.y - aabb.min.y;
		sr.localRect.Max.x = commonRect.Max.x - aabb.min.x;
		sr.localRect.Max.y = commonRect.Max.y - aabb.min.y;
		sr.nSID = nSID;

		segmentRects.Add(sr);
	}
}

Vec3 CTerrain::GetTerrainSurfaceNormal(Vec3 vPos, float fRange, int nSID)
{
	FUNCTION_PROFILER_3DENGINE;

#ifdef SEG_WORLD
	if (nSID < 0)
	{
		nSID = FindSegment((int)vPos.x, (int)vPos.y);
		Vec3 vSegOrigin = GetSegmentOrigin(nSID);
		vPos.x -= vSegOrigin.x;
		vPos.y -= vSegOrigin.y;
	}
#else
	nSID = GetDefSID();
#endif

	fRange += 0.05f;
	Vec3 v1 = Vec3(vPos.x - fRange, vPos.y - fRange, GetZApr(vPos.x - fRange, vPos.y - fRange, nSID));
	Vec3 v2 = Vec3(vPos.x - fRange, vPos.y + fRange, GetZApr(vPos.x - fRange, vPos.y + fRange, nSID));
	Vec3 v3 = Vec3(vPos.x + fRange, vPos.y - fRange, GetZApr(vPos.x + fRange, vPos.y - fRange, nSID));
	Vec3 v4 = Vec3(vPos.x + fRange, vPos.y + fRange, GetZApr(vPos.x + fRange, vPos.y + fRange, nSID));
	return (v3 - v2).Cross(v4 - v1).GetNormalized();
}
