// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleSystem/ParticleSystem.h"
#include "ClipVolumeManager.h"
#include "ParamMod.h"

namespace pfx2
{

extern TDataType<float>
	EPDT_Alpha,
	EPDT_Size;
extern TDataType<UCol>
	EPDT_Color;

SERIALIZATION_DECLARE_ENUM(ELightAffectsFog,
	No,
	FogOnly,
	Both
	)

class CFeatureLightSource : public CParticleFeature, public Cry3DEngineBase
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->RenderDeferred.add(this);
		pComponent->AddParticleData(EPVF_Position);
		if (GetPSystem()->GetFlareMaterial() && !m_flare.empty())
			m_hasFlareOptics = gEnv->pOpticsManager->Load(m_flare.c_str(), m_lensOpticsId);

		// Compute max effective radius
		SRenderLight light;
		light.SetRadius(m_radiusClip, FLT_MIN);
		light.SetLightColor(ColorF(m_intensity * pParams->m_maxParticleAlpha));
		SetMax(pParams->m_physicalSizeSlope.start, light.m_fRadius);
		SetMax(pParams->m_maxParticleSize, light.m_fRadius);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		SERIALIZE_VAR(ar, m_intensity);
		SERIALIZE_VAR(ar, m_radiusClip);
		SERIALIZE_VAR(ar, m_affectsFog);
		SERIALIZE_VAR(ar, m_affectsThisAreaOnly);
		SERIALIZE_VAR(ar, m_flare);
	}

	virtual void RenderDeferred(const CParticleComponentRuntime& runtime, const SRenderContext& renderContext) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		if (!GetCVars()->e_DynamicLights)
			return;

		if (!runtime.IsCPURuntime())
			return;

		SRenderLight light;
		if (renderContext.m_passInfo.IsRecursivePass())
			return;

		light.m_nStencilRef[0] = m_affectsThisAreaOnly ? renderContext.m_renderParams.nClipVolumeStencilRef : CClipVolumeManager::AffectsEverythingStencilRef;
		light.m_nStencilRef[1] = CClipVolumeManager::InactiveVolumeStencilRef;
		light.m_Flags |= DLF_DEFERRED_LIGHT;
		if (m_affectsFog == ELightAffectsFog::Both)
			light.m_Flags |= DLF_VOLUMETRIC_FOG;
		else if (m_affectsFog == ELightAffectsFog::FogOnly)
			light.m_Flags |= DLF_VOLUMETRIC_FOG | DLF_VOLUMETRIC_FOG_ONLY;

		if (m_hasFlareOptics)
		{
			light.m_sName = "Wavicle";
			IOpticsElementBase* pOptics = gEnv->pOpticsManager->GetOptics(m_lensOpticsId);
			light.SetLensOpticsElement(pOptics);
			light.m_Shader = GetPSystem()->GetFlareMaterial()->GetShaderItem();
		}

		UCol defaultColor;
		defaultColor.dcolor = ~0;
		const CParticleContainer& container = runtime.GetContainer();
		const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
		const IColorStream colors = container.GetIColorStream(EPDT_Color, defaultColor);
		const IFStream alphas = container.GetIFStream(EPDT_Alpha, 1.0f);
		const IFStream sizes = container.GetIFStream(EPDT_Size);

		const SRenderingPassInfo& passInfo = renderContext.m_passInfo;
		const CCamera& camera = passInfo.GetCamera();
		const Vec3 camPos = camera.GetPosition();
		const float distRatio = GetFloatCVar(e_ParticlesLightsViewDistRatio);

		for (auto particleId : container.GetFullRange())
		{
			const Vec3 position = positions.Load(particleId);
			light.SetPosition(position);
			const float bulbSize = max(sizes.SafeLoad(particleId), 0.001f);
			light.SetRadius(m_radiusClip, bulbSize);
			const UCol color = colors.SafeLoad(particleId);
			const float intensity = m_intensity * alphas.SafeLoad(particleId) * rcp(light.GetIntensityScale());
			light.SetLightColor(ToColorF(color) * ColorF(intensity));

			if (position.GetSquaredDistance(camPos) < sqr(light.m_fRadius * distRatio)
				&& camera.IsSphereVisible_F(Sphere(position, light.m_fRadius)))
			{
				Get3DEngine()->SetupLightScissors(&light, passInfo);
				light.m_n3DEngineUpdateFrameID = passInfo.GetMainFrameID();
				Get3DEngine()->AddLightToRenderer(light, 1.0f, passInfo);
			}
		}
	}

private:
	UFloat10         m_intensity           = 1;
	UInfFloat        m_radiusClip;
	ELightAffectsFog m_affectsFog          = ELightAffectsFog::Both;
	bool             m_affectsThisAreaOnly = false;
	string           m_flare;

	bool             m_hasFlareOptics      = false;
	int              m_lensOpticsId;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLightSource, "Light", "Light", colorLight);

}
