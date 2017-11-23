#pragma once

#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

class CPlugin_CryDefaultEntities;

namespace Cry
{
	namespace DefaultComponents
	{
		class CThrusterComponent
			: public IEntityComponent
#ifndef RELEASE
			, public IEntityComponentPreviewer
#endif
		{
		protected:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);

		public:
			static void ReflectType(Schematyc::CTypeDesc<CThrusterComponent>& desc)
			{
				desc.SetGUID("{B3222B05-BE90-4E19-B0A8-C0250F62AC98}"_cry_guid);
				desc.SetEditorCategory("Physics");
				desc.SetLabel("Thruster");
				desc.SetDescription("Allows for applying thrust on a specific point, requires a Simple Physics or Character Controller component.");
				desc.SetIcon("icons:ObjectTypes/object.ico");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });

				desc.AddMember(&CThrusterComponent::m_bEnableConstantThrustByDefault, 'enab', "EnableConstantThrust", "Constant", "Whether to continuously apply the thrust each frame", true);
				desc.AddMember(&CThrusterComponent::m_constantThrust, 'cons', "ConstantThrust", "Constant Thrust", "The impulse to apply every frame if constant thrust is enabled", 1.f);
			}

			CThrusterComponent() {}
			virtual ~CThrusterComponent() {}

			// IEntityComponent
			virtual void Initialize() final;

			virtual void ProcessEvent(const SEntityEvent& event) final;
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

			virtual void EnableConstantThrust(bool bEnable);
			virtual bool IsConstantThrustEnabled() const { return m_bConstantThrustActive; }

			virtual void ApplySingleThrust(float thrust);
			
		protected:
			bool m_bEnableConstantThrustByDefault = true;
			float m_constantThrust = 1;

			bool m_bConstantThrustActive = true;
		};
	}
}