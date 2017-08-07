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

				desc.AddMember(&CCylinderPrimitiveComponent::m_mass, 'mass', "Mass", "Mass", "Mass of the object in kg, note that this cannot be set at the same time as density. Both being set to 0 means no physics.", 10.f);
				desc.AddMember(&CCylinderPrimitiveComponent::m_density, 'dens', "Density", "Density", "Density of the object, note that this cannot be set at the same time as mass. Both being set to 0 means no physics.", 0.f);

				desc.AddMember(&CCylinderPrimitiveComponent::m_surfaceTypeName, 'surf', "Surface", "Surface Type", "Surface type assigned to this object, determines its physical properties", "");
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