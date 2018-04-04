// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "FeatureRenderGpuSprites.h"
#include "ParticleSystem/ParticleEmitter.h"

CRY_PFX2_DBG

namespace pfx2
{

#if !CRY_PLATFORM_ORBIS
	#define CRY_PFX2_POINT_SPRITES
#endif

CFeatureRenderGpuSprites::CFeatureRenderGpuSprites()
	: CParticleRenderBase(gpu_pfx2::eGpuFeatureType_RenderGpu)
	, m_maxParticles(131072)
	, m_maxNewBorns(8192)
	, m_sortMode(EGpuSpritesSortMode::None)
	, m_facingMode(EGpuSpritesFacingMode::Screen)
	, m_axisScale(1.0f)
	, m_sortBias(0.0f)
{
}

void CFeatureRenderGpuSprites::AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams)
{
	CParticleRenderBase::AddToComponent(pComponent, pParams);
	pParams->m_renderObjectFlags |= FOB_POINT_SPRITE;
	pParams->m_renderObjectSortBias = m_sortBias;
	pParams->m_shaderData.m_axisScale = m_axisScale;
}

void CFeatureRenderGpuSprites::Serialize(Serialization::IArchive& ar)
{
	CParticleFeature::Serialize(ar);
	ar(m_maxParticles, "MaxParticles", "Max Particles");
	ar(m_maxNewBorns, "MaxNewBorns", "Max New Particles");
	ar(m_sortMode, "SortMode", "Sort Mode");
	ar(m_facingMode, "FacingMode", "Facing Mode");

	if (m_facingMode == EGpuSpritesFacingMode::Velocity)
		ar(m_axisScale, "AxisScale", "Axis Scale");

	ar(m_sortBias, "SortBias", "Sort Bias");
}

void CFeatureRenderGpuSprites::ResolveDependency(CParticleComponent* pComponent)
{
	SRuntimeInitializationParameters params;
	params.usesGpuImplementation = true;
	params.maxParticles = m_maxParticles;
	params.maxNewBorns = m_maxNewBorns;
	params.sortMode = static_cast<EGpuSortMode>(m_sortMode);
	params.facingMode = static_cast<EGpuFacingMode>(m_facingMode);
	auto& compParams = pComponent->GetComponentParams();
	params.isSecondGen = compParams.IsSecondGen();
	params.parentId = compParams.m_parentId;
	params.version = pComponent->GetEffect()->GetEditVersion();

	pComponent->SetRuntimeInitializationParameters(params);
}

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureRenderGpuSprites, "GPU Particles", "Sprites", colorGPU);
CRY_PFX2_LEGACY_FEATURE(CParticleFeature, CFeatureRenderGpuSprites, "RenderGPU Sprites");

}
