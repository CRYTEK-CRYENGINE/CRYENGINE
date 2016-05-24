// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

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
		vtx_idx id = _x / _step * (nSectorSize / _step + 1) + _y / _step;
		idx_array.Add(id);
	}

	static vtx_idx GetIndex(int _x, int _y, int _step, int nSectorSize)
	{
		vtx_idx id = _x / _step * (nSectorSize / _step + 1) + _y / _step;
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
	// 1U<<MML_NOT_SET will generate 0!
	if (pNode->m_cNewGeomMML == MML_NOT_SET)
		return true;
	STerrainNodeLeafData* pLeafData = pNode->GetLeafData();
	if (!pLeafData)
		return true;

	const unsigned alignPad = TARGET_DEFAULT_ALIGN;

	// Preallocate enough temp memory to prevent reallocations
	const int nStep = (1 << pNode->m_cNewGeomMML) * CTerrain::GetHeightMapUnitSize();
	const int nSectorSize = CTerrain::GetSectorSize() << pNode->m_nTreeLevel;

	int nNumVerts = (nStep) ? ((nSectorSize / nStep) + 1) * ((nSectorSize / nStep) + 1) : 0;
	int nNumIdx = (nStep) ? (nSectorSize / nStep) * (nSectorSize / nStep) * 6 : 0;

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

		PodArray<CTerrainNode*>& lstNeighbourSectors = pLeafData->m_lstNeighbourSectors;
		PodArray<uint8>& lstNeighbourLods = pLeafData->m_lstNeighbourLods;

#ifdef SEG_WORLD
		lstNeighbourSectors.reserve(128U);
		lstNeighbourLods.PreAllocate(128U, 128U);
#else
		lstNeighbourSectors.reserve(64U);
		lstNeighbourLods.PreAllocate(64U, 64U);
#endif

		// dont run async in case of editor or if we render into shadowmap
		executeAsJob &= !gEnv->IsEditor();
		executeAsJob &= !passInfo.IsShadowPass();

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
	if (!Get3DEngine()->CheckAndCreateRenderNodeTempData(&m_pTempData, this, passInfo))
		return;

	// Use mesh instancing for distant low-lod sectors and during shadow map generation
	if(passInfo.IsShadowPass() || (m_nTreeLevel >= GetCVars()->e_TerrainMeshInstancingMinLod))
	{
		SetupTexturing(false, passInfo);

		IRenderMesh * pRenderMesh = GetSharedRenderMesh();

		CRenderObject* pTerrainRenderObject = 0;

		CLodValue dymmyLod(3, 0, 3);
		if (!GetObjManager()->AddOrCreatePersistentRenderObject(m_pTempData, pTerrainRenderObject, &dymmyLod, passInfo))
		{
			pTerrainRenderObject->m_pRenderNode = 0;
			pTerrainRenderObject->m_II.m_AmbColor = Get3DEngine()->GetSkyColor();
			pTerrainRenderObject->m_fDistance = m_arrfDistance[passInfo.GetRecursiveLevel()];

			Vec3 vScale = GetBBox().GetSize();
			vScale.z = 1.f;
			pTerrainRenderObject->m_II.m_Matrix.SetIdentity();
			pTerrainRenderObject->m_II.m_Matrix.SetScale(vScale, Vec3(m_nOriginX, m_nOriginY, 0));
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

	CRenderObject* pTerrainRenderObject = 0;

	CLodValue dymmyLod(0, 0, 0);
	if (!GetObjManager()->AddOrCreatePersistentRenderObject(m_pTempData, pTerrainRenderObject, &dymmyLod, passInfo))
	{
		pTerrainRenderObject->m_pRenderNode = 0;
		pTerrainRenderObject->m_II.m_AmbColor = Get3DEngine()->GetSkyColor();
		pTerrainRenderObject->m_fDistance = m_arrfDistance[passInfo.GetRecursiveLevel()];

		pTerrainRenderObject->m_II.m_Matrix.SetIdentity();
		Vec3 vOrigin(m_nOriginX, m_nOriginY, 0);
		vOrigin += GetTerrain()->m_arrSegmentOrigns[m_nSID];
		pTerrainRenderObject->m_II.m_Matrix.SetTranslation(vOrigin);
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

		for (int nP = 0; nP < 2; nP++)
			if (bDrawDetailLayersXYZ[nP])
			{
				CRenderObject* pDetailObj;

				CLodValue dymmyLod(1 + nP, 0, 1 + nP);
				if (GetObjManager()->AddOrCreatePersistentRenderObject(m_pTempData, pDetailObj, &dymmyLod, passInfo))
					continue;
				;

				pDetailObj->m_pRenderNode = 0;
				pDetailObj->m_ObjFlags |= (((passInfo.IsGeneralPass()) ? FOB_NO_FOG : 0)); // enable fog on recursive rendering (per-vertex)
				pDetailObj->m_II.m_AmbColor = Get3DEngine()->GetSkyColor();
				pDetailObj->m_fDistance = m_arrfDistance[passInfo.GetRecursiveLevel()];
				pDetailObj->m_II.m_Matrix = pTerrainRenderObject->m_II.m_Matrix;
				pDetailObj->m_ObjFlags |= FOB_TRANS_TRANSLATE | FOB_TERRAIN_LAYER;

				if (passInfo.RenderShadows())
				{
					pDetailObj->m_ObjFlags |= FOB_INSHADOW;
				}

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

										pMesh->AddRenderElements(pMat, pDetailObj, passInfo, EFSLIST_TERRAINLAYER, 1);
									}
								}
							}
					}
				}
			}
	}
}

// update data in video buffer
void CTerrainNode::UpdateRenderMesh(CStripsInfo* pArrayInfo, bool bUpdateVertices)
{
	FUNCTION_PROFILER_3DENGINE;

	_smart_ptr<IRenderMesh>& pRenderMesh = GetLeafData()->m_pRenderMesh;
	bool bIndicesUpdated = false;

	// if changing lod or there is no buffer allocated - reallocate
	if (!pRenderMesh || m_cCurrGeomMML != m_cNewGeomMML || bUpdateVertices)// || GetCVars()->e_TerrainDrawThisSectorOnly)
	{
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

		ERenderMeshType eRMType = eRMT_Dynamic;

#if CRY_PLATFORM_WINDOWS
		eRMType = eRMT_Dynamic;
#endif

		pRenderMesh = GetRenderer()->CreateRenderMeshInitialized(
		  m_pUpdateTerrainTempData->m_lstTmpVertArray.GetElements(), m_pUpdateTerrainTempData->m_lstTmpVertArray.Count(), eVF_P2S_N4B_C4B_T1F,
		  pArrayInfo->idx_array.GetElements(), pArrayInfo->idx_array.Count(),
		  prtTriangleList, "TerrainSector", "TerrainSector", eRMType, 1,
		  m_nTexSet.nTex0, NULL, NULL, false, true, lstTangents.Count() ? lstTangents.GetElements() : NULL);
		AABB boxWS = GetBBox();
		pRenderMesh->SetBBox(boxWS.min, boxWS.max);
		bIndicesUpdated = true;
	}

	if (!bUpdateVertices && !GetCVars()->e_TerrainDrawThisSectorOnly)
	{
		assert(pArrayInfo->idx_array.Count() <= (int)pRenderMesh->GetIndicesCount());
	}

	if (!bIndicesUpdated)
		pRenderMesh->UpdateIndices(pArrayInfo->idx_array.GetElements(), pArrayInfo->idx_array.Count(), 0, 0u);

	pRenderMesh->SetChunk(GetTerrain()->m_pTerrainEf, 0, pRenderMesh->GetVerticesCount(), 0, pArrayInfo->idx_array.Count(), 1.0f, 0);

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

	if (m_pLeafData)
		m_pLeafData->m_lstNeighbourSectors.Clear();

	if (bRecursive && m_pChilds)
		for (int i = 0; i < 4; i++)
			m_pChilds[i].ResetHeightMapGeometry(bRecursive, pBox);
}

// fill vertex buffer
void CTerrainNode::BuildVertices(int nStep, bool bSafetyBorder)
{
	FUNCTION_PROFILER_3DENGINE;

	m_pUpdateTerrainTempData->m_lstTmpVertArray.Clear();

	int nSectorSize = CTerrain::GetSectorSize() << m_nTreeLevel;

	int nSafetyBorder = 0;

	// keep often used variables on stack
	const int nOriginX = m_nOriginX;
	const int nOriginY = m_nOriginY;
	const int nSID = m_nSID;
	const int nTerrainSize = CTerrain::GetTerrainSize();
	const int iLookupRadius = 2 * CTerrain::GetHeightMapUnitSize();
	CTerrain* pTerrain = GetTerrain();

	for (int x = nOriginX - nSafetyBorder; x <= nOriginX + nSectorSize + nSafetyBorder; x += nStep)
	{
		for (int y = nOriginY - nSafetyBorder; y <= nOriginY + nSectorSize + nSafetyBorder; y += nStep)
		{
			int _x = CLAMP(x, nOriginX, nOriginX + nSectorSize);
			int _y = CLAMP(y, nOriginY, nOriginY + nSectorSize);
			float _z = pTerrain->GetZ(_x, _y, nSID);

			SVF_P2S_N4B_C4B_T1F vert;

			vert.xy = CryHalf2((float)(_x - nOriginX), (float)(_y - nOriginY));
			vert.z = _z;

			// calculate surface normal
#ifdef SEG_WORLD
			bool bOutOfBound = (x + iLookupRadius) >= nTerrainSize || x <= iLookupRadius;
			float sx = pTerrain->GetZ(x + iLookupRadius, y, nSID, bOutOfBound) - pTerrain->GetZ(x - iLookupRadius, y, nSID, bOutOfBound);

			bOutOfBound = (y + iLookupRadius) >= nTerrainSize || y <= iLookupRadius;
			float sy = pTerrain->GetZ(x, y + iLookupRadius, nSID, bOutOfBound) - pTerrain->GetZ(x, y - iLookupRadius, nSID, bOutOfBound);
#else
			float sx;
			if ((x + iLookupRadius) < nTerrainSize && x > iLookupRadius)
				sx = pTerrain->GetZ(x + iLookupRadius, y, nSID) - pTerrain->GetZ(x - iLookupRadius, y, nSID);
			else
				sx = 0;

			float sy;
			if ((y + iLookupRadius) < nTerrainSize && y > iLookupRadius)
				sy = pTerrain->GetZ(x, y + iLookupRadius, nSID) - pTerrain->GetZ(x, y - iLookupRadius, nSID);
			else
				sy = 0;
#endif
			// z component of normal will be used as point brightness ( for burned terrain )
			Vec3 vNorm(-sx, -sy, iLookupRadius * 2.0f);
			vNorm.Normalize();

			vert.normal.bcolor[0] = (byte)(vNorm[0] * 127.5f + 128.0f);
			vert.normal.bcolor[1] = (byte)(vNorm[1] * 127.5f + 128.0f);
			vert.normal.bcolor[2] = (byte)(vNorm[2] * 127.5f + 128.0f);
			vert.normal.bcolor[3] = 255;
			SwapEndian(vert.normal.dcolor, eLittleEndian);

			uint8 ucSurfaceTypeID = pTerrain->GetSurfaceTypeID(x, y, nSID);
			if (ucSurfaceTypeID == SRangeInfo::e_hole)
			{
				// in case of hole - try to find some valid surface type around
				for (int i = -nStep; i <= nStep && (ucSurfaceTypeID == SRangeInfo::e_hole); i += nStep)
					for (int j = -nStep; j <= nStep && (ucSurfaceTypeID == SRangeInfo::e_hole); j += nStep)
						ucSurfaceTypeID = pTerrain->GetSurfaceTypeID(x + i, y + j, nSID);
			}

			vert.color.bcolor[0] = 255;
			vert.color.bcolor[1] = ucSurfaceTypeID;
			vert.color.bcolor[2] = 255;
			vert.color.bcolor[3] = 255;
			SwapEndian(vert.color.dcolor, eLittleEndian);

			m_pUpdateTerrainTempData->m_lstTmpVertArray.Add(vert);
		}
	}
}

namespace Util
{
void AddNeighbourNode(PodArray<CTerrainNode*>* plstNeighbourSectors, CTerrainNode* pNode)
{
	// todo: cache this list, it is always the same
	if (pNode && plstNeighbourSectors->Find(pNode) < 0)
		plstNeighbourSectors->Add(pNode);
}
}

void CTerrainNode::AddIndexAliased(int _x, int _y, int _step, int nSectorSize, PodArray<CTerrainNode*>* plstNeighbourSectors, CStripsInfo* pArrayInfo, const SRenderingPassInfo& passInfo)
{
	int nAliasingX = 1, nAliasingY = 1;
	int nShiftX = 0, nShiftY = 0;

	CTerrain* pTerrain = GetTerrain();
	int nHeightMapUnitSize = CTerrain::GetHeightMapUnitSize();

	IF (_x && _x < nSectorSize && plstNeighbourSectors, true)
	{
		IF (_y == 0, false)
		{
			if (CTerrainNode* pNode = pTerrain->GetSecInfo(m_nOriginX + _x, m_nOriginY + _y - _step, m_nSID))
			{
				int nAreaMML = pNode->GetAreaLOD(passInfo);
				if (nAreaMML != MML_NOT_SET)
				{
					nAliasingX = nHeightMapUnitSize * (1 << nAreaMML);
					nShiftX = nAliasingX / 4;
				}
				Util::AddNeighbourNode(plstNeighbourSectors, pNode);
			}
		}
		else if (_y == nSectorSize)
		{
			if (CTerrainNode* pNode = pTerrain->GetSecInfo(m_nOriginX + _x, m_nOriginY + _y + _step, m_nSID))
			{
				int nAreaMML = pNode->GetAreaLOD(passInfo);
				if (nAreaMML != MML_NOT_SET)
				{
					nAliasingX = nHeightMapUnitSize * (1 << nAreaMML);
					nShiftX = nAliasingX / 2;
				}
				Util::AddNeighbourNode(plstNeighbourSectors, pNode);
			}
		}
	}

	IF (_y && _y < nSectorSize && plstNeighbourSectors, true)
	{
		IF (_x == 0, false)
		{
			if (CTerrainNode* pNode = pTerrain->GetSecInfo(m_nOriginX + _x - _step, m_nOriginY + _y, m_nSID))
			{
				int nAreaMML = pNode->GetAreaLOD(passInfo);
				if (nAreaMML != MML_NOT_SET)
				{
					nAliasingY = nHeightMapUnitSize * (1 << nAreaMML);
					nShiftY = nAliasingY / 4;
				}
				Util::AddNeighbourNode(plstNeighbourSectors, pNode);
			}
		}
		else if (_x == nSectorSize)
		{
			if (CTerrainNode* pNode = pTerrain->GetSecInfo(m_nOriginX + _x + _step, m_nOriginY + _y, m_nSID))
			{
				int nAreaMML = pNode->GetAreaLOD(passInfo);
				if (nAreaMML != MML_NOT_SET)
				{
					nAliasingY = nHeightMapUnitSize * (1 << nAreaMML);
					nShiftY = nAliasingY / 2;
				}
				Util::AddNeighbourNode(plstNeighbourSectors, pNode);
			}
		}
	}

	int XA = nAliasingX ? (_x + nShiftX) / nAliasingX * nAliasingX : _x;
	int YA = nAliasingY ? (_y + nShiftY) / nAliasingY * nAliasingY : _y;

	assert(XA >= 0 && XA <= nSectorSize);
	assert(YA >= 0 && YA <= nSectorSize);

	pArrayInfo->AddIndex(XA, YA, _step, nSectorSize);
}

void CTerrainNode::BuildIndices(CStripsInfo& si, PodArray<CTerrainNode*>* pNeighbourSectors, bool bSafetyBorder, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	// 1<<MML_NOT_SET will generate 0;
	if (m_cNewGeomMML == MML_NOT_SET)
		return;

	int nStep = (1 << m_cNewGeomMML) * CTerrain::GetHeightMapUnitSize();
	int nSectorSize = CTerrain::GetSectorSize() << m_nTreeLevel;

	int nSafetyBorder = bSafetyBorder ? nStep : 0;

	int nSectorSizeSB = nSectorSize + nSafetyBorder * 2;

	si.Clear();

	CStripsInfo* pSI = &si;
	CTerrain* pTerrain = GetTerrain();

	int nHalfStep = nStep / 2;

	// add non borders
	for (int x = 0; x < nSectorSizeSB; x += nStep)
	{
		for (int y = 0; y < nSectorSizeSB; y += nStep)
		{
			if (!m_bHasHoles || !pTerrain->GetHole(m_nOriginX + x + nHalfStep, m_nOriginY + y + nHalfStep, m_nSID))
			{
				if (pTerrain->IsMeshQuadFlipped(m_nOriginX + x, m_nOriginY + y, nStep, 0))
				{
					AddIndexAliased(x + nStep, y, nStep, nSectorSizeSB, pNeighbourSectors, pSI, passInfo);
					AddIndexAliased(x + nStep, y + nStep, nStep, nSectorSizeSB, pNeighbourSectors, pSI, passInfo);
					AddIndexAliased(x, y, nStep, nSectorSizeSB, pNeighbourSectors, pSI, passInfo);

					AddIndexAliased(x, y, nStep, nSectorSizeSB, pNeighbourSectors, pSI, passInfo);
					AddIndexAliased(x + nStep, y + nStep, nStep, nSectorSizeSB, pNeighbourSectors, pSI, passInfo);
					AddIndexAliased(x, y + nStep, nStep, nSectorSizeSB, pNeighbourSectors, pSI, passInfo);
				}
				else
				{
					AddIndexAliased(x, y, nStep, nSectorSizeSB, pNeighbourSectors, pSI, passInfo);
					AddIndexAliased(x + nStep, y, nStep, nSectorSizeSB, pNeighbourSectors, pSI, passInfo);
					AddIndexAliased(x, y + nStep, nStep, nSectorSizeSB, pNeighbourSectors, pSI, passInfo);

					AddIndexAliased(x + nStep, y, nStep, nSectorSizeSB, pNeighbourSectors, pSI, passInfo);
					AddIndexAliased(x + nStep, y + nStep, nStep, nSectorSizeSB, pNeighbourSectors, pSI, passInfo);
					AddIndexAliased(x, y + nStep, nStep, nSectorSizeSB, pNeighbourSectors, pSI, passInfo);
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

	//m_nNodeRenderLastFrameId = passInfo.GetMainFrameID();

	assert(m_cNewGeomMML == MML_NOT_SET || m_cNewGeomMML < GetTerrain()->m_nUnitsToSectorBitShift);

	STerrainNodeLeafData* pLeafData = GetLeafData();

	// detect if any neighbors switched lod since previous frame
	bool bNeighbourChanged = false;

	if (!passInfo.IsShadowPass())
	{
		for (int i = 0; i < pLeafData->m_lstNeighbourSectors.Count(); i++)
		{
			if (!pLeafData->m_lstNeighbourSectors[i])
				continue;
			int nNeighbourNewMML = pLeafData->m_lstNeighbourSectors[i]->GetAreaLOD(passInfo);
			if (nNeighbourNewMML == MML_NOT_SET)
				continue;
			if (nNeighbourNewMML != pLeafData->m_lstNeighbourLods[i] && (nNeighbourNewMML > m_cNewGeomMML || pLeafData->m_lstNeighbourLods[i] > m_cNewGeomMML))
			{
				bNeighbourChanged = true;
				break;
			}
		}
	}
	else
	{
		assert(!"Shadow-gen is not supposed to be rendered this way");
	}

	IRenderMesh * pRenderMesh = (m_nTreeLevel >= GetCVars()->e_TerrainMeshInstancingMinLod) ? GetSharedRenderMesh() : GetLeafData()->m_pRenderMesh;

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
		if (passInfo.GetRecursiveLevel() || (m_cCurrGeomMML == m_cNewGeomMML && !bNeighbourChanged) || m_nTreeLevel >= GetCVars()->e_TerrainMeshInstancingMinLod ||
		    (passInfo.IsCachedShadowPass() && passInfo.GetShadowMapType() == SRenderingPassInfo::SHADOW_MAP_CACHED_MGPU_COPY))
		{
			DrawArray(passInfo);
			return true;
		}
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
		FRAME_PROFILER("Sync_UpdateTerrain", GetSystem(), PROFILE_3DENGINE);
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

	UpdateRenderMesh(&m_pUpdateTerrainTempData->m_StripsInfo, !m_bUpdateOnlyBorders);

	m_cCurrGeomMML = m_cNewGeomMML;

	// update detail layers indices
	if (passInfo.RenderTerrainDetailMaterial())
	{
		// build all indices
		GenerateIndicesForAllSurfaces(pRenderMesh, m_bUpdateOnlyBorders, pLeafData->m_arrpNonBorderIdxNum, m_pUpdateTerrainTempData->m_StripsInfo.nNonBorderIndicesCount, 0, m_nSID, m_pUpdateTerrainTempData);

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

							if (!m_bUpdateOnlyBorders && m_lstSurfaceTypeInfo[i].arrpRM[p] && (lstIndices.Count() != m_lstSurfaceTypeInfo[i].arrpRM[p]->GetIndicesCount()))
							{
								m_lstSurfaceTypeInfo[i].arrpRM[p] = NULL;
							}

							if (lstIndices.Count())
								UpdateSurfaceRenderMeshes(pRenderMesh, pSurfaceType, m_lstSurfaceTypeInfo[i].arrpRM[p], p, lstIndices, "TerrainMaterialLayer", m_bUpdateOnlyBorders, pLeafData->m_arrpNonBorderIdxNum[pSurfaceType->ucThisSurfaceTypeId][nProjId], passInfo);

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
	PodArray<CTerrainNode*>& lstNeighbourSectors = pLeafData->m_lstNeighbourSectors;
	PodArray<uint8>& lstNeighbourLods = pLeafData->m_lstNeighbourLods;
	lstNeighbourSectors.Clear();
	BuildIndices(pUpdateTerrainTempData->m_StripsInfo, &lstNeighbourSectors, false, passInfo);

	// remember neighbor LOD's
	for (int i = 0; i < lstNeighbourSectors.Count(); i++)
	{
		int nNeighbourMML = lstNeighbourSectors[i]->GetAreaLOD(passInfo);
		assert(0 <= nNeighbourMML && nNeighbourMML <= ((uint8) - 1));
		lstNeighbourLods[i] = nNeighbourMML;
	}
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

	// 1U<<MML_NOT_SET will generate zero
	if (m_cNewGeomMML == MML_NOT_SET)
		return;

	int nStep = (1 << m_cNewGeomMML) * CTerrain::GetHeightMapUnitSize();
	int nSectorSize = CTerrain::GetSectorSize() << m_nTreeLevel;
	assert(nStep && nStep <= nSectorSize);
	if (nStep > nSectorSize)
		nStep = nSectorSize;

	// this could cost performace? to allow parallel execution, we cannot check for unchanged indices
#if 1
	BuildVertices(nStep, false);
	m_bUpdateOnlyBorders = false;
#else
	// update vertices if needed
	int nVertsNumRequired = (nSectorSize / nStep) * (nSectorSize / nStep) + (nSectorSize / nStep) + (nSectorSize / nStep) + 1;
	if (!pRenderMesh || m_cCurrGeomMML != m_cNewGeomMML || nVertsNumRequired != pRenderMesh->GetVerticesCount() || m_bEditor || !m_cNewGeomMML)
	{
		if (m_pUpdateTerrainTempData->m_StripsInfo.idx_array.Count())
			BuildVertices(nStep, false);
		else
			m_pUpdateTerrainTempData->m_lstTmpVertArray.Clear();

		m_bUpdateOnlyBorders = false;
	}
	else
	{
		m_bUpdateOnlyBorders = true;
	}
#endif

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

void CTerrainNode::GenerateIndicesForAllSurfaces(IRenderMesh* pRM, bool bOnlyBorder, int arrpNonBorderIdxNum[SRangeInfo::e_max_surface_types][4], int nBorderStartIndex, SSurfaceTypeInfo* pSurfaceTypeInfos, int nSID, CUpdateTerrainTempData* pUpdateTerrainTempData)
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

			if (!bOnlyBorder)
				arrpNonBorderIdxNum[s][p] = -1;
		}

		if (pSurfaceTypeInfos)
		{
			if (pSurfaceTypeInfos[s].pSurfaceType)
				arrMat3DFlag[s] = pSurfaceTypeInfos[s].pSurfaceType->IsMaterial3D();
		}
		else
			arrMat3DFlag[s] = GetTerrain()->GetSurfaceTypes(nSID)[s].IsMaterial3D();
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
	if (pUpdateTerrainTempData && !bOnlyBorder)
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

		for (int j = (bOnlyBorder ? nBorderStartIndex : 0); j < nSrcCount; j += 3)
		{
			vtx_idx arrTriangle[3] = { pSrcInds[j + 0], pSrcInds[j + 1], pSrcInds[j + 2] };

			// ignore invalid triangles
			if (arrTriangle[0] == arrTriangle[1] || arrTriangle[1] == arrTriangle[2] || arrTriangle[2] == arrTriangle[0])
				continue;

			assert(arrTriangle[0] < (unsigned)nVertCount && arrTriangle[1] < (unsigned)nVertCount && arrTriangle[2] < (unsigned)nVertCount);

			if (arrTriangle[0] >= (unsigned)nVertCount || arrTriangle[1] >= (unsigned)nVertCount || arrTriangle[2] >= (unsigned)nVertCount)
				continue;

			UCol& Color0 = *(UCol*)&pColor[arrTriangle[0] * nColorStride];
			UCol& Color1 = *(UCol*)&pColor[arrTriangle[1] * nColorStride];
			UCol& Color2 = *(UCol*)&pColor[arrTriangle[2] * nColorStride];

			uint32 nVertSurfId0 = Color0.bcolor[1] & SRangeInfo::e_undefined;
			uint32 nVertSurfId1 = Color1.bcolor[1] & SRangeInfo::e_undefined;
			uint32 nVertSurfId2 = Color2.bcolor[1] & SRangeInfo::e_undefined;

			nVertSurfId0 = CLAMP(nVertSurfId0, 0, SRangeInfo::e_undefined);
			nVertSurfId1 = CLAMP(nVertSurfId1, 0, SRangeInfo::e_undefined);
			nVertSurfId2 = CLAMP(nVertSurfId2, 0, SRangeInfo::e_undefined);

			int lstSurfTypes[3];
			int idxSurfTypes(0);
			lstSurfTypes[idxSurfTypes++] = nVertSurfId0;
			if (nVertSurfId1 != nVertSurfId0)
				lstSurfTypes[idxSurfTypes++] = nVertSurfId1;
			if (nVertSurfId2 != nVertSurfId0 && nVertSurfId2 != nVertSurfId1)
				lstSurfTypes[idxSurfTypes++] = nVertSurfId2;

			int lstProjIds[3];
			int idxProjIds(0);

			byte* pNorm;
			Vec3 vNorm;

			// if there are 3d materials - analyze normals
			if (arrMat3DFlag[nVertSurfId0])
			{
				pNorm = &pNormB[arrTriangle[0] * nNormStride];
				vNorm.Set(((float)pNorm[0] - 127.5f), ((float)pNorm[1] - 127.5f), ((float)pNorm[2] - 127.5f));
				int nProjId0 = GetVecProjectId(vNorm);
				lstProjIds[idxProjIds++] = nProjId0;
			}

			if (arrMat3DFlag[nVertSurfId1])
			{
				pNorm = &pNormB[arrTriangle[1] * nNormStride];
				vNorm.Set(((float)pNorm[0] - 127.5f), ((float)pNorm[1] - 127.5f), ((float)pNorm[2] - 127.5f));
				int nProjId1 = GetVecProjectId(vNorm);
				if (idxProjIds == 0 || lstProjIds[0] != nProjId1)
					lstProjIds[idxProjIds++] = nProjId1;
			}

			if (arrMat3DFlag[nVertSurfId2])
			{
				pNorm = &pNormB[arrTriangle[2] * nNormStride];
				vNorm.Set(((float)pNorm[0] - 127.5f), ((float)pNorm[1] - 127.5f), ((float)pNorm[2] - 127.5f));
				int nProjId2 = GetVecProjectId(vNorm);
				if ((idxProjIds < 2 || lstProjIds[1] != nProjId2) &&
				    (idxProjIds < 1 || lstProjIds[0] != nProjId2))
					lstProjIds[idxProjIds++] = nProjId2;
			}

			// if not 3d materials found
			if (!arrMat3DFlag[nVertSurfId0] || !arrMat3DFlag[nVertSurfId1] || !arrMat3DFlag[nVertSurfId2])
				lstProjIds[idxProjIds++] = 3;

			for (int s = 0; s < idxSurfTypes; s++)
			{
				if (lstSurfTypes[s] == SRangeInfo::e_undefined)
				{
					continue;
				}

				for (int p = 0; p < idxProjIds; p++)
				{
					assert(lstSurfTypes[s] >= 0 && lstSurfTypes[s] < SRangeInfo::e_undefined);
					assert(lstProjIds[p] >= 0 && lstProjIds[p] < 4);
					PodArray<vtx_idx>& rList = m_arrIndices[lstSurfTypes[s]][lstProjIds[p]];

					if (!bOnlyBorder && j >= nBorderStartIndex && arrpNonBorderIdxNum[lstSurfTypes[s]][lstProjIds[p]] < 0)
						arrpNonBorderIdxNum[lstSurfTypes[s]][lstProjIds[p]] = rList.Count();

					rList.AddList(arrTriangle, 3);
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
                                             PodArray<vtx_idx>& lstIndices, const char* szComment, bool bUpdateOnlyBorders, int nNonBorderIndicesCount,
                                             const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	ERenderMeshType eRMType = eRMT_Dynamic;

#if CRY_PLATFORM_WINDOWS
	eRMType = eRMT_Dynamic;
#endif

	// force new rendermesh if vertex container has changed. Vertex containers aren't thread
	// safe, but seems like maybe something relies on this behaviour since fixing it on RM side
	// causes flickering.
	if (!pMatRM || (pMatRM && pMatRM->GetVertexContainer() != pSrcRM))
	{
		pMatRM = GetRenderer()->CreateRenderMeshInitialized(
		  NULL, 0, eVF_P2S_N4B_C4B_T1F, NULL, 0,
		  prtTriangleList, szComment, szComment, eRMType, 1, 0, NULL, NULL, false, false);
	}

	uint8 szProj[] = "XYZ";

	pMatRM->LockForThreadAccess();
	pMatRM->SetVertexContainer(pSrcRM);

	assert(1 || nNonBorderIndicesCount >= 0);

	if (bUpdateOnlyBorders)
	{
		int nOldIndexCount = pMatRM->GetIndicesCount();

		if (nNonBorderIndicesCount < 0 || !nOldIndexCount)
		{
			pMatRM->UnLockForThreadAccess();
			return; // no borders involved
		}

		vtx_idx* pIndicesOld = pMatRM->GetIndexPtr(FSL_READ);

		int nIndexCountNew = nNonBorderIndicesCount + lstIndices.Count();

		assert(true || nIndexCountNew >= lstIndices.Count());

		//C6255: _alloca indicates failure by raising a stack overflow exception. Consider using _malloca instead
		PREFAST_SUPPRESS_WARNING(6255)
		vtx_idx * pIndicesNew = (vtx_idx*) alloca(sizeof(vtx_idx) * nIndexCountNew);

		memcpy(pIndicesNew, pIndicesOld, sizeof(vtx_idx) * nNonBorderIndicesCount);

		memcpy(pIndicesNew + nNonBorderIndicesCount, lstIndices.GetElements(), sizeof(vtx_idx) * lstIndices.Count());

		pMatRM->UpdateIndices(pIndicesNew, nIndexCountNew, 0, 0u);
		pMatRM->SetChunk(pSurface->GetMaterialOfProjection(szProj[nProjectionId]), 0, pSrcRM->GetVerticesCount(), 0, nIndexCountNew, 1.0f);
	}
	else
	{
		pMatRM->UpdateIndices(lstIndices.GetElements(), lstIndices.Count(), 0, 0u);
		pMatRM->SetChunk(pSurface->GetMaterialOfProjection(szProj[nProjectionId]), 0, pSrcRM->GetVerticesCount(), 0, lstIndices.Count(), 1.0f);
	}

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

	// set texgen offset
	Vec3 vCamPos = passInfo.GetCamera().GetPosition();

	pParams[12] = pParams[13] = pParams[14] = pParams[15] = 0;

	// for diffuse
	if (IMaterial* pMat = pSurface->GetMaterialOfProjection(szProj[nProjectionId]))
		if (pMat->GetShaderItem().m_pShaderResources)
		{
			if (SEfResTexture* pTex = pMat->GetShaderItem().m_pShaderResources->GetTexture(EFTT_DIFFUSE))
			{
				float fScaleX = pTex->m_bUTile ? pTex->GetTiling(0) * pSurface->fScale : 1.f;
				float fScaleY = pTex->m_bVTile ? pTex->GetTiling(1) * pSurface->fScale : 1.f;

				pParams[12] = int(vCamPos.x * fScaleX) / fScaleX;
				pParams[13] = int(vCamPos.y * fScaleY) / fScaleY;
			}

			if (SEfResTexture* pTex = pMat->GetShaderItem().m_pShaderResources->GetTexture(EFTT_NORMALS))
			{
				float fScaleX = pTex->m_bUTile ? pTex->GetTiling(0) * pSurface->fScale : 1.f;
				float fScaleY = pTex->m_bVTile ? pTex->GetTiling(1) * pSurface->fScale : 1.f;

				pParams[14] = int(vCamPos.x * fScaleX) / fScaleX;
				pParams[15] = int(vCamPos.y * fScaleY) / fScaleY;
			}
		}

	assert(8 + 8 <= ARR_TEX_OFFSETS_SIZE_DET_MAT);

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

void AddIndexShared(int _x, int _y, PodArray<vtx_idx> & arrIndices, int nSectorSize)
{
	int _step = 1;

	vtx_idx id = _x / _step * (nSectorSize / _step + 1) + _y / _step;

	arrIndices.Add(id);
}

// Build single render mesh (with safety borders) to be re-used for multiple sectors
_smart_ptr<IRenderMesh> CTerrainNode::GetSharedRenderMesh()
{
	if (m_pTerrain->m_pSharedRenderMesh)
		return m_pTerrain->m_pSharedRenderMesh;

	int nDim = 32;
	int nBorder = 1;

	SVF_P2S_N4B_C4B_T1F vert;
	ZeroStruct(vert);
	PodArray<SVF_P2S_N4B_C4B_T1F> arrVertices;

	for (int x = -nBorder; x <= (nDim + nBorder); x++)
	{
		for (int y = -nBorder; y <= (nDim + nBorder); y++)
		{
			vert.xy = CryHalf2(SATURATE(float(x)/float(nDim)), SATURATE(float(y) / float(nDim)));

			if(x<0 || y<0 || x>nDim || y>nDim)
				vert.z = -0.1f;
			else
				vert.z = 0.f;

			arrVertices.Add(vert);
		}
	}

	PodArray<vtx_idx> arrIndices;

	int nDimEx = nDim + nBorder * 2;

	for (int x = 0; x < nDimEx; x++)
	{
		for (int y = 0; y < nDimEx; y++)
		{
			AddIndexShared(x + 1, y + 0, arrIndices, nDimEx);
			AddIndexShared(x + 1, y + 1, arrIndices, nDimEx);
			AddIndexShared(x + 0, y + 0, arrIndices, nDimEx);

			AddIndexShared(x + 0, y + 0, arrIndices, nDimEx);
			AddIndexShared(x + 1, y + 1, arrIndices, nDimEx);
			AddIndexShared(x + 0, y + 1, arrIndices, nDimEx);
		}
	}

	m_pTerrain->m_pSharedRenderMesh = GetRenderer()->CreateRenderMeshInitialized(
		arrVertices.GetElements(),
		arrVertices.Count(),
		eVF_P2S_N4B_C4B_T1F,
		arrIndices.GetElements(),
		arrIndices.Count(),
		prtTriangleList,
		"TerrainSectorSharedRenderMesh", "TerrainSectorSharedRenderMesh",
		eRMT_Static);

	m_pTerrain->m_pSharedRenderMesh->SetChunk(NULL, 0, arrVertices.Count(), 0, arrIndices.Count(), 1.0f, 0);

	return m_pTerrain->m_pSharedRenderMesh;
}
