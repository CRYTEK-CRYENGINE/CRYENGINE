#pragma once

#include "DefaultComponents/Geometry/BaseMeshComponent.h"

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>

#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

class CPlugin_CryDefaultEntities;

namespace Cry
{
	namespace DefaultComponents
	{
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

		class CPointLightComponent
			: public IEntityComponent
#ifndef RELEASE
			, public IEntityComponentPreviewer
#endif
		{
		protected:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);

			// IEntityComponent
			virtual void Initialize() final;

			virtual void   ProcessEvent(const SEntityEvent& event) final;
			virtual uint64 GetEventMask() const final;

#ifndef RELEASE
			virtual IEntityComponentPreviewer* GetPreviewer() final { return this; }
#endif
			// ~IEntityComponent

#ifndef RELEASE
			// IEntityComponentPreviewer
			virtual void SerializeProperties(Serialization::IArchive& archive) final {}

			virtual void Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const final;
			// ~IEntityComponentPreviewer
#endif

		public:
			CPointLightComponent() {}
			virtual ~CPointLightComponent() {}

			static void ReflectType(Schematyc::CTypeDesc<CPointLightComponent>& desc)
			{
				desc.SetGUID("0A86908D-642F-4590-ACEF-484E8E39F31B"_cry_guid);
				desc.SetEditorCategory("Lights");
				desc.SetLabel("Point Light");
				desc.SetDescription("Emits light from its origin into all directions");
				desc.SetIcon("icons:ObjectTypes/light.ico");
				desc.SetComponentFlags({ IEntityComponent::EFlags::NoCreationOffset, IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly });

				desc.AddMember(&CPointLightComponent::m_bActive, 'actv', "Active", "Active", "Determines whether the light is enabled", true);
				desc.AddMember(&CPointLightComponent::m_radius, 'radi', "Radius", "Radius", "Determines the range of the point light", 10.f);
				desc.AddMember(&CPointLightComponent::m_viewDistance, 'view', "ViewDistance", "View Distance", "Determines the distance in which this light will be active", 50);

				desc.AddMember(&CPointLightComponent::m_color, 'colo', "Color", "Color", "Color emission information", CPointLightComponent::SColor());
				desc.AddMember(&CPointLightComponent::m_shadows, 'shad', "Shadows", "Shadows", "Shadow casting settings", CPointLightComponent::SShadows());
				desc.AddMember(&CPointLightComponent::m_options, 'opt', "Options", "Options", "Specific Light Options", CPointLightComponent::SOptions());
				desc.AddMember(&CPointLightComponent::m_animations, 'anim', "Animations", "Animations", "Light style / animation properties", CPointLightComponent::SAnimations());
			}

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

			struct SShadows
			{
				inline bool operator==(const SShadows &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

				EMiniumSystemSpec m_castShadowSpec = EMiniumSystemSpec::Disabled;
				float m_shadowBias = 1.f;
				float m_shadowSlopeBias = 1.f;
				float m_shadowResolutionScale = 1.f;
				float m_shadowUpdateMinRadius = 4.f;
				int m_shadowMinResolution = 0;
				int m_shadowUpdateRatio = 1;
			};

			struct SAnimations
			{
				inline bool operator==(const SAnimations &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

				uint32 m_style = 0;
				float m_speed = 1.f;
			};

			virtual void Enable(bool bEnable);
			bool IsEnabled() const { return m_bActive; }

			virtual SOptions& GetOptions() { return m_options; }
			const SOptions& GetOptions() const { return m_options; }

			virtual SColor& GetColorParameters() { return m_color; }
			const SColor& GetColorParameters() const { return m_color; }

			virtual SShadows& GetShadowParameters() { return m_shadows; }
			const SShadows& GetShadowParameters() const { return m_shadows; }

			virtual SAnimations& GetAnimationParameters() { return m_animations; }
			const SAnimations& GetAnimationParameters() const { return m_animations; }

		protected:
			bool m_bActive = true;
			Schematyc::Range<0, 32768> m_radius = 10.f;

			Schematyc::Range<0, 100, 0, 100, int> m_viewDistance = 50;
			SOptions m_options;
			SColor m_color;
			SShadows m_shadows;
			SAnimations m_animations;
		};

		static void ReflectType(Schematyc::CTypeDesc<CPointLightComponent::SOptions>& desc)
		{
			desc.SetGUID("{DB10AB64-7A5B-4B91-BC90-6D692D1D1222}"_cry_guid);
			desc.AddMember(&CPointLightComponent::SOptions::m_attenuationBulbSize, 'atte', "AttenuationBulbSize", "Attenuation Bulb Size", "Controls the fall-off exponentially from the origin, a value of 1 means that the light is at full intensity within a 1 meter ball before it begins to fall-off.", SRenderLight().m_fAttenuationBulbSize);
			desc.AddMember(&CPointLightComponent::SOptions::m_bIgnoreVisAreas, 'igvi', "IgnoreVisAreas", "Ignore VisAreas", nullptr, false);
			desc.AddMember(&CPointLightComponent::SOptions::m_bAffectsVolumetricFog, 'volf', "AffectVolumetricFog", "Affect Volumetric Fog", nullptr, true);
			desc.AddMember(&CPointLightComponent::SOptions::m_bVolumetricFogOnly, 'volo', "VolumetricFogOnly", "Only Affect Volumetric Fog", nullptr, false);
			desc.AddMember(&CPointLightComponent::SOptions::m_bAffectsOnlyThisArea, 'area', "OnlyAffectThisArea", "Only Affect This Area", nullptr, true);
			desc.AddMember(&CPointLightComponent::SOptions::m_bLinkToSkyColor, 'ltsc', "LinkToSkyColor", "Link To Sky Color", "Multiply light color with current sky color (use GI sky color if available).", false);
			desc.AddMember(&CPointLightComponent::SOptions::m_bAmbient, 'ambi', "Ambient", "Ambient", nullptr, false);
			desc.AddMember(&CPointLightComponent::SOptions::m_fogRadialLobe, 'fogr', "FogRadialLobe", "Fog Radial Lobe", nullptr, SRenderLight().m_fFogRadialLobe);
			desc.AddMember(&CPointLightComponent::SOptions::m_giMode, 'gimo', "GIMode", "Global Illumination", nullptr, ELightGIMode::Disabled);
		}

		static 	void ReflectType(Schematyc::CTypeDesc<CPointLightComponent::SColor>& desc)
		{
			desc.SetGUID("{B71C3414-F85A-4EAA-9CE0-5110A2E040AD}"_cry_guid);
			desc.AddMember(&CPointLightComponent::SColor::m_color, 'col', "Color", "Color", nullptr, ColorF(1.f));
			desc.AddMember(&CPointLightComponent::SColor::m_diffuseMultiplier, 'diff', "DiffMult", "Diffuse Multiplier", nullptr, 1.f);
			desc.AddMember(&CPointLightComponent::SColor::m_specularMultiplier, 'spec', "SpecMult", "Specular Multiplier", nullptr, 1.f);
		}

		static void ReflectType(Schematyc::CTypeDesc<CPointLightComponent::SShadows>& desc)
		{
			desc.SetGUID("{95F6EF06-2101-427C-9E55-481042117504}"_cry_guid);
			desc.AddMember(&CPointLightComponent::SShadows::m_castShadowSpec, 'shad', "ShadowSpec", "Minimum Shadow Graphics", "Minimum graphical setting to cast shadows from this light.", EMiniumSystemSpec::Disabled);
			desc.AddMember(&CPointLightComponent::SShadows::m_shadowBias, 'bias', "ShadowBias", "Shadow Bias", "Moves the shadow cascade toward or away from the shadow-casting object.", 1.0f);
			desc.AddMember(&CPointLightComponent::SShadows::m_shadowSlopeBias, 'sbia', "ShadowSlopeBias", "Shadow Slope Bias", "Allows you to adjust the gradient (slope-based) bias used to compute the shadow bias.", 1.0f);
			desc.AddMember(&CPointLightComponent::SShadows::m_shadowResolutionScale, 'srsc', "ShadownResolution", "Shadow Resolution Scale", "", 1.0f);
			desc.AddMember(&CPointLightComponent::SShadows::m_shadowUpdateMinRadius, 'sumr', "ShadowUpdateMin", "Shadow Min Update Radius", "Define the minimum radius from the light source to the player camera that the ShadowUpdateRatio setting will be ignored.", 4.0f);
			desc.AddMember(&CPointLightComponent::SShadows::m_shadowMinResolution, 'smin', "ShadowMinRes", "Shadow Min Resolution", "Percentage of the shadow pool the light should use for its shadows.", 0);
			desc.AddMember(&CPointLightComponent::SShadows::m_shadowUpdateRatio, 'sura', "ShadowUpdateRatio", "Shadow Update Ratio", "Define the update ratio for shadow maps cast from this light. The lower the value (example 0.01), the less frequent the updates will be and the more stuttering the shadow will appear.", 1);
		}

		static void ReflectType(Schematyc::CTypeDesc<CPointLightComponent::SAnimations>& desc)
		{
			desc.SetGUID("{95F6EF06-2101-427C-9E55-481042117504}"_cry_guid);
			desc.AddMember(&CPointLightComponent::SAnimations::m_style, 'styl', "Style", "Style", "Determines the light style to load, see Shaders/HWScripts/CryFX/Light.cfx for the full list", 0u);
			desc.AddMember(&CPointLightComponent::SAnimations::m_speed, 'sped', "Speed", "Speed", "Speed at which we animate", 1.f);
		}
	}
}