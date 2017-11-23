#pragma once

#include "Legacy/Helpers/DesignerEntityComponent.h"

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>

#include <CrySerialization/Color.h>
#include <CrySerialization/STL.h>
#include <CrySerialization/Decorators/Resources.h>

////////////////////////////////////////////////////////
// Sample entity for creating a light source
////////////////////////////////////////////////////////
class CDefaultLightEntity final
	: public CDesignerEntityComponent<>
	, public IEntityPropertyGroup
{
	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS_GUID(CDefaultLightEntity, "LightEntity", "61bfe88a-ab1c-4dfb-bbd2-63d328c3d037"_cry_guid);

	CDefaultLightEntity();
	virtual ~CDefaultLightEntity() {}

	enum EFlowgraphInputPorts
	{
		eInputPorts_Active = 0,
		eInputPorts_Enable,
		eInputPorts_Disable,
	};

	enum EFlowgraphOutputPorts
	{
		eOutputPorts_Active,
	};

public:
	enum ECastShadowsSpec
	{
		eCastShadowsSpec_No = 0,
		eCastShadowsSpec_Low,
		eCastShadowsSpec_Medium,
		eCastShadowsSpec_High,
		eCastShadowsSpec_VeryHigh
	};

public:
	// CDesignerEntityComponent
	virtual IEntityPropertyGroup* GetPropertyGroup() final { return this; }

	virtual void OnResetState() override;
	// ~CDesignerEntityComponent

	// IEntityPropertyGroup
	virtual const char* GetLabel() const override { return "Light Properties"; }

	virtual void SerializeProperties(Serialization::IArchive& archive) override
	{
		archive(m_bActive, "Active", "Active");
		archive(m_light.m_fClipRadius, "Radius", "Radius");
		archive(m_light.m_fAttenuationBulbSize, "AttenuationBulbSize", "AttenuationBulbSize");
		if (archive.isInput())
			m_light.ComputeEffectiveRadius();

		if (archive.openBlock("Color", "Color"))
		{
			archive(m_diffuseColor, "DiffuseColor", "DiffuseColor");
			archive(m_diffuseMultiplier, "DiffuseMultiplier", "DiffuseMultiplier");

			archive.closeBlock();
		}

		if (archive.openBlock("Options", "Options"))
		{
			archive(m_bIgnoreVisAreas, "IgnoreVisAreas", "IgnoreVisAreas");
			archive(m_bAffectsThisAreaOnly, "AffectsThisAreaOnly", "AffectsThisAreaOnly");
			archive(m_bAmbient, "Ambient", "Ambient");
			archive(m_bLinkToSkyColor, "LinkToSkyColor", "LinkToSkyColor");
			archive(m_bFake, "FakeLight", "FakeLight");
			archive(m_bAffectVolumetricFog, "AffectVolumetricFog", "AffectVolumetricFog");
			archive(m_bAffectVolumetricFogOnly, "AffectVolumetricFogOnly", "AffectVolumetricFogOnly");
			archive(m_light.m_fFogRadialLobe, "FogRadialLobe", "FogRadialLobe");

			archive.closeBlock();
		}

		if (archive.openBlock("Shadows", "Shadows"))
		{
			archive(m_castShadowSpec, "CastShadows", "CastShadows");

			archive.closeBlock();
		}

		if (archive.openBlock("Animation", "Animation"))
		{
			archive(m_light.m_nLightStyle, "Style", "Style");
			archive(m_animSpeed, "Speed", "Speed");

			archive.closeBlock();
		}

		if (archive.openBlock("Projector", "Projector"))
		{
			archive(Serialization::TextureFilename(m_projectorTexturePath), "ProjectionTexture", "ProjectionTexture");
			archive(m_light.m_fLightFrustumAngle, "ProjectionFieldOfView", "ProjectionFieldOfView");
			archive(m_light.m_fProjectorNearPlane, "ProjectionNearPlane", "ProjectionNearPlane");

			archive.closeBlock();
		}

		if (archive.openBlock("Flare", "Flare"))
		{
			archive(Serialization::TextureFilename(m_flareTexturePath), "FlareTextures", "FlareTextures");
			archive(m_flareFieldOfView, "FlareFieldOfView", "FlareFieldOfView");

			archive.closeBlock();
		}

		if (archive.isInput())
		{
			OnResetState();
		}
	}
	// ~IEntityPropertyGroup

	void SetActive(bool bActive)
	{
		if (m_bActive != bActive)
		{
			m_bActive = bActive;
			OnResetState();
		}
	}

public:
	static void OnFlowgraphActivation(EntityId entityId, IFlowNode::SActivationInfo* pActInfo, const class CEntityFlowNode* pNode);

protected:
	// Specifies the entity geometry slot in which the light is loaded, -1 if not currently loaded
	// We default to using slot 1 for this light sample, in case the user would want to put geometry into slot 0.
	int m_lightSlot;

	// Light parameters, updated in the OnResetState function
	SRenderLight m_light;

	bool m_bActive = true;

	bool m_bIgnoreVisAreas = false;
	bool m_bAffectsThisAreaOnly = true;
	bool m_bAmbient = false;
	bool m_bLinkToSkyColor = false;
	bool m_bFake = false;
	bool m_bAffectVolumetricFog = true;
	bool m_bAffectVolumetricFogOnly = false;

	ECastShadowsSpec m_castShadowSpec = eCastShadowsSpec_No;

	float m_animSpeed = 1.f;

	ColorF m_diffuseColor = ColorF(1, 1, 1, 1);
	float m_diffuseMultiplier = 1.f;

	string m_projectorTexturePath;

	string m_flareTexturePath;
	float m_flareFieldOfView = 360.f;
};
