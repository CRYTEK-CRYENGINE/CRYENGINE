// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   RenderMeshUtils.cpp
//  Created:     14/11/2006 by Timur.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "RenderMeshUtils.h"

namespace
{
enum { MAX_CACHED_HITS = 8 };
struct SCachedHit
{
	IRenderMesh* pRenderMesh;
	SRayHitInfo  hitInfo;
	Vec3         tri[3];
};
static SCachedHit last_hits[MAX_CACHED_HITS];
}

bool SIntersectionData::Init(IRenderMesh* param_pRenderMesh, SRayHitInfo* param_pHitInfo, IMaterial* param_pMtl, bool param_bRequestDecalPlacementTest)
{
	pRenderMesh = param_pRenderMesh;
	pHitInfo = param_pHitInfo;
	pMtl = param_pMtl;
	bDecalPlacementTestRequested = param_bRequestDecalPlacementTest;

	bool bAllDMeshData = pHitInfo->bGetVertColorAndTC;

	nVerts = pRenderMesh->GetVerticesCount();
	nInds = pRenderMesh->GetIndicesCount();

	if (nInds == 0 || nVerts == 0)
		return false;
	pPos = (uint8*)pRenderMesh->GetPosPtr(nPosStride, FSL_READ);
	pInds = pRenderMesh->GetIndexPtr(FSL_READ);

	if (!pPos || !pInds)
		return false;

	if (bAllDMeshData)
	{
		pUV = (uint8*)pRenderMesh->GetUVPtr(nUVStride, FSL_READ);
		pCol = (uint8*)pRenderMesh->GetColorPtr(nColStride, FSL_READ);

		pTangs = pRenderMesh->GetTangentPtr(nTangsStride, FSL_READ);
	}

	return true;
}

template<class T> bool GetBarycentricCoordinates(T P_A, T B_A, T C_A, float& u, float& v, float& w, float fBorder)
{
	// Compute vectors
	const T& v0 = C_A;
	const T& v1 = B_A;
	const T& v2 = P_A;

	// Compute dot products
	float dot00 = v0.Dot(v0);
	float dot01 = v0.Dot(v1);
	float dot02 = v0.Dot(v2);
	float dot11 = v1.Dot(v1);
	float dot12 = v1.Dot(v2);

	// Compute barycentric coordinates
	float invDenom = 1.f / (dot00 * dot11 - dot01 * dot01);
	u = (dot11 * dot02 - dot01 * dot12) * invDenom;
	v = (dot00 * dot12 - dot01 * dot02) * invDenom;
	w = 1.f - u - v;

	// Check if point is in triangle
	return (u >= -fBorder) && (v >= -fBorder) && (w >= -fBorder);
}

void CRenderMeshUtils::ClearHitCache()
{
	// do not allow items to stay too long in the cache, it allows to minimize wrong hit detections
	memmove(&last_hits[1], &last_hits[0], sizeof(last_hits) - sizeof(last_hits[0])); // Move hits to the end of array, throwing out the last one.
	memset(&last_hits[0], 0, sizeof(last_hits[0]));
}
//////////////////////////////////////////////////////////////////////////
bool CRenderMeshUtils::RayIntersection(IRenderMesh* pRenderMesh, SRayHitInfo& hitInfo, IMaterial* pMtl)
{
	SIntersectionData data;

	pRenderMesh->LockForThreadAccess();
	if (!data.Init(pRenderMesh, &hitInfo, pMtl))
	{
		return false;
	}

	// forward call to implementation
	bool result = CRenderMeshUtils::RayIntersectionImpl(&data, &hitInfo, pMtl, false);

	pRenderMesh->UnlockStream(VSF_GENERAL);
	pRenderMesh->UnlockIndexStream();
	pRenderMesh->UnLockForThreadAccess();
	return result;
}

void CRenderMeshUtils::RayIntersectionAsync(SIntersectionData* pIntersectionRMData)
{
	// forward call to implementation
	SRayHitInfo& rHitInfo = *pIntersectionRMData->pHitInfo;
	SIntersectionData& rIntersectionData = *pIntersectionRMData;

	if (CRenderMeshUtils::RayIntersectionImpl(&rIntersectionData, &rHitInfo, rIntersectionData.pMtl, true))
	{
		const float testAreaSize = GetFloatCVar(e_DecalsPlacementTestAreaSize);
		const float minTestDepth = GetFloatCVar(e_DecalsPlacementTestMinDepth);

		if (rIntersectionData.bDecalPlacementTestRequested && testAreaSize)
		{
			rIntersectionData.fDecalPlacementTestMaxSize = 0.f;
			float fRange = testAreaSize * 0.5f;

			for (uint32 i = 0; i < 2; ++i, fRange *= 2.f)
			{
				Vec3 vDir = -rHitInfo.vHitNormal;
				vDir.Normalize();

				Vec3 vRight;
				Vec3 vUp;

				if (fabs(vDir.Dot(Vec3(0, 0, 1))) > 0.995f)
				{
					vRight = Vec3(1, 0, 0);
					vUp = Vec3(0, 1, 0);
				}
				else
				{
					vRight = vDir.Cross(Vec3(0, 0, 1));
					vUp = vRight.Cross(vDir);
				}

				Vec3 arrOffset[4] = { vRight* fRange, -vRight * fRange, vUp * fRange, -vUp * fRange };

				SRayHitInfo hInfo;
				SIntersectionData intersectionData;

				float fDepth = max(minTestDepth, fRange * 0.2f);

				int nPoint;
				for (nPoint = 0; nPoint < 4; nPoint++)
				{
					intersectionData = rIntersectionData;

					hInfo = rHitInfo;
					hInfo.inReferencePoint = hInfo.vHitPos + arrOffset[nPoint];//*fRange;
					hInfo.inRay.origin = hInfo.inReferencePoint + hInfo.vHitNormal * fDepth;
					hInfo.inRay.direction = -hInfo.vHitNormal * fDepth * 2.f;
					hInfo.fMaxHitDistance = fDepth;
					if (!CRenderMeshUtils::RayIntersectionImpl(&intersectionData, &hInfo, rIntersectionData.pMtl, true))
						break;
				}

				if (nPoint == 4)
					rIntersectionData.fDecalPlacementTestMaxSize = fRange;
				else
					break;
			}
		}
	}
}

bool CRenderMeshUtils::RayIntersectionImpl(SIntersectionData* pIntersectionRMData, SRayHitInfo* phitInfo, IMaterial* pMtl, bool bAsync)
{
	IF (phitInfo->bGetVertColorAndTC, 0)
		return RayIntersectionFastImpl(*pIntersectionRMData, *phitInfo, pMtl, bAsync);

	SIntersectionData& rIntersectionRMData = *pIntersectionRMData;
	SRayHitInfo& hitInfo = *phitInfo;

	FUNCTION_PROFILER_3DENGINE;

	//CTimeValue t0 = gEnv->pTimer->GetAsyncTime();

	float fMaxDist2 = hitInfo.fMaxHitDistance * hitInfo.fMaxHitDistance;

	Vec3 vHitPos(0, 0, 0);
	Vec3 vHitNormal(0, 0, 0);

	static bool bClearHitCache = true;
	if (bClearHitCache && !bAsync)
	{
		memset(last_hits, 0, sizeof(last_hits));
		bClearHitCache = false;
	}

	if (hitInfo.bUseCache && !bAsync)
	{
		Vec3 vOut;
		// Check for cached hits.
		for (int i = 0; i < MAX_CACHED_HITS; i++)
		{
			if (last_hits[i].pRenderMesh == rIntersectionRMData.pRenderMesh)
			{
				// If testing same render mesh, check if we hit the same triangle again.
				if (Intersect::Ray_Triangle(hitInfo.inRay, last_hits[i].tri[0], last_hits[i].tri[2], last_hits[i].tri[1], vOut))
				{
					if (fMaxDist2)
					{
						float fDistance2 = hitInfo.inReferencePoint.GetSquaredDistance(vOut);
						if (fDistance2 > fMaxDist2)
							continue; // Ignore hits that are too far.
					}

					// Cached hit.
					hitInfo.vHitPos = vOut;
					hitInfo.vHitNormal = last_hits[i].hitInfo.vHitNormal;
					hitInfo.nHitMatID = last_hits[i].hitInfo.nHitMatID;
					hitInfo.nHitSurfaceID = last_hits[i].hitInfo.nHitSurfaceID;

					if (hitInfo.inRetTriangle)
					{
						hitInfo.vTri0 = last_hits[i].tri[0];
						hitInfo.vTri1 = last_hits[i].tri[1];
						hitInfo.vTri2 = last_hits[i].tri[2];
					}
					//CTimeValue t1 = gEnv->pTimer->GetAsyncTime();
					//CryLogAlways( "TestTime :%.2f", (t1-t0).GetMilliSeconds() );
					//static int nCount = 0; CryLogAlways( "Cached Hit %d",++nCount );
					hitInfo.pRenderMesh = rIntersectionRMData.pRenderMesh;
					rIntersectionRMData.bResult = true;
					return true;
				}
			}
		}
	}

	uint nVerts = rIntersectionRMData.nVerts;
	int nInds = rIntersectionRMData.nInds;

	assert(nInds != 0 && nVerts != 0);

	// get position offset and stride
	const int nPosStride = rIntersectionRMData.nPosStride;
	uint8* const pPos = rIntersectionRMData.pPos;

	// get indices
	vtx_idx* const pInds = rIntersectionRMData.pInds;

	assert(pInds != NULL && pPos != NULL);
	assert(nInds % 3 == 0);

	float fMinDistance2 = FLT_MAX;

	Ray inRay = hitInfo.inRay;

	bool bAnyHit = false;

	Vec3 vOut;
	Vec3 tri[3];

	// test tris
	TRenderChunkArray& RESTRICT_REFERENCE Chunks = rIntersectionRMData.pRenderMesh->GetChunks();
	int nChunkCount = Chunks.size();

	for (int nChunkId = 0; nChunkId < nChunkCount; nChunkId++)
	{
		CRenderChunk* pChunk = &Chunks[nChunkId];

		IF (pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE, 0)
			continue;
		const int16 nChunkMatID = pChunk->m_nMatID;

		bool b2Sided = false;

		if (pMtl)
		{
			const SShaderItem& shaderItem = pMtl->GetShaderItem(nChunkMatID);
			if (hitInfo.bOnlyZWrite && !shaderItem.IsZWrite())
				continue;
			if (!shaderItem.m_pShader || shaderItem.m_pShader->GetFlags() & EF_NODRAW || shaderItem.m_pShader->GetFlags() & EF_DECAL)
				continue;
			if (shaderItem.m_pShader->GetCull() & eCULL_None)
				b2Sided = true;
			if (shaderItem.m_pShaderResources && shaderItem.m_pShaderResources->GetResFlags() & MTL_FLAG_2SIDED)
				b2Sided = true;
		}

		int nLastIndexId = pChunk->nFirstIndexId + pChunk->nNumIndices;
		const vtx_idx* __restrict pIndices = rIntersectionRMData.pInds;

		IF (nLastIndexId - 1 >= nInds, 0)
		{
			Error("%s (%s): invalid mesh chunk", __FUNCTION__, rIntersectionRMData.pRenderMesh->GetSourceName());
			rIntersectionRMData.bResult = false;
			return 0;
		}

		// make line triangle intersection
		int i = pChunk->nFirstIndexId;

		while (i < nLastIndexId)
		{
			int p = nLastIndexId;

			for (; i < p; i += 3)//process all prefetched vertices
			{
				IF (pInds[i + 2] >= nVerts, 0)
				{
					Error("%s (%s): invalid mesh indices", __FUNCTION__, rIntersectionRMData.pRenderMesh->GetSourceName());
					rIntersectionRMData.bResult = false;
					return 0;
				}

				// get tri vertices
				const Vec3& tv0 = *(const Vec3*)&pPos[nPosStride * pIndices[i + 0]];
				const Vec3& tv1 = *(const Vec3*)&pPos[nPosStride * pIndices[i + 1]];
				const Vec3& tv2 = *(const Vec3*)&pPos[nPosStride * pIndices[i + 2]];

				if (Intersect::Ray_Triangle(inRay, tv0, tv2, tv1, vOut))
				{
					float fDistance2 = hitInfo.inReferencePoint.GetSquaredDistance(vOut);
					if (fMaxDist2)
					{
						if (fDistance2 > fMaxDist2)
							continue; // Ignore hits that are too far.
					}
					bAnyHit = true;
					// Test front.
					if (hitInfo.bInFirstHit)
					{
						vHitPos = vOut;
						hitInfo.nHitMatID = nChunkMatID;
						tri[0] = tv0;
						tri[1] = tv1;
						tri[2] = tv2;
						goto AnyHit;//need to break chunk loops, vertex loop and prefetched loop
					}
					if (fDistance2 < fMinDistance2)
					{
						fMinDistance2 = fDistance2;
						vHitPos = vOut;
						hitInfo.nHitMatID = nChunkMatID;
						tri[0] = tv0;
						tri[1] = tv1;
						tri[2] = tv2;
					}
				}
				else if (b2Sided)
				{
					if (Intersect::Ray_Triangle(inRay, tv0, tv1, tv2, vOut))
					{
						float fDistance2 = hitInfo.inReferencePoint.GetSquaredDistance(vOut);
						if (fMaxDist2)
						{
							if (fDistance2 > fMaxDist2)
								continue; // Ignore hits that are too far.
						}

						bAnyHit = true;
						// Test back.
						if (hitInfo.bInFirstHit)
						{
							vHitPos = vOut;
							hitInfo.nHitMatID = nChunkMatID;
							tri[0] = tv0;
							tri[1] = tv2;
							tri[2] = tv1;
							goto AnyHit;//need to break chunk loops, vertex loop and prefetched loop
						}
						if (fDistance2 < fMinDistance2)
						{
							fMinDistance2 = fDistance2;
							vHitPos = vOut;
							hitInfo.nHitMatID = nChunkMatID;
							tri[0] = tv0;
							tri[1] = tv2;
							tri[2] = tv1;
						}
					}
				}
			}
		}
	}
AnyHit:
	if (bAnyHit)
	{
		hitInfo.pRenderMesh = rIntersectionRMData.pRenderMesh;

		// return closest to the shooter
		hitInfo.fDistance = (float)sqrt_tpl(fMinDistance2);
		hitInfo.vHitNormal = (tri[1] - tri[0]).Cross(tri[2] - tri[0]).GetNormalized();
		hitInfo.vHitPos = vHitPos;
		hitInfo.nHitSurfaceID = 0;

		if (hitInfo.inRetTriangle)
		{
			hitInfo.vTri0 = tri[0];
			hitInfo.vTri1 = tri[1];
			hitInfo.vTri2 = tri[2];
		}

		if (pMtl)
		{
			pMtl = pMtl->GetSafeSubMtl(hitInfo.nHitMatID);
			if (pMtl)
				hitInfo.nHitSurfaceID = pMtl->GetSurfaceTypeId();
		}

		if (!bAsync)
		{
			//////////////////////////////////////////////////////////////////////////
			// Add to cached results.
			memmove(&last_hits[1], &last_hits[0], sizeof(last_hits) - sizeof(last_hits[0])); // Move hits to the end of array, throwing out the last one.
			last_hits[0].pRenderMesh = rIntersectionRMData.pRenderMesh;
			last_hits[0].hitInfo = hitInfo;
			memcpy(last_hits[0].tri, tri, sizeof(tri));
			//////////////////////////////////////////////////////////////////////////
		}

	}
	//CTimeValue t1 = gEnv->pTimer->GetAsyncTime();
	//CryLogAlways( "TestTime :%.2f", (t1-t0).GetMilliSeconds() );

	rIntersectionRMData.bResult = bAnyHit;
	return bAnyHit;
}

bool CRenderMeshUtils::RayIntersectionFast(IRenderMesh* pRenderMesh, SRayHitInfo& hitInfo, IMaterial* pMtl)
{
	SIntersectionData data;

	if (!data.Init(pRenderMesh, &hitInfo, pMtl))
	{
		return false;
	}

	// forward call to implementation
	return CRenderMeshUtils::RayIntersectionFastImpl(data, hitInfo, pMtl, false);
}

//////////////////////////////////////////////////////////////////////////
bool CRenderMeshUtils::RayIntersectionFastImpl(SIntersectionData& rIntersectionRMData, SRayHitInfo& hitInfo, IMaterial* pMtl, bool bAsync)
{
	float fBestDist = hitInfo.fMaxHitDistance; // squared distance works different for values less and more than 1.f

	Vec3 vHitPos(0, 0, 0);
	Vec3 vHitNormal(0, 0, 0);

	int nVerts = rIntersectionRMData.nVerts;
	int nInds = rIntersectionRMData.nInds;

	assert(nInds != 0 && nVerts != 0);

	// get position offset and stride
	int nPosStride = rIntersectionRMData.nPosStride;
	uint8* pPos = rIntersectionRMData.pPos;

	int nUVStride = rIntersectionRMData.nUVStride;
	uint8* pUV = rIntersectionRMData.pUV;

	int nColStride = rIntersectionRMData.nColStride;
	uint8* pCol = rIntersectionRMData.pCol;

	// get indices
	vtx_idx* pInds = rIntersectionRMData.pInds;

	assert(pInds != NULL && pPos != NULL);

	assert(nInds % 3 == 0);

	Ray inRay = hitInfo.inRay;

	bool bAnyHit = false;

	Vec3 vOut;
	Vec3 tri[3];

	// test tris

	Line inLine(inRay.origin, inRay.direction);

	if (!inRay.direction.IsZero() && hitInfo.nHitTriID >= 0)
	{
		if (hitInfo.nHitTriID + 2 >= nInds)
			return false;

		int I0 = pInds[hitInfo.nHitTriID + 0];
		int I1 = pInds[hitInfo.nHitTriID + 1];
		int I2 = pInds[hitInfo.nHitTriID + 2];

		if (I0 < nVerts && I1 < nVerts && I2 < nVerts)
		{
			// get tri vertices
			Vec3& tv0 = *((Vec3*)&pPos[nPosStride * I0]);
			Vec3& tv1 = *((Vec3*)&pPos[nPosStride * I1]);
			Vec3& tv2 = *((Vec3*)&pPos[nPosStride * I2]);

			if (Intersect::Line_Triangle(inLine, tv0, tv2, tv1, vOut))  // || Intersect::Line_Triangle( inLine, tv0, tv1, tv2, vOut ))
			{
				float fDistance = (hitInfo.inReferencePoint - vOut).GetLengthFast();

				if (fDistance < fBestDist)
				{
					bAnyHit = true;
					fBestDist = fDistance;
					vHitPos = vOut;
					tri[0] = tv0;
					tri[1] = tv1;
					tri[2] = tv2;
				}
			}
		}
	}

	if (hitInfo.nHitTriID == HIT_UNKNOWN)
	{
		if (inRay.direction.IsZero())
		{
			ProcessBoxIntersection(inRay, hitInfo, rIntersectionRMData, pMtl, pInds, nVerts, pPos, nPosStride, pUV, nUVStride, pCol, nColStride, nInds, bAnyHit, fBestDist, vHitPos, tri);
		}
		else
		{
			if (const PodArray<std::pair<int, int>>* pTris = rIntersectionRMData.pRenderMesh->GetTrisForPosition(inRay.origin + inRay.direction * 0.5f, pMtl))
			{
				for (int nId = 0; nId < pTris->Count(); ++nId)
				{
					std::pair<int, int>& t = pTris->GetAt(nId);

					if (t.first + 2 >= nInds)
						return false;

					int I0 = pInds[t.first + 0];
					int I1 = pInds[t.first + 1];
					int I2 = pInds[t.first + 2];

					if (I0 >= nVerts || I1 >= nVerts || I2 >= nVerts)
						return false;

					// get tri vertices
					Vec3& tv0 = *((Vec3*)&pPos[nPosStride * I0]);
					Vec3& tv1 = *((Vec3*)&pPos[nPosStride * I1]);
					Vec3& tv2 = *((Vec3*)&pPos[nPosStride * I2]);

					if (Intersect::Line_Triangle(inLine, tv0, tv2, tv1, vOut))  // || Intersect::Line_Triangle( inLine, tv0, tv1, tv2, vOut ))
					{
						float fDistance = (hitInfo.inReferencePoint - vOut).GetLengthFast();

						if (fDistance < fBestDist)
						{
							bAnyHit = true;
							fBestDist = fDistance;
							vHitPos = vOut;
							tri[0] = tv0;
							tri[1] = tv1;
							tri[2] = tv2;
							hitInfo.nHitMatID = t.second;
							hitInfo.nHitTriID = t.first;
						}
					}
				}
			}
		}
	}

	if (bAnyHit)
	{
		hitInfo.pRenderMesh = rIntersectionRMData.pRenderMesh;

		// return closest to the shooter
		hitInfo.fDistance = fBestDist;
		hitInfo.vHitNormal = (tri[1] - tri[0]).Cross(tri[2] - tri[0]).GetNormalized();
		hitInfo.vHitPos = vHitPos;
		hitInfo.nHitSurfaceID = 0;

		if (pMtl)
		{
			pMtl = pMtl->GetSafeSubMtl(hitInfo.nHitMatID);
			if (pMtl)
				hitInfo.nHitSurfaceID = pMtl->GetSurfaceTypeId();
		}

		if (hitInfo.bGetVertColorAndTC && hitInfo.nHitTriID >= 0 && !inRay.direction.IsZero())
		{
			int I0 = pInds[hitInfo.nHitTriID + 0];
			int I1 = pInds[hitInfo.nHitTriID + 1];
			int I2 = pInds[hitInfo.nHitTriID + 2];

			// get tri vertices
			Vec3& tv0 = *((Vec3*)&pPos[nPosStride * I0]);
			Vec3& tv1 = *((Vec3*)&pPos[nPosStride * I1]);
			Vec3& tv2 = *((Vec3*)&pPos[nPosStride * I2]);

			float u = 0, v = 0, w = 0;
			if (GetBarycentricCoordinates(vHitPos - tv0, tv1 - tv0, tv2 - tv0, u, v, w, 16.0f))
			{
				float arrVertWeight[3] = { max(0.f, w), max(0.f, v), max(0.f, u) };
				float fDiv = 1.f / (arrVertWeight[0] + arrVertWeight[1] + arrVertWeight[2]);
				arrVertWeight[0] *= fDiv;
				arrVertWeight[1] *= fDiv;
				arrVertWeight[2] *= fDiv;

				Vec2 tc0 = ((Vec2f16*)&pUV[nUVStride * I0])->ToVec2();
				Vec2 tc1 = ((Vec2f16*)&pUV[nUVStride * I1])->ToVec2();
				Vec2 tc2 = ((Vec2f16*)&pUV[nUVStride * I2])->ToVec2();

				hitInfo.vHitTC = tc0 * arrVertWeight[0] + tc1 * arrVertWeight[1] + tc2 * arrVertWeight[2];

				Vec4 c0 = (*(ColorB*)&pCol[nColStride * I0]).toVec4();
				Vec4 c1 = (*(ColorB*)&pCol[nColStride * I1]).toVec4();
				Vec4 c2 = (*(ColorB*)&pCol[nColStride * I2]).toVec4();

				// get tangent basis
				int nTangsStride = rIntersectionRMData.nTangsStride;
				byte* pTangs = rIntersectionRMData.pTangs;

				Vec4 tangent[3];
				Vec4 bitangent[3];
				int arrId[3] = { I0, I1, I2 };
				for (int ii = 0; ii < 3; ii++)
				{
					SPipTangents tb = *(SPipTangents*)&pTangs[nTangsStride * arrId[ii]];

					tb.GetTB(tangent[ii], bitangent[ii]);
				}

				hitInfo.vHitTangent = (tangent[0] * arrVertWeight[0] + tangent[1] * arrVertWeight[1] + tangent[2] * arrVertWeight[2]);
				hitInfo.vHitBitangent = (bitangent[0] * arrVertWeight[0] + bitangent[1] * arrVertWeight[1] + bitangent[2] * arrVertWeight[2]);
				hitInfo.vHitColor = (c0 * arrVertWeight[0] + c1 * arrVertWeight[1] + c2 * arrVertWeight[2]) / 255.f;
			}
		}
	}

	//CTimeValue t1 = gEnv->pTimer->GetAsyncTime();
	//CryLogAlways( "TestTime :%.2f", (t1-t0).GetMilliSeconds() );

	return bAnyHit;
}

// used for CPU voxelization
bool CRenderMeshUtils::ProcessBoxIntersection(Ray& inRay, SRayHitInfo& hitInfo, SIntersectionData& rIntersectionRMData, IMaterial* pMtl, vtx_idx* pInds, int nVerts, uint8* pPos, int nPosStride, uint8* pUV, int nUVStride, uint8* pCol, int nColStride, int nInds, bool& bAnyHit, float& fBestDist, Vec3& vHitPos, Vec3* tri)
{
	AABB voxBox;
	voxBox.min = inRay.origin - Vec3(hitInfo.fMaxHitDistance);
	voxBox.max = inRay.origin + Vec3(hitInfo.fMaxHitDistance);

	if (hitInfo.pHitTris)
	{
		// just collect tris
		TRenderChunkArray& Chunks = rIntersectionRMData.pRenderMesh->GetChunks();
		int nChunkCount = Chunks.size();

		for (int nChunkId = 0; nChunkId < nChunkCount; nChunkId++)
		{
			CRenderChunk* pChunk(&Chunks[nChunkId]);

			IF (pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE, 0)
				continue;

			const int16 nChunkMatID = pChunk->m_nMatID;

			bool b2Sided = false;

			const SShaderItem& shaderItem = pMtl->GetShaderItem(nChunkMatID);
			//					if (!shaderItem.IsZWrite())
			//					continue;
			IShader* pShader = shaderItem.m_pShader;
			if (!pShader || pShader->GetFlags() & EF_NODRAW || pShader->GetFlags() & EF_DECAL || (pShader->GetShaderType() != eST_General && pShader->GetShaderType() != eST_Vegetation))
				continue;
			if (pShader->GetCull() & eCULL_None)
				b2Sided = true;
			if (shaderItem.m_pShaderResources && shaderItem.m_pShaderResources->GetResFlags() & MTL_FLAG_2SIDED)
				b2Sided = true;

			float fOpacity = shaderItem.m_pShaderResources->GetStrengthValue(EFTT_OPACITY) * shaderItem.m_pShaderResources->GetVoxelCoverage();
			if (fOpacity < hitInfo.fMinHitOpacity)
				continue;

			//          ColorB colEm = shaderItem.m_pShaderResources->GetEmissiveColor();
			//          if(!colEm.r && !colEm.g && !colEm.b)
			//            colEm = Col_DarkGray;

			// make line triangle intersection
			for (uint ii = pChunk->nFirstIndexId; ii < pChunk->nFirstIndexId + pChunk->nNumIndices; ii += 3)
			{
				int I0 = pInds[ii + 0];
				int I1 = pInds[ii + 1];
				int I2 = pInds[ii + 2];

				if (I0 >= nVerts || I1 >= nVerts || I2 >= nVerts)
					return false;

				// get tri vertices
				Vec3& tv0 = *((Vec3*)&pPos[nPosStride * I0]);
				Vec3& tv1 = *((Vec3*)&pPos[nPosStride * I1]);
				Vec3& tv2 = *((Vec3*)&pPos[nPosStride * I2]);

				if (Overlap::AABB_Triangle(voxBox, tv0, tv2, tv1))
				{
					AABB triBox(tv0, tv0);
					triBox.Add(tv2);
					triBox.Add(tv1);

					if (triBox.GetRadiusSqr() > 0.00001f)
					{
						SRayHitTriangle ht;
						ht.v[0] = tv0;
						ht.v[1] = tv1;
						ht.v[2] = tv2;

						ht.t[0] = *((Vec2*)&pUV[nUVStride * I0]);
						ht.t[1] = *((Vec2*)&pUV[nUVStride * I1]);
						ht.t[2] = *((Vec2*)&pUV[nUVStride * I2]);

						ht.pMat = pMtl->GetSafeSubMtl(nChunkMatID);

						ht.c[0] = (*(ColorB*)&pCol[nColStride * I0]);
						ht.c[1] = (*(ColorB*)&pCol[nColStride * I1]);
						ht.c[2] = (*(ColorB*)&pCol[nColStride * I2]);

						ht.nOpacity = SATURATEB(int(fOpacity * 255.f));
						hitInfo.pHitTris->Add(ht);
					}
				}
			}
		}
	}
	else if (const PodArray<std::pair<int, int>>* pTris = rIntersectionRMData.pRenderMesh->GetTrisForPosition(inRay.origin, pMtl))
	{
		for (int nId = 0; nId < pTris->Count(); ++nId)
		{
			std::pair<int, int>& t = pTris->GetAt(nId);

			if (t.first + 2 >= nInds)
				return false;

			int I0 = pInds[t.first + 0];
			int I1 = pInds[t.first + 1];
			int I2 = pInds[t.first + 2];

			if (I0 >= nVerts || I1 >= nVerts || I2 >= nVerts)
				return false;

			// get tri vertices
			Vec3& tv0 = *((Vec3*)&pPos[nPosStride * I0]);
			Vec3& tv1 = *((Vec3*)&pPos[nPosStride * I1]);
			Vec3& tv2 = *((Vec3*)&pPos[nPosStride * I2]);

			if (Overlap::AABB_Triangle(voxBox, tv0, tv2, tv1))
			{
				{
					IMaterial* pSubMtl = pMtl->GetSafeSubMtl(t.second);
					if (pSubMtl)
					{
						if (!pSubMtl->GetShaderItem().IsZWrite())
							continue;
						if (!pSubMtl->GetShaderItem().m_pShader)
							continue;
						if (pSubMtl->GetShaderItem().m_pShader->GetShaderType() != eST_Metal && pSubMtl->GetShaderItem().m_pShader->GetShaderType() != eST_General)
							continue;
					}
				}

				bAnyHit = true;
				fBestDist = 0;
				vHitPos = voxBox.GetCenter();
				tri[0] = tv0;
				tri[1] = tv1;
				tri[2] = tv2;
				hitInfo.nHitMatID = t.second;
				hitInfo.nHitTriID = t.first;

				break;
			}
		}
	}

	return bAnyHit;
}
