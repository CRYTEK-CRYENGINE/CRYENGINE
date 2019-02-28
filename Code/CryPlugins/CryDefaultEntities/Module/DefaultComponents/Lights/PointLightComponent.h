#pragma once
#include "ILightComponent.h"

class CPlugin_CryDefaultEntities;

namespace Cry
{
	namespace DefaultComponents
	{
		class CPointLightComponent
			: public ILightComponent
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

			virtual void SetOptics(const char* szOpticsFullName) override
			{
				m_optics.m_lensFlareName = szOpticsFullName;

				Initialize();
			}

			virtual void Enable(bool enable) override;

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

				desc.AddMember(&CPointLightComponent::m_optics, 'opti', "Optics", "Optics", "Specific Optic Options", CPointLightComponent::SOptics());
				desc.AddMember(&CPointLightComponent::m_color, 'colo', "Color", "Color", "Color emission information", CPointLightComponent::SColor());
				desc.AddMember(&CPointLightComponent::m_shadows, 'shad', "Shadows", "Shadows", "Shadow casting settings", CPointLightComponent::SShadows());
				desc.AddMember(&CPointLightComponent::m_options, 'opt', "Options", "Options", "Specific Light Options", CPointLightComponent::SOptions());
				desc.AddMember(&CPointLightComponent::m_animations, 'anim', "Animations", "Animations", "Light style / animation properties", CPointLightComponent::SAnimations());
			}

		protected:
			Schematyc::Range<0, 32768> m_radius = 10.f;
			Schematyc::Range<0, 100, 0, 100, int> m_viewDistance = 50;
		};
	}
}