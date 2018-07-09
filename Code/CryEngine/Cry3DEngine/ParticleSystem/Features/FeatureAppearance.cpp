// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureCommon.h"
#include <CrySerialization/Decorators/Resources.h>

namespace pfx2
{

MakeDataType(EPDT_Tile, uint8);

SERIALIZATION_ENUM_DEFINE(EVariantMode, ,
                          Random,
                          Ordered
                          )

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
			if (m_variantMode == EVariantMode::Ordered)
				pComponent->AddParticleData(EPDT_SpawnId);
			pComponent->InitParticles.add(this);
		}

		MakeGpuInterface(pComponent, gpu_pfx2::eGpuFeatureType_Dummy);
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		if (m_variantMode == EVariantMode::Random)
			AssignTiles<EVariantMode::Random>(runtime);
		else
			AssignTiles<EVariantMode::Ordered>(runtime);
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
	EVariantMode      m_variantMode = EVariantMode::Random;
	STextureAnimation m_anim;

	template<EVariantMode mode>
	void AssignTiles(CParticleComponentRuntime& runtime)
	{
		CParticleContainer& container = runtime.GetContainer();
		TIOStream<uint8> tiles = container.IOStream(EPDT_Tile);
		TIStream<uint> spawnIds = container.IStream(EPDT_SpawnId);
		uint variantCount = VariantCount();

		for (auto particleId : runtime.SpawnedRange())
		{
			uint32 tile;
			if (mode == EVariantMode::Random)
			{
				tile = runtime.Chaos().Rand();
			}
			else if (mode == EVariantMode::Ordered)
			{
				tile = spawnIds.Load(particleId);
			}
			tile %= variantCount;
			tile *= m_anim.m_frameCount;
			tiles.Store(particleId, tile);
		}
	}

};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureAppearanceTextureTiling, "Appearance", "Texture Tiling", colorAppearance);

class CFeatureAppearanceMaterial : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		if (!m_materialName.empty())
		{
			pParams->m_pMaterial = gEnv->p3DEngine->GetMaterialManager()->FindMaterial(m_materialName);
			if (!pParams->m_pMaterial)
			{
				GetPSystem()->CheckFileAccess(m_materialName);
				pParams->m_pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(m_materialName);
			}
		}
		if (!m_textureName.empty())
			pParams->m_diffuseMap = m_textureName;
		MakeGpuInterface(pComponent, gpu_pfx2::eGpuFeatureType_Dummy);
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
			pParams->m_particleObjFlags |= CREParticle::ePOF_VOLUME_FOG;
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

SERIALIZATION_DECLARE_ENUM(EBlendMode,
                           Opaque = 0,
                           Alpha = OS_ALPHA_BLEND,
                           Additive = OS_ADD_BLEND,
                           Multiplicative = OS_MULTIPLY_BLEND
                           )

class CFeatureAppearanceBlending : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pParams->m_renderStateFlags = (pParams->m_renderStateFlags & ~OS_TRANSPARENT) | (int)m_blendMode;
		MakeGpuInterface(pComponent, gpu_pfx2::eGpuFeatureType_Dummy);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_blendMode, "BlendMode", "Blend Mode");
	}

private:
	EBlendMode m_blendMode = EBlendMode::Alpha;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureAppearanceBlending, "Appearance", "Blending", colorAppearance);

class CFeatureAppearanceSoftIntersect : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		if (m_softNess > 0.f)
		{
			pParams->m_renderObjectFlags |= FOB_SOFT_PARTICLE;
			pParams->m_shaderData.m_softnessMultiplier = m_softNess;
		}
		MakeGpuInterface(pComponent, gpu_pfx2::eGpuFeatureType_Dummy);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_softNess, "Softness", "Softness");
	}

private:
	UFloat m_softNess = 1.0f;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureAppearanceSoftIntersect, "Appearance", "SoftIntersect", colorAppearance);

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
