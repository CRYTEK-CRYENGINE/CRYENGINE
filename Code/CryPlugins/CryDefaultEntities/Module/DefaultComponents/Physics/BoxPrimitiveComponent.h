// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "PhysicsPrimitiveComponent.h"

class CPlugin_CryDefaultEntities;

namespace Cry
{
	namespace DefaultComponents
	{
		class CBoxPrimitiveComponent
			: public CPhysicsPrimitiveComponent
		{
		protected:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);
			
		public:
			static void ReflectType(Schematyc::CTypeDesc<CBoxPrimitiveComponent>& desc)
			{
				desc.SetGUID("{52431C11-77EE-410A-A7A7-A7FA69E79685}"_cry_guid);
				desc.SetEditorCategory("Physics");
				desc.SetLabel("Box Collider");
				desc.SetDescription("Adds a box primitive to the physical entity, requires a Simple Physics or Character Controller component.");
				desc.SetIcon("icons:ObjectTypes/object.ico");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });

				desc.AddMember(&CBoxPrimitiveComponent::m_size, 'size', "Size", "Size", "Size of the box", Vec3(1, 1, 1));

				desc.AddMember(&CBoxPrimitiveComponent::m_physics, 'phys', "Physics", "Physics Settings", "Physical properties for the object, only used if a simple physics or character controller is applied to the entity.", SPhysicsParameters());

				desc.AddMember(&CBoxPrimitiveComponent::m_surfaceTypeName, 'surf', "Surface", "Surface Type", "Surface type assigned to this object, determines its physical properties", "");
				desc.AddMember(&CBoxPrimitiveComponent::m_bReactToCollisions, 'coll', "ReactToCollisions", "React to Collisions", "Whether the part will react to collisions, or only report them", true);
			}

			CBoxPrimitiveComponent() {}
			virtual ~CBoxPrimitiveComponent() {}

			virtual IGeometry* CreateGeometry() const final
			{
				primitives::box primitive;
				primitive.size = m_size;
				primitive.Basis = IDENTITY;
				primitive.bOriented = 1;
				primitive.center = Vec3(0, 0, m_size.z);

				return gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive((int)primitive.type, &primitive);
			}

			Vec3 m_size = Vec3(1, 1, 1);
		};
	}
}