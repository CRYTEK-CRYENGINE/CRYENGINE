// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "StatObj.h"
#include "IndexedMesh.h"
#include "3dEngine.h"
#include <CryParticleSystem/IParticles.h>
#include <Cry3DEngine/CGF/CGFContent.h>
#include "ObjMan.h"
#include <CryThreading/IJobManager.h>
#include <CryThreading/IJobManager_JobDelegator.h>
#include <CryEntitySystem/IEntity.h>

#define SMALL_MESH_NUM_INDEX 30

#pragma warning(push)
#pragma warning(disable: 4244)

//////////////////////////////////////////////////////////////////////////
void CStatObj::PhysicalizeCompiled(CNodeCGF* pNode, int bAppend)
{
	FUNCTION_PROFILER_3DENGINE;

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Physics, 0, "Physics");

	if (!GetPhysicalWorld())
		return;

	CMesh* pMesh = pNode->pMesh;
	bool bMeshNotNeeded = (pNode->nPhysicalizeFlags & CNodeCGF::ePhysicsalizeFlag_MeshNotNeeded) != 0;
	if (bMeshNotNeeded)
	{
		// Solves a crash in loading solid brushes for now, remove later
		//pMesh = 0;
	}

	bool bHavePhysicsData = false;
	for (int nPhysGeomType = 0; nPhysGeomType < 4; nPhysGeomType++)
	{
		if (!pNode->physicalGeomData[nPhysGeomType].empty())
		{
			bHavePhysicsData = true;
			break;
		}
	}
	if (!bHavePhysicsData)
		return;

	std::vector<char> faceMaterials;
	if (pMesh)
	{
		CMesh& mesh = *pMesh;
		// Fill faces material array.
		faceMaterials.resize(mesh.GetIndexCount() / 3);
		if (!faceMaterials.empty())
		{
			for (int m = 0; m < mesh.GetSubSetCount(); m++)
			{
				char* pFaceMats = &faceMaterials[0];
				SMeshSubset& subset = mesh.m_subsets[m];
				for (int nFaceIndex = subset.nFirstIndexId / 3, last = subset.nFirstIndexId / 3 + subset.nNumIndices / 3; nFaceIndex < last; nFaceIndex++)
				{
					pFaceMats[nFaceIndex] = subset.nMatID;
				}
			}
		}
	}

	for (int nPhysGeomType = 0; nPhysGeomType < 4; nPhysGeomType++)
	{
		DynArray<char>& physData = pNode->physicalGeomData[nPhysGeomType];
		if (!physData.empty())
		{
			char* pFaceMats = 0;
			if (!faceMaterials.empty())
				pFaceMats = &faceMaterials[0];

			phys_geometry* pPhysGeom = 0;

			// We have physical geometry for this node.
			CMemStream stm(&physData.front(), physData.size(), true);
			if (pMesh)
			{
				// Load using mesh information.
				CMesh& mesh = *pMesh;
				if (mesh.m_pPositionsF16)
				{
					const int vertexCount = mesh.GetVertexCount();
					if (vertexCount <= 0 || bMeshNotNeeded)
					{
						pPhysGeom = GetPhysicalWorld()->GetGeomManager()->LoadPhysGeometry(stm, 0, 0, 0);
					}
					else
					{
						Vec3* const pPos = new Vec3[vertexCount];
						for (int i = 0; i < vertexCount; ++i)
						{
							pPos[i] = mesh.m_pPositionsF16[i].ToVec3();
						}
						pPhysGeom = GetPhysicalWorld()->GetGeomManager()->LoadPhysGeometry(stm, pPos, mesh.m_pIndices, pFaceMats);
						delete[] pPos;
					}
				}
				else
				{
					pPhysGeom = GetPhysicalWorld()->GetGeomManager()->LoadPhysGeometry(stm, mesh.m_pPositions, mesh.m_pIndices, pFaceMats);
				}
			}
			else
			{
				// Load only using physics saved data.
				pPhysGeom = GetPhysicalWorld()->GetGeomManager()->LoadPhysGeometry(stm, 0, 0, 0);
			}
			if (!pPhysGeom)
			{
				FileWarning(0, GetFilePath(), "Bad physical data in CGF node, Physicalization Failed, (%s)", GetFilePath());
				return;
			}
			if (pPhysGeom->pGeom && pPhysGeom->pGeom->GetType() == GEOM_TRIMESH && pPhysGeom->pGeom->SanityCheck() == 0)
			{
				FileWarning(0, GetFilePath(), "Bad physical data in CGF node, Sanity Check failed, (%s)", GetFilePath());
			}
			mesh_data* pmd;
			if (pPhysGeom->pGeom && pPhysGeom->pGeom->GetType() == GEOM_TRIMESH && (pmd = (mesh_data*)pPhysGeom->pGeom->GetData()) &&
			    (uint)(pmd->nTris - 1) > (uint)(GetCVars()->e_PhysProxyTriLimit - 1))
			{
				FileWarning(0, GetFilePath(), "Phys proxy rejected due to triangle count limit, %d > %d (%s)", pmd->nTris, GetCVars()->e_PhysProxyTriLimit, GetFilePath());
				GetPhysicalWorld()->GetGeomManager()->UnregisterGeometry(pPhysGeom);
				m_isProxyTooBig = true;
				m_pMaterial = Get3DEngine()->GetMaterialManager()->LoadMaterial("%ENGINE%/EngineAssets/Materials/PhysProxyTooBig");
			}
			else
				AssignPhysGeom(nPhysGeomType + PHYS_GEOM_TYPE_DEFAULT, pPhysGeom, bAppend, /*bLoading*/ 1);
		}
	}

	if (pNode->nPhysicalizeFlags & CNodeCGF::ePhysicsalizeFlag_NoBreaking)
		m_nFlags |= STATIC_OBJECT_CANT_BREAK;
}

//////////////////////////////////////////////////////////////////////////
bool CStatObj::PhysicalizeGeomType(int nGeomType, CMesh& mesh, float tolerance, int bAppend)
{
	assert(mesh.m_pPositionsF16 == 0);

	std::vector<uint16> indices;
	std::vector<char> faceMaterials;
	indices.reserve(mesh.GetIndexCount());
	faceMaterials.reserve(mesh.GetIndexCount() / 3);

	for (int m = 0; m < mesh.GetSubSetCount(); m++)
	{
		SMeshSubset& subset = mesh.m_subsets[m];

		if (subset.nPhysicalizeType != nGeomType && !(subset.nPhysicalizeType == PHYS_GEOM_TYPE_DEFAULT_PROXY && nGeomType == PHYS_GEOM_TYPE_DEFAULT))
			continue;

		for (int idx = subset.nFirstIndexId; idx < subset.nFirstIndexId + subset.nNumIndices; idx += 3)
		{
			for (int c = 0; c < 3; ++c)
			{
				if (mesh.m_pIndices[idx + c] >= 0xffff)
				{
					return false;
				}
				indices.push_back(mesh.m_pIndices[idx + c]);
			}
			faceMaterials.push_back(subset.nMatID);
		}
	}

	if (indices.empty())
		return false;

	Vec3* pVertices = NULL;
	int nVerts = mesh.GetVertexCount();

	pVertices = mesh.m_pPositions;

	if (nVerts > 2)
	{
		Vec3 ptmin = mesh.m_bbox.max;
		Vec3 ptmax = mesh.m_bbox.min;

		int flags = mesh_multicontact1;
		//float tol = 0.05f;
		flags |= indices.size() <= SMALL_MESH_NUM_INDEX ? mesh_SingleBB : mesh_OBB | mesh_AABB;
		flags |= mesh_approx_box | mesh_approx_sphere | mesh_approx_cylinder | mesh_approx_capsule;
		flags |= mesh_shared_foreign_idx; // when this flags is set and pForeignIdx is 0, the physics assumes an array of fidx[i]==i

		if (m_bEditor)
			flags |= mesh_keep_vtxmap_for_saving;

		flags |= mesh_full_serialization;

		//////////////////////////////////////////////////////////////////////////
		/*
		   if (strstr(m_szGeomName,"wheel"))
		   {
		   flags |= mesh_approx_cylinder;
		   tol = 1.0f;
		   } else if (strstr(m_szFileName,"explosion") || strstr(m_szFileName,"crack"))
		   flags = flags & ~mesh_multicontact1 | mesh_multicontact2;	// temporary solution
		   else
		   flags |= mesh_approx_box | mesh_approx_sphere | mesh_approx_cylinder;
		 */

		int nMinTrisPerNode = 2;
		int nMaxTrisPerNode = 4;
		Vec3 size = ptmax - ptmin;
		if (indices.size() < 600 && max(max(size.x, size.y), size.z) > 6) // make more dense OBBs for large (wrt terrain grid) objects
			nMinTrisPerNode = nMaxTrisPerNode = 1;

		//////////////////////////////////////////////////////////////////////////
		// Create physical mesh.
		//////////////////////////////////////////////////////////////////////////
		IGeomManager* pGeoman = GetPhysicalWorld()->GetGeomManager();
		IGeometry* pGeom = pGeoman->CreateMesh(
		  pVertices,
		  &indices[0],
		  &faceMaterials[0],
		  0,
		  indices.size() / 3,
		  flags, tolerance, nMinTrisPerNode, nMaxTrisPerNode, 2.5f);

		phys_geometry* pPhysGeom = pGeoman->RegisterGeometry(pGeom, faceMaterials[0]);
		AssignPhysGeom(nGeomType, pPhysGeom, bAppend);
		pGeom->Release();
	}

	// Free temporary verts array.
	if (pVertices != mesh.m_pPositions)
		delete[] pVertices;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::AssignPhysGeom(int nGeomType, phys_geometry* pPhysGeom, int bAppend, int bLoading)
{
	assert(pPhysGeom);
	m_arrPhysGeomInfo.SetPhysGeom(pPhysGeom, m_arrPhysGeomInfo.GetGeomCount() & - bAppend | nGeomType & ~-bAppend, nGeomType);

	if (pPhysGeom->pGeom && !m_bNoHitRefinement)
	{
		pPhysGeom->pGeom->SetForeignData((IStatObj*)this, 0);
	}

	//////////////////////////////////////////////////////////////////////////
	// Set material mapping to physics geometry.
	//////////////////////////////////////////////////////////////////////////
	if (m_pMaterial)
	{
		int surfaceTypesId[MAX_SUB_MATERIALS], flagsPhys[MAX_SUB_MATERIALS];
		memset(surfaceTypesId, 0, sizeof(surfaceTypesId));
		int i, j, numIds = m_pMaterial->FillSurfaceTypeIds(surfaceTypesId);

		if (nGeomType == PHYS_GEOM_TYPE_DEFAULT)
		{
			ISurfaceTypeManager* pSurfaceTypeManager = Get3DEngine()->GetMaterialManager()->GetSurfaceTypeManager();
			ISurfaceType* pMat;
			float bounciness, friction;
			unsigned int flags = 0;

			for (i = 0; i < numIds; i++)
			{
				GetPhysicalWorld()->GetSurfaceParameters(surfaceTypesId[i], bounciness, friction, flags);
				flagsPhys[i] = flags >> sf_matbreakable_bit;
				if ((pMat = pSurfaceTypeManager->GetSurfaceType(surfaceTypesId[i])) &&
				    (pMat->GetFlags() & SURFACE_TYPE_VEHICLE_ONLY_COLLISION))
				{
					flagsPhys[i] |= (int)1 << 16;
				}
				if (flags & sf_manually_breakable)
					flagsPhys[i] |= (int)1 << 17;
			}

			mesh_data* pmd;
			m_idmatBreakable = -1;
			m_bVehicleOnlyPhysics = 0;
			m_bBreakableByGame = 0;
			assert(pPhysGeom->pGeom);
			PREFAST_ASSUME(pPhysGeom->pGeom);
			if ((i = pPhysGeom->pGeom->GetType()) == GEOM_TRIMESH || i == GEOM_VOXELGRID)
				for (pmd = (mesh_data*)pPhysGeom->pGeom->GetData(), i = 0; i < pmd->nTris; i++)
				{
					j = pmd->pMats[i];
					j &= -isneg(j - numIds);
					assert(j >= 0 && j < numIds);
					PREFAST_ASSUME(j >= 0 && j < numIds);
					m_idmatBreakable = max(m_idmatBreakable, (flagsPhys[j] & 0xFFFF) - 1);
					m_bVehicleOnlyPhysics |= flagsPhys[j] >> 16 & 1;
					m_bBreakableByGame |= flagsPhys[j] >> 17;
				}
			else if (pPhysGeom->surface_idx >= 0)
			{
				j = pPhysGeom->surface_idx & - isneg(pPhysGeom->surface_idx - numIds);
				assert(j >= 0 && j < numIds);
				PREFAST_ASSUME(j >= 0 && j < numIds);
				m_bVehicleOnlyPhysics = flagsPhys[j] >> 16;
				m_bBreakableByGame |= flagsPhys[j] >> 17;
			}
		}

		if (bLoading && m_idmatBreakable == -1 && m_bBreakableByGame == 0 && !strstr(m_szGeomName, "skeleton") && !(m_nFlags & STATIC_OBJECT_DYNAMIC) && !gEnv->IsEditor())
		{
			assert(pPhysGeom->pGeom);
			PREFAST_ASSUME(pPhysGeom->pGeom);
			pPhysGeom->pGeom->CompactMemory();  // Reclaim any sparse vertex memory
		}

		GetPhysicalWorld()->GetGeomManager()->SetGeomMatMapping(pPhysGeom, surfaceTypesId, numIds);

		if (m_bBreakableByGame)
			m_nFlags |= STATIC_OBJECT_DYNAMIC;
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::SavePhysicalizeData(CNodeCGF* pNode)
{
	pNode->nPhysicalizeFlags = CNodeCGF::ePhysicsalizeFlag_MeshNotNeeded;
	for (int nGeomType = 0; nGeomType < MAX_PHYS_GEOMS_TYPES; nGeomType++)
	{
		if (!m_arrPhysGeomInfo[nGeomType])
			continue;

		CMemStream stm(false);
		IGeomManager* pGeoman = GetPhysicalWorld()->GetGeomManager();
		pGeoman->SavePhysGeometry(stm, m_arrPhysGeomInfo[nGeomType]);

		// Add physicalized data to the node.
		pNode->physicalGeomData[nGeomType].resize(stm.GetUsedSize());
		memcpy(&pNode->physicalGeomData[nGeomType][0], stm.GetBuf(), stm.GetUsedSize());
	}
}

//////////////////////////////////////////////////////////////////////////
///////////////////////// Breakable Geometry /////////////////////////////
//////////////////////////////////////////////////////////////////////////

namespace
{
template<class T> struct SplitArray
{
	T* ptr[2];
	T& operator[](int idx) { return ptr[idx >> 15][idx & ~(1 << 15)]; }
	T* operator+(int idx)  { return ptr[idx >> 15] + (idx & ~(1 << 15)); }
	T* operator-(int idx)  { return ptr[idx >> 15] - (idx & ~(1 << 15)); }
};
}

static inline int mapiTri(int itri, int* pIdx2iTri)
{
	int mask = (itri - BOP_NEWIDX0) >> 31;
	return itri & mask | pIdx2iTri[itri - BOP_NEWIDX0 | mask];
}

static inline void swap(int* pSubsets, vtx_idx* pidx, int* pmap, int i1, int i2)
{
	int i = pSubsets[i1];
	pSubsets[i1] = pSubsets[i2];
	pSubsets[i2] = i;
	i = pmap[i1];
	pmap[i1] = pmap[i2];
	pmap[i2] = i;
	for (i = 0; i < 3; i++)
	{
		vtx_idx u = pidx[i1 * 3 + i];
		pidx[i1 * 3 + i] = pidx[i2 * 3 + i];
		pidx[i2 * 3 + i] = u;
	}
}
static void qsort(int* pSubsets, vtx_idx* pidx, int* pmap, int ileft, int iright, int iter = 0)
{
	if (ileft >= iright) return;
	int i, ilast, diff = 0;
	swap(pSubsets, pidx, pmap, ileft, (ileft + iright) >> 1);
	for (ilast = ileft, i = ileft + 1; i <= iright; i++)
	{
		diff |= pSubsets[i] - pSubsets[ileft];
		if (pSubsets[i] < pSubsets[ileft] + iter) // < when iter==0 and <= when iter==1
			swap(pSubsets, pidx, pmap, ++ilast, i);
	}
	swap(pSubsets, pidx, pmap, ileft, ilast);

	if (diff)
	{
		qsort(pSubsets, pidx, pmap, ileft, ilast - 1, iter ^ 1);
		qsort(pSubsets, pidx, pmap, ilast + 1, iright, iter ^ 1);
	}
}

static inline int check_mask(unsigned int* pMask, int i)
{
	return pMask[i >> 5] >> (i & 31) & 1;
}
static inline void set_mask(unsigned int* pMask, int i)
{
	pMask[i >> 5] |= 1u << (i & 31);
}
static inline void clear_mask(unsigned int* pMask, int i)
{
	pMask[i >> 5] &= ~(1u << (i & 31));
}

static int InjectVertices(CMesh* pMesh, int nNewVtx, SMeshBoneMapping_uint8*& pBoneMapping)
{
	assert(pMesh->m_pPositionsF16 == 0);

	int i, j, nVtx0 = pMesh->GetVertexCount(), nVtx, nMovedSubsets = 0;

	nVtx = pMesh->GetVertexCount() + nNewVtx;
	pMesh->ReallocStream(CMesh::POSITIONS, nVtx);
	pMesh->ReallocStream(CMesh::NORMALS, nVtx);
	pMesh->ReallocStream(CMesh::TEXCOORDS, nVtx);
	pMesh->ReallocStream(CMesh::TANGENTS, nVtx);
	if (pMesh->m_pColor0)
		pMesh->ReallocStream(CMesh::COLORS_0, nVtx);

	for (i = (int)pMesh->m_subsets.size() - 1; i >= 0; i--)
	{
		if (pMesh->m_subsets[i].nPhysicalizeType != PHYS_GEOM_TYPE_DEFAULT)
		{
			// move vertices in non-physicalized subsets up to make space for the vertices added by physics
			if (nNewVtx > 0)
			{
				j = pMesh->m_subsets[i].nFirstVertId;
				memmove(pMesh->m_pPositions + j + nNewVtx, pMesh->m_pPositions + j, sizeof(pMesh->m_pPositions[0]) * pMesh->m_subsets[i].nNumVerts);
				memmove(pMesh->m_pNorms + j + nNewVtx, pMesh->m_pNorms + j, sizeof(pMesh->m_pNorms[0]) * pMesh->m_subsets[i].nNumVerts);
				memmove(pMesh->m_pTexCoord + j + nNewVtx, pMesh->m_pTexCoord + j, sizeof(pMesh->m_pTexCoord[0]) * pMesh->m_subsets[i].nNumVerts);
				if (pMesh->m_pTangents)
					memmove(pMesh->m_pTangents + j + nNewVtx, pMesh->m_pTangents + j, sizeof(pMesh->m_pTangents[0]) * pMesh->m_subsets[i].nNumVerts);
				if (pMesh->m_pQTangents)
					memmove(pMesh->m_pQTangents + j + nNewVtx, pMesh->m_pQTangents + j, sizeof(pMesh->m_pQTangents[0]) * pMesh->m_subsets[i].nNumVerts);
				if (pMesh->m_pColor0)
					memmove(pMesh->m_pColor0 + j + nNewVtx, pMesh->m_pColor0 + j, sizeof(pMesh->m_pColor0[0]) * pMesh->m_subsets[i].nNumVerts);
				pMesh->m_subsets[i].nFirstVertId += nNewVtx;
				for (j = pMesh->m_subsets[i].nFirstIndexId; j < pMesh->m_subsets[i].nFirstIndexId + pMesh->m_subsets[i].nNumIndices; j++)
					pMesh->m_pIndices[j] += nNewVtx;
			}
			if (pMesh->m_subsets[i].nNumVerts)
				nVtx0 = pMesh->m_subsets[i].nFirstVertId, nMovedSubsets++;
		}
	}

	if (pBoneMapping && nNewVtx > 0)
	{
		SMeshBoneMapping_uint8* const pBoneMapping0 = pBoneMapping;
		memcpy(pBoneMapping = new SMeshBoneMapping_uint8[nVtx], pBoneMapping0, (pMesh->GetVertexCount() - nNewVtx) * sizeof(pBoneMapping[0]));
		delete[] pBoneMapping0;
		if (nMovedSubsets > 0)
			memmove(pBoneMapping + nVtx0, pBoneMapping + nVtx0 - nNewVtx, (nVtx - nVtx0) * sizeof(pBoneMapping[0]));
		memset(pBoneMapping, 0, nVtx0 * sizeof(pBoneMapping[0]));
	}

	return nVtx0;
}

IStatObj* C3DEngine::UpdateDeformableStatObj(IGeometry* pPhysGeom, bop_meshupdate* pLastUpdate, IFoliage* pSrcFoliage)
{
	int i, j, j1, itri, iop, itriOrg, ivtx, ivtxNew, nTris0, nVtx0, iSubsetB = 0, nNewVtx, nNewPhysVtx, ivtxRef, bEmptyObj, ibox;
	int* pIdx2iTri, * piTri2Idx, * pSubsets, * pSubsets0, * pVtxWeld;
	int bHasColors[2] = { 0, 0 };
	vtx_idx* pIdx0, * pIdx;
	float dist[3];
	Vec4 Color;
	unsigned int* pRemovedVtxMask;
	IIndexedMesh* pIndexedMesh[2];
	mesh_data* pPhysMesh /*,*pPhysMesh1*/;
	bop_meshupdate* pmu, * pmu0;
	CMesh* pMesh[2], meshAppend;
	IStatObj* pStatObj[2], * pObjClone;
	IMaterial* pMaterial[2];
	SplitArray<Vec3> pVtx;
	SplitArray<SMeshNormal> pNormals[2];
	SplitArray<SMeshTexCoord> pTexCoord[2];
	SplitArray<SMeshTangents> pTangents[2];
	SplitArray<SMeshColor> pColors[2];
	SplitArray<int> pVtxMap;
	SMeshSubset mss;
	Vec2 Coord;
	SMeshColor clrDummy = SMeshColor(128, 128, 128, 255);
	Vec3 Tangent, Bitangent, Normal, area;
	int16 NormalFlip;
	const char* matname;
	AABB BBox;
	int minVtx, maxVtx;
	static int g_ncalls = 0;
	g_ncalls++;

	FUNCTION_PROFILER_3DENGINE

#define AllocVtx                                                                                \
  if ((nNewVtx & 31) == 0)                                                                      \
  {                                                                                             \
    meshAppend.ReallocStream(CMesh::POSITIONS, nNewVtx + 32);                                   \
    meshAppend.ReallocStream(CMesh::NORMALS, nNewVtx + 32);                                     \
    meshAppend.ReallocStream(CMesh::TEXCOORDS, nNewVtx + 32);                                   \
    meshAppend.ReallocStream(CMesh::TANGENTS, nNewVtx + 32);                                    \
    if (bHasColors[0]) {                                                                        \
      meshAppend.ReallocStream(CMesh::COLORS_0, nNewVtx + 32);                                  \
      pColors[0].ptr[1] = meshAppend.m_pColor0;                                                 \
    }                                                                                           \
    pVtxMap.ptr[1] = (int*)realloc(pVtxMap.ptr[1], (nNewVtx + 32) * sizeof(pVtxMap.ptr[1][0])); \
    pVtx.ptr[1] = meshAppend.m_pPositions; pNormals[0].ptr[1] = meshAppend.m_pNorms;            \
    pTexCoord[0].ptr[1] = meshAppend.m_pTexCoord; pTangents[0].ptr[1] = meshAppend.m_pTangents; \
  }                                                                                             \
  ivtx = nNewVtx++ | 1 << 15

	pmu0 = (bop_meshupdate*)pPhysGeom->GetForeignData(DATA_MESHUPDATE);
	pStatObj[0] = (IStatObj*)pPhysGeom->GetForeignData();
	if (!pmu0)
		return pStatObj[0];

	if (!pStatObj[0])
	{
		(pStatObj[0] = CreateStatObj());//->AddRef();
		pStatObj[0]->SetFlags(STATIC_OBJECT_CLONE | STATIC_OBJECT_GENERATED);
		((CStatObj*)pStatObj[0])->m_bTmpIndexedMesh = true;
	}
	else if (!(pStatObj[0]->GetFlags() & STATIC_OBJECT_CLONE))
	{
		if (pStatObj[0]->GetFlags() & STATIC_OBJECT_CANT_BREAK)
		{
			CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR, "Error: breaking StatObj unsuitable for breaking (%s)", pStatObj[0]->GetFilePath());
			return pStatObj[0];
		}
		(pObjClone = CreateStatObj());//->AddRef();
		if (!pStatObj[0]->GetIndexedMesh(true) || !pObjClone->GetIndexedMesh(true))
		{
			pObjClone->Release();
			CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR, "Error: breaking StatObj without a mesh (%s)", pStatObj[0]->GetFilePath());
			pStatObj[0]->SetFlags(pStatObj[0]->GetFlags() | STATIC_OBJECT_CANT_BREAK);
			return pStatObj[0];
		}
		pObjClone->GetIndexedMesh(true)->GetMesh()->CopyFrom(*(pMesh[0] = pStatObj[0]->GetIndexedMesh(true)->GetMesh()));
		pObjClone->SetMaterial(pStatObj[0]->GetMaterial());
		pObjClone->SetFilePath(pStatObj[0]->GetFilePath());
		pStatObj[0]->CopyFoliageData(pObjClone, false, pSrcFoliage);
		pObjClone->SetFlags(pStatObj[0]->GetFlags() | STATIC_OBJECT_CLONE | STATIC_OBJECT_GENERATED);
		pStatObj[0] = pObjClone;
	}
	pPhysGeom->SetForeignData(pStatObj[0], 0);
	if (gEnv->IsDedicated())
		return pStatObj[0];
	if (!(pIndexedMesh[0] = pStatObj[0]->GetIndexedMesh(true)))
	{
		CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR, "Error: breaking StatObj without a mesh (%s)", pStatObj[0]->GetFilePath());
		pStatObj[0]->SetFlags(pStatObj[0]->GetFlags() | STATIC_OBJECT_CANT_BREAK);
		return pStatObj[0];
	}
	pPhysGeom->Lock();
	pPhysMesh = (mesh_data*)pPhysGeom->GetData();
	pMesh[0] = pIndexedMesh[0]->GetMesh();
	assert(pMesh[0]->m_pPositionsF16 == 0);
	bHasColors[0] = pMesh[0]->m_pColor0 != 0;
	nNewVtx = 0;
	pVtx.ptr[0] = pMesh[0]->m_pPositions;
	pNormals[0].ptr[0] = pMesh[0]->m_pNorms;
	pTexCoord[0].ptr[0] = pMesh[0]->m_pTexCoord;
	pTangents[0].ptr[0] = pMesh[0]->m_pTangents;
	pColors[0].ptr[0] = pMesh[0]->m_pColor0;
	pVtxMap.ptr[0] = pPhysMesh->pVtxMap;
	memset(pVtxWeld = new int[pPhysMesh->nVertices], -1, pPhysMesh->nVertices * sizeof(int));
	memset(pRemovedVtxMask = new unsigned int[((pPhysMesh->nVertices - 1) >> 5) + 1], 0, (((pPhysMesh->nVertices - 1) >> 5) + 1) * sizeof(int));
	pVtxMap.ptr[1] = 0;
	pVtx.ptr[1] = 0;
	BBox.Reset();

	nTris0 = pMesh[0]->GetIndexCount() / 3;
	// if filling a new mesh, reserve space for all source mesh indices, since we might copy them later as non-physical parts
	if (nTris0 == 0 && (pStatObj[1] = (IStatObj*)pmu0->pMesh[1]->GetForeignData()) && (pIndexedMesh[1] = pStatObj[1]->GetIndexedMesh(true)))
		nTris0 = pIndexedMesh[1]->GetMesh()->GetIndexCount() / 3;
	((CStatObj*)pStatObj[0])->m_bTmpIndexedMesh = true;
	for (pmu = pmu0, itri = 0; pmu; pmu = pmu->next)
	{
		nTris0 += pmu->nNewTri;
		for (i = 0; i < pmu->nNewTri; i++)
			itri = max(itri, pmu->pNewTri[i].idxNew - BOP_NEWIDX0);
		if (pMesh[0]->GetIndexCount() && pmu->nNewTri > 0) // don't release idxMesh now, as there probably will be a subsequent update
			((CStatObj*)pStatObj[0])->m_bTmpIndexedMesh = false;
		if (pmu == pLastUpdate)
			break;
	}

	// reserve space for the new vertices	(take into account even those that were added after pLastUpdate)
	nVtx0 = InjectVertices(pMesh[0], 0, ((CStatObj*)pStatObj[0])->m_pBoneMapping);
	InjectVertices(pMesh[0], nNewPhysVtx = max(0, pPhysMesh->nVertices - nVtx0), ((CStatObj*)pStatObj[0])->m_pBoneMapping);

	pVtx.ptr[0] = pMesh[0]->m_pPositions;
	pNormals[0].ptr[0] = pMesh[0]->m_pNorms;
	pTexCoord[0].ptr[0] = pMesh[0]->m_pTexCoord;
	pTangents[0].ptr[0] = pMesh[0]->m_pTangents;
	pColors[0].ptr[0] = pMesh[0]->m_pColor0;
	pColors[0].ptr[1] = &clrDummy;

	nTris0 = max(nTris0, itri + 1);
	piTri2Idx = new int[nTris0];
	pSubsets = new int[nTris0];
	pIdx2iTri = (new int[nTris0 + 1]) + 1;
	for (i = 0; i < nTris0; i++)
		piTri2Idx[i] = i, pSubsets[i] = 1 << 30;
	pIdx2iTri[-1] = 0;
	for (i = 0; i < (int)pMesh[0]->m_subsets.size(); i++)
		for (j = pMesh[0]->m_subsets[i].nFirstIndexId / 3; j*3 < pMesh[0]->m_subsets[i].nFirstIndexId + pMesh[0]->m_subsets[i].nNumIndices; j++)
			pSubsets[j] = i + (0x1000 & ~-iszero(pMesh[0]->m_subsets[i].nPhysicalizeType - PHYS_GEOM_TYPE_DEFAULT));
	pMaterial[0] = pStatObj[0]->GetMaterial();

	for (pmu = pmu0; pmu; pmu = pmu->next)
	{
		pStatObj[1] = (IStatObj*)pmu->pMesh[1]->GetForeignData();
		if (!pStatObj[1] || !(pIndexedMesh[1] = pStatObj[1]->GetIndexedMesh(true)))
		{
			pStatObj[0]->FreeIndexedMesh(); // to prevent further problems - we can't recover in this case
			pPhysGeom->SetForeignData(0, DATA_MESHUPDATE);
			pStatObj[0]->SetFlags(pStatObj[0]->GetFlags() | STATIC_OBJECT_CANT_BREAK);
			goto ExitUDS;
		}
		if (pStatObj[1]->GetFlags() & STATIC_OBJECT_CANT_BREAK)
		{
			CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR, "Error: breaking StatObj unsuitable for breaking (%s)", pStatObj[1]->GetFilePath());
			pPhysGeom->SetForeignData(0, DATA_MESHUPDATE);
			pStatObj[0]->SetFlags(pStatObj[0]->GetFlags() | STATIC_OBJECT_CANT_BREAK);
			goto ExitUDS;
		}
		if (pStatObj[1] && pStatObj[1] != pStatObj[0])
		{
			((CStatObj*)pStatObj[0])->m_pLastBooleanOp = pStatObj[1];
			((CStatObj*)pStatObj[0])->m_lastBooleanOpScale = pmu->relScale;
		}
		if (pmu->pMesh[1] != pmu->pMesh[0])
			pmu->pMesh[1]->Lock(0);
		pMesh[1] = pIndexedMesh[1]->GetMesh();
		//		pPhysMesh1 = (mesh_data*)pmu->pMesh[1]->GetData();
		pNormals[1].ptr[0] = pMesh[1]->m_pNorms;
		pTexCoord[1].ptr[0] = pMesh[1]->m_pTexCoord;
		pTangents[1].ptr[0] = pMesh[1]->m_pTangents;
		bHasColors[1] = (pColors[1].ptr[0] = pMesh[1]->m_pColor0) != 0;

		nTris0 = pMesh[0]->GetIndexCount() / 3;
		pMesh[1] = pIndexedMesh[1]->GetMesh();
		assert(pMesh[1]->m_pPositionsF16 == 0);
		// preserve previous indixes and subset ids, because new tris can overwrite tri slots before we need them
		pIdx0 = new vtx_idx[pMesh[0]->GetIndexCount()];
		memcpy(pIdx0, pMesh[0]->m_pIndices, pMesh[0]->GetIndexCount() * sizeof(pIdx0[0]));
		int subset0 = 0, subsBmask = 0, * pSubsetsB = &subset0;

		if (pMesh[0]->GetIndexCount())
		{
			if ((j = max(1, pStatObj[1]->GetMaterial()->GetSubMtlCount())) > 1)
			{
				pSubsetsB = new int[pMesh[1]->GetIndexCount() / 3];
				subsBmask = -1;
			}
			for (--j; j >= 0; j--)
			{
				pMaterial[1] = pStatObj[1]->GetMaterial()->GetSafeSubMtl(j);
				matname = pMaterial[1]->GetName();
				for (i = pMaterial[0]->GetSubMtlCount() - 1; i >= 0 && strcmp(pMaterial[0]->GetSafeSubMtl(i)->GetName(), matname); i--)
					;
				if (i < 0)
				{
					// append material slot to the destination material
					i = pMaterial[0]->GetSubMtlCount();
					pMaterial[0]->SetSubMtlCount(i + 1);
					pMaterial[0]->SetSubMtl(i, pMaterial[1]);
					iSubsetB = -1;
				}
				else    // find the subset that uses this slot
					for (iSubsetB = pMesh[0]->m_subsets.size() - 1; iSubsetB >= 0 && pMesh[0]->m_subsets[iSubsetB].nMatID != i; iSubsetB--)
						;
				if (iSubsetB < 0)
				{
					// append the slot that uses this material
					mss = pMesh[1]->m_subsets[0];
					mss.nFirstVertId = mss.nFirstIndexId = 0;
					mss.nNumIndices = mss.nNumVerts = 0;
					mss.nMatID = i;
					iSubsetB = pMesh[0]->m_subsets.size();
					pMesh[0]->m_subsets.push_back(mss);
				}
				for (i = 0; i < pMesh[1]->m_subsets.size(); i++)
					if (pMesh[1]->m_subsets[i].nMatID == j)
						for (int itri0 = pMesh[1]->m_subsets[i].nFirstIndexId / 3, itri = itri0, nTris = pMesh[1]->m_subsets[i].nNumIndices / 3; itri < itri0 + nTris; pSubsetsB[itri++ & subsBmask] = iSubsetB)
							;
			}
			subset0 = iSubsetB;
			pSubsets0 = new int[nTris0];
			memcpy(pSubsets0, pSubsets, nTris0 * sizeof(pSubsets0[0]));
			bEmptyObj = 0;
		}
		else
		{
			// if we are creating an object from scratch, always copy materials and subsets verbatim from pMesh[1]
			pStatObj[0]->SetMaterial(pStatObj[1]->GetMaterial());

			pSubsets0 = new int[pMesh[1]->GetIndexCount() / 3];
			for (i = pMesh[1]->GetIndexCount() / 3 - 1; i >= 0; i--)
				pSubsets0[i] = 1 << 30;
			for (i = 0; i < (int)pMesh[1]->m_subsets.size(); i++)
			{
				pMesh[0]->m_subsets.push_back(pMesh[1]->m_subsets[i]);
				pMesh[0]->m_subsets[i].nFirstIndexId = pMesh[0]->m_subsets[i].nFirstVertId = 0;
				pMesh[0]->m_subsets[i].nNumVerts = pMesh[0]->m_subsets[i].nNumIndices = 0;
				for (j = pMesh[1]->m_subsets[i].nFirstIndexId / 3; j*3 < pMesh[1]->m_subsets[i].nFirstIndexId + pMesh[1]->m_subsets[i].nNumIndices; j++)
					pSubsets0[j] = i;
			}
			bEmptyObj = 1;
			//bHasColors[0] |= bHasColors[1];
		}
		if (!pColors[0].ptr[0]) pColors[0].ptr[0] = &clrDummy;
		if (!pColors[1].ptr[0]) pColors[1].ptr[0] = &clrDummy;

		// reserve space for the added triangles
		pMesh[0]->SetIndexCount(pMesh[0]->GetIndexCount() + max(0, pmu->nNewTri - pmu->nRemovedTri) * 3);

		for (i = 0; i < pmu->nNewVtx; i++)
		{
			// copy positions of the new vertices from physical vertices (they can also be shared)
			pVtx[pmu->pNewVtx[i].idx] = pPhysMesh->pVertices[pmu->pNewVtx[i].idx];

			pNormals[0][pmu->pNewVtx[i].idx] = SMeshNormal(Vec3(0, 0, 1));
			pTexCoord[0][pmu->pNewVtx[i].idx] = SMeshTexCoord(0, 0);
			pTangents[0][pmu->pNewVtx[i].idx] = SMeshTangents(Vec4sf(0, 0, 0, 0), Vec4sf(0, 0, 0, 0));
			pColors[0][pmu->pNewVtx[i].idx & - bHasColors[0]] = clrDummy;
		}

		// mark removed vertices from pmu->pRemovedVtx, make new render vertices that map to them map to themselves
		for (i = 0; i < pmu->nRemovedVtx; i++)
			set_mask(pRemovedVtxMask, pmu->pRemovedVtx[i]);
		{
			const int vertexCount = pMesh[0]->GetVertexCount();
			for (i = 0; i < nNewVtx; i++)
				if (check_mask(pRemovedVtxMask, pVtxMap.ptr[1][i]))
					pVtxMap.ptr[1][i] = vertexCount + i;
		}
		for (i = 0; i < pmu->nRemovedVtx; i++)
			clear_mask(pRemovedVtxMask, pmu->pRemovedVtx[i]);

		if (pmu->nWeldedVtx)
		{
			// for welded vertices (which result from mesh filtering) and those that map to them, just force positions to be the same as destinations
			for (i = 0; i < pmu->nWeldedVtx; i++)
				pVtxWeld[pmu->pWeldedVtx[i].ivtxWelded] = pmu->pWeldedVtx[i].ivtxDst;
			for (i = 0; i < nNewVtx; i++)
				if ((j = pVtxMap.ptr[1][i]) < pPhysMesh->nVertices && pVtxWeld[j] >= 0 && j != pVtxWeld[j])
					pVtx.ptr[1][i] = pVtx[pVtxMap.ptr[1][i] = pVtxWeld[j]];
			/*for(i=0; i<pMesh[0]->m_nIndexCount; i++) if ((j=pVtxMap[j1=pMesh[0]->m_pIndices[i]])<pPhysMesh->nVertices)
			   {
			   for(; pVtxWeld[j]>=0; j=pVtxWeld[j]);
			   if (pVtxMap[j1]!=j || j1<pPhysMesh->nVertices && pVtxWeld[j1]>=0)
			   {
			    pVtxMap[pMesh[0]->m_pIndices[i]] = j;
			    pVtx[pMesh[0]->m_pIndices[i]] = pVtx[j];
			   }
			   }*/
			for (i = 0; i < pmu->nWeldedVtx; i++)
				pVtxWeld[pmu->pWeldedVtx[i].ivtxWelded] = -1;
		}

		for (i = pmu->nNewTri; i < pmu->nRemovedTri; i++)
		{
			// mark unused removed slots as deleted (the list will be compacted later)
			itri = mapiTri(pmu->pRemovedTri[i], pIdx2iTri); // remap if it's a generated idx from previous updates
			pSubsets[itri] = 1 << 30;
		}
		// remove non-physical subsets marked for removal (they were already attached to a child chunk of this mesh)
		for (i = 0; i < (int)pMesh[0]->m_subsets.size(); i++)
			if (pMesh[0]->m_subsets[i].nMatFlags & 1 << 30)
			{
				pMesh[0]->m_subsets[i].nMatFlags &= ~(1 << 30);
				for (j = pMesh[0]->m_subsets[i].nFirstIndexId / 3; j*3 < pMesh[0]->m_subsets[i].nFirstIndexId + pMesh[0]->m_subsets[i].nNumIndices; j++)
					pSubsets[j] = 1 << 30;
			}

		for (i = 0; i < pmu->nNewTri; i++)
		{
			if (i < pmu->nRemovedTri)
				itri = mapiTri(pmu->pRemovedTri[i], pIdx2iTri);
			else
				itri = nTris0 + i - pmu->nRemovedTri;
			piTri2Idx[itri] = pmu->pNewTri[i].idxNew;
			pIdx2iTri[pmu->pNewTri[i].idxNew - BOP_NEWIDX0] = itri;
			itriOrg = mapiTri(pmu->pNewTri[i].idxOrg, pIdx2iTri);
			iop = pmu->pNewTri[i].iop;

			for (j = 0; j < 3; j++)
			{
				if (pmu->pNewTri[i].iVtx[j] >= 0 && iop == 0)
				{
					// if an original vertex, find a vertex in the original triangle that maps to it and use it
					//for(j1=0; j1<2 && pVtxMap[pIdx0[itriOrg*3+j1]]!=pmu->pNewTri[i].iVtx[j]; j1++);
					for (j1 = 0; j1 < 3; j1++)
						dist[j1] = (pVtx[pIdx0[itriOrg * 3 + j1]] - pVtx[pmu->pNewTri[i].iVtx[j]]).len2();
					j1 = idxmin3(dist);
					pMesh[0]->m_pIndices[itri * 3 + j] = pIdx0[itriOrg * 3 + j1];
				}
				else
				{
					if ((ivtxNew = -pmu->pNewTri[i].iVtx[j] - 1) < 0 || pmu->pNewVtx[ivtxNew].iBvtx < 0 || iop == 0)
					{
						// for intersection vertices - interpolate tex coords, tangent basis
						area = pmu->pNewTri[i].area[j] / pmu->pNewTri[i].areaOrg;
						pIdx = iop == 0 ? pIdx0 : pMesh[1]->m_pIndices;
						for (j1 = 0, Coord.zero(), Tangent.zero(), Bitangent.zero(), Normal.zero(), Color.zero(); j1 < 3; j1++)
						{
							Vec2 uv;
							Vec3 t, b, n;
							Vec4 c;

							pTexCoord[iop][pIdx[itriOrg * 3 + j1]].GetUV(uv);
							pTangents[iop][pIdx[itriOrg * 3 + j1]].GetTB(t, b);
							pNormals[iop][pIdx[itriOrg * 3 + j1]].GetN(n);
							pColors[iop][pIdx[itriOrg * 3 + j1] & - bHasColors[iop]].GetRGBA(c);

							Coord += uv * area[j1];
							Tangent += t * area[j1];
							Bitangent += b * area[j1];
							Normal += n * area[j1];
							Color += c * area[j1];
						}

						Tangent.Normalize();
						Bitangent -= Tangent * (Tangent * Bitangent);
						Bitangent.Normalize();
						Normal.Normalize();

						pTangents[iop][pIdx[itriOrg * 3]].GetR(NormalFlip);
					}
					else
					{
						// for B vertices - copy data from the original tringle's vertex that maps to it
						//for(j1=0; j1<2 && pPhysMesh1->pVtxMap[pMesh[1]->m_pIndices[itriOrg*3+j1]]!=pmu->pNewVtx[ivtxNew].iBvtx; j1++);
						for (j1 = 0; j1 < 3; j1++)
							dist[j1] = (pMesh[1]->m_pPositions[pMesh[1]->m_pIndices[itriOrg * 3 + j1]] - pMesh[1]->m_pPositions[pmu->pNewVtx[ivtxNew].iBvtx]).len2();
						j1 = idxmin3(dist);

						pTexCoord[1][pMesh[1]->m_pIndices[itriOrg * 3 + j1]].GetUV(Coord);
						pTangents[1][pMesh[1]->m_pIndices[itriOrg * 3 + j1]].GetTB(Tangent, Bitangent);
						pNormals[1][pMesh[1]->m_pIndices[itriOrg * 3 + j1]].GetN(Normal);
						pColors[1][pMesh[1]->m_pIndices[itriOrg * 3 + j1] & - bHasColors[iop]].GetRGBA(Color);

						pTangents[1][pMesh[1]->m_pIndices[itriOrg * 3 + j1]].GetR(NormalFlip);
					}

					j1 = 0;
					ivtxRef = pmu->pNewTri[i].iVtx[j] >= 0 ? pmu->pNewTri[i].iVtx[j] : pmu->pNewVtx[ivtxNew].idx;

					// if s and t are close enough to another render vertex generated for this new vertex (if any), use it
					if (ivtxNew >= 0 && (j1 = -pmu->pNewVtx[ivtxNew].idxTri[iop] - 1) >= 0 && pTexCoord[0][j1].IsEquivalent(Coord, 0.003f) && pTangents[0][j1].IsEquivalent(Tangent, Bitangent, NormalFlip, 0.01f))
					{
						ivtx = j1;
					}
					else
					{
						if (!iop && j1 < 0) // if iop==0 and this new vtx wasn't used yet, use the vertex slot pmu.pNewVtx[].idx,
							ivtx = pmu->pNewVtx[ivtxNew].idx;
						else
						{
							// else generate a new vertex slot
							AllocVtx;
							pVtxMap[ivtx] = ivtxRef;
						}

						BBox.Add(pVtx[ivtx] = pMesh[0]->m_pPositions[ivtxRef]);
						if (ivtxNew >= 0) // store an index of the generated slot here
							pmu->pNewVtx[ivtxNew].idxTri[iop] = -(ivtx + 1);

						Quat qrot = Quat(IDENTITY) * (1.0f - iop) + pmu->relRot * (float)iop;
						int flip = 1 - (iop & isneg(-pmu->relScale)) * 2;

						pTexCoord[0][ivtx] = SMeshTexCoord(Coord);
						pTangents[0][ivtx] = SMeshTangents(qrot * Tangent, qrot * Bitangent, NormalFlip * (int16)flip);
						pNormals[0][ivtx] = SMeshNormal(Normal * float(flip));
						pColors[0][ivtx & - bHasColors[0]] = SMeshColor(Color);
					}

					pMesh[0]->m_pIndices[itri * 3 + j] = ivtx;
				}
			}

			pSubsets[itri] = (iop ^ 1 | bEmptyObj) ? pSubsets0[itriOrg] : pSubsetsB[itriOrg & subsBmask];
		}
		if (pSubsetsB != &subset0) delete[] pSubsetsB;

		for (i = 0; i < pmu->nTJFixes; i++)
		{
			// T-Junction fixes:
			// A _____J____ C	 (ACJ is a thin triangle on top of ABC; J is 'the junction vertex')
			//   \		.	  /		 in ABC: set A->Jnew
			//		 \	. /			 in ACJ: set J->Jnew, A -> A from original ABC, C -> B from original ABC
			//			 \/
			//			 B
			int iACJ, iABC, iAC, iCA;

			iACJ = mapiTri(pmu->pTJFixes[i].iACJ, pIdx2iTri);
			iCA = pmu->pTJFixes[i].iCA;
			iABC = mapiTri(pmu->pTJFixes[i].iABC, pIdx2iTri);
			iAC = pmu->pTJFixes[i].iAC;

			// allocate a new junction vtx (Jnew) and calculate tex coords for it by projecting J on AC in ABC
			Vec3 v = ((pVtx[pIdx0[iABC * 3 + inc_mod3[iCA]]] - pVtx[pIdx0[iABC * 3 + iCA]]));
			float k = ((pVtx[pIdx0[iACJ * 3 + dec_mod3[iAC]]] - pVtx[pIdx0[iABC * 3 + iCA]]) * v) / (v.len2() + 1e-5f);

			AllocVtx;

			{
				Vec2 uv[2];
				Vec3 t[2], b[2], n[2];
				Vec4 c[2];

				pTexCoord[0][pIdx0[iABC * 3 + iCA]].GetUV(uv[0]);
				pTangents[0][pIdx0[iABC * 3 + iCA]].GetTB(t[0], b[0]);
				pNormals[0][pIdx0[iABC * 3 + iCA]].GetN(n[0]);
				pColors[0][pIdx0[iABC * 3 + iCA] & - bHasColors[0]].GetRGBA(c[0]);

				pTexCoord[0][pIdx0[iABC * 3 + inc_mod3[iCA]]].GetUV(uv[1]);
				pTangents[0][pIdx0[iABC * 3 + inc_mod3[iCA]]].GetTB(t[1], b[1]);
				pNormals[0][pIdx0[iABC * 3 + inc_mod3[iCA]]].GetN(n[1]);
				pColors[0][pIdx0[iABC * 3 + inc_mod3[iCA]] & - bHasColors[1]].GetRGBA(c[1]);

				Coord = uv[0] * (1 - k) + uv[1] * k;
				Tangent = t[0] * (1 - k) + t[1] * k;
				Bitangent = b[0] * (1 - k) + b[1] * k;
				Normal = n[0] * (1 - k) + n[1] * k;
				Color = c[0] * (1 - k) + c[1] * k;
			}

			pTangents[0][pIdx0[iABC * 3]].GetR(NormalFlip);

			pVtx[ivtx] = pMesh[0]->m_pPositions[pmu->pTJFixes[i].iTJvtx];

			Tangent.Normalize();
			(Bitangent -= Tangent * (Tangent * Bitangent)).Normalize();

			pTexCoord[0][ivtx] = SMeshTexCoord(Coord);
			pTangents[0][ivtx] = SMeshTangents(Tangent, Bitangent, NormalFlip);
			pNormals[0][ivtx] = SMeshNormal(Normal);
			pColors[0][ivtx & - bHasColors[0]] = SMeshColor(Color);

			pVtxMap[ivtx] = pVtxMap[pmu->pTJFixes[i].iTJvtx];
			pMesh[0]->m_pIndices[iACJ * 3 + inc_mod3[iAC]] = pMesh[0]->m_pIndices[iABC * 3 + dec_mod3[iCA]];
			pMesh[0]->m_pIndices[iACJ * 3 + iAC] = pMesh[0]->m_pIndices[iABC * 3 + inc_mod3[iCA]];
			pMesh[0]->m_pIndices[iABC * 3 + inc_mod3[iCA]] = ivtx;
			pMesh[0]->m_pIndices[iACJ * 3 + dec_mod3[iAC]] = ivtx;
		}

		// check if the source object has non-physicalized triangles, move them to this object if needed
		if (bEmptyObj)
		{
			int* pUsedVtx = 0, i1;

			for (i = iop = 0; i < pMesh[1]->m_subsets.size() - 1; i++)
				if (pMesh[1]->m_subsets[i].nPhysicalizeType == PHYS_GEOM_TYPE_NONE && !(pMesh[1]->m_subsets[i].nMatFlags & (MTL_FLAG_NODRAW | 1 << 30)))
					if (pMesh[1]->m_subsets[i].nMatFlags & MTL_FLAG_HIDEONBREAK)
						pMesh[1]->m_subsets[i].nMatFlags |= 1 << 30;
					else
						for (ibox = 0; ibox < pmu->nMovedBoxes; ibox++)
						{
							Vec3 n = pmu->pMovedBoxes[ibox].size - (pmu->pMovedBoxes[ibox].Basis * (pMesh[1]->m_subsets[i].vCenter - pmu->pMovedBoxes[ibox].center)).abs();
							if (min(min(n.x, n.y), n.z) > 0) // the subset's center is inside an obstruct box that was attached to this mesh
							{
								if (!pUsedVtx)
								{
									pUsedVtx = new int[pMesh[1]->GetVertexCount()];
									memset(pUsedVtx, -1, pMesh[1]->GetVertexCount() * sizeof(pUsedVtx[0]));
								}
								ivtxNew = pMesh[0]->GetVertexCount();

								pMesh[0]->ReallocStream(CMesh::POSITIONS, ivtxNew + pMesh[1]->m_subsets[i].nNumVerts);
								pMesh[0]->ReallocStream(CMesh::NORMALS, ivtxNew + pMesh[1]->m_subsets[i].nNumVerts);
								pMesh[0]->ReallocStream(CMesh::TEXCOORDS, ivtxNew + pMesh[1]->m_subsets[i].nNumVerts);
								pMesh[0]->ReallocStream(CMesh::TANGENTS, ivtxNew + pMesh[1]->m_subsets[i].nNumVerts);
								if (bHasColors[0])
									pMesh[0]->ReallocStream(CMesh::COLORS_0, ivtxNew + pMesh[1]->m_subsets[i].nNumVerts);

								pMesh[0]->m_subsets[i].nFirstVertId = ivtxNew;
								itri = (pMesh[0]->m_subsets[i].nFirstIndexId = pMesh[0]->GetIndexCount()) / 3;
								pMesh[0]->m_subsets[i].nNumIndices = pMesh[1]->m_subsets[i].nNumIndices;
								pMesh[0]->SetIndexCount(pMesh[0]->GetIndexCount() + pMesh[1]->m_subsets[i].nNumIndices);

								for (itriOrg = (j = pMesh[1]->m_subsets[i].nFirstIndexId) / 3; j < pMesh[1]->m_subsets[i].nFirstIndexId + pMesh[1]->m_subsets[i].nNumIndices;
								     j += 3, itriOrg++, itri++)
								{
									for (i1 = 0; i1 < 3; i1++)
									{
										if ((ivtx = pUsedVtx[j1 = pMesh[1]->m_pIndices[j + i1]]) < 0)
										{
											ivtx = ivtxNew++;
											pMesh[0]->m_subsets[i].nNumVerts++;
											pMesh[0]->m_pPositions[ivtx] = pMesh[1]->m_pPositions[j1];
											pMesh[0]->m_pNorms[ivtx] = pMesh[1]->m_pNorms[j1];
											pMesh[0]->m_pTexCoord[ivtx] = pMesh[1]->m_pTexCoord[j1];
											pMesh[0]->m_pTangents[ivtx] = pMesh[1]->m_pTangents[j1];
											if (bHasColors[0])
												pMesh[0]->m_pColor0[ivtx] = pMesh[1]->m_pColor0[j1];
											pUsedVtx[j1] = ivtx;
										}
										pMesh[0]->m_pIndices[itri * 3 + i1] = ivtx;
									}
									pSubsets[itri] = pSubsets0[itriOrg];
								}

								pMesh[1]->m_subsets[i].nMatFlags |= 1 << 30; // tell pMesh[1] to remove its non-physical triangles on its next update
								IRenderMesh* pRndMesh;
								if (pStatObj[1] && (pRndMesh = pStatObj[1]->GetRenderMesh()))
									pRndMesh->GetChunks()[i].m_nMatFlags |= MTL_FLAG_NODRAW;
								iop++;
								break;
							}
						}
			if (iop)
			{
				pStatObj[1]->CopyFoliageData(pStatObj[0], true, 0, pUsedVtx, pmu->pMovedBoxes, pmu->nMovedBoxes); // move foliage data to the new stat object
				((CStatObj*)pStatObj[0])->SetPhysGeom(((CStatObj*)pStatObj[1])->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_OBSTRUCT], PHYS_GEOM_TYPE_OBSTRUCT);
				((CStatObj*)pStatObj[1])->SetPhysGeom(0, PHYS_GEOM_TYPE_OBSTRUCT);
			}
			pStatObj[0]->SetFilePath(pStatObj[1]->GetFilePath());
			delete[] pUsedVtx;
		}

		if (pmu->pMesh[1] != pmu->pMesh[0])
			pmu->pMesh[1]->Unlock(0);

		delete[] pSubsets0;
		delete[] pIdx0;
		//if (pStatObj[0]!=pStatObj[1])
		//	((CStatObj*)pStatObj[1])->ReleaseIndexedMesh();

		if (pmu == pLastUpdate)
			break;
	}

	if (((CStatObj*)pStatObj[0])->m_hasClothTangentsData && (!pStatObj[0]->GetCloneSourceObject() ||
	                                                         ((CStatObj*)pStatObj[0]->GetCloneSourceObject())->m_pClothTangentsData != ((CStatObj*)pStatObj[0])->m_pClothTangentsData))
		delete[] ((CStatObj*)pStatObj[0])->m_pClothTangentsData;
	((CStatObj*)pStatObj[0])->m_hasClothTangentsData = 0;
	if (pmu)
	{
		pPhysGeom->SetForeignData(pmu->next, DATA_MESHUPDATE);
		pmu->next = 0;
	}
	else
		pPhysGeom->SetForeignData(0, DATA_MESHUPDATE);

	meshAppend.m_streamSize[CMesh::POSITIONS] = meshAppend.m_streamSize[CMesh::NORMALS] =
	                                              meshAppend.m_streamSize[CMesh::TEXCOORDS] = meshAppend.m_streamSize[CMesh::TANGENTS] = nNewVtx;
	if (meshAppend.m_pColor0)
		meshAppend.m_streamSize[CMesh::COLORS_0] = nNewVtx;
	//pMesh[0]->Append(meshAppend);
	InjectVertices(pMesh[0], nNewVtx, ((CStatObj*)pStatObj[0])->m_pBoneMapping);
	for (i = 0; i < CMesh::LAST_STREAM; i++)
		if (meshAppend.m_streamSize[i])
		{
			void* pSrcStream = 0, * pTrgStream = 0;
			int nElementSize = 0;
			meshAppend.GetStreamInfo(i, pSrcStream, nElementSize);
			pMesh[0]->GetStreamInfo(i, pTrgStream, nElementSize);
			if (pSrcStream && pTrgStream)
				memcpy((char*)pTrgStream + (nVtx0 + nNewPhysVtx) * nElementSize, pSrcStream, meshAppend.m_streamSize[i] * nElementSize);
		}

	// sort the faces by subsetId
	qsort(pSubsets, pMesh[0]->m_pIndices, piTri2Idx, 0, (nTris0 = pMesh[0]->GetIndexCount() / 3) - 1);
	PREFAST_SUPPRESS_WARNING(6385);
	for (; nTris0 > 0 && pSubsets[nTris0 - 1] == 1 << 30; nTris0--)
		;
	pMesh[0]->SetIndexCount(nTris0 * 3);
	// remap indices that refer to new vertices
	{
		const int indexCount = pMesh[0]->GetIndexCount();
		for (i = 0; i < indexCount; i++)
			pMesh[0]->m_pIndices[i] = (pMesh[0]->m_pIndices[i] & ~(1 << 15)) + ((nVtx0 + nNewPhysVtx) & - int(pMesh[0]->m_pIndices[i] >> 15));
	}

	for (i = 0; i < (int)pMesh[0]->m_subsets.size(); i++)
		pMesh[0]->m_subsets[i].nFirstIndexId = pMesh[0]->m_subsets[i].nNumIndices = 0;
	{
		const int vertexCount = pMesh[0]->GetVertexCount();
		minVtx = vertexCount;
		maxVtx = 0;
		for (i = j1 = 0; i < nTris0; i++)
		{
			minVtx = min(minVtx, (int)min(min(pMesh[0]->m_pIndices[i * 3], pMesh[0]->m_pIndices[i * 3 + 1]), pMesh[0]->m_pIndices[i * 3 + 2]));
			maxVtx = max(maxVtx, (int)max(max(pMesh[0]->m_pIndices[i * 3], pMesh[0]->m_pIndices[i * 3 + 1]), pMesh[0]->m_pIndices[i * 3 + 2]));
			if (i == nTris0 - 1 || pSubsets[i + 1] != pSubsets[i])
			{
				j = pSubsets[i] & ~0x1000;
				if (pMesh[0]->m_subsets[j].nPhysicalizeType == PHYS_GEOM_TYPE_DEFAULT)
				{
					pMesh[0]->m_subsets[j].nFirstVertId = minVtx;
					pMesh[0]->m_subsets[j].nNumVerts = max(0, maxVtx - minVtx + 1);
				}
				pMesh[0]->m_subsets[j].nFirstIndexId = j1 * 3;
				pMesh[0]->m_subsets[j].nNumIndices = (i - j1 + 1) * 3;
				minVtx = vertexCount;
				maxVtx = 0;
				j1 = i + 1;
			}
		}
	}

	// request physics to remap indices and add split vertices
	for (i = 0; i < nTris0; i++)
		pIdx2iTri[i] = i;
	pPhysGeom->RemapForeignIdx(piTri2Idx, pIdx2iTri, nTris0);
	pPhysGeom->AppendVertices(pVtx.ptr[1], pVtxMap.ptr[1], nNewVtx);

#ifdef _DEBUG
	/*if (!pPhysGeom->GetForeignData(DATA_MESHUPDATE))
	   {
	   nTris0 = pMesh[0]->m_nIndexCount/3;
	   if (nTris0!=pPhysMesh->nTris)
	    i=i;
	   for(i=0;i<nTris0;i++) pSubsets[i] = -1;
	   for(i=0;i<pPhysMesh->nTris;i++)
	    pSubsets[pPhysMesh->pForeignIdx[i]] = i;
	   for(i=0;i<nTris0;i++) if (pSubsets[i]==-1)
	    i=i;
	   for(i=0;i<nTris0;i++) for(j=0;j<3;j++)
	    if (pPhysMesh->pVtxMap[pMesh[0]->m_pIndices[pPhysMesh->pForeignIdx[i]*3+j]]!=pPhysMesh->pIndices[i*3+j])
	      i=i;
	   for(i=0;i<nTris0;i++) for(j=0;j<3;j++)
	    if ((pMesh[0]->m_pPositions[pMesh[0]->m_pIndices[pPhysMesh->pForeignIdx[i]*3+j]]-pPhysMesh->pVertices[pPhysMesh->pIndices[i*3+j]]).len2()>0.0001f)
	      i=i;
	   }*/
#endif

ExitUDS:
	delete[] pVtxWeld;
	if (pVtxMap.ptr[1]) free(pVtxMap.ptr[1]);
	delete[] (pIdx2iTri - 1);
	delete[] piTri2Idx;
	delete[] pSubsets;
	delete[] pRemovedVtxMask;
	pPhysGeom->Unlock();
	delete pmu0;
	if (pStatObj[0]->GetIndexedMesh(true))
	{
		pStatObj[0]->Invalidate();
		if (!pStatObj[0]->GetRenderMesh())
		{
			pStatObj[0]->SetBBoxMax(Vec3(0));
			pStatObj[0]->SetBBoxMin(Vec3(0));
		}
		if (((CStatObj*)pStatObj[0])->m_nSpines && ((CStatObj*)pStatObj[0])->m_pSpines && pStatObj[0]->GetRenderMesh())
			pStatObj[0]->GetRenderMesh()->GenerateQTangents();
	}

	return pStatObj[0];
}

//////////////////////////////////////////////////////////////////////////
///////////////////////// Deformable Geometry ////////////////////////////
//////////////////////////////////////////////////////////////////////////

int CStatObj::SubobjHasDeformMorph(int iSubObj)
{
	int i;
	char nameDeformed[256];
	cry_strcpy(nameDeformed, m_subObjects[iSubObj].name);
	cry_strcat(nameDeformed, "_Destroyed");

	for (i = m_subObjects.size() - 1; i >= 0 && strcmp(m_subObjects[i].name, nameDeformed); i--)
		;
	return i;
}

#define getBidx(islot) _getBidx(islot, pIdxABuf, pFaceToFace0A, pFace0ToFaceB, pIdxB)
static inline int _getBidx(int islot, int* pIdxABuf, uint16* pFaceToFace0A, int* pFace0ToFaceB, vtx_idx* pIdxB)
{
	int idx, mask;
	idx = pFace0ToFaceB[pFaceToFace0A[pIdxABuf[islot] >> 2]] * 3 + (pIdxABuf[islot] & 3);
	mask = idx >> 31;
	return (pIdxB[idx & ~mask] & ~mask) + mask;
}

int CStatObj::SetDeformationMorphTarget(IStatObj* pDeformed)
{
	int i, j, k, j0, it, ivtx, nVtxA, nVtxAnew, nVtxA0, nIdxA, nFacesA, nFacesB;
	vtx_idx* pIdxA, * pIdxB;
	int* pVtx2IdxA, * pIdxABuf, * pFace0ToFaceB;
	uint16* pFaceToFace0A, * pFaceToFace0B, maxFace0;
	_smart_ptr<IRenderMesh> pMeshA, pMeshB, pMeshBnew;
	strided_pointer<Vec3> pVtxA, pVtxB, pVtxBnew;
	strided_pointer<Vec2> pTexA, pTexB, pTexBnew;
	strided_pointer<SPipTangents> pTangentsA, pTangentsB, pTangentsBnew;
	if (!GetRenderMesh())
		MakeRenderMesh();
	if (!pDeformed->GetRenderMesh())
		((CStatObj*)pDeformed)->MakeRenderMesh();
	if (!(pMeshA = GetRenderMesh()) || !(pMeshB = pDeformed->GetRenderMesh()) ||
	    !(pFaceToFace0A = m_pMapFaceToFace0) || !(pFaceToFace0B = ((CStatObj*)pDeformed)->m_pMapFaceToFace0))
		return 0;
	if (pMeshA->GetMorphBuddy())
		return 1;

	if (pMeshA->GetVerticesCount() > 0xffff || pMeshB->GetVerticesCount() > 0xffff)
		return 0;

	nVtxA0 = nVtxA = pMeshA->GetVerticesCount();
	pVtxA.data = (Vec3*)pMeshA->GetPosPtr(pVtxA.iStride, FSL_READ);
	pTexA.data = (Vec2*)pMeshA->GetUVPtr(pTexA.iStride, FSL_READ);
	pTangentsA.data = (SPipTangents*)pMeshA->GetTangentPtr(pTangentsA.iStride, FSL_READ);

	pVtxB.data = (Vec3*)pMeshB->GetPosPtr(pVtxB.iStride, FSL_READ);
	pTexB.data = (Vec2*)pMeshB->GetUVPtr(pTexB.iStride, FSL_READ);
	pTangentsB.data = (SPipTangents*)pMeshB->GetTangentPtr(pTangentsB.iStride, FSL_READ);

	nFacesB = pMeshB->GetIndicesCount();
	nFacesB /= 3;
	pIdxB = pMeshB->GetIndexPtr(FSL_READ);
	nIdxA = pMeshA->GetIndicesCount();
	nFacesA = nIdxA /= 3;
	pIdxA = pMeshA->GetIndexPtr(FSL_READ);

	memset(pVtx2IdxA = new int[nVtxA + 1], 0, (nVtxA + 1) * sizeof(int));
	for (i = 0; i < nIdxA; i++)
		pVtx2IdxA[pIdxA[i]]++;
	for (i = maxFace0 = 0; i < nFacesA; i++)
		maxFace0 = max(maxFace0, pFaceToFace0A[i]);
	for (i = 0; i < nVtxA; i++)
		pVtx2IdxA[i + 1] += pVtx2IdxA[i];
	pIdxABuf = new int[nIdxA];
	for (i = nFacesA - 1; i >= 0; i--)
		for (j = 2; j >= 0; j--)
			pIdxABuf[--pVtx2IdxA[pIdxA[i * 3 + j]]] = i * 4 + j;

	for (i = nFacesB - 1; i >= 0; i--)
		maxFace0 = max(maxFace0, pFaceToFace0B[i]);
	memset(pFace0ToFaceB = new int[maxFace0 + 1], -1, (maxFace0 + 1) * sizeof(int));
	for (i = nFacesB - 1; i >= 0; i--)
		pFace0ToFaceB[pFaceToFace0B[i]] = i;

	for (i = nVtxAnew = k = 0; i < nVtxA; i++)
	{
		for (j = pVtx2IdxA[i]; j < pVtx2IdxA[i + 1] - 1; j++)
			for (k = pVtx2IdxA[i + 1] - 1; k > j; k--)
				if (getBidx(k - 1) > getBidx(k))
					it = pIdxABuf[k - 1], pIdxABuf[k - 1] = pIdxABuf[k], pIdxABuf[k] = it;
		for (j = pVtx2IdxA[i] + 1; j < pVtx2IdxA[i + 1]; j++)
		{
			nVtxAnew += iszero(getBidx(j) - getBidx(j - 1)) ^ 1;
#ifdef _DEBUG
			if ((pVtxB[getBidx(j)] - pVtxB[getBidx(j - 1)]).len2() > sqr(0.01f))
				k++;
#endif
		}
	}

	pMeshBnew = GetRenderer()->CreateRenderMesh("StatObj_Deformed", GetFilePath());
	pMeshBnew->UpdateVertices(0, nVtxA0 + nVtxAnew, 0, VSF_GENERAL, 0u);
	if (nVtxAnew)
	{
		pMeshA = GetRenderer()->CreateRenderMesh("StatObj_MorphTarget", GetFilePath());
		m_pRenderMesh->CopyTo(pMeshA, nVtxAnew);
		pVtxA.data = (Vec3*)pMeshA->GetPosPtr(pVtxA.iStride, FSL_SYSTEM_UPDATE);
		pTexA.data = (Vec2*)pMeshA->GetUVPtr(pTexA.iStride, FSL_SYSTEM_UPDATE);
		pTangentsA.data = (SPipTangents*)pMeshA->GetTangentPtr(pTangentsA.iStride, FSL_SYSTEM_UPDATE);
		nIdxA = pMeshA->GetIndicesCount();
		pIdxA = pMeshA->GetIndexPtr(FSL_READ);
		m_pRenderMesh = pMeshA;
	}

	pVtxBnew.data = (Vec3*)pMeshBnew->GetPosPtr(pVtxBnew.iStride, FSL_SYSTEM_UPDATE);
	pTexBnew.data = (Vec2*)pMeshBnew->GetUVPtr(pTexBnew.iStride, FSL_SYSTEM_UPDATE);
	pTangentsBnew.data = (SPipTangents*)pMeshBnew->GetTangentPtr(pTangentsBnew.iStride, FSL_SYSTEM_UPDATE);

	for (i = 0; i < nVtxA0; i++)
		for (j = j0 = pVtx2IdxA[i]; j < pVtx2IdxA[i + 1]; j++)
			if (j == pVtx2IdxA[i + 1] - 1 || getBidx(j) != getBidx(j + 1))
			{
				if (j0 > pVtx2IdxA[i])
				{
					ivtx = nVtxA++;
					pVtxA[ivtx] = pVtxA[i];
					pTangentsA[ivtx] = pTangentsA[i];
					pTexA[ivtx] = pTexA[i];
					for (k = j0; k <= j; k++)
						pIdxA[(pIdxABuf[k] >> 2) * 3 + (pIdxABuf[k] & 3)] = ivtx;
				}
				else
					ivtx = i;
				if ((it = getBidx(j)) >= 0)
				{
#ifdef _DEBUG
					static float maxdist = 0.1f;
					float dist = (pVtxB[it] - pVtxA[i]).len();
					if (dist > maxdist)
						k++;
#endif
					pVtxBnew[ivtx] = pVtxB[it];
					pTangentsBnew[ivtx] = pTangentsB[it];
					pTexBnew[ivtx] = pTexB[it];
				}
				else
				{
					pVtxBnew[ivtx] = pVtxA[i];
					pTangentsBnew[ivtx] = pTangentsA[i];
					pTexBnew[ivtx] = pTexA[i];
				}
				j0 = j + 1;
			}

	pMeshA->SetMorphBuddy(pMeshBnew);
	pDeformed->SetFlags(pDeformed->GetFlags() | STATIC_OBJECT_HIDDEN);

	pMeshBnew->UnlockStream(VSF_GENERAL);
	pMeshBnew->UnlockStream(VSF_TANGENTS);
	pMeshA->UnlockStream(VSF_GENERAL);
	pMeshA->UnlockStream(VSF_TANGENTS);

	delete[] pVtx2IdxA;
	delete[] pIdxABuf;
	delete[] pFace0ToFaceB;

	return 1;
}

static inline float max_fast(float op1, float op2) { return (op1 + op2 + fabsf(op1 - op2)) * 0.5f; }
static inline float min_fast(float op1, float op2) { return (op1 + op2 - fabsf(op1 - op2)) * 0.5f; }

static void         UpdateWeights(const Vec3& pt, float r, float strength, IRenderMesh* pMesh, IRenderMesh* pWeights)
{
	int i, nVtx = pMesh->GetVerticesCount();
	float r2 = r * r, rr = 1 / r;
	strided_pointer<Vec3> pVtx;
	strided_pointer<Vec2> pWeight;
	pVtx.data = (Vec3*)pMesh->GetPosPtr(pVtx.iStride, FSL_SYSTEM_UPDATE);
	pWeight.data = (Vec2*)pWeights->GetPosPtr(pWeight.iStride, FSL_SYSTEM_UPDATE);

	if (r > 0)
	{
		for (i = 0; i < nVtx; i++)
			if ((pVtx[i] - pt).len2() < r2)
				pWeight[i].x = max_fast(0.0f, min_fast(1.0f, pWeight[i].x + strength * (1 - (pVtx[i] - pt).len() * rr)));
	}
	else
		for (i = 0; i < nVtx; i++)
			pWeight[i].x = max_fast(0.0f, min_fast(1.0f, pWeight[i].x + strength));
}

IStatObj* CStatObj::DeformMorph(const Vec3& pt, float r, float strength, IRenderMesh* pWeights)
{
	int i, j;
	CStatObj* pObj = this;

	if (!GetCVars()->e_DeformableObjects)
	{
		return pObj;
	}

	if (m_bHasDeformationMorphs)
	{
		if (!(GetFlags() & STATIC_OBJECT_CLONE))
		{
			pObj = (CStatObj*)Clone(true, false, false);
			pObj->m_bUnmergable = 1;
			for (i = pObj->GetSubObjectCount() - 1; i >= 0; i--)
				if ((j = pObj->SubobjHasDeformMorph(i)) >= 0)
				{
					pObj->GetSubObject(i)->pWeights = pObj->GetSubObject(i)->pStatObj->GetRenderMesh()->GenerateMorphWeights();
					pObj->GetSubObject(j)->pStatObj->SetFlags(pObj->GetSubObject(j)->pStatObj->GetFlags() | STATIC_OBJECT_HIDDEN);
				}
			return pObj->DeformMorph(pt, r, strength, pWeights);
		}
		for (i = m_subObjects.size() - 1; i >= 0; i--)
			if (m_subObjects[i].pWeights)
				UpdateWeights(m_subObjects[i].tm.GetInverted() * pt,
				              r * (fabs_tpl(m_subObjects[i].tm.GetColumn(0).len2() - 1.0f) < 0.01f ? 1.0f : 1.0f / m_subObjects[i].tm.GetColumn(0).len()),
				              strength, m_subObjects[i].pStatObj->GetRenderMesh(), m_subObjects[i].pWeights);
	}
	else if (m_nSubObjectMeshCount == 0 && m_pRenderMesh && m_pRenderMesh->GetMorphBuddy())
	{
		if (!pWeights)
		{
			pObj = new CStatObj;
			pObj->m_pMaterial = m_pMaterial;
			pObj->m_AABB = m_AABB;
			pObj->m_vVegCenter = m_vVegCenter;
			pObj->m_fRadiusHors = m_fRadiusHors;
			pObj->m_fRadiusVert = m_fRadiusVert;
			pObj->m_nFlags = m_nFlags | STATIC_OBJECT_CLONE;
			pObj->m_bHasDeformationMorphs = true;
			pObj->m_nSubObjectMeshCount = 1;
			pObj->m_bSharesChildren = true;
			pObj->m_subObjects.resize(1);
			pObj->m_subObjects[0].nType = STATIC_SUB_OBJECT_MESH;
			pObj->m_subObjects[0].name = "";
			pObj->m_subObjects[0].properties = "";
			pObj->m_subObjects[0].bIdentityMatrix = true;
			pObj->m_subObjects[0].tm.SetIdentity();
			pObj->m_subObjects[0].localTM.SetIdentity();
			pObj->m_subObjects[0].pStatObj = this;
			pObj->m_subObjects[0].nParent = -1;
			pObj->m_subObjects[0].helperSize = Vec3(0, 0, 0);
			pObj->m_subObjects[0].pWeights = GetRenderMesh()->GenerateMorphWeights();
			return pObj->DeformMorph(pt, r, strength, pWeights);
		}
		UpdateWeights(pt, r, strength, m_pRenderMesh, pWeights);
	}

	return this;
}

IStatObj* CStatObj::HideFoliage()
{
	int i;//,j,idx0;
	CMesh* pMesh;
	if (!GetIndexedMesh())
		return this;

	pMesh = GetIndexedMesh()->GetMesh();
	for (i = pMesh->m_subsets.size() - 1; i >= 0; i--)
		if (pMesh->m_subsets[i].nPhysicalizeType == PHYS_GEOM_TYPE_NONE)
		{
			//pMesh->m_subsets[i].nMatFlags |= MTL_FLAG_NODRAW;
			//idx0 = i>0 ? pMesh->m_subsets[i-1].nFirstIndexId+pMesh->m_subsets[i-1].nNumIndices : 0;
			pMesh->m_subsets.erase(pMesh->m_subsets.begin() + i);
			/*for(j=i;j<pMesh->m_subsets.size();j++)
			   {
			   memmove(pMesh->m_pIndices+idx0, pMesh->m_pIndices+pMesh->m_subsets[j].nFirstIndexId,
			    pMesh->m_subsets[j].nNumIndices*sizeof(pMesh->m_pIndices[0]));
			   pMesh->m_subsets[j].nFirstIndexId = idx0;
			   idx0 += pMesh->m_subsets[j].nNumIndices;
			   }
			   pMesh->m_nIndexCount = idx0;*/
		}
	Invalidate();

	return this;
}

//////////////////////////////////////////////////////////////////////////
////////////////////////   SubObjects    /////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static inline int GetEdgeByBuddy(mesh_data* pmd, int itri, int itri_buddy)
{
	int iedge = 0, imask;
	imask = pmd->pTopology[itri].ibuddy[1] - itri_buddy;
	imask = (imask - 1) >> 31 ^ imask >> 31;
	iedge = 1 & imask;
	imask = pmd->pTopology[itri].ibuddy[2] - itri_buddy;
	imask = (imask - 1) >> 31 ^ imask >> 31;
	iedge = iedge & ~imask | 2 & imask;
	return iedge;
}
static inline float qmin(float op1, float op2) { return (op1 + op2 - fabsf(op1 - op2)) * 0.5f; }
static inline float qmax(float op1, float op2) { return (op1 + op2 + fabsf(op1 - op2)) * 0.5f; }

static int __s_pvtx_map_dummy = 0;

static void SyncToRenderMesh(SSyncToRenderMeshContext* ctx, volatile int* updateState)
{
	IGeometry* pPhysGeom = ctx->pObj->GetPhysGeom() ? ctx->pObj->GetPhysGeom()->pGeom : 0;
	if (pPhysGeom)
	{
		pPhysGeom->Lock(0);
		IStatObj* pObjSrc = (IStatObj*)pPhysGeom->GetForeignData(0);
		if (pPhysGeom->GetForeignData(DATA_MESHUPDATE) || !ctx->pObj->m_hasClothTangentsData || pObjSrc != ctx->pObj && pObjSrc != ctx->pObj->GetCloneSourceObject())
		{
			// skip all updates if the mesh was altered
			if (updateState)
				CryInterlockedDecrement(updateState);
			pPhysGeom->Unlock(0);
			return;
		}
	}
	Vec3 n, edge, t;
	int i, j;
	Vec3* vmin = ctx->vmin, * vmax = ctx->vmax;
	int iVtx0 = ctx->iVtx0, nVtx = ctx->nVtx, mask = ctx->mask;
	strided_pointer<Vec3> pVtx = ctx->pVtx;
	int* pVtxMap = (mask == ~0) ? &__s_pvtx_map_dummy : ctx->pVtxMap;
	float rscale = ctx->rscale;
	SClothTangentVtx* ctd = ctx->ctd;
	strided_pointer<Vec3> pMeshVtx = ctx->pMeshVtx;
	strided_pointer<SPipTangents> pTangents = ctx->pTangents;
	strided_pointer<Vec3> pNormals = ctx->pNormals;

	AABB bbox;
	bbox.Reset();

	if (pMeshVtx)
	{
		for (i = iVtx0; i < nVtx; i++)
		{
			Vec3 v = pVtx[j = pVtxMap[i & ~mask] | i & mask] * rscale;
			bbox.Add(v);
			pMeshVtx[i] = v;
		}
		*vmin = bbox.min;
		*vmax = bbox.max;
	}

	if (pTangents)
	{
		for (i = iVtx0; i < nVtx; i++)
		{
			SMeshTangents tb(pTangents[i]);
			int16 nsg;
			tb.GetR(nsg);

			n = pNormals[j = pVtxMap[i & ~mask] | i & mask] * ctd[i].sgnNorm;
			edge = (pVtx[ctd[i].ivtxT] - pVtx[j]).normalized();
			Matrix33 M;
			crossproduct_matrix(pNormals[j] * ctd[i].edge.y, M) *= nsg;
			M += Matrix33(IDENTITY) * ctd[i].edge.x;
			t = M.GetInverted() * (edge - n * ctd[i].edge.z);
			(t -= n * (n * t)).Normalize();
			t.x = qmin(qmax(t.x, -0.9999f), 0.9999f);
			t.y = qmin(qmax(t.y, -0.9999f), 0.9999f);
			t.z = qmin(qmax(t.z, -0.9999f), 0.9999f);
			Vec3 b = n.Cross(t) * nsg;

			tb = SMeshTangents(t, b, nsg);
			tb.ExportTo(pTangents[i]);
		}
	}

	if (updateState)
		CryInterlockedDecrement(updateState);
	if (pPhysGeom)
		pPhysGeom->Unlock(0);
}

DECLARE_JOB(
  "StatObj_SyncToRenderMesh"
  , TStatObj_SyncToRenderMesh
  , SyncToRenderMesh);

static void SkinVertices(SSyncToRenderMeshContext* ctx, volatile int* ready)
{
	ctx->pObj->SkinVertices(ctx->pVtx, Matrix34(IDENTITY), ready);
}

DECLARE_JOB(
  "StatObj_SkinVertices"
  , TStatObj_SkinVertices
  , SkinVertices);

IStatObj* CStatObj::UpdateVertices(strided_pointer<Vec3> pVtx, strided_pointer<Vec3> pNormals, int iVtx0, int nVtx, int* pVtxMap, float rscale)
{
	CStatObj* pObj = this;
	if (m_pRenderMesh)
	{
		strided_pointer<Vec3> pMeshVtx;
		strided_pointer<SPipTangents> pTangents;
		int i, j, mask = 0, dummy = 0, nVtxFull;
		if (!pVtxMap)
			pVtxMap = &dummy, mask = ~0;
		AABB bbox;
		bbox.Reset();
		SClothTangentVtx* ctd;

		if (!m_hasClothTangentsData && GetPhysGeom() && GetPhysGeom()->pGeom->GetType() == GEOM_TRIMESH && m_pRenderMesh)
		{
			if (GetPhysGeom()->pGeom->GetForeignData(DATA_MESHUPDATE))
				return this;
			ctd = m_pClothTangentsData = new SClothTangentVtx[nVtxFull = m_pRenderMesh->GetVerticesCount()];
			m_hasClothTangentsData = 1;
			memset(m_pClothTangentsData, 0, sizeof(SClothTangentVtx) * nVtxFull);
			mesh_data* pmd = (mesh_data*)GetPhysGeom()->pGeom->GetData();
			m_pRenderMesh->LockForThreadAccess();
			pTangents.data = (SPipTangents*)m_pRenderMesh->GetTangentPtr(pTangents.iStride, FSL_READ);
			for (i = 0; i < pmd->nTris; i++)
				for (j = 0; j < 3; j++)
					ctd[pmd->pIndices[i * 3 + j]].ivtxT = i, ctd[pmd->pIndices[i * 3 + j]].sgnNorm = j;
			if (pmd->pVtxMap)
				for (i = 0; i < pmd->nVertices; i++)
					ctd[i].ivtxT = ctd[pmd->pVtxMap[i]].ivtxT, ctd[i].sgnNorm = ctd[pmd->pVtxMap[i]].sgnNorm;

			for (i = 0; i < nVtxFull && pTangents; i++)
			{
				j = pmd->pVtxMap ? pmd->pVtxMap[i] : i;

				SMeshTangents tb = SMeshTangents(pTangents[i]);
				Vec3 t, b, s;
				tb.GetTBN(t, b, s);

				float tedge = -1, tedgeDenom = 0;
				int itri = ctd[i].ivtxT, iedge = ctd[i].sgnNorm, itriT = 0, iedgeT = 0, itri1, loop;
				Vec3 n, edge, edge0;
				n.zero();

				for (int iter = 0; iter < 2; iter++)
				{
					for (edge0.zero(), loop = 20;; ) // iter==0 - trace cw, 1 - ccw
					{
						edge = (pmd->pVertices[pmd->pIndices[itri * 3 + inc_mod3[iedge]]] - pmd->pVertices[pmd->pIndices[itri * 3 + iedge]]) * (1 - iter * 2);
						n += (edge0 ^ edge) * (iter * 2 - 1);
						edge0 = edge;
						if (sqr(t * edge) * tedgeDenom > tedge * edge.len2())
							tedge = sqr(t * edge), tedgeDenom = edge.len2(), itriT = itri, iedgeT = iedge;
						itri1 = pmd->pTopology[itri].ibuddy[iedge];
						if (itri1 == ctd[i].ivtxT || itri1 < 0 || --loop < 0)
							break;
						iedge = GetEdgeByBuddy(pmd, itri1, itri) + 1 + iter;
						itri = itri1;
						iedge -= 3 & (2 - iedge) >> 31;
					}
					if (itri1 >= 0 && iter == 0)
						n.zero();
					itri = ctd[i].ivtxT;
					iedge = dec_mod3[ctd[i].sgnNorm];
				}
				n += pmd->pVertices[pmd->pIndices[ctd[i].ivtxT * 3 + 1]] - pmd->pVertices[pmd->pIndices[ctd[i].ivtxT * 3]] ^
				     pmd->pVertices[pmd->pIndices[ctd[i].ivtxT * 3 + 2]] - pmd->pVertices[pmd->pIndices[ctd[i].ivtxT * 3]];

				if ((ctd[i].ivtxT = pmd->pIndices[itriT * 3 + iedgeT]) == j)
					ctd[i].ivtxT = pmd->pIndices[itriT * 3 + inc_mod3[iedgeT]];
				edge = (pmd->pVertices[ctd[i].ivtxT] - pmd->pVertices[j]).normalized();

				ctd[i].edge.Set(edge * t, edge * b, edge * s);
				ctd[i].sgnNorm = sgnnz(n * s);
			}
			if (pObj != this)
			{
				memcpy(pObj->m_pClothTangentsData = new SClothTangentVtx[nVtxFull], ctd, nVtxFull * sizeof(SClothTangentVtx));
				pObj->m_hasClothTangentsData = 1;
			}
			pObj->SetFlags(pObj->GetFlags() & ~STATIC_OBJECT_CANT_BREAK);
			m_pRenderMesh->UnLockForThreadAccess();
		}

		if (!(GetFlags() & STATIC_OBJECT_CLONE) || (GetTetrLattice() || m_hasSkinInfo) && !pObj->m_pClonedSourceObject && pVtx)
		{
			pObj = (CStatObj*)Clone(true, true, false);
			pObj->m_pRenderMesh->KeepSysMesh(true);
			pObj->m_pLattice = m_pLattice;
			m_pLattice = 0;
		}

		if (!m_pAsyncUpdateContext)
			m_pAsyncUpdateContext = new SSyncToRenderMeshContext;
		gEnv->pJobManager->WaitForJob(m_pAsyncUpdateContext->jobState);

		if (GetTetrLattice() || m_hasSkinInfo)
		{
			Vec3 sz = GetAABB().GetSize();
			float szmin = min(min(sz.x, sz.y), sz.z), szmax = max(max(sz.x, sz.y), sz.z), szmed = sz.x + sz.y + sz.y - szmin - szmax;
			if (!m_hasSkinInfo)
				PrepareSkinData(Matrix34(IDENTITY), 0, min(szmin * 0.5f, szmed * 0.15f));
			if (pVtx)
			{
				if (GetCVars()->e_RenderMeshUpdateAsync)
				{
					IRenderMesh* mesh = pObj->m_pRenderMesh;
					mesh->LockForThreadAccess();
					m_pAsyncUpdateContext->pObj = pObj;
					m_pAsyncUpdateContext->pVtx = pVtx;
					TStatObj_SkinVertices job(m_pAsyncUpdateContext, mesh->SetAsyncUpdateState());
					job.RegisterJobState(&m_pAsyncUpdateContext->jobState);
					job.Run();
				}
				else
				{
					SkinVertices(pVtx, Matrix34(IDENTITY), NULL);
				}
			}
			return pObj;
		}

		if (!pVtx)
			return pObj;

		IRenderMesh* mesh = pObj->m_pRenderMesh;
		mesh->LockForThreadAccess();
		pMeshVtx.data = (Vec3*)((mesh = pObj->m_pRenderMesh)->GetPosPtr(pMeshVtx.iStride, FSL_SYSTEM_UPDATE));
		if (m_hasClothTangentsData && m_pClothTangentsData)
		{
			pTangents.data = (SPipTangents*)mesh->GetTangentPtr(pTangents.iStride, FSL_SYSTEM_UPDATE);
		}

		m_pAsyncUpdateContext->Set(&pObj->m_AABB.min, &pObj->m_AABB.max, iVtx0, nVtx, pVtx, pVtxMap, mask, rscale
		                           , m_pClothTangentsData, pMeshVtx, pTangents, pNormals, pObj);

		if (GetCVars()->e_RenderMeshUpdateAsync)
		{
			TStatObj_SyncToRenderMesh job(m_pAsyncUpdateContext, mesh->SetAsyncUpdateState());
			job.RegisterJobState(&m_pAsyncUpdateContext->jobState);
			job.Run();
		}
		else
		{
			SyncToRenderMesh(m_pAsyncUpdateContext, NULL);
			if (m_hasClothTangentsData && m_pClothTangentsData)
				mesh->UnlockStream(VSF_TANGENTS);
			mesh->UnlockStream(VSF_GENERAL);
		}

		mesh->UnLockForThreadAccess();
	}
	return pObj;
}

//////////////////////////////////////////////////////////////////////////
namespace
{
CryCriticalSection g_cPrepareSkinData;
}

void CStatObj::PrepareSkinData(const Matrix34& mtxSkelToMesh, IGeometry* pPhysSkel, float r)
{
	if (m_hasSkinInfo || pPhysSkel && pPhysSkel->GetType() != GEOM_TRIMESH)
		return;

	// protect again possible paralle calls, if streaming is here, but mainthread also reaches this function
	// before streaming thread has finished and wants to prepare data also
	AUTO_LOCK(g_cPrepareSkinData);

	// recheck again to guard again creating the object two times
	if (m_hasSkinInfo)
		return;

	m_nFlags |= STATIC_OBJECT_DYNAMIC;
	if (!m_pRenderMesh)
	{
		if (!m_pDelayedSkinParams)
		{
			m_pDelayedSkinParams = new SDelayedSkinParams;
			m_pDelayedSkinParams->mtxSkelToMesh = mtxSkelToMesh;
			m_pDelayedSkinParams->pPhysSkel = pPhysSkel;
			m_pDelayedSkinParams->r = r;
		}
		return;
	}
	m_pRenderMesh->KeepSysMesh(true);

	int i, j, nVtx;
	Vec3 vtx[4];
	geom_world_data gwd[2];
	geom_contact* pcontact;
	mesh_data* md;
	IGeometry* pSphere, * pSphereBig;
	strided_pointer<Vec3> pVtx;
	Matrix34 mtxMeshToSkel = mtxSkelToMesh.GetInverted();
	ITetrLattice* pLattice = GetTetrLattice();
	if (pLattice)
		pPhysSkel = pLattice->CreateSkinMesh();

	gwd[1].scale = mtxSkelToMesh.GetColumn0().len();
	gwd[1].offset = mtxSkelToMesh.GetTranslation();
	gwd[1].R = Matrix33(mtxSkelToMesh) * (1.0f / gwd[1].scale);
	m_pRenderMesh->GetBBox(vtx[0], vtx[1]);
	vtx[1] -= vtx[0];
	primitives::sphere sph;
	sph.center.zero();
	sph.r = r > 0.0f ? r : min(min(vtx[1].x, vtx[1].y), vtx[1].z);
	pSphere = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::sphere::type, &sph);
	sph.r *= 3.0f;
	pSphereBig = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::sphere::type, &sph);
	assert(pPhysSkel);
	PREFAST_ASSUME(pPhysSkel);
	md = (mesh_data*)pPhysSkel->GetData();
	IRenderMesh::ThreadAccessLock lockrm(m_pRenderMesh);
	pVtx.data = (Vec3*)m_pRenderMesh->GetPosPtr(pVtx.iStride, FSL_READ);
	SSkinVtx* pSkinInfo = m_pSkinInfo = new SSkinVtx[nVtx = m_pRenderMesh->GetVerticesCount()];
	m_hasSkinInfo = 1;

	for (i = 0; i < nVtx; i++)
	{
		Vec3 v = pVtx[i];
		if (pSkinInfo[i].bVolumetric = (pLattice && pLattice->CheckPoint(mtxMeshToSkel * v, pSkinInfo[i].idx, pSkinInfo[i].w)))
			pSkinInfo[i].M = Matrix33(md->pVertices[pSkinInfo[i].idx[1]] - md->pVertices[pSkinInfo[i].idx[0]],
			                          md->pVertices[pSkinInfo[i].idx[2]] - md->pVertices[pSkinInfo[i].idx[0]],
			                          md->pVertices[pSkinInfo[i].idx[3]] - md->pVertices[pSkinInfo[i].idx[0]]).GetInverted();
		else
		{
			gwd[0].offset = v;
			{
				WriteLockCond locki;
				if (pSphere->IntersectLocked(pPhysSkel, gwd, gwd + 1, 0, pcontact, locki) ||
				    pSphereBig->IntersectLocked(pPhysSkel, gwd, gwd + 1, 0, pcontact, locki))
				{
					for (j = 0; j < 3; j++)
						vtx[j] = mtxSkelToMesh * md->pVertices[pSkinInfo[i].idx[j] = md->pIndices[pcontact->iPrim[1] * 3 + j]];
					Vec3 n = vtx[1] - vtx[0] ^ vtx[2] - vtx[0];
					float rnlen = 1.0f / n.len();
					n *= rnlen;
					vtx[3] = v - n * (pSkinInfo[i].w[3] = n * (v - vtx[0]));
					Vec3 edge = (vtx[1] + vtx[2] - vtx[0] * 2).normalized();
					pSkinInfo[i].M = Matrix33(edge, n ^ edge, n).T();
					for (j = 0, n *= rnlen; j < 3; j++)
						pSkinInfo[i].w[j] = (vtx[inc_mod3[j]] - vtx[3] ^ vtx[dec_mod3[j]] - vtx[3]) * n;
				}
				else
					pSkinInfo[i].idx[0] = -1;
			} // locki
		}
	}
	if (pLattice)
		pPhysSkel->Release();
	pSphereBig->Release();
	pSphere->Release();
}

IStatObj* CStatObj::SkinVertices(strided_pointer<Vec3> pSkelVtx, const Matrix34& mtxSkelToMesh, volatile int* ready)
{
	if (!m_hasSkinInfo && m_pDelayedSkinParams)
	{
		PrepareSkinData(m_pDelayedSkinParams->mtxSkelToMesh, m_pDelayedSkinParams->pPhysSkel, m_pDelayedSkinParams->r);
		if (m_hasSkinInfo)
			delete m_pDelayedSkinParams, m_pDelayedSkinParams = 0;
	}
	if (!m_pRenderMesh || !m_hasSkinInfo)
		return this;
	CStatObj* pObj = this;
	if (!(GetFlags() & STATIC_OBJECT_CLONE) || !pObj->m_pClonedSourceObject)
	{
		pObj = (CStatObj*)Clone(true, true, false);
		pObj->m_pRenderMesh->KeepSysMesh(true);
	}
	if (!pObj->m_pClonedSourceObject || !pObj->m_pClonedSourceObject->m_pRenderMesh)
		return pObj;

	strided_pointer<Vec3> pVtx;
	strided_pointer<SPipTangents> pTangents, pTangents0;
	Vec3 vtx[4], t, b;
	Matrix33 M;
	AABB bbox;
	bbox.Reset();
	int i, j, nVtx;
	SSkinVtx* pSkinInfo = m_pSkinInfo;
	if (!ready)
		pObj->m_pRenderMesh->LockForThreadAccess();
	pObj->m_pClonedSourceObject->m_pRenderMesh->LockForThreadAccess();

	pVtx.data = (Vec3*)pObj->m_pRenderMesh->GetPosPtr(pVtx.iStride, FSL_SYSTEM_UPDATE);
	pTangents.data = (SPipTangents*)pObj->m_pRenderMesh->GetTangentPtr(pTangents.iStride, FSL_SYSTEM_UPDATE);
	pTangents0.data = (SPipTangents*)pObj->m_pClonedSourceObject->m_pRenderMesh->GetTangentPtr(pTangents0.iStride, FSL_READ);
	nVtx = pObj->m_pRenderMesh->GetVerticesCount();
	if (!pVtx.data)
		nVtx = 0;
	const bool canUseTangents = pTangents.data && pTangents0.data;
	for (i = 0; i < nVtx; i++)
	{
		Vec3 v3 = pVtx[i];
		if (pSkinInfo[i].idx[0] >= 0)
		{
			for (j = 0, v3.zero(); j < 3 + pSkinInfo[i].bVolumetric; j++)
				v3 += pSkinInfo[i].w[j] * (vtx[j] = mtxSkelToMesh * pSkelVtx[pSkinInfo[i].idx[j]]);
			if (!pSkinInfo[i].bVolumetric)
			{
				Vec3 n = (vtx[1] - vtx[0] ^ vtx[2] - vtx[0]).normalized();
				v3 += n * pSkinInfo[i].w[3];
				Vec3 edge = (vtx[1] + vtx[2] - vtx[0] * 2).normalized();
				M = Matrix33(edge, n ^ edge, n);
			}
			else
				M = Matrix33(vtx[1] - vtx[0], vtx[2] - vtx[0], vtx[3] - vtx[0]);
			M *= pSkinInfo[i].M;
			if (canUseTangents)
			{
				SMeshTangents tb(pTangents0[i]);

				tb.RotateSafelyBy(M);
				tb.ExportTo(pTangents[i]);
			}
			pVtx[i] = v3;
		}
		bbox.Add(v3);
	}
	pObj->m_pClonedSourceObject->m_pRenderMesh->UnlockStream(VSF_TANGENTS);
	pObj->m_pClonedSourceObject->m_pRenderMesh->UnLockForThreadAccess();
	if (!ready)
	{
		pObj->m_pRenderMesh->UnlockStream(VSF_GENERAL);
		pObj->m_pRenderMesh->UnlockStream(VSF_TANGENTS);
		pObj->m_pRenderMesh->UnLockForThreadAccess();
	}
	else
		CryInterlockedDecrement(ready);
	pObj->m_AABB = bbox;
	return pObj;
}

//////////////////////////////////////////////////////////////////////////

int CStatObj::Physicalize(IPhysicalEntity* pent, pe_geomparams* pgp, int id, const char* szPropsOverride)
{
	int res = -1;
	if (GetFlags() & STATIC_OBJECT_COMPOUND)
	{
		Matrix34 mtxId(IDENTITY);
		res = PhysicalizeSubobjects(pent, pgp->pMtx3x4 ? pgp->pMtx3x4 : &mtxId, pgp->mass, pgp->density, id, 0, szPropsOverride, 
			pgp->type == pe_articgeomparams::type_id ? ((pe_articgeomparams*)pgp)->idbody : -1);
	}
	else
	{
		int i, nNoColl, iNoColl = 0;
		float V;
		if (pgp->mass < 0 && pgp->density < 0)
			GetPhysicalProperties(pgp->mass, pgp->density);
		for (i = m_arrPhysGeomInfo.GetGeomCount() - 1, nNoColl = 0, V = 0.0f; i >= 0; i--)
			if (m_arrPhysGeomInfo.GetGeomType(i) == PHYS_GEOM_TYPE_DEFAULT)
				V += m_arrPhysGeomInfo[i]->V;
			else
				iNoColl = i, ++nNoColl;
		int flags0 = pgp->flags;
		ISurfaceTypeManager* pSurfaceMan = Get3DEngine()->GetMaterialManager()->GetSurfaceTypeManager();
		if (phys_geometry* pSolid = m_arrPhysGeomInfo[PHYS_GEOM_TYPE_DEFAULT])
			if (pSolid->surface_idx < pSolid->nMats)
				if (ISurfaceType* pMat = pSurfaceMan->GetSurfaceType(pSolid->pMatMapping[pSolid->surface_idx]))
					if (pMat->GetPhyscalParams().collType >= 0)
						(pgp->flags &= ~(geom_collides | geom_floats)) |= pMat->GetPhyscalParams().collType;
		if (pgp->mass > pgp->density && V > 0.0f) // mass is set instead of density and V is valid
			pgp->density = pgp->mass / V, pgp->mass = -1.0f;
		(pgp->flags &= ~geom_colltype_explosion) |= geom_colltype_explosion & ~-(int)m_bDontOccludeExplosions;
		(pgp->flags &= ~geom_manually_breakable) |= geom_manually_breakable &  - (int)m_bBreakableByGame;
		if (m_nFlags & STATIC_OBJECT_NO_PLAYER_COLLIDE)
			pgp->flags &= ~geom_colltype_player;
		pgp->flagsCollider |= geom_allow_id_duplicates;
		if (m_nSpines && m_arrPhysGeomInfo.GetGeomCount() - nNoColl <= 1 &&
		    (nNoColl == 1 || nNoColl == 2 && m_arrPhysGeomInfo[PHYS_GEOM_TYPE_NO_COLLIDE] && m_arrPhysGeomInfo[PHYS_GEOM_TYPE_OBSTRUCT]))
		{
			pe_params_structural_joint psj;
			bool bHasJoints = false;
			psj.partid[1] = 1;
			if (m_arrPhysGeomInfo.GetGeomCount() > nNoColl)
			{
				if (nNoColl && m_pParentObject && m_pParentObject != this)
				{
					CStatObj* pParent;
					for (pParent = m_pParentObject; pParent->m_pParentObject; pParent = pParent->m_pParentObject)
						;
					bHasJoints = pParent->FindSubObject_StrStr("$joint") != 0;
					psj.partid[0] = id;
					psj.partid[1] = pParent->GetSubObjectCount();
					psj.pt = m_arrPhysGeomInfo[iNoColl]->origin;
					psj.bBreakable = 0;
				}
				res = pent->AddGeometry(m_arrPhysGeomInfo[PHYS_GEOM_TYPE_DEFAULT], pgp, id);
			}
			pgp->minContactDist = GetFloatCVar(e_FoliageStiffness);
			pgp->density = 5.0f;
			if (nNoColl == 1)
			{
				pgp->flags = geom_log_interactions | geom_squashy;
				pgp->flags |= geom_colltype_foliage_proxy;
				res = pent->AddGeometry(m_arrPhysGeomInfo[iNoColl], pgp, psj.partid[1]);
				if (bHasJoints)
					pent->SetParams(&psj);
			}
			else
			{
				pgp->flags = geom_squashy | geom_colltype_obstruct;
				pent->AddGeometry(m_arrPhysGeomInfo[PHYS_GEOM_TYPE_OBSTRUCT], pgp, psj.partid[1]++);
				if (bHasJoints)
					pent->SetParams(&psj);
				pgp->flags = geom_log_interactions | geom_colltype_foliage_proxy;
				int flagsCollider = pgp->flagsCollider;
				pgp->flagsCollider = 0;
				pent->AddGeometry(m_arrPhysGeomInfo[PHYS_GEOM_TYPE_NO_COLLIDE], pgp, psj.partid[1]);
				pgp->flagsCollider = flagsCollider;
				if (bHasJoints)
					pent->SetParams(&psj);
			}
		}
		else if (nNoColl == 1 && m_arrPhysGeomInfo.GetGeomCount() == 2)
		{
			// one solid, one obstruct or nocoll proxy -> use single part with ray proxy
			res = pent->AddGeometry(m_arrPhysGeomInfo[iNoColl], pgp, id);
			pgp->flags |= geom_proxy;
			pent->AddGeometry(m_arrPhysGeomInfo[iNoColl ^ 1], pgp, res);
			pgp->flags &= ~geom_proxy;
		}
		else
		{
			// add all solid and non-colliding geoms as individual parts
			for (i = 0; i < m_arrPhysGeomInfo.GetGeomCount(); i++)
				if (m_arrPhysGeomInfo.GetGeomType(i) == PHYS_GEOM_TYPE_DEFAULT)
				{
					int flags1 = pgp->flags;
					if (m_arrPhysGeomInfo[i]->surface_idx < m_arrPhysGeomInfo[i]->nMats)
						if (ISurfaceType* pMat = pSurfaceMan->GetSurfaceType(m_arrPhysGeomInfo[i]->pMatMapping[m_arrPhysGeomInfo[i]->surface_idx]))
							if (pMat->GetPhyscalParams().collType >= 0)
								(pgp->flags &= ~(geom_collides | geom_floats)) |= pMat->GetPhyscalParams().collType;
					res = pent->AddGeometry(m_arrPhysGeomInfo[i], pgp, id);
					pgp->flags = flags1;
				}
			pgp->idmatBreakable = -1;
			for (i = 0; i < m_arrPhysGeomInfo.GetGeomCount(); i++)
				if (m_arrPhysGeomInfo.GetGeomType(i) == PHYS_GEOM_TYPE_NO_COLLIDE)
					pgp->flags = geom_colltype_ray, res = pent->AddGeometry(m_arrPhysGeomInfo[i], pgp, id);
				else if (m_arrPhysGeomInfo.GetGeomType(i) == PHYS_GEOM_TYPE_OBSTRUCT)
					pgp->flags = geom_colltype_obstruct, res = pent->AddGeometry(m_arrPhysGeomInfo[i], pgp, id);
		}
		pgp->flags = flags0;

		if (m_arrPhysGeomInfo.GetGeomCount() >= 10 && pent->GetType() == PE_STATIC)
		{
			pe_params_flags pf;
			pf.flagsOR = pef_parts_traceable;
			pf.flagsAND = ~pef_traceable;
			pent->SetParams(&pf);
		}
	}
	return res;
}

#define isalpha(c) isalpha((unsigned char)c)
#define isdigit(c) isdigit((unsigned char)c)

int CStatObj::PhysicalizeSubobjects(IPhysicalEntity* pent, const Matrix34* pMtx, float mass, float density, int id0, strided_pointer<int> pJointsIdMap, const char* szPropsOverride, int idbodyArtic)
{
	int i, j, i0, i1, len = 0, len1, nObj = GetSubObjectCount(), ipart[2], nparts, bHasPlayerOnlyGeoms = 0, nGeoms = 0, id, idNext = 0;
	float V[2], M, scale3, jointsz;
	const char* pval, * properties;
	bool bHasSkeletons = false, bAutoJoints = false;
	Matrix34 mtxLoc;
	Vec3 dist;
	primitives::box joint_bbox;
	IGeometry* pJointBox = 0;

	primitives::box bbox;
	IStatObj::SSubObject* pSubObj, * pSubObj1;
	pe_articgeomparams partpos;
	pe_params_structural_joint psj;
	pe_params_flags pf;
	geom_world_data gwd;
	intersection_params ip;
	geom_contact* pcontacts;
	scale3 = pMtx ? pMtx->GetColumn0().len() * pMtx->GetColumn1().len() * pMtx->GetColumn2().len() : 1.0f;
	ip.bStopAtFirstTri = ip.bNoBorder = true;

	bbox.Basis.SetIdentity();
	bbox.size.Set(0.5f, 0.5f, 0.5f);
	bbox.center.zero();
	joint_bbox = bbox;
	pf.flagsOR = 0;

	for (i = j = 0; i < nObj; i++)
		if (pSubObj = GetSubObject(i))
			j += pSubObj->pStatObj && pSubObj->pStatObj->GetPhysGeom(0) != 0;
	id0 = id0 < 0 ? 0 : EntityPhysicsUtils::AllocPartIdRange(id0, j);

	for (i = 0; i < nObj; i++)
		if ((pSubObj = GetSubObject(i))->nType == STATIC_SUB_OBJECT_MESH && pSubObj->pStatObj && pSubObj->pStatObj->GetPhysGeom() &&
		    pSubObj->bHidden &&
		    (!strncmp(pSubObj->name, "childof_", 8) && (pSubObj1 = FindSubObject((const char*)pSubObj->name + 8)) ||
		     (pSubObj1 = GetSubObject(pSubObj->nParent)) && strstr(pSubObj1->properties, "group")))
			pSubObj->bHidden = pSubObj1->bHidden;

	for (i = 0, V[0] = V[1] = M = 0; i < nObj; i++)
		if ((pSubObj = GetSubObject(i))->nType == STATIC_SUB_OBJECT_MESH && pSubObj->pStatObj && pSubObj->pStatObj->GetPhysGeom() && !pSubObj->bHidden && strncmp(pSubObj->name, "skeleton_", 9))
		{
			float mi = -1.0f, dens = -1.0f, Vsubobj, scaleLoc3 = scale3 * pSubObj->tm.GetColumn0().len() * pSubObj->tm.GetColumn1().len() * pSubObj->tm.GetColumn2().len();
			for (j = 0, Vsubobj = 0.0f; pSubObj->pStatObj->GetPhysGeom(j); j++)
				if (((CStatObj*)pSubObj->pStatObj)->m_arrPhysGeomInfo.GetGeomType(j) == PHYS_GEOM_TYPE_DEFAULT)
					Vsubobj += pSubObj->pStatObj->GetPhysGeom(j)->V;
			pSubObj->pStatObj->GetPhysicalProperties(mi, dens);
			if (dens > 0)
				mi = Vsubobj * scaleLoc3 * dens; // Calc mass.

			if (mi != 0.0f)
				V[isneg(mi)] += Vsubobj * scaleLoc3;
			M += max(0.0f, mi);

			if (((CStatObj*)pSubObj->pStatObj)->m_nRenderTrisCount <= 0 &&
			    pSubObj->pStatObj->GetPhysGeom()->pGeom->GetForeignData() == pSubObj->pStatObj &&
			    strstr(pSubObj->properties, "other_rendermesh"))
			{
				Vec3 center = pSubObj->localTM * (((CStatObj*)pSubObj->pStatObj)->m_AABB.GetCenter());
				float mindist = 1e10f, curdist;
				for (j = 0, i0 = i; j < nObj; j++)
					if (j != i && (pSubObj1 = GetSubObject(j)) && pSubObj1->pStatObj && ((CStatObj*)pSubObj1->pStatObj)->m_nRenderTrisCount > 0 &&
					    (curdist = (pSubObj1->localTM * (((CStatObj*)pSubObj1->pStatObj)->m_AABB.GetCenter()) - center).len2()) < mindist)
					{
						mindist = curdist;
						i0 = j;
					}
				pSubObj->pStatObj->GetPhysGeom()->pGeom->SetForeignData(GetSubObject(i0)->pStatObj, 0);
			}
		}
	for (i = 0; i < nObj; i++)
		GetSubObject(i)->nBreakerJoints = 0;
	if (mass <= 0)
		mass = M * density;
	if (density <= 0)
	{
		if ((V[0] + V[1]) != 0)
			density = mass / (V[0] + V[1]);
		else
			density = 1000.0f; // Some default.
	}
	partpos.flags = geom_collides | geom_floats;

	for (i = 0; i < nObj; i++)
		if ((pSubObj = GetSubObject(i))->nType == STATIC_SUB_OBJECT_MESH && pSubObj->pStatObj && pSubObj->pStatObj->GetPhysGeom() &&
		    !pSubObj->bHidden && strcmp(pSubObj->name, "colltype_player") == 0)
			bHasPlayerOnlyGeoms = 1;

	for (i = 0; i < nObj; i++)
		if ((pSubObj = GetSubObject(i))->nType == STATIC_SUB_OBJECT_MESH && pSubObj->pStatObj && ((CStatObj*)pSubObj->pStatObj)->m_arrPhysGeomInfo.GetGeomCount() && !pSubObj->bHidden)
		{
			if (pJointsIdMap)
				continue;
			partpos.idbody = idbodyArtic + (i + id0 - idbodyArtic & idbodyArtic >> 31);
			partpos.pMtx3x4 = pMtx ? &(mtxLoc = *pMtx * pSubObj->tm) : &pSubObj->tm;

			float mi = 0, di = 0;
			if (pSubObj->pStatObj->GetPhysicalProperties(mi, di))
			{
				if (mi >= 0)
					partpos.mass = mi, partpos.density = 0;
				else
					partpos.mass = 0, partpos.density = di;
			}
			else
				partpos.density = density;
			if (GetCVars()->e_ObjQuality != CONFIG_LOW_SPEC)
			{
				partpos.idmatBreakable = ((CStatObj*)pSubObj->pStatObj)->m_idmatBreakable;
				if (((CStatObj*)pSubObj->pStatObj)->m_bVehicleOnlyPhysics)
					partpos.flags = geom_colltype6;
				else
				{
					partpos.flags = (geom_colltype_solid & ~(geom_colltype_player & - bHasPlayerOnlyGeoms)) | geom_colltype_ray | geom_floats | geom_colltype_explosion;
					if (bHasPlayerOnlyGeoms && strcmp(pSubObj->name, "colltype_player") == 0)
						partpos.flags = geom_colltype_player;
				}
			}
			else
			{
				partpos.idmatBreakable = -1;
				if (((CStatObj*)pSubObj->pStatObj)->m_bVehicleOnlyPhysics)
					partpos.flags = geom_colltype6;
			}
			if (!strncmp(pSubObj->name, "skeleton_", 9))
			{
				if (!GetCVars()->e_DeformableObjects)
					continue;
				bHasSkeletons = true;
				if (mi <= 0.0f)
					partpos.mass = 1.0f, partpos.density = 0.0f;
			}
			partpos.flagsCollider &= ~geom_destroyed_on_break;
			if (strstr(pSubObj->properties, "pieces"))
				partpos.flagsCollider |= geom_destroyed_on_break;
			if (strstr(pSubObj->properties, "noselfcoll"))
				partpos.flags &= ~(partpos.flagsCollider = geom_colltype_debris);
			if ((id = pSubObj->pStatObj->Physicalize(pent, &partpos, i + id0)) >= 0)
				nGeoms++, idNext = id + 1;
			if (strstr(pSubObj->properties, "force_joint"))
			{
				pe_params_structural_joint psj1;
				psj1.id = 1024 + i;
				psj1.partid[0] = i + id0;
				psj1.partid[1] = pSubObj->nParent + id0;
				psj1.bBreakable = 0;
				psj1.pt = *partpos.pMtx3x4 * pSubObj->pStatObj->GetAABB().GetCenter();
				pent->SetParams(&psj1);
			}
		}
		else if (pSubObj->nType == STATIC_SUB_OBJECT_DUMMY && !strncmp(pSubObj->name, "$joint", 6))
		{
			properties = szPropsOverride ? szPropsOverride : (const char*)pSubObj->properties;
			psj.pt = pSubObj->tm.GetTranslation();
			psj.n = pSubObj->tm.GetColumn(2).normalized();
			psj.axisx = pSubObj->tm.GetColumn(0).normalized();
			float maxdim = max(pSubObj->helperSize.x, pSubObj->helperSize.y);
			maxdim = max(maxdim, pSubObj->helperSize.z);
			psj.szSensor = jointsz = maxdim * pSubObj->tm.GetColumn(0).len();
			psj.partidEpicenter = -1;
			psj.bBroken = 0;
			psj.id = i;
			psj.bReplaceExisting = 1;
			if (pSubObj->name[6] != ' ')
			{
				gwd.offset = psj.pt;
				ipart[1] = nObj;
				for (i0 = nparts = 0; i0 < nObj && nparts < 2; i0++)
					if ((pSubObj1 = GetSubObject(i0))->nType == STATIC_SUB_OBJECT_MESH && pSubObj1->pStatObj && pSubObj1->pStatObj->GetPhysGeom() &&
					    strncmp(pSubObj1->name, "skeleton_", 9) && !strstr(pSubObj1->properties, "group"))
					{
						gwd.offset = pSubObj1->tm.GetInverted() * psj.pt;
						pSubObj1->pStatObj->GetPhysGeom()->pGeom->GetBBox(&bbox);
						dist = bbox.Basis * (gwd.offset - bbox.center);
						for (j = 0; j < 3; j++)
							dist[j] = max(0.0f, fabs_tpl(dist[j]) - bbox.size[j]);
						gwd.scale = jointsz;
						if (fabs_tpl(pSubObj1->tm.GetColumn(0).len2() - 1.0f) > 0.01f)
							gwd.scale /= pSubObj1->tm.GetColumn(0).len();

						// Make a geometry box for intersection.
						if (!pJointBox)
							pJointBox = GetPhysicalWorld()->GetGeomManager()->CreatePrimitive(primitives::box::type, &joint_bbox);
						{
							WriteLockCond lockColl;
							if (dist.len2() < sqr(gwd.scale * 0.5f) && pSubObj1->pStatObj->GetPhysGeom()->pGeom->IntersectLocked(pJointBox, 0, &gwd, &ip, pcontacts, lockColl))
								ipart[nparts++] = i0;
						} // lock
					}
				if (nparts == 0)
					continue;
				GetSubObject(ipart[0])->pStatObj->GetPhysGeom()->pGeom->GetBBox(&bbox);
				gwd.offset = GetSubObject(ipart[0])->tm * bbox.center;
				j = isneg((gwd.offset - psj.pt) * psj.n);
				i0 = ipart[j];
				i1 = ipart[1 ^ j];

				if (strlen((const char*)pSubObj->name) >= 7 && !strncmp((const char*)pSubObj->name + 7, "sample", 6))
					psj.bBroken = 2;
				else if (strlen((const char*)pSubObj->name) >= 7 && !strncmp((const char*)pSubObj->name + 7, "impulse", 7))
					psj.bBroken = 2, psj.id = joint_impulse, psj.bReplaceExisting = 0, bAutoJoints = true;
			}
			else
			{
				for (i0 = 0; i0 < nObj && ((pSubObj1 = GetSubObject(i0))->nType != STATIC_SUB_OBJECT_MESH ||
				                           (strlen((const char*)pSubObj->name) >= 7 && (strncmp((const char*)pSubObj->name + 7, pSubObj1->name, len = strlen(pSubObj1->name)) || isalpha(pSubObj->name[7 + len])))); i0++)
					;
				for (i1 = 0; i1 < nObj && ((pSubObj1 = GetSubObject(i1))->nType != STATIC_SUB_OBJECT_MESH ||
				                           (strlen((const char*)pSubObj->name) >= 8 && (strncmp((const char*)pSubObj->name + 8 + len, pSubObj1->name, len1 = strlen(pSubObj1->name)) || isalpha(pSubObj->name[8 + len + len1])))); i1++)
					;
				if (i0 >= nObj && i1 >= nObj)
					CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR, "Error: cannot resolve part names in %s (%s)", (const char*)pSubObj->name, (const char*)m_szFileName);
			}
			if (pJointsIdMap)
				i0 = pJointsIdMap[i0], i1 = pJointsIdMap[i1];
			psj.partid[0] = i0 + id0;
			psj.partid[1] = i1 + id0;
			psj.maxForcePush = psj.maxForcePull = psj.maxForceShift = psj.maxTorqueBend = psj.maxTorqueTwist = 1E20f;
			if (pMtx)
			{
				psj.pt = *pMtx * psj.pt;
				psj.n = pMtx->TransformVector(psj.n).normalized();
				psj.axisx = pMtx->TransformVector(psj.axisx).normalized();
			}

			if ((pval = strstr(properties, "limit")) && (pval - 11 < properties - 11 || strncmp(pval - 11, "constraint_", 11)))
			{
				for (pval += 5; *pval && !isdigit(*pval); pval++)
					;
				if (pval) psj.maxTorqueBend = atof(pval);
				//psj.maxTorqueTwist = psj.maxTorqueBend*5;
				psj.maxForcePull = psj.maxTorqueBend;
				psj.maxForceShift = psj.maxTorqueBend;
				//				psj.maxForcePush = psj.maxForcePull*2.5f;
				psj.bBreakable = 1;
			}
			if (pval = strstr(properties, "twist"))
			{
				for (pval += 5; *pval && !isdigit(*pval); pval++)
					;
				if (pval) psj.maxTorqueTwist = atof(pval);
			}
			if (pval = strstr(properties, "bend"))
			{
				for (pval += 4; *pval && !isdigit(*pval); pval++)
					;
				if (pval) psj.maxTorqueBend = atof(pval);
			}
			if (pval = strstr(properties, "push"))
			{
				for (pval += 4; *pval && !isdigit(*pval); pval++)
					;
				if (pval) psj.maxForcePush = atof(pval);
			}
			if (pval = strstr(properties, "pull"))
			{
				for (pval += 4; *pval && !isdigit(*pval); pval++)
					;
				if (pval) psj.maxForcePull = atof(pval);
			}
			if (pval = strstr(properties, "shift"))
			{
				for (pval += 5; *pval && !isdigit(*pval); pval++)
					;
				if (pval) psj.maxForceShift = atof(pval);
			}
			if (pval = strstr(properties, "damage_accum"))
			{
				for (pval += 5; *pval && !isdigit(*pval); pval++)
					;
				if (pval) psj.damageAccum = atof(pval);
			}
			if (pval = strstr(properties, "damage_accum_threshold"))
			{
				for (pval += 5; *pval && !isdigit(*pval); pval++)
					;
				if (pval) psj.damageAccumThresh = atof(pval);
			}
			if (psj.maxForcePush + psj.maxForcePull + psj.maxForceShift + psj.maxTorqueBend + psj.maxTorqueTwist > 4.9E20f)
				if (sscanf(properties, "%f %f %f %f %f",
				           &psj.maxForcePush, &psj.maxForcePull, &psj.maxForceShift, &psj.maxTorqueBend, &psj.maxTorqueTwist) == 5)
				{
					psj.maxForcePush *= density;
					psj.maxForcePull *= density;
					psj.maxForceShift *= density;
					psj.maxTorqueBend *= density;
					psj.maxTorqueTwist *= density;
					psj.bBreakable = 1;
				}
				else
					psj.bBreakable = 0;
			psj.bDirectBreaksOnly = strstr(properties, "hits_only") != 0;
			psj.limitConstraint.zero();
			psj.bConstraintWillIgnoreCollisions = 1;
			if ((len = 16, pval = strstr(properties, "constraint_limit")) || (len = 5, pval = strstr(properties, "C_lmt")))
			{
				for (pval += len; *pval && !isdigit(*pval); pval++)
					;
				if (pval) psj.limitConstraint.z = atof(pval);
			}
			if ((len = 17, pval = strstr(properties, "constraint_minang")) || (len = 5, pval = strstr(properties, "C_min")))
			{
				for (pval += len; *pval && !isdigit(*pval) && *pval != '-'; pval++)
					;
				if (pval) psj.limitConstraint.x = DEG2RAD(atof(pval));
			}
			if ((len = 17, pval = strstr(properties, "constraint_maxang")) || (len = 5, pval = strstr(properties, "C_max")))
			{
				for (pval += len; *pval && !isdigit(*pval) && *pval != '-'; pval++)
					;
				if (pval) psj.limitConstraint.y = DEG2RAD(atof(pval));
			}
			if ((len = 18, pval = strstr(properties, "constraint_damping")) || (len = 5, pval = strstr(properties, "C_dmp")))
			{
				for (pval += len; *pval && !isdigit(*pval); pval++)
					;
				if (pval) psj.dampingConstraint = atof(pval);
			}
			if (strstr(properties, "constraint_collides") || strstr(properties, "C_coll"))
				psj.bConstraintWillIgnoreCollisions = 0;
			float scale = GetFloatCVar(e_JointStrengthScale);
			psj.maxForcePush *= scale;
			psj.maxForcePull *= scale;
			psj.maxForceShift *= scale;
			psj.maxTorqueBend *= scale;
			psj.maxTorqueTwist *= scale;
			psj.limitConstraint.z *= scale;
			pent->SetParams(&psj);
			if (!gEnv->bMultiplayer && strstr(properties, "gameplay_critical"))
				pf.flagsOR |= pef_override_impulse_scale;
			if (gEnv->bMultiplayer && strstr(properties, "mp_break_always"))
				pf.flagsOR |= pef_override_impulse_scale;
			if (strstr(properties, "player_can_break"))
				pf.flagsOR |= pef_players_can_break;

			if (strstr(pSubObj->properties, "breaker"))
			{
				if (GetSubObject(i0))
					GetSubObject(i0)->nBreakerJoints++;
				if (GetSubObject(i1))
					GetSubObject(i1)->nBreakerJoints++;
			}
		}

	if (bAutoJoints)
	{
		psj.idx = -2; // tels the physics to try and generate joints
		pent->SetParams(&psj);
	}

	for (i = 0; i < nObj; i++)
		if (!strcmp((pSubObj = GetSubObject(i))->name, "$compiled_joints"))
			pent->SetStateFromSnapshotTxt(pSubObj->properties, strlen(pSubObj->properties));

	pe_params_part pp;
	if (bHasSkeletons)
		for (i = 0; i < nObj; i++)
			if ((pSubObj = GetSubObject(i))->nType == STATIC_SUB_OBJECT_MESH && pSubObj->pStatObj && pSubObj->pStatObj->GetPhysGeom() && !pSubObj->bHidden &&
			    !strncmp(pSubObj->name, "skeleton_", 9) && (pSubObj1 = FindSubObject((const char*)pSubObj->name + 9)))
			{
				pe_params_skeleton ps;
				properties = szPropsOverride ? szPropsOverride : (const char*)pSubObj->properties;
				if (pval = strstr(properties, "stiffness"))
				{
					for (pval += 9; *pval && !isdigit(*pval); pval++)
						;
					if (pval) ps.stiffness = atof(pval);
				}
				if (pval = strstr(properties, "thickness"))
				{
					for (pval += 9; *pval && !isdigit(*pval); pval++)
						;
					if (pval) ps.thickness = atof(pval);
				}
				if (pval = strstr(properties, "max_stretch"))
				{
					for (pval += 11; *pval && !isdigit(*pval); pval++)
						;
					if (pval) ps.maxStretch = atof(pval);
				}
				if (pval = strstr(properties, "max_impulse"))
				{
					for (pval += 11; *pval && !isdigit(*pval); pval++)
						;
					if (pval) ps.maxImpulse = atof(pval);
				}
				if (pval = strstr(properties, "skin_dist"))
				{
					for (pval += 9; *pval && !isdigit(*pval); pval++)
						;
					if (pval) pp.minContactDist = atof(pval);
				}
				if (pval = strstr(properties, "hardness"))
				{
					for (pval += 8; *pval && !isdigit(*pval); pval++)
						;
					if (pval) ps.hardness = atof(pval);
				}
				if (pval = strstr(properties, "explosion_scale"))
				{
					for (pval += 15; *pval && !isdigit(*pval); pval++)
						;
					if (pval) ps.explosionScale = atof(pval);
				}

				pp.partid = ps.partid = (pSubObj1 - &m_subObjects[0]) + id0;
				pp.idSkeleton = i + id0;
				pent->SetParams(&pp);
				pent->SetParams(&ps);
				((CStatObj*)pSubObj1->pStatObj)->PrepareSkinData(pSubObj1->localTM.GetInverted() * pSubObj->localTM, pSubObj->pStatObj->GetPhysGeom()->pGeom,
				                                                 is_unused(pp.minContactDist) ? 0.0f : pp.minContactDist);
			}

	new(&pp)pe_params_part;
	for (i = 0; i < nObj; i++)
	{
		if ((pSubObj = GetSubObject(i))->nType == STATIC_SUB_OBJECT_MESH && pSubObj->pStatObj && pSubObj->pStatObj->GetPhysGeom() && !pSubObj->bHidden &&
		    (!strncmp(pSubObj->name, "childof_", 8) && (pSubObj1 = FindSubObject((const char*)pSubObj->name + 8)) ||
		     (pSubObj1 = GetSubObject(pSubObj->nParent)) && strstr(pSubObj1->properties, "group")))
		{
			pp.partid = i + id0;
			pp.idParent = pSubObj1 - &m_subObjects[0];
			pent->SetParams(&pp);
			pSubObj->bHidden = true;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Iterate through sub objects and update hide mask.
	//////////////////////////////////////////////////////////////////////////
	m_nInitialSubObjHideMask = 0;
	for (uint a = 0, numsub = m_subObjects.size(); a < numsub; a++)
	{
		SSubObject& subObj = m_subObjects[a];
		if (subObj.pStatObj && subObj.nType == STATIC_SUB_OBJECT_MESH && subObj.bHidden)
		{
			m_nInitialSubObjHideMask |= (hidemask1 << a);
		}
	}

	if (nGeoms >= 10 && pent->GetType() == PE_STATIC)
		pf.flagsOR |= pef_parts_traceable, pf.flagsAND = ~pef_traceable;

	if (pf.flagsOR)
		pent->SetParams(&pf);
	if (pJointBox)
		pJointBox->Release();
	return idNext - 1;
}

//////////////////////////////////////////////////////////////////////////
////////////////////////////// Foliage ///////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CStatObj::CopyFoliageData(IStatObj* pObjDst, bool bMove, IFoliage* pSrcFoliage, int* pVtxMap, primitives::box* pMovedBoxes, int nMovedBoxes)
{
	CStatObj* pDst = (CStatObj*)pObjDst;
	CMesh* pMesh = 0;
	int i, j, nVtx = 0, nVtxDst = 0, isubs, ibone, i1;
	if (GetIndexedMesh(true))
		nVtx = GetIndexedMesh(true)->GetVertexCount(), pMesh = GetIndexedMesh(true)->GetMesh();
	if (pObjDst->GetIndexedMesh(true))
		nVtxDst = pObjDst->GetIndexedMesh(true)->GetVertexCount();

	if (pDst->m_pSpines)
	{
		for (i = 0; i < pDst->m_nSpines; i++)
			delete[] pDst->m_pSpines[i].pVtx, delete[] pDst->m_pSpines[i].pVtxCur;
		free(pDst->m_pSpines);
		if (pDst->m_pBoneMapping)
			delete[] pDst->m_pBoneMapping;
		pDst->m_pBoneMapping = 0;
	}
	pDst->m_nSpines = m_nSpines;

	if (bMove && nMovedBoxes == -1 || !m_pSpines)
	{
		pDst->m_pSpines = m_pSpines;
		m_pSpines = 0;
		m_nSpines = 0;
	}
	else
	{
		memcpy(pDst->m_pSpines = (SSpine*)malloc(m_nSpines * sizeof(SSpine)), m_pSpines, m_nSpines * sizeof(SSpine));
		for (i1 = isubs = 0, ibone = 1; i1 < (int)m_chunkBoneIds.size() && m_chunkBoneIds[i1] != ibone; i1++)
			isubs += !m_chunkBoneIds[i1];
		for (i = 0; i < m_nSpines; ibone += m_pSpines[i++].nVtx - 1)
		{
			memcpy(pDst->m_pSpines[i].pVtx = new Vec3[m_pSpines[i].nVtx], m_pSpines[i].pVtx, m_pSpines[i].nVtx * sizeof(Vec3));
			memcpy(pDst->m_pSpines[i].pSegDim = new Vec4[m_pSpines[i].nVtx], m_pSpines[i].pSegDim, m_pSpines[i].nVtx * sizeof(Vec4));
			pDst->m_pSpines[i].pVtxCur = new Vec3[m_pSpines[i].nVtx];
			if (pDst->m_pSpines[i].bActive = m_pSpines[i].bActive && (!pSrcFoliage || ((CStatObjFoliage*)pSrcFoliage)->m_pRopes[i] != 0))
			{
				for (; i1 < (int)m_chunkBoneIds.size() && m_chunkBoneIds[i1] != ibone; i1++)
					isubs += !m_chunkBoneIds[i1];
				j = 0;
				if (isubs < pMesh->m_subsets.size())
					for (; j < nMovedBoxes; j++)
					{
						Vec3 dist = pMovedBoxes[j].size - (pMovedBoxes[j].Basis * (pMesh->m_subsets[isubs].vCenter - pMovedBoxes[j].center)).abs();
						if (min(min(dist.x, dist.y), dist.z) > 0)
							break;
					}
				if (j == nMovedBoxes)
				{
					pDst->m_pSpines[i].bActive = false;
					continue;
				}
				m_pSpines[i].bActive = !bMove;
			}
		}
	}

	if (m_pSpines && m_pBoneMapping)
	{
		pDst->m_pBoneMapping = pVtxMap || !bMove || nMovedBoxes >= 0 ? new SMeshBoneMapping_uint8[nVtxDst] : m_pBoneMapping;
		if (pVtxMap)
		{
			memset(pDst->m_pBoneMapping, 0, nVtxDst * sizeof(pDst->m_pBoneMapping[0]));
			for (i = 0; i < nVtx; i++)
				if (pVtxMap[i] >= 0 && m_pBoneMapping[i].weights[0] + m_pBoneMapping[i].weights[1] > 0)
					pDst->m_pBoneMapping[pVtxMap[i]] = m_pBoneMapping[i];
			if (bMove && nMovedBoxes == -1)
				delete[] m_pBoneMapping;
		}
		else if (!bMove)
			memcpy(pDst->m_pBoneMapping, m_pBoneMapping, nVtx * sizeof(pDst->m_pBoneMapping[0]));
		if (bMove && nMovedBoxes == -1)
			m_pBoneMapping = 0;
		pDst->m_chunkBoneIds = m_chunkBoneIds;
	}
}

static int UpdatePtTriDist(const Vec3* vtx, const Vec3& n, const Vec3& pt, float& minDist, float& minDenom)
{
	float rvtx[3] = { (vtx[0] - pt).len2(), (vtx[1] - pt).len2(), (vtx[2] - pt).len2() }, elen2[2], dist, denom;
	int i = idxmin3(rvtx), bInside[2];
	Vec3 edge[2], dp = pt - vtx[i];

	edge[0] = vtx[incm3(i)] - vtx[i];
	elen2[0] = edge[0].len2();
	edge[1] = vtx[decm3(i)] - vtx[i];
	elen2[1] = edge[1].len2();
	bInside[0] = isneg((dp ^ edge[0]) * n);
	bInside[1] = isneg((edge[1] ^ dp) * n);
	rvtx[i] = rvtx[i] * elen2[bInside[0]] - sqr(max(0.0f, dp * edge[bInside[0]])) * (bInside[0] | bInside[1]);
	denom = elen2[bInside[0]];

	if (bInside[0] & bInside[1])
	{
		if (edge[0] * edge[1] < 0)
		{
			edge[0] = vtx[decm3(i)] - vtx[incm3(i)];
			dp = pt - vtx[incm3(i)];
			if ((dp ^ edge[0]) * n > 0)
			{
				dist = rvtx[incm3(i)] * edge[0].len2() - sqr(dp * edge[0]);
				denom = edge[0].len2();
				goto found;
			}
		}
		dist = sqr((pt - vtx[0]) * n), denom = n.len2();
	}
	else
		dist = rvtx[i];

found:
	if (dist * minDenom < minDist * denom)
	{
		minDist = dist;
		minDenom = denom;
		return 1;
	}
	return 0;
}

//static const int g_nRanges = 5;
static int g_rngb2a[] = { 0, 'A', 26, 'a', 52, '0', 62, '+', 63, '/' };
static int g_rnga2b[] = { '+', 62, '/', 63, '0', 52, 'A', 0, 'a', 26 };
static inline int mapsymb(int symb, int* pmap, int n)
{
	int i, j;
	for (i = j = 0; j < n; j++)
		i += isneg(symb - pmap[j * 2]);
	i = n - 1 - i;
	return symb - pmap[i * 2] + pmap[i * 2 + 1];
}

static int Bin2ascii(const unsigned char* pin, int sz, unsigned char* pout)
{
	int a0, a1, a2, i, j, nout, chr[4];
	for (i = nout = 0; i < sz; i += 3, nout += 4)
	{
		a0 = pin[i];
		j = isneg(i + 1 - sz);
		a1 = pin[i + j] & - j;
		j = isneg(i + 2 - sz);
		a2 = pin[i + j * 2] & - j;
		chr[0] = a0 >> 2;
		chr[1] = a0 << 4 & 0x30 | (a1 >> 4) & 0x0F;
		chr[2] = a1 << 2 & 0x3C | a2 >> 6 & 0x03;
		chr[3] = a2 & 0x03F;
		for (j = 0; j < 4; j++)
			*pout++ = mapsymb(chr[j], g_rngb2a, 5);
	}
	return nout;
}
static int Ascii2bin(const unsigned char* pin, int sz, unsigned char* pout, int szout)
{
	int a0, a1, a2, a3, i, nout;
	for (i = nout = 0; i < sz - 4; i += 4, nout += 3)
	{
		a0 = mapsymb(pin[i + 0], g_rnga2b, 5);
		a1 = mapsymb(pin[i + 1], g_rnga2b, 5);
		a2 = mapsymb(pin[i + 2], g_rnga2b, 5);
		a3 = mapsymb(pin[i + 3], g_rnga2b, 5);
		*pout++ = a0 << 2 | a1 >> 4;
		*pout++ = a1 << 4 & 0xF0 | a2 >> 2 & 0x0F;
		*pout++ = a2 << 6 & 0xC0 | a3;
	}
	a0 = mapsymb(pin[i + 0], g_rnga2b, 5);
	a1 = mapsymb(pin[i + 1], g_rnga2b, 5);
	a2 = mapsymb(pin[i + 2], g_rnga2b, 5);
	a3 = mapsymb(pin[i + 3], g_rnga2b, 5);
	if (nout < szout)
		*pout++ = a0 << 2 | a1 >> 4, nout++;
	if (nout < szout)
		*pout++ = a1 << 4 & 0xF0 | a2 >> 2 & 0x0F, nout++;
	if (nout < szout)
		*pout++ = a2 << 6 & 0xC0 | a3, nout++;
	return nout;
}

static void SerializeData(TSerialize ser, const char* name, void* pdata, int size)
{
	/*static std::vector<int> arr;
	   if (ser.IsReading())
	   {
	   ser.Value(name, arr);
	   memcpy(pdata, &arr[0], size);
	   } else
	   {
	   arr.resize(((size-1)>>2)+1);
	   memcpy(&arr[0], pdata, size);
	   ser.Value(name, arr);
	   }*/
	static string str;
	if (!size)
		return;
	if (ser.IsReading())
	{
		ser.Value(name, str);
		int n = Ascii2bin((const unsigned char*)(const char*)str, str.length(), (unsigned char*)pdata, size);
		assert(n == size);
		(void)n;
	}
	else
	{
		str.resize(((size - 1) / 3 + 1) * 4);
		int n = Bin2ascii((const unsigned char*)pdata, size, (unsigned char*)(const char*)str);
		assert(n == str.length());
		(void)n;
		ser.Value(name, str);
	}
}

int CStatObj::Serialize(TSerialize ser)
{
	ser.BeginGroup("StatObj");
	ser.Value("Flags", m_nFlags);
	if (GetFlags() & STATIC_OBJECT_COMPOUND)
	{
		int i, nParts = m_subObjects.size();
		bool bVal;
		string srcObjName;
		ser.Value("nParts", nParts);
		if (m_pClonedSourceObject)
		{
			SetSubObjectCount(nParts);
			for (i = 0; i < nParts; i++)
			{
				ser.BeginGroup("part");
				ser.Value("bGenerated", bVal = !ser.IsReading() && m_subObjects[i].pStatObj &&
				                               m_subObjects[i].pStatObj->GetFlags() & STATIC_OBJECT_GENERATED && !m_subObjects[i].bHidden);
				if (bVal)
				{
					if (ser.IsReading())
						(m_subObjects[i].pStatObj = gEnv->p3DEngine->CreateStatObj())->AddRef();
					m_subObjects[i].pStatObj->Serialize(ser);
				}
				else
				{
					ser.Value("subobj", m_subObjects[i].name);
					if (ser.IsReading())
					{
						IStatObj::SSubObject* pSrcSubObj = m_pClonedSourceObject->FindSubObject(m_subObjects[i].name);
						if (pSrcSubObj)
						{
							m_subObjects[i] = *pSrcSubObj;
							if (pSrcSubObj->pStatObj)
								pSrcSubObj->pStatObj->AddRef();
						}
					}
				}
				bVal = m_subObjects[i].bHidden;
				ser.Value("hidden", bVal);
				m_subObjects[i].bHidden = bVal;
				ser.EndGroup();
			}
		}
	}
	else
	{
#if !CRY_PLATFORM_DESKTOP
		CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR, "Error: full geometry serialization should never happen on consoles. file: '%s' Geom: '%s'", m_szFileName.c_str(), m_szGeomName.c_str());
#else
		CMesh* pMesh;
		int i, nVtx, nTris, nSubsets;
		string matName;
		IMaterial* pMat;

		if (ser.IsReading())
		{
			ser.Value("nvtx", nVtx = 0);
			if (nVtx)
			{
				m_pIndexedMesh = new CIndexedMesh();
				pMesh = m_pIndexedMesh->GetMesh();
				assert(pMesh->m_pPositionsF16 == 0);
				ser.Value("ntris", nTris);
				ser.Value("nsubsets", nSubsets);
				pMesh->SetVertexCount(nVtx);
				pMesh->ReallocStream(CMesh::TEXCOORDS, nVtx);
				pMesh->ReallocStream(CMesh::TANGENTS, nVtx);
				pMesh->SetIndexCount(nTris * 3);

				for (i = 0; i < nSubsets; i++)
				{
					SMeshSubset mss;
					ser.BeginGroup("subset");
					ser.Value("matid", mss.nMatID);
					ser.Value("matflg", mss.nMatFlags);
					ser.Value("vtx0", mss.nFirstVertId);
					ser.Value("nvtx", mss.nNumVerts);
					ser.Value("idx0", mss.nFirstIndexId);
					ser.Value("nidx", mss.nNumIndices);
					ser.Value("center", mss.vCenter);
					ser.Value("radius", mss.fRadius);
					pMesh->m_subsets.push_back(mss);
					ser.EndGroup();
				}

				SerializeData(ser, "Positions", pMesh->m_pPositions, nVtx * sizeof(pMesh->m_pPositions[0]));
				SerializeData(ser, "Normals", pMesh->m_pNorms, nVtx * sizeof(pMesh->m_pNorms[0]));
				SerializeData(ser, "TexCoord", pMesh->m_pTexCoord, nVtx * sizeof(pMesh->m_pTexCoord[0]));
				SerializeData(ser, "Tangents", pMesh->m_pTangents, nVtx * sizeof(pMesh->m_pTangents[0]));
				SerializeData(ser, "Indices", pMesh->m_pIndices, nTris * 3 * sizeof(pMesh->m_pIndices[0]));

				ser.Value("Material", matName);
				SetMaterial(gEnv->p3DEngine->GetMaterialManager()->FindMaterial(matName));
				ser.Value("MaterialAux", matName);
				if (m_pMaterial && (pMat = gEnv->p3DEngine->GetMaterialManager()->FindMaterial(matName)))
				{
					if (pMat->GetSubMtlCount() > 0)
						pMat = pMat->GetSubMtl(0);
					for (i = m_pMaterial->GetSubMtlCount() - 1; i >= 0 && strcmp(m_pMaterial->GetSubMtl(i)->GetName(), matName); i--)
						;
					if (i < 0)
					{
						i = m_pMaterial->GetSubMtlCount();
						m_pMaterial->SetSubMtlCount(i + 1);
						m_pMaterial->SetSubMtl(i, pMat);
					}
				}

				int surfaceTypesId[MAX_SUB_MATERIALS];
				memset(surfaceTypesId, 0, sizeof(surfaceTypesId));
				int numIds = 0;
				if (m_pMaterial)
					numIds = m_pMaterial->FillSurfaceTypeIds(surfaceTypesId);

				char* pIds = new char[nTris];
				memset(pIds, 0, nTris);
				int j, itri;
				for (i = 0; i < pMesh->m_subsets.size(); i++)
					for (itri = (j = pMesh->m_subsets[i].nFirstIndexId) / 3; j < pMesh->m_subsets[i].nFirstIndexId + pMesh->m_subsets[i].nNumIndices; j += 3, itri++)
						pIds[itri] = pMesh->m_subsets[i].nMatID;

				ser.Value("PhysSz", i);
				if (i)
				{
					char* pbuf = new char[i];
					CMemStream stm(pbuf, i, false);
					SerializeData(ser, "PhysMeshData", pbuf, i);
					IGeometry* pPhysMesh = GetPhysicalWorld()->GetGeomManager()->LoadGeometry(stm, pMesh->m_pPositions, pMesh->m_pIndices, pIds);
					pPhysMesh->AddRef();
					pPhysMesh->SetForeignData((IStatObj*)this, 0);
					m_arrPhysGeomInfo.SetPhysGeom(GetPhysicalWorld()->GetGeomManager()->RegisterGeometry(pPhysMesh, 0, surfaceTypesId, numIds));
					delete[] pbuf;
				}
				delete[] pIds;

				Invalidate();
				SetFlags(STATIC_OBJECT_GENERATED);
			}
		}
		else
		{
			if (GetIndexedMesh(true))
			{
				pMesh = GetIndexedMesh(true)->GetMesh();
				assert(pMesh->m_pPositionsF16 == 0);
				ser.Value("nvtx", nVtx = pMesh->GetVertexCount());
				ser.Value("ntris", nTris = pMesh->GetIndexCount() / 3);
				ser.Value("nsubsets", nSubsets = pMesh->m_subsets.size());

				for (i = 0; i < nSubsets; i++)
				{
					ser.BeginGroup("subset");
					ser.Value("matid", pMesh->m_subsets[i].nMatID);
					ser.Value("matflg", pMesh->m_subsets[i].nMatFlags);
					ser.Value("vtx0", pMesh->m_subsets[i].nFirstVertId);
					ser.Value("nvtx", pMesh->m_subsets[i].nNumVerts);
					ser.Value("idx0", pMesh->m_subsets[i].nFirstIndexId);
					ser.Value("nidx", pMesh->m_subsets[i].nNumIndices);
					ser.Value("center", pMesh->m_subsets[i].vCenter);
					ser.Value("radius", pMesh->m_subsets[i].fRadius);
					ser.EndGroup();
				}

				if (m_pMaterial && m_pMaterial->GetSubMtlCount() > 0)
					ser.Value("auxmatname", m_pMaterial->GetSubMtl(m_pMaterial->GetSubMtlCount() - 1)->GetName());

				SerializeData(ser, "Positions", pMesh->m_pPositions, nVtx * sizeof(pMesh->m_pPositions[0]));
				SerializeData(ser, "Normals", pMesh->m_pNorms, nVtx * sizeof(pMesh->m_pNorms[0]));
				SerializeData(ser, "TexCoord", pMesh->m_pTexCoord, nVtx * sizeof(pMesh->m_pTexCoord[0]));
				SerializeData(ser, "Tangents", pMesh->m_pTangents, nVtx * sizeof(pMesh->m_pTangents[0]));
				SerializeData(ser, "Indices", pMesh->m_pIndices, nTris * 3 * sizeof(pMesh->m_pIndices[0]));

				ser.Value("Material", GetMaterial()->GetName());
				if (m_pMaterial && m_pMaterial->GetSubMtlCount() > 0)
					ser.Value("MaterialAux", m_pMaterial->GetSubMtl(m_pMaterial->GetSubMtlCount() - 1)->GetName());
				else
					ser.Value("MaterialAux", matName = "");

				if (GetPhysGeom())
				{
					CMemStream stm(false);
					GetPhysicalWorld()->GetGeomManager()->SaveGeometry(stm, GetPhysGeom()->pGeom);
					ser.Value("PhysSz", i = stm.GetUsedSize());
					SerializeData(ser, "PhysMeshData", stm.GetBuf(), i);
				}
				else
					ser.Value("PhysSz", i = 0);
			}
			else
				ser.Value("nvtx", nVtx = 0);
		}
#endif // !CRY_PLATFORM_DESKTOP
	}

	ser.EndGroup(); //StatObj

	return 1;
}

//////////////////////////////////////////////////////////////////////////
IParticleEffect* CStatObj::GetSurfaceBreakageEffect(const char* sType)
{
	if (!m_pRenderMesh)
		return 0;

	IMaterial* pSubMtl;
	TRenderChunkArray& meshChunks = m_pRenderMesh->GetChunks();
	for (int iss = 0; iss < meshChunks.size(); iss++)
	{
		if ((meshChunks[iss].m_nMatFlags & (MTL_FLAG_NOPHYSICALIZE | MTL_FLAG_NODRAW)) == MTL_FLAG_NOPHYSICALIZE ||
		    meshChunks.size() == 1 && !m_arrPhysGeomInfo[PHYS_GEOM_TYPE_DEFAULT])
			if (m_pMaterial && ((pSubMtl = m_pMaterial->GetSubMtl(meshChunks[iss].m_nMatID)) || (pSubMtl = m_pMaterial)))
			{
				//idmat = pSubMtl->GetSurfaceTypeId();
				ISurfaceType* pSurface = pSubMtl->GetSurfaceType();
				ISurfaceType::SBreakageParticles* pParticles = pSurface->GetBreakageParticles(sType);
				if (pParticles)
				{
					return gEnv->pParticleManager->FindEffect(pParticles->particle_effect);
				}
			}
	}
	return 0;
}

namespace
{
struct SBranchPt
{
	Vec3  pt;
	Vec2  tex;
	int   itri;
	float minDist, minDenom;
};

struct SBranch
{
	SBranchPt* pt;
	float      len;
};
}

void CStatObj::FreeFoliageData()
{
	if (m_pSpines)
	{
		for (int i = 0; i < m_nSpines; i++)
			delete[] m_pSpines[i].pVtx, delete[] m_pSpines[i].pVtxCur, delete[] m_pSpines[i].pSegDim;
		free(m_pSpines);
		m_pSpines = 0;
		m_nSpines = 0;
	}
	delete[] m_pBoneMapping;
	m_pBoneMapping = 0;
	m_chunkBoneIds.resize(0);
}

void CStatObj::AnalyzeFoliage(IRenderMesh* pRenderMesh, CContentCGF* pCGF)
{
	FUNCTION_PROFILER_3DENGINE;

	if (pRenderMesh && m_nFlags & STATIC_OBJECT_DYNAMIC)
		pRenderMesh->KeepSysMesh(true);
	CStatObj* pHelpersHost = this;
	while (pHelpersHost->m_pParentObject)
		pHelpersHost = pHelpersHost->m_pParentObject;
	if (!pHelpersHost->FindSubObject_StrStr("branch"))
		return;

	FreeFoliageData();

	assert(!m_nSpines && !m_pSpines && !m_pBoneMapping);
	if (!(m_nFlags & STATIC_OBJECT_CANT_BREAK))
	{
		if (pRenderMesh)
			pRenderMesh->KeepSysMesh(true);
		m_nFlags |= STATIC_OBJECT_DYNAMIC;
	}
	phys_geometry* pgeom;
	int idmat = 0;
	if ((pgeom = m_arrPhysGeomInfo[PHYS_GEOM_TYPE_OBSTRUCT]) && pgeom->pMatMapping)
		idmat = pgeom->pMatMapping[pgeom->surface_idx];
	if ((pgeom = m_arrPhysGeomInfo[PHYS_GEOM_TYPE_NO_COLLIDE]) && pgeom->pMatMapping)
		idmat = pgeom->pMatMapping[pgeom->surface_idx];

	if (pCGF && pCGF->GetFoliageInfo()->nSpines)
	{
		if (pRenderMesh)
			pRenderMesh->GenerateQTangents();

		const SFoliageInfoCGF& fi = *pCGF->GetFoliageInfo();
		SSpine* const pSpines = (SSpine*)malloc(fi.nSpines * sizeof(SSpine));
		for (int i = 0; i < fi.nSpines; ++i)
		{
			const SSpineRC& srcSpine = fi.pSpines[i];

			const int nVtx = srcSpine.nVtx;

			SSpine& dstSpine = pSpines[i];

			dstSpine.bActive = true;
			dstSpine.idmat = idmat;
			dstSpine.len = srcSpine.len;
			dstSpine.navg = srcSpine.navg;
			dstSpine.iAttachSpine = srcSpine.iAttachSpine;
			dstSpine.iAttachSeg = srcSpine.iAttachSeg;
			dstSpine.nVtx = nVtx;

			static_assert(sizeof(Vec3) == sizeof(dstSpine.pVtx[0]) && sizeof(Vec3) == sizeof(srcSpine.pVtx[0]), "Invalid type size!");
			memcpy(dstSpine.pVtx = new Vec3[nVtx], srcSpine.pVtx, sizeof(Vec3) * nVtx);

			static_assert(sizeof(Vec4) == sizeof(dstSpine.pSegDim[0]) && sizeof(Vec4) == sizeof(srcSpine.pSegDim[0]), "Invalid type size!");
			memcpy(dstSpine.pSegDim = new Vec4[nVtx], srcSpine.pSegDim, sizeof(Vec4) * nVtx);

			dstSpine.pVtxCur = new Vec3[nVtx];
		}

		std::vector<uint16> chunkBoneIds(fi.chunkBoneIds.size());
		for (int i = 0; i < (int)fi.chunkBoneIds.size(); ++i)
		{
			chunkBoneIds[i] = fi.chunkBoneIds[i];
		}

		if (GetCVars()->e_VegetationBoneInfo)
		{
			const int nMaxBoneIds = 256;

			if ((int)chunkBoneIds.size() <= nMaxBoneIds)
			{
				CryLog("Vegetation object [%s] has [%" PRISIZE_T "] bone IDs", m_szFileName.c_str(), chunkBoneIds.size());
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING,
				           "Vegetation object[%s] has [%" PRISIZE_T "] bone IDs - maximum supported is [%d]",
				           m_szFileName.c_str(), chunkBoneIds.size(), nMaxBoneIds);
			}
		}

		SMeshBoneMapping_uint8* pBoneMapping;
		memcpy(pBoneMapping = new SMeshBoneMapping_uint8[fi.nSkinnedVtx], fi.pBoneMapping, sizeof(pBoneMapping[0]) * fi.nSkinnedVtx);

		// Convert per-subset bone ids to per-mesh bone ids
		{
			const char* err = 0;

			if (!pRenderMesh)
			{
				err = "has no render mesh";
				goto errorDetected;
			}

			{
				const uint boneIdCount = (uint)fi.chunkBoneIds.size();

				uint start = 0;
				uint n = 0;

				const TRenderChunkArray& renderChunks = pRenderMesh->GetChunks();

				for (int chunkIndex = 0; chunkIndex < (int)renderChunks.size(); ++chunkIndex)
				{
					const CRenderChunk& chunk = renderChunks[chunkIndex];

					if ((chunk.m_nMatFlags & (MTL_FLAG_NOPHYSICALIZE | MTL_FLAG_NODRAW)) != MTL_FLAG_NOPHYSICALIZE)
					{
						continue;
					}

					start += n;
					if (start >= boneIdCount)
					{
						err = "bad or damaged remapping table (start >= boneIdCount)";
						goto errorDetected;
					}
					if (fi.chunkBoneIds[start] != 0)
					{
						err = "bad or damaged remapping table (fi.chunkBoneIds[start] != 0)";
						goto errorDetected;
					}
					n = 1;
					while (start + n < boneIdCount && fi.chunkBoneIds[start + n] != 0)
					{
						++n;
					}

					for (int i = 0; i < (int)chunk.nNumVerts; ++i)
					{
						const uint vIdx = chunk.nFirstVertId + i;
						if (vIdx > (uint)fi.nSkinnedVtx)
						{
							err = "bad or damaged subset data";
							goto errorDetected;
						}

						for (int jj = 0; jj < 4; ++jj)
						{
							const uint localBoneId = pBoneMapping[vIdx].boneIds[jj];
							if (localBoneId >= n)
							{
								err = "bad or damaged skinning data";
								goto errorDetected;
							}
							pBoneMapping[vIdx].boneIds[jj] = fi.chunkBoneIds[start + localBoneId];
						}
					}
				}
			}

errorDetected:
			if (err)
			{
				CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING,
				           "Vegetation object[%s]: %s", m_szFileName.c_str(), err);

				for (int i = 0; i < fi.nSkinnedVtx; ++i)
				{
					for (int jj = 0; jj < 4; ++jj)
					{
						pBoneMapping[i].boneIds[jj] = 0;
					}
				}
			}
		}

		m_chunkBoneIds.swap(chunkBoneIds);
		m_pBoneMapping = pBoneMapping;
		m_nSpines = fi.nSpines;
		m_pSpines = pSpines;
		return;
	}
#ifdef RUNTIME_ANALYZE_FOLIAGE
	if (!pRenderMesh)
		return;
	int i, j, isle, iss, ivtx0, nVtx, itri, ivtx, iedge, ibran0, ibran1, ibran, nBranches, ispine, ispineDst, nGlobalBones,
	    * pIdxSorted, * pForeignIdx;
	Vec3 pt[7], edge, edge1, n, nprev, axisx, axisy;
	Vec2 tex[6];
	float t, s, mindist, k, denom, len, dist[3], e, t0, t1, denomPrev;
	unsigned int* pUsedTris, * pUsedVtx;
	char sname[64], * pname = sname;
	Matrix34 tm;
	mesh_data* pmd;
	IGeometry* pPhysGeom;
	SBranch branch, * pBranches;
	primitives::box bbox;

	int nMeshVtx, nMeshIdx;
	unsigned short* pMeshIdx;
	TRenderChunkArray& meshChunks = pRenderMesh->GetChunks();
	nMeshVtx = pRenderMesh->GetVerticesCount();
	nMeshIdx = pRenderMesh->GetIndicesCount();
	strided_pointer<Vec3> pMeshVtx;
	strided_pointer<Vec2> pMeshTex;
	pMeshVtx.data = (Vec3*)pRenderMesh->GetPosPtr(pMeshVtx.iStride, FSL_READ);
	pMeshTex.data = (Vec2*)pRenderMesh->GetUVPtr(pMeshTex.iStride, FSL_READ);
	pMeshIdx = pRenderMesh->GetIndexPtr(FSL_READ);

	// iterate through branch points branch#_#, build a branch structure
	tm.SetIdentity();
	if (pHelpersHost != this)
	{
		cry_sprintf(sname, "%s_", m_szGeomName.c_str());
		pname += strlen(sname);
		CNodeCGF* pNode;
		for (i = pCGF->GetNodeCount() - 1; i >= 0; i--)
			if (!strcmp((pNode = pCGF->GetNode(i))->name, m_szGeomName))
			{
				for (tm = pNode->localTM; pNode->pParent; pNode = pNode->pParent)
					tm = pNode->pParent->localTM * tm;
				tm.Invert();
				break;
			}
	}
	pBranches = 0;
	nBranches = 0;
	do
	{
		branch.pt = 0;
		branch.npt = 0;
		branch.len = 0;
		do
		{
			cry_sprintf(pname, "branch%d_%d", nBranches + 1, branch.npt + 1);
			if ((pt[0] = pHelpersHost->GetHelperPos(sname)).len2() == 0)
				break;
			if ((branch.npt & 15) == 0)
				branch.pt = (SBranchPt*)realloc(branch.pt, (branch.npt + 16) * sizeof(SBranchPt));
			branch.pt[branch.npt].minDist = 1;
			branch.pt[branch.npt].minDenom = 0;
			branch.pt[branch.npt].itri = -1;
			branch.pt[branch.npt++].pt = tm * pt[0];
			branch.len += (pt[0] - branch.pt[max(0, branch.npt - 2)].pt).len();
		}
		while (true);
		if (branch.npt < 2)
		{
			if (branch.pt) free(branch.pt);
			break;
		}
		if ((nBranches & 15) == 0)
		{
			// cppcheck-suppress memleakOnRealloc
			pBranches = (SBranch*)realloc(pBranches, (nBranches + 16) * sizeof(SBranch));
		}
		pBranches[nBranches++] = branch;
		// make sure the segments have equal length
		len = branch.len / (branch.npt - 1);
		for (i = 1; i < branch.npt; i++)
			branch.pt[i].pt = branch.pt[i - 1].pt + (branch.pt[i].pt - branch.pt[i - 1].pt).normalized() * len;
	}
	while (true);

	if (nBranches == 0)
		return;
	SSpine spine;
	memset(&spine, 0, sizeof spine);
	spine.bActive = true;
	memset(m_pBoneMapping = new SMeshBoneMapping_uint8[nMeshVtx], 0, nMeshVtx * sizeof(m_pBoneMapping[0]));
	for (i = 0; i < nMeshVtx; i++)
		m_pBoneMapping[i].weights[0] = 255;
	pIdxSorted = new int[nMeshVtx];
	nVtx = 0;
	pForeignIdx = new int[nMeshIdx / 3];
	pUsedTris = new unsigned int[i = ((nMeshIdx - 1) >> 5) + 1];
	memset(pUsedTris, 0, i * sizeof(int));
	pUsedVtx = new unsigned int[i = ((nMeshVtx - 1) >> 5) + 1];
	memset(pUsedVtx, 0, i * sizeof(int));

	// iterate through all triangles, track the closest for each branch point
	for (iss = 0; iss < (int)meshChunks.size(); iss++)
		if ((meshChunks[iss].m_nMatFlags & (MTL_FLAG_NOPHYSICALIZE | MTL_FLAG_NODRAW)) == MTL_FLAG_NOPHYSICALIZE)
			for (i = meshChunks[iss].nFirstIndexId; i < meshChunks[iss].nFirstIndexId + meshChunks[iss].nNumIndices; i += 3)
			{
				for (j = 0; j < 3; j++)
					pt[j] = pMeshVtx[pMeshIdx[i + j]];
				n = pt[1] - pt[0] ^ pt[2] - pt[0];
				for (ibran = 0; ibran < nBranches; ibran++)
					for (ivtx0 = 0; ivtx0 < pBranches[ibran].npt; ivtx0++)
						if (UpdatePtTriDist(pt, n, pBranches[ibran].pt[ivtx0].pt, pBranches[ibran].pt[ivtx0].minDist, pBranches[ibran].pt[ivtx0].minDenom))
							pBranches[ibran].pt[ivtx0].itri = i;
			}

	for (ibran = 0; ibran < nBranches; ibran++)
		for (i = 0; i < pBranches[ibran].npt; i++)
			if (pBranches[ibran].pt[i].itri >= 0)
			{
				// get the closest point on the triangle for each vertex, get tex coords from it
				for (j = 0; j < 3; j++)
					pt[j] = pMeshVtx[pMeshIdx[pBranches[ibran].pt[i].itri + j]];
				n = pt[1] - pt[0] ^ pt[2] - pt[0];
				pt[3] = pBranches[ibran].pt[i].pt;
				for (j = 0; j < 3 && (pt[incm3(j)] - pt[j] ^ pt[3] - pt[j]) * n > 0; j++)
					;
				if (j == 3)
				{
					// the closest pt is in triangle interior
					denom = 1.0f / n.len2();
					pt[3] -= n * (n * (pt[3] - pt[0])) * denom;
					for (j = 0, s = 0, t = 0; j < 3; j++)
					{
						k = ((pt[3] - pt[incm3(j)] ^ pt[3] - pt[decm3(j)]) * n) * denom;
						s += pMeshTex[pMeshIdx[pBranches[ibran].pt[i].itri + j]].x * k;
						t += pMeshTex[pMeshIdx[pBranches[ibran].pt[i].itri + j]].y * k;
					}
				}
				else
				{
					for (j = 0; j < 3 && !inrange((pt[incm3(j)] - pt[j]) * (pt[3] - pt[j]), 0.0f, (pt[incm3(j)] - pt[j]).len2()); j++)
						;
					if (j < 3)
					{
						// the closest pt in on an edge
						k = ((pt[3] - pt[j]) * (pt[incm3(j)] - pt[j])) / (pt[incm3(j)] - pt[j]).len2();
						s = pMeshTex[pMeshIdx[pBranches[ibran].pt[i].itri + j]].x * (1 - k) +
						    pMeshTex[pMeshIdx[pBranches[ibran].pt[i].itri + incm3(j)]].x * k;
						t = pMeshTex[pMeshIdx[pBranches[ibran].pt[i].itri + j]].y * (1 - k) +
						    pMeshTex[pMeshIdx[pBranches[ibran].pt[i].itri + incm3(j)]].y * k;
					}
					else
					{
						// the closest pt is a vertex
						for (j = 0; j < 3; j++)
							dist[j] = (pt[j] - pt[3]).len2();
						j = idxmin3(dist);
						s = pMeshTex[pMeshIdx[pBranches[ibran].pt[i].itri + j]].x;
						t = pMeshTex[pMeshIdx[pBranches[ibran].pt[i].itri + j]].y;
					}
				}
				pBranches[ibran].pt[i].tex.set(s, t);
			}

	int nBones;
	for (iss = 0, nGlobalBones = 1; iss < (int)meshChunks.size(); iss++, nGlobalBones += nBones - 1)
		if ((meshChunks[iss].m_nMatFlags & (MTL_FLAG_NOPHYSICALIZE | MTL_FLAG_NODRAW)) == MTL_FLAG_NOPHYSICALIZE)
		{
			// fill the foreign idx list
			for (i = 0, j = meshChunks[iss].nFirstIndexId / 3; j*3 < meshChunks[iss].nFirstIndexId + meshChunks[iss].nNumIndices; j++, i++)
				pForeignIdx[i] = j;
			pPhysGeom = GetPhysicalWorld()->GetGeomManager()->CreateMesh(pMeshVtx, pMeshIdx + meshChunks[iss].nFirstIndexId, 0, pForeignIdx,
			                                                             meshChunks[iss].nNumIndices / 3, mesh_shared_vtx | mesh_SingleBB | mesh_keep_vtxmap);
			pmd = (mesh_data*)pPhysGeom->GetData();
			pPhysGeom->GetBBox(&bbox);
			e = sqr((bbox.size.x + bbox.size.y + bbox.size.z) * 0.002f);
			nBones = 1;
			m_chunkBoneIds.push_back(0);

			for (isle = 0; isle < pmd->nIslands; isle++)
				if (pmd->pIslands[isle].nTris >= 4)
				{
					ibran0 = 0;
					ibran1 = nBranches;
					ivtx = 0;
					spine.nVtx = 0;
					spine.pVtx = 0;
					spine.iAttachSpine = spine.iAttachSeg = -1;
					do
					{
						for (itri = pmd->pIslands[isle].itri, i = j = 0; i < pmd->pIslands[isle].nTris; itri = pmd->pTri2Island[itri].inext, i++)
						{
							for (j = 0; j < 3; j++)
								tex[j].set(pMeshTex[pMeshIdx[pmd->pForeignIdx[itri] * 3 + j]].x,
								           pMeshTex[pMeshIdx[pmd->pForeignIdx[itri] * 3 + j]].y);
							k = tex[1] - tex[0] ^ tex[2] - tex[0];
							s = fabs_tpl(k);
							for (ibran = ibran0, j = 0; ibran < ibran1; ibran++)
							{
								for (j = 0; j<3 && sqr_signed(tex[incm3(j)] - tex[j]^ pBranches[ibran].pt[ivtx].tex - tex[j])* k> -e * s * (tex[incm3(j)] - tex[j]).GetLength2(); j++)
									;
								if (j == 3) goto foundtri;
							}
						}
						break; // if no tri has the next point in texture space, skip the rest
foundtri:
						const int nMaxBones = 60;
						if (nBones + pBranches[ibran].npt - 1 > nMaxBones)
							break;

						// output phys vtx
						if (!spine.pVtx)
							spine.pVtx = new Vec3[pBranches[ibran].npt];
						denom = 1.0f / (tex[1] - tex[0] ^ tex[2] - tex[0]);
						tex[3] = pBranches[ibran].pt[ivtx].tex;
						for (j = 0, pt[0].zero(); j < 3; j++)
							pt[0] += pmd->pVertices[pmd->pIndices[itri * 3 + j]].scale((tex[3] - tex[incm3(j)] ^ tex[3] - tex[decm3(j)])) * denom;
						spine.pVtx[ivtx++] = pt[0];

						ibran0 = ibran;
						ibran1 = ibran + 1;
					}
					while (ivtx < pBranches[ibran].npt);

					if (ivtx < 3)
					{
						if (spine.pVtx) delete[] spine.pVtx;
						continue;
					}
					spine.nVtx = ivtx;
					spine.pVtxCur = new Vec3[spine.nVtx];
					spine.pSegDim = new Vec4[spine.nVtx];
					spine.idmat = idmat;

					// enforce equal length for phys rope segs
					for (i = 0, len = 0; i < spine.nVtx - 1; i++)
						len += (spine.pVtx[i + 1] - spine.pVtx[i]).len();
					spine.len = len;
					for (i = 1, len /= (spine.nVtx - 1); i < spine.nVtx; i++)
						spine.pVtx[i] = spine.pVtx[i - 1] + (spine.pVtx[i] - spine.pVtx[i - 1]).normalized() * len;

					// append island's vertices to the vertex index list
					for (itri = pmd->pIslands[isle].itri, i = 0, ivtx0 = nVtx, n.zero(); i < pmd->pIslands[isle].nTris; itri = pmd->pTri2Island[itri].inext, i++)
					{
						for (iedge = 0; iedge < 3; iedge++)
							if (!check_mask(pUsedVtx, pMeshIdx[pmd->pForeignIdx[itri] * 3 + iedge]))
								set_mask(pUsedVtx, pIdxSorted[nVtx++] = pMeshIdx[pmd->pForeignIdx[itri] * 3 + iedge]);
						nprev = pMeshVtx[pMeshIdx[pmd->pForeignIdx[itri] * 3 + 1]].sub(pMeshVtx[pMeshIdx[pmd->pForeignIdx[itri] * 3]]) ^
						        pMeshVtx[pMeshIdx[pmd->pForeignIdx[itri] * 3 + 2]].sub(pMeshVtx[pMeshIdx[pmd->pForeignIdx[itri] * 3]]);
						n += nprev * sgnnz(nprev.z);
					}
					spine.navg = n.normalized();

					nprev = spine.pVtx[1] - spine.pVtx[0];
					for (ivtx = 0; ivtx < spine.nVtx - 1; ivtx++)
					{
						// generate a separating plane between current seg and next seg	and move vertices that are sliced by it to the front
						n = edge = spine.pVtx[ivtx + 1] - spine.pVtx[ivtx];
						axisx = (n ^ spine.navg).normalized();
						axisy = axisx ^ n;
						spine.pSegDim[ivtx] = Vec4(0, 0, 0, 0);
						if (ivtx < spine.nVtx - 2)
							n += spine.pVtx[ivtx + 2] - spine.pVtx[ivtx + 1];
						denom = 1.0f / (edge * n);
						denomPrev = 1.0f / (edge * nprev);
						for (i = ivtx0; i < nVtx; i++)
							if ((t1 = (spine.pVtx[ivtx + 1].sub(pMeshVtx[pIdxSorted[i]])) * n) > 0)
							{
								t = (pMeshVtx[pIdxSorted[i]].sub(spine.pVtx[ivtx])) * axisx;
								spine.pSegDim[ivtx].x = min(t, spine.pSegDim[ivtx].x);
								spine.pSegDim[ivtx].y = max(t, spine.pSegDim[ivtx].y);
								t = (pMeshVtx[pIdxSorted[i]].sub(spine.pVtx[ivtx])) * axisy;
								spine.pSegDim[ivtx].z = min(t, spine.pSegDim[ivtx].z);
								spine.pSegDim[ivtx].w = max(t, spine.pSegDim[ivtx].w);
								t0 = ((spine.pVtx[ivtx].sub(pMeshVtx[pIdxSorted[i]])) * nprev) * denomPrev;
								t1 *= denom;
								t = min(1.0f, max(0.0f, 1.0f - fabs_tpl(t0 + t1) * 0.5f / (t1 - t0)));
								j = isneg(t0 + t1);
								m_pBoneMapping[pIdxSorted[i]].boneIDs[0] = nBones + min(spine.nVtx - 2, ivtx + j);
								m_pBoneMapping[pIdxSorted[i]].boneIDs[1] = nBones + max(0, ivtx - 1 + j);
								m_pBoneMapping[pIdxSorted[i]].weights[j ^ 1] = 255 - (m_pBoneMapping[pIdxSorted[i]].weights[j] = max(1, min(254, FtoI(t * 255))));
								j = pIdxSorted[ivtx0];
								pIdxSorted[ivtx0++] = pIdxSorted[i];
								pIdxSorted[i] = j;
							}
						nprev = n;
						m_chunkBoneIds.push_back(nGlobalBones + nBones - 1 + ivtx);
					}
					for (i = ivtx0; i < nVtx; i++)
					{
						m_pBoneMapping[pIdxSorted[i]].boneIDs[0] = nBones + spine.nVtx - 2;
						m_pBoneMapping[pIdxSorted[i]].weights[0] = 255;
					}

					if ((m_nSpines & 7) == 0)
						m_pSpines = (SSpine*)realloc(m_pSpines, (m_nSpines + 8) * sizeof(SSpine));
					m_pSpines[m_nSpines++] = spine;
					nBones += spine.nVtx - 1;
				}
			pPhysGeom->Release();
		}
		else
			nBones = 1;

	// for every spine, check if its base is close enough to a point on another spine
	for (ispine = 0; ispine < m_nSpines; ispine++)
		for (ispineDst = 0, pt[0] = m_pSpines[ispine].pVtx[0]; ispineDst < m_nSpines; ispineDst++)
			if (ispine != ispineDst && (ispineDst > ispine || m_pSpines[ispineDst].iAttachSeg != ispine))
			{
				mindist = sqr(m_pSpines[ispine].len * 0.05f);
				for (i = 0, j = -1; i < m_pSpines[ispineDst].nVtx - 1; i++)
				{
					// find the point on this seg closest to pt[0]
					if ((t = (m_pSpines[ispineDst].pVtx[i + 1] - pt[0]).len2()) < mindist)
						mindist = t, j = i; // end vertex
					edge = m_pSpines[ispineDst].pVtx[i + 1] - m_pSpines[ispineDst].pVtx[i];
					edge1 = pt[0] - m_pSpines[ispineDst].pVtx[i];
					if (inrange(edge1 * edge, edge.len2() * -0.3f * ((i - 1) >> 31), edge.len2()) && (t = (edge ^ edge1).len2()) < mindist * edge.len2())
						mindist = t / edge.len2(), j = i; // point on edge
				}
				if (j >= 0)
				{
					// attach ispine to a point on iSpineDst
					m_pSpines[ispine].iAttachSpine = ispineDst;
					m_pSpines[ispine].iAttachSeg = j;
					break;
				}
			}

	delete[] pUsedVtx;
	delete[] pUsedTris;
	delete[] pForeignIdx;
	delete[] pIdxSorted;
	spine.pVtx = spine.pVtxCur = 0;
	spine.pSegDim = 0;

#endif
}

int CStatObj::PhysicalizeFoliage(IPhysicalEntity* pTrunk, const Matrix34& mtxWorld, IFoliage*& pIRes, float lifeTime, int iSource)
{
	if (gEnv->IsDedicated() || !m_pSpines || GetCVars()->e_PhysFoliage < 1 + (pTrunk && pTrunk->GetType() == PE_STATIC))
		return 0;
	CStatObjFoliage* pRes = (CStatObjFoliage*)pIRes, * pFirstFoliage = Get3DEngine()->m_pFirstFoliage;
	int i, j;
	pe_params_flags pf;
	pe_simulation_params sp;

	if (pIRes)
	{
		pRes->m_timeIdle = 0;
		if (iSource & 4)
		{
			pf.flagsAND = ~pef_ignore_areas;
			pe_action_awake aa;
			for (i = 0; i < m_nSpines; i++)
				if (pRes->m_pRopes[i])
					pRes->m_pRopes[i]->Action(&aa), pRes->m_pRopes[i]->SetParams(&pf);
			MARK_UNUSED pf.flagsAND;
		}
		pRes->m_iActivationSource |= iSource & 2;
		if (!(iSource & 1) && (pRes->m_iActivationSource & 1))
		{
			pRes->m_iActivationSource = iSource;
			pf.flagsOR = rope_collides;
			sp.minEnergy = sqr(0.1f);
			if (pTrunk && pTrunk->GetType() != PE_STATIC)
				pf.flagsOR |= rope_collides_with_terrain;
			for (i = 0; i < m_nSpines; i++)
				if (pRes->m_pRopes[i])
					pRes->m_pRopes[i]->SetParams(&pf), pRes->m_pRopes[i]->SetParams(&sp);
		}
		return 0;
	}

	pRes = new CStatObjFoliage;
	AddRef();

	pRes->m_next = pFirstFoliage;
	pRes->m_prev = pFirstFoliage->m_prev;
	pFirstFoliage->m_prev->m_next = pRes;
	pFirstFoliage->m_prev = pRes;
	pRes->m_lifeTime = lifeTime;
	pRes->m_ppThis = &pIRes;
	pRes->m_iActivationSource = iSource;

	pRes->m_pStatObj = this;
	pRes->m_pTrunk = pTrunk;
	pRes->m_pRopes = new IPhysicalEntity*[m_nSpines];
	memset(pRes->m_pRopes, 0, m_nSpines * sizeof(pRes->m_pRopes[0]));
	for (i = 0, pRes->m_pRopesActiveTime = new float[m_nSpines]; i < m_nSpines; i++)
		pRes->m_pRopesActiveTime[i] = -1.0f;
	pRes->m_nRopes = m_nSpines;

	pe_params_pos pp;
	pe_params_rope pr, pra;
	pe_action_target_vtx atv;
	pe_params_foreign_data pfd;
	pe_params_timeout pto;
	pe_params_outer_entity poe;

	sp.minEnergy = sqr(0.1f);
	if (poe.pOuterEntity = pTrunk)
		pTrunk->SetParams(&sp);

	pp.pos = mtxWorld.GetTranslation();
	float collDist0 = 0.03f, collDist1 = 0.07f;
	if (const char* thickness = strstr(m_szProperties, "thickness"))
		collDist0 = atof(thickness + 9), collDist1 = 0;
	pr.bTargetPoseActive = 2;
	pr.stiffnessAnim = GetFloatCVar(e_FoliageBranchesStiffness);
	if (const char* stiffness = strstr(m_szProperties, "stiffness"))
		pr.stiffnessAnim = atof(stiffness + 9);
	pRes->m_stiffness = pr.stiffnessAnim;
	pr.friction = 2.0f;
	pr.flagsCollider = geom_colltype_foliage;
	pr.windVariance = 0.3f;
	pr.airResistance = GetCVars()->e_FoliageWindActivationDist > 0 ? 1.0f : 0.0f;
	pr.noCollDist = 0.15f;
	pr.sensorRadius = 0.0f;
	j = -(iSource & 1 ^ 1);
	pf.flagsOR = rope_collides & j | rope_target_vtx_rel0 | rope_ignore_attachments | pef_traceable;
	if (pTrunk && pTrunk->GetType() != PE_STATIC)
	{
		sp.maxTimeStep = 0.05f; // 0.02f
		pf.flagsOR |= rope_collides_with_terrain & j;
		pr.dampingAnim = GetFloatCVar(e_FoliageBrokenBranchesDamping);
	}
	else
	{
		sp.maxTimeStep = 0.05f;
		if (GetCVars()->e_PhysFoliage < 3)
			pf.flagsOR |= pef_ignore_areas, sp.gravity.zero();
		pr.dampingAnim = GetFloatCVar(e_FoliageBranchesDamping);
		if (const char* damping = strstr(m_szProperties, "damping"))
			pr.dampingAnim = atof(damping + 7);
	}
	pRes->m_minEnergy = sp.minEnergy = sqr(0.01f - 0.09f * j);
	pfd.iForeignData = PHYS_FOREIGN_ID_FOLIAGE;
	pfd.pForeignData = pRes;
	pto.maxTimeIdle = GetCVars()->e_FoliageBranchesTimeout;
	if (const char* timeout = strstr(m_szProperties, "timeout"))
		pto.maxTimeIdle = atof(timeout + 7);
	AABB bbox(AABB::RESET);
	for (i = 0; i < m_nSpines; i++)
		if (m_pSpines[i].bActive)
			for (j = 0; j < m_pSpines[i].nVtx; j++)
				bbox.Add(m_pSpines[i].pVtx[j]);
	pr.collisionBBox[0] = bbox.min;
	pr.collisionBBox[1] = bbox.max;

	for (i = 0; i < m_nSpines; i++)
		if (m_pSpines[i].bActive)
		{
			pRes->m_pRopes[i] = GetPhysicalWorld()->CreatePhysicalEntity(PE_ROPE, &pp);
			pr.pPoints = strided_pointer<Vec3>(m_pSpines[i].pVtxCur);
			pr.nSegments = (atv.nPoints = m_pSpines[i].nVtx) - 1;
			for (j = 0; j < m_pSpines[i].nVtx; j++)
				pr.pPoints[j] = mtxWorld * m_pSpines[i].pVtx[j];
			pr.surface_idx = m_pSpines[i].idmat;
			pr.collDist = collDist0 + collDist1 * isneg(3.0f - m_pSpines[i].len);
			pfd.iForeignFlags = i;
			pRes->m_pRopes[i]->SetParams(&pr);
			pRes->m_pRopes[i]->SetParams(&pf);
			pRes->m_pRopes[i]->SetParams(&sp);
			pRes->m_pRopes[i]->SetParams(&pto);
			pRes->m_pRopes[i]->SetParams(&poe);

			if (m_pSpines[i].iAttachSpine < 0)
				atv.points = m_pSpines[i].pVtx;
			else
			{
				int ivtx = m_pSpines[i].iAttachSeg;
				Vec3 pos0 = m_pSpines[j = m_pSpines[i].iAttachSpine].pVtx[ivtx];
				Quat q0 = Quat::CreateRotationV0V1(Vec3(0, 0, -1),
				                                   mtxWorld.TransformVector((m_pSpines[j].pVtx[ivtx + 1] - m_pSpines[j].pVtx[ivtx])).normalized());
				atv.points = m_pSpines[i].pVtxCur;
				for (j = 0; j < m_pSpines[i].nVtx; j++)
					atv.points[j] = mtxWorld.TransformVector(m_pSpines[i].pVtx[j] - pos0) * q0;
			}
			pRes->m_pRopes[i]->Action(&atv);
			pRes->m_pRopes[i]->SetParams(&pfd, 1);
		}
		else
			pRes->m_pRopes[i] = 0;

	for (i = 0; i < m_nSpines; i++)
		if (pRes->m_pRopes[i])
		{
			if (m_pSpines[i].iAttachSpine >= 0)
			{
				pra.pEntTiedTo[0] = pRes->m_pRopes[m_pSpines[i].iAttachSpine];
				pra.idPartTiedTo[0] = m_pSpines[i].iAttachSeg;
			}
			else
			{
				pra.pEntTiedTo[0] = pTrunk;
				MARK_UNUSED pra.idPartTiedTo[0];
			}
			pRes->m_pRopes[i]->SetParams(&pra, -1);
		}

	if (!gEnv->IsDedicated() && m_pRenderMesh && !m_pRenderMesh->GetChunksSkinned().size() && m_pRenderMesh->GetVerticesCount())
	{
		m_pRenderMesh->CreateChunksSkinned();

		TRenderChunkArray& meshChunks = m_pRenderMesh->GetChunksSkinned();
		for (i = 0; i < (int)meshChunks.size(); i++)
		{
			if ((meshChunks[i].m_nMatFlags & (MTL_FLAG_NOPHYSICALIZE | MTL_FLAG_NODRAW)) == MTL_FLAG_NOPHYSICALIZE)
			{
				meshChunks[i].m_bUsesBones = true;
				if (meshChunks[i].pRE)
					meshChunks[i].pRE->mfUpdateFlags(FCEF_SKINNED);
			}
		}

		for (i = 0; i < (int)meshChunks.size(); i++)
			if (!meshChunks[i].m_bUsesBones)
				for (j = meshChunks[i].nFirstVertId; j < (int)meshChunks[i].nFirstVertId + (int)meshChunks[i].nNumVerts; j++)
					m_pBoneMapping[j].boneIds[0] = m_pBoneMapping[j].boneIds[1] = 0;

		if (0)
		{
			//--------------------------------------------------------------------------------------
			//--- this code is checking if all bone-mappings in the subsets are in a valid range ---
			//--------------------------------------------------------------------------------------
			for (uint32 s = 0; s < (uint32)meshChunks.size(); s++)
			{
				const uint32 nStartIndex = meshChunks[s].nFirstVertId;
				const uint32 nNumVertices = meshChunks[s].nNumVerts;

				if (meshChunks[s].m_bUsesBones)
				{
					for (i = nStartIndex; i < (int)(nStartIndex + nNumVertices); i++)
					{
						uint32 v = i;
						// get bone-ids
						uint16 b0 = m_pBoneMapping[v].boneIds[0];
						uint16 b1 = m_pBoneMapping[v].boneIds[1];
						uint16 b2 = m_pBoneMapping[v].boneIds[2];
						uint16 b3 = m_pBoneMapping[v].boneIds[3];
						// get weights
						const uint8 w0 = m_pBoneMapping[v].weights[0];
						const uint8 w1 = m_pBoneMapping[v].weights[1];
						const uint8 w2 = m_pBoneMapping[v].weights[2];
						const uint8 w3 = m_pBoneMapping[v].weights[3];
						// if weight is zero set bone ID to zero as the bone has no influence anyway,
						if (w0 == 0) b0 = 0;
						if (w1 == 0) b1 = 0;
						if (w2 == 0) b2 = 0;
						if (w3 == 0) b3 = 0;
						static const int maxBoneIdx = 255;
						assert(b0 <= maxBoneIdx);
						assert(b1 <= maxBoneIdx);
						assert(b2 <= maxBoneIdx);
						assert(b3 <= maxBoneIdx);

						/*
						   if (b0 > maxBoneIdx)
						   CryFatalError("Fatal Error: index out of range: Index: %d   maxBoneIdx: %d (contact: Anton@Crytek.de or Ivo@Crytek.de)",b0,maxBoneIdx );
						   if (b1 > maxBoneIdx)
						   CryFatalError("Fatal Error: index out of range: Index: %d   maxBoneIdx: %d (contact: Anton@Crytek.de or Ivo@Crytek.de)",b1,maxBoneIdx );
						   if (b2 > maxBoneIdx)
						   CryFatalError("Fatal Error: index out of range: Index: %d   maxBoneIdx: %d (contact: Anton@Crytek.de or Ivo@Crytek.de)",b2,maxBoneIdx );
						   if (b3 > maxBoneIdx)
						   CryFatalError("Fatal Error: index out of range: Index: %d   maxBoneIdx: %d (contact: Anton@Crytek.de or Ivo@Crytek.de)",b3,maxBoneIdx );
						 */
					}
				}
			}
		}

		m_pRenderMesh->SetSkinningDataVegetation(m_pBoneMapping);
	}

	pIRes = pRes;
	return m_nSpines;
}

////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////// CStatObjFoliage /////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////

CStatObjFoliage::~CStatObjFoliage()
{
	if (m_pRopes)
	{
		for (int i = 0; i < m_nRopes; i++)
			if (m_pRopes[i])
				gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_pRopes[i]);
		delete[] m_pRopes;
		delete[] m_pRopesActiveTime;
	}
	delete[] m_pSkinningTransformations[0];
	delete[] m_pSkinningTransformations[1];
	m_next->m_prev = m_prev;
	m_prev->m_next = m_next;
	if (m_pStatObj)
		m_pStatObj->Release();
	if (*m_ppThis == this)
		*m_ppThis = nullptr;
	if (!m_bDelete)
	{
		if (m_pRenderObject)
		{
			m_pRenderObject->m_ObjFlags &= ~(FOB_SKINNED);
			threadID nThreadID = 0;
			gEnv->pRenderer->EF_Query(EFQ_MainThreadList, nThreadID);
			if (SRenderObjData* pOD = m_pRenderObject->GetObjData())
				pOD->m_pSkinningData = NULL;
		}
	}
}

uint32 CStatObjFoliage::ComputeSkinningTransformationsCount()
{
	uint32 count = 1;
	uint32 spineCount = uint32(m_pStatObj->m_nSpines);
	for (uint32 i = 0; i < spineCount; ++i)
		count += uint32(m_pStatObj->m_pSpines[i].nVtx - 1);
	return count;
}

void CStatObjFoliage::ComputeSkinningTransformations(uint32 nList)
{
	uint32 jointCount = ComputeSkinningTransformationsCount();
	if (!m_pSkinningTransformations[0])
	{
		m_pSkinningTransformations[0] = new QuatTS[jointCount];
		m_pSkinningTransformations[1] = new QuatTS[jointCount];
		memset(m_pSkinningTransformations[0], 0, jointCount * sizeof(QuatTS));
		memset(m_pSkinningTransformations[1], 0, jointCount * sizeof(QuatTS));
	}

	QuatTS* pPose = m_pSkinningTransformations[nList];

	pe_status_rope sr;
	Vec3 point0, point1;
	uint32 jointIndex = 1;
	uint32 spineCount = uint32(m_pStatObj->m_nSpines);
	for (uint32 i = 0; i < spineCount; ++i)
	{
		if (!m_pRopes[i])
		{
			jointIndex += uint32(m_pStatObj->m_pSpines[i].nVtx - 1);
			continue;
		}

		sr.pPoints = NULL;
		m_pRopes[i]->GetStatus(&sr);
		if (sr.timeLastActive > m_pRopesActiveTime[i])
		{
			m_pRopesActiveTime[i] = sr.timeLastActive;

			// We use the scalar component of the first joint for this section
			// to mark the NEED for further computation.
			assert(jointIndex < jointCount);
			PREFAST_ASSUME(jointIndex < jointCount);
			pPose[jointIndex].s = 1.0f;

			sr.pPoints = m_pStatObj->m_pSpines[i].pVtxCur;
			m_pRopes[i]->GetStatus(&sr);
			point0 = sr.pPoints[0];
			uint32 spineVertexCount = m_pStatObj->m_pSpines[i].nVtx - 1;
			for (uint32 j = 0; j < spineVertexCount; ++j)
			{
				assert(jointIndex < jointCount);
				PREFAST_ASSUME(jointIndex < jointCount);

				point1 = sr.pPoints[j + 1];

				Vec3& p0 = pPose[jointIndex].t;
				Vec3& p1 = *(Vec3*)&pPose[jointIndex].q;

				p0 = point0;
				p1 = point1;

				point0 = point1;
				jointIndex++;
			}
		}
		else
		{
			uint32 count = m_pStatObj->m_pSpines[i].nVtx - 1;
			memcpy(&pPose[jointIndex], &m_pSkinningTransformations[(nList + 1) & 1][jointIndex], count * sizeof(QuatTS));

			// SF: no need to do this! Causes a bug, where unprocessed data can be
			// copied from the other list and then marked as not needing processing!
			// pPose[jointIndex].s = 0.0f;

			jointIndex += count;
		}
	}
}

SSkinningData* CStatObjFoliage::GetSkinningData(const Matrix34& RenderMat34, const SRenderingPassInfo& passInfo)
{
	int nList = passInfo.ThreadID();
	QuatTS* pPose = m_pSkinningTransformations[nList];
	if (pPose == 0)
		return NULL;

	if (m_pStatObj && m_pStatObj->m_pRenderMesh)
		m_pStatObj->m_pRenderMesh->NextDrawSkinned();

	int nNumBones = ComputeSkinningTransformationsCount();

	// get data to fill
	int nSkinningFrameID = passInfo.GetIRenderView()->GetSkinningPoolIndex();
	int nSkinningList = nSkinningFrameID % 3;
	int nPrevSkinningList = (nSkinningFrameID - 1) % 3;

	//int nCurrentFrameID = passInfo.GetFrameID();

	// before allocating new skinning date, check if we already have for this frame
	if (arrSkinningRendererData[nSkinningList].nFrameID == nSkinningFrameID && arrSkinningRendererData[nSkinningList].pSkinningData)
		return arrSkinningRendererData[nSkinningList].pSkinningData;

	//m_nFrameID = nCurrentFrameID;

	Matrix34 transformation = RenderMat34.GetInverted();

	pPose[0].q.SetIdentity();
	;
	pPose[0].t.x = 0.0f;
	pPose[0].t.y = 0.0f;
	pPose[0].t.z = 0.0f;
	pPose[0].s = 0.0f;

	assert(m_pStatObj);
	uint32 jointIndex = 1;
	uint32 spineCount = uint32(m_pStatObj->m_nSpines);
	for (uint32 i = 0; i < spineCount; ++i)
	{
		if (!m_pRopes[i])
		{
			uint32 spineVertexCount = m_pStatObj->m_pSpines[i].nVtx - 1;
			for (uint32 j = 0; j < spineVertexCount; ++j)
			{
				pPose[jointIndex].q.SetIdentity();
				pPose[jointIndex].t.x = 0.0f;
				pPose[jointIndex].t.y = 0.0f;
				pPose[jointIndex].t.z = 0.0f;
				pPose[jointIndex].s = 0.0f;

				jointIndex++;
			}
			continue;
		}

		// If the scalar component is not set no further computation is needed.
		if (pPose[jointIndex].s < 1.0f)
		{
			jointIndex += uint32(m_pStatObj->m_pSpines[i].nVtx - 1);
			continue;
		}

		uint32 spineVertexCount = m_pStatObj->m_pSpines[i].nVtx - 1;
		for (uint32 j = 0; j < spineVertexCount; ++j)
		{
			Vec3& p0 = pPose[jointIndex].t;
			Vec3& p1 = *(Vec3*)&pPose[jointIndex].q;

			p0 = transformation * p0;
			p1 = transformation * p1;

			pPose[jointIndex].q = Quat::CreateRotationV0V1(
			  (m_pStatObj->m_pSpines[i].pVtx[j + 1] - m_pStatObj->m_pSpines[i].pVtx[j]).normalized(),
			  (p1 - p0).normalized());
			pPose[jointIndex].t = p0 - pPose[jointIndex].q * m_pStatObj->m_pSpines[i].pVtx[j];

			*(DualQuat*)&m_pSkinningTransformations[nList ^ 1][jointIndex] =
			  *(DualQuat*)&pPose[jointIndex] = DualQuat(pPose[jointIndex].q, pPose[jointIndex].t);

			jointIndex++;
		}
	}

	// get data to fill
	assert(jointIndex == nNumBones);

	SSkinningData* pSkinningData = gEnv->pRenderer->EF_CreateSkinningData(passInfo.GetIRenderView(), jointIndex, false);
	assert(pSkinningData);
	PREFAST_ASSUME(pSkinningData);
	memcpy(pSkinningData->pBoneQuatsS, pPose, jointIndex * sizeof(DualQuat));
	arrSkinningRendererData[nSkinningList].pSkinningData = pSkinningData;
	arrSkinningRendererData[nSkinningList].nFrameID = nSkinningFrameID;

	// set data for motion blur
	if (arrSkinningRendererData[nPrevSkinningList].nFrameID == (nSkinningFrameID - 1) && arrSkinningRendererData[nPrevSkinningList].pSkinningData)
	{
		pSkinningData->nHWSkinningFlags |= eHWS_MotionBlured;
		pSkinningData->pPreviousSkinningRenderData = arrSkinningRendererData[nPrevSkinningList].pSkinningData;
	}
	else
	{
		// if we don't have motion blur data, use the some as for the current frame
		pSkinningData->pPreviousSkinningRenderData = pSkinningData;
	}

	return pSkinningData;
}

void CStatObjFoliage::Update(float dt, const CCamera& rCamera)
{
	if (!*m_ppThis && !m_bDelete)
		m_bDelete = 2;
	if (m_bDelete)
	{
		if (--m_bDelete <= 0)
			delete this;
		return;
	}
	FUNCTION_PROFILER_3DENGINE;

	int i, j, nContactEnts = 0, nColl;
	//int nContacts[4]={0,0,0,0};
	pe_status_rope sr;
	pe_params_bbox pbb;
	pe_status_dynamics sd;
	bool bHasBrokenRopes = false;
	IPhysicalEntity* pContactEnt, * pContactEnts[4];
	//	IPhysicalEntity *pMultiContactEnt=0;
	AABB bbox;
	bbox.Reset();

	for (i = nColl = 0; i < m_nRopes; i++)
		if (m_pRopes[i] && m_pRopes[i]->GetStatus(&sr))
		{
			pContactEnt = sr.pContactEnts[0];
			if (m_pVegInst && pContactEnt && pContactEnt->GetType() != PE_STATIC)
			{
				for (j = 0; j < nContactEnts && pContactEnts[j] != pContactEnt; j++)
					;
				if (j < nContactEnts)
				{
					//				if (++nContacts[j]*2>=m_pStatObj->m_nSpines)
					//					pMultiContactEnt = pContactEnt;
				}
				else if (nContactEnts < 4)
					pContactEnts[nContactEnts++] = pContactEnt;
			}
			nColl += sr.nCollDyn;
			if (m_flags & FLAG_FROZEN && sr.nCollStat + sr.nCollDyn)
				BreakBranch(i), bHasBrokenRopes = true;
			else
			{
				m_pRopes[i]->GetParams(&pbb);
				bbox.Add(pbb.BBox[0]);
				bbox.Add(pbb.BBox[1]);
			}
		}
		else
			bHasBrokenRopes = true;

	// disables destruction of non-frozen vegetation
	/*if (pMultiContactEnt && !(m_flags & FLAG_FROZEN) && pMultiContactEnt->GetStatus(&sd) && sd.mass>500 && (sd.v.len2()>25 || sd.w.len()>0.5))
	   {	// destroy the vegetation object, spawn particle effect
	   Vec3 sz = m_pVegInst->GetBBox().GetSize();
	   IParticleEffect *pDisappearEffect = m_pStatObj->GetSurfaceBreakageEffect( SURFACE_BREAKAGE_TYPE("destroy") );
	   if (pDisappearEffect)
	    pDisappearEffect->Spawn(true, IParticleEffect::ParticleLoc(m_pVegInst->GetBBox().GetCenter(),Vec3(0,0,1),(sz.x+sz.y+sz.z)*0.3f));
	   m_pStatObj->Get3DEngine()->m_pObjectsTree[nSID]->DeleteObject(m_pVegInst);
	   m_pVegInst->Dephysicalize();
	   return;
	   }*/

	float clipDist = GetCVars()->e_CullVegActivation;
	int bVisible = rCamera.IsAABBVisible_E(bbox);
	int bEnable = bVisible & isneg(((rCamera.GetPosition() - bbox.GetCenter()).len2() - sqr(clipDist)) * clipDist - 0.0001f);
	if (!bEnable)
	{
		if (inrange(m_timeInvisible += dt, 6.0f, 8.0f))
			bEnable = 1;
		else if (m_pTrunk && bVisible)
		{
			pe_status_awake psa;
			bEnable = m_pTrunk->GetStatus(&psa); // Is the trunk awake?
		}

	}
	else
		m_timeInvisible = 0;
	if (m_bEnabled != bEnable)
	{
		pe_params_flags pf;
		pf.flagsAND = ~pef_disabled;
		pf.flagsOR = pef_disabled & ~-bEnable;

		for (i = 0; i < m_nRopes; i++)
			if (m_pRopes[i])
				m_pRopes[i]->SetParams(&pf);
		m_bEnabled = bEnable;
	}

	pe_simulation_params sp;
	sp.minEnergy = sqr(nColl && bVisible ? 0.1f : 0.005f);
	if (sp.minEnergy != m_minEnergy)
		for (i = 0, m_minEnergy = sp.minEnergy; i < m_nRopes; i++)
			if (m_pRopes[i])
				m_pRopes[i]->SetParams(&sp);

	if (nColl && (m_iActivationSource & 2 || bVisible))
		m_timeIdle = 0;
	if (!bHasBrokenRopes && m_lifeTime > 0 && (m_timeIdle += dt) > m_lifeTime)
	{
		*m_ppThis = 0;
		m_bDelete = 2;
	}
	threadID nThreadID = 0;
	gEnv->pRenderer->EF_Query(EFQ_MainThreadList, nThreadID);
	ComputeSkinningTransformations(nThreadID);
}

void CStatObjFoliage::SetFlags(int flags)
{
	if (flags != m_flags)
	{
		pe_params_rope pr;
		if (flags & FLAG_FROZEN)
		{
			pr.collDist = 0.1f;
			pr.stiffnessAnim = 0;
		}
		else
		{
			pr.collDist = 0.03f;
			pr.stiffnessAnim = m_stiffness;
		}
		for (int i = 0; i < m_pStatObj->m_nSpines; i++)
			if (m_pRopes[i])
				m_pRopes[i]->SetParams(&pr);
		m_flags = flags;
	}
}

void CStatObjFoliage::OnHit(struct EventPhysCollision* pHit)
{
	if (m_flags & FLAG_FROZEN)
	{
		pe_params_foreign_data pfd;
		pHit->pEntity[1]->GetParams(&pfd);
		BreakBranch(pfd.iForeignFlags);
	}
}

void CStatObjFoliage::BreakBranch(int idx)
{
	if (m_pRopes[idx])
	{
		int i, nActiveRopes;

		IParticleEffect* pBranchBreakEffect = m_pStatObj->GetSurfaceBreakageEffect(SURFACE_BREAKAGE_TYPE("freeze_shatter"));
		if (pBranchBreakEffect)
		{
			float t, maxt, kb, kc, dim;
			float effStep = max(0.1f, m_pStatObj->m_pSpines[idx].len * 0.1f);
			Vec3 pos, dir;
			pe_status_rope sr;

			sr.pPoints = m_pStatObj->m_pSpines[idx].pVtxCur;
			m_pRopes[idx]->GetStatus(&sr);
			pos = sr.pPoints[0];
			i = 0;
			t = 0;
			dir = sr.pPoints[1] - sr.pPoints[0];
			dir /= (maxt = dir.len());
			do
			{
				pos = sr.pPoints[i] + dir * t;
				dim = max(fabs_tpl(m_pStatObj->m_pSpines[idx].pSegDim[i].y - m_pStatObj->m_pSpines[idx].pSegDim[i].x),
				          fabs_tpl(m_pStatObj->m_pSpines[idx].pSegDim[i].w - m_pStatObj->m_pSpines[idx].pSegDim[i].z));
				dim = max(0.05f, min(1.5f, dim));
				if ((t += effStep) > maxt)
				{
					if (++i >= sr.nSegments)
						break;
					dir = sr.pPoints[i + 1] - sr.pPoints[i];
					dir /= (maxt = dir.len());
					kb = dir * (sr.pPoints[i] - pos);
					kc = (sr.pPoints[i] - pos).len2() - sqr(effStep);
					pos = sr.pPoints[i] + dir * (t = kb + sqrt_tpl(max(1e-5f, kb * kb - kc)));
				}
				pBranchBreakEffect->Spawn(true, IParticleEffect::ParticleLoc(pos, (Vec3(0, 0, 1) - dir * dir.z).GetNormalized(), dim));
			}
			while (true);
		}

		if (!m_bGeomRemoved)
		{
			for (i = nActiveRopes = 0; i < m_pStatObj->m_nSpines; i++)
				nActiveRopes += m_pRopes[i] != 0;
			if (nActiveRopes * 3 < m_pStatObj->m_nSpines * 2)
			{
				pe_params_rope pr;
				pe_params_part pp;
				m_pRopes[idx]->GetParams(&pr);
				pp.flagsCond = geom_squashy;
				pp.flagsAND = 0;
				pp.flagsColliderAND = 0;
				pp.mass = 0;
				if (pr.pEntTiedTo[0])
					pr.pEntTiedTo[0]->SetParams(&pp);
				m_bGeomRemoved = 1;
			}
		}
		m_pStatObj->GetPhysicalWorld()->DestroyPhysicalEntity(m_pRopes[idx]);
		m_pRopes[idx] = 0;
		if (m_pStatObj->GetFlags() & (STATIC_OBJECT_CLONE))
			m_pStatObj->m_pSpines[idx].bActive = false;

		for (i = 0; i < m_pStatObj->m_nSpines; i++)
			if (m_pStatObj->m_pSpines[i].iAttachSpine == idx)
				BreakBranch(i);
	}
}

int CStatObjFoliage::Serialize(TSerialize ser)
{
	int i, nRopes = m_nRopes;
	bool bVal;
	ser.Value("nropes", nRopes);

	if (!ser.IsReading())
	{
		for (i = 0; i < m_nRopes; i++)
		{
			ser.BeginGroup("rope");
			if (m_pRopes[i])
			{
				ser.Value("active", bVal = true);
				m_pRopes[i]->GetStateSnapshot(ser);
			}
			else
				ser.Value("active", bVal = false);
			ser.EndGroup();
		}
	}
	else
	{
		if (m_nRopes != nRopes)
			return 0;
		for (i = 0; i < m_nRopes; i++)
		{
			ser.BeginGroup("rope");
			ser.Value("active", bVal);
			if (m_pRopes[i] && !bVal)
				m_pStatObj->GetPhysicalWorld()->DestroyPhysicalEntity(m_pRopes[i]), m_pRopes[i] = 0;
			else if (m_pRopes[i])
				m_pRopes[i]->SetStateFromSnapshot(ser);
			m_pRopesActiveTime[i] = -1;
			ser.EndGroup();
		}
	}

	return 1;
}

#pragma warning(pop)
