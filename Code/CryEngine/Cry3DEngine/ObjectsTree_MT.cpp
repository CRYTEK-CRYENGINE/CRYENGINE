// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PolygonClipContext.h"
#include "RoadRenderNode.h"
#include "Brush.h"
#include "RenderMeshMerger.h"
#include "DecalManager.h"
#include "VisAreas.h"
#include <CryAnimation/ICryAnimation.h>
#include "LightEntity.h"

#ifndef PI
	#define PI 3.14159f
#endif

//////////////////////////////////////////////////////////////////////////
void CObjManager::PrepareCullbufferAsync(const CCamera& rCamera)
{
	if (gEnv->pRenderer)
	{
		m_CullThread.PrepareCullbufferAsync(rCamera);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjManager::BeginOcclusionCulling(const SRenderingPassInfo& passInfo)
{
	if (gEnv->pRenderer)
	{
		m_CullThread.CullStart(passInfo);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjManager::EndOcclusionCulling()
{
	if (gEnv->pRenderer)
	{
		m_CullThread.CullEnd();
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjManager::RenderNonJobObjects(const SRenderingPassInfo& passInfo)
{
	CRY_PROFILE_REGION(PROFILE_3DENGINE, "3DEngine: RenderNonJobObjects");
	CRYPROFILE_SCOPE_PROFILE_MARKER("RenderNonJobObjects");

	SCheckOcclusionOutput outputData;
	while (1)
	{
		// process till we know that no more procers are working
		if (!GetObjManager()->PopFromCullOutputQueue(&outputData))
			break;

		switch (outputData.type)
		{
		case SCheckOcclusionOutput::ROAD_DECALS:
			GetObjManager()->RenderDecalAndRoad(outputData.common.pObj,
			                                    outputData.common.pAffectingLights,
			                                    outputData.vAmbColor,
			                                    outputData.objBox,
			                                    outputData.common.fEntDistance,
			                                    outputData.common.bCheckPerObjectOcclusion,
			                                    passInfo);
			break;

		case SCheckOcclusionOutput::COMMON:
			{
				switch (outputData.common.pObj->GetRenderNodeType())
				{
				case eERType_Brush:
				case eERType_MovableBrush:
					GetObjManager()->RenderBrush((CBrush*)outputData.common.pObj,
					                             outputData.common.pAffectingLights,
					                             outputData.common.pTerrainTexInfo,
					                             outputData.objBox,
					                             outputData.common.fEntDistance,
					                             outputData.common.bCheckPerObjectOcclusion,
					                             passInfo,
					                             outputData.passCullMask);
					break;

				case eERType_Vegetation:
					GetObjManager()->RenderVegetation((CVegetation*)outputData.common.pObj,
					                                  outputData.common.pAffectingLights,
					                                  outputData.objBox,
					                                  outputData.common.fEntDistance,
					                                  outputData.common.pTerrainTexInfo,
					                                  outputData.common.bCheckPerObjectOcclusion,
					                                  passInfo,
					                                  outputData.passCullMask);
					break;

				default:
					GetObjManager()->RenderObject(outputData.common.pObj,
					                              outputData.common.pAffectingLights,
					                              outputData.vAmbColor,
					                              outputData.objBox,
					                              outputData.common.fEntDistance,
					                              outputData.common.pObj->GetRenderNodeType(),
					                              passInfo,
					                              outputData.passCullMask);
					break;
				}
			}
			break;
		case SCheckOcclusionOutput::TERRAIN:
			outputData.terrain.pTerrainNode->RenderNodeHeightmap(passInfo, outputData.passCullMask);
			break;

		case SCheckOcclusionOutput::DEFORMABLE_BRUSH:
			outputData.deformable_brush.pBrush->m_pDeform->RenderInternalDeform(outputData.deformable_brush.pRenderObject,
			                                                                    outputData.deformable_brush.nLod,
			                                                                    outputData.deformable_brush.pBrush->CBrush::GetBBox(),
			                                                                    passInfo);
			break;

		default:
			CryFatalError("Got Unknown Output type from CheckOcclusion");
			break;
		}
	}
#if !defined(_RELEASE)
	if (GetCVars()->e_CoverageBufferDebug)
	{
		GetObjManager()->CoverageBufferDebugDraw();
	}
#endif
}

bool IsValidOccluder(IMaterial* pMat)
{
	CMatInfo* __restrict pMaterial = static_cast<CMatInfo*>(pMat);
	if (!pMaterial)
		return false;

	for (size_t a = 0, S = pMaterial->GetSubMtlCount(); a <= S; a++)
	{
		IMaterial* pSubMat = a < S ? pMaterial->GetSubMtl(a) : pMaterial;
		if (!pSubMat)
			continue;
		SShaderItem& rShaderItem = pSubMat->GetShaderItem();
		IShader* pShader = rShaderItem.m_pShader;
		if (!pShader)
			continue;
		if (pShader->GetFlags2() & (EF2_HASALPHATEST | EF2_HASALPHABLEND))
			return false;
		if (pShader->GetFlags() & (EF_NODRAW | EF_DECAL))
			return false;
		if (pShader->GetShaderType() == eST_Vegetation)
			return false;
		else if (!rShaderItem.m_pShaderResources)
			continue;
		if (rShaderItem.m_pShaderResources->IsAlphaTested())
			return false;
		if (rShaderItem.m_pShaderResources->IsTransparent())
			return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CObjManager::BeginCulling()
{
	m_CheckOcclusionOutputQueue.SetRunningState();
}
