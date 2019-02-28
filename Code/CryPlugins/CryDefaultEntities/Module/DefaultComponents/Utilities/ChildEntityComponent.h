#pragma once

#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

class CPlugin_CryDefaultEntities;

namespace Cry
{
	namespace DefaultComponents
	{
		class CChildEntityComponent
			: public IEntityComponent
		{
		protected:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);

			// IEntityComponent
			virtual void Initialize() final;

			virtual void ProcessEvent(const SEntityEvent& event) final;
			virtual uint64 GetEventMask() const final;
			// ~IEntityComponent

		public:
			CChildEntityComponent() = default;
			virtual ~CChildEntityComponent();

			static void ReflectType(Schematyc::CTypeDesc<CChildEntityComponent>& desc)
			{
				desc.SetGUID("{BCB58434-4EA2-4C61-A8F7-A264A6B87636}"_cry_guid);
				desc.SetEditorCategory("Utilities");
				desc.SetLabel("Child Entity");
				desc.SetDescription("Spawns an entity of the specified type at creation");
				//desc.SetIcon("icons:General/Camera.ico");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });
				desc.AddMember(&CChildEntityComponent::m_className, 'clsn', "ClassName", "Entity Class", "Class of the entity we want to spawn", "");
				desc.AddMember(&CChildEntityComponent::m_bLinkTransformation, 'link', "LockTransform", "Lock Transformation", "Whether to lock the spawned entities transformation to its parent", false);
				desc.AddMember(&CChildEntityComponent::m_bIgnoreContactsWithChild, 'ign', "IgnoreChild", "Ignore Collisions With", "Whether to ignore all collisions with the child", true);
			}

			Schematyc::ExplicitEntityId GetChildEntityId() const { return Schematyc::ExplicitEntityId(m_childEntityId); }

		protected:
			void SpawnEntity();
			void RemoveEntity();
			void AddIgnoreConstraint();
			void RemoveIgnoreConstraint();

		protected:
			// WORKAROUND: Can't register the same ResourceSelector (EntityClassName) for both Schematyc versions.
			// Change this to EntityClassName when we finally have just one Schematyc.
			Schematyc::EntityClass m_className;
			// ~WORKAROUND

			bool m_bLinkTransformation = false;
			bool m_bIgnoreContactsWithChild = true;
			
			EntityId m_childEntityId = INVALID_ENTITYID;
			int m_constraintId = -1;
			int m_childConstraintId = -1;
		};
	}
}