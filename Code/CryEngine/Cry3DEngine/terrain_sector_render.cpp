// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   terrain_sector_render.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Create buffer, copy it into var memory, draw
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "terrain.h"
#include "terrain_sector.h"
#include "ObjMan.h"
#include "terrain_water.h"
#include <CryThreading/IJobManager_JobDelegator.h>

DECLARE_JOB("Terrain_BuildIndices", TBuildIndicesJob, CTerrainNode::BuildIndices_Wrapper);
DECLARE_JOB("Terrain_BuildVertices", TBuildVerticesJob, CTerrainNode::BuildVertices_Wrapper);

// special class to wrap the terrain node allocator functions with a vector like interface
template<typename Type>
class CTerrainTempDataStorage
{
public:
	CTerrainTempDataStorage();

	void  Clear();
	void  Free();

	void  PreAllocate(Type* pMemory, size_t nSize);

	int   Count() const       { return m_nCount; }
	Type* GetElements() const { return m_pMemory; }

	void  Add(const Type& obj);

	int   MemorySize() const { return m_nCap * sizeof(Type); }

	void  GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(GetElements(), MemorySize());
	}

private:
	Type* m_pMemory;      // continuous memory block to store objects in main memory
	int   m_nCount;       // number of objects in the container
	int   m_nCap;         // maximum number of objects fitting into the allocated memory
};

template<typename Type>
CTerrainTempDataStorage<Type>::CTerrainTempDataStorage() :
	m_pMemory(NULL),
	m_nCount(0),
	m_nCap(0)
{}

template<typename Type>
void CTerrainTempDataStorage<Type >::Clear()
{
	m_nCount = 0;
}

template<typename Type>
void CTerrainTempDataStorage<Type >::Free()
{
	m_pMemory = NULL;
	m_nCount = 0;
	m_nCap = 0;
}

template<typename Type>
void CTerrainTempDataStorage<Type >::PreAllocate(Type* pMemory, size_t nSize)
{
	m_pMemory = pMemory;
	m_nCap = nSize;
}

template<typename Type>
void CTerrainTempDataStorage<Type >::Add(const Type& obj)
{
	assert(m_nCount < m_nCap);

	// special case for when a reallocation is needed
	IF (m_nCount == m_nCap, false)
	{
		CryFatalError("terrain sector scheduled update error");
		return;
	}

	// add object
	Type* pMemory = m_pMemory;
	pMemory[m_nCount] = obj;
	++m_nCount;

}

struct CStripInfo
{
	int begin;
};

struct CStripsInfo
{
	CTerrainTempDataStorage<vtx_idx> idx_array;
	int                              nNonBorderIndicesCount;

	inline void Clear() { idx_array.Clear(); nNonBorderIndicesCount = 0; }

	inline void AddIndex(int _x, int _y, int _step, int nSectorSize)
	{
		vtx_idx id = vtx_idx(_x / _step * (nSectorSize / _step + 1) + _y / _step);
		idx_array.Add(id);
	}

	static vtx_idx GetIndex(float _x, float _y, float _step, float nSectorSize)
	{
		vtx_idx id = vtx_idx(_x / _step * (nSectorSize / _step + 1) + _y / _step);
		return id;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(idx_array);
	}
};

class CUpdateTerrainTempData
{
public:

	JobManager::SJobState m_JobStateBuildIndices;
	JobManager::SJobState m_JobStateBuildVertices;

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_StripsInfo);
		pSizer->AddObject(m_lstTmpVertArray);
	}

	// align objects to 128 byte to provide each with an own cacheline since they are updated in parallel
	// for Update Indices
	CRY_ALIGN(128) CStripsInfo m_StripsInfo;
	// for update vectices
	CRY_ALIGN(128) CTerrainTempDataStorage<SVF_P2S_N4B_C4B_T1F> m_lstTmpVertArray;
};

CTerrainUpdateDispatcher::CTerrainUpdateDispatcher()
{
	m_pHeapStorage = CryGetIMemoryManager()->AllocPages(TempPoolSize);
	m_pHeap = CryGetIMemoryManager()->CreateGeneralMemoryHeap(m_pHeapStorage, TempPoolSize, "Terrain temp pool");
}

CTerrainUpdateDispatcher::~CTerrainUpdateDispatcher()
{
	SyncAllJobs(true, SRenderingPassInfo::CreateGeneralPassRenderingInfo(gEnv->p3DEngine->GetRenderingCamera()));
	if (m_queuedJobs.size() || m_arrRunningJobs.size())
	{
		CryFatalError(
		  "CTerrainUpdateDispatcher::~CTerrainUpdateDispatcher(): instance still has jobs "
		  "queued while being destructed!\n");
	}

	m_pHeap = NULL;
	CryGetIMemoryManager()->FreePages(m_pHeapStorage, TempPoolSize);
}

void CTerrainUpdateDispatcher::QueueJob(CTerrainNode* pNode, const SRenderingPassInfo& passInfo)
{
	if (pNode && !Contains(pNode))
	{
		if (!AddJob(pNode, true, passInfo))
		{
			// if job submission was unsuccessful, queue the terrain job
			m_queuedJobs.Add(pNode);
		}
	}
}

bool CTerrainUpdateDispatcher::AddJob(CTerrainNode* pNode, bool executeAsJob, const SRenderingPassInfo& passInfo)
{
	STerrainNodeLeafData* pLeafData = pNode->GetLeafData();
	if (!pLeafData)
		return true;

	const unsigned alignPad = TARGET_DEFAULT_ALIGN;

	// Preallocate enough temp memory to prevent reallocations
	const int meshDim = int(float(CTerrain::GetSectorSize()) / CTerrain::GetHeightMapUnitSize());

	int nNumVerts = (meshDim + 1 + 2) * (meshDim + 1 + 2);
	int nNumIdx = (meshDim + 2) * (meshDim + 2) * 6;

	if (!pNode->m_nTreeLevel && Get3DEngine()->m_bIntegrateObjectsIntoTerrain)
	{
		nNumIdx = max(nNumIdx, GetCVars()->e_TerrainIntegrateObjectsMaxVertices * 3);
		nNumVerts = max(nNumVerts, GetCVars()->e_TerrainIntegrateObjectsMaxVertices);
	}

	if (nNumVerts == 0 || nNumIdx == 0)
		return true;

	size_t allocSize = sizeof(CUpdateTerrainTempData);
	allocSize += alignPad;

	allocSize += sizeof(vtx_idx) * nNumIdx;
	allocSize += alignPad;

	allocSize += sizeof(SVF_P2S_N4B_C4B_T1F) * nNumVerts;

	uint8* pTempData = (uint8*)m_pHeap->Memalign(__alignof(CUpdateTerrainTempData), allocSize, ""); // /*NOTE: Disabled due aliasing issues in the allocator with SNC compiler */m_TempStorage.Allocate<uint8*>(allocSize, __alignof(CUpdateTerrainTempData));
	if (pTempData)
	{
		CUpdateTerrainTempData* pUpdateTerrainTempData = new(pTempData) CUpdateTerrainTempData();
		pTempData += sizeof(CUpdateTerrainTempData);
		pTempData = (uint8*)((reinterpret_cast<uintptr_t>(pTempData) + alignPad) & ~uintptr_t(alignPad));

		pUpdateTerrainTempData->m_StripsInfo.idx_array.PreAllocate(reinterpret_cast<vtx_idx*>(pTempData), nNumIdx);
		pTempData += sizeof(vtx_idx) * nNumIdx;
		pTempData = (uint8*)((reinterpret_cast<uintptr_t>(pTempData) + alignPad) & ~uintptr_t(alignPad));

		pUpdateTerrainTempData->m_lstTmpVertArray.PreAllocate(reinterpret_cast<SVF_P2S_N4B_C4B_T1F*>(pTempData), nNumVerts);

		pNode->m_pUpdateTerrainTempData = pUpdateTerrainTempData;

		// dont run async in case of editor or if we render into shadowmap
		executeAsJob &= !gEnv->IsEditor();
		executeAsJob &= !passInfo.IsShadowPass();
		executeAsJob &= !Get3DEngine()->m_bIntegrateObjectsIntoTerrain; // TODO: support jobs for this mode as well

		if (executeAsJob)
		{
			TBuildIndicesJob jobIndice(passInfo);
			jobIndice.SetClassInstance(pNode);
			jobIndice.RegisterJobState(&pUpdateTerrainTempData->m_JobStateBuildIndices);
			jobIndice.Run();

			TBuildVerticesJob jobVertices;
			jobVertices.SetClassInstance(pNode);
			jobVertices.RegisterJobState(&pUpdateTerrainTempData->m_JobStateBuildVertices);
			jobVertices.Run();
		}
		else
		{
			pNode->BuildIndices_Wrapper(passInfo);
			pNode->BuildVertices_Wrapper();
		}

		m_arrRunningJobs.Add(pNode);
		return true;
	}

	return false;
}

void CTerrainUpdateDispatcher::SyncAllJobs(bool bForceAll, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	uint32 nNothingQueued = 0;
	do
	{
		bool bQueued = m_queuedJobs.size() ? false : true;
		size_t i = 0, nEnd = m_queuedJobs.size();
		while (i < nEnd)
		{
			CTerrainNode* pNode = m_queuedJobs[i];
			if (AddJob(pNode, false, passInfo))
			{
				bQueued = true;
				m_queuedJobs[i] = m_queuedJobs.Last();
				m_queuedJobs.DeleteLast();
				--nEnd;
				continue;
			}
			++i;
		}

		i = 0;
		nEnd = m_arrRunningJobs.size();
		while (i < nEnd)
		{
			CTerrainNode* pNode = m_arrRunningJobs[i];
			CUpdateTerrainTempData* pTempStorage = pNode->m_pUpdateTerrainTempData;
			assert(pTempStorage);
			PREFAST_ASSUME(pTempStorage);
			if (!pTempStorage->m_JobStateBuildIndices.IsRunning() && !pTempStorage->m_JobStateBuildVertices.IsRunning())
			{
				// Finish the render mesh update
				pNode->RenderSectorUpdate_Finish(passInfo);
				pTempStorage->~CUpdateTerrainTempData();
				m_pHeap->Free(pTempStorage);
				pNode->m_pUpdateTerrainTempData = NULL;

				// Remove from running list
				m_arrRunningJobs[i] = m_arrRunningJobs.Last();
				m_arrRunningJobs.DeleteLast();
				--nEnd;
				continue;
			}
			++i;
		}

		if (m_arrRunningJobs.size() == 0 && !bQueued)
			++nNothingQueued;
		if (!bForceAll && nNothingQueued > 4)
		{
			CryLogAlways("ERROR: not all terrain sector vertex/index update requests could be scheduled");
			break;
		}
	}
	while (m_queuedJobs.size() != 0 || m_arrRunningJobs.size() != 0);
}

void CTerrainUpdateDispatcher::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(m_pHeapStorage, TempPoolSize);
	pSizer->AddObject(m_arrRunningJobs);
	pSizer->AddObject(m_queuedJobs);
}

PodArray<vtx_idx> CTerrainNode::m_arrIndices[SRangeInfo::e_max_surface_types][4];
PodArray<SSurfaceType*> CTerrainNode::m_lstReadyTypes(9);

void CTerrainNode::ResetStaticData()
{
	for (int s = 0; s < SRangeInfo::e_max_surface_types; s++)
	{
		for (int p = 0; p < 4; p++)
		{
			stl::free_container(m_arrIndices[s][p]);
		}
	}
	stl::free_container(m_lstReadyTypes);
}

void CTerrainNode::SetupTexGenParams(SSurfaceType* pSurfaceType, float* pOutParams, uint8 ucProjAxis, bool bOutdoor, float fTexGenScale)
{
	assert(pSurfaceType);

	if (pSurfaceType->fCustomMaxDistance > 0)
	{
		pSurfaceType->fMaxMatDistanceZ = pSurfaceType->fMaxMatDistanceXY = pSurfaceType->fCustomMaxDistance;
	}
	else
	{
		pSurfaceType->fMaxMatDistanceZ = float(GetFloatCVar(e_TerrainDetailMaterialsViewDistZ) * Get3DEngine()->m_fTerrainDetailMaterialsViewDistRatio);
		pSurfaceType->fMaxMatDistanceXY = float(GetFloatCVar(e_TerrainDetailMaterialsViewDistXY) * Get3DEngine()->m_fTerrainDetailMaterialsViewDistRatio);
	}

	// setup projection direction
	if (ucProjAxis == 'X')
	{
		pOutParams[0] = 0;
		pOutParams[1] = pSurfaceType->fScale * fTexGenScale;
		pOutParams[2] = 0;
		pOutParams[3] = pSurfaceType->fMaxMatDistanceXY;

		pOutParams[4] = 0;
		pOutParams[5] = 0;
		pOutParams[6] = -pSurfaceType->fScale * fTexGenScale;
		pOutParams[7] = 0;
	}
	else if (ucProjAxis == 'Y')
	{
		pOutParams[0] = pSurfaceType->fScale * fTexGenScale;
		pOutParams[1] = 0;
		pOutParams[2] = 0;
		pOutParams[3] = pSurfaceType->fMaxMatDistanceXY;

		pOutParams[4] = 0;
		pOutParams[5] = 0;
		pOutParams[6] = -pSurfaceType->fScale * fTexGenScale;
		pOutParams[7] = 0;
	}
	else // Z
	{
		pOutParams[0] = pSurfaceType->fScale * fTexGenScale;
		pOutParams[1] = 0;
		pOutParams[2] = 0;
		pOutParams[3] = pSurfaceType->fMaxMatDistanceZ;

		pOutParams[4] = 0;
		pOutParams[5] = -pSurfaceType->fScale * fTexGenScale;
		pOutParams[6] = 0;
		pOutParams[7] = 0;
	}
}

void CTerrainNode::DrawArray(const SRenderingPassInfo& passInfo)
{
	const auto pTempData = Get3DEngine()->CheckAndCreateRenderNodeTempData(this, passInfo);
	if (!pTempData)
	{
		CRY_ASSERT(false);
		return;
	}

	// Use mesh instancing for distant low-lod sectors and during shadow map generation
	if (passInfo.IsShadowPass() || (m_nTreeLevel >= GetCVars()->e_TerrainMeshInstancingMinLod))
	{
		SetupTexturing(false, passInfo);

		IRenderMesh* pRenderMesh = GetSharedRenderMesh();

		CRenderObject* pTerrainRenderObject = nullptr;

		// Prepare mesh model matrix
		Vec3 vScale = GetBBox().GetSize();
		vScale.z = 1.f;
		const auto objMat = Matrix34::CreateScale(vScale, Vec3{ static_cast<float>(m_nOriginX), static_cast<float>(m_nOriginY), 0.0f });

		CLodValue lodValue(3, 0, 3);
		if (!GetObjManager()->AddOrCreatePersistentRenderObject(pTempData, pTerrainRenderObject, &lodValue, objMat, passInfo))
		{
			pTerrainRenderObject->m_pRenderNode = this;
			pTerrainRenderObject->SetAmbientColor(Get3DEngine()->GetSkyColor(), passInfo);
			pTerrainRenderObject->m_fDistance = m_arrfDistance[passInfo.GetRecursiveLevel()];

			pTerrainRenderObject->m_ObjFlags |= FOB_TRANS_TRANSLATE | FOB_TRANS_SCALE;

			pTerrainRenderObject->m_nTextureID = -(int)m_nTexSet.nSlot0 - 1;
			pTerrainRenderObject->m_data.m_pTerrainSectorTextureInfo = &m_nTexSet;
			pTerrainRenderObject->m_data.m_fMaxViewDistance = min(-0.01f, -GetCVars()->e_TerrainMeshInstancingShadowBias * powf(2.f, (float)m_nTreeLevel));

			pRenderMesh->SetCustomTexID(m_nTexSet.nTex0);

			pRenderMesh->AddRenderElements(GetTerrain()->m_pTerrainEf, pTerrainRenderObject, passInfo, EFSLIST_GENERAL, 1);
		}

		return;
	}

	_smart_ptr<IRenderMesh>& pRenderMesh = GetLeafData()->m_pRenderMesh;

	CRenderObject* pTerrainRenderObject = nullptr;

	// Prepare mesh model matrix
	Vec3 vOrigin = { static_cast<float>(m_nOriginX), static_cast<float>(m_nOriginY), 0.0f };
	const auto objMat = Matrix34::CreateTranslationMat(vOrigin);

	CLodValue lodValue(0, 0, 0);
	if (!GetObjManager()->AddOrCreatePersistentRenderObject(pTempData, pTerrainRenderObject, &lodValue, objMat, passInfo))
	{
		pTerrainRenderObject->m_pRenderNode = nullptr;
		pTerrainRenderObject->SetAmbientColor(Get3DEngine()->GetSkyColor(), passInfo);
		pTerrainRenderObject->m_fDistance = m_arrfDistance[passInfo.GetRecursiveLevel()];

		pTerrainRenderObject->m_ObjFlags |= FOB_TRANS_TRANSLATE;
		if (!passInfo.IsShadowPass() && passInfo.RenderShadows())
			pTerrainRenderObject->m_ObjFlags |= FOB_INSHADOW;

		pTerrainRenderObject->m_nTextureID = -(int)m_nTexSet.nSlot0 - 1;
		pTerrainRenderObject->m_data.m_pTerrainSectorTextureInfo = &m_nTexSet;

		pRenderMesh->SetCustomTexID(m_nTexSet.nTex0);

		pRenderMesh->AddRenderElements(GetTerrain()->m_pTerrainEf, pTerrainRenderObject, passInfo, EFSLIST_GENERAL, 1);
	}

	if (passInfo.RenderTerrainDetailMaterial() && !passInfo.IsShadowPass())
	{
		bool bDrawDetailLayersXYZ[2] =
		{
			false, // XY
			false  // Z
		};

		for (int i = 0; i < m_lstSurfaceTypeInfo.Count(); i++)
		{
			assert(1 || m_lstSurfaceTypeInfo[i].pSurfaceType->HasMaterial() && m_lstSurfaceTypeInfo[i].HasRM());

			if (!m_lstSurfaceTypeInfo[i].pSurfaceType->HasMaterial() || !m_lstSurfaceTypeInfo[i].HasRM())
				continue;

			uint8 szProj[] = "XYZ";
			for (int p = 0; p < 3; p++)
			{
				if (SSurfaceType* pSurf = m_lstSurfaceTypeInfo[i].pSurfaceType)
					if (IMaterial* pMat = pSurf->GetMaterialOfProjection(szProj[p]))
					{
						pSurf->fMaxMatDistanceZ = float(GetFloatCVar(e_TerrainDetailMaterialsViewDistZ) * Get3DEngine()->m_fTerrainDetailMaterialsViewDistRatio);
						pSurf->fMaxMatDistanceXY = float(GetFloatCVar(e_TerrainDetailMaterialsViewDistXY) * Get3DEngine()->m_fTerrainDetailMaterialsViewDistRatio);

						if (m_arrfDistance[passInfo.GetRecursiveLevel()] < pSurf->GetMaxMaterialDistanceOfProjection(szProj[p]))
							if (IRenderMesh* pMesh = m_lstSurfaceTypeInfo[i].arrpRM[p])
							{
								bDrawDetailLayersXYZ[p == 2] = true;
							}
					}
			}
		}

		// One iteration for XY projections and one for Z projection
		for (int nP = 0; nP < 2; nP++)
			if (bDrawDetailLayersXYZ[nP])
			{
				CRenderObject* pDetailObj = nullptr;

				CLodValue detailLodValue(1 + nP, 0, 1 + nP);
				if (GetObjManager()->AddOrCreatePersistentRenderObject(pTempData, pDetailObj, &detailLodValue, objMat, passInfo))
					continue;

				pDetailObj->m_pRenderNode = nullptr;
				pDetailObj->SetAmbientColor(Get3DEngine()->GetSkyColor(), passInfo);
				pDetailObj->m_fDistance = m_arrfDistance[passInfo.GetRecursiveLevel()];

				pDetailObj->m_ObjFlags |= passInfo.IsGeneralPass() ? FOB_NO_FOG : 0; // enable fog on recursive rendering (per-vertex)
				pDetailObj->m_ObjFlags |= FOB_TRANS_TRANSLATE | FOB_TERRAIN_LAYER;
				if (passInfo.RenderShadows())
					pDetailObj->m_ObjFlags |= FOB_INSHADOW;

				pDetailObj->m_nTextureID = -(int)m_nTexSet.nSlot0 - 1;
				pDetailObj->m_data.m_pTerrainSectorTextureInfo = &m_nTexSet;

				for (int i = 0; i < m_lstSurfaceTypeInfo.Count(); i++)
				{
					if (!m_lstSurfaceTypeInfo[i].pSurfaceType->HasMaterial() || !m_lstSurfaceTypeInfo[i].HasRM())
						continue;

					uint8 szProj[] = "XYZ";
					for (int p = 0; p < 3; p++)
					{
						if ((p == 2) != (nP))
							continue;

						if (SSurfaceType* pSurf = m_lstSurfaceTypeInfo[i].pSurfaceType)
						{
							if (IMaterial* pMat = pSurf->GetMaterialOfProjection(szProj[p]))
							{
								pSurf->fMaxMatDistanceZ = float(GetFloatCVar(e_TerrainDetailMaterialsViewDistZ) * Get3DEngine()->m_fTerrainDetailMaterialsViewDistRatio);
								pSurf->fMaxMatDistanceXY = float(GetFloatCVar(e_TerrainDetailMaterialsViewDistXY) * Get3DEngine()->m_fTerrainDetailMaterialsViewDistRatio);

								//if(m_arrfDistance[passInfo.GetRecursiveLevel()]<pSurf->GetMaxMaterialDistanceOfProjection(szProj[p]))
								if (IRenderMesh* pMesh = m_lstSurfaceTypeInfo[i].arrpRM[p])
								{
									AABB aabb;
									pMesh->GetBBox(aabb.min, aabb.max);
									//		if(passInfo.GetCamera().IsAABBVisible_F(aabb))
									if (pMesh->GetVertexContainer() == pRenderMesh && pMesh->GetIndicesCount())
									{
										if (GetCVars()->e_TerrainBBoxes == 2)
											GetRenderer()->GetIRenderAuxGeom()->DrawAABB(aabb, false, ColorB(255 * ((m_nTreeLevel & 1) > 0), 255 * ((m_nTreeLevel & 2) > 0), 0, 255), eBBD_Faceted);

										pMesh->SetCustomTexID(m_nTexSet.nTex0);

										// every draw call uses custom constants (at least surface type id and direction of projection) so we have to duplicate render object
										pDetailObj = GetRenderer()->EF_DuplicateRO(pDetailObj, passInfo);

										pMesh->AddRenderElements(pMat, pDetailObj, passInfo, EFSLIST_TERRAINLAYER, 1);
									}
								}
							}
						}
					}
				}
			}
	}
}

// update data in video buffer
void CTerrainNode::UpdateRenderMesh(CStripsInfo* pArrayInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	_smart_ptr<IRenderMesh>& pRenderMesh = GetLeafData()->m_pRenderMesh;

	assert(m_pUpdateTerrainTempData->m_lstTmpVertArray.Count() < 65536);

	static PodArray<SPipTangents> lstTangents;
	lstTangents.Clear();

	if (GetCVars()->e_DefaultMaterial && m_pUpdateTerrainTempData->m_lstTmpVertArray.Count())
	{
		lstTangents.PreAllocate(m_pUpdateTerrainTempData->m_lstTmpVertArray.Count(), m_pUpdateTerrainTempData->m_lstTmpVertArray.Count());

		Vec3 vTang(0, 1, 0);
		Vec3 vBitang(1, 0, 0);
		Vec3 vNormal(0, 0, 1);

		// Orthonormalize Tangent Frame
		vBitang = -vNormal.Cross(vTang);
		vTang = vNormal.Cross(vBitang);

		lstTangents[0] = SPipTangents(vTang, vBitang, -1);
		for (int i = 1; i < lstTangents.Count(); i++)
			lstTangents[i] = lstTangents[0];
	}

	ERenderMeshType eRMType = eRMT_Static;

	bool bMultiGPU;
	gEnv->pRenderer->EF_Query(EFQ_MultiGPUEnabled, bMultiGPU);

	pRenderMesh = GetRenderer()->CreateRenderMeshInitialized(
	  m_pUpdateTerrainTempData->m_lstTmpVertArray.GetElements(), m_pUpdateTerrainTempData->m_lstTmpVertArray.Count(), EDefaultInputLayouts::P2S_N4B_C4B_T1F,
	  pArrayInfo->idx_array.GetElements(), pArrayInfo->idx_array.Count(),
	  prtTriangleList, "TerrainSector", "TerrainSector", eRMType, 1,
	  m_nTexSet.nTex0, NULL, NULL, false, true, lstTangents.Count() ? lstTangents.GetElements() : NULL);
	AABB boxWS = GetBBox();
	pRenderMesh->SetBBox(boxWS.min, boxWS.max);

	pRenderMesh->SetChunk(GetTerrain()->m_pTerrainEf, 0, pRenderMesh->GetVerticesCount(), 0, min(m_pUpdateTerrainTempData->m_StripsInfo.nNonBorderIndicesCount, pArrayInfo->idx_array.Count()), 1.0f, 0);

	if (pRenderMesh->GetChunks().size() && pRenderMesh->GetChunks()[0].pRE)
		pRenderMesh->GetChunks()[0].pRE->m_CustomData = GetLeafData()->m_arrTexGen[0];

	assert(pRenderMesh->GetChunks().size() <= 256); // holes may cause more than 100 chunks
}

void CTerrainNode::UpdateRangeInfoShift()
{
	if (!m_pChilds || !m_nTreeLevel)
		return;

	m_rangeInfo.nUnitBitShift = GetTerrain()->m_nUnitsToSectorBitShift;

	for (int i = 0; i < 4; i++)
	{
		m_pChilds[i].UpdateRangeInfoShift();
		m_rangeInfo.nUnitBitShift = min(m_pChilds[i].m_rangeInfo.nUnitBitShift, m_rangeInfo.nUnitBitShift);
	}
}

void CTerrainNode::ReleaseHeightMapGeometry(bool bRecursive, const AABB* pBox)
{
	if (pBox && !Overlap::AABB_AABB(*pBox, GetBBox()))
		return;

	for (int i = 0; i < m_lstSurfaceTypeInfo.Count(); i++)
		m_lstSurfaceTypeInfo[i].DeleteRenderMeshes(GetRenderer());

	if (GetLeafData() && GetLeafData()->m_pRenderMesh)
	{
		_smart_ptr<IRenderMesh>& pRenderMesh = GetLeafData()->m_pRenderMesh;
		pRenderMesh = NULL;
	}

	delete m_pLeafData;
	m_pLeafData = NULL;

	if (bRecursive && m_pChilds)
		for (int i = 0; i < 4; i++)
			m_pChilds[i].ReleaseHeightMapGeometry(bRecursive, pBox);
}

void CTerrainNode::ResetHeightMapGeometry(bool bRecursive, const AABB* pBox)
{
	if (pBox && !Overlap::AABB_AABB(*pBox, GetBBox()))
		return;

	if (bRecursive && m_pChilds)
		for (int i = 0; i < 4; i++)
			m_pChilds[i].ResetHeightMapGeometry(bRecursive, pBox);
}

// fill vertex buffer
void CTerrainNode::BuildVertices(float stepSize)
{
	FUNCTION_PROFILER_3DENGINE;

	m_pUpdateTerrainTempData->m_lstTmpVertArray.Clear();

	int nSectorSize = CTerrain::GetSectorSize() << m_nTreeLevel;

	float safetyBorder = stepSize;

	// keep often used variables on stack
	const int nOriginX = m_nOriginX;
	const int nOriginY = m_nOriginY;
	const int nTerrainSize = CTerrain::GetTerrainSize();
	CTerrain* pTerrain = GetTerrain();

	for (float x = float(nOriginX - safetyBorder); x <= float(nOriginX + nSectorSize + safetyBorder); x += stepSize)
	{
		for (float y = float(nOriginY - safetyBorder); y <= float(nOriginY + nSectorSize + safetyBorder); y += stepSize)
		{
			float _x = CLAMP(x, nOriginX, nOriginX + nSectorSize);
			float _y = CLAMP(y, nOriginY, nOriginY + nSectorSize);
			float _z = pTerrain->GetZ(_x, _y);

			SVF_P2S_N4B_C4B_T1F vert;

			vert.xy = CryHalf2((float)(_x - nOriginX), (float)(_y - nOriginY));
			vert.z = _z;

			if (_x != x || _y != y)
			{
				vert.z -= stepSize * 0.35f;
			}

			// set terrain surface normal
			SetVertexNormal(x, y, stepSize, pTerrain, nTerrainSize, vert);

			// set terrain surface type
			SetVertexSurfaceType(x, y, stepSize, pTerrain, vert);

			m_pUpdateTerrainTempData->m_lstTmpVertArray.Add(vert);
		}
	}

	m_fBBoxExtentionByObjectsIntegration = 0;

	if (!m_nTreeLevel && Get3DEngine()->m_bIntegrateObjectsIntoTerrain)
	{
		AppendTrianglesFromObjects(nOriginX, nOriginY, pTerrain, stepSize, nTerrainSize);
	}
}

void CTerrainNode::BuildIndices(CStripsInfo& si, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	int sectorSize = CTerrain::GetSectorSize() << m_nTreeLevel;
	const int meshDim = int(float(CTerrain::GetSectorSize()) / CTerrain::GetHeightMapUnitSize());
	float stepSize = float(sectorSize) / float(meshDim);

	float sectorSizeSB = sectorSize + stepSize * 2;

	si.Clear();

	CStripsInfo* pSI = &si;
	CTerrain* pTerrain = GetTerrain();

	float halfStep = stepSize / 2;

	for (float x = 0; x < sectorSizeSB; x += stepSize)
	{
		for (float y = 0; y < sectorSizeSB; y += stepSize)
		{
			if (!m_bHasHoles || !pTerrain->GetHole(m_nOriginX + x - halfStep, m_nOriginY + y - halfStep))
			{
				// convert to units
				int xu = int(x * CTerrain::GetInvUnitSize());
				int yu = int(y * CTerrain::GetInvUnitSize());
				int su = int(stepSize * CTerrain::GetInvUnitSize());
				int ssu = int(sectorSizeSB * CTerrain::GetInvUnitSize());

				if (pTerrain->IsMeshQuadFlipped(m_nOriginX + x - halfStep, m_nOriginY + y - halfStep, stepSize))
				{
					pSI->AddIndex(xu + su, yu, su, ssu);
					pSI->AddIndex(xu + su, yu + su, su, ssu);
					pSI->AddIndex(xu, yu, su, ssu);

					pSI->AddIndex(xu, yu, su, ssu);
					pSI->AddIndex(xu + su, yu + su, su, ssu);
					pSI->AddIndex(xu, yu + su, su, ssu);
				}
				else
				{
					pSI->AddIndex(xu, yu, su, ssu);
					pSI->AddIndex(xu + su, yu, su, ssu);
					pSI->AddIndex(xu, yu + su, su, ssu);

					pSI->AddIndex(xu + su, yu, su, ssu);
					pSI->AddIndex(xu + su, yu + su, su, ssu);
					pSI->AddIndex(xu, yu + su, su, ssu);
				}
			}
		}
	}

	si.nNonBorderIndicesCount = si.idx_array.Count();
}

// entry point
bool CTerrainNode::RenderSector(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	STerrainNodeLeafData* pLeafData = GetLeafData();

	IRenderMesh* pRenderMesh = (m_nTreeLevel >= GetCVars()->e_TerrainMeshInstancingMinLod) ? GetSharedRenderMesh() : GetLeafData()->m_pRenderMesh;

	bool bDetailLayersReady = passInfo.IsShadowPass() ||
	                          !m_lstSurfaceTypeInfo.Count() ||
	                          !passInfo.RenderTerrainDetailMaterial();
	for (int i = 0; i < m_lstSurfaceTypeInfo.Count() && !bDetailLayersReady; ++i)
	{
		bDetailLayersReady =
		  m_lstSurfaceTypeInfo[i].HasRM() ||
		  m_lstSurfaceTypeInfo[i].pSurfaceType ||
		  !m_lstSurfaceTypeInfo[i].pSurfaceType->HasMaterial();
	}

	// just draw if everything is up to date already
	if (!GetLeafData())
		return false;

	if (pRenderMesh && GetCVars()->e_TerrainDrawThisSectorOnly < 2 && bDetailLayersReady)
	{
			DrawArray(passInfo);
			return true;
		}

	if (passInfo.GetRecursiveLevel())
		if (pRenderMesh)
			return true;

	return false;
}

void CTerrainNode::RenderSectorUpdate_Finish(const SRenderingPassInfo& passInfo)
{
	assert(m_pUpdateTerrainTempData != NULL);
	if (passInfo.IsGeneralPass())
	{
		CRY_PROFILE_REGION(PROFILE_3DENGINE, "Sync_UpdateTerrain");
		gEnv->GetJobManager()->WaitForJob(m_pUpdateTerrainTempData->m_JobStateBuildIndices);
		gEnv->GetJobManager()->WaitForJob(m_pUpdateTerrainTempData->m_JobStateBuildVertices);

		if (m_pUpdateTerrainTempData->m_StripsInfo.idx_array.Count() == 0)
			m_pUpdateTerrainTempData->m_lstTmpVertArray.Clear();
	}

	// if an allocation failed, destroy the temp data and return
	if (m_pUpdateTerrainTempData->m_StripsInfo.idx_array.MemorySize() == 0 ||
	    m_pUpdateTerrainTempData->m_lstTmpVertArray.MemorySize() == 0)
		return;

	STerrainNodeLeafData* pLeafData = GetLeafData();
	if (!pLeafData)
		return;

	_smart_ptr<IRenderMesh>& pRenderMesh = pLeafData->m_pRenderMesh;

	InvalidatePermanentRenderObject();

	UpdateRenderMesh(&m_pUpdateTerrainTempData->m_StripsInfo);

	// update detail layers indices
	if (passInfo.RenderTerrainDetailMaterial())
	{
		// build all indices
		GenerateIndicesForAllSurfaces(pRenderMesh, pLeafData->m_arrpNonBorderIdxNum, m_pUpdateTerrainTempData->m_StripsInfo.nNonBorderIndicesCount, 0, m_pUpdateTerrainTempData);

		m_lstReadyTypes.Clear(); // protection from duplications in palette of types

		uint8 szProj[] = "XYZ";
		for (int i = 0; i < m_lstSurfaceTypeInfo.Count(); i++)
		{
			SSurfaceType* pSurfaceType = m_lstSurfaceTypeInfo[i].pSurfaceType;

			if (pSurfaceType && pSurfaceType->ucThisSurfaceTypeId < SRangeInfo::e_undefined)
			{
				if (m_lstReadyTypes.Find(pSurfaceType) < 0)
				{
					bool b3D = pSurfaceType->IsMaterial3D();
					for (int p = 0; p < 3; p++)
					{
						if (b3D || pSurfaceType->GetMaterialOfProjection(szProj[p]))
						{
							int nProjId = b3D ? p : 3;
							PodArray<vtx_idx>& lstIndices = m_arrIndices[pSurfaceType->ucThisSurfaceTypeId][nProjId];

							if (m_lstSurfaceTypeInfo[i].arrpRM[p] && (lstIndices.Count() != m_lstSurfaceTypeInfo[i].arrpRM[p]->GetIndicesCount()))
							{
								m_lstSurfaceTypeInfo[i].arrpRM[p] = NULL;
							}

							if (lstIndices.Count())
								UpdateSurfaceRenderMeshes(pRenderMesh, pSurfaceType, m_lstSurfaceTypeInfo[i].arrpRM[p], p, lstIndices, "TerrainMaterialLayer", pLeafData->m_arrpNonBorderIdxNum[pSurfaceType->ucThisSurfaceTypeId][nProjId], passInfo);

							if (!b3D)
								break;
						}
					}
					m_lstReadyTypes.Add(pSurfaceType);
				}
			}
		}
	}

	DrawArray(passInfo);
}

void CTerrainNode::BuildIndices_Wrapper(SRenderingPassInfo passInfo)
{
	CUpdateTerrainTempData* pUpdateTerrainTempData = m_pUpdateTerrainTempData;

	// don't try to create terrain data if an allocation failed
	if (pUpdateTerrainTempData->m_StripsInfo.idx_array.MemorySize() == 0) return;

	STerrainNodeLeafData* pLeafData = GetLeafData();
	BuildIndices(pUpdateTerrainTempData->m_StripsInfo, passInfo);
}

void CTerrainNode::BuildVertices_Wrapper()
{
	// don't try to create terrain data if an allocation failed
	if (m_pUpdateTerrainTempData->m_lstTmpVertArray.MemorySize() == 0)
		return;

	STerrainNodeLeafData* pLeafData = GetLeafData();
	if (!pLeafData)
		return;

	_smart_ptr<IRenderMesh>& pRenderMesh = pLeafData->m_pRenderMesh;

	int sectorSize = CTerrain::GetSectorSize() << m_nTreeLevel;
	const int meshDim = int(float(CTerrain::GetSectorSize()) / CTerrain::GetHeightMapUnitSize());
	float stepSize = float(sectorSize) / float(meshDim);

	BuildVertices(stepSize);
}

int GetVecProjectId(const Vec3& vNorm)
{
	Vec3 vNormAbs = vNorm.abs();

	int nOpenId = 0;

	if (vNormAbs.x >= vNormAbs.y && vNormAbs.x >= vNormAbs.z)
		nOpenId = 0;
	else if (vNormAbs.y >= vNormAbs.x && vNormAbs.y >= vNormAbs.z)
		nOpenId = 1;
	else
		nOpenId = 2;

	return nOpenId;
}

void CTerrainNode::GenerateIndicesForAllSurfaces(IRenderMesh* pRM, int arrpNonBorderIdxNum[SRangeInfo::e_max_surface_types][4], int nBorderStartIndex, SSurfaceTypeInfo* pSurfaceTypeInfos, CUpdateTerrainTempData* pUpdateTerrainTempData)
{
	FUNCTION_PROFILER_3DENGINE;

	byte arrMat3DFlag[SRangeInfo::e_max_surface_types];

	enum { ARR_INDICES_0_BUFFER_SIZE = 8192 * 3, ARR_INDICES_BUFFER_SIZE = 2048 * 3 };

	m_arrIndices[0][3].reserve(ARR_INDICES_0_BUFFER_SIZE);

	for (int s = 0; s < SRangeInfo::e_max_surface_types; s++)
	{
		if (s < 10)
		{
			m_arrIndices[s][3].reserve(ARR_INDICES_BUFFER_SIZE);
		}

		for (int p = 0; p < 4; p++)
		{
			m_arrIndices[s][p].Clear();

			arrpNonBorderIdxNum[s][p] = -1;
		}

		if (pSurfaceTypeInfos)
		{
			if (pSurfaceTypeInfos[s].pSurfaceType)
				arrMat3DFlag[s] = pSurfaceTypeInfos[s].pSurfaceType->IsMaterial3D();
		}
		else
			arrMat3DFlag[s] = GetTerrain()->GetSurfaceTypes()[s].IsMaterial3D();
	}

	int nSrcCount = 0;
	vtx_idx* pSrcInds = NULL;
	byte* pPosPtr = NULL;
	int nColorStride = 0;
	byte* pColor = NULL;
	int nNormStride = 0;
	byte* pNormB = NULL;

	bool useMesh = false;

	// when possible, use the already on ppu computed data instead of going through the rendermesh
	if (pUpdateTerrainTempData)
	{
		nSrcCount = pUpdateTerrainTempData->m_StripsInfo.idx_array.Count();
		if (!nSrcCount)
			return;
		pSrcInds = pUpdateTerrainTempData->m_StripsInfo.idx_array.GetElements();
		assert(pSrcInds);
		if (!pSrcInds)
			return;

		pPosPtr = reinterpret_cast<byte*>(pUpdateTerrainTempData->m_lstTmpVertArray.GetElements());
		uint32 nColorOffset = offsetof(SVF_P2S_N4B_C4B_T1F, color.dcolor);
		nColorStride = sizeof(SVF_P2S_N4B_C4B_T1F);
		pColor = pPosPtr + nColorOffset;

		nNormStride = sizeof(SVF_P2S_N4B_C4B_T1F);
		uint32 nNormOffset = offsetof(SVF_P2S_N4B_C4B_T1F, normal);
		pNormB = pPosPtr + nNormOffset;
	}
	else
	{
		pRM->LockForThreadAccess();

		// we don't have all data we need, get it from the rendermesh
		nSrcCount = pRM->GetIndicesCount();
		if (!nSrcCount)
			return;
		pSrcInds = pRM->GetIndexPtr(FSL_READ);
		assert(pSrcInds);
		if (!pSrcInds)
			return;

		pColor = pRM->GetColorPtr(nColorStride, FSL_READ);
		pNormB = pRM->GetNormPtr(nNormStride, FSL_READ);

		if (!pColor || !pNormB)
		{
			pRM->UnlockStream(VSF_GENERAL);
			pRM->UnLockForThreadAccess();
			return;
		}
		else assert(0);

		useMesh = true;
	}

	//	for(int i=0; pColor && i<pRM->GetChunks()->Count(); i++)
	{
		//	CRenderChunk * pChunk = pRM->GetChunks()->Get(i);

		int nVertCount = pRM->GetVerticesCount();

		for (int j = 0; j < nSrcCount; j += 3)
		{
			vtx_idx arrTriangle[3] = { pSrcInds[j + 0], pSrcInds[j + 1], pSrcInds[j + 2] };

			// ignore invalid triangles
			if (arrTriangle[0] == arrTriangle[1] || arrTriangle[1] == arrTriangle[2] || arrTriangle[2] == arrTriangle[0])
				continue;

			assert(arrTriangle[0] < (unsigned)nVertCount && arrTriangle[1] < (unsigned)nVertCount && arrTriangle[2] < (unsigned)nVertCount);

			if (arrTriangle[0] >= (unsigned)nVertCount || arrTriangle[1] >= (unsigned)nVertCount || arrTriangle[2] >= (unsigned)nVertCount)
				continue;

			// skip safety border triangles
			CryHalf2 xy0 = ((SVF_P2S_N4B_C4B_T1F*)pPosPtr)[arrTriangle[0]].xy;
			CryHalf2 xy1 = ((SVF_P2S_N4B_C4B_T1F*)pPosPtr)[arrTriangle[1]].xy;
			CryHalf2 xy2 = ((SVF_P2S_N4B_C4B_T1F*)pPosPtr)[arrTriangle[2]].xy;
			if (xy0 == xy1 || xy0 == xy2 || xy1 == xy2)
			{
				continue;
			}

			UCol arrColor[3];
			arrColor[0] = *(UCol*)&pColor[arrTriangle[0] * nColorStride];
			arrColor[1] = *(UCol*)&pColor[arrTriangle[1] * nColorStride];
			arrColor[2] = *(UCol*)&pColor[arrTriangle[2] * nColorStride];

			// analyze triangle and collect used surface types and projection directions

			struct SSurfTypeProjInfo
			{
				bool operator==(const SSurfTypeProjInfo& o) const { return surfType == o.surfType; }
				int  surfType = 0;
				byte projDir[4] = { 0 };
			};

			assert(Cry3DEngineBase::m_nMainThreadId == CryGetCurrentThreadId());
			static PodArray<SSurfTypeProjInfo> lstSurfTypes;
			lstSurfTypes.Clear();

			// iterate through vertices
			for (int v = 0; v < 3; v++)
			{
				// extract weights from vertex
				const int weightBitMask = 15;
				int surfWeights[3] = { 0, (arrColor[v].bcolor[3] & weightBitMask), ((arrColor[v].bcolor[3] >> 4) & weightBitMask) };
				const int maxSummWeight = 15;
				surfWeights[0] = SATURATEB(maxSummWeight - surfWeights[1] - surfWeights[2]);

				// iterate through vertex surface types
				for (int s = 0; s < 3; s++)
				{
					if (surfWeights[s])
					{
						SSurfTypeProjInfo stpi;
						stpi.surfType = arrColor[v].bcolor[s];
						assert(stpi.surfType >= 0 && stpi.surfType <= SRangeInfo::e_hole);

						if (stpi.surfType < SRangeInfo::e_hole)
						{
							SSurfTypeProjInfo* pType = 0;

							// find or add new item
							int foundId = lstSurfTypes.Find(stpi);
							if (foundId < 0)
							{
								lstSurfTypes.Add(stpi);
								pType = &lstSurfTypes.Last();
							}
							else
							{
								pType = &lstSurfTypes[foundId];
							}

							// check if surface type material is 3D
							if (arrMat3DFlag[stpi.surfType])
							{
								byte* pNorm = &pNormB[arrTriangle[v] * nNormStride];
								Vec3 vNorm(((float)pNorm[0] - 127.5f), ((float)pNorm[1] - 127.5f), ((float)pNorm[2] - 127.5f));

								// register projection direction
								int p = GetVecProjectId(vNorm);
								assert(p >= 0 && p < 3);
								pType->projDir[p] = true;
							}
							else
							{
								// slot 3 used for non 3d materials
								pType->projDir[3] = true;
							}
						}
					}
				}
			}

			for (int s = 0; s < lstSurfTypes.Count(); s++)
			{
				SSurfTypeProjInfo& rType = lstSurfTypes[s];

				if (rType.surfType == SRangeInfo::e_undefined)
				{
					continue;
				}

				for (int p = 0; p < 4; p++)
				{
					if (rType.projDir[p])
					{
						PodArray<vtx_idx>& rList = m_arrIndices[rType.surfType][p];

						if (j >= nBorderStartIndex && arrpNonBorderIdxNum[rType.surfType][p] < 0)
						{
							arrpNonBorderIdxNum[rType.surfType][p] = rList.Count();
						}

						rList.AddList(arrTriangle, 3);
					}
				}
			}
		}
	}

	if (useMesh)
	{
		pRM->UnlockStream(VSF_GENERAL);
		pRM->UnLockForThreadAccess();
	}
}

void CTerrainNode::UpdateSurfaceRenderMeshes(const _smart_ptr<IRenderMesh> pSrcRM, SSurfaceType* pSurface, _smart_ptr<IRenderMesh>& pMatRM, int nProjectionId,
                                             PodArray<vtx_idx>& lstIndices, const char* szComment, int nNonBorderIndicesCount,
                                             const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	// force new rendermesh if vertex container has changed. Vertex containers aren't thread
	// safe, but seems like maybe something relies on this behaviour since fixing it on RM side
	// causes flickering.
	if (!pMatRM || (pMatRM && pMatRM->GetVertexContainer() != pSrcRM))
	{
		ERenderMeshType eRMType = eRMT_Static;

		bool bMultiGPU;
		gEnv->pRenderer->EF_Query(EFQ_MultiGPUEnabled, bMultiGPU);

		if (bMultiGPU && (gEnv->pRenderer->GetRenderType() != ERenderType::Direct3D12)
		    && (gEnv->pRenderer->GetRenderType() != ERenderType::Vulkan))
			eRMType = eRMT_Dynamic;

		pMatRM = GetRenderer()->CreateRenderMeshInitialized(
		  NULL, 0, EDefaultInputLayouts::P2S_N4B_C4B_T1F, NULL, 0,
		  prtTriangleList, szComment, szComment, eRMType, 1, 0, NULL, NULL, false, false);
	}

	uint8 szProj[] = "XYZ";

	pMatRM->LockForThreadAccess();
	pMatRM->SetVertexContainer(pSrcRM);

	assert(1 || nNonBorderIndicesCount >= 0);

	pMatRM->UpdateIndices(lstIndices.GetElements(), lstIndices.Count(), 0, 0u);
	pMatRM->SetChunk(pSurface->GetMaterialOfProjection(szProj[nProjectionId]), 0, pSrcRM->GetVerticesCount(), 0, lstIndices.Count(), 1.0f);

	assert(nProjectionId >= 0 && nProjectionId < 3);
	float* pParams = pSurface->arrRECustomData[nProjectionId];
	SetupTexGenParams(pSurface, pParams, szProj[nProjectionId], true);

	// set surface type
	if (pSurface->IsMaterial3D())
	{
		pParams[8] = (nProjectionId == 0);
		pParams[9] = (nProjectionId == 1);
		pParams[10] = (nProjectionId == 2);
	}
	else
	{
		pParams[8] = 1.f;
		pParams[9] = 1.f;
		pParams[10] = 1.f;
	}

	pParams[11] = pSurface->ucThisSurfaceTypeId;

	pParams[12] = pParams[13] = pParams[14] = pParams[15] = 0; // previously used for texgen offset in order to avoid too big numbers on GPU

	assert(8 + 8 <= TERRAIN_TEX_OFFSETS_SIZE);

	if (pMatRM->GetChunks().size() && pMatRM->GetChunks()[0].pRE)
	{
		pMatRM->GetChunks()[0].pRE->m_CustomData = pParams;
		pMatRM->GetChunks()[0].pRE->mfUpdateFlags(FCEF_DIRTY);
	}

	Vec3 vMin, vMax;
	pSrcRM->GetBBox(vMin, vMax);
	pMatRM->SetBBox(vMin, vMax);
	pMatRM->UnLockForThreadAccess();
}

STerrainNodeLeafData::~STerrainNodeLeafData()
{
	m_pRenderMesh = NULL;
}

void CTerrainUpdateDispatcher::RemoveJob(CTerrainNode* pNode)
{
	int index = m_arrRunningJobs.Find(pNode);
	if (index >= 0)
	{
		m_arrRunningJobs.Delete(index);
		return;
	}

	index = m_queuedJobs.Find(pNode);
	if (index >= 0)
	{
		m_queuedJobs.Delete(index);
		return;
	}
}

void AddIndexShared(int _x, int _y, PodArray<vtx_idx>& arrIndices, int nSectorSize)
{
	int _step = 1;

	vtx_idx id = _x / _step * (nSectorSize / _step + 1) + _y / _step;

	arrIndices.Add(id);
}

// Build single render mesh (with safety borders) to be re-used for multiple sectors
_smart_ptr<IRenderMesh> CTerrainNode::GetSharedRenderMesh()
{
	const int meshDim = int(float(CTerrain::GetSectorSize()) / CTerrain::GetHeightMapUnitSize());

	if (m_pTerrain->m_pSharedRenderMesh)
		return m_pTerrain->m_pSharedRenderMesh;

	int nBorder = 1;

	SVF_P2S_N4B_C4B_T1F vert;
	ZeroStruct(vert);

	vert.normal.bcolor[0] = 128l;
	vert.normal.bcolor[1] = 128l;
	vert.normal.bcolor[2] = 255l;
	vert.normal.bcolor[3] = 255l;

	vert.color.bcolor[0] = 0l;
	vert.color.bcolor[1] = 255l;
	vert.color.bcolor[2] = 255l;
	vert.color.bcolor[3] = 255l;

	PodArray<SVF_P2S_N4B_C4B_T1F> arrVertices;

	for (int x = -nBorder; x <= (meshDim + nBorder); x++)
	{
		for (int y = -nBorder; y <= (meshDim + nBorder); y++)
		{
			vert.xy = CryHalf2(SATURATE(float(x) / float(meshDim)), SATURATE(float(y) / float(meshDim)));

			if (x < 0 || y < 0 || x > meshDim || y > meshDim)
				vert.z = -0.1f;
			else
				vert.z = 0.f;

			arrVertices.Add(vert);
		}
	}

	PodArray<vtx_idx> arrIndices;

	int meshDimEx = meshDim + nBorder * 2;

	for (int x = 0; x < meshDimEx; x++)
	{
		for (int y = 0; y < meshDimEx; y++)
		{
			AddIndexShared(x + 1, y + 0, arrIndices, meshDimEx);
			AddIndexShared(x + 1, y + 1, arrIndices, meshDimEx);
			AddIndexShared(x + 0, y + 0, arrIndices, meshDimEx);

			AddIndexShared(x + 0, y + 0, arrIndices, meshDimEx);
			AddIndexShared(x + 1, y + 1, arrIndices, meshDimEx);
			AddIndexShared(x + 0, y + 1, arrIndices, meshDimEx);
		}
	}

	m_pTerrain->m_pSharedRenderMesh = GetRenderer()->CreateRenderMeshInitialized(
	  arrVertices.GetElements(),
	  arrVertices.Count(),
	  EDefaultInputLayouts::P2S_N4B_C4B_T1F,
	  arrIndices.GetElements(),
	  arrIndices.Count(),
	  prtTriangleList,
	  "TerrainSectorSharedRenderMesh", "TerrainSectorSharedRenderMesh",
	  eRMT_Static);

	m_pTerrain->m_pSharedRenderMesh->SetChunk(NULL, 0, arrVertices.Count(), 0, arrIndices.Count(), 1.0f, 0);

	return m_pTerrain->m_pSharedRenderMesh;
}

uint32 CTerrainNode::GetMaterialsModificationId()
{
	uint32 nModificationId = 0;

	for (int i = 0; i < m_lstSurfaceTypeInfo.Count(); i++)
	{
		if (!m_lstSurfaceTypeInfo[i].pSurfaceType->HasMaterial() || !m_lstSurfaceTypeInfo[i].HasRM())
			continue;

		uint8 szProj[] = "XYZ";
		for (int p = 0; p < 3; p++)
		{
			if (SSurfaceType* pSurf = m_lstSurfaceTypeInfo[i].pSurfaceType)
				if (IMaterial* pMat = pSurf->GetMaterialOfProjection(szProj[p]))
				{
					if (CMatInfo* pMatInfo = (CMatInfo*)pMat)
						nModificationId += pMatInfo->GetModificationId();
				}
		}
	}

	return nModificationId;
}

// add triangles (from marked objects) intersecting terrain
void CTerrainNode::AppendTrianglesFromObjects(const int nOriginX, const int nOriginY, CTerrain* pTerrain, const float stepSize, const int nTerrainSize)
{
	AABB aabbTNode = GetBBox();
	float fHeightMapMax = aabbTNode.max.z;
	aabbTNode.max.z += GetCVars()->e_TerrainIntegrateObjectsMaxHeight;

	PodArray<IRenderNode*> lstObjects;
	Get3DEngine()->GetObjectsByTypeGlobal(lstObjects, eERType_MovableBrush, &aabbTNode);

	for (int i = 0; i < lstObjects.Count(); i++)
	{
		IRenderNode* pRNode = (IRenderNode*)lstObjects[i];

		if (pRNode->GetGIMode() != IRenderNode::eGM_IntegrateIntoTerrain)
		{
			continue;
		}

		IRenderMesh* pRM = pRNode->GetRenderMesh(0);

		if (pRM)
		{
			pRM->LockForThreadAccess();

			int nPosStride = 0, nTangsStride = 0;
			int nInds = pRM->GetIndicesCount();
			const byte* pPos = pRM->GetPosPtr(nPosStride, FSL_READ);
			vtx_idx* pInds = pRM->GetIndexPtr(FSL_READ);
			byte* pTangs = pRM->GetTangentPtr(nTangsStride, FSL_READ);

			if (pInds && pPos && pTangs)
			{
				Matrix34 m34;
				m34.SetIdentity();
				pRNode->GetEntityStatObj(0, &m34);

				IMaterial* pMat = pRNode->GetMaterial();

				float unitSize2 = (float)CTerrain::GetHeightMapUnitSize() / 2;

				TRenderChunkArray& Chunks = pRM->GetChunks();
				int nChunkCount = Chunks.size();

				const int nHashDim = 16;
				PodArray<vtx_idx> arrVertHash[nHashDim][nHashDim];

				for (int nChunkId = 0; nChunkId < nChunkCount; nChunkId++)
				{
					CRenderChunk* pChunk = &Chunks[nChunkId];
					if (!(pChunk->m_nMatFlags & MTL_FLAG_NODRAW))
					{
						const SShaderItem& shaderItem = pMat->GetShaderItem(pChunk->m_nMatID);

						if (!shaderItem.m_pShader || !shaderItem.m_pShaderResources)
							continue;

						if (shaderItem.m_pShader->GetFlags() & (EF_NODRAW | EF_DECAL))
							continue;

						int lastIndex = pChunk->nFirstIndexId + pChunk->nNumIndices;
						for (int i = pChunk->nFirstIndexId; i < lastIndex; i += 3)
						{
							if (m_pUpdateTerrainTempData->m_lstTmpVertArray.Count() + 3 > m_pUpdateTerrainTempData->m_lstTmpVertArray.MemorySize() / (int)sizeof(SVF_P2S_N4B_C4B_T1F))
							{
								break;
							}

							if (m_pUpdateTerrainTempData->m_StripsInfo.idx_array.Count() + 3 > m_pUpdateTerrainTempData->m_StripsInfo.idx_array.MemorySize() / (int)sizeof(vtx_idx))
							{
								break;
							}

							Vec3 vPosWS[3];
							Vec3 vNormWS[3];
							float fElev[3];

							AABB triBox;
							triBox.Reset();

							float fElevMin = 10000, fElevMax = 0;

							for (int v = 0; v < 3; v++)
							{
								vPosWS[v] = m34.TransformPoint((*(Vec3*)&pPos[nPosStride * pInds[i + v]]));

								SPipTangents& basis = *(SPipTangents*)&pTangs[nTangsStride * pInds[i + v]];

								vNormWS[v] = m34.TransformVector(basis.GetN()).GetNormalized();

								vPosWS[v] += vNormWS[v] * 0.02f;

								triBox.Add(vPosWS[v]);

								fElev[v] = GetTerrain()->GetZApr(vPosWS[v].x, vPosWS[v].y);

								fElevMin = min(fElevMin, fElev[v]);
								fElevMax = max(fElevMax, fElev[v]);
							}

							if (triBox.max.z > fElevMin && triBox.min.z < fElevMax + GetCVars()->e_TerrainIntegrateObjectsMaxHeight && Overlap::AABB_AABB2D(triBox, aabbTNode))
							{
								m_fBBoxExtentionByObjectsIntegration = max(m_fBBoxExtentionByObjectsIntegration, triBox.max.z - fHeightMapMax);

								for (int v = 0; v < 3; v++)
								{
									SVF_P2S_N4B_C4B_T1F vert;

									vert.xy = CryHalf2((float)(vPosWS[v].x - nOriginX), (float)(vPosWS[v].y - nOriginY));
									vert.z = vPosWS[v].z;

									float x = (vPosWS[v].x + unitSize2);
									float y = (vPosWS[v].y + unitSize2);
									int xh = (int)(vPosWS[v].x * 64.f);
									int yh = (int)(vPosWS[v].y * 64.f);

									int nIndex = -1;

									PodArray<vtx_idx>& rHashIndices = arrVertHash[xh & (nHashDim - 1)][yh & (nHashDim - 1)];

									for (int nElem = 0; nElem < rHashIndices.Count(); nElem++)
									{
										vtx_idx nId = rHashIndices[nElem];

										SVF_P2S_N4B_C4B_T1F& vertCached = *(m_pUpdateTerrainTempData->m_lstTmpVertArray.GetElements() + nId);

										if (IsEquivalent(vertCached.xy.x, vert.xy.x) && IsEquivalent(vertCached.xy.y, vert.xy.y) && IsEquivalent(vertCached.z, vert.z, 0.1f))
										{
											nIndex = nId;
											break;
										}
									}

									if (nIndex < 0)
									{
										// set terrain surface normal
										Vec3 vTerrainNorm;
										SetVertexNormal(x, y, stepSize, pTerrain, nTerrainSize, vert, &vTerrainNorm);

										// use terrain normal near the ground
										float fLerp = SATURATE((vPosWS[v].z - fElev[v]) * 2.f);
										Vec3 vNorm = Vec3::CreateLerp(vTerrainNorm, vNormWS[v], fLerp);
										vert.normal.bcolor[0] = (byte)(vNorm[0] * 127.5f + 128.0f);
										vert.normal.bcolor[1] = (byte)(vNorm[1] * 127.5f + 128.0f);
										vert.normal.bcolor[2] = (byte)(vNorm[2] * 127.5f + 128.0f);
										vert.normal.bcolor[3] = 0;

										// set terrain surface type
										SetVertexSurfaceType(x, y, stepSize, pTerrain, vert);

										nIndex = m_pUpdateTerrainTempData->m_lstTmpVertArray.Count();

										m_pUpdateTerrainTempData->m_lstTmpVertArray.Add(vert);

										arrVertHash[xh & (nHashDim - 1)][yh & (nHashDim - 1)].Add(nIndex);
									}

									m_pUpdateTerrainTempData->m_StripsInfo.idx_array.Add(nIndex);
								}
							}
						}
					}
				}
			}

			pRM->UnLockForThreadAccess();
		}
	}
}

void CTerrainNode::SetVertexNormal(float x, float y, const float iLookupRadius, CTerrain* pTerrain, const int nTerrainSize, SVF_P2S_N4B_C4B_T1F& vert, Vec3* pTerrainNorm /*= nullptr*/)
{
	float sx;
	if ((x + iLookupRadius) < nTerrainSize && x > iLookupRadius)
	{
		sx = pTerrain->GetZ(x + iLookupRadius, y) - pTerrain->GetZ(x - iLookupRadius, y);
	}
	else
	{
		sx = 0;
	}

	float sy;
	if ((y + iLookupRadius) < nTerrainSize && y > iLookupRadius)
	{
		sy = pTerrain->GetZ(x, y + iLookupRadius) - pTerrain->GetZ(x, y - iLookupRadius);
	}
	else
	{
		sy = 0;
	}

	// z component of normal will be used as point brightness ( for burned terrain )
	Vec3 vNorm(-sx, -sy, iLookupRadius * 2.0f);
	vNorm.Normalize();

	if (pTerrainNorm)
	{
		*pTerrainNorm = vNorm;
	}

	vert.normal.bcolor[0] = (byte)(vNorm[0] * 127.5f + 128.0f);
	vert.normal.bcolor[1] = (byte)(vNorm[1] * 127.5f + 128.0f);
	vert.normal.bcolor[2] = (byte)(vNorm[2] * 127.5f + 128.0f);
	vert.normal.bcolor[3] = 255;
	SwapEndian(vert.normal.dcolor, eLittleEndian);
}

void CTerrainNode::SetVertexSurfaceType(float x, float y, float stepSize, CTerrain* pTerrain, SVF_P2S_N4B_C4B_T1F& vert)
{
	SSurfaceTypeItem st = pTerrain->GetSurfaceTypeItem(x, y);

	if (st.GetHole())
	{
		// in case of hole - try to find some valid surface type around
		for (float i = -stepSize; i <= stepSize && (st.GetHole()); i += stepSize)
		{
			for (float j = -stepSize; j <= stepSize && (st.GetHole()); j += stepSize)
			{
				st = pTerrain->GetSurfaceTypeID(x + i, y + j);
			}
		}
	}

	vert.color.bcolor[0] = st.ty[0];
	vert.color.bcolor[1] = st.ty[1];
	vert.color.bcolor[2] = st.ty[2];

	const int weightBitMask = 15;
	vert.color.bcolor[3] = (st.we[1] & weightBitMask) | ((st.we[2] & weightBitMask) << 4);

	SwapEndian(vert.color.dcolor, eLittleEndian);
}

uint16 SRangeInfo::GetLocalSurfaceTypeID(uint16 usGlobalSurfaceTypeID)
{
	if (pSTPalette)
	{
		if (usGlobalSurfaceTypeID == e_undefined)
			return e_index_undefined;
		if (usGlobalSurfaceTypeID == e_hole)
			return e_index_hole;

		// Check if a local entry has already been assigned for this global entry.
		for (uint16 i = 0; i < e_index_undefined; i++)
		{
			if (pSTPalette[i] == usGlobalSurfaceTypeID)
				return i;
		}
		// No local entry has been assigned; look for an entry that is marked as currently unused.
		for (uint16 i = 0; i < e_index_undefined; i++)
		{
			if (pSTPalette[i] == e_undefined)
			{
				pSTPalette[i] = (uchar)usGlobalSurfaceTypeID;
				return i;
			}
		}
		// No local entry is marked as unused; look for one whose local ID does not actually occur in the data.
		int nUsageCounters[e_palette_size];
		memset(nUsageCounters, 0, sizeof(nUsageCounters));
		int nCount = nSize * nSize;

		for (uint16 i = 0; i < nCount; i++)
		{
			SSurfaceTypeLocal si;
			SSurfaceTypeLocal::DecodeFromUint32(pHMData[i].surface, si);

			for (int s = 0; s < SSurfaceTypeLocal::kMaxSurfaceTypesNum; s++)
			{
				if (si.we[s])
				{
					nUsageCounters[si.ty[s] & e_index_hole]++;
				}
			}
		}

		for (uint16 i = 0; i < e_index_undefined; i++)
		{
			if (!nUsageCounters[i])
			{
				pSTPalette[i] = (uchar)usGlobalSurfaceTypeID;
				return i;
			}
		}
		// Could not assign a local ID; mark the problem area with holes. (Should not happen, we have integrity checks.)
		return e_index_undefined;
	}
	else
	{
		// If the sector still has no palette, create one and assign local ID 0.
		pSTPalette = new uchar[e_palette_size];

		for (int i = 0; i <= e_index_hole; pSTPalette[i++] = e_undefined)
			;

		pSTPalette[0] = (uchar)usGlobalSurfaceTypeID;
		pSTPalette[e_index_hole] = e_hole;
		return 0;
	}
}
