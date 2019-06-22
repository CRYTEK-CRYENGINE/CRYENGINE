// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureCommon.h"
#include <CrySerialization/Decorators/Resources.h>

namespace pfx2
{

//////////////////////////////////////////////////////////////////////////
// Feature Appearance Opacity

MakeDataType(EPDT_Alpha, float, EDD_ParticleUpdate);

SERIALIZATION_DECLARE_ENUM(EBlendMode,
	Opaque         = 0,
	Alpha          = OS_ALPHA_BLEND,
	Additive       = OS_ADD_BLEND,
	Multiplicative = OS_MULTIPLY_BLEND
)

class CFeatureAppearanceOpacity : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE;

	// Initialize after Material feature
	virtual int Priority() const override { return 1; }

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		m_opacity.AddToComponent(pComponent, this, EPDT_Alpha);

		pParams->m_renderStateFlags = (pParams->m_renderStateFlags & ~OS_TRANSPARENT) | (int)m_blendMode;

		if (m_blendMode == EBlendMode::Opaque)
		{
			// Force alpha-test params to pure alpha-test, with no feathering
			pParams->m_shaderData.m_alphaTest[0][0] = 1;
			pParams->m_shaderData.m_alphaTest[1][0] = 0;
			pParams->m_shaderData.m_alphaTest[0][1] = 1;
			pParams->m_shaderData.m_alphaTest[1][1] = -1;
			pParams->m_shaderData.m_alphaTest[0][2] = 0;
			pParams->m_shaderData.m_alphaTest[1][2] = 0;

			Range alpha = m_opacity.GetValueRange();
			pParams->m_renderObjectFlags |= FOB_ZPREPASS;
			if (alpha.start < 1.0f)
				pParams->m_renderObjectFlags |= FOB_ALPHATEST;
 			if (m_castShadows)
 				pComponent->AddEnvironFlags(ENV_CAST_SHADOWS);
		}
		else
		{
			pParams->m_shaderData.m_alphaTest[0][0] = m_alphaScale.start;
			pParams->m_shaderData.m_alphaTest[1][0] = m_alphaScale.Length();
			pParams->m_shaderData.m_alphaTest[0][1] = m_clipLow.start;
			pParams->m_shaderData.m_alphaTest[1][1] = m_clipLow.Length();
			pParams->m_shaderData.m_alphaTest[0][2] = m_clipRange.start;
			pParams->m_shaderData.m_alphaTest[1][2] = m_clipRange.Length();

			if (m_softIntersect > 0.f)
			{
				pParams->m_renderObjectFlags |= FOB_SOFT_PARTICLE;
				pParams->m_shaderData.m_softnessMultiplier = m_softIntersect;
			}
		}

		if (auto pInt = MakeGpuInterface(pComponent, gpu_pfx2::eGpuFeatureType_FieldOpacity))
		{
			const uint numSamples = gpu_pfx2::kNumModifierSamples;
			float samples[numSamples];
			m_opacity.Sample({samples, numSamples});
			gpu_pfx2::SFeatureParametersOpacity parameters;
			parameters.samples = samples;
			parameters.numSamples = numSamples;
			parameters.alphaScale = m_alphaScale;
			parameters.clipLow = m_clipLow;
			parameters.clipRange = m_clipRange;
			pInt->SetParameters(parameters);
		}
		pComponent->UpdateParticles.add(this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_opacity, "value", "Value");
		ar(m_blendMode, "BlendMode", "Blend Mode");
		if (m_blendMode == EBlendMode::Opaque)
		{
			ar(m_castShadows, "CastShadows", "Cast Shadows");
		}
		else
		{
			ar(m_softIntersect, "Softness", "Soft Intersect");
			ar(m_alphaScale, "AlphaScale", "Alpha Scale");
			ar(m_clipLow, "ClipLow", "Clip Low");
			ar(m_clipRange, "ClipRange", "Clip Range");
		}
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;
		m_opacity.Init(runtime, EPDT_Alpha);
	}

	virtual void UpdateParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;
		m_opacity.Update(runtime, EPDT_Alpha);
	}

protected:
	CParamMod<EDD_ParticleUpdate, UUnitFloat> m_opacity;
	EBlendMode                                m_blendMode     = EBlendMode::Alpha;
	UFloat                                    m_softIntersect = 0.0f;
	bool                                      m_castShadows   = true;
	Range                                     m_alphaScale    {0, 1};
	Range                                     m_clipLow       {0, 0};
	Range                                     m_clipRange     {1, 1};
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureAppearanceOpacity, "Appearance", "Opacity", colorAppearance);
CRY_PFX2_LEGACY_FEATURE(CFeatureAppearanceOpacity, "Field", "Opacity");

//////////////////////////////////////////////////////////////////////////
// Feature Appearance TextureTiling

MakeDataType(EPDT_Tile, uint8);

class CFeatureAppearanceTextureTiling : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	uint VariantCount() const
	{
		return m_tileCount / max(1u, uint(m_anim.m_frameCount));
	}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pParams->m_shaderData.m_tileSize[0] = 1.0f / float(m_tilesX);
		pParams->m_shaderData.m_tileSize[1] = 1.0f / float(m_tilesY);
		pParams->m_shaderData.m_frameCount = float(m_tileCount);
		pParams->m_shaderData.m_firstTile = float(m_firstTile);
		pParams->m_textureAnimation = m_anim;

		if (m_anim.IsAnimating() && m_anim.m_frameBlending)
			pParams->m_renderStateFlags |= OS_ANIM_BLEND;

		if (VariantCount() > 1)
		{
			pComponent->AddParticleData(EPDT_Tile);
			pComponent->InitParticles.add(this);
		}

		MakeGpuInterface(pComponent, gpu_pfx2::eGpuFeatureType_Dummy);
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		if (m_variantMode == EDistribution::Random)
			AssignTiles<EDistribution::Random>(runtime);
		else
			AssignTiles<EDistribution::Ordered>(runtime);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_tilesX, "TilesX", "Tiles X");
		ar(m_tilesY, "TilesY", "Tiles Y");
		ar(m_tileCount, "TileCount", "Tile Count");
		ar(m_firstTile, "FirstTile", "First Tile");
		ar(m_variantMode, "VariantMode", "Variant Mode");
		ar(m_anim, "Animation", "Animation");
	}

private:
	UBytePos          m_tilesX;
	UBytePos          m_tilesY;
	UBytePos          m_tileCount;
	UByte             m_firstTile;
	EDistribution     m_variantMode = EDistribution::Random;
	STextureAnimation m_anim;

	template<EDistribution mode>
	void AssignTiles(CParticleComponentRuntime& runtime)
	{
		CParticleContainer& container = runtime.GetContainer();
		TIOStream<uint8> tiles = container.IOStream(EPDT_Tile);
		const uint spawnIdOffset = container.GetSpawnIdOffset();
		const uint variantCount = VariantCount();

		for (auto particleId : runtime.SpawnedRange())
		{
			uint32 tile;
			if (mode == EDistribution::Random)
			{
				tile = runtime.Chaos().Rand();
			}
			else if (mode == EDistribution::Ordered)
			{
				tile = particleId + spawnIdOffset;
			}
			tile %= variantCount;
			tile *= m_anim.m_frameCount;
			tiles.Store(particleId, tile);
		}
	}

};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureAppearanceTextureTiling, "Appearance", "Texture Tiling", colorAppearance);

//////////////////////////////////////////////////////////////////////////
// Feature Appearance Material

class CFeatureAppearanceMaterial : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->LoadResources.add(this);
		LoadResources(*pComponent);
		MakeGpuInterface(pComponent, gpu_pfx2::eGpuFeatureType_Dummy);
	}

	virtual void LoadResources(CParticleComponent& component) override
	{
		SComponentParams& params = component.ComponentParams();
		if (!params.m_pMaterial)
		{
			if (!m_materialName.empty())
			{
				if (GetPSystem()->IsRuntime())
					params.m_pMaterial = gEnv->p3DEngine->GetMaterialManager()->FindMaterial(m_materialName);
				if (!params.m_pMaterial)
				{
					GetPSystem()->CheckFileAccess(m_materialName);
					params.m_pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(m_materialName);
				}
				if (params.m_pMaterial)
				{
					IShader* pShader = params.m_pMaterial->GetShaderItem().m_pShader;
					if (!pShader)
						params.m_pMaterial = nullptr;
					else if (params.m_requiredShaderType != eST_All)
					{
						if (pShader->GetFlags() & EF_LOADED && pShader->GetShaderType() != params.m_requiredShaderType)
							params.m_pMaterial = nullptr;
					}
				}
			}
			if (!params.m_pMaterial && !m_textureName.empty())
				params.m_pMaterial = GetPSystem()->GetTextureMaterial(m_textureName, 
					params.m_usesGPU, component.GPUComponentParams().facingMode);
		}
		if (params.m_pMaterial && GetCVars()->e_ParticlesPrecacheAssets)
		{
			params.m_pMaterial->PrecacheMaterial(0.0f, nullptr, true, true);
		}
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		using Serialization::MaterialPicker;
		using Serialization::TextureFilename;
		CParticleFeature::Serialize(ar);
		ar(MaterialPicker(m_materialName), "Material", "Material");
		ar(TextureFilename(m_textureName), "Texture", "Texture");
	}

	virtual uint GetNumResources() const override 
	{ 
		return !m_materialName.empty() || !m_textureName.empty() ? 1 : 0; 
	}

	virtual const char* GetResourceName(uint resourceId) const override
	{ 
		// Material has priority over the texture.
		return !m_materialName.empty() ? m_materialName.c_str() : m_textureName.c_str();
	}

private:
	string m_materialName;
	string m_textureName;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureAppearanceMaterial, "Appearance", "Material", colorAppearance);

//////////////////////////////////////////////////////////////////////////
// Feature Appearance Lighting

static const float kiloScale = 1000.0f;
static const float toLightUnitScale = kiloScale / RENDERER_LIGHT_UNIT_SCALE;

class CFeatureAppearanceLighting : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureAppearanceLighting()
		: m_diffuse(1.0f)
		, m_backLight(0.0f)
		, m_emissive(0.0f)
		, m_curvature(0.0f)
		, m_environmentLighting(true)
		, m_receiveShadows(false)
		, m_affectedByFog(true)
		, m_volumeFog(false)
		{}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		// light energy normalizer for the light equation:
		//	L = max(0, cos(a)*(1-y)+y)
		//	cos(a) = dot(l, n)
		//	y = back lighting
		const float y = m_backLight;
		const float energyNorm = (y < 0.5) ? (1 - y) : (1.0f / (4.0f * y));
		pParams->m_shaderData.m_diffuseLighting = m_diffuse * energyNorm;
		pParams->m_shaderData.m_backLighting = m_backLight;
		pParams->m_shaderData.m_emissiveLighting = m_emissive * toLightUnitScale;
		pParams->m_shaderData.m_curvature = m_curvature;
		if (m_diffuse >= FLT_EPSILON)
			pParams->m_renderObjectFlags |= FOB_LIGHTVOLUME;
		if (m_environmentLighting)
			pParams->m_renderStateFlags |= OS_ENVIRONMENT_CUBEMAP;
		if (m_receiveShadows)
			pParams->m_renderObjectFlags |= FOB_INSHADOW;
		if (!m_affectedByFog)
			pParams->m_renderObjectFlags |= FOB_NO_FOG;
		if (m_volumeFog)
			pParams->m_renderObjectFlags |= FOB_VOLUME_FOG;
		MakeGpuInterface(pComponent, gpu_pfx2::eGpuFeatureType_Dummy);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_diffuse, "Diffuse", "Diffuse");
		ar(m_backLight, "BackLight", "Back Light");
		ar(m_emissive, "Emissive", "Emissive (kcd/m2)");
		ar(m_curvature, "Curvature", "Curvature");
		ar(m_environmentLighting, "EnvironmentLighting", "Environment Lighting");
		ar(m_receiveShadows, "ReceiveShadows", "Receive Shadows");
		ar(m_affectedByFog, "AffectedByFog", "Affected by Fog");
		ar(m_volumeFog, "VolumeFog", "Volume Fog");
		if (ar.isInput())
			VersionFix(ar);
	}

private:
	void VersionFix(Serialization::IArchive& ar)
	{
		uint version = GetVersion(ar);
		if (version == 1)
		{
			m_emissive.Set(m_emissive / toLightUnitScale);
		}
		else if (version < 10)
		{
			if (ar(m_diffuse, "Albedo"))
				m_diffuse.Set(m_diffuse * 0.01f);
		}
	}

	UFloat     m_diffuse;
	UUnitFloat m_backLight;
	UFloat10   m_emissive;
	UUnitFloat m_curvature;
	bool       m_environmentLighting;
	bool       m_receiveShadows;
	bool       m_affectedByFog;
	bool       m_volumeFog;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureAppearanceLighting, "Appearance", "Lighting", colorAppearance);

//////////////////////////////////////////////////////////////////////////
// Legacy feature Appearance Blending, now included in Opacity

class CFeatureAppearanceBlending : public CFeatureAppearanceOpacity
{
public:
	virtual CParticleFeature* ResolveDependency(CParticleComponent* pComponent) override
	{
		// If Opacity feature exists, use it, and set the Blending param.
		// Otherwise, keep this feature
		if (auto pFeature = pComponent->FindDuplicateFeature(this))
		{
			pFeature->m_blendMode = m_blendMode;
			return nullptr;
		}
		return this;
	}
};

CRY_PFX2_LEGACY_FEATURE(CFeatureAppearanceBlending, "Appearance", "Blending");

//////////////////////////////////////////////////////////////////////////
// Legacy feature Appearance SoftIntersect, now included in Opacity

class CFeatureAppearanceSoftIntersect : public CFeatureAppearanceOpacity
{
public:
	virtual CParticleFeature* ResolveDependency(CParticleComponent* pComponent) override
	{
		// If Opacity feature exists, use it, and set the Blending param.
		// Otherwise, keep this feature
		if (auto pFeature = pComponent->FindDuplicateFeature(this))
		{
			pFeature->m_softIntersect = m_softIntersect;
			return nullptr;
		}
		return this;
	}
};

CRY_PFX2_LEGACY_FEATURE(CFeatureAppearanceSoftIntersect, "Appearance", "SoftIntersect");

//////////////////////////////////////////////////////////////////////////
// Feature Appearance Visibility

class CFeatureAppearanceVisibility : public CParticleFeature, SVisibilityParams
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pParams->m_visibility.Combine(*this);
		if (m_drawNear)
			pParams->m_renderObjectFlags |= FOB_NEAREST;
		if (m_drawOnTop)
			pParams->m_renderStateFlags |= OS_NODEPTH_TEST;
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		SERIALIZE_VAR(ar, m_minCameraDistance);
		SERIALIZE_VAR(ar, m_maxCameraDistance);
		SERIALIZE_VAR(ar, m_maxScreenSize);
		SERIALIZE_VAR(ar, m_viewDistanceMultiple);
		SERIALIZE_VAR(ar, m_indoorVisibility);
		SERIALIZE_VAR(ar, m_waterVisibility);
		SERIALIZE_VAR(ar, m_drawNear);
		SERIALIZE_VAR(ar, m_drawOnTop);

		if (ar.isInput() && GetVersion(ar) < 11)
		{
			if (m_maxCameraDistance == 0.0f)
				m_maxCameraDistance = gInfinity;
			if (m_maxScreenSize == 0.0f)
				m_maxScreenSize = gInfinity;
		}
	}

private:
	bool m_drawNear  = false;
	bool m_drawOnTop = false;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureAppearanceVisibility, "Appearance", "Visibility", colorAppearance);

}
