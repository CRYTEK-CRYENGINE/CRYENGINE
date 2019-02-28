#pragma once
#include "DefaultComponents/Geometry/BaseMeshComponent.h"
#include <CrySchematyc/Utils/SharedString.h>

namespace Cry
{
namespace DefaultComponents
{
	constexpr const char* g_szDefaultLensFlareMaterialName = "%ENGINE%/EngineAssets/Materials/lens_optics";
	
	// Used to indicate the minimum graphical setting for an effect
	enum class ELightGIMode
	{
		Disabled = IRenderNode::EGIMode::eGM_None,
		StaticLight = IRenderNode::EGIMode::eGM_StaticVoxelization,
		DynamicLight = IRenderNode::EGIMode::eGM_DynamicVoxelization,
		ExcludeForGI = IRenderNode::EGIMode::eGM_HideIfGiIsActive,
	};

	static void ReflectType(Schematyc::CTypeDesc<ELightGIMode>& desc)
	{
		desc.SetGUID("{2DC20597-A17E-4306-A023-B73A6FBDBB3D}"_cry_guid);
		desc.SetLabel("Global Illumination Mode");
		desc.SetDefaultValue(ELightGIMode::Disabled);
		desc.AddConstant(ELightGIMode::Disabled, "None", "Disabled");
		desc.AddConstant(ELightGIMode::StaticLight, "StaticLight", "Static");
		desc.AddConstant(ELightGIMode::DynamicLight, "DynamicLight", "Dynamic");
		desc.AddConstant(ELightGIMode::ExcludeForGI, "ExcludeForGI", "Hide if GI is Active");
	}

	struct ILightComponent : public IEntityComponent
	{
	public:
		struct SOptions
		{
			inline bool operator==(const SOptions &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

			Schematyc::Range<0, 64000> m_attenuationBulbSize = SRenderLight().m_fAttenuationBulbSize;
			bool m_bIgnoreVisAreas = false;
			bool m_bVolumetricFogOnly = false;
			bool m_bAffectsVolumetricFog = true;
			bool m_bAffectsOnlyThisArea = true;
			bool m_bLinkToSkyColor = false;
			bool m_bAmbient = false;
			Schematyc::Range<0, 10000> m_fogRadialLobe = SRenderLight().m_fFogRadialLobe;

			ELightGIMode m_giMode = ELightGIMode::Disabled;
		};

		struct SColor
		{
			inline bool operator==(const SColor &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

			ColorF m_color = ColorF(1.f);
			Schematyc::Range<0, 10000, 0, 100> m_diffuseMultiplier = 1.f;
			Schematyc::Range<0, 10000> m_specularMultiplier = 1.f;
		};

		struct SOptics
		{
			inline bool operator==(const SOptics &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

			Schematyc::CSharedString m_lensFlareName = "";
			bool m_attachToSun = false;
			bool m_flareEnable = true;
			int32 m_flareFOV = 360;
		};

		struct SShadows
		{
			inline bool operator==(const SShadows &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

			EMiniumSystemSpec m_castShadowSpec = EMiniumSystemSpec::Disabled;
			float m_shadowBias = 1.f;
			float m_shadowSlopeBias = 1.f;
			float m_shadowResolutionScale = 1.f;
			float m_shadowUpdateMinRadius = 4.f;
			int32 m_shadowMinResolution = 0;
			int32 m_shadowUpdateRatio = 1;
		};

		struct SAnimations
		{
			inline bool operator==(const SAnimations &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

			uint32 m_style = 0;
			float m_speed = 1.f;
		};

	public:
		virtual void SetOptics(const char* szFullOpticsName) = 0;

		virtual void Enable(bool bEnable) = 0;
		virtual bool IsEnabled() const { return m_bActive; }

		virtual SOptions& GetOptions() { return m_options; }
		virtual const SOptions& GetOptions() const { return m_options; }

		virtual SColor& GetColorParameters() { return m_color; }
		virtual const SColor& GetColorParameters() const { return m_color; }

		virtual SShadows& GetShadowParameters() { return m_shadows; }
		virtual const SShadows& GetShadowParameters() const { return m_shadows; }

		virtual SAnimations& GetAnimationParameters() { return m_animations; }
		virtual const SAnimations& GetAnimationParameters() const { return m_animations; }

		virtual SOptics& GetOpticParameters() { return m_optics; }
		virtual const SOptics& GetOpticParameters() const { return m_optics; }

	protected:
		bool m_bActive = true;
		SOptions m_options;
		SColor m_color;
		SShadows m_shadows;
		SOptics m_optics;
		SAnimations m_animations;
	};

	static void ReflectType(Schematyc::CTypeDesc<ILightComponent::SOptions>& desc)
	{
		desc.SetGUID("{DB10AB64-7A5B-4B91-BC90-6D692D1D1222}"_cry_guid);
		desc.AddMember(&ILightComponent::SOptions::m_attenuationBulbSize, 'atte', "AttenuationBulbSize", "Attenuation Bulb Size", "Controls the fall-off exponentially from the origin, a value of 1 means that the light is at full intensity within a 1 meter ball before it begins to fall-off.", SRenderLight().m_fAttenuationBulbSize);
		desc.AddMember(&ILightComponent::SOptions::m_bIgnoreVisAreas, 'igvi', "IgnoreVisAreas", "Ignore VisAreas", nullptr, false);
		desc.AddMember(&ILightComponent::SOptions::m_bAffectsVolumetricFog, 'volf', "AffectVolumetricFog", "Affect Volumetric Fog", nullptr, true);
		desc.AddMember(&ILightComponent::SOptions::m_bVolumetricFogOnly, 'volo', "VolumetricFogOnly", "Only Affect Volumetric Fog", nullptr, false);
		desc.AddMember(&ILightComponent::SOptions::m_bAffectsOnlyThisArea, 'area', "OnlyAffectThisArea", "Only Affect This Area", nullptr, true);
		desc.AddMember(&ILightComponent::SOptions::m_bLinkToSkyColor, 'ltsc', "LinkToSkyColor", "Link To Sky Color", "Multiply light color with current sky color (use GI sky color if available).", false);
		desc.AddMember(&ILightComponent::SOptions::m_bAmbient, 'ambi', "Ambient", "Ambient", nullptr, false);
		desc.AddMember(&ILightComponent::SOptions::m_fogRadialLobe, 'fogr', "FogRadialLobe", "Fog Radial Lobe", nullptr, SRenderLight().m_fFogRadialLobe);
		desc.AddMember(&ILightComponent::SOptions::m_giMode, 'gimo', "GIMode", "Global Illumination", nullptr, ELightGIMode::Disabled);
	}

	static void ReflectType(Schematyc::CTypeDesc<ILightComponent::SOptics>& desc)
	{
		desc.SetGUID("{DBD843BC-E4E7-4950-B54D-4FD507B223FD}"_cry_guid);
		desc.AddMember(&ILightComponent::SOptics::m_flareEnable, 'ena', "Enable", "Enable", "Decides if the fare should be able", true);
		desc.AddMember(&ILightComponent::SOptics::m_lensFlareName, 'lens', "LensFlare", "Lens Flare Name", "Name of the lens flare", "");
		desc.AddMember(&ILightComponent::SOptics::m_attachToSun, 'atta', "AttachToSun", "Attach To Sun", "When enabled, sets the Sun to use the Flare properties for this light", false);
		desc.AddMember(&ILightComponent::SOptics::m_flareFOV, 'fov', "FlareFOV", "Flare Field of View", "FOV for the flare", 360);
	}

	static void ReflectType(Schematyc::CTypeDesc<ILightComponent::SColor>& desc)
	{
		desc.SetGUID("{B71C3414-F85A-4EAA-9CE0-5110A2E040AD}"_cry_guid);
		desc.AddMember(&ILightComponent::SColor::m_color, 'col', "Color", "Color", nullptr, ColorF(1.f));
		desc.AddMember(&ILightComponent::SColor::m_diffuseMultiplier, 'diff', "DiffMult", "Diffuse Multiplier", nullptr, 1.f);
		desc.AddMember(&ILightComponent::SColor::m_specularMultiplier, 'spec', "SpecMult", "Specular Multiplier", nullptr, 1.f);
	}

	static void ReflectType(Schematyc::CTypeDesc<ILightComponent::SShadows>& desc)
	{
		desc.SetGUID("{95F6EF06-2101-427C-9E55-481042117504}"_cry_guid);
		desc.AddMember(&ILightComponent::SShadows::m_castShadowSpec, 'shad', "ShadowSpec", "Minimum Shadow Graphics", "Minimum graphical setting to cast shadows from this light.", EMiniumSystemSpec::Disabled);
		desc.AddMember(&ILightComponent::SShadows::m_shadowBias, 'bias', "ShadowBias", "Shadow Bias", "Moves the shadow cascade toward or away from the shadow-casting object.", 1.0f);
		desc.AddMember(&ILightComponent::SShadows::m_shadowSlopeBias, 'sbia', "ShadowSlopeBias", "Shadow Slope Bias", "Allows you to adjust the gradient (slope-based) bias used to compute the shadow bias.", 1.0f);
		desc.AddMember(&ILightComponent::SShadows::m_shadowResolutionScale, 'srsc', "ShadownResolution", "Shadow Resolution Scale", "", 1.0f);
		desc.AddMember(&ILightComponent::SShadows::m_shadowUpdateMinRadius, 'sumr', "ShadowUpdateMin", "Shadow Min Update Radius", "Define the minimum radius from the light source to the player camera that the ShadowUpdateRatio setting will be ignored.", 4.0f);
		desc.AddMember(&ILightComponent::SShadows::m_shadowMinResolution, 'smin', "ShadowMinRes", "Shadow Min Resolution", "Percentage of the shadow pool the light should use for its shadows.", 0);
		desc.AddMember(&ILightComponent::SShadows::m_shadowUpdateRatio, 'sura', "ShadowUpdateRatio", "Shadow Update Ratio", "Define the update ratio for shadow maps cast from this light. The lower the value (example 0.01), the less frequent the updates will be and the more stuttering the shadow will appear.", 1);
	}

	static void ReflectType(Schematyc::CTypeDesc<ILightComponent>& desc)
	{
		desc.SetGUID("{684B1333-184B-4B28-9474-BEC205FD18A0}"_cry_guid);
	}

	static void ReflectType(Schematyc::CTypeDesc<ILightComponent::SAnimations>& desc)
	{
		desc.SetGUID("{95F6EF06-2101-427C-9E55-481042117504}"_cry_guid);
		desc.AddMember(&ILightComponent::SAnimations::m_style, 'styl', "Style", "Style", "Determines the light style to load, see Shaders/HWScripts/CryFX/Light.cfx for the full list", 0u);
		desc.AddMember(&ILightComponent::SAnimations::m_speed, 'sped', "Speed", "Speed", "Speed at which we animate", 1.f);
	}
}
}
