#pragma once

#include "PhysicsPrimitiveComponent.h"

class CPlugin_CryDefaultEntities;

namespace Cry
{
	namespace DefaultComponents
	{
		class CCylinderPrimitiveComponent
			: public CPhysicsPrimitiveComponent
		{
		protected:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);

		public:
			static void ReflectType(Schematyc::CTypeDesc<CCylinderPrimitiveComponent>& desc)
			{
				desc.SetGUID("{00B840E8-FEFF-45B0-8C1E-B3B3F1D9E0EE}"_cry_guid);
				desc.SetEditorCategory("Physics");
				desc.SetLabel("Cylinder Collider");
				desc.SetDescription("Adds a cylinder collider to the physical entity, requires a Simple Physics or Character Controller component.");
				desc.SetIcon("icons:ObjectTypes/object.ico");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });

				desc.AddMember(&CCylinderPrimitiveComponent::m_radius, 'radi', "Radius", "Radius", "Radius of the cylinder", 0.5f);
				desc.AddMember(&CCylinderPrimitiveComponent::m_height, 'heig', "Height", "Height", "Height of the cylinder", 1.f);

				desc.AddMember(&CCylinderPrimitiveComponent::m_physics, 'phys', "Physics", "Physics Settings", "Physical properties for the object, only used if a simple physics or character controller is applied to the entity.", SPhysicsParameters());

				desc.AddMember(&CCylinderPrimitiveComponent::m_surfaceTypeName, 'surf', "Surface", "Surface Type", "Surface type assigned to this object, determines its physical properties", "");
				desc.AddMember(&CCylinderPrimitiveComponent::m_bReactToCollisions, 'coll', "ReactToCollisions", "React to Collisions", "Whether the part will react to collisions, or only report them", true);
			}

			CCylinderPrimitiveComponent() {}
			virtual ~CCylinderPrimitiveComponent() {}

			virtual IGeometry* CreateGeometry() const final
			{
				primitives::cylinder primitive;
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