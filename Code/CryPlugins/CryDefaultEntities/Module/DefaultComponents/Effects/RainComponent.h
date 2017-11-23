#pragma once
#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <Cry3DEngine/I3DEngine.h>
#include <limits>

class CPlugin_CryDefaultEntities;

namespace Cry
{
	namespace DefaultComponents
	{
		class CRainComponent
			: public IEntityComponent
		{
		public:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);

			// IEntityComponent
			virtual void Initialize() final;
			virtual void ProcessEvent(const SEntityEvent& event) final;
			virtual uint64 GetEventMask() const final;
			// ~IEntityComponent

			virtual ~CRainComponent();

			static void ReflectType(Schematyc::CTypeDesc<CRainComponent>& desc)
			{
				desc.SetGUID("{A161B0DA-E215-45AC-9EF5-06E9304F2378}"_cry_guid);
				desc.SetEditorCategory("Effects");
				desc.SetLabel("Rain");
				desc.SetDescription("Adds an rain effect in the radius of the entity");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly });
				desc.AddMember(&CRainComponent::m_bEnable, 'ena', "Enable", "Enable", "Enables or disables the rain", true);
				desc.AddMember(&CRainComponent::m_color, 'col', "Color", "Color", "Color of the rain", ColorF(1.0f));
				desc.AddMember(&CRainComponent::m_amount, 'amo', "Amount", "Amount", "Amount of rain", 1.0f);
				desc.AddMember(&CRainComponent::m_ignoreVisareas, 'vis', "IgnoreVisareas", "Ignore Visareas", "Defines if the rain should ignore visareas", false);
				desc.AddMember(&CRainComponent::m_disableOcclusion, 'dis', "DisableOcclusion", "Disable Occlusion", "Enables or disables occlusion", false);
				desc.AddMember(&CRainComponent::m_radius, 'rad', "Radius", "Radius", "Radius of the rain area", 1000.f);
				desc.AddMember(&CRainComponent::m_fakeGlossiness, 'glo', "Glossiness", "Glossiness", "Fake glossiness", 1.0f);
				desc.AddMember(&CRainComponent::m_fakeReflectionAmount, 'ref', "ReflectionAmount", "Reflection Amount", "Reflection amount", 1.0f);
				desc.AddMember(&CRainComponent::m_diffuseDarkening, 'dar', "DiffuseDarkening", "Diffuse Darkening", "Diffuse darking of the rain", 1.0f);
				desc.AddMember(&CRainComponent::m_rainDropsAmount, 'rai', "RainDropsAmount", "Rain Drops Amount", "Amount of rain drops falling", 1.0f);
				desc.AddMember(&CRainComponent::m_rainDropsSpeed, 'ras', "RainDropsSpeed", "Rain Drops Speed", "Falling speed of the rain drops", 1.0f);
				desc.AddMember(&CRainComponent::m_rainDropsLighting, 'ral', "RainDropsLighting", "Rain Drops Lighting", "Lighting of the rain drops", 1.0f);
				desc.AddMember(&CRainComponent::m_mistAmount, 'mia', "MistAmount", "Mist Amount", "Amount of the mist", 1.0f);
				desc.AddMember(&CRainComponent::m_mistHeight, 'mih', "MistHeight", "Mist Height", "Height of the mist", 1.0f);
				desc.AddMember(&CRainComponent::m_puddlesAmount, 'pua', "PuddlesAmount", "Puddles Amount", "Amount of puddles", 1.0f);
				desc.AddMember(&CRainComponent::m_puddlesMaskAmount, 'pma', "PuddlesMaskAmount", "Puddles Mask Amount", "Amount of puddles masks", 1.0f);
				desc.AddMember(&CRainComponent::m_puddlesRippleAmount, 'pra', "PuddlesRippleAmount", "Puddles Ripple Amount", "Amount of ripples on the puddles surface", 1.0f);
				desc.AddMember(&CRainComponent::m_splashesAmount, 'spa', "SplashesAmount", "SplashesAmount", "Amount splashes on surfaces", 1.0f);
			}

			void Enable(bool bEnable);
			bool IsEnabled() const;

		protected:
			void PreloadTextures();

			void Reset();

			void SetParams(SRainParams& params);

		protected:
			typedef std::vector<ITexture*> TTextureList;
			TTextureList m_Textures;

			bool m_bEnable = true;

			ColorF m_color = ColorF(1.0f);

			Schematyc::Range<0, 100, 0, 100, float> m_amount = 1.0f;
			Schematyc::Range< 0, 10000, 0, 10000, float> m_radius = 1000.f;

			Schematyc::Range<0, 100, 0, 100, float> m_fakeGlossiness = 1.0f;
			Schematyc::Range<0, 1000, 0, 1000, float> m_fakeReflectionAmount = 1.0f;
			Schematyc::Range<0, 1, 0, 1, float> m_diffuseDarkening = 0.5f;

			Schematyc::Range<0, 100, 0, 100, float> m_rainDropsAmount = 1.0f;
			Schematyc::Range<0, 100, 0, 100, float> m_rainDropsSpeed = 1.0f;
			Schematyc::Range<0, 100, 0, 100, float> m_rainDropsLighting = 1.0f;

			Schematyc::Range<0, 1000, 0, 1000, float> m_mistAmount = 1.0f;
			Schematyc::Range<0, 100, 0, 100, float> m_mistHeight = 1.0f;

			Schematyc::Range<0, 10, 0, 10, float> m_puddlesAmount = 1.0f;
			Schematyc::Range<0, 1, 0, 1, float> m_puddlesMaskAmount = 1.0f;
			Schematyc::Range<0, 100, 0, 100, float> m_puddlesRippleAmount = 1.0f;
			Schematyc::Range<0, 1000, 0, 1000, float> m_splashesAmount = 1.0f;

			bool m_ignoreVisareas = false;
			bool m_disableOcclusion = false;
		};
	}
}
