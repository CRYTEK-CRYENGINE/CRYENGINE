// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "StatObj.h"
#include "ObjMan.h"
#include "MatMan.h"
#include "VisAreas.h"
#include "terrain_sector.h"
#include "CCullThread.h"
#include "3dEngine.h"
#include "IndexedMesh.h"
#include "Brush.h"
#include "Vegetation.h"
#include "terrain.h"
#include "RenderMeshMerger.h"
#include "MeshCompiler/MeshCompiler.h"
#include <CryThreading/IJobManager_JobDelegator.h>

CRenderMeshMerger::CRenderMeshMerger()
{
	m_nTotalVertexCount = 0;
	m_nTotalIndexCount = 0;
}

CRenderMeshMerger::~CRenderMeshMerger()
{
}

int CRenderMeshMerger::Cmp_Materials(IMaterial* pMat1, IMaterial* pMat2)
{
	if (!pMat1 || !pMat2)
		return 0;

	SShaderItem& shaderItem1 = pMat1->GetShaderItem();
	SShaderItem& shaderItem2 = pMat2->GetShaderItem();

#ifdef _DEBUG
	const char* pName1 = shaderItem1.m_pShader->GetName();
	const char* pName2 = shaderItem2.m_pShader->GetName();
#endif

	// vert format
	InputLayoutHandle nVertFormat1 = shaderItem1.m_pShader->GetVertexFormat();
	InputLayoutHandle nVertFormat2 = shaderItem2.m_pShader->GetVertexFormat();

	if (nVertFormat1 > nVertFormat2)
		return 1;
	if (nVertFormat1 < nVertFormat2)
		return -1;

	bool bDecal1 = (shaderItem1.m_pShader->GetFlags() & EF_DECAL) != 0;
	bool bDecal2 = (shaderItem2.m_pShader->GetFlags() & EF_DECAL) != 0;

	// shader
	if (bDecal1 > bDecal2)
		return 1;
	if (bDecal1 < bDecal2)
		return -1;

	// shader resources
	if (shaderItem1.m_pShaderResources > shaderItem2.m_pShaderResources)
		return 1;
	if (shaderItem1.m_pShaderResources < shaderItem2.m_pShaderResources)
		return -1;

	// shader
	if (shaderItem1.m_pShader > shaderItem2.m_pShader)
		return 1;
	if (shaderItem1.m_pShader < shaderItem2.m_pShader)
		return -1;

	// compare mats ptr
	if (pMat1 > pMat2)
		return 1;
	if (pMat1 < pMat2)
		return -1;

	return 0;
}

int CRenderMeshMerger::Cmp_RenderChunksInfo(const void* v1, const void* v2)
{
	SRenderMeshInfoInput* in1 = (SRenderMeshInfoInput*)v1;
	SRenderMeshInfoInput* in2 = (SRenderMeshInfoInput*)v2;

	//assert(pChunk1->LMInfo.iRAETex == pChunk2->LMInfo.iRAETex);

	IMaterial* pMat1 = in1->pMat;
	IMaterial* pMat2 = in2->pMat;

	return Cmp_Materials(pMat1, pMat2);
}

int CRenderMeshMerger::Cmp_RenderChunks_(const void* v1, const void* v2)
{
	SMergedChunk* pChunk1 = (SMergedChunk*)v1;
	SMergedChunk* pChunk2 = (SMergedChunk*)v2;

	if (pChunk1->rChunk.nSubObjectIndex < pChunk2->rChunk.nSubObjectIndex)
		return -1;
	if (pChunk1->rChunk.nSubObjectIndex > pChunk2->rChunk.nSubObjectIndex)
		return 1;

	IMaterial* pMat1 = pChunk1->pMaterial;
	IMaterial* pMat2 = pChunk2->pMaterial;

	return Cmp_Materials(pMat1, pMat2);
}

void CRenderMeshMerger::IsChunkValid(CRenderChunk& Ch, PodArray<SVF_P3S_C4B_T2S>& m_lstVerts, PodArray<uint32>& m_lstIndices)
{
#ifdef _DEBUG
	assert(Ch.nFirstIndexId + Ch.nNumIndices <= (uint32)m_lstIndices.Count());
	assert(Ch.nFirstVertId + Ch.nNumVerts <= (uint16)m_lstVerts.Count());

	for (uint32 i = Ch.nFirstIndexId; i < Ch.nFirstIndexId + Ch.nNumIndices; i++)
	{
		assert(m_lstIndices[i] >= Ch.nFirstVertId && m_lstIndices[i] < (uint32)(Ch.nFirstVertId + Ch.nNumVerts));
		assert(m_lstIndices[i] >= 0 && m_lstIndices[i] < (uint32)m_lstVerts.Count());
	}
#endif
}

void CRenderMeshMerger::MakeRenderMeshInfoListOfAllChunks(SRenderMeshInfoInput* pRMIArray, int nRMICount, SMergeInfo& info)
{
	for (int nRmi = 0; nRmi < nRMICount; nRmi++)
	{
		SRenderMeshInfoInput* pRMI = &pRMIArray[nRmi];
		IRenderMesh* pRM = pRMI->pMesh;

		const TRenderChunkArray& chunks = pRM->GetChunks();
		int nChunkCount = chunks.size();
		for (int nChunk = 0; nChunk < nChunkCount; nChunk++)
		{
			const CRenderChunk& renderChunk = chunks[nChunk];

			if (renderChunk.m_nMatFlags & MTL_FLAG_NODRAW || !renderChunk.pRE)
				continue;

			IMaterial* pCustMat;
			if (pRMI->pMat && renderChunk.m_nMatID < pRMI->pMat->GetSubMtlCount())
				pCustMat = pRMI->pMat->GetSubMtl(renderChunk.m_nMatID);
			else
				pCustMat = pRMI->pMat;

			if (!pCustMat)
				continue;
			if (!pCustMat->GetShaderItem().m_pShader)
				pCustMat = GetMatMan()->GetDefaultMaterial();

			IShader* pShader = pCustMat->GetShaderItem().m_pShader;

			if (!pShader)
				continue;
			if (pShader->GetFlags() & EF_NODRAW)
				continue;

			if (info.pDecalClipInfo)
				if (pShader->GetFlags() & EF_DECAL)
					continue;

			SRenderMeshInfoInput RMIChunk = *pRMI;
			RMIChunk.pMat = info.pDecalClipInfo ? NULL : pCustMat;
			RMIChunk.nChunkId = nChunk;

			m_lstRMIChunks.Add(RMIChunk);
		}
	}
}

void CRenderMeshMerger::MakeListOfAllCRenderChunks(SMergeInfo& info)
{
	FUNCTION_PROFILER_3DENGINE;

	m_nTotalVertexCount = 0;
	m_nTotalIndexCount = 0;

	for (int nEntityId = 0; nEntityId < m_lstRMIChunks.Count(); nEntityId++)
	{
		SRenderMeshInfoInput* pRMI = &m_lstRMIChunks[nEntityId];

		bool bMatrixHasRotation;
		if (!pRMI->mat.m01 && !pRMI->mat.m02 && !pRMI->mat.m10 && !pRMI->mat.m12 && !pRMI->mat.m20 && !pRMI->mat.m21)
			bMatrixHasRotation = false;
		else
			bMatrixHasRotation = true;

		Matrix34 matInv = pRMI->mat.GetInverted();

		IRenderMesh* pRM = pRMI->pMesh;
		if (!pRM->GetVerticesCount())
			continue;

		int nInitVertCout = m_lstVerts.Count();

		int nPosStride = 0;
		int nTexStride = 0;
		int nColorStride = 0;
		Vec3* pPos = 0;
		Vec2* pTex = 0;
		UCol* pColor = 0;

		// get vertices's
		{
			CRY_PROFILE_REGION(PROFILE_3DENGINE, "CRenderMeshMerger::MakeListOfAllCRenderChunks_GetPosPtr");

			pPos = reinterpret_cast<Vec3*>(pRM->GetPosPtr(nPosStride, FSL_READ));
			pTex = reinterpret_cast<Vec2*>(pRM->GetUVPtr(nTexStride, FSL_READ));
			pColor = reinterpret_cast<UCol*>(pRM->GetColorPtr(nColorStride, FSL_READ));
		}

		if (!pPos || !pTex || !pColor)
			continue;

		// get tangent basis
		int nNormStride = 0;
		int nTangsStride = 0;
		byte* pNorm = 0;
		byte* pTangs = 0;

		if (pRM->GetVertexFormat() != EDefaultInputLayouts::P3S_N4B_C4B_T2S)
		{
			pTangs = pRM->GetTangentPtr(nTangsStride, FSL_READ);
		}

#if ENABLE_NORMALSTREAM_SUPPORT
		// get normal stream
		{
			pNorm = pRM->GetNormPtr(nNormStride, FSL_READ);
		}
#endif

		Vec3 vMin, vMax;
		pRM->GetBBox(vMin, vMax);

		// get indices
		uint32 nIndCount = pRM->GetIndicesCount();
		vtx_idx* pSrcInds = pRM->GetIndexPtr(FSL_READ);

		Vec3 vOSPos(0, 0, 0);
		Vec3 vOSProjDir(0, 0, 0);
		float fMatrixScale = 1.f;
		float fOSRadius = 0.f;

		if (info.pDecalClipInfo && info.pDecalClipInfo->fRadius)
		{
			vOSPos = matInv.TransformPoint(info.pDecalClipInfo->vPos);
			vOSProjDir = matInv.TransformVector(info.pDecalClipInfo->vProjDir);
			fMatrixScale = vOSProjDir.GetLength();
			fOSRadius = info.pDecalClipInfo->fRadius * fMatrixScale;
			if (vOSProjDir.GetLength() > 0.01f)
				vOSProjDir.Normalize();
		}

		CRenderChunk newMatInfo = pRM->GetChunks()[pRMI->nChunkId];

#ifdef _DEBUG
		CRenderChunk Ch = newMatInfo;
		for (uint32 i = Ch.nFirstIndexId; i < Ch.nFirstIndexId + Ch.nNumIndices; i++)
		{
			assert(i >= 0 && i < nIndCount);
			assert(pSrcInds[i] >= Ch.nFirstVertId && pSrcInds[i] < Ch.nFirstVertId + Ch.nNumVerts);
			assert((int)pSrcInds[i] < pRM->GetVerticesCount());
		}
#endif

		int nFirstIndexId = m_lstIndices.Count();

		// add indices
		for (uint32 i = newMatInfo.nFirstIndexId; i < newMatInfo.nFirstIndexId + newMatInfo.nNumIndices; i += 3)
		{
			assert((int)pSrcInds[i + 0] < pRM->GetVerticesCount());
			assert((int)pSrcInds[i + 1] < pRM->GetVerticesCount());
			assert((int)pSrcInds[i + 2] < pRM->GetVerticesCount());

			assert((int)pSrcInds[i + 0] >= newMatInfo.nFirstVertId && pSrcInds[i + 0] < newMatInfo.nFirstVertId + newMatInfo.nNumVerts);
			assert((int)pSrcInds[i + 1] >= newMatInfo.nFirstVertId && pSrcInds[i + 1] < newMatInfo.nFirstVertId + newMatInfo.nNumVerts);
			assert((int)pSrcInds[i + 2] >= newMatInfo.nFirstVertId && pSrcInds[i + 2] < newMatInfo.nFirstVertId + newMatInfo.nNumVerts);

			// skip not needed triangles for decals
			if (fOSRadius)
			{
				// get verts
				Vec3 v0 = pPos[nPosStride * pSrcInds[i + 0]];
				Vec3 v1 = pPos[nPosStride * pSrcInds[i + 1]];
				Vec3 v2 = pPos[nPosStride * pSrcInds[i + 2]];

				if (vOSProjDir.IsZero())
				{
					// explosion mode
					// test the face
					float fDot0 = (vOSPos - v0).Dot((v1 - v0).Cross(v2 - v0));
					float fTest = -0.15f;
					if (fDot0 < fTest)
						continue;
				}
				else
				{
					// get triangle normal
					Vec3 vNormal = (v1 - v0).Cross(v2 - v0);

					// test the face
					if (vNormal.Dot(vOSProjDir) <= 0)
						continue;
				}

				// get bbox
				AABB triBox;
				triBox.min = v0;
				triBox.min.CheckMin(v1);
				triBox.min.CheckMin(v2);
				triBox.max = v0;
				triBox.max.CheckMax(v1);
				triBox.max.CheckMax(v2);

				if (!Overlap::Sphere_AABB(Sphere(vOSPos, fOSRadius), triBox))
					continue;
			}
			else if (info.pClipCellBox)
			{
				// get verts
				Vec3 v0 = pRMI->mat.TransformPoint(pPos[nPosStride * pSrcInds[i + 0]]);
				Vec3 v1 = pRMI->mat.TransformPoint(pPos[nPosStride * pSrcInds[i + 1]]);
				Vec3 v2 = pRMI->mat.TransformPoint(pPos[nPosStride * pSrcInds[i + 2]]);

				if (!Overlap::AABB_Triangle(*info.pClipCellBox, v0, v1, v2))
					continue;
			}

			m_lstIndices.Add((int)(pSrcInds[i + 0]) - newMatInfo.nFirstVertId + nInitVertCout);
			m_lstIndices.Add((int)(pSrcInds[i + 1]) - newMatInfo.nFirstVertId + nInitVertCout);
			m_lstIndices.Add((int)(pSrcInds[i + 2]) - newMatInfo.nFirstVertId + nInitVertCout);
		}

		newMatInfo.nFirstIndexId = nFirstIndexId;
		newMatInfo.nNumIndices = m_lstIndices.Count() - nFirstIndexId;

		if (!newMatInfo.nNumIndices)
			continue;

		// add vertices
		for (int v = newMatInfo.nFirstVertId; v < (int)newMatInfo.nFirstVertId + (int)newMatInfo.nNumVerts; v++)
		{
			assert(v >= 0 && v < pRM->GetVerticesCount());

			SVF_P3S_C4B_T2S vert;

			// set pos
			Vec3 vPos = pPos[nPosStride * v];
			vPos = pRMI->mat.TransformPoint(vPos);
			vert.xyz = vPos - info.vResultOffset;

			// set uv
			if (pTex)
			{
				vert.st = pTex[nTexStride * v];
			}
			else
			{
				vert.st = Vec2f16(0, 0);
			}

			vert.color = pColor[nColorStride * v + 0];

			m_lstVerts.Add(vert);

			// get tangent basis + normal
			SPipTangents basis = SPipTangents(Vec4sf(0, 0, 0, 0), Vec4sf(0, 0, 0, 0));
			SPipNormal normal = SPipNormal(Vec3(0, 0, 0));

			assert((pTangs) || (!pNorm));
			if (pTangs)
			{
				basis = *(SPipTangents*)&pTangs[nTangsStride * v];
#if ENABLE_NORMALSTREAM_SUPPORT
				if (pNorm)
				{
					normal = *(SPipNormal*)&pNorm[nNormStride * v];
				}
#endif

				if (bMatrixHasRotation)
				{
					basis.TransformSafelyBy(pRMI->mat);
#if ENABLE_NORMALSTREAM_SUPPORT
					if (pNorm)
						normal.TransformSafelyBy(pRMI->mat);
#endif
				}
			}

			m_lstTangBasises.Add(basis);
#if ENABLE_NORMALSTREAM_SUPPORT
			m_lstNormals.Add(normal);
#endif
		}

		// set vert range
		newMatInfo.nFirstVertId = m_lstVerts.Count() - newMatInfo.nNumVerts;
		newMatInfo.pRE = NULL;

		//		assert(IsHeapValid());
		//		IsChunkValid(newMatInfo, m_lstVerts, m_lstIndices);

		if (m_lstChunks.Count())
			assert(m_lstChunks.Last().rChunk.nFirstVertId + m_lstChunks.Last().rChunk.nNumVerts == newMatInfo.nFirstVertId);

		if (newMatInfo.nNumIndices)
		{
			SMergedChunk mrgChunk;
			mrgChunk.rChunk = newMatInfo;
			mrgChunk.rChunk.nSubObjectIndex = pRMI->nSubObjectIndex;
			mrgChunk.pMaterial = info.pDecalClipInfo ? NULL : pRMI->pMat.get();
			if (pRMI->pMat)
				mrgChunk.rChunk.m_nMatFlags = pRMI->pMat->GetFlags();
			m_lstChunks.Add(mrgChunk);
		}

		m_nTotalVertexCount += newMatInfo.nNumVerts;
		m_nTotalIndexCount += newMatInfo.nNumIndices;
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderMeshMerger::CompactVertices(SMergeInfo& info)
{
	if (info.bPrintDebugMessages)
		PrintMessage("Removing unused vertices");

	PodArray<uint32> lstVertUsage;
	lstVertUsage.PreAllocate(m_lstVerts.Count(), m_lstVerts.Count());
	for (int i = 0; i < m_lstIndices.Count(); i++)
		lstVertUsage[m_lstIndices[i]] = 1;

	PodArray<SVF_P3S_C4B_T2S> lstVertsOptimized;
	lstVertsOptimized.PreAllocate(m_lstVerts.Count());
	PodArray<SPipTangents> lstTangBasisesOptimized;
	lstTangBasisesOptimized.PreAllocate(m_lstVerts.Count());
#if ENABLE_NORMALSTREAM_SUPPORT
	PodArray<SPipNormal> lstNormalsOptimized;
	lstNormalsOptimized.PreAllocate(m_lstVerts.Count());
#endif

	int nCurChunkId = 0;
	CRenderChunk* pChBack = &m_lstChunks[0].rChunk;
	int nVertsRemoved = 0;
	PodArray<SMergedChunk> lstChunksBk;
	lstChunksBk.AddList(m_lstChunks);
	for (int i = 0; i < m_lstVerts.Count(); )
	{
		if (lstVertUsage[i])
		{
			lstVertsOptimized.Add(m_lstVerts[i]);
			lstTangBasisesOptimized.Add(m_lstTangBasises[i]);
#if ENABLE_NORMALSTREAM_SUPPORT
			lstNormalsOptimized.Add(m_lstNormals[i]);
#endif
		}
		else
			nVertsRemoved++;

		lstVertUsage[i] = lstVertsOptimized.Count() - 1;

		i++;

		if (i >= (int)lstChunksBk[nCurChunkId].rChunk.nFirstVertId + (int)lstChunksBk[nCurChunkId].rChunk.nNumVerts)
		{
			if (nVertsRemoved)
			{
				pChBack->nNumVerts -= nVertsRemoved;

				for (int nId = nCurChunkId + 1; nId < m_lstChunks.Count(); nId++)
				{
					CRenderChunk& ChBack = m_lstChunks[nId].rChunk;
					ChBack.nFirstVertId -= nVertsRemoved;
				}

				nVertsRemoved = 0;
			}

			nCurChunkId++;
			if (nCurChunkId < m_lstChunks.Count())
				pChBack = &m_lstChunks[nCurChunkId].rChunk;
			else
				break;
		}
	}

	for (int i = 0; i < m_lstIndices.Count(); i++)
		m_lstIndices[i] = lstVertUsage[m_lstIndices[i]];

	int nOldVertsNum = m_lstVerts.Count();

	m_lstVerts = lstVertsOptimized;
	m_lstTangBasises = lstTangBasisesOptimized;
#if ENABLE_NORMALSTREAM_SUPPORT
	m_lstNormals = lstNormalsOptimized;
#endif

	if (info.bPrintDebugMessages)
		PrintMessage("old->new = %d->%d vertices", nOldVertsNum, m_lstVerts.Count());

	int nBadTrisCount = 0;
	for (int i = 0; i < m_lstIndices.Count(); i += 3)
	{
		if (m_lstIndices[i + 0] == m_lstIndices[i + 1] ||
		    m_lstIndices[i + 1] == m_lstIndices[i + 2] ||
		    m_lstIndices[i + 2] == m_lstIndices[i + 0])
		{
			nBadTrisCount++;
		}
	}

	if (nBadTrisCount)
		PrintMessage("CRenderMeshMerger::CompactVertices: Warning: %d bad tris found", nBadTrisCount);
}

//////////////////////////////////////////////////////////////////////////
void CRenderMeshMerger::ClipByAABB(SMergeInfo& info)
{
	if (info.bPrintDebugMessages)
		PrintMessage("  Do clipping . . ." /*, m_lstChunks.Count()*/);

	{
		// minimize range
		uint32 nMin = ~0;
		uint32 nMax = 0;

		for (int i = 0; i < m_lstIndices.Count(); i++)
		{
			if (nMin > m_lstIndices[i]) nMin = m_lstIndices[i];
			if (nMax < m_lstIndices[i]) nMax = m_lstIndices[i];
		}

		for (int i = 0; i < m_lstIndices.Count(); i++)
			m_lstIndices[i] -= nMin;

		if (m_lstVerts.Count() - nMax - 1 > 0)
		{
			m_lstVerts.Delete(nMax + 1, m_lstVerts.Count() - (nMax + 1));
			m_lstTangBasises.Delete(nMax + 1, m_lstTangBasises.Count() - (nMax + 1));
#if ENABLE_NORMALSTREAM_SUPPORT
			m_lstNormals.Delete(nMax + 1, m_lstNormals.Count() - (nMax + 1));
#endif
		}

		m_lstVerts.Delete(0, nMin);
		m_lstTangBasises.Delete(0, nMin);
#if ENABLE_NORMALSTREAM_SUPPORT
		m_lstNormals.Delete(0, nMin);
#endif
	}

	// define clip planes
	Plane planes[6];
	float fClipRadius = info.pDecalClipInfo->fRadius * 1.3f;
	planes[0].SetPlane(Vec3(0, 0, 1), info.pDecalClipInfo->vPos + Vec3(0, 0, fClipRadius));
	planes[1].SetPlane(Vec3(0, 0, -1), info.pDecalClipInfo->vPos + Vec3(0, 0, -fClipRadius));
	planes[2].SetPlane(Vec3(0, 1, 0), info.pDecalClipInfo->vPos + Vec3(0, fClipRadius, 0));
	planes[3].SetPlane(Vec3(0, -1, 0), info.pDecalClipInfo->vPos + Vec3(0, -fClipRadius, 0));
	planes[4].SetPlane(Vec3(1, 0, 0), info.pDecalClipInfo->vPos + Vec3(fClipRadius, 0, 0));
	planes[5].SetPlane(Vec3(-1, 0, 0), info.pDecalClipInfo->vPos + Vec3(-fClipRadius, 0, 0));

	// clip triangles
	int nOrigCount = m_lstIndices.Count();
	for (int i = 0; i < nOrigCount; i += 3)
	{
		if (ClipTriangle(i, planes, 6))
		{
			i -= 3;
			nOrigCount -= 3;
		}
	}

	if (m_lstIndices.Count() < 3 || m_lstVerts.Count() < 3)
		return;

	assert(m_lstTangBasises.Count() == m_lstVerts.Count());
#if ENABLE_NORMALSTREAM_SUPPORT
	assert(m_lstNormals.Count() == m_lstVerts.Count());
#endif

	{
		// minimize range
		uint32 nMin = ~0;
		uint32 nMax = 0;

		for (int i = 0; i < m_lstIndices.Count(); i++)
		{
			if (nMin > m_lstIndices[i]) nMin = m_lstIndices[i];
			if (nMax < m_lstIndices[i]) nMax = m_lstIndices[i];
		}

		for (int i = 0; i < m_lstIndices.Count(); i++)
			m_lstIndices[i] -= nMin;

		if (m_lstVerts.Count() - nMax - 1 > 0)
		{
			m_lstVerts.Delete(nMax + 1, m_lstVerts.Count() - (nMax + 1));
			m_lstTangBasises.Delete(nMax + 1, m_lstTangBasises.Count() - (nMax + 1));
#if ENABLE_NORMALSTREAM_SUPPORT
			m_lstNormals.Delete(nMax + 1, m_lstNormals.Count() - (nMax + 1));
#endif
		}

		m_lstVerts.Delete(0, nMin);
		m_lstTangBasises.Delete(0, nMin);
#if ENABLE_NORMALSTREAM_SUPPORT
		m_lstNormals.Delete(0, nMin);
#endif
	}

#ifdef _DEBUG
	assert(m_lstTangBasises.Count() == m_lstVerts.Count());
	#if ENABLE_NORMALSTREAM_SUPPORT
	assert(m_lstNormals.Count() == m_lstVerts.Count());
	#endif
	for (int i = 0; i < m_lstIndices.Count(); i++)
		assert(m_lstIndices[i] >= 0 && m_lstIndices[i] < (uint32)m_lstVerts.Count());
#endif

	if (m_lstIndices.Count() < 3 || m_lstVerts.Count() < 3)
		return;

	m_lstChunks[0].rChunk.nNumIndices = m_lstIndices.Count();
	m_lstChunks[0].rChunk.nNumVerts = m_lstVerts.Count();
}

//////////////////////////////////////////////////////////////////////////
bool CRenderMeshMerger::ClipTriangle(int nStartIdxId, Plane* pPlanes, int nPlanesNum)
{
	const PodArray<Vec3>& clipped = m_tmpClipContext.Clip(
	  m_lstVerts[m_lstIndices[nStartIdxId + 0]].xyz.ToVec3(),
	  m_lstVerts[m_lstIndices[nStartIdxId + 1]].xyz.ToVec3(),
	  m_lstVerts[m_lstIndices[nStartIdxId + 2]].xyz.ToVec3(),
	  pPlanes, nPlanesNum);

	if (clipped.Count() < 3)
	{
		m_lstIndices.Delete(nStartIdxId, 3);
		return true; // entire triangle is clipped away
	}

	if (clipped.Count() == 3)
		if (clipped[0].IsEquivalent(m_lstVerts[m_lstIndices[nStartIdxId + 0]].xyz.ToVec3()))
			if (clipped[1].IsEquivalent(m_lstVerts[m_lstIndices[nStartIdxId + 1]].xyz.ToVec3()))
				if (clipped[2].IsEquivalent(m_lstVerts[m_lstIndices[nStartIdxId + 2]].xyz.ToVec3()))
					return false;
	// entire triangle is in

	// replace old triangle with several new triangles
	int nStartId = m_lstVerts.Count();
	int nStartIndex = m_lstIndices[nStartIdxId + 0];
	SVF_P3S_C4B_T2S fullVert = m_lstVerts[nStartIndex];
	SPipTangents fullTang = m_lstTangBasises[nStartIndex];
#if ENABLE_NORMALSTREAM_SUPPORT
	SPipNormal fullNorm = m_lstNormals[nStartIndex];
#endif

	for (int i = 0; i < clipped.Count(); i++)
	{
		fullVert.xyz = clipped[i];
		m_lstVerts.Add(fullVert);
		m_lstTangBasises.Add(fullTang);
#if ENABLE_NORMALSTREAM_SUPPORT
		m_lstNormals.Add(fullNorm);
#endif
	}

	// put first new triangle into position of original one
	m_lstIndices[nStartIdxId + 0] = nStartId + 0;
	m_lstIndices[nStartIdxId + 1] = nStartId + 1;
	m_lstIndices[nStartIdxId + 2] = nStartId + 2;

	// put others in the end
	for (int i = 1; i < clipped.Count() - 2; i++)
	{
		m_lstIndices.Add(nStartId + 0);
		m_lstIndices.Add(nStartId + i + 1);
		m_lstIndices.Add(nStartId + i + 2);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CRenderMeshMerger::ClipDecals(SMergeInfo& info)
{
	if (info.bPrintDebugMessages)
		PrintMessage("  Do clipping . . ." /*, m_lstChunks.Count()*/);

	{
		// minimize range
		uint32 nMin = ~0;
		uint32 nMax = 0;

		for (int i = 0; i < m_lstIndices.Count(); i++)
		{
			if (nMin > m_lstIndices[i]) nMin = m_lstIndices[i];
			if (nMax < m_lstIndices[i]) nMax = m_lstIndices[i];
		}

		for (int i = 0; i < m_lstIndices.Count(); i++)
			m_lstIndices[i] -= nMin;

		if (m_lstVerts.Count() - nMax - 1 > 0)
		{
			m_lstVerts.Delete(nMax + 1, m_lstVerts.Count() - (nMax + 1));
			m_lstTangBasises.Delete(nMax + 1, m_lstTangBasises.Count() - (nMax + 1));
#if ENABLE_NORMALSTREAM_SUPPORT
			m_lstNormals.Delete(nMax + 1, m_lstNormals.Count() - (nMax + 1));
#endif
		}

		m_lstVerts.Delete(0, nMin);
		m_lstTangBasises.Delete(0, nMin);
#if ENABLE_NORMALSTREAM_SUPPORT
		m_lstNormals.Delete(0, nMin);
#endif
	}

	// define clip planes
	Plane planes[6];
	float fClipRadius = info.pDecalClipInfo->fRadius * 1.3f;
	planes[0].SetPlane(Vec3(0, 0, 1), info.pDecalClipInfo->vPos - info.vResultOffset + Vec3(0, 0, fClipRadius));
	planes[1].SetPlane(Vec3(0, 0, -1), info.pDecalClipInfo->vPos - info.vResultOffset + Vec3(0, 0, -fClipRadius));
	planes[2].SetPlane(Vec3(0, 1, 0), info.pDecalClipInfo->vPos - info.vResultOffset + Vec3(0, fClipRadius, 0));
	planes[3].SetPlane(Vec3(0, -1, 0), info.pDecalClipInfo->vPos - info.vResultOffset + Vec3(0, -fClipRadius, 0));
	planes[4].SetPlane(Vec3(1, 0, 0), info.pDecalClipInfo->vPos - info.vResultOffset + Vec3(fClipRadius, 0, 0));
	planes[5].SetPlane(Vec3(-1, 0, 0), info.pDecalClipInfo->vPos - info.vResultOffset + Vec3(-fClipRadius, 0, 0));

	// clip triangles
	int nOrigCount = m_lstIndices.Count();
	for (int i = 0; i < nOrigCount; i += 3)
	{
		if (ClipTriangle(i, planes, 6))
		{
			i -= 3;
			nOrigCount -= 3;
		}
	}

	if (m_lstIndices.Count() < 3 || m_lstVerts.Count() < 3)
		return;

	assert(m_lstTangBasises.Count() == m_lstVerts.Count());
#if ENABLE_NORMALSTREAM_SUPPORT
	assert(m_lstNormals.Count() == m_lstVerts.Count());
#endif

	{
		// minimize range
		uint32 nMin = ~0;
		uint32 nMax = 0;

		for (int i = 0; i < m_lstIndices.Count(); i++)
		{
			if (nMin > m_lstIndices[i]) nMin = m_lstIndices[i];
			if (nMax < m_lstIndices[i]) nMax = m_lstIndices[i];
		}

		for (int i = 0; i < m_lstIndices.Count(); i++)
			m_lstIndices[i] -= nMin;

		if (m_lstVerts.Count() - nMax - 1 > 0)
		{
			m_lstVerts.Delete(nMax + 1, m_lstVerts.Count() - (nMax + 1));
			m_lstTangBasises.Delete(nMax + 1, m_lstTangBasises.Count() - (nMax + 1));
#if ENABLE_NORMALSTREAM_SUPPORT
			m_lstNormals.Delete(nMax + 1, m_lstNormals.Count() - (nMax + 1));
#endif
		}

		m_lstVerts.Delete(0, nMin);
		m_lstTangBasises.Delete(0, nMin);
#if ENABLE_NORMALSTREAM_SUPPORT
		m_lstNormals.Delete(0, nMin);
#endif
	}

#ifdef _DEBUG
	assert(m_lstTangBasises.Count() == m_lstVerts.Count());
	#if ENABLE_NORMALSTREAM_SUPPORT
	assert(m_lstNormals.Count() == m_lstVerts.Count());
	#endif
	for (int i = 0; i < m_lstIndices.Count(); i++)
		assert(m_lstIndices[i] >= 0 && m_lstIndices[i] < (uint32)m_lstVerts.Count());
#endif

	if (m_lstIndices.Count() < 3 || m_lstVerts.Count() < 3)
		return;

	m_lstChunks[0].rChunk.nNumIndices = m_lstIndices.Count();
	m_lstChunks[0].rChunk.nNumVerts = m_lstVerts.Count();
}

//////////////////////////////////////////////////////////////////////////
void CRenderMeshMerger::TryMergingChunks(SMergeInfo& info)
{
	PodArray<SMergedChunk>& lstChunksMerged = m_lstChunksMergedTemp;
	lstChunksMerged.clear();
	lstChunksMerged.reserve(m_lstChunks.size());

	InputLayoutHandle nCurrVertFormat = InputLayoutHandle::Unspecified;;
	for (int nChunkId = 0; nChunkId < m_lstChunks.Count(); nChunkId++)
	{
		SMergedChunk& mergChunk = m_lstChunks[nChunkId];

		//		IsChunkValid(mergChunk, m_lstVerts, m_lstIndices);

		InputLayoutHandle nChunkVertFormat;
		if (info.pDecalClipInfo || info.bMergeToOneRenderMesh)
			nChunkVertFormat = InputLayoutHandle::Unspecified;
		else
		{
			IMaterial* pMat = mergChunk.pMaterial;
			SShaderItem& shaderItem = pMat->GetShaderItem();
			nChunkVertFormat = shaderItem.m_pShader->GetVertexFormat();
		}

		if (info.bMergeToOneRenderMesh)
		{
			nChunkVertFormat = nCurrVertFormat;
		}

		if (!nChunkId ||
		    (nChunkVertFormat != nCurrVertFormat || Cmp_RenderChunks_(&mergChunk, &m_lstChunks[nChunkId - 1]) ||
		     (lstChunksMerged.Last().rChunk.nNumVerts + mergChunk.rChunk.nNumVerts) > 0xFFFF))
		{
			// not equal materials - add new chunk
			lstChunksMerged.Add(mergChunk);
		}
		else
		{
			float texelAreaDensity = 0.0f;
			int totalIndices = 0;

			if (lstChunksMerged.Last().rChunk.m_texelAreaDensity != (float)UINT_MAX)
			{
				texelAreaDensity += lstChunksMerged.Last().rChunk.nNumIndices * lstChunksMerged.Last().rChunk.m_texelAreaDensity;
				totalIndices += lstChunksMerged.Last().rChunk.nNumIndices;
			}

			if (mergChunk.rChunk.m_texelAreaDensity != (float)UINT_MAX)
			{
				texelAreaDensity += mergChunk.rChunk.nNumIndices * mergChunk.rChunk.m_texelAreaDensity;
				totalIndices += mergChunk.rChunk.nNumIndices;
			}

			if (totalIndices != 0)
			{
				lstChunksMerged.Last().rChunk.m_texelAreaDensity = texelAreaDensity / totalIndices;
			}

			lstChunksMerged.Last().rChunk.nNumIndices += mergChunk.rChunk.nNumIndices;
			lstChunksMerged.Last().rChunk.nNumVerts += mergChunk.rChunk.nNumVerts;
		}

		IsChunkValid(lstChunksMerged.Last().rChunk, m_lstVerts, m_lstIndices);

		nCurrVertFormat = nChunkVertFormat;
	}

	m_lstChunks = lstChunksMerged;
}

//////////////////////////////////////////////////////////////////////////
_smart_ptr<IRenderMesh> CRenderMeshMerger::MergeRenderMeshes(SRenderMeshInfoInput* pRMIArray, int nRMICount,
                                                             PodArray<SRenderMeshInfoOutput>& outRenderMeshes,
                                                             SMergeInfo& info) PREFAST_SUPPRESS_WARNING(6262) //function uses > 32k stack space
{
	FUNCTION_PROFILER_3DENGINE;

	if (info.bPrintDebugMessages)
		PrintMessage("MergeRenderMeshs: name: %s, input brushes num: %d", info.sMeshName, nRMICount);

	m_nTotalVertexCount = 0;
	m_nTotalIndexCount = 0;

	m_lstRMIChunks.clear();
	m_lstVerts.clear();
	m_lstTangBasises.clear();
#if ENABLE_NORMALSTREAM_SUPPORT
	m_lstNormals.clear();
#endif
	m_lstIndices.clear();
	m_lstChunks.clear();

	// make list of all chunks
	MakeRenderMeshInfoListOfAllChunks(pRMIArray, nRMICount, info);

	if (info.bPrintDebugMessages)
		PrintMessage("%d render chunks found", m_lstRMIChunks.Count());

	if (!m_lstRMIChunks.Count())
		return NULL;

	// sort by materials
	if (!info.pDecalClipInfo)
		qsort(m_lstRMIChunks.GetElements(), m_lstRMIChunks.Count(), sizeof(m_lstRMIChunks[0]), Cmp_RenderChunksInfo);

	// make list of all CRenderChunks
	MakeListOfAllCRenderChunks(info);

	//	assert(IsHeapValid());

	if (!m_lstVerts.Count() || !m_lstChunks.Count() || !m_lstIndices.Count())
		return NULL;

	if (info.bPrintDebugMessages)
		PrintMessage("%d chunks left after culling (%d verts, %d indices)", m_lstChunks.Count(), m_lstVerts.Count(), m_lstIndices.Count());

	// Split chunks that does not fit together.
	TryMergingChunks(info);

	if (info.bPrintDebugMessages)
		PrintMessage("%d chunks left after merging", m_lstChunks.Count());

	if (!m_lstChunks.Count())
		return NULL;

	// now we have list of merged/sorted chunks, indices and vertices's
	// overall amount of vertices's may be more than 0xFFFF
	if (m_lstChunks.Count() == 1 && info.pDecalClipInfo && GetCVars()->e_DecalsClip)
	{
		// clip decals if needed
		ClipDecals(info);
		if (m_lstIndices.Count() < 3 || m_lstVerts.Count() < 3)
			return NULL;

		// find AABB
		AABB aabb = AABB(m_lstVerts[0].xyz.ToVec3(), m_lstVerts[0].xyz.ToVec3());
		for (int i = 0; i < m_lstVerts.Count(); i++)
			aabb.Add(m_lstVerts[i].xyz.ToVec3());

		// weld positions
		mesh_compiler::CMeshCompiler meshCompiler;
#if ENABLE_NORMALSTREAM_SUPPORT
		PREFAST_SUPPRESS_WARNING(6385);
		meshCompiler.WeldPos_VF_P3X(m_lstVerts, m_lstTangBasises, m_lstNormals, m_lstIndices, VEC_EPSILON, aabb);
#else
		PodArray<Vec3> lstNormalsDummy;
		meshCompiler.WeldPos_VF_P3X(m_lstVerts, m_lstTangBasises, lstNormalsDummy, m_lstIndices, VEC_EPSILON, aabb);
#endif

		// update chunk
		CRenderChunk& Ch0 = m_lstChunks[0].rChunk;
		Ch0.nFirstIndexId = 0;
		Ch0.nNumIndices = m_lstIndices.Count();
		Ch0.nFirstVertId = 0;
		Ch0.nNumVerts = m_lstVerts.Count();
	}

	if (/*GetCVars()->e_scene_merging_compact_vertices && */ info.bCompactVertBuffer)
	{
		// remove gaps in vertex buffers
		CompactVertices(info);
	}

#ifdef _DEBUG
	for (int nChunkId = 0; nChunkId < m_lstChunks.Count(); nChunkId++)
	{
		CRenderChunk& Ch0 = m_lstChunks[nChunkId].rChunk;
		IsChunkValid(Ch0, m_lstVerts, m_lstIndices);
	}
#endif // _DEBUG

	if (info.bPrintDebugMessages)
		PrintMessage("Making new RenderMeshes");

	outRenderMeshes.clear();

	m_lstNewChunks.reserve(m_lstChunks.Count());
	m_lstNewVerts.reserve(m_nTotalVertexCount);
	m_lstNewTangBasises.reserve(m_nTotalVertexCount);
#if ENABLE_NORMALSTREAM_SUPPORT
	m_lstNormals.reserve(m_nTotalVertexCount);
#endif
	m_lstNewIndices.reserve(m_nTotalIndexCount);

	for (int nChunkId = 0; nChunkId < m_lstChunks.Count(); )
	{
		AABB finalBox;
		finalBox.Reset();

		m_lstNewVerts.clear();
		m_lstNewTangBasises.clear();
#if ENABLE_NORMALSTREAM_SUPPORT
		m_lstNormals.clear();
#endif
		m_lstNewIndices.clear();
		m_lstNewChunks.clear();

		int nVertsNum = 0;
		for (; nChunkId < m_lstChunks.Count(); )
		{
			CRenderChunk Ch = m_lstChunks[nChunkId].rChunk;
			SMergedChunk& mrgChunk = m_lstChunks[nChunkId];

			assert(m_lstNewVerts.Count() + Ch.nNumVerts <= 0xFFFF);

			IsChunkValid(Ch, m_lstVerts, m_lstIndices);

			int nCurrIdxPos = m_lstNewIndices.Count();
			int nCurrVertPos = m_lstNewVerts.Count();

			m_lstNewVerts.AddList(&m_lstVerts[Ch.nFirstVertId], Ch.nNumVerts);
			m_lstNewTangBasises.AddList(&m_lstTangBasises[Ch.nFirstVertId], Ch.nNumVerts);
#if ENABLE_NORMALSTREAM_SUPPORT
			m_lstNormals.AddList(&m_lstNormals[Ch.nFirstVertId], Ch.nNumVerts);
#endif

			for (uint32 i = Ch.nFirstIndexId; i < Ch.nFirstIndexId + Ch.nNumIndices; i++)
			{
				uint32 nIndex = m_lstIndices[i] - Ch.nFirstVertId + nCurrVertPos;
				assert(nIndex >= 0 && nIndex <= 0xFFFF && nIndex < (uint32)m_lstNewVerts.Count());
				nIndex = nIndex & 0xFFFF;
				m_lstNewIndices.Add(nIndex);
				finalBox.Add(m_lstNewVerts[nIndex].xyz.ToVec3());
			}

			Ch.nFirstIndexId = nCurrIdxPos;
			Ch.nFirstVertId = nCurrVertPos;

			nVertsNum += Ch.nNumVerts;

			{
				assert(Ch.nFirstIndexId + Ch.nNumIndices <= (uint32)m_lstNewIndices.Count());
				assert(Ch.nFirstVertId + Ch.nNumVerts <= (uint16)m_lstNewVerts.Count());

#ifdef _DEBUG
				for (uint32 i = Ch.nFirstIndexId; i < Ch.nFirstIndexId + Ch.nNumIndices; i++)
				{
					assert(m_lstNewIndices[i] >= Ch.nFirstVertId && m_lstNewIndices[i] < Ch.nFirstVertId + Ch.nNumVerts);
					assert((int)m_lstNewIndices[i] < m_lstNewVerts.Count());
				}
#endif
			}

			SMergedChunk newMergedChunk;
			newMergedChunk.rChunk = Ch;
			newMergedChunk.pMaterial = mrgChunk.pMaterial;
			m_lstNewChunks.Add(newMergedChunk);

			nChunkId++;

			if (nChunkId < m_lstChunks.Count())
			{
				if (nVertsNum + m_lstChunks[nChunkId].rChunk.nNumVerts > 0xFFFF)
					break;

				if (info.bMergeToOneRenderMesh)
					continue;

				// detect vert format change
				SShaderItem& shaderItemCur = mrgChunk.pMaterial->GetShaderItem();
				InputLayoutHandle nNextChunkVertFormatCur = shaderItemCur.m_pShader->GetVertexFormat();

				SShaderItem& shaderItemNext = m_lstChunks[nChunkId].pMaterial->GetShaderItem();
				InputLayoutHandle nNextChunkVertFormatNext = shaderItemNext.m_pShader->GetVertexFormat();

				if (nNextChunkVertFormatNext != nNextChunkVertFormatCur)
					break;
			}
		}

		IRenderMesh::SInitParamerers params;
		params.pVertBuffer = m_lstNewVerts.GetElements();
		params.nVertexCount = m_lstNewVerts.Count();
		params.eVertexFormat = EDefaultInputLayouts::P3S_C4B_T2S;
		params.pIndices = m_lstNewIndices.GetElements();
		params.nIndexCount = m_lstNewIndices.Count();
		params.pTangents = m_lstNewTangBasises.GetElements();
#if ENABLE_NORMALSTREAM_SUPPORT
		params.pNormals = m_lstNewNormals.GetElements();
#endif
		params.nPrimetiveType = prtTriangleList;
		params.eType = eRMT_Static;
		params.nRenderChunkCount = 0;
		params.bOnlyVideoBuffer = false;
		params.bPrecache = false;

		// make new RenderMesh
		_smart_ptr<IRenderMesh> pNewLB = GetRenderer()->CreateRenderMesh(info.sMeshName, info.sMeshType, &params);

		IMaterial* pParentMaterial = NULL;

		pNewLB->SetBBox(finalBox.min, finalBox.max);

		if (!info.pUseMaterial)
		{
			// make new parent material
			if (!info.pDecalClipInfo)
			{
				char szMatName[256];
				cry_sprintf(szMatName, "%s_Material", info.sMeshName);
				pParentMaterial = GetMatMan()->CreateMaterial(szMatName, MTL_FLAG_MULTI_SUBMTL);
				pParentMaterial->AddRef();
				pParentMaterial->SetSubMtlCount(m_lstChunks.Count());
			}

			// define chunks
			for (int i = 0; i < m_lstNewChunks.Count(); i++)
			{
				CRenderChunk* pChunk = &m_lstNewChunks[i].rChunk;

				IMaterial* pMat = m_lstNewChunks[i].pMaterial;

				if (pParentMaterial)
				{
					assert(pMat);
					pParentMaterial->SetSubMtl(i, pMat);
				}

				assert(pChunk->nFirstIndexId + pChunk->nNumIndices <= (uint32)m_lstNewIndices.Count());

				pChunk->m_nMatID = i;
				if (pMat)
					pChunk->m_nMatFlags = pMat->GetFlags();
				pNewLB->SetChunk(i, *pChunk);
			}
		}
		else
		{
			pParentMaterial = info.pUseMaterial;

			// define chunks
			for (int i = 0; i < m_lstNewChunks.Count(); i++)
			{
				CRenderChunk* pChunk = &m_lstNewChunks[i].rChunk;
				assert(pChunk->nFirstIndexId + pChunk->nNumIndices <= (uint32)m_lstNewIndices.Count());

				IMaterial* pSubMtl = info.pUseMaterial->GetSafeSubMtl(pChunk->m_nMatID);
				pChunk->m_nMatFlags = pSubMtl->GetFlags();

				pNewLB->SetChunk(i, *pChunk);
			}
		}

		SRenderMeshInfoOutput rmi;
		rmi.pMesh = pNewLB;
		rmi.pMat = pParentMaterial;
		if (pParentMaterial)
			pParentMaterial->AddRef();

		outRenderMeshes.push_back(rmi);
	}

	if (info.bPrintDebugMessages)
		PrintMessage("%" PRISIZE_T " RenderMeshes created", outRenderMeshes.size());

	return outRenderMeshes.Count() ? outRenderMeshes[0].pMesh : _smart_ptr<IRenderMesh>(NULL);
}

struct sort_render_chunks_by_material
{
	bool operator()(const SMergedChunk& c1, const SMergedChunk& c2) const
	{
		return c1.pMaterial < c2.pMaterial;
	}
};

//////////////////////////////////////////////////////////////////////////
bool CRenderMeshMerger::GenerateRenderChunks(SRenderMeshInfoInput* pRMIArray, int nRMICount)
{
	bool bCanMerge = true;
	PodArray<SMergedChunk>& allChunks = m_lstChunksAll;

	allChunks.clear();
	allChunks.reserve(nRMICount);

	for (int nRmi = 0; nRmi < nRMICount; nRmi++)
	{
		SRenderMeshInfoInput* pRMI = &pRMIArray[nRmi];
		IRenderMesh* pRM = pRMI->pMesh;

		// Ignore bad meshes.
		if (pRM->GetVerticesCount() == 0 || pRM->GetIndicesCount() == 0)
			continue;

		const TRenderChunkArray& chunks = pRM->GetChunks();
		int nChunkCount = chunks.size();
		for (int nChunk = 0; nChunk < nChunkCount; nChunk++)
		{
			const CRenderChunk& renderChunk = chunks[nChunk];

			if (renderChunk.m_nMatFlags & MTL_FLAG_NODRAW || !renderChunk.pRE)
				continue;

			if (renderChunk.nNumVerts == 0 || renderChunk.nNumIndices == 0)
				continue;

			if (!pRMI->pMat)
				continue;

			IMaterial* pCustMat = (pRMI->pMat) ? pRMI->pMat->GetSafeSubMtl(renderChunk.m_nMatID) : 0;
			if (!pCustMat)
				continue;

			if (!pCustMat->GetShaderItem().m_pShader)
				pCustMat = GetMatMan()->GetDefaultMaterial();

			IShader* pShader = pCustMat->GetShaderItem().m_pShader;

			if (!pShader)
				continue;

			if (pShader->GetFlags() & EF_NODRAW)
				continue;

			SMergedChunk newChunk;
			newChunk.pFromMesh = pRMI;
			newChunk.pMaterial = pCustMat;
			newChunk.rChunk = renderChunk;
			newChunk.rChunk.nSubObjectIndex = pRMI->nSubObjectIndex;
			newChunk.rChunk.pRE = 0;

			allChunks.push_back(newChunk);
		}
	}

	// sort by materials
	std::sort(allChunks.begin(), allChunks.end(), sort_render_chunks_by_material());

	return bCanMerge;
}

//////////////////////////////////////////////////////////////////////////
void CRenderMeshMerger::MergeRenderChunks()
{
	PodArray<SMergedChunk>& allChunks = m_lstChunksAll;

	//////////////////////////////////////////////////////////////////////////
	// Create array of merged chunks.
	//////////////////////////////////////////////////////////////////////////
	PodArray<SMergedChunk>& mergedChunks = m_lstChunks;
	mergedChunks.clear();
	mergedChunks.reserve(allChunks.size());

	if (allChunks.size() > 0)
	{
		// Add first chunk.
		mergedChunks.push_back(allChunks[0]);
	}

	for (size_t nChunkId = 1; nChunkId < allChunks.size(); nChunkId++)
	{
		SMergedChunk& currChunk = allChunks[nChunkId];

		SMergedChunk& prevChunk = mergedChunks.back();

		if ((currChunk.pMaterial != prevChunk.pMaterial) ||
		    ((prevChunk.rChunk.nNumVerts + currChunk.rChunk.nNumVerts) > 0xFFFF) ||
		    ((prevChunk.rChunk.nNumIndices + currChunk.rChunk.nNumIndices) > 0xFFFF))
		{
			// not equal materials - add new chunk
			mergedChunks.Add(currChunk);
		}
		else
		{
			float texelAreaDensity = 0.0f;
			int totalIndices = 0;

			if (prevChunk.rChunk.m_texelAreaDensity != (float)UINT_MAX)
			{
				texelAreaDensity += prevChunk.rChunk.nNumIndices * prevChunk.rChunk.m_texelAreaDensity;
				totalIndices += prevChunk.rChunk.nNumIndices;
			}

			if (currChunk.rChunk.m_texelAreaDensity != (float)UINT_MAX)
			{
				texelAreaDensity += currChunk.rChunk.nNumIndices * currChunk.rChunk.m_texelAreaDensity;
				totalIndices += currChunk.rChunk.nNumIndices;
			}

			if (totalIndices != 0)
			{
				prevChunk.rChunk.m_texelAreaDensity = texelAreaDensity / totalIndices;
			}
			else
			{
				prevChunk.rChunk.m_texelAreaDensity = 1.f;
			}

			prevChunk.rChunk.nNumIndices += currChunk.rChunk.nNumIndices;
			prevChunk.rChunk.nNumVerts += currChunk.rChunk.nNumVerts;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderMeshMerger::MergeBuffers(AABB& bounds)
{
	FUNCTION_PROFILER_3DENGINE;

	m_nTotalVertexCount = 0;
	m_nTotalIndexCount = 0;

	int nNeedVertices = 0;
	int nNeedIndices = 0;

	PodArray<SMergedChunk>& allChunks = m_lstChunksAll;

	// Calculate total required sizes.
	for (size_t nChunk = 0; nChunk < allChunks.size(); nChunk++)
	{
		SMergedChunk& renderChunk = allChunks[nChunk];
		nNeedIndices += renderChunk.rChunk.nNumIndices;
		nNeedVertices += renderChunk.rChunk.nNumVerts;
	}

	PodArray<vtx_idx>& arrIndices = m_lstNewIndices;
	arrIndices.clear();
	arrIndices.reserve(nNeedIndices);

	PodArray<SVF_P3S_C4B_T2S>& arrVertices = m_lstVerts;
	arrVertices.clear();
	arrVertices.reserve(nNeedVertices);

	PodArray<SPipTangents>& arrTangents = m_lstTangBasises;
	arrTangents.clear();
	arrTangents.reserve(nNeedVertices);

#if ENABLE_NORMALSTREAM_SUPPORT
	PodArray<SPipNormal>& arrNormals = m_lstNormals;
	arrNormals.clear();
	arrNormals.reserve(nNeedVertices);
#endif

	m_lstMergeBuffersData.clear();
	m_lstMergeBuffersData.resize(allChunks.size());

	m_tmpAABB = bounds;

	// do all GetPos etc calls before in the t thread
	for (size_t nChunk = 0; nChunk < allChunks.size(); nChunk++)
	{
		SMergeBuffersData& rMergeBufferData = m_lstMergeBuffersData[nChunk];
		IRenderMesh* pRM = allChunks[nChunk].pFromMesh->pMesh;
		pRM->LockForThreadAccess();
		rMergeBufferData.pPos = pRM->GetPosPtr(rMergeBufferData.nPosStride, FSL_READ);
		rMergeBufferData.pTex = pRM->GetUVPtr(rMergeBufferData.nTexStride, FSL_READ);
		rMergeBufferData.pColor = pRM->GetColorPtr(rMergeBufferData.nColorStride, FSL_READ);
		rMergeBufferData.pTangs = pRM->GetTangentPtr(rMergeBufferData.nTangsStride, FSL_READ);
		rMergeBufferData.nIndCount = pRM->GetIndicesCount();
		rMergeBufferData.pSrcInds = pRM->GetIndexPtr(FSL_READ);

#if ENABLE_NORMALSTREAM_SUPPORT
		rMergeBufferData.pNorm = pRM->GetNormPtr(rMergeBufferData.nNormStride, FSL_READ);
#endif
	}

	MergeBuffersImpl(&m_tmpAABB, m_lstMergeBuffersData.GetElements());

	// operation on buffers has finished, unlock them again for rendermesh garbage collection
	for (size_t nChunk = 0; nChunk < allChunks.size(); nChunk++)
	{
		IRenderMesh* pRM = allChunks[nChunk].pFromMesh->pMesh;
		pRM->UnLockForThreadAccess();
	}

	bounds = m_tmpAABB;
}

void CRenderMeshMerger::MergeBuffersImpl(AABB* pBounds, SMergeBuffersData* _arrMergeBuffersData)
{
	// size for SPU buffers, currently we have 16 buffers, means we need 128 kb
	uint32 nNumMergeChunks = m_lstChunksAll.size();

	PodArray<SMergedChunk>& allChunks = m_lstChunksAll;
	SMergeBuffersData* arrMergeBuffersData = _arrMergeBuffersData;

	PodArray<vtx_idx>& arrIndices = m_lstNewIndices;
	PodArray<SVF_P3S_C4B_T2S>& arrVertices = m_lstVerts;
	PodArray<SPipTangents>& arrTangents = m_lstTangBasises;

#if ENABLE_NORMALSTREAM_SUPPORT
	PodArray<SPipNormal>& arrNormals = m_lstNormals;
#endif

	for (size_t nChunk = 0; nChunk < nNumMergeChunks; nChunk++)
	{

		SMergedChunk& renderChunk = allChunks[nChunk];
		SMergeBuffersData& rMergeBufferData = arrMergeBuffersData[nChunk];
		SRenderMeshInfoInput* pRMI = renderChunk.pFromMesh;
		Matrix34& rMatrix = pRMI->mat;

		bool bMatrixHasRotation;
		if (!rMatrix.m01 && !rMatrix.m02 && !rMatrix.m10 && !rMatrix.m12 && !rMatrix.m20 && !rMatrix.m21)
			bMatrixHasRotation = false;
		else
			bMatrixHasRotation = true;

		Vec3 vOffset = rMatrix.GetTranslation();

		IRenderMesh* pRM = pRMI->pMesh;

		// get streams.
		int nPosStride = rMergeBufferData.nPosStride;
		int nTexStride = rMergeBufferData.nTexStride;
		int nColorStride = rMergeBufferData.nColorStride;
		int nTangsStride = rMergeBufferData.nTangsStride;

		uint8* pPos = rMergeBufferData.pPos;
		uint8* pTex = rMergeBufferData.pTex;
		uint8* pColor = rMergeBufferData.pColor;
		uint8* pTangs = rMergeBufferData.pTangs;

#if ENABLE_NORMALSTREAM_SUPPORT
		int nNormStride = rMergeBufferData.nNormStride;
		uint8* pNorm = rMergeBufferData.pNorm;
#endif

		if (!pPos || !pTex || !pColor || !pTangs)
		{
			assert(0); // Should not happen.
			continue;
		}

		Vec3 vMin, vMax;
		pRM->GetBBox(vMin, vMax);
		pBounds->Add(vMin);
		pBounds->Add(vMax);

		// get indices
		vtx_idx* pInds = rMergeBufferData.pSrcInds;

		Vec3 vOSPos(0, 0, 0);
		Vec3 vOSProjDir(0, 0, 0);

		int nLastVertex = arrVertices.size();
		int nLastIndex = arrIndices.size();

		//////////////////////////////////////////////////////////////////////////
		// add indices
		//////////////////////////////////////////////////////////////////////////
		int nCurrIndex = nLastIndex;
		arrIndices.resize(nLastIndex + renderChunk.rChunk.nNumIndices);
		int nAdjustedVertexOffset = nLastVertex - renderChunk.rChunk.nFirstVertId;
		int i = renderChunk.rChunk.nFirstIndexId;
		int numInd = renderChunk.rChunk.nFirstIndexId + renderChunk.rChunk.nNumIndices;

		vtx_idx* __restrict pSrcInds = pInds;
		vtx_idx* __restrict pDstInds = &arrIndices[0];

		for (; i < numInd; i += 3, nCurrIndex += 3)
		{
			pDstInds[nCurrIndex + 0] = pSrcInds[i + 0] + nAdjustedVertexOffset;
			pDstInds[nCurrIndex + 1] = pSrcInds[i + 1] + nAdjustedVertexOffset;
			pDstInds[nCurrIndex + 2] = pSrcInds[i + 2] + nAdjustedVertexOffset;
		}

		//////////////////////////////////////////////////////////////////////////
		renderChunk.rChunk.nFirstIndexId = nLastIndex;

		//////////////////////////////////////////////////////////////////////////
		// add vertices
		//////////////////////////////////////////////////////////////////////////
		int nCurrVertex = nLastVertex;
		arrVertices.resize(nLastVertex + renderChunk.rChunk.nNumVerts);
		arrTangents.resize(nLastVertex + renderChunk.rChunk.nNumVerts);

#if ENABLE_NORMALSTREAM_SUPPORT
		arrNormals.resize(nLastVertex + renderChunk.rChunk.nNumVerts);
#endif

		int v = renderChunk.rChunk.nFirstVertId;
		int numVert = renderChunk.rChunk.nFirstVertId + renderChunk.rChunk.nNumVerts;
		if (bMatrixHasRotation)
		{
			SVF_P3S_C4B_T2S* __restrict pDstVerts = &arrVertices[0];
			SPipTangents* __restrict pDstTangs = &arrTangents[0];
#if ENABLE_NORMALSTREAM_SUPPORT
			SPipNormal* __restrict pDstNorms = &arrNormals[0];
#endif

			uint8* pSrcPos = pPos;
			uint8* pSrcTex = pTex;
			uint8* pSrcColor = pColor;
			uint8* pSrcTangs = pTangs;
#if ENABLE_NORMALSTREAM_SUPPORT
			uint8* pSrcNorm = pNorm;
#endif
			for (; v < numVert; v++, nCurrVertex++)
			{
				SVF_P3S_C4B_T2S& vert = pDstVerts[nCurrVertex];
				SPipTangents& basis = pDstTangs[nCurrVertex];
#if ENABLE_NORMALSTREAM_SUPPORT
				SPipNormal& normal = pDstNorms[nCurrVertex];
#endif
				// set pos/uv
				Vec3& vPos = (*(Vec3*)&pSrcPos[nPosStride * v]);
				Vec2* pUV = (Vec2*)&pSrcTex[nTexStride * v];

				vert.xyz = rMatrix.TransformPoint(vPos);
				vert.st = *pUV;
				vert.color.dcolor = *(uint32*)&pSrcColor[nColorStride * v + 0];

				// get tangent basis
				basis = *(SPipTangents*)&pSrcTangs[nTangsStride * v];

#if ENABLE_NORMALSTREAM_SUPPORT
				normal = SPipNormal(Vec3(0, 0, 0));
				if (pSrcNorm)
					normal = *(SPipNormal*)&pSrcNorm[nNormStride * v];
#endif

				if (bMatrixHasRotation)
				{
					basis.TransformSafelyBy(rMatrix);
#if ENABLE_NORMALSTREAM_SUPPORT
					if (pSrcNorm)
					{
						normal.TransformSafelyBy(rMatrix);
					}
#endif
				}
			}
		}
		else
		{
			SVF_P3S_C4B_T2S* __restrict pDstVerts = &arrVertices[0];
			SPipTangents* __restrict pDstTangs = &arrTangents[0];
#if ENABLE_NORMALSTREAM_SUPPORT
			SPipNormal* __restrict pDstNorms = &arrNormals[0];
#endif

			uint8* pSrcPos = pPos;
			uint8* pSrcTex = pTex;
			uint8* pSrcColor = pColor;
			uint8* pSrcTangs = pTangs;
#if ENABLE_NORMALSTREAM_SUPPORT
			uint8* pSrcNorm = pNorm;
#endif
			for (; v < numVert; v++, nCurrVertex++)
			{
				SVF_P3S_C4B_T2S& vert = pDstVerts[nCurrVertex];
				SPipTangents& basis = pDstTangs[nCurrVertex];

#if ENABLE_NORMALSTREAM_SUPPORT
				SPipNormal& normal = pDstNorms[nCurrVertex];
#endif

				// set pos/uv
				Vec3& vPos = (*(Vec3*)&pSrcPos[nPosStride * v]);
				Vec2* pUV = (Vec2*)&pSrcTex[nTexStride * v];

				vert.xyz = rMatrix.TransformPoint(vPos);
				vert.st = *pUV;
				vert.color.dcolor = *(uint32*)&pSrcColor[nColorStride * v];

				// get tangent basis
				basis = *(SPipTangents*)&pSrcTangs[nTangsStride * v];

#if ENABLE_NORMALSTREAM_SUPPORT
				normal = SPipNormal(Vec3(0, 0, 0));
				if (pSrcNorm)
				{
					normal = *(SPipNormal*)(&pSrcNorm[nNormStride * v]);
				}
#endif
			}
		}

		// set vert range
		renderChunk.rChunk.nFirstVertId = nLastVertex;

		m_nTotalVertexCount += renderChunk.rChunk.nNumVerts;
		m_nTotalIndexCount += renderChunk.rChunk.nNumIndices;
	}
}

void CRenderMeshMerger::Reset()
{
	stl::free_container(m_lstRMIChunks);
	stl::free_container(m_lstVerts);
	stl::free_container(m_lstTangBasises);
	stl::free_container(m_lstIndices);
	stl::free_container(m_lstChunks);
	stl::free_container(m_lstChunksAll);
	stl::free_container(m_lstNewVerts);
	stl::free_container(m_lstNewTangBasises);
	stl::free_container(m_lstNewIndices);
	stl::free_container(m_lstNewChunks);
	stl::free_container(m_lstChunksMergedTemp);
	stl::free_container(m_tmpRenderChunkArray);
	stl::free_container(m_lstMergeBuffersData);

#if ENABLE_NORMALSTREAM_SUPPORT
	stl::free_container(m_lstNormals);
	stl::free_container(m_lstNewNormals);
#endif

	m_tmpClipContext.Reset();
}

//////////////////////////////////////////////////////////////////////////
_smart_ptr<IRenderMesh> CRenderMeshMerger::MergeRenderMeshes(SRenderMeshInfoInput* pRMIArray, int nRMICount, SMergeInfo& info)
{
	m_nTotalVertexCount = 0;
	m_nTotalIndexCount = 0;

	m_lstRMIChunks.clear();
	m_lstVerts.clear();
	m_lstTangBasises.clear();
#if ENABLE_NORMALSTREAM_SUPPORT
	m_lstNormals.clear();
#endif
	m_lstIndices.clear();
	m_lstNewIndices.clear();
	m_lstChunks.clear();

	// make list of all chunks
	if (!GenerateRenderChunks(pRMIArray, nRMICount))
	{
		return 0;
	}

	MergeRenderChunks();

	if (m_lstChunksAll.empty())
		return NULL;

	// Often even single mesh must be merged, when non identity matrix is provided
	/*
	   if (m_lstChunks.size() == m_lstChunksAll.size())
	   {
	   // No reduction of render chunks.
	   return 0;
	   }
	 */

	AABB finalBounds;
	finalBounds.Reset();
	MergeBuffers(finalBounds);

	if (m_lstNewIndices.empty() || m_lstVerts.empty() || m_lstTangBasises.empty())
	{
		return 0;
	}

	// Repeat merging to properly update vertex ranges.
	MergeRenderChunks();

	IRenderMesh::SInitParamerers params;
	params.pVertBuffer = &m_lstVerts.front();
	params.nVertexCount = m_lstVerts.size();
	params.eVertexFormat = EDefaultInputLayouts::P3S_C4B_T2S;
	params.pIndices = &m_lstNewIndices.front();
	params.nIndexCount = m_lstNewIndices.size();
	params.pTangents = &m_lstTangBasises.front();
#if ENABLE_NORMALSTREAM_SUPPORT
	params.pNormals = &m_lstNormals.front();
#endif
	params.nPrimetiveType = prtTriangleList;
	params.eType = eRMT_Static;
	params.nRenderChunkCount = 0;
	params.bOnlyVideoBuffer = false;
	params.bPrecache = false;
	params.bLockForThreadAccess = true;//calls LockForThreadAccess in the Rendermesh ctor

	_smart_ptr<IRenderMesh> pRenderMesh = GetRenderer()->CreateRenderMesh(info.sMeshType, info.sMeshName, &params);
	pRenderMesh->SetBBox(finalBounds.min, finalBounds.max);

	//////////////////////////////////////////////////////////////////////////
	// Setup merged chunks
	//////////////////////////////////////////////////////////////////////////
	m_tmpRenderChunkArray.resize(m_lstChunks.size());
	for (size_t i = 0, num = m_lstChunks.size(); i < num; i++)
	{
		m_tmpRenderChunkArray[i] = m_lstChunks[i].rChunk;
	}
	pRenderMesh->SetRenderChunks(&m_tmpRenderChunkArray.front(), m_tmpRenderChunkArray.size(), false);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Setup un-merged chunks
	//////////////////////////////////////////////////////////////////////////
	m_tmpRenderChunkArray.resize(m_lstChunksAll.size());
	for (size_t i = 0, num = m_lstChunksAll.size(); i < num; i++)
	{
		m_tmpRenderChunkArray[i] = m_lstChunksAll[i].rChunk;
	}
	pRenderMesh->SetRenderChunks(&m_tmpRenderChunkArray.front(), m_tmpRenderChunkArray.size(), true);
	//////////////////////////////////////////////////////////////////////////

	pRenderMesh->UnLockForThreadAccess();

	return pRenderMesh;
}
