// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

void CTerrain::AddVisSector(CTerrainNode* pNode, uint32 passCullMask)
{
	m_lstVisSectors.Add(STerrainVisItem(pNode, passCullMask));
}

void CTerrain::CheckVis(const SRenderingPassInfo& passInfo, uint32 passCullMask)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!Get3DEngine()->m_bSunShadowsFromTerrain)
		passCullMask &= kPassCullMainMask;

	if (passInfo.IsGeneralPass())
		m_fDistanceToSectorWithWater = OCEAN_IS_VERY_FAR_AWAY;

	ClearVisSectors();

	// reopen texture file if needed, texture pack may be randomly closed by editor so automatic reopening used
	if (!m_arrBaseTexInfos.m_nDiffTexIndexTableSize && m_bEditor)
		OpenTerrainTextureFile(m_arrBaseTexInfos.m_hdrDiffTexHdr, m_arrBaseTexInfos.m_hdrDiffTexInfo,
		                       COMPILED_TERRAIN_TEXTURE_FILE_NAME, m_arrBaseTexInfos.m_ucpDiffTexTmpBuffer, m_arrBaseTexInfos.m_nDiffTexIndexTableSize);

	if (GetParentNode())
	{
		GetParentNode()->CheckVis(false, (GetCVars()->e_CoverageBufferTerrain != 0) && (GetCVars()->e_CoverageBuffer != 0), passInfo, passCullMask);
	}

	if (passInfo.IsGeneralPass())
	{
		m_bOceanIsVisible = (int)((m_fDistanceToSectorWithWater != OCEAN_IS_VERY_FAR_AWAY) || !m_checkVisSectorsCount);

		if (m_fDistanceToSectorWithWater < 0)
			m_fDistanceToSectorWithWater = 0;

		if (!m_checkVisSectorsCount)
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

void CTerrain::CheckNodesGeomUnload(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!Get3DEngine()->m_bShowTerrainSurface || !GetParentNode())
		return;

	for (int n = 0; n < 32; n++)
	{
		static uint32 nOldSectorsX = ~0, nOldSectorsY = ~0, nTreeLevel = ~0;

		if (nTreeLevel > (uint32) GetParentNode()->m_nTreeLevel)
			nTreeLevel = GetParentNode()->m_nTreeLevel;

		uint32 nTableSize = CTerrain::GetSectorsTableSize() >> nTreeLevel;
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

		if (nTreeLevel > (uint32)GetParentNode()->m_nTreeLevel)
			nTreeLevel = 0;

		if (CTerrainNode* pNode = m_arrSecInfoPyramid[nTreeLevel][nOldSectorsX][nOldSectorsY])
			pNode->CheckNodeGeomUnload(passInfo);
	}
}

void CTerrain::UpdateNodesIncrementaly(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

#if defined(FEATURE_SVO_GI)
	// make sure top most node is always ready
	if (GetParentNode() && GetCVars()->e_svoTI_Apply)
		ActivateNodeTexture(GetParentNode(), passInfo);
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
	    vPos.z < (GetZApr(vPos.x, vPos.y) - 1.f))                                                         // under ground
		return;

	Get3DEngine()->AddForcedWindArea(vPos, fAmountOfForce, fRadius);
}

Vec3 CTerrain::GetTerrainSurfaceNormal_Int(float x, float y)
{
	FUNCTION_PROFILER_3DENGINE;

	const int nTerrainSize = CTerrain::GetTerrainSize();
	const float nRange = GetHeightMapUnitSize();

	float sx;
	if ((x + nRange) < nTerrainSize && x >= nRange)
		sx = GetZ(x + nRange, y) - GetZ(x - nRange, y);
	else
		sx = 0;

	float sy;
	if ((y + nRange) < nTerrainSize && y >= nRange)
		sy = GetZ(x, y + nRange) - GetZ(x, y - nRange);
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
		if (m_pParentNode)
			m_pParentNode->GetMemoryUsage(pSizer);
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

int CTerrain::GetTerrainNodesAmount()
{
	//	((N pow l)-1)/(N-1)
#if defined(__GNUC__)
	uint64 amount = (uint64)0xaaaaaaaaaaaaaaaaULL;
#else
	uint64 amount = (uint64)0xaaaaaaaaaaaaaaaa;
#endif
	amount >>= (65 - (GetParentNode()->m_nTreeLevel + 1) * 2);
	return (int)amount;
}

void CTerrain::IntersectWithShadowFrustum(PodArray<IShadowCaster*>* plstResult, ShadowMapFrustum* pFrustum, const SRenderingPassInfo& passInfo)
{
	if (GetParentNode())
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
			GetParentNode()->IntersectWithShadowFrustum(false, plstResult, &tmpFrustum, fHalfGSMBoxSize, passInfo);
		}
		else
		{
			GetParentNode()->IntersectWithShadowFrustum(false, plstResult, pFrustum, fHalfGSMBoxSize, passInfo);
		}
	}
}

void CTerrain::IntersectWithBox(const AABB& aabbBox, PodArray<CTerrainNode*>* plstResult)
{
	if (GetParentNode())
		GetParentNode()->IntersectWithBox(aabbBox, plstResult);
}

void CTerrain::MarkAllSectorsAsUncompiled()
{
	if (GetParentNode())
	{
		GetParentNode()->RemoveProcObjects(true);
	}

	if (Cry3DEngineBase::Get3DEngine()->GetObjectsTree())
	{
		Cry3DEngineBase::Get3DEngine()->GetObjectsTree()->MarkAsUncompiled();
	}
}

void CTerrain::SetHeightMapMaxHeight(float fMaxHeight)
{
	if (m_pParentNode)
		InitHeightfieldPhysics();
}

void CTerrain::SerializeTerrainState(TSerialize ser)
{
	ser.BeginGroup("TerrainState");

	m_StoredModifications.SerializeTerrainState(ser);

	ser.EndGroup();
}

int CTerrain::GetTerrainLightmapTexId(Vec4& vTexGenInfo)
{
	AABB nearWorldBox;
	float fBoxSize = 512;
	nearWorldBox.min = gEnv->p3DEngine->GetRenderingCamera().GetPosition() - Vec3(fBoxSize, fBoxSize, fBoxSize);
	nearWorldBox.max = gEnv->p3DEngine->GetRenderingCamera().GetPosition() + Vec3(fBoxSize, fBoxSize, fBoxSize);

	CTerrainNode* pNode = GetParentNode()->FindMinNodeContainingBox(nearWorldBox);

	if (pNode)
		pNode = pNode->GetTexuringSourceNode(0, ett_LM);
	else
		pNode = GetParentNode();

	vTexGenInfo.x = (float)pNode->m_nOriginX;
	vTexGenInfo.y = (float)pNode->m_nOriginY;
	vTexGenInfo.z = (float)(CTerrain::GetSectorSize() << pNode->m_nTreeLevel);
	vTexGenInfo.w = 512.0f;
	return (pNode && pNode->m_nNodeTexSet.nTex1 > 0) ? pNode->m_nNodeTexSet.nTex1 : 0;
}

void CTerrain::GetAtlasTexId(int& nTex0, int& nTex1, int& nTex2)
{
	nTex0 = m_texCache[0].m_nPoolTexId; // RGB
	nTex1 = m_texCache[1].m_nPoolTexId; // Normal
	nTex2 = m_texCache[2].m_nPoolTexId; // Height
}

void CTerrain::GetResourceMemoryUsage(ICrySizer* pSizer, const AABB& crstAABB)
{
	CTerrainNode* poTerrainNode = FindMinNodeContainingBox(crstAABB);

	if (poTerrainNode)
	{
		poTerrainNode->GetResourceMemoryUsage(pSizer, crstAABB);
	}
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

Vec3 CTerrain::GetTerrainSurfaceNormal(Vec3 vPos, float fRange)
{
	FUNCTION_PROFILER_3DENGINE;

	fRange += 0.05f;
	Vec3 v1 = Vec3(vPos.x - fRange, vPos.y - fRange, GetZApr(vPos.x - fRange, vPos.y - fRange));
	Vec3 v2 = Vec3(vPos.x - fRange, vPos.y + fRange, GetZApr(vPos.x - fRange, vPos.y + fRange));
	Vec3 v3 = Vec3(vPos.x + fRange, vPos.y - fRange, GetZApr(vPos.x + fRange, vPos.y - fRange));
	Vec3 v4 = Vec3(vPos.x + fRange, vPos.y + fRange, GetZApr(vPos.x + fRange, vPos.y + fRange));
	return (v3 - v2).Cross(v4 - v1).GetNormalized();
}
