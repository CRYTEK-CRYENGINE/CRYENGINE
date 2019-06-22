// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureCommon.h"

namespace pfx2
{

extern TDataType<float> EPDT_Alpha;

//////////////////////////////////////////////////////////////////////////
// CFeatureFieldSize

MakeDataType(EPDT_Size, float, EDD_ParticleUpdate);

class CFeatureFieldSize : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		m_size.AddToComponent(pComponent, this, EPDT_Size);
		pParams->m_maxParticleSize = m_size.GetValueRange().end;

		if (auto gpuInt = MakeGpuInterface(pComponent, gpu_pfx2::eGpuFeatureType_FieldSize))
		{
			const uint numSamples = gpu_pfx2::kNumModifierSamples;
			float samples[numSamples];
			m_size.Sample({samples, numSamples});
			gpu_pfx2::SFeatureParametersSizeTable sizeTable;
			sizeTable.samples = samples;
			sizeTable.numSamples = numSamples;
			gpuInt->SetParameters(sizeTable);
		}
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_size, "value", "Value");
	}

	virtual EFeatureType GetFeatureType() override { return EFT_Size; }
	static uint DefaultForType() { return EFT_Size; }

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;
		m_size.Init(runtime, EPDT_Size);
	}

	virtual void UpdateParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;
		m_size.Update(runtime, EPDT_Size);
	}

private:
	CParamMod<EDD_ParticleUpdate, UFloat10> m_size;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureFieldSize, "Field", "Size", colorField);

//////////////////////////////////////////////////////////////////////////
// CFeatureFieldPixelSize

class CFeatureFieldPixelSize : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureFieldPixelSize()
		: m_minSize(4.0f)
		, m_maxSize(0.0f)
		, m_initAlphas(false)
		, m_affectOpacity(true)
	{}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->AddParticleData(EPVF_Position);
		pComponent->AddParticleData(EPDT_Size);
		if (m_affectOpacity)
		{
			m_initAlphas = !pComponent->UseParticleData(EPDT_Alpha);
			pComponent->AddParticleData(EPDT_Alpha);
		}
		pComponent->UpdateParticles.add(this);

		if (auto gpuInt = MakeGpuInterface(pComponent, gpu_pfx2::eGpuFeatureType_FieldPixelSize))
		{
			gpu_pfx2::SFeatureParametersPixelSize params;
			params.minSize = m_minSize;
			params.maxSize = m_maxSize;
			params.affectOpacity = m_affectOpacity;
			gpuInt->SetParameters(params);
		}
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_minSize, "MinSize", "Minimum Pixel Size");
		ar(m_maxSize, "MaxSize", "Maximum Pixel Size");
		ar(m_affectOpacity, "AffectOpacity", "Affect Opacity");
	}

	virtual void UpdateParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		const CCamera& camera = gEnv->p3DEngine->GetRenderingCamera();
		const Planev projectionPlane = ToPlanev(Plane(camera.GetViewdir(), -camera.GetPosition().dot(camera.GetViewdir())));

		const float height = float(gEnv->pRenderer->GetHeight());
		const float fov = camera.GetFov();
		const float minPixelSizeF = m_minSize > 0.0f ? float(m_minSize) : 0.0f;
		const floatv minPixelSize = ToFloatv(minPixelSizeF);
		const floatv invMinPixelSize = ToFloatv(minPixelSizeF != 0.0f ? 1.0f / minPixelSizeF : 1.0f);
		const floatv maxPixelSize = ToFloatv(m_maxSize > 0.0f ? float(m_maxSize) : std::numeric_limits<float>::max());
		const floatv minDrawPixels = ToFloatv(fov / height * 0.5f);
		const floatv epsilon = ToFloatv(1.0f / 1024.0f);

		CParticleContainer& container = runtime.GetContainer();
		IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
		IOFStream sizes = container.GetIOFStream(EPDT_Size);
		IFStream inputAlphas = m_initAlphas ? IFStream(nullptr, 1.0f) : container.GetIFStream(EPDT_Alpha);
		IOFStream outputAlphas = container.GetIOFStream(EPDT_Alpha);

		for (auto particleGroupId : runtime.FullRangeV())
		{
			const Vec3v position = positions.Load(particleGroupId);
			const floatv size0 = sizes.Load(particleGroupId);
			const floatv distance = max(epsilon, projectionPlane.DistFromPlane(position));
			const floatv pixelSize0 = rcp_fast(distance * minDrawPixels) * size0;
			const floatv pixelSize1 = clamp(pixelSize0, minPixelSize, maxPixelSize);
			const floatv size1 = pixelSize1 * distance * minDrawPixels;
			sizes.Store(particleGroupId, size1);

			if (m_affectOpacity)
			{
				const floatv alpha0 = inputAlphas.SafeLoad(particleGroupId);
				const floatv alpha1 = alpha0 * saturate(pixelSize0 * invMinPixelSize);
				outputAlphas.Store(particleGroupId, alpha1);
			}
		}
	}

private:
	UFloat10 m_minSize;
	UFloat10 m_maxSize;
	bool     m_affectOpacity;
	bool     m_initAlphas;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureFieldPixelSize, "Field", "PixelSize", colorField);

}
