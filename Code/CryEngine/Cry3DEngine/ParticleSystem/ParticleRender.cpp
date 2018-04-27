// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleRender.h"
#include "ParticleManager.h"
#include "ParticleSystem.h"

namespace pfx2
{

struct CRY_ALIGN(CRY_PFX2_PARTICLES_ALIGNMENT) SCachedRenderObject
{
};

EFeatureType CParticleRenderBase::GetFeatureType()
{
	return EFT_Render;
}

void CParticleRenderBase::AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams)
{
	CParticleEffect* pEffect = pComponent->GetEffect();
	pComponent->Render.add(this);
	pComponent->ComputeVertices.add(this);
	m_waterCulling = SupportsWaterCulling();
	if (m_waterCulling)
		m_renderObjectBeforeWaterId = pEffect->AddRenderObjectId();
	m_renderObjectAfterWaterId = pEffect->AddRenderObjectId();
	pParams->m_requiredShaderType = eST_Particle;
}

void CParticleRenderBase::Render(CParticleEmitter* pEmitter, CParticleComponentRuntime* pComponentRuntime, CParticleComponent* pComponent, const SRenderContext& renderContext)
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
	const SComponentParams& params = pComponent->GetComponentParams();

	CRenderObject* pRenderObject = gEnv->pRenderer->EF_GetObject();
	auto particleMaterial = params.m_pMaterial;

	pRenderObject->SetIdentityMatrix();
	pRenderObject->m_fAlpha = 1.0f;
	pRenderObject->m_pCurrMaterial = particleMaterial;
	pRenderObject->m_pRenderNode = pEmitter;
	pRenderObject->m_RState = params.m_renderStateFlags;
	pRenderObject->m_fSort = 0;
	pRenderObject->m_ParticleObjFlags = params.m_particleObjFlags;
	pRenderObject->m_pRE = gEnv->pRenderer->EF_CreateRE(eDATA_Particle);

	SRenderObjData* pObjData = pRenderObject->GetObjData();
	pObjData->m_pParticleShaderData = &params.m_shaderData;

	pEmitter->SetRenderObject(pRenderObject, std::move(particleMaterial), threadId, renderObjectId);
}

void CParticleRenderBase::AddRenderObject(CParticleEmitter* pEmitter, CParticleComponentRuntime* pComponentRuntime, CParticleComponent* pComponent, const SRenderContext& renderContext, uint renderObjectId, uint threadId, uint64 objFlags)
{
	const SComponentParams& params = pComponent->GetComponentParams();
	CRenderObject* pRenderObject = pEmitter->GetRenderObject(threadId, renderObjectId);
	if (!pRenderObject)
	{
		PrepareRenderObject(pEmitter, pComponent, renderObjectId, threadId, params.m_renderObjectFlags);
		pRenderObject = pEmitter->GetRenderObject(threadId, renderObjectId);
	}
	SRenderObjData* pObjData = pRenderObject->GetObjData();

	const float sortBiasSize = 1.0f / 1024.0f;
	const float sortBias = renderContext.m_distance * params.m_renderObjectSortBias * sortBiasSize;

	pRenderObject->m_ObjFlags = objFlags;
	pRenderObject->m_fDistance = renderContext.m_distance - sortBias;
	pRenderObject->SetInstanceDataDirty();
	pObjData->m_FogVolumeContribIdx = renderContext.m_fogVolumeId;
	pObjData->m_LightVolumeId = renderContext.m_lightVolumeId;
	if (const auto p = pEmitter->m_pTempData.load())
		*((Vec4f*)&pObjData->m_fTempVars[0]) = Vec4f(p->userData.vEnvironmentProbeMults);
	else
		*((Vec4f*)&pObjData->m_fTempVars[0]) = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);

	CParticleJobManager& kernel = GetPSystem()->GetJobManager();
	kernel.ScheduleComputeVertices(pComponentRuntime, pRenderObject, renderContext);
	
	int passId = renderContext.m_passInfo.IsShadowPass() ? 1 : 0;
	int passMask = BIT(passId);
	pRenderObject->m_passReadyMask |= passMask;

	renderContext.m_passInfo.GetIRenderView()->AddPermanentObject(
		pRenderObject,
		renderContext.m_passInfo);
}

float CParticleRenderBase::CullArea(float area, float areaLimit, TParticleIdArray& ids, TVarArray<float> alphas, TConstArray<float> areas)
{
	if (area > areaLimit)
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
		
		// Sort sprites by area, cull largest
		stl::sort(ids, [&areas](uint id) { return areas[id]; });

		static float Slope = 2.0f;
		float factor = Slope / areaLimit;
		float areaDrawn = 0;
		uint newSize = ids.size();
		for (uint i = 0; i < ids.size(); ++i)
		{
			uint id = ids[i];
			areaDrawn += areas[id];
			float adjust = Slope - areaDrawn * factor;
			if (adjust < 1.0f)
			{
				if (adjust <= 0.0f)
				{
					newSize = i;
					areaDrawn -= areas[id];
					break;
				}
				alphas[id] *= adjust;
			}
		}
		for (uint i = newSize; i < ids.size(); ++i)
		{
			uint id = ids[i];
			alphas[id] = 0.0f;
		}
		ids.resize(newSize);
		return areaDrawn;
	}
	else
		return area;
}

}
