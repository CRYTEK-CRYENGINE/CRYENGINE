#pragma once

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>

#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryPhysics/physinterface.h>

struct EventPhysPostStep;

namespace Cry
{
	namespace DefaultComponents
	{
		class CSampleActorComponent	: public IEntityComponent
		{
		protected:
			// IEntityComponent

			virtual void Initialize() final { Physicalize(); }

			virtual void ProcessEvent(const SEntityEvent& event) final
			{
				switch (event.event)
				{
					case ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED: m_pEntity->UpdateComponentEventMask(this); break;
					case ENTITY_EVENT_PHYS_POSTSTEP: OnPostStep(event.fParam[0]); break;
				}
			}

			virtual Cry::Entity::EventFlags GetEventMask() const final	{	return ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED | ENTITY_EVENT_PHYS_POSTSTEP; }

			// ~IEntityComponent

		public:
			CSampleActorComponent() {}
			virtual ~CSampleActorComponent();

			static void Register(Schematyc::CEnvRegistrationScope& componentScope);
			static void ReflectType(Schematyc::CTypeDesc<CSampleActorComponent>& desc)
			{
				desc.SetGUID(CSampleActorComponent::IID());
				desc.SetEditorCategory("Physics");
				desc.SetLabel("Sample Rigidbody Actor");
				desc.SetDescription("Very basic sample support for the new actor (walking rigid) entity in physics");
				//desc.SetIcon("icons:ObjectTypes/object.ico");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });

				desc.AddMember(&CSampleActorComponent::m_friction, 'fric', "Friction", "Friction", "Ground friction when standing still", 1.0f);
				desc.AddMember(&CSampleActorComponent::m_minMass, 'gmas', "MinGroundMass", "MinGroundMass", "Only check ground colliders with this or higher masses", 1.0f);
				desc.AddMember(&CSampleActorComponent::m_legStiffness, 'stif', "LegStiffness", "LegStiffness", "Leg stiffness", 10.0f);
			}


			static CryGUID& IID()
			{
				static CryGUID id = "{72A9B995-078C-468a-80D2-09BD10FE44D5}"_cry_guid;
				return id;
			}

			virtual void SetMoveVel(const Vec3& vel) { m_velMove = vel; OnInput(); }
			virtual Vec3 GetMoveVel() const          { return m_velMove; }

			virtual void Jump(float height) 
			{ 
				Vec3 g = gEnv->pPhysicalWorld->GetPhysVars()->gravity;
				m_velJump = -g * (height / sqrt(g.len())); 
				OnInput();
			}

			void Physicalize();
			void SetupLegs(bool immediately = false);
			void OnInput();
			int  OnPostStep(float deltaTime);

			float m_friction     = 1.0f;
			float m_minMass      = 1.0f;
			float m_legStiffness = 10.0f;

		protected:
			Vec3  m_velMove = Vec3(ZERO);
			Vec3  m_velJump = Vec3(ZERO);
			float m_timeFly = 0.0f;
		};
	}
}