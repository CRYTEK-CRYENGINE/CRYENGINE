// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   statobjmandraw.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Render all entities in the sector together with shadows
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <CryAnimation/ICryAnimation.h>

#include "terrain.h"
#include "StatObj.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "terrain_sector.h"
#include "3dEngine.h"
#include "PolygonClipContext.h"
#include "3dEngine.h"
#include "LightEntity.h"
#include "DecalManager.h"
#include "ObjectsTree.h"
#include "Brush.h"
#include "ClipVolumeManager.h"

void CObjManager::RenderDecalAndRoad(IRenderNode* pEnt, PodArray<SRenderLight*>* pAffectingLights,
                                     const Vec3& vAmbColor, const AABB& objBox,
                                     float fEntDistance,
                                     bool nCheckOcclusion,
                                     const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

#ifdef _DEBUG
	const char* szName = pEnt->GetName();
	const char* szClassName = pEnt->GetEntityClassName();
#endif // _DEBUG

	// do not draw if marked to be not drawn or already drawn in this frame
	auto nRndFlags = pEnt->GetRndFlags();

	if (nRndFlags & ERF_HIDDEN)
		return;

	EERType eERType = pEnt->GetRenderNodeType();

	// detect bad objects
	float fEntLengthSquared = objBox.GetSize().GetLengthSquared();
	if (eERType != eERType_Light || !_finite(fEntLengthSquared))
	{
		if (fEntLengthSquared > MAX_VALID_OBJECT_VOLUME || !_finite(fEntLengthSquared) || fEntLengthSquared <= 0)
		{
			Warning("CObjManager::RenderObject: Object has invalid bbox: %s,%s, Radius = %.2f, Center = (%.1f,%.1f,%.1f)",
			        pEnt->GetName(), pEnt->GetEntityClassName(), sqrt_tpl(fEntLengthSquared) * 0.5f,
			        pEnt->GetBBox().GetCenter().x, pEnt->GetBBox().GetCenter().y, pEnt->GetBBox().GetCenter().z);
			return; // skip invalid objects - usually only objects with invalid very big scale will reach this point
		}
	}
	else
		pAffectingLights = NULL;

	// allocate RNTmpData for potentially visible objects
	SRenderNodeTempData* pTempData = Get3DEngine()->CheckAndCreateRenderNodeTempData(pEnt, passInfo);
	if (!pTempData)
		return;

	if (nCheckOcclusion && pEnt->m_pOcNode)
		if (GetObjManager()->IsBoxOccluded(objBox, fEntDistance * passInfo.GetInverseZoomFactor(), &pTempData->userData.m_OcclState,
		                                   pEnt->m_pOcNode->GetVisArea() != NULL, eoot_OBJECT, passInfo))
			return;

	CVisArea* pVisArea = (CVisArea*)pEnt->GetEntityVisArea();

	const Vec3& vCamPos = passInfo.GetCamera().GetPosition();

	// test only near/big occluders - others will be tested on tree nodes level
	if (!objBox.IsContainPoint(vCamPos))
		if (eERType == eERType_Light || fEntDistance < pEnt->m_fWSMaxViewDist * GetCVars()->e_OcclusionCullingViewDistRatio)
			if (IsBoxOccluded(objBox, fEntDistance * passInfo.GetInverseZoomFactor(), &pTempData->userData.m_OcclState, pVisArea != NULL, eoot_OBJECT, passInfo))
				return;

	SRendParams DrawParams;
	DrawParams.pTerrainTexInfo = NULL;
	DrawParams.dwFObjFlags = 0;
	DrawParams.fDistance = fEntDistance;
	DrawParams.AmbientColor = vAmbColor;
	DrawParams.pRenderNode = pEnt;
	DrawParams.nAfterWater = IsAfterWater(objBox.GetCenter(), vCamPos, passInfo) ? 1 : 0;

	// draw bbox
	if (GetCVars()->e_BBoxes)// && eERType != eERType_Light)
	{
		RenderObjectDebugInfo(pEnt, fEntDistance, passInfo);
	}

	DrawParams.dwFObjFlags |= FOB_TRANS_MASK;

	DrawParams.m_pVisArea = pVisArea;

	DrawParams.nMaterialLayers = pEnt->GetMaterialLayers();

	pEnt->Render(DrawParams, passInfo);
}

void CObjManager::RenderVegetation(CVegetation* pEnt, PodArray<SRenderLight*>* pAffectingLights,
                                   const AABB& objBox,
                                   float fEntDistance,
                                   SSectorTextureSet* pTerrainTexInfo, bool nCheckOcclusion,
                                   const SRenderingPassInfo& passInfo,
                                   uint32 passCullMask)
{
	FUNCTION_PROFILER_3DENGINE;

#ifdef _DEBUG
	const char* szName = pEnt->GetName();
	const char* szClassName = pEnt->GetEntityClassName();
#endif // _DEBUG

	// check cvars
	assert(passInfo.RenderVegetation());

	// check-allocate RNTmpData for visible objects
	SRenderNodeTempData* pTempData = Get3DEngine()->CheckAndCreateRenderNodeTempData(pEnt, passInfo);
	if (!pTempData)
		return;

	if (passCullMask & kPassCullMainMask && nCheckOcclusion && pEnt->m_pOcNode)
	{
		if (GetObjManager()->IsBoxOccluded(objBox, fEntDistance * passInfo.GetInverseZoomFactor(), &pTempData->userData.m_OcclState, pEnt->m_pOcNode->GetVisArea() != NULL, eoot_OBJECT, passInfo))
		{
			passCullMask &= ~kPassCullMainMask;
		}
	}

	const CLodValue lodValue = pEnt->ComputeLod(pTempData->userData.nWantedLod, passInfo);

	if (passCullMask & kPassCullMainMask)
	{
		if (GetCVars()->e_LodTransitionTime && passInfo.IsGeneralPass())
		{
			// Render current lod and (if needed) previous lod and perform time based lod transition using dissolve

			CLodValue arrlodVals[2];
			int nLodsNum = ComputeDissolve(lodValue, pTempData, pEnt, fEntDistance, &arrlodVals[0]);

			for (int i = 0; i < nLodsNum; i++)
				pEnt->Render(passInfo, arrlodVals[i], pTerrainTexInfo);
		}
		else
		{
			pEnt->Render(passInfo, lodValue, pTerrainTexInfo);
		}
	}

	if (passCullMask & ~kPassCullMainMask)
	{
		COctreeNode::RenderObjectIntoShadowViews(passInfo, fEntDistance, pEnt, objBox, passCullMask);
	}
}

void CObjManager::RenderObject(IRenderNode* pEnt, PodArray<SRenderLight*>* pAffectingLights,
                               const Vec3& vAmbColor, const AABB& objBox,
                               float fEntDistance,
                               EERType eERType,
                               const SRenderingPassInfo& passInfo,
                               uint32 passCullMask)
{
	FUNCTION_PROFILER_3DENGINE;

	const CVars* pCVars = GetCVars();

#ifdef _DEBUG
	const char* szName = pEnt->GetName();
	const char* szClassName = pEnt->GetEntityClassName();
#endif // _DEBUG

	// do not draw if marked to be not drawn or already drawn in this frame
	auto nRndFlags = pEnt->GetRndFlags();

	if (nRndFlags & ERF_HIDDEN)
		return;

	CRY_ASSERT(eERType != eERType_MovableBrush);

#ifndef _RELEASE
	if (!passInfo.RenderEntities() && pEnt->GetOwnerEntity())
		return;
	// check cvars
	switch (eERType)
	{
	case eERType_Brush:
		if (!passInfo.RenderBrushes()) return;
		break;
	case eERType_Vegetation:
		assert(0);
		break;
	case eERType_ParticleEmitter:
		if (!passInfo.RenderParticles()) return;
		break;
	case eERType_Decal:
		if (!passInfo.RenderDecals()) return;
		break;
	case eERType_WaterWave:
		if (!passInfo.RenderWaterWaves()) return;
		break;
	case eERType_WaterVolume:
		if (!passInfo.RenderWaterVolumes()) return;
		break;
	case eERType_Light:
		if (!GetCVars()->e_DynamicLights || !passInfo.RenderEntities()) return;
		break;
	case eERType_Road:
		if (!passInfo.RenderRoads()) return;
		break;
	case eERType_DistanceCloud:
	case eERType_CloudBlocker:
		if (!passInfo.RenderClouds()) return;
		break;
	case eERType_MergedMesh:
		if (!passInfo.RenderMergedMeshes()) return;
		break;
	default:
		if (!passInfo.RenderEntities()) return;
		break;
	}

	// detect bad objects
	float fEntLengthSquared = objBox.GetSize().GetLengthSquared();
	if (eERType != eERType_Light || !_finite(fEntLengthSquared))
	{
		if (fEntLengthSquared > MAX_VALID_OBJECT_VOLUME || !_finite(fEntLengthSquared) || fEntLengthSquared <= 0)
		{
			Warning("CObjManager::RenderObject: Object has invalid bbox: %s, %s, Radius = %.2f, Center = (%.1f,%.1f,%.1f)",
			        pEnt->GetName(), pEnt->GetEntityClassName(), sqrt_tpl(fEntLengthSquared) * 0.5f,
			        pEnt->GetBBox().GetCenter().x, pEnt->GetBBox().GetCenter().y, pEnt->GetBBox().GetCenter().z);
			return; // skip invalid objects - usually only objects with invalid very big scale will reach this point
		}
	}
	else
		pAffectingLights = NULL;
#endif

	if (pEnt->m_dwRndFlags & ERF_COLLISION_PROXY || pEnt->m_dwRndFlags & ERF_RAYCAST_PROXY)
	{
		// Collision proxy is visible in Editor while in editing mode.
		if (!gEnv->IsEditor() || !gEnv->IsEditing())
		{
			if (GetCVars()->e_DebugDraw == 0)
				return; //true;
		}
	}

	// allocate RNTmpData for potentially visible objects
	SRenderNodeTempData* pTempData = Get3DEngine()->CheckAndCreateRenderNodeTempData(pEnt, passInfo);
	if (!pTempData)
		return;

	PrefetchLine(pTempData, 0); //m_pRNTmpData is >128 bytes, prefetching data used in dissolveref here

#if CRY_PLATFORM_DESKTOP
	// detect already culled occluder
	if ((nRndFlags & ERF_GOOD_OCCLUDER))
	{
		if (pTempData->userData.m_OcclState.nLastOccludedMainFrameID == passInfo.GetMainFrameID())
			passCullMask &= ~kPassCullMainMask;

		if (pCVars->e_CoverageBufferDrawOccluders)
		{
			passCullMask &= ~kPassCullMainMask;
		}
	}
#endif

	CVisArea* pVisArea = (CVisArea*)pEnt->GetEntityVisArea();

	const Vec3& vCamPos = passInfo.GetCamera().GetPosition();

	// test only near/big occluders - others will be tested on tree nodes level
	// Note: Not worth prefetch on rCam or objBox as both have been recently used by calling functions & will be in cache - Rich S
	if (!(nRndFlags & ERF_RENDER_ALWAYS) && !objBox.IsContainPoint(vCamPos))
		if (eERType == eERType_Light || fEntDistance < pEnt->m_fWSMaxViewDist * pCVars->e_OcclusionCullingViewDistRatio)
			if (IsBoxOccluded(objBox, fEntDistance * passInfo.GetInverseZoomFactor(), &pTempData->userData.m_OcclState, pVisArea != NULL, eoot_OBJECT, passInfo))
				passCullMask &= ~kPassCullMainMask;

	SRendParams DrawParams;
	DrawParams.pTerrainTexInfo = NULL;
	DrawParams.dwFObjFlags = 0;
	DrawParams.fDistance = fEntDistance;
	DrawParams.AmbientColor = vAmbColor;
	DrawParams.pRenderNode = pEnt;
	DrawParams.nEditorSelectionID = pEnt->m_nEditorSelectionID;
	//DrawParams.pInstance = pEnt;

	if (passCullMask & kPassCullMainMask && eERType != eERType_Light && (pEnt->m_nInternalFlags & IRenderNode::REQUIRES_NEAREST_CUBEMAP))
	{
		Vec4 envProbMults = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
		uint16 nCubemapTexId = 0;
		if (!(nCubemapTexId = CheckCachedNearestCubeProbe(pEnt, &envProbMults)) || !pCVars->e_CacheNearestCubePicking)
			nCubemapTexId = GetNearestCubeProbe(pAffectingLights, pVisArea, objBox, true, &envProbMults);

		pTempData->userData.nCubeMapId = nCubemapTexId;
		pTempData->userData.vEnvironmentProbeMults = envProbMults;

		DrawParams.nTextureID = nCubemapTexId;
	}

	DrawParams.nAfterWater = IsAfterWater(objBox.GetCenter(), vCamPos, passInfo) ? 1 : 0;

	if (nRndFlags & ERF_SELECTED)
		DrawParams.dwFObjFlags |= FOB_SELECTED;

	// draw bbox
#if !defined(_RELEASE)
	if (pCVars->e_BBoxes)// && eERType != eERType_Light)
	{
		RenderObjectDebugInfo(pEnt, fEntDistance, passInfo);
	}
#endif

	DrawParams.dwFObjFlags |= FOB_TRANS_MASK;

	if (pEnt->m_dwRndFlags & ERF_NO_DECALNODE_DECALS)
		DrawParams.dwFObjFlags |= FOB_DYNAMIC_OBJECT;

	if (pEnt->GetRndFlags() & ERF_HUD_REQUIRE_DEPTHTEST)
	{
		DrawParams.nCustomFlags |= COB_HUD_REQUIRE_DEPTHTEST;
		DrawParams.dwFObjFlags |= FOB_HUD_REQUIRE_DEPTHTEST;
	}
	if (pEnt->GetRndFlags() & ERF_DISABLE_MOTION_BLUR)
	{
		DrawParams.nCustomFlags |= COB_DISABLE_MOTIONBLUR;
	}
	if (pEnt->GetRndFlags() & ERF_FORCE_POST_3D_RENDER)
	{
		DrawParams.nCustomFlags |= COB_POST_3D_RENDER;
	}

	DrawParams.m_pVisArea = pVisArea;

	// Update clip volume
	Vec3 vEntCenter = Get3DEngine()->GetEntityRegisterPoint(pEnt);
	if (pVisArea)
		pTempData->userData.m_pClipVolume = pVisArea;
	else if (Get3DEngine()->GetClipVolumeManager()->IsClipVolumeRequired(pEnt))
		Get3DEngine()->GetClipVolumeManager()->UpdateEntityClipVolume(vEntCenter, pEnt);

	DrawParams.nClipVolumeStencilRef = 0;
	if (pTempData->userData.m_pClipVolume)
		DrawParams.nClipVolumeStencilRef = pTempData->userData.m_pClipVolume->GetStencilRef();

	DrawParams.nMaterialLayers = pEnt->GetMaterialLayers();
	DrawParams.lodValue = pEnt->ComputeLod(pTempData->userData.nWantedLod, passInfo);

	if (passCullMask & kPassCullMainMask)
	{
		if (GetCVars()->e_LodTransitionTime && passInfo.IsGeneralPass() && pEnt->GetRenderNodeType() == eERType_MovableBrush)
		{
			// Render current lod and (if needed) previous lod and perform time based lod transition using dissolve

			CLodValue arrlodVals[2];
			int nLodsNum = ComputeDissolve(DrawParams.lodValue, pTempData, pEnt, fEntDistance, &arrlodVals[0]);

			for (int i = 0; i < nLodsNum; i++)
			{
				DrawParams.lodValue = arrlodVals[i];
				pEnt->Render(DrawParams, passInfo);
			}
		}
		else
		{
			pEnt->Render(DrawParams, passInfo);
		}
	}

	if (passCullMask & ~kPassCullMainMask)
	{
		COctreeNode::RenderObjectIntoShadowViews(passInfo, fEntDistance, pEnt, objBox, passCullMask);
	}
}

void CObjManager::RenderAllObjectDebugInfo()
{
	m_arrRenderDebugInfo.CoalesceMemory();
	for (size_t i = 0; i < m_arrRenderDebugInfo.size(); ++i)
	{
		SObjManRenderDebugInfo& rRenderDebugInfo = m_arrRenderDebugInfo[i];
		if (rRenderDebugInfo.pEnt)
			RenderObjectDebugInfo_Impl(rRenderDebugInfo.pEnt, rRenderDebugInfo.fEntDistance);
	}
	m_arrRenderDebugInfo.resize(0);
}

void CObjManager::RemoveFromRenderAllObjectDebugInfo(IRenderNode* pEnt)
{
	for (size_t i = 0; i < m_arrRenderDebugInfo.size(); ++i)
	{
		SObjManRenderDebugInfo& rRenderDebugInfo = m_arrRenderDebugInfo[i];
		if (rRenderDebugInfo.pEnt == pEnt)
		{
			rRenderDebugInfo.pEnt = NULL;
			break;
		}
	}
}

void CObjManager::RenderObjectDebugInfo_Impl(IRenderNode* pEnt, float fEntDistance)
{

	if (GetCVars()->e_BBoxes > 0)
	{
		ColorF color(1, 1, 1, 1);

		if (GetCVars()->e_BBoxes == 2 && pEnt->GetRndFlags() & ERF_SELECTED)
		{
			color.a *= clamp_tpl(pEnt->GetImportance(), 0.5f, 1.f);
			float fFontSize = max(2.f - fEntDistance * 0.01f, 1.f);

			string sLabel = pEnt->GetDebugString();
			if (sLabel.empty())
			{
				sLabel.Format("%s/%s", pEnt->GetName(), pEnt->GetEntityClassName());
			}
			IRenderAuxText::DrawLabelEx(pEnt->GetBBox().GetCenter(), fFontSize, (float*)&color, true, true, sLabel.c_str());
		}

		IRenderAuxGeom* pRenAux = GetRenderer()->GetIRenderAuxGeom();
		pRenAux->SetRenderFlags(SAuxGeomRenderFlags());
		AABB rAABB = pEnt->GetBBox();
		const float Bias = GetCVars()->e_CoverageBufferAABBExpand;
		if (Bias < 0.f)
			rAABB.Expand((rAABB.max - rAABB.min) * Bias - Vec3(Bias, Bias, Bias));
		else
			rAABB.Expand(Vec3(Bias, Bias, Bias));

		pRenAux->DrawAABB(rAABB, false, color, eBBD_Faceted);
	}
}

bool CObjManager::RayRenderMeshIntersection(IRenderMesh* pRenderMesh, const Vec3& vInPos, const Vec3& vInDir, Vec3& vOutPos, Vec3& vOutNormal, bool bFastTest, IMaterial* pMat)
{
	FUNCTION_PROFILER_3DENGINE;

	struct MeshLock
	{
		MeshLock(IRenderMesh* m = 0) : mesh(m)
		{
			if (m) m->LockForThreadAccess();
		}
		~MeshLock()
		{
			if (mesh)
			{
				mesh->UnlockStream(VSF_GENERAL);
				mesh->UnlockIndexStream();
				mesh->UnLockForThreadAccess();
			}
		}
	private:
		IRenderMesh* mesh;
	};

	MeshLock rmLock(pRenderMesh);

	// get position offset and stride
	int nPosStride = 0;
	byte* pPos = pRenderMesh->GetPosPtr(nPosStride, FSL_READ);

	// get indices
	vtx_idx* pInds = pRenderMesh->GetIndexPtr(FSL_READ);
	int nInds = pRenderMesh->GetIndicesCount();
	assert(nInds % 3 == 0);

	float fClosestHitDistance = -1;

	Lineseg l0(vInPos + vInDir, vInPos - vInDir);
	Lineseg l1(vInPos - vInDir, vInPos + vInDir);

	Vec3 vHitPoint(0, 0, 0);

	//	bool b2DTest = fabs(vInDir.x)<0.001f && fabs(vInDir.y)<0.001f;

	// test tris
	TRenderChunkArray& Chunks = pRenderMesh->GetChunks();
	for (int nChunkId = 0; nChunkId < Chunks.size(); nChunkId++)
	{
		CRenderChunk* pChunk = &Chunks[nChunkId];
		if (pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE)
			continue;

		if (pMat)
		{
			const SShaderItem& shaderItem = pMat->GetShaderItem(pChunk->m_nMatID);
			if (!shaderItem.m_pShader || shaderItem.m_pShader->GetFlags() & EF_NODRAW)
				continue;
		}

		AABB triBox;

		int nLastIndexId = pChunk->nFirstIndexId + pChunk->nNumIndices;
		for (int i = pChunk->nFirstIndexId; i < nLastIndexId; i += 3)
		{
			assert((int)pInds[i + 0] < pRenderMesh->GetVerticesCount());
			assert((int)pInds[i + 1] < pRenderMesh->GetVerticesCount());
			assert((int)pInds[i + 2] < pRenderMesh->GetVerticesCount());

			// get vertices
			const Vec3 v0 = (*(Vec3*)&pPos[nPosStride * pInds[i + 0]]);
			const Vec3 v1 = (*(Vec3*)&pPos[nPosStride * pInds[i + 1]]);
			const Vec3 v2 = (*(Vec3*)&pPos[nPosStride * pInds[i + 2]]);
			/*
			      if(b2DTest)
			      {
			        triBox.min = triBox.max = v0;
			        triBox.Add(v1);
			        triBox.Add(v2);
			        if(	vInPos.x < triBox.min.x || vInPos.x > triBox.max.x || vInPos.y < triBox.min.y || vInPos.y > triBox.max.y )
			          continue;
			      }
			 */
			// make line triangle intersection
			if (Intersect::Lineseg_Triangle(l0, v0, v1, v2, vHitPoint) ||
			    Intersect::Lineseg_Triangle(l1, v0, v1, v2, vHitPoint))
			{
				if (bFastTest)
					return (true);

				float fDist = vHitPoint.GetDistance(vInPos);
				if (fDist < fClosestHitDistance || fClosestHitDistance < 0)
				{
					fClosestHitDistance = fDist;
					vOutPos = vHitPoint;
					vOutNormal = (v1 - v0).Cross(v2 - v0);
				}
			}
		}
	}

	if (fClosestHitDistance >= 0)
	{
		vOutNormal.Normalize();
		return true;
	}

	return false;
}

bool CObjManager::RayStatObjIntersection(IStatObj* pStatObj, const Matrix34& objMatrix, IMaterial* pMat,
                                         Vec3 vStart, Vec3 vEnd, Vec3& vClosestHitPoint, float& fClosestHitDistance, bool bFastTest)
{
	assert(pStatObj);

	CStatObj* pCStatObj = (CStatObj*)pStatObj;

	if (!pCStatObj || pCStatObj->m_nFlags & STATIC_OBJECT_HIDDEN)
		return false;

	Matrix34 matInv = objMatrix.GetInverted();
	Vec3 vOSStart = matInv.TransformPoint(vStart);
	Vec3 vOSEnd = matInv.TransformPoint(vEnd);
	Vec3 vOSHitPoint(0, 0, 0), vOSHitNorm(0, 0, 0);

	Vec3 vBoxHitPoint;
	if (!Intersect::Ray_AABB(Ray(vOSStart, vOSEnd - vOSStart), pCStatObj->GetAABB(), vBoxHitPoint))
		return false;

	bool bHitDetected = false;

	if (IRenderMesh* pRenderMesh = pStatObj->GetRenderMesh())
	{
		if (CObjManager::RayRenderMeshIntersection(pRenderMesh, vOSStart, vOSEnd - vOSStart, vOSHitPoint, vOSHitNorm, bFastTest, pMat))
		{
			bHitDetected = true;
			Vec3 vHitPoint = objMatrix.TransformPoint(vOSHitPoint);
			float fDist = vHitPoint.GetDistance(vStart);
			if (fDist < fClosestHitDistance)
			{
				fClosestHitDistance = fDist;
				vClosestHitPoint = vHitPoint;
			}
		}
	}
	else
	{
		// multi-sub-objects
		for (int s = 0, num = pCStatObj->SubObjectCount(); s < num; s++)
		{
			IStatObj::SSubObject& subObj = pCStatObj->SubObject(s);
			if (subObj.pStatObj && !subObj.bHidden && subObj.nType == STATIC_SUB_OBJECT_MESH)
			{
				Matrix34 subObjMatrix = objMatrix * subObj.tm;
				if (RayStatObjIntersection(subObj.pStatObj, subObjMatrix, pMat, vStart, vEnd, vClosestHitPoint, fClosestHitDistance, bFastTest))
					bHitDetected = true;
			}
		}
	}

	return bHitDetected;
}
