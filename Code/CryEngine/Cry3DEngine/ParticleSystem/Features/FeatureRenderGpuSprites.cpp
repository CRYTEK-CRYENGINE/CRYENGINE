// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleSystem/ParticleRender.h"

namespace gpu_pfx2
{

SERIALIZATION_ENUM_IMPLEMENT(ESortMode,
	None,
	BackToFront,
	FrontToBack,
	OldToNew,
	NewToOld
);

SERIALIZATION_ENUM_IMPLEMENT(EFacingMode,
    Screen,
    Velocity
);

};

namespace pfx2
{


#if !CRY_PLATFORM_ORBIS
	#define CRY_PFX2_POINT_SPRITES
#endif

class CFeatureRenderGpuSprites : public CParticleRenderBase
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureRenderGpuSprites()
		: CParticleRenderBase(gpu_pfx2::eGpuFeatureType_RenderGpu)
	{}

	virtual void         Serialize(Serialization::IArchive& ar) override;
	virtual EFeatureType GetFeatureType() override { return EFT_Render; }
	virtual void         ResolveDependency(CParticleComponent* pComponent) override;
	virtual void         AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override;

private:
	struct ConvertNextPowerOfTwo
	{
		template<typename T> static T From(T val) { return NextPower2(val); }
		template<typename T> static T To(T val)   { return NextPower2(val); }
	};
	typedef TValue<uint, THardLimits<1024, 1024*1024, ConvertNextPowerOfTwo>> UIntNextPowerOfTwo;

	UIntNextPowerOfTwo    m_maxParticles = 128*1024;
	UIntNextPowerOfTwo    m_maxNewBorns  = 8*1024;
	gpu_pfx2::ESortMode   m_sortMode     = gpu_pfx2::ESortMode::None;
	gpu_pfx2::EFacingMode m_facingMode   = gpu_pfx2::EFacingMode::Screen;
	UFloat10              m_axisScale    = 1.0f;
	SFloat                m_sortBias     = 0.0f;
};

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

	if (m_facingMode == gpu_pfx2::EFacingMode::Velocity)
		ar(m_axisScale, "AxisScale", "Axis Scale");

	ar(m_sortBias, "SortBias", "Sort Bias");
}

void CFeatureRenderGpuSprites::ResolveDependency(CParticleComponent* pComponent)
{
	gpu_pfx2::SComponentParams params;
	params.usesGpuImplementation = true;
	params.maxParticles = m_maxParticles;
	params.maxNewBorns = m_maxNewBorns;
	params.sortMode = m_sortMode;
	params.facingMode = m_facingMode;
	params.version = pComponent->GetEffect()->GetEditVersion();

	pComponent->SetGPUComponentParams(params);
}

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureRenderGpuSprites, "GPU Particles", "Sprites", colorGPU);
CRY_PFX2_LEGACY_FEATURE(CParticleFeature, CFeatureRenderGpuSprites, "RenderGPU Sprites");

}
