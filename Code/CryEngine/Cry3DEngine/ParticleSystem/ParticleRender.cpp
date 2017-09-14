// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     26/11/2014 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleRender.h"
#include "ParticleManager.h"
#include "ParticleEmitter.h"

namespace pfx2
{

struct CRY_ALIGN(CRY_PFX2_PARTICLES_ALIGNMENT) SCachedRenderObject
{
};

CParticleRenderBase::CParticleRenderBase(gpu_pfx2::EGpuFeatureType type)
	: CParticleFeature(type)
	, m_renderObjectBeforeWaterId(-1)
	, m_renderObjectAfterWaterId(-1)
	, m_waterCulling(false)
{
}

EFeatureType CParticleRenderBase::GetFeatureType()
{
	return EFT_Render;
}

void CParticleRenderBase::AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams)
{
	CParticleEffect* pEffect = pComponent->GetEffect();
	pComponent->AddToUpdateList(EUL_Render, this);
	m_waterCulling = SupportsWaterCulling();
	if (m_waterCulling)
		m_renderObjectBeforeWaterId = pEffect->AddRenderObjectId();
	m_renderObjectAfterWaterId = pEffect->AddRenderObjectId();
	pParams->m_requiredShaderType = eST_Particle;
}

void CParticleRenderBase::PrepareRenderObjects(CParticleEmitter* pEmitter, CParticleComponent* pComponent)
{
	const SComponentParams& params = pComponent->GetComponentParams();

	if (!params.m_pMaterial)
		return;

	for (uint threadId = 0; threadId < RT_COMMAND_BUF_COUNT; ++threadId)
	{
		const uint64 objFlags = params.m_renderObjectFlags;
		if (m_waterCulling)
			PrepareRenderObject(pEmitter, pComponent, m_renderObjectBeforeWaterId, threadId, objFlags);
		PrepareRenderObject(pEmitter, pComponent, m_renderObjectAfterWaterId, threadId, objFlags);
	}
}

void CParticleRenderBase::ResetRenderObjects(CParticleEmitter* pEmitter, CParticleComponent* pComponent)
{
	for (uint threadId = 0; threadId < RT_COMMAND_BUF_COUNT; ++threadId)
	{
		ResetRenderObject(pEmitter, pComponent, m_renderObjectBeforeWaterId, threadId);
		ResetRenderObject(pEmitter, pComponent, m_renderObjectAfterWaterId, threadId);
	}
}

void CParticleRenderBase::Render(CParticleEmitter* pEmitter, IParticleComponentRuntime* pComponentRuntime, CParticleComponent* pComponent, const SRenderContext& renderContext)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	const SComponentParams& params = pComponent->GetComponentParams();
	const uint threadId = renderContext.m_passInfo.ThreadID();

	if (!params.m_pMaterial)
		return;

	const bool isIndoors = pEmitter->GetVisEnv().OriginIndoors();
	const bool renderIndoor = params.m_visibility.m_indoorVisibility != EIndoorVisibility::OutdoorOnly;
	const bool renderOutdoors = params.m_visibility.m_indoorVisibility != EIndoorVisibility::IndoorOnly;
	if ((isIndoors && !renderIndoor) || (!isIndoors && !renderOutdoors))
		return;

	uint64 objFlags = params.m_renderObjectFlags;
	if (!renderContext.m_lightVolumeId)
		objFlags &= ~FOB_LIGHTVOLUME;

	const bool isCameraUnderWater = renderContext.m_passInfo.IsCameraUnderWater();;
	const bool renderBelowWater = params.m_visibility.m_waterVisibility != EWaterVisibility::AboveWaterOnly;
	const bool renderAboveWater = params.m_visibility.m_waterVisibility != EWaterVisibility::BelowWaterOnly;

	if (m_waterCulling && ((isCameraUnderWater && renderAboveWater) || (!isCameraUnderWater && renderBelowWater)))
		AddRenderObject(pEmitter, pComponentRuntime, pComponent, renderContext, m_renderObjectBeforeWaterId, threadId, objFlags);
	if ((isCameraUnderWater && renderBelowWater) || (!isCameraUnderWater && renderAboveWater))
		AddRenderObject(pEmitter, pComponentRuntime, pComponent, renderContext, m_renderObjectAfterWaterId, threadId, objFlags | FOB_AFTER_WATER);
}

void CParticleRenderBase::PrepareRenderObject(CParticleEmitter* pEmitter, CParticleComponent* pComponent, uint renderObjectId, uint threadId, uint64 objFlags)
{
	CRenderObject* pRenderObject = gEnv->pRenderer->EF_GetObject();
	pEmitter->SetRenderObject(pRenderObject, threadId, renderObjectId);

	const SComponentParams& params = pComponent->GetComponentParams();
	const SParticleShaderData& rData = params.m_shaderData;

	pRenderObject->m_II.m_Matrix.SetIdentity();
	pRenderObject->m_fAlpha = 1.0f;
	pRenderObject->m_pCurrMaterial = params.m_pMaterial;
	pRenderObject->m_pRenderNode = pEmitter;
	pRenderObject->m_RState = params.m_renderStateFlags;
	pRenderObject->m_fSort = 0;
	pRenderObject->m_ParticleObjFlags = params.m_particleObjFlags;
	pRenderObject->m_pRE = gEnv->pRenderer->EF_CreateRE(eDATA_Particle);

	SRenderObjData* pObjData = pRenderObject->GetObjData();
	pObjData->m_pParticleShaderData = &params.m_shaderData;
}

void CParticleRenderBase::ResetRenderObject(CParticleEmitter* pEmitter, CParticleComponent* pComponent, uint renderObjectId, uint threadId)
{
	if (renderObjectId == -1)
		return;
	CRenderObject* pRenderObject = pEmitter->GetRenderObject(threadId, renderObjectId);
	if (!pRenderObject)
		return;

	if (pRenderObject->m_pRE != nullptr)
		pRenderObject->m_pRE->Release();
	gEnv->pRenderer->EF_FreeObject(pRenderObject);
	pEmitter->SetRenderObject(nullptr, threadId, renderObjectId);
}

void CParticleRenderBase::AddRenderObject(CParticleEmitter* pEmitter, IParticleComponentRuntime* pComponentRuntime, CParticleComponent* pComponent, const SRenderContext& renderContext, uint renderObjectId, uint threadId, uint64 objFlags)
{
	const SComponentParams& params = pComponent->GetComponentParams();
	CRenderObject* pRenderObject = pEmitter->GetRenderObject(threadId, renderObjectId);
	SRenderObjData* pObjData = pRenderObject->GetObjData();

	const float sortBiasSize = 1.0f / 1024.0f;
	const float sortBias = renderContext.m_distance * params.m_renderObjectSortBias * sortBiasSize;

	pRenderObject->m_ObjFlags = objFlags;
	pRenderObject->m_fDistance = renderContext.m_distance - sortBias;
	pObjData->m_FogVolumeContribIdx = renderContext.m_fogVolumeId;
	pObjData->m_LightVolumeId = renderContext.m_lightVolumeId;
	if (pEmitter->m_pTempData)
		*((Vec4f*)&pObjData->m_fTempVars[0]) = (const Vec4f&)pEmitter->m_pTempData->userData.vEnvironmentProbeMults;
	else
		*((Vec4f*)&pObjData->m_fTempVars[0]) = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);

	CParticleJobManager& kernel = GetPSystem()->GetJobManager();
	kernel.ScheduleComputeVertices(pComponentRuntime, pRenderObject, renderContext);

	renderContext.m_passInfo.GetIRenderView()->AddPermanentObject(
		pRenderObject,
		renderContext.m_passInfo);
}

}
