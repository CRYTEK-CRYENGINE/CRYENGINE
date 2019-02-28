// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   statobjconstr.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: creation
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "StatObj.h"

#include "IndexedMesh.h"
#include "../RenderDll/Common/Shadow_Renderer.h"
#include <CryRenderer/IRenderer.h>
#include <CryMemory/CrySizer.h>
#include "ObjMan.h"
#include "PolygonClipContext.h"
#include "MatMan.h"
#include "RenderMeshMerger.h"
#include "MergedMeshGeometry.h"

#define MAX_VERTICES_MERGABLE       15000
#define MAX_TRIS_IN_LOD_0           512
#define TRIS_IN_LOD_WARNING_RAIO    (1.5f)
// Minimal ratio of Lod(n-1)/Lod(n) polygons to consider LOD for sub-object merging.
#define MIN_TRIS_IN_MERGED_LOD_RAIO (1.5f)

DEFINE_INTRUSIVE_LINKED_LIST(CStatObj)

//////////////////////////////////////////////////////////////////////////
CStatObj::CStatObj()
{
	m_pAsyncUpdateContext = 0;
	m_nNodeCount = 0;
	m_nMergedMemoryUsage = 0;
	m_nUsers = 0; // reference counter
	m_nLastDrawMainFrameId = 0;
	m_nFlags = 0;
	m_pLODs = 0;
	m_lastBooleanOpScale = 1.f;
	m_fGeometricMeanFaceArea = 0.f;
	m_fLodDistance = 0.0f;

	Init();

#ifdef TRACE_CGF_LEAKS
	m_sLoadingCallstack = GetSystem()->GetLoadingProfilerCallstack();
#endif
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::Init()
{
	m_pAsyncUpdateContext = 0;
	m_pIndexedMesh = 0;
	m_lockIdxMesh = 0;
	m_nRenderTrisCount = m_nLoadedTrisCount = m_nLoadedVertexCount = 0;
	m_nRenderMatIds = 0;
	m_fRadiusHors = 0;
	m_fRadiusVert = 0;
	m_pParentObject = 0;
	m_pClonedSourceObject = 0;
	m_bVehicleOnlyPhysics = 0;
	m_bBreakableByGame = 0;
	m_idmatBreakable = -1;

	m_nLoadedLodsNum = 1;
	m_nMinUsableLod0 = 0;
	m_nMaxUsableLod0 = 0;
	m_nMaxUsableLod = 0;
	m_pLod0 = 0;

	m_aiVegetationRadius = -1.0f;
	m_phys_mass = -1.0f;
	m_phys_density = -1.0f;

	m_AABB = AABB(0.f);
	m_vVegCenter.Set(0, 0, 0);

	m_fGeometricMeanFaceArea = 0.f;
	m_fLodDistance = 0.0f;
	m_depthSortOffset.Set(0, 0, 0);

	m_pRenderMesh = 0;

	m_bDefaultObject = false;

	if (m_pLODs)
		for (int i = 0; i < MAX_STATOBJ_LODS_NUM; i++)
			if (m_pLODs[i])
				m_pLODs[i]->Init();

	m_pReadStream = 0;
	m_nModificationId = 0;
	m_nSubObjectMeshCount = 0;

	m_nRenderMeshMemoryUsage = 0;
	m_arrRenderMeshesPotentialMemoryUsage[0] = m_arrRenderMeshesPotentialMemoryUsage[1] = -1;

	m_bCanUnload = false;
	m_bLodsLoaded = false;
	m_bDefaultObject = false;
	m_bOpenEdgesTested = false;
	m_bSubObject = false;
	m_bSharesChildren = false;
	m_bHasDeformationMorphs = false;
	m_bTmpIndexedMesh = false;
	m_bMerged = false;
	m_bMergedLODs = false;
	m_bUnmergable = false;
	m_bLowSpecLod0Set = false;
	m_bHaveOcclusionProxy = false;
	m_bCheckGarbage = false;
	m_bLodsAreLoadedFromSeparateFile = false;
	m_bNoHitRefinement = false;
	m_bDontOccludeExplosions = false;
	m_isDeformable = false;
	m_isProxyTooBig = false;
	m_bHasStreamOnlyCGF = true;

	// Assign default material originally.
	m_pMaterial = GetMatMan()->GetDefaultMaterial();

	m_pLattice = 0;
	m_pSpines = 0;
	m_nSpines = 0;
	m_pBoneMapping = 0;
	m_pLastBooleanOp = 0;
	m_pMapFaceToFace0 = 0;
	m_pClothTangentsData = 0;
	m_pSkinInfo = 0;
	m_hasClothTangentsData = m_hasSkinInfo = 0;
	m_pDelayedSkinParams = 0;
	m_arrPhysGeomInfo.m_array.clear();

	m_nInitialSubObjHideMask = 0;

#if !defined (_RELEASE)
	m_fStreamingStart = 0.0f;
#endif

#ifdef OBJMAN_STREAM_STATS
	m_nStatoscopeState = 0;
#endif
}

//////////////////////////////////////////////////////////////////////////
CStatObj::~CStatObj()
{
	ShutDown();
}

void CStatObj::ShutDown()
{
	if (m_pReadStream)
	{
		// We don't need this stream anymore.
		m_pReadStream->Abort();
		m_pReadStream = NULL;
	}

	SAFE_DELETE(m_pAsyncUpdateContext);

	//	assert (IsHeapValid());

	SAFE_DELETE(m_pIndexedMesh);
	//	assert (IsHeapValid());

	for (int n = 0; n < m_arrPhysGeomInfo.GetGeomCount(); n++)
		if (m_arrPhysGeomInfo[n])
		{
			if (m_arrPhysGeomInfo[n]->pGeom->GetForeignData() == (void*)this)
				m_arrPhysGeomInfo[n]->pGeom->SetForeignData(0, 0);
			GetPhysicalWorld()->GetGeomManager()->UnregisterGeometry(m_arrPhysGeomInfo[n]);
		}
	m_arrPhysGeomInfo.m_array.clear();

	m_pStreamedRenderMesh = 0;
	m_pMergedRenderMesh = 0;
	SetRenderMesh(0);

	//	assert (IsHeapValid());

	/* // SDynTexture is not accessible for 3dengine

	   for(int i=0; i<FAR_TEX_COUNT; i++)
	    if(m_arrSpriteTexPtr[i])
	      m_arrSpriteTexPtr[i]->ReleaseDynamicRT(true);

	   for(int i=0; i<FAR_TEX_COUNT_60; i++)
	    if(m_arrSpriteTexPtr_60[i])
	      m_arrSpriteTexPtr_60[i]->ReleaseDynamicRT(true);
	 */

	SAFE_RELEASE(m_pLattice);

	if (m_pLODs)
		for (int i = 0; i < MAX_STATOBJ_LODS_NUM; i++)
			if (m_pLODs[i])
			{
				if (m_pLODs[i]->m_pParentObject)
					GetObjManager()->UnregisterForStreaming(m_pLODs[i]->m_pParentObject);
				else
					GetObjManager()->UnregisterForStreaming(m_pLODs[i]);

				// Sub objects do not own the LODs, so they should not delete them.
				m_pLODs[i] = 0;
			}

	//////////////////////////////////////////////////////////////////////////
	// Handle sub-objects and parents.
	//////////////////////////////////////////////////////////////////////////
	for (size_t i = 0; i < m_subObjects.size(); i++)
	{
		CStatObj* pChildObj = (CStatObj*)m_subObjects[i].pStatObj;
		if (pChildObj)
		{
			if (!m_bSharesChildren)
				pChildObj->m_pParentObject = NULL;
			GetObjManager()->UnregisterForStreaming(pChildObj);
			pChildObj->Release();
		}
	}
	m_subObjects.clear();

	if (m_pParentObject && !m_pParentObject->m_subObjects.empty())
	{
		// Remove this StatObject from sub-objects of the parent.
		SSubObject* pSubObjects = &m_pParentObject->m_subObjects[0];
		for (int i = 0, num = m_pParentObject->m_subObjects.size(); i < num; i++)
		{
			if (pSubObjects[i].pStatObj == this)
			{
				m_pParentObject->m_subObjects.erase(m_pParentObject->m_subObjects.begin() + i);
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////

	FreeFoliageData();

	if (m_pMapFaceToFace0)
		delete[] m_pMapFaceToFace0;
	m_pMapFaceToFace0 = 0;
	if (m_hasClothTangentsData && !m_pClonedSourceObject)
		delete[] m_pClothTangentsData;
	if (m_hasSkinInfo && !m_pClonedSourceObject)
		delete[] m_pSkinInfo;
	m_pClothTangentsData = 0;
	m_hasClothTangentsData = 0;
	m_pSkinInfo = 0;
	m_hasSkinInfo = 0;

	SAFE_DELETE(m_pDelayedSkinParams);

	SAFE_RELEASE(m_pClonedSourceObject);

	GetObjManager()->UnregisterForStreaming(this);

	SAFE_DELETE_ARRAY(m_pLODs);
}

//////////////////////////////////////////////////////////////////////////
int CStatObj::Release()
{
	// Has to be thread safe, as it can be called by a worker thread for deferred plane breaks
	int newRef = CryInterlockedDecrement(&m_nUsers);
	if (newRef <= 1)
	{
		if (m_pParentObject && m_pParentObject->m_nUsers <= 0)
		{
			GetObjManager()->CheckForGarbage(m_pParentObject);
		}
		if (newRef <= 0)
		{
			GetObjManager()->CheckForGarbage(this);
		}
	}
	return newRef;
}

//////////////////////////////////////////////////////////////////////////
void* CStatObj::operator new(size_t size)
{
	CObjManager* pObjManager = GetObjManager();
	assert(size == sizeof(CStatObj));
	return pObjManager->AllocateStatObj();
}

void CStatObj::operator delete(void* pToFree)
{
	CObjManager* pObjManager = GetObjManager();
	pObjManager->FreeStatObj((CStatObj*)pToFree);
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::FreeIndexedMesh()
{
	if (!m_lockIdxMesh)
	{
		WriteLock lock(m_lockIdxMesh);
		delete m_pIndexedMesh;
		m_pIndexedMesh = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::CalcRadiuses()
{
	if (!m_AABB.min.IsValid() || !m_AABB.max.IsValid())
	{
		Error("CStatObj::CalcRadiuses: Invalid bbox, File name: %s", m_szFileName.c_str());
		m_AABB.min.zero();
		m_AABB.max.zero();
	}

	float dxh = (float)max(fabs(GetBoxMax().x), fabs(GetBoxMin().x));
	float dyh = (float)max(fabs(GetBoxMax().y), fabs(GetBoxMin().y));
	m_fRadiusHors = (float)sqrt_tpl(dxh * dxh + dyh * dyh);
	m_fRadiusVert = (GetBoxMax().z - 0) * 0.5f;// never change this
	m_vVegCenter = m_AABB.GetCenter();
	m_vVegCenter.z = m_fRadiusVert;
}

void CStatObj::MakeRenderMesh()
{
	if (gEnv->IsDedicated())
		return;

	FUNCTION_PROFILER_3DENGINE;

	SetRenderMesh(0);

	if (!m_pIndexedMesh || m_pIndexedMesh->GetSubSetCount() == 0)
		return;

	CMesh* pMesh = m_pIndexedMesh->GetMesh();

	m_nRenderTrisCount = 0;
	//////////////////////////////////////////////////////////////////////////
	// Initialize Mesh subset material flags.
	//////////////////////////////////////////////////////////////////////////
	for (int i = 0; i < pMesh->GetSubSetCount(); i++)
	{
		SMeshSubset& subset = pMesh->m_subsets[i];
		if (!(subset.nMatFlags & MTL_FLAG_NODRAW))
		{
			m_nRenderTrisCount += subset.nNumIndices / 3;
		}
	}
	//////////////////////////////////////////////////////////////////////////
	if (!m_nRenderTrisCount)
		return;

	if (!(GetFlags() & STATIC_OBJECT_DYNAMIC))
		m_pRenderMesh = GetRenderer()->CreateRenderMesh("StatObj_Static", GetFilePath(), NULL, eRMT_Static);
	else
	{
		m_pRenderMesh = GetRenderer()->CreateRenderMesh("StatObj_Dynamic", GetFilePath(), NULL, eRMT_Dynamic);
		m_pRenderMesh->KeepSysMesh(true);
	}

	SMeshBoneMapping_uint16* const pBoneMap = pMesh->m_pBoneMapping;
	pMesh->m_pBoneMapping = 0;
	uint32 nFlags = 0;
	nFlags |= (!GetCVars()->e_StreamCgf && Get3DEngine()->m_bInLoad) ? FSM_SETMESH_ASYNC : 0;
	m_pRenderMesh->SetMesh(*pMesh, 0, nFlags, NULL, false);
	pMesh->m_pBoneMapping = pBoneMap;
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::SetMaterial(IMaterial* pMaterial)
{
	m_pMaterial = pMaterial;
}

///////////////////////////////////////////////////////////////////////////////////////
Vec3 CStatObj::GetHelperPos(const char* szHelperName)
{
	SSubObject* pSubObj = FindSubObject(szHelperName);
	if (!pSubObj)
		return Vec3(0, 0, 0);

	return Vec3(pSubObj->tm.m03, pSubObj->tm.m13, pSubObj->tm.m23);
}

///////////////////////////////////////////////////////////////////////////////////////
const Matrix34& CStatObj::GetHelperTM(const char* szHelperName)
{
	SSubObject* pSubObj = FindSubObject(szHelperName);

	if (!pSubObj)
	{
		static Matrix34 identity(IDENTITY);
		return identity;
	}

	return pSubObj->tm;
}

bool CStatObj::IsSameObject(const char* szFileName, const char* szGeomName)
{
	// cmp object names
	if (szGeomName)
	{
		if (stricmp(szGeomName, m_szGeomName) != 0)
			return false;
	}

	// Normilize file name
	char szFileNameNorm[MAX_PATH_LENGTH] = "";
	char* pszDest = szFileNameNorm;
	const char* pszSource = szFileName;
	while (*pszSource)
	{
		if (*pszSource == '\\')
			*pszDest++ = '/';
		else
			*pszDest++ = *pszSource;
		pszSource++;
	}
	*pszDest = 0;

	// cmp file names
	if (stricmp(szFileNameNorm, m_szFileName) != 0)
		return false;

	return true;
}

void CStatObj::GetMemoryUsage(ICrySizer* pSizer) const
{
	{
		SIZER_COMPONENT_NAME(pSizer, "Self");
		pSizer->AddObject(this, sizeof(*this));
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "subObjects");
		pSizer->AddObject(m_subObjects);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "Strings");
		pSizer->AddObject(m_szFileName);
		pSizer->AddObject(m_szGeomName);
		pSizer->AddObject(m_szProperties);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "Material");
		pSizer->AddObject(m_pMaterial);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "PhysGeomInfo");
		pSizer->AddObject(m_arrPhysGeomInfo);
	}

	if (m_pLODs)
		for (int i = 1; i < MAX_STATOBJ_LODS_NUM; i++)
		{
			SIZER_COMPONENT_NAME(pSizer, "StatObjLods");
			pSizer->AddObject(m_pLODs[i]);
		}

	if (m_pIndexedMesh)
	{
		SIZER_COMPONENT_NAME(pSizer, "Mesh");
		pSizer->AddObject(m_pIndexedMesh);
	}

	int nVtx = 0;
	if (m_pIndexedMesh)
		nVtx = m_pIndexedMesh->GetVertexCount();
	else if (m_pRenderMesh)
		nVtx = m_pRenderMesh->GetVerticesCount();

	if (m_pSpines)
	{
		SIZER_COMPONENT_NAME(pSizer, "StatObj Foliage Data");
		pSizer->AddObject(m_pSpines, sizeof(m_pSpines[0]), m_nSpines);
		if (m_pBoneMapping)
			pSizer->AddObject(m_pBoneMapping, sizeof(m_pBoneMapping[0]), nVtx);
		for (int i = 0; i < m_nSpines; i++)
			pSizer->AddObject(m_pSpines[i].pVtx, sizeof(Vec3) * 2 + sizeof(Vec4), m_pSpines[i].nVtx);
	}

	if (m_hasClothTangentsData && (!m_pClonedSourceObject || m_pClonedSourceObject == this))
	{
		SIZER_COMPONENT_NAME(pSizer, "Deformable StatObj ClothTangents");
		pSizer->AddObject(m_pClothTangentsData, nVtx * sizeof(m_pClothTangentsData[0]));
	}

	if (m_hasSkinInfo && (!m_pClonedSourceObject || m_pClonedSourceObject == this))
	{
		SIZER_COMPONENT_NAME(pSizer, "Deformable StatObj SkinData");
		pSizer->AddObject(m_pSkinInfo, (nVtx + 1) * sizeof(m_pSkinInfo[0]));
	}

	if (m_pMapFaceToFace0)
	{
		SIZER_COMPONENT_NAME(pSizer, "Deformable StatObj Mesh");
		pSizer->AddObject(m_pMapFaceToFace0, sizeof(m_pMapFaceToFace0[0]) * max(m_nLoadedTrisCount, m_nRenderTrisCount));
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::SetLodObject(int nLod, IStatObj* pObj)
{
	assert(nLod > 0 && nLod < MAX_STATOBJ_LODS_NUM);
	if (nLod <= 0 || nLod >= MAX_STATOBJ_LODS_NUM)
		return;

	CStatObj* pLod = (CStatObj*)pObj;
	if (strstr(m_szProperties, "lowspeclod0"))
		m_bLowSpecLod0Set = true;

	bool bLodCompound = pLod && (pLod->GetFlags() & STATIC_OBJECT_COMPOUND) != 0;

	if (pLod && !bLodCompound)
	{
		// Check if low lod decrease amount of used materials.
		int nPrevLodMatIds = m_nRenderMatIds;
		int nPrevLodTris = m_nLoadedTrisCount;
		if (nLod > 1 && m_pLODs && m_pLODs[nLod - 1])
		{
			nPrevLodMatIds = m_pLODs[nLod - 1]->m_nRenderMatIds;
			nPrevLodTris = m_pLODs[nLod - 1]->m_nLoadedTrisCount;
		}

		if (GetCVars()->e_LodsForceUse)
		{
			if ((int)m_nMaxUsableLod < nLod)
				m_nMaxUsableLod = nLod;
		}
		else
		{
			int min_tris = GetCVars()->e_LodMinTtris;
			if (((pLod->m_nLoadedTrisCount >= min_tris || nPrevLodTris >= (3 * min_tris) / 2)
			     || pLod->m_nRenderMatIds < nPrevLodMatIds) && nLod > (int)m_nMaxUsableLod)
			{
				m_nMaxUsableLod = nLod;
			}
		}

		if (m_pParentObject && (m_pParentObject->m_nMaxUsableLod < m_nMaxUsableLod))
			m_pParentObject->m_nMaxUsableLod = m_nMaxUsableLod;

		pLod->m_pLod0 = this;
		pLod->m_pMaterial = m_pMaterial; // Lod must use same material as parent.

		if (pLod->m_nLoadedTrisCount > MAX_TRIS_IN_LOD_0)
		{
			if ((strstr(pLod->GetProperties(), "lowspeclod0") != 0) && !m_bLowSpecLod0Set)
			{
				m_bLowSpecLod0Set = true;
				m_nMaxUsableLod0 = nLod;
			}
			if (!m_bLowSpecLod0Set)
				m_nMaxUsableLod0 = nLod;
		}
		if (nLod + 1 > (int)m_nLoadedLodsNum)
			m_nLoadedLodsNum = nLod + 1;

		// When assigning lod to child object.
		if (m_pParentObject)
		{
			if (nLod + 1 > (int)m_pParentObject->m_nLoadedLodsNum)
				m_pParentObject->m_nLoadedLodsNum = nLod + 1;
		}
	}

	if (pLod && bLodCompound)
	{
		m_nMaxUsableLod = nLod;
		pLod->m_bUnmergable = m_bUnmergable;
	}

	if (!m_pLODs && pLod)
		m_pLODs = new _smart_ptr<CStatObj>[MAX_STATOBJ_LODS_NUM];

	if (m_pLODs)
		m_pLODs[nLod] = pLod;
}

//////////////////////////////////////////////////////////////////////////
IStatObj* CStatObj::GetLodObject(int nLodLevel, bool bReturnNearest)
{
	if (nLodLevel < 1)
	{
		return this;
	}

	if (!m_pLODs)
	{
		if (bReturnNearest || nLodLevel == 0)
			return this;
		else
			return NULL;
	}

	if (bReturnNearest)
		nLodLevel = CLAMP(nLodLevel, 0, MAX_STATOBJ_LODS_NUM - 1);

	CStatObj* pLod = 0;
	if (nLodLevel < MAX_STATOBJ_LODS_NUM)
	{
		pLod = m_pLODs[nLodLevel];

		// Look up
		if (bReturnNearest && !pLod)
		{
			// Find highest existing lod looking up.
			int lod = nLodLevel;
			while (lod > 0 && m_pLODs[lod] == 0)
				lod--;
			if (lod > 0)
			{
				pLod = m_pLODs[lod];
			}
			else
			{
				pLod = this;
			}
		}
		// Look down
		if (bReturnNearest && !pLod)
		{
			// Find highest existing lod looking down.
			for (int lod = nLodLevel + 1; lod < MAX_STATOBJ_LODS_NUM; lod++)
			{
				if (m_pLODs[lod])
				{
					pLod = m_pLODs[lod];
					break;
				}
			}
		}
	}

	return pLod;
}

bool CStatObj::IsPhysicsExist() const
{
	return m_arrPhysGeomInfo.GetGeomCount() > 0;
}

bool CStatObj::IsSphereOverlap(const Sphere& sSphere)
{
	if (m_pRenderMesh && Overlap::Sphere_AABB(sSphere, m_AABB))
	{
		// if inside bbox
		int nPosStride = 0;
		int nInds = m_pRenderMesh->GetIndicesCount();
		const byte* pPos = m_pRenderMesh->GetPosPtr(nPosStride, FSL_READ);
		vtx_idx* pInds = m_pRenderMesh->GetIndexPtr(FSL_READ);

		if (pInds && pPos)
			for (int i = 0; (i + 2) < nInds; i += 3)
			{
				// test all triangles of water surface strip
				Vec3 v0 = (*(Vec3*)&pPos[nPosStride * pInds[i + 0]]);
				Vec3 v1 = (*(Vec3*)&pPos[nPosStride * pInds[i + 1]]);
				Vec3 v2 = (*(Vec3*)&pPos[nPosStride * pInds[i + 2]]);
				Vec3 vBoxMin = v0;
				vBoxMin.CheckMin(v1);
				vBoxMin.CheckMin(v2);
				Vec3 vBoxMax = v0;
				vBoxMax.CheckMax(v1);
				vBoxMax.CheckMax(v2);

				if (Overlap::Sphere_AABB(sSphere, AABB(vBoxMin, vBoxMax)))
					return true;
			}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::Invalidate(bool bPhysics, float tolerance)
{
	if (m_pIndexedMesh)
	{
		m_pIndexedMesh->CalcBBox();

		m_AABB = m_pIndexedMesh->m_bbox;

		MakeRenderMesh();
		m_nLoadedVertexCount = m_pIndexedMesh->GetVertexCount();
		m_nLoadedTrisCount = m_pIndexedMesh->GetFaceCount();
		if (!m_nLoadedTrisCount)
		{
			m_nLoadedTrisCount = m_pIndexedMesh->GetIndexCount() / 3;
		}
		CalcRadiuses();

		if (bPhysics)
		{
			PhysicalizeGeomType(PHYS_GEOM_TYPE_DEFAULT, *m_pIndexedMesh->GetMesh(), tolerance, 0);
		}

		ReleaseIndexedMesh(true);
	}

	//////////////////////////////////////////////////////////////////////////
	// Iterate through sub objects and update hide mask and sub obj mesh count.
	//////////////////////////////////////////////////////////////////////////
	m_nSubObjectMeshCount = 0;
	for (size_t i = 0, numsub = m_subObjects.size(); i < numsub; i++)
	{
		SSubObject& subObj = m_subObjects[i];
		if (subObj.pStatObj && subObj.nType == STATIC_SUB_OBJECT_MESH)
		{
			m_nSubObjectMeshCount++;
		}
	}

	UnMergeSubObjectsRenderMeshes();
}

//////////////////////////////////////////////////////////////////////////
IStatObj* CStatObj::Clone(bool bCloneGeometry, bool bCloneChildren, bool bMeshesOnly)
{
	if (m_bDefaultObject)
		return this;

	CStatObj* pNewObj = new CStatObj;

	pNewObj->m_pClonedSourceObject = m_pClonedSourceObject ? m_pClonedSourceObject : this;
	pNewObj->m_pClonedSourceObject->AddRef(); // Cloned object will keep a reference to this.

	pNewObj->m_nLoadedTrisCount = m_nLoadedTrisCount;
	pNewObj->m_nLoadedVertexCount = m_nLoadedVertexCount;
	pNewObj->m_nRenderTrisCount = m_nRenderTrisCount;

	if (bCloneGeometry)
	{
		if (m_bMerged)
		{
			UnMergeSubObjectsRenderMeshes();
		}
		if (m_pIndexedMesh && !m_bMerged)
		{
			pNewObj->m_pIndexedMesh = new CIndexedMesh;
			pNewObj->m_pIndexedMesh->CopyFrom(*m_pIndexedMesh->GetMesh());
		}
		if (m_pRenderMesh && !m_bMerged)
		{
			_smart_ptr<IRenderMesh> pRenderMesh = GetRenderer()->CreateRenderMesh(
			  "StatObj_Cloned",
			  pNewObj->GetFilePath(),
			  NULL,
			  ((GetFlags() & STATIC_OBJECT_DYNAMIC) ? eRMT_Dynamic : eRMT_Static));

			m_pRenderMesh->CopyTo(pRenderMesh);
			pNewObj->SetRenderMesh(pRenderMesh);
		}
	}
	else
	{
		if (m_pRenderMesh)
			if (m_pMergedRenderMesh != m_pRenderMesh)
				pNewObj->SetRenderMesh(m_pRenderMesh);
			else
				pNewObj->m_pRenderMesh = m_pRenderMesh;
		pNewObj->m_pMergedRenderMesh = m_pMergedRenderMesh;
		pNewObj->m_bMerged = m_pMergedRenderMesh != NULL;
	}

	pNewObj->m_szFileName = m_szFileName;
	pNewObj->m_szGeomName = m_szGeomName;

	// Default material.
	pNewObj->m_pMaterial = m_pMaterial;

	for (int i = 0; i < m_arrPhysGeomInfo.GetGeomCount(); i++)
	{
		pNewObj->m_arrPhysGeomInfo.SetPhysGeom(m_arrPhysGeomInfo[i], i, m_arrPhysGeomInfo.GetGeomType(i));
		if (pNewObj->m_arrPhysGeomInfo[i])
			GetPhysicalWorld()->GetGeomManager()->AddRefGeometry(pNewObj->m_arrPhysGeomInfo[i]);
	}
	pNewObj->m_phys_mass = m_phys_mass;
	pNewObj->m_phys_density = m_phys_density;
	pNewObj->m_AABB = m_AABB;
	pNewObj->m_vVegCenter = m_vVegCenter;

	pNewObj->m_fRadiusHors = m_fRadiusHors;
	pNewObj->m_fRadiusVert = m_fRadiusVert;

	pNewObj->m_nFlags = m_nFlags | STATIC_OBJECT_CLONE;

	pNewObj->m_fGeometricMeanFaceArea = m_fGeometricMeanFaceArea;
	pNewObj->m_fLodDistance = m_fLodDistance;
	pNewObj->m_depthSortOffset = m_depthSortOffset;

	//////////////////////////////////////////////////////////////////////////
	// Internal Flags.
	//////////////////////////////////////////////////////////////////////////
	pNewObj->m_bCanUnload = false;
	pNewObj->m_bDefaultObject = m_bDefaultObject;
	pNewObj->m_bOpenEdgesTested = m_bOpenEdgesTested;
	pNewObj->m_bSubObject = false; // [anton] since parent is not copied anyway
	pNewObj->m_bVehicleOnlyPhysics = m_bVehicleOnlyPhysics;
	pNewObj->m_idmatBreakable = m_idmatBreakable;
	pNewObj->m_bBreakableByGame = m_bBreakableByGame;
	pNewObj->m_bHasDeformationMorphs = m_bHasDeformationMorphs;
	pNewObj->m_bTmpIndexedMesh = m_bTmpIndexedMesh;
	pNewObj->m_bHaveOcclusionProxy = m_bHaveOcclusionProxy;
	pNewObj->m_bHasStreamOnlyCGF = m_bHasStreamOnlyCGF;
	pNewObj->m_eStreamingStatus = m_eStreamingStatus;
	//////////////////////////////////////////////////////////////////////////

	int numSubObj = (int)m_subObjects.size();
	if (bMeshesOnly)
	{
		numSubObj = 0;
		for (size_t i = 0; i < m_subObjects.size(); i++)
		{
			if (m_subObjects[i].nType == STATIC_SUB_OBJECT_MESH)
				numSubObj++;
			else
				break;
		}
	}
	pNewObj->m_subObjects.reserve(numSubObj);
	for (int i = 0; i < numSubObj; i++)
	{
		pNewObj->m_subObjects.push_back(m_subObjects[i]);
		pNewObj->m_subObjects[i].pFoliage = 0;
		if (m_subObjects[i].pStatObj)
		{
			if (bCloneChildren)
			{
				pNewObj->m_subObjects[i].pStatObj = m_subObjects[i].pStatObj->Clone(bCloneGeometry, bCloneChildren, bMeshesOnly);
				pNewObj->m_subObjects[i].pStatObj->AddRef();
				((CStatObj*)(pNewObj->m_subObjects[i].pStatObj))->m_pParentObject = this;
			}
			else
			{
				m_subObjects[i].pStatObj->AddRef();
				((CStatObj*)(m_subObjects[i].pStatObj))->m_nFlags |= STATIC_OBJECT_MULTIPLE_PARENTS;
			}
		}
	}
	pNewObj->m_nSubObjectMeshCount = m_nSubObjectMeshCount;
	if (!bCloneChildren)
		pNewObj->m_bSharesChildren = true;

	if (pNewObj->m_hasClothTangentsData = m_hasClothTangentsData)
		pNewObj->m_pClothTangentsData = m_pClothTangentsData;
	if (pNewObj->m_hasSkinInfo = m_hasSkinInfo)
		pNewObj->m_pSkinInfo = m_pSkinInfo;

	return pNewObj;
}
//////////////////////////////////////////////////////////////////////////

IIndexedMesh* CStatObj::GetIndexedMesh(bool bCreateIfNone)
{
	WriteLock lock(m_lockIdxMesh);
	if (m_pIndexedMesh)
		return m_pIndexedMesh;
	else if (m_pRenderMesh && bCreateIfNone)
	{

		if (m_pRenderMesh->GetIndexedMesh(m_pIndexedMesh = new CIndexedMesh) == NULL)
		{
			// GetIndexMesh will free the IndexedMesh object if an allocation failed
			m_pIndexedMesh = NULL;
			return NULL;
		}

		CMesh* pMesh = m_pIndexedMesh->GetMesh();
		if (!pMesh || pMesh->m_subsets.size() <= 0)
		{
			m_pIndexedMesh->Release();
			m_pIndexedMesh = 0;
			return 0;
		}
		m_bTmpIndexedMesh = true;

		int i, j, i0 = pMesh->m_subsets[0].nFirstVertId + pMesh->m_subsets[0].nNumVerts;
		for (i = j = 1; i < pMesh->m_subsets.size(); i++)
			if (pMesh->m_subsets[i].nFirstVertId - i0 < pMesh->m_subsets[j].nFirstVertId - i0)
				j = i;
		if (j < pMesh->m_subsets.size() && pMesh->m_subsets[0].nPhysicalizeType == PHYS_GEOM_TYPE_DEFAULT &&
		    pMesh->m_subsets[j].nPhysicalizeType != PHYS_GEOM_TYPE_DEFAULT && pMesh->m_subsets[j].nFirstVertId > i0)
		{
			pMesh->m_subsets[j].nNumVerts += pMesh->m_subsets[j].nFirstVertId - i0;
			pMesh->m_subsets[j].nFirstVertId = i0;
		}
		return m_pIndexedMesh;
	}
	return 0;
}

IIndexedMesh* CStatObj::CreateIndexedMesh()
{
	if (!m_pIndexedMesh)
		m_pIndexedMesh = new CIndexedMesh();

	return m_pIndexedMesh;
}

void CStatObj::ReleaseIndexedMesh(bool bRenderMeshUpdated)
{
	WriteLock lock(m_lockIdxMesh);
	if (m_bTmpIndexedMesh && m_pIndexedMesh)
	{
		int istart, iend;
		CMesh* pMesh = m_pIndexedMesh->GetMesh();
		if (m_pRenderMesh && !bRenderMeshUpdated)
		{
			TRenderChunkArray& Chunks = m_pRenderMesh->GetChunks();
			for (int i = 0; i < pMesh->m_subsets.size(); i++)
				Chunks[i].m_nMatFlags |= pMesh->m_subsets[i].nMatFlags & 1 << 30;
		}
		if (bRenderMeshUpdated && m_pBoneMapping)
		{
			for (int i = iend = 0; i < (int)pMesh->m_subsets.size(); i++)
				if ((pMesh->m_subsets[i].nMatFlags & (MTL_FLAG_NOPHYSICALIZE | MTL_FLAG_NODRAW)) == MTL_FLAG_NOPHYSICALIZE)
				{
					for (istart = iend++; iend < (int)m_chunkBoneIds.size() && m_chunkBoneIds[iend]; iend++)
						;
					if (!pMesh->m_subsets[i].nNumIndices)
					{
						m_chunkBoneIds.erase(m_chunkBoneIds.begin() + istart, m_chunkBoneIds.begin() + iend);
						iend = istart;
					}
				}
		}
		m_pIndexedMesh->Release();
		m_pIndexedMesh = 0;
		m_bTmpIndexedMesh = false;
	}
}

//////////////////////////////////////////////////////////////////////////
static bool SubObjectsOfCompoundHaveLOD(const CStatObj* pStatObj, int nLod)
{
	int numSubObjects = pStatObj->GetSubObjectCount();
	for (int i = 0; i < numSubObjects; ++i)
	{
		const IStatObj::SSubObject* pSubObject = const_cast<CStatObj*>(pStatObj)->GetSubObject(i);
		if (!pSubObject)
			continue;
		const CStatObj* pChildObj = (const CStatObj*)pSubObject->pStatObj;
		if (!pChildObj)
			continue;
		if (!pChildObj->m_pLODs)
			continue;
		const CStatObj* pLod = pChildObj->m_pLODs[nLod];
		if (pLod)
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::TryMergeSubObjects(bool bFromStreaming)
{
	// sub meshes merging
	if (GetCVars()->e_StatObjMerge)
	{
		if (!m_bUnmergable && !IsDeformable())
		{
			MergeSubObjectsRenderMeshes(bFromStreaming, this, 0);

			if (!bFromStreaming && !m_pLODs && m_nFlags & STATIC_OBJECT_COMPOUND)
			{
				// Check if LODs were not split (production mode)
				for (int i = 1; i < MAX_STATOBJ_LODS_NUM; ++i)
				{
					if (!SubObjectsOfCompoundHaveLOD(this, i))
						continue;

					CStatObj* pStatObj = new CStatObj();
					pStatObj->m_szFileName = m_szFileName;
					char lodName[32];
					cry_strcpy(lodName, "-mlod");
					ltoa(i, lodName + 5, 10);
					pStatObj->m_szFileName.append(lodName);
					pStatObj->m_szGeomName = m_szGeomName;
					pStatObj->m_bSubObject = true;

					SetLodObject(i, pStatObj);
					m_bMergedLODs = true;
				}
			}

			if (m_pLODs)
			{
				for (int i = 1; i < MAX_STATOBJ_LODS_NUM; i++)
				{
					if (m_pLODs[i])
					{
						m_pLODs[i]->MergeSubObjectsRenderMeshes(bFromStreaming, this, i);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::MergeSubObjectsRenderMeshes(bool bFromStreaming, CStatObj* pLod0, int nLod)
{
	if (m_bUnmergable)
		return;

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Merged StatObj");
	FUNCTION_PROFILER_3DENGINE;

	m_bMerged = false;
	m_pMergedRenderMesh = 0;

	int nSubObjCount = (int)pLod0->m_subObjects.size();

	std::vector<SRenderMeshInfoInput> lstRMI;

	SRenderMeshInfoInput rmi;

	rmi.pMat = m_pMaterial;
	rmi.mat.SetIdentity();
	rmi.pMesh = 0;
	rmi.pSrcRndNode = 0;

	for (int s = 0; s < nSubObjCount; s++)
	{
		CStatObj::SSubObject* pSubObj = &pLod0->m_subObjects[s];
		if (pSubObj->pStatObj && pSubObj->nType == STATIC_SUB_OBJECT_MESH)
		{
			CStatObj* pStatObj = (CStatObj*)pSubObj->pStatObj->GetLodObject(nLod, true); // Get lod, if not exist get lowest existing.
			if (pStatObj)
			{
				CryAutoCriticalSection lock(pStatObj->m_streamingMeshLock);
				if (pStatObj->m_pRenderMesh || pStatObj->m_pStreamedRenderMesh)
				{
					rmi.pMesh = pStatObj->m_pRenderMesh ? pStatObj->m_pRenderMesh : pStatObj->m_pStreamedRenderMesh;
					rmi.mat = pSubObj->tm;
					rmi.bIdentityMatrix = pSubObj->bIdentityMatrix;
					rmi.nSubObjectIndex = s;
					lstRMI.push_back(rmi);
				}
			}
		}
	}

	_smart_ptr<IRenderMesh> pMergedMesh = 0;
	if (lstRMI.size() == 1 && lstRMI[0].bIdentityMatrix)
	{
		// If identity matrix and only mesh-subobject use it as a merged render mesh.
		pMergedMesh = rmi.pMesh;
	}
	else if (!lstRMI.empty())
	{
		SMergeInfo info;
		info.sMeshName = GetFilePath();
		info.sMeshType = "StatObj_Merged";
		info.bMergeToOneRenderMesh = true;
		info.pUseMaterial = m_pMaterial;
		pMergedMesh = GetSharedRenderMeshMerger()->MergeRenderMeshes(&lstRMI[0], lstRMI.size(), info);
	}

	if (pMergedMesh)
	{
		if (bFromStreaming)
		{
			CryAutoCriticalSection lock(m_streamingMeshLock);
			m_pMergedRenderMesh = pMergedMesh;
			m_pStreamedRenderMesh = pMergedMesh;
		}
		else
		{
			m_pMergedRenderMesh = pMergedMesh;
			SetRenderMesh(pMergedMesh);
		}

		m_bMerged = true;
		if (m_pLod0)
		{
			// Make sure upmost lod is also marked to be merged.
			m_pLod0->m_bMerged = true;
		}
	}
}

bool CStatObj::IsMatIDReferencedByObj(uint16 matID)
{
	//Check root obj
	if (IRenderMesh* pRenderMesh = GetRenderMesh())
	{
		TRenderChunkArray& Chunks = pRenderMesh->GetChunks();

		for (int nChunkId = 0; nChunkId < Chunks.size(); nChunkId++)
		{
			if (Chunks[nChunkId].m_nMatID == matID)
			{
				return true;
			}
		}
	}

	//check children
	int nSubObjCount = (int)m_subObjects.size();
	for (int s = 0; s < nSubObjCount; s++)
	{
		CStatObj::SSubObject* pSubObj = &m_subObjects[s];

		if (pSubObj->pStatObj)
		{
			CStatObj* pSubStatObj = (CStatObj*)pSubObj->pStatObj;

			if (IRenderMesh* pSubRenderMesh = pSubStatObj->GetRenderMesh())
			{
				TRenderChunkArray& Chunks = pSubRenderMesh->GetChunks();

				for (int nChunkId = 0; nChunkId < Chunks.size(); nChunkId++)
				{
					if (Chunks[nChunkId].m_nMatID == matID)
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CStatObj::CanMergeSubObjects()
{
	if (gEnv->IsDedicated())
		return false;

	int nSubMeshes = 0;
	int nTotalVertexCount = 0;
	int nTotalTriCount = 0;
	int nTotalRenderChunksCount = 0;

	int nSubObjCount = (int)m_subObjects.size();
	for (int s = 0; s < nSubObjCount; s++)
	{
		CStatObj::SSubObject* pSubObj = &m_subObjects[s];
		if (pSubObj->pStatObj && pSubObj->nType == STATIC_SUB_OBJECT_MESH && !pSubObj->bHidden)
		{
			CStatObj* pStatObj = (CStatObj*)pSubObj->pStatObj;
			if (pStatObj->m_pMaterial != m_pMaterial || pStatObj->m_nSpines) // All materials must be same, and no bendable foliage
			{
				return false;
			}
			nSubMeshes++;
			nTotalVertexCount += pStatObj->m_nLoadedVertexCount;
			nTotalTriCount += pStatObj->m_nLoadedTrisCount;
			nTotalRenderChunksCount += pStatObj->m_nRenderMatIds;
		}
	}

	// Check for mat_breakable surface type in material
	if (m_pMaterial)
	{
		int nSubMtls = m_pMaterial->GetSubMtlCount();
		if (nSubMtls > 0)
		{
			for (int i = 0; i < nSubMtls; ++i)
			{
				IMaterial* pSubMtl = m_pMaterial->GetSafeSubMtl(i);
				if (pSubMtl)
				{
					ISurfaceType* pSFType = pSubMtl->GetSurfaceType();

					// This is breakable glass.
					// Do not merge meshes that have procedural physics breakability in them.
					if (pSFType && pSFType->GetBreakability() != 0)
					{
						//if mesh is streamed, we must assume the material is referenced
						if (m_bMeshStrippedCGF)
						{
							return false;
						}
						else if (IsMatIDReferencedByObj((uint16)i))
						{
							return false;
						}
					}
				}
			}
		}
		else // nSubMtls==0
		{
			ISurfaceType* pSFType = m_pMaterial->GetSurfaceType();

			// This is breakable glass.
			// Do not merge meshes that have procedural physics breakability in them.
			if (pSFType && pSFType->GetBreakability() != 0)
			{
				return false;
			}
		}
	}

	if (nTotalVertexCount > MAX_VERTICES_MERGABLE || nSubMeshes <= 1 || nTotalRenderChunksCount <= 1)
	{
		return false;
	}
	if ((nTotalTriCount / nTotalRenderChunksCount) > GetCVars()->e_StatObjMergeMaxTrisPerDrawCall)
	{
		return false; // tris to draw calls ratio is already not so bad
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::UnMergeSubObjectsRenderMeshes()
{
	if (m_bMerged)
	{
		m_bMerged = false;
		m_pMergedRenderMesh = 0;
		SetRenderMesh(0);
	}
	if (m_bMergedLODs)
	{
		delete[] m_pLODs;
		m_pLODs = 0;
	}
}

//------------------------------------------------------------------------------
// This function is recovered from previous FindSubObject function which was then
// changed and creating many CGA model issues (some joints never move).
CStatObj::SSubObject* CStatObj::FindSubObject_CGA(const char* sNodeName)
{
	uint32 numSubObjects = m_subObjects.size();
	for (uint32 i = 0; i < numSubObjects; i++)
	{
		if (stricmp(m_subObjects[i].name.c_str(), sNodeName) == 0)
		{
			return &m_subObjects[i];
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CStatObj::SSubObject* CStatObj::FindSubObject(const char* sNodeName)
{
	uint32 numSubObjects = m_subObjects.size();
	int len = 1; // some objects has ' ' in the beginning
	for (; sNodeName[len] > ' ' && sNodeName[len] != ',' && sNodeName[len] != ';'; len++)
		;
	for (uint32 i = 0; i < numSubObjects; i++)
	{
		if (strnicmp(m_subObjects[i].name.c_str(), sNodeName, len) == 0 && m_subObjects[i].name[len] == 0)
		{
			return &m_subObjects[i];
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CStatObj::SSubObject* CStatObj::FindSubObject_StrStr(const char* sNodeName)
{
	uint32 numSubObjects = m_subObjects.size();
	for (uint32 i = 0; i < numSubObjects; i++)
	{
		if (stristr(m_subObjects[i].name.c_str(), sNodeName))
		{
			return &m_subObjects[i];
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
IStatObj::SSubObject& CStatObj::AddSubObject(IStatObj* pSubObj)
{
	assert(pSubObj);
	SetSubObjectCount(m_subObjects.size() + 1);
	InitializeSubObject(m_subObjects[m_subObjects.size() - 1]);
	m_subObjects[m_subObjects.size() - 1].pStatObj = pSubObj;
	pSubObj->AddRef();
	return m_subObjects[m_subObjects.size() - 1];
}

//////////////////////////////////////////////////////////////////////////
bool CStatObj::RemoveSubObject(int nIndex)
{
	if (nIndex >= 0 && nIndex < (int)m_subObjects.size())
	{
		SSubObject* pSubObj = &m_subObjects[nIndex];
		CStatObj* pChildObj = (CStatObj*)pSubObj->pStatObj;
		if (pChildObj)
		{
			if (!m_bSharesChildren)
				pChildObj->m_pParentObject = NULL;
			pChildObj->Release();
		}
		m_subObjects.erase(m_subObjects.begin() + nIndex);
		Invalidate(true);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::SetSubObjectCount(int nCount)
{
	// remove sub objects.
	while ((int)m_subObjects.size() > nCount)
	{
		RemoveSubObject(m_subObjects.size() - 1);
	}

	SSubObject subobj;
	InitializeSubObject(subobj);
	m_subObjects.resize(nCount, subobj);

	if (nCount > 0)
	{
		m_nFlags |= STATIC_OBJECT_COMPOUND;
	}
	else
	{
		m_nFlags &= ~STATIC_OBJECT_COMPOUND;
	}
	Invalidate(true);
	UnMergeSubObjectsRenderMeshes();
}

//////////////////////////////////////////////////////////////////////////
bool CStatObj::CopySubObject(int nToIndex, IStatObj* pFromObj, int nFromIndex)
{
	SSubObject* pSrcSubObj = pFromObj->GetSubObject(nFromIndex);
	if (!pSrcSubObj)
		return false;

	if (nToIndex >= (int)m_subObjects.size())
	{
		SetSubObjectCount(nToIndex + 1);
		if (pFromObj == this)
			pSrcSubObj = pFromObj->GetSubObject(nFromIndex);
	}

	m_subObjects[nToIndex] = *pSrcSubObj;
	if (pSrcSubObj->pStatObj)
		pSrcSubObj->pStatObj->AddRef();
	m_subObjects[nToIndex].pFoliage = 0;

	Invalidate(true);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CStatObj::GetPhysicalProperties(float& mass, float& density)
{
	mass = m_phys_mass;
	density = m_phys_density;
	if (mass < 0 && density < 0)
		return false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CStatObj::RayIntersection(SRayHitInfo& hitInfo, IMaterial* pCustomMtl)
{
	Vec3 vOut;

	bool bNonDirectionalTest = hitInfo.inRay.direction.IsZero();

	// First check if ray intersect objects bounding box.
	if (!bNonDirectionalTest && !Intersect::Ray_AABB(hitInfo.inRay, m_AABB, vOut))
		return false;

	if (bNonDirectionalTest && !Overlap::AABB_AABB(
	      AABB(hitInfo.inRay.origin - Vec3(hitInfo.fMaxHitDistance), hitInfo.inRay.origin + Vec3(hitInfo.fMaxHitDistance)),
	      m_AABB))
		return false;

	_smart_ptr<IRenderMesh> pRenderMesh = m_pRenderMesh;

	// Sometimes, object has no base lod mesh. So need to hit test with low level mesh.
	// If the distance from camera is larger then a base lod distance, then base lod mesh is not loaded yet.
	assert(m_nMaxUsableLod < MAX_STATOBJ_LODS_NUM && (m_nMaxUsableLod == 0 || (m_pLODs && m_pLODs[m_nMaxUsableLod])));
	if (!pRenderMesh && m_nMaxUsableLod > 0 && m_pLODs && m_pLODs[m_nMaxUsableLod])
		pRenderMesh = m_pLODs[m_nMaxUsableLod]->GetRenderMesh();

	if ((m_nFlags & STATIC_OBJECT_COMPOUND) && !m_bMerged)
	{
		SRayHitInfo hitOut;
		bool bAnyHit = false;
		float fMinDistance = FLT_MAX;
		for (int i = 0, num = m_subObjects.size(); i < num; i++)
		{
			if (m_subObjects[i].pStatObj && m_subObjects[i].nType == STATIC_SUB_OBJECT_MESH && !m_subObjects[i].bHidden)
			{
				if (((CStatObj*)(m_subObjects[i].pStatObj))->m_nFlags & STATIC_OBJECT_HIDDEN)
					continue;

				Matrix34 invertedTM = m_subObjects[i].tm.GetInverted();

				SRayHitInfo hit = hitInfo;

				// Transform ray into sub-object local space.
				hit.inReferencePoint = invertedTM.TransformPoint(hit.inReferencePoint);
				hit.inRay.origin = invertedTM.TransformPoint(hit.inRay.origin);
				hit.inRay.direction = invertedTM.TransformVector(hit.inRay.direction);

				int nFirstTriangleId = hit.pHitTris ? hit.pHitTris->Count() : 0;

				if (((CStatObj*)m_subObjects[i].pStatObj)->RayIntersection(hit, pCustomMtl))
				{
					if (hit.fDistance < fMinDistance)
					{
						hitInfo.pStatObj = m_subObjects[i].pStatObj;
						bAnyHit = true;
						hitOut = hit;
					}
				}

				// transform collected triangles from sub-object space into object space
				if (hit.pHitTris)
				{
					for (int t = nFirstTriangleId; t < hit.pHitTris->Count(); t++)
					{
						SRayHitTriangle& ht = hit.pHitTris->GetAt(t);
						for (int c = 0; c < 3; c++)
							ht.v[c] = m_subObjects[i].tm.TransformPoint(ht.v[c]);
					}
				}
			}
		}
		if (bAnyHit)
		{
			// Restore input ray/reference point.
			hitOut.inReferencePoint = hitInfo.inReferencePoint;
			hitOut.inRay = hitInfo.inRay;

			hitInfo = hitOut;
			return true;
		}
	}
	else
	{
		if (pRenderMesh)
		{
			bool bRes = CRenderMeshUtils::RayIntersection(pRenderMesh, hitInfo, pCustomMtl);
			if (bRes)
			{
				hitInfo.pStatObj = this;
				hitInfo.pRenderMesh = pRenderMesh;
			}
			return bRes;
		}
	}
	return false;
}

bool CStatObj::LineSegIntersection(const Lineseg& lineSeg, Vec3& hitPos, int& surfaceTypeId)
{
	bool intersects = false;
	if (m_pRenderMesh)
	{
		m_pRenderMesh->LockForThreadAccess();
		int numIndices = m_pRenderMesh->GetIndicesCount();
		int numVertices = m_pRenderMesh->GetVerticesCount();
		if (numIndices && numVertices)
		{
			int posStride;
			uint8* pPositions = (uint8*)m_pRenderMesh->GetPosPtr(posStride, FSL_READ);
			vtx_idx* pIndices = m_pRenderMesh->GetIndexPtr(FSL_READ);

			TRenderChunkArray& Chunks = m_pRenderMesh->GetChunks();
			int nChunkCount = Chunks.size();
			for (int nChunkId = 0; nChunkId < nChunkCount; nChunkId++)
			{
				CRenderChunk* pChunk = &Chunks[nChunkId];
				if (!(pChunk->m_nMatFlags & MTL_FLAG_NODRAW))
				{
					int lastIndex = pChunk->nFirstIndexId + pChunk->nNumIndices;
					for (int i = pChunk->nFirstIndexId; i < lastIndex; )
					{
						const Vec3& v0 = *(Vec3*)(pPositions + pIndices[i++] * posStride);
						const Vec3& v1 = *(Vec3*)(pPositions + pIndices[i++] * posStride);
						const Vec3& v2 = *(Vec3*)(pPositions + pIndices[i++] * posStride);

						if (Intersect::Lineseg_Triangle(lineSeg, v0, v2, v1, hitPos) || // Front face
						    Intersect::Lineseg_Triangle(lineSeg, v0, v1, v2, hitPos))   // Back face
						{
							IMaterial* pMtl = m_pMaterial->GetSafeSubMtl(pChunk->m_nMatID);
							surfaceTypeId = pMtl->GetSurfaceTypeId();
							intersects = true;
							break;
						}
					}
				}
			}
		}
		m_pRenderMesh->UnlockStream(VSF_GENERAL);
		m_pRenderMesh->UnlockIndexStream();
		m_pRenderMesh->UnLockForThreadAccess();
	}
	return intersects;
}

void CStatObj::SetRenderMesh(IRenderMesh* pRM)
{
	LOADING_TIME_PROFILE_SECTION;

	if (pRM == m_pRenderMesh)
		return;

	{
		CryAutoCriticalSection lock(m_streamingMeshLock);
		m_pRenderMesh = pRM;

		IncrementModificationId();
	}

	if (m_pRenderMesh)
	{
		m_nRenderTrisCount = 0;
		m_nRenderMatIds = 0;

		TRenderChunkArray& Chunks = m_pRenderMesh->GetChunks();
		for (int nChunkId = 0; nChunkId < Chunks.size(); nChunkId++)
		{
			CRenderChunk* pChunk = &Chunks[nChunkId];
			if (pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE)
				continue;

			if (pChunk->nNumIndices > 0)
			{
				m_nRenderMatIds++;
				m_nRenderTrisCount += pChunk->nNumIndices / 3;
			}
		}
		m_nLoadedTrisCount = pRM->GetIndicesCount() / 3;
		m_nLoadedVertexCount = pRM->GetVerticesCount();
	}

	// this will prepare all deformable object during loading instead when needed.
	// Thus is will prevent runtime stalls (300ms when shooting a taxi with 6000 vertices)
	// but will incrase memory since every deformable information needs to be loaded(500kb for the taxi)
	// So this is more a workaround, the real solution would precompute the data in the RC and load
	// the data only when needed into memory
	if (GetCVars()->e_PrepareDeformableObjectsAtLoadTime && m_pRenderMesh && m_pDelayedSkinParams)
	{
		PrepareSkinData(m_pDelayedSkinParams->mtxSkelToMesh, m_pDelayedSkinParams->pPhysSkel, m_pDelayedSkinParams->r);
	}
}

//////////////////////////////////////////////////////////////////////////
int CStatObj::CountChildReferences()
{
	// Check if it must be released.
	int nChildRefs = 0;
	int numChilds = (int)m_subObjects.size();
	for (int i = 0; i < numChilds; i++)
	{
		IStatObj::SSubObject& so = m_subObjects[i];
		CStatObj* pChildStatObj = (CStatObj*)so.pStatObj;
		if (!pChildStatObj) // All stat objects must be at the begining of the array//
			break;

		// if I'm the real parent of this child, check that no-one else is referencing it, otherwise it doesn't matter if I get deleted
		if (pChildStatObj->m_pParentObject == this)
		{
			bool bIgnoreSharedStatObj = false;
			for (int k = 0; k < i; k++)
			{
				if (pChildStatObj == m_subObjects[k].pStatObj)
				{
					// If we share pointer to this stat obj then do not count it again.
					bIgnoreSharedStatObj = true;
					nChildRefs -= 1; // 1 reference from current object should be ignored.
					break;
				}
			}
			if (!bIgnoreSharedStatObj)
			{
				nChildRefs += pChildStatObj->m_nUsers - 1; // 1 reference from parent should be ignored.
			}
		}
	}
	assert(nChildRefs >= 0);
	return nChildRefs;
}

IStatObj* CStatObj::GetLowestLod()
{
	if (int nLowestLod = CStatObj::GetMinUsableLod())
		return m_pLODs ? (CStatObj*)m_pLODs[CStatObj::GetMinUsableLod()] : (CStatObj*)NULL;
	return this;
}

//////////////////////////////////////////////////////////////////////////
int CStatObj::FindHighestLOD(int nBias)
{
	int nLowestLod = CStatObj::GetMinUsableLod();

	// if requested lod is not ready - find nearest ready one
	int nLod = CLAMP((int)m_nMaxUsableLod + nBias, nLowestLod, (int)m_nMaxUsableLod);

	while (nLod && nLod >= nLowestLod && (!m_pLODs || !m_pLODs[nLod] || !m_pLODs[nLod]->GetRenderMesh()))
		nLod--;

	if (nLod == 0 && !GetRenderMesh())
		nLod = -1;

	return nLod;
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::GetStatisticsNonRecursive(SStatistics& si)
{
	CStatObj* pStatObj = this;

	for (int lod = 0; lod < MAX_STATOBJ_LODS_NUM; lod++)
	{
		CStatObj* pLod = (CStatObj*)pStatObj->GetLodObject(lod);
		if (pLod)
		{
			//if (!pLod0 && pLod->GetRenderMesh())
			//pLod0 = pLod;

			if (lod > 0 && lod + 1 > si.nLods) // Assign last existing lod.
				si.nLods = lod + 1;

			si.nVerticesPerLod[lod] += pLod->m_nLoadedVertexCount;
			si.nIndicesPerLod[lod] += pLod->m_nLoadedTrisCount * 3;
			si.nMeshSize += pLod->m_nRenderMeshMemoryUsage;

			IRenderMesh* pRenderMesh = pLod->GetRenderMesh();
			if (pRenderMesh)
			{
				si.nMeshSizeLoaded += pRenderMesh->GetMemoryUsage(0, IRenderMesh::MEM_USAGE_ONLY_STREAMS);
				//if (si.pTextureSizer)
				//pRenderMesh->GetTextureMemoryUsage(0,si.pTextureSizer);
				//if (si.pTextureSizer2)
				//pRenderMesh->GetTextureMemoryUsage(0,si.pTextureSizer2);
			}
		}
	}

	si.nIndices += m_nLoadedTrisCount * 3;
	si.nVertices += m_nLoadedVertexCount;

	for (int j = 0; j < pStatObj->m_arrPhysGeomInfo.GetGeomCount(); j++)
	{
		if (pStatObj->GetPhysGeom(j))
		{
			ICrySizer* pPhysSizer = GetISystem()->CreateSizer();
			pStatObj->GetPhysGeom(j)->pGeom->GetMemoryStatistics(pPhysSizer);
			int sz = pPhysSizer->GetTotalSize();
			si.nPhysProxySize += sz;
			si.nPhysProxySizeMax = max(si.nPhysProxySizeMax, sz);
			si.nPhysPrimitives += pStatObj->GetPhysGeom(j)->pGeom->GetPrimitiveCount();
			pPhysSizer->Release();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::GetStatistics(SStatistics& si)
{
	si.bSplitLods = m_bLodsAreLoadedFromSeparateFile;

	bool bMultiSubObj = (GetFlags() & STATIC_OBJECT_COMPOUND) != 0;
	if (!bMultiSubObj)
	{
		GetStatisticsNonRecursive(si);
		si.nSubMeshCount = 0;
		si.nNumRefs = GetRefCount();
		si.nDrawCalls = m_nRenderMatIds;
	}
	else
	{
		si.nNumRefs = GetRefCount();

		std::vector<void*> addedObjects;
		for (int k = 0; k < GetSubObjectCount(); k++)
		{
			if (!GetSubObject(k))
				continue;
			CStatObj* pSubObj = (CStatObj*)GetSubObject(k)->pStatObj;

			int nSubObjectType = GetSubObject(k)->nType;
			if (nSubObjectType != STATIC_SUB_OBJECT_MESH && nSubObjectType != STATIC_SUB_OBJECT_HELPER_MESH)
				continue;

			if (stl::find(addedObjects, pSubObj))
				continue;
			addedObjects.push_back(pSubObj);

			if (pSubObj)
			{
				pSubObj->GetStatisticsNonRecursive(si);
				si.nSubMeshCount++;

				if (nSubObjectType == STATIC_SUB_OBJECT_MESH)
				{
					si.nDrawCalls += pSubObj->m_nRenderMatIds;
				}

				if (pSubObj->GetRefCount() > si.nNumRefs)
					si.nNumRefs = pSubObj->GetRefCount();
			}
		}
	}

	// Only do rough estimation based on the material
	// This is more consistent when streaming is enabled and render mesh may no exist
	if (m_pMaterial)
	{
		if (si.pTextureSizer)
			m_pMaterial->GetTextureMemoryUsage(si.pTextureSizer);
		if (si.pTextureSizer2)
			m_pMaterial->GetTextureMemoryUsage(si.pTextureSizer2);
	}
}
