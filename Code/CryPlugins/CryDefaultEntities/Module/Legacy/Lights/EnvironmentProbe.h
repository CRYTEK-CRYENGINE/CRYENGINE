#pragma once

#include "Legacy/Helpers/DesignerEntityComponent.h"

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>

#include <CrySerialization/Math.h>
#include <CrySerialization/Decorators/Resources.h>
#include <CrySerialization/Color.h>

////////////////////////////////////////////////////////
// Sample entity for creating an environment probe
////////////////////////////////////////////////////////
class CEnvironmentProbeEntity final 
	: public CDesignerEntityComponent<>
	, public IEntityPropertyGroup
{
	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS_GUID(CEnvironmentProbeEntity, "EnvironmentProbe", "0d3d1840-d239-411e-8738-14c56cccee2c"_cry_guid);

	CEnvironmentProbeEntity();
	virtual ~CEnvironmentProbeEntity() {}

public:
	// CDesignerEntityComponent
	virtual IEntityPropertyGroup* GetPropertyGroup() final { return this; }

	virtual void OnResetState() override;
	// ~CDesignerEntityComponent

	// IEntityPropertyGroup
	virtual const char* GetLabel() const override { return "EnvironmentProbe Properties"; }

	virtual void SerializeProperties(Serialization::IArchive& archive) override
	{
		archive(m_bActive, "Active", "Active");
		archive(m_light.m_ProbeExtents, "BoxSize", "Box Size");

		if (archive.openBlock("Options", "Options"))
		{
			archive(m_bIgnoreVisAreas, "IgnoreVisAreas", "Ignore Vis Areas");
			archive(m_light.m_nSortPriority, "SortPriority", "Sort Priority");
			archive(m_attenuationFalloffMax, "AttentuationFalloffMax", "Attenuation Falloff Max");
			archive(m_bVolumetricFogOnly, "VolumetricFogOnly", "Volumetric Fog Only");
			archive(m_bAffectsVolumetricFog, "AffectsVolumetricFog", "Affects Volumetric Fog");
			archive(m_bAffectsOnlyThisArea, "AffectsOnlyThisArea", "Affects Only This Area");
			archive(m_bLinkToSkyColor, "LinkToSkyColor", "Link To Sky Color");

			archive.closeBlock();
		}

		if (archive.openBlock("Color", "Color"))
		{
			archive(m_color, "Color", "Color");
			archive(m_diffuseMultiplier, "DiffuseMultiplier", "Diffuse Multiplier");
			archive(m_light.m_SpecMult, "SpecularMultiplier", "Specular Multiplier");

			archive.closeBlock();
		}

		if (archive.openBlock("Projection", "Projection"))
		{
			archive(m_bBoxProjection, "ProjectionActive", "Active");

			archive(m_light.m_fBoxHeight, "ProjectionHeight", "Height");
			archive(m_light.m_fBoxLength,  "ProjectionLength", "Length");
			archive(m_light.m_fBoxWidth, "ProjectionWidth", "Width" );

			archive.closeBlock();
		}

		if (archive.openBlock("OptionsAdvanced", "Advanced"))
		{
			archive(Serialization::TextureFilename(m_cubemapPath), "Cubemap", "Cubemap");

			archive.closeBlock();
		}

		if (archive.isInput())
		{
			OnResetState();
		}
	}
	// ~IEntityPropertyGroup

protected:
	void GetCubemapTextures(const char* path, ITexture** pSpecular, ITexture** pDiffuse) const;

	// Specifies the entity geometry slot in which the light is loaded, -1 if not currently loaded
	// We default to using slot 1 for this light sample, in case the user would want to put geometry into slot 0.
	int m_lightSlot;

	// Light parameters, updated in the OnResetState function
	SRenderLight m_light;

	ColorF m_color = ColorF(1.0f, 1.0f, 1.0f);
	float m_diffuseMultiplier = 1.0f;

	bool m_bIgnoreVisAreas = false;
	bool m_bBoxProjection = false;
	bool m_bVolumetricFogOnly = false;
	bool m_bAffectsVolumetricFog = true;
	bool m_bAffectsOnlyThisArea = true;
	bool m_bLinkToSkyColor = false;

	bool m_bActive = true;
	string m_cubemapPath;

	float m_attenuationFalloffMax = 1.f;
};
