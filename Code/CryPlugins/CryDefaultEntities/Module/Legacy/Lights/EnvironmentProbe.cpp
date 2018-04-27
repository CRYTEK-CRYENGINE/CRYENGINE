#include "StdAfx.h"
#include "EnvironmentProbe.h"

class CProbeRegistrator : public IEntityRegistrator
{
	virtual void Register() override
	{
		if (gEnv->pEntitySystem->GetClassRegistry()->FindClass("EnvironmentLight") != nullptr)
		{
			// Skip registration of default engine class if the game has overridden it
			CryLog("Skipping registration of default engine entity class EnvironmentLight, overridden by game");
			return;
		}

		RegisterEntityWithDefaultComponent<CEnvironmentProbeEntity>("EnvironmentLight", "Lights", "Light.bmp");
	}
};

CProbeRegistrator g_probeRegistrator;

CRYREGISTER_CLASS(CEnvironmentProbeEntity);

CEnvironmentProbeEntity::CEnvironmentProbeEntity()
// Start by setting the light slot to an invalid value by default
	: m_lightSlot(-1)
{
}

void CEnvironmentProbeEntity::OnResetState()
{
	IEntity& entity = *GetEntity();

	// Check if we have to unload first
	if (m_lightSlot != -1)
	{
		entity.FreeSlot(m_lightSlot);
		m_lightSlot = -1;
	}

	if (!m_bActive || gEnv->IsDedicated())
		return;

	m_light.m_nLightStyle = 0;
	m_light.SetPosition(ZERO);
	m_light.m_fLightFrustumAngle = 45;
	m_light.m_Flags = DLF_POINT | DLF_DEFERRED_CUBEMAPS;
	m_light.m_LensOpticsFrustumAngle = 255;

	m_light.m_fRadius = pow(m_light.m_ProbeExtents.GetLengthSquared(), 0.5f);

	m_light.SetLightColor(m_color * m_diffuseMultiplier);

	m_light.m_fHDRDynamic = 0.f;

	if (m_bAffectsOnlyThisArea)
		m_light.m_Flags |= DLF_THIS_AREA_ONLY;

	if (m_bIgnoreVisAreas)
		m_light.m_Flags |= DLF_IGNORES_VISAREAS;

	if (m_bLinkToSkyColor)
		m_light.m_Flags |= DLF_LINK_TO_SKY_COLOR;

	if (m_bBoxProjection)
		m_light.m_Flags |= DLF_BOX_PROJECTED_CM;

	if (m_bVolumetricFogOnly)
		m_light.m_Flags |= DLF_VOLUMETRIC_FOG_ONLY;

	if (m_bAffectsVolumetricFog)
		m_light.m_Flags |= DLF_VOLUMETRIC_FOG;

	m_light.SetFalloffMax(m_attenuationFalloffMax);

	m_light.m_fFogRadialLobe = 0.f;

	m_light.SetAnimSpeed(0.f);
	m_light.m_fProjectorNearPlane = 0.f;

	if (m_cubemapPath.IsEmpty())
	{
		m_light.DropResources();
		return;
	}

	ITexture* pSpecularTexture, *pDiffuseTexture;
	GetCubemapTextures(m_cubemapPath, &pSpecularTexture, &pDiffuseTexture);

	// When the textures remains the same, reference count of their resources should be bigger than 1 now,
	// so light resources can be dropped without releasing textures
	m_light.DropResources();

	m_light.SetSpecularCubemap(pSpecularTexture);
	m_light.SetDiffuseCubemap(pDiffuseTexture);

	if (!m_light.GetSpecularCubemap() || !m_light.GetDiffuseCubemap())
	{
		CRY_ASSERT_MESSAGE(m_light.GetSpecularCubemap(), "Failed to load specular cubemap for environment probe!");
		CRY_ASSERT_MESSAGE(m_light.GetDiffuseCubemap(), "Failed to load diffuse cubemap for environment probe!");

		m_light.DropResources();
		m_light.m_Flags &= ~DLF_DEFERRED_CUBEMAPS;
		return;
	}

	// Load the light source into the entity
	m_lightSlot = entity.LoadLight(1, &m_light);
}

void CEnvironmentProbeEntity::GetCubemapTextures(const char* path, ITexture** pSpecular, ITexture** pDiffuse) const
{
	stack_string specularCubemap = path;

	stack_string sSpecularName(specularCubemap);
	int strIndex = sSpecularName.find("_diff");
	if (strIndex >= 0)
	{
		sSpecularName = sSpecularName.substr(0, strIndex) + sSpecularName.substr(strIndex + 5, sSpecularName.length());
		specularCubemap = sSpecularName.c_str();
	}

	char diffuseCubemap[ICryPak::g_nMaxPath];
	cry_sprintf(diffuseCubemap, "%s%s%s.%s", PathUtil::AddSlash(PathUtil::GetPathWithoutFilename(specularCubemap)).c_str(),
	          PathUtil::GetFileName(specularCubemap).c_str(), "_diff", PathUtil::GetExt(specularCubemap));

	// '\\' in filename causing texture duplication
	stack_string specularCubemapUnix = PathUtil::ToUnixPath(specularCubemap.c_str());
	stack_string diffuseCubemapUnix = PathUtil::ToUnixPath(diffuseCubemap);

	*pSpecular = gEnv->pRenderer->EF_LoadTexture(specularCubemapUnix, 0);
	*pDiffuse = gEnv->pRenderer->EF_LoadTexture(diffuseCubemapUnix, 0);
}
