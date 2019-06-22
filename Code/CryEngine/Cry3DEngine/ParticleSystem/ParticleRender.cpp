// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleRender.h"
#include "ParticleManager.h"
#include "ParticleSystem.h"
#include "ParticleEmitter.h"

namespace pfx2
{

struct CRY_ALIGN(CRY_PFX2_PARTICLES_ALIGNMENT) SCachedRenderObject
{
};

void CParticleRenderBase::AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams)
{
	pComponent->Render.add(this);
	pComponent->ComputeVertices.add(this);
	pParams->m_requiredShaderType = eST_Particle;
	pParams->m_renderObjectFlags |= FOB_VERTEX_PULL_MODEL;
}

void CParticleRenderBase::Render(CParticleComponentRuntime& runtime, const SRenderContext& renderContext)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	const SComponentParams& params = runtime.ComponentParams();

	if (!params.m_pMaterial)
		return;

	const auto& emitter = *runtime.GetEmitter();
	const bool isNear = (emitter.GetRndFlags() & ERF_FOB_NEAREST) || (params.m_renderObjectFlags & FOB_NEAREST);
	if (!isNear)
	{
		const bool isIndoors = emitter.GetVisEnv().OriginIndoors();
		const bool renderIndoor = params.m_visibility.m_indoorVisibility != EIndoorVisibility::OutdoorOnly;
		const bool renderOutdoors = params.m_visibility.m_indoorVisibility != EIndoorVisibility::IndoorOnly;
		if ((isIndoors && !renderIndoor) || (!isIndoors && !renderOutdoors))
			return;
	}

	const auto emitterUnderWater = emitter.GetPhysicsEnv().m_tUnderWater;
	const bool cameraUnderWater  = renderContext.m_passInfo.IsCameraUnderWater();
	const auto waterVisibility   = params.m_visibility.m_waterVisibility;
	const bool renderBelowWater  = waterVisibility != EWaterVisibility::AboveWaterOnly && emitterUnderWater != ETrinary::If_False;
	const bool renderAboveWater  = waterVisibility != EWaterVisibility::BelowWaterOnly && emitterUnderWater != ETrinary::If_True;

	if (renderContext.m_passInfo.IsShadowPass())
		AddRenderObject(runtime, renderContext);
	else if (isNear)
		AddRenderObject(runtime, renderContext, FOB_NEAREST | FOB_AFTER_WATER);
	else if (params.m_renderStateFlags & OS_TRANSPARENT)
	{
		if (m_waterCulling && (cameraUnderWater ? renderAboveWater : renderBelowWater))
			AddRenderObject(runtime, renderContext);
		if (cameraUnderWater ? renderBelowWater : renderAboveWater)
			AddRenderObject(runtime, renderContext, FOB_AFTER_WATER);
	}
	else if (renderAboveWater || renderBelowWater)
		AddRenderObject(runtime, renderContext);
}

void CParticleRenderBase::AddRenderObject(CParticleComponentRuntime& runtime, const SRenderContext& renderContext, ERenderObjectFlags extraFlags)
{
	CRY_PFX2_PROFILE_DETAIL;

	CRenderObject* pRenderObject = runtime.GetRenderObject(renderContext.m_passInfo.ThreadID(), extraFlags);
	SRenderObjData* pObjData = pRenderObject->GetObjData();

	const float sortBiasSize = 1.0f / 1024.0f;
	const float numComponents = (float) runtime.GetEffect()->GetNumComponents();
	const float sortBias = (float) runtime.GetComponent()->GetComponentId() / numComponents + m_sortBias;

	pRenderObject->m_fDistance = renderContext.m_distance;
	pRenderObject->m_fSort = -sortBias * sortBiasSize * renderContext.m_distance;
	pRenderObject->SetInstanceDataDirty();
	pObjData->m_FogVolumeContribIdx = renderContext.m_fogVolumeId;
	pObjData->m_LightVolumeId = renderContext.m_lightVolumeId;
	if (const auto p = runtime.GetEmitter()->m_pTempData)
		reinterpret_cast<Vec4f&>(pObjData->m_fTempVars[0]) = (Vec4f const&)(p->userData.vEnvironmentProbeMults);
	else
		reinterpret_cast<Vec4f&>(pObjData->m_fTempVars[0]) = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);

	pRenderObject->m_pRE->m_CustomTexBind[0] = renderContext.m_renderParams.nTextureID;

	static_cast<CREParticle*>(pRenderObject->m_pRE)->SetBBox(runtime.GetBounds());
	GetPSystem()->GetJobManager().ScheduleComputeVertices(runtime, pRenderObject, renderContext);

	pRenderObject->SetPreparedForPass(renderContext.m_passInfo.GetPassType());

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

		static float Slope = 1.0f;
		const float factor = Slope / areaLimit;
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


SFarCulling::SFarCulling(const CParticleComponentRuntime& runtime, const CCamera& camera)
{
	const CParticleEmitter& emitter = *runtime.GetEmitter();
	const SVisibilityParams& visibility = runtime.ComponentParams().m_visibility;

	maxCamDist = visibility.m_maxCameraDistance;
	invMinAng = GetPSystem()->GetMaxAngularDensity(camera) * emitter.GetViewDistRatio() * visibility.m_viewDistanceMultiple;
	const float farDist = runtime.GetBounds().GetFarDistance(camera.GetPosition());
	doCull = farDist > maxCamDist * 0.75f
		|| Cry3DEngineBase::GetCVars()->e_ParticlesMinDrawPixels > 0.0f;
}

SNearCulling::SNearCulling(const CParticleComponentRuntime& runtime, const CCamera& camera, bool clipNear)
{
	const SVisibilityParams& visibility = runtime.ComponentParams().m_visibility;

	const float camNearClip = clipNear ? camera.GetEdgeN().GetLength() : 0.0f;
	const float nearDist = runtime.GetBounds().GetDistance(camera.GetPosition());
	const float camAng = camera.GetFov();
	const float maxScreen = min(+visibility.m_maxScreenSize, Cry3DEngineBase::GetCVars()->e_ParticlesMaxDrawScreen);
	minCamDist = max(+visibility.m_minCameraDistance, camNearClip);
	invMaxAng = 1.0f / (maxScreen * camAng * 0.5f);

	doCull = nearDist < minCamDist * 2.0f
		|| maxScreen < 2 && nearDist < runtime.ComponentParams().m_maxParticleSize * invMaxAng * 2.0f;
}

}
