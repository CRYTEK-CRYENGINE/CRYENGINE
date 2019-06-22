// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PolygonClipContext.h"
#include "CryThreading/IJobManager.h"
#include "RoadRenderNode.h"
#include "Brush.h"
#include "RenderMeshMerger.h"
#include "DecalManager.h"
#include "VisAreas.h"
#include <CryAnimation/ICryAnimation.h>
#include "LightEntity.h"

//////////////////////////////////////////////////////////////////////////
void CObjManager::PrepareCullbufferAsync(const CCamera& rCamera, const SGraphicsPipelineKey& cullGraphicsContextKey)
{
	if (gEnv->pRenderer)
	{
		m_CullThread.PrepareCullbufferAsync(rCamera, cullGraphicsContextKey);
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
	CRY_PROFILE_SECTION(PROFILE_3DENGINE, "3DEngine: RenderNonJobObjects");
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "CObjManager::RenderNonJobObjects");

	bool hasWaited = false;
	while (1)
	{
		// Optimization:
		// First we work off all already queued occlusion job outputs.
		// Once there is no more output in the queue, we wait on the occlusion jobs to finish which will spawn an extra worker.
		// Lastly we work off all remaining occlusion job outputs
		SCheckOcclusionOutput outputData;
		if (!GetObjManager()->PopFromCullOutputQueue(outputData))
		{
			if (!hasWaited)
			{
				m_CullThread.WaitOnCheckOcclusionJobs();
				hasWaited = true;
				continue;
			}
			else if (!GetObjManager()->PopFromCullOutputQueue(outputData))
			{
				break;
			}
		}

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
