// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureCommon.h"
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

	virtual void              Serialize(Serialization::IArchive& ar) override;
	virtual EFeatureType      GetFeatureType() override { return EFT_Render; }
	virtual CParticleFeature* ResolveDependency(CParticleComponent* pComponent) override;
	virtual void              AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override;

private:
	struct ConvertNextPowerOfTwo
	{
		template<typename T> static T From(T val) { return NextPower2(val); }
		template<typename T> static T To(T val)   { return NextPower2(val); }
	};
	typedef TValue<uint, THardLimits<1024, 1024*1024, ConvertNextPowerOfTwo>> UIntNextPowerOfTwo;

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
	ar(m_sortMode, "SortMode", "Sort Mode");
	ar(m_facingMode, "FacingMode", "Facing Mode");

	if (m_facingMode == gpu_pfx2::EFacingMode::Velocity)
		ar(m_axisScale, "AxisScale", "Axis Scale");

	ar(m_sortBias, "SortBias", "Sort Bias");
}

CParticleFeature* CFeatureRenderGpuSprites::ResolveDependency(CParticleComponent* pComponent)
{
	pComponent->ComponentParams().m_usesGPU = true;

	gpu_pfx2::SComponentParams& params = pComponent->GPUComponentParams();
	params.sortMode = m_sortMode;
	params.facingMode = m_facingMode;
	params.version = pComponent->GetEffect()->GetEditVersion();

	return this;
}

CRY_PFX2_LEGACY_FEATURE(CFeatureRenderGpuSprites, "Render", "GPU Sprites");
CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureRenderGpuSprites, "GPU Particles", "Sprites", colorGPU);

}
