#pragma once

#include "Helpers/DesignerEntityComponent.h"

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>

#include <CrySerialization/Math.h>
#include <CrySerialization/Decorators/Resources.h>

////////////////////////////////////////////////////////
// Sample entity for creating an environment probe
////////////////////////////////////////////////////////
class CEnvironmentProbeEntity final 
	: public CDesignerEntityComponent<>
	, public IEntityPropertyGroup
{
	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS(CEnvironmentProbeEntity, "EnvironmentProbe", 0x0D3D1840D239411E, 0x873814C56CCCEE2C);

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
		archive(m_light.m_ProbeExtents, "BoxSize", "BoxSize");

		if (archive.openBlock("Options", "Options"))
		{
			archive(m_bIgnoreVisAreas, "IgnoreVisAreas", "IgnoreVisAreas");
			archive(m_light.m_nSortPriority, "SortPriority", "SortPriority");
			archive(m_attentuationFalloffMax, "AttentuationFalloffMax", "AttentuationFalloffMax");

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
	CDLight m_light;

	bool m_bActive = true;
	string m_cubemapPath;

	bool m_bIgnoreVisAreas = false;
	float m_attentuationFalloffMax = 1.f;
};
