#pragma once

#include "PhysicsPrimitiveComponent.h"

class CPlugin_CryDefaultEntities;

namespace Cry
{
	namespace DefaultComponents
	{
		class CCapsulePrimitiveComponent
			: public CPhysicsPrimitiveComponent
		{
		protected:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);
			
		public:
			static void ReflectType(Schematyc::CTypeDesc<CCapsulePrimitiveComponent>& desc)
			{
				desc.SetGUID("{6657F34C-D32A-469F-B9E3-62C7A0A0CCF7}"_cry_guid);
				desc.SetEditorCategory("Physics");
				desc.SetLabel("Capsule Collider");
				desc.SetDescription("Adds a capsule collider to the physical entity, requires a Simple Physics or Character Controller component.");
				desc.SetIcon("icons:ObjectTypes/object.ico");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });

				desc.AddMember(&CCapsulePrimitiveComponent::m_radius, 'radi', "Radius", "Radius", "Radius of the capsule", 0.5f);
				desc.AddMember(&CCapsulePrimitiveComponent::m_height, 'heig', "Height", "Height", "Height of the capsule", 1.f);

				desc.AddMember(&CCapsulePrimitiveComponent::m_physics, 'phys', "Physics", "Physics Settings", "Physical properties for the object, only used if a simple physics or character controller is applied to the entity.", SPhysicsParameters());

				desc.AddMember(&CCapsulePrimitiveComponent::m_surfaceTypeName, 'surf', "Surface", "Surface Type", "Surface type assigned to this object, determines its physical properties", "");
				desc.AddMember(&CCapsulePrimitiveComponent::m_bReactToCollisions, 'coll', "ReactToCollisions", "React to Collisions", "Whether the part will react to collisions, or only report them", true);
			}

			CCapsulePrimitiveComponent() {}
			virtual ~CCapsulePrimitiveComponent() {}

			virtual IGeometry* CreateGeometry() const final
			{
				primitives::capsule primitive;
				primitive.center = ZERO;
				primitive.axis = Vec3(0, 0, 1);
				primitive.r = m_radius;
				primitive.hh = m_height * 0.5f;

				return gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive((int)primitive.type, &primitive);
			}

			Schematyc::Range<0, 1000000> m_radius = 0.5f;
			Schematyc::Range<0, 1000000> m_height = 1.f;
		};
	}
}