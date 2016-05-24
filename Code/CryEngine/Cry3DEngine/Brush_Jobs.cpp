// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   brush.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "StatObj.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "terrain_sector.h"
#include "PolygonClipContext.h"
#include "3dEngine.h"
#include "IndexedMesh.h"
#include "Brush.h"
#include "terrain.h"

///////////////////////////////////////////////////////////////////////////////
void CBrush::Render(const CLodValue& lodValue, const SRenderingPassInfo& passInfo, SSectorTextureSet* pTerrainTexInfo, PodArray<CDLight*>* pAffectingLights)
{
	FUNCTION_PROFILER_3DENGINE;
	CVars* pCVars = GetCVars();

	// Collision proxy is visible in Editor while in editing mode.
	if (m_dwRndFlags & ERF_COLLISION_PROXY)
	{
		if (!gEnv->IsEditor() || !gEnv->IsEditing())
			if (pCVars->e_DebugDraw == 0)
				return;
	}
	if (m_dwRndFlags & ERF_HIDDEN)
		return;

	const Vec3 vCamPos = passInfo.GetCamera().GetPosition();
	const Vec3 vObjCenter = CBrush::GetBBox().GetCenter();
	const Vec3 vObjPos = CBrush::GetPos();

	SRenderNodeTempData::SUserData& userData = m_pTempData->userData;
	CRenderObject* pObj = 0;

	if (GetObjManager()->AddOrCreatePersistentRenderObject(m_pTempData, pObj, &lodValue, passInfo))
		return;

	pObj->m_fDistance = sqrt_tpl(Distance::Point_AABBSq(vCamPos, CBrush::GetBBox())) * passInfo.GetZoomFactor();

	pObj->m_pRenderNode = this;
	pObj->m_II.m_Matrix = userData.objMat;
	pObj->m_fAlpha = 1.f;
	IF (!m_bDrawLast, 1)
		pObj->m_nSort = fastround_positive(pObj->m_fDistance * 2.0f);
	else
		pObj->m_fSort = 10000.0f;

	IMaterial* pMat = pObj->m_pCurrMaterial = CBrush::GetMaterial();
	pObj->m_ObjFlags |= FOB_INSHADOW | FOB_TRANS_MASK;

	if (!userData.objMat.m01 && !userData.objMat.m02 && !userData.objMat.m10 && !userData.objMat.m12 && !userData.objMat.m20 && !userData.objMat.m21)
		pObj->m_ObjFlags &= ~FOB_TRANS_ROTATE;
	else
		pObj->m_ObjFlags |= FOB_TRANS_ROTATE;

	if (m_dwRndFlags & ERF_NO_DECALNODE_DECALS)
		pObj->m_ObjFlags |= FOB_DYNAMIC_OBJECT;
	else
		pObj->m_ObjFlags &= ~FOB_DYNAMIC_OBJECT;

	if (uint8 nMaterialLayers = IRenderNode::GetMaterialLayers())
	{
		uint8 nFrozenLayer = (nMaterialLayers & MTL_LAYER_FROZEN) ? MTL_LAYER_FROZEN_MASK : 0;
		uint8 nWetLayer = (nMaterialLayers & MTL_LAYER_WET) ? MTL_LAYER_WET_MASK : 0;
		pObj->m_nMaterialLayers = (uint32)(nFrozenLayer << 24) | (nWetLayer << 16);
	}
	else
		pObj->m_nMaterialLayers = 0;

	if (!passInfo.IsShadowPass() && m_nInternalFlags & IRenderNode::REQUIRES_NEAREST_CUBEMAP)
	{
		if (!(pObj->m_nTextureID = GetObjManager()->CheckCachedNearestCubeProbe(this)) || !pCVars->e_CacheNearestCubePicking)
			pObj->m_nTextureID = GetObjManager()->GetNearestCubeProbe(pAffectingLights, m_pOcNode->m_pVisArea, CBrush::GetBBox());

		m_pTempData->userData.nCubeMapId = pObj->m_nTextureID;
	}

	//////////////////////////////////////////////////////////////////////////
	// temp fix to update ambient color (Vlad please review!)
	pObj->m_nClipVolumeStencilRef = userData.m_pClipVolume ? userData.m_pClipVolume->GetStencilRef() : 0;
	if (m_pOcNode && m_pOcNode->m_pVisArea)
		pObj->m_II.m_AmbColor = m_pOcNode->m_pVisArea->GetFinalAmbientColor();
	else
		pObj->m_II.m_AmbColor = Get3DEngine()->GetSkyColor();
	//////////////////////////////////////////////////////////////////////////

	if (pTerrainTexInfo)
	{
		bool bUseTerrainColor = (GetCVars()->e_BrushUseTerrainColor == 2);
		if (pMat && GetCVars()->e_BrushUseTerrainColor == 1)
		{
			if (pMat->GetSafeSubMtl(0)->GetFlags() & MTL_FLAG_BLEND_TERRAIN)
				bUseTerrainColor = true;
		}

		if (bUseTerrainColor)
		{
			m_pTempData->userData.bTerrainColorWasUsed = true;
			pObj->m_ObjFlags |= FOB_BLEND_WITH_TERRAIN_COLOR;

			pObj->m_data.m_pTerrainSectorTextureInfo = pTerrainTexInfo;
			pObj->m_nTextureID = -(int)pTerrainTexInfo->nSlot0 - 1; // nTextureID is set only for proper batching, actual texture id is same for all terrain sectors

			{
				float fBlendDistance = GetCVars()->e_VegetationUseTerrainColorDistance;

				if (fBlendDistance == 0)
				{
					pObj->m_data.m_fMaxViewDistance = m_fWSMaxViewDist;
				}
				else if (fBlendDistance > 0)
				{
					pObj->m_data.m_fMaxViewDistance = fBlendDistance;
				}
				else // if(fBlendDistance < 0)
				{
					pObj->m_data.m_fMaxViewDistance = abs(fBlendDistance) * GetCVars()->e_ViewDistRatio;
				}
			}
		}
		else
			pObj->m_ObjFlags &= ~FOB_BLEND_WITH_TERRAIN_COLOR;
	}

	// check the object against the water level
	if (CObjManager::IsAfterWater(vObjCenter, vCamPos, passInfo, Get3DEngine()->GetWaterLevel()))
		pObj->m_ObjFlags |= FOB_AFTER_WATER;
	else
		pObj->m_ObjFlags &= ~FOB_AFTER_WATER;

	if (CStatObj* pStatObj = (CStatObj*)CBrush::GetEntityStatObj())
	{
		// temporary fix for autoreload from max export, Vladimir needs to properly fix it!
		if (pObj->m_pCurrMaterial != GetMaterial())
			pObj->m_pCurrMaterial = GetMaterial();

		if (Get3DEngine()->IsTessellationAllowed(pObj, passInfo, true))
		{
			// Allow this RO to be tessellated, however actual tessellation will be applied if enabled in material
			pObj->m_ObjFlags |= FOB_ALLOW_TESSELLATION;
		}
		else
			pObj->m_ObjFlags &= ~FOB_ALLOW_TESSELLATION;

		if (pCVars->e_BBoxes)
			GetObjManager()->RenderObjectDebugInfo((IRenderNode*)this, pObj->m_fDistance, passInfo);

		if (lodValue.LodA() <= 0 && Cry3DEngineBase::GetCVars()->e_MergedMeshes != 0 && m_pDeform && m_pDeform->HasDeformableData())
		{
			if (GetCVars()->e_StatObjBufferRenderTasks == 1 && passInfo.IsGeneralPass() && JobManager::InvokeAsJob("CheckOcclusion"))
			{
				GetObjManager()->PushIntoCullOutputQueue(SCheckOcclusionOutput::CreateDeformableBrushOutput(this, gEnv->pRenderer->EF_DuplicateRO(pObj, passInfo), lodValue.LodA(), passInfo));
			}
			else
			{
				m_pDeform->RenderInternalDeform(gEnv->pRenderer->EF_DuplicateRO(pObj, passInfo), lodValue.LodA(), CBrush::GetBBox(), passInfo);
			}
		}

		pStatObj->RenderInternal(pObj, 0, lodValue, passInfo);
	}
}

///////////////////////////////////////////////////////////////////////////////
IStatObj* CBrush::GetEntityStatObj(unsigned int nPartId, unsigned int nSubPartId, Matrix34A* pMatrix, bool bReturnOnlyVisible)
{
	if (nPartId != 0)
		return 0;

	if (pMatrix)
		*pMatrix = m_Matrix;

	return m_pStatObj;
}

///////////////////////////////////////////////////////////////////////////////
void CBrush::OnRenderNodeBecomeVisible(const SRenderingPassInfo& passInfo)
{
	assert(m_pTempData);
	SRenderNodeTempData::SUserData& userData = m_pTempData->userData;

	userData.objMat = m_Matrix;
	const Vec3 vCamPos = passInfo.GetCamera().GetPosition();
	float fEntDistance = sqrt_tpl(Distance::Point_AABBSq(vCamPos, CBrush::GetBBox())) * passInfo.GetZoomFactor();

	userData.nWantedLod = CObjManager::GetObjectLOD(this, fEntDistance);

	int nLod = userData.nWantedLod;

	if (CStatObj* pStatObj = (CStatObj*)CBrush::GetEntityStatObj())
	{
		nLod = CLAMP(nLod, pStatObj->GetMinUsableLod(), (int)pStatObj->m_nMaxUsableLod);
		nLod = pStatObj->FindNearesLoadedLOD(nLod);
	}

	userData.lodDistDissolveTransitionState.nNewLod = userData.lodDistDissolveTransitionState.nOldLod = userData.nWantedLod;
	userData.lodDistDissolveTransitionState.fStartDist = 0.0f;
	userData.lodDistDissolveTransitionState.bFarside = false;
}

///////////////////////////////////////////////////////////////////////////////
bool CBrush::CanExecuteRenderAsJob()
{
	// disabled as workaround for CE-9292 crash
	return false;// !HasDeformableData();
}
