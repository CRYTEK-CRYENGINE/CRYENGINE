#pragma once

#include "DefaultComponents/Physics/PhysicsPrimitiveComponent.h"
#include "VehicleComponent.h"

namespace Cry
{
	namespace DefaultComponents
	{
		class CWheelComponent
			: public CPhysicsPrimitiveComponent
		{
		protected:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);

		public:
			CWheelComponent() {}
			virtual ~CWheelComponent() {}

			// CPhysicsPrimitiveComponent
			virtual std::unique_ptr<pe_geomparams> GetGeomParams() const final 
			{
				std::unique_ptr<pe_geomparams> pParams = stl::make_unique<pe_cargeomparams>();
				pe_cargeomparams* pWheelParams = static_cast<pe_cargeomparams*>(pParams.get());
				pWheelParams->bDriving = 1;
				pWheelParams->flagsCollider = geom_colltype_vehicle;
				pWheelParams->flags &= ~geom_floats;
				pWheelParams->iAxle = m_axleIndex;
				pWheelParams->bDriving = m_driving;
				pWheelParams->bCanBrake = m_handBrake;
				pWheelParams->pivot = m_pTransform->ToMatrix34() * m_suspensionPivot + Vec3(0, 0, m_suspensionLength);
				pWheelParams->lenMax = m_suspensionLength;
				pWheelParams->lenInitial = m_suspensionLengthComp;
				pWheelParams->kStiffness = 0.f;
				pWheelParams->kDamping = -m_damping;
				pWheelParams->bRayCast = m_bRaycast ? 1 : 0;

				return pParams;
			}

			virtual IGeometry* CreateGeometry() const final
			{
				primitives::cylinder primitive;
				primitive.center = ZERO;
				primitive.axis = Vec3(1, 0, 0);
				primitive.r = m_radius;
				primitive.hh = m_height * 0.5f;

				return gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive((int)primitive.type, &primitive);
			}
			// ~CPhysicsPrimitiveComponent

			static void ReflectType(Schematyc::CTypeDesc<CWheelComponent>& desc)
			{
				desc.SetGUID("{1988C96E-194F-48F4-B54D-467A851AB728}"_cry_guid);
				desc.SetEditorCategory("Physics");
				desc.SetLabel("Wheel Collider");
				desc.SetDescription("Adds a wheel collider to the vehicle.");
				desc.SetIcon("icons:ObjectTypes/object.ico");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });
				desc.AddComponentInteraction<CVehiclePhysicsComponent>(SEntityComponentRequirements::EType::HardDependency);

				desc.AddMember(&CWheelComponent::m_radius, 'radi', "Radius", "Radius", "Radius of the cylinder", 0.5f);
				desc.AddMember(&CWheelComponent::m_height, 'heig', "Height", "Width", "Width of the cylinder", 0.1f);

				desc.AddMember(&CWheelComponent::m_physics, 'phys', "Physics", "Physics Settings", "Physical properties for the object, only used if a simple physics or character controller is applied to the entity.", SPhysicsParameters());

				desc.AddMember(&CWheelComponent::m_surfaceTypeName, 'surf', "Surface", "Surface Type", "Surface type assigned to this object, determines its physical properties", "");
				desc.AddMember(&CWheelComponent::m_axleIndex, 'axle', "AxleIndex", "Axle Index", nullptr, 0);
				desc.AddMember(&CWheelComponent::m_driving, 'driv', "Driving", "Driving", "Whether the wheel is driving", true);
				desc.AddMember(&CWheelComponent::m_handBrake, 'brak', "HandBrake", "Hand Brake", "Whether the wheel is locked by hand brake", true);
				
				desc.AddMember(&CWheelComponent::m_suspensionLength, 'susp', "SuspensionLength", "Suspension Length", nullptr, 0.5f);
				desc.AddMember(&CWheelComponent::m_suspensionLengthComp, 'sspc', "SuspensionLength0", "Initial (Compressed) Suspension Length", nullptr, 0.3f);
				desc.AddMember(&CWheelComponent::m_suspensionPivot, 'pivt', "Pivot", "Suspension Pivot", "Suspension attachment point on the wheel, offset from the wheel's center in the entity space", Vec3(ZERO));
				desc.AddMember(&CWheelComponent::m_damping, 'damp', "Damping", "Suspension Damping", "Damping of the suspension spring, normalized to 0..1 range", 0.7f);
				desc.AddMember(&CWheelComponent::m_bRaycast, 'rayc', "RayCast", "Ray Cast", "Whether to cast a ray downwards to find collisions, or to check for mesh intersections", false);
			}

			virtual void SetAxleIndex(int index) { m_axleIndex = index; }

#ifndef RELEASE
			// IEntityComponentPreviewer
			virtual void Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const final
			{
				if (context.bSelected)
				{
					if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysicalEntity())
					{
						pe_status_wheel sw;
						pe_status_pos sp;
						sw.partid = sp.partid = m_pEntity->GetPhysicalEntityPartId0(GetEntitySlotId());
						if (pPhysicalEntity->GetStatus(&sw) && pPhysicalEntity->GetStatus(&sp))
						{
							Vec3 pivot = m_pEntity->GetWorldTM() * (m_pTransform->ToMatrix34() * m_suspensionPivot + Vec3(0, 0, m_suspensionLengthComp));
							Quat qEnt = m_pEntity->GetRotation();
							Vec3 pt0 = pivot, pt1, r(sw.suspLenFull * 0.1f, 0, 0);
							for(int i = 0; i < 80; i++, pt0 = pt1)
							{
								r.Set((r.x - r.y) / (float)sqrt2, (r.x + r.y) / (float)sqrt2, r.z + sw.suspLen / 80.0f);
								pt1 = pivot - qEnt * r;
								gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(pt0, context.debugDrawInfo.color, pt1, context.debugDrawInfo.color);
							}
							geom_world_data gwd;
							gwd.offset = m_pEntity->GetWorldTM() * (m_pTransform->GetTranslation() + Vec3(0, 0, sw.suspLen0 - sw.suspLen));
							gwd.R = Matrix33(sp.q);
							gwd.scale = sp.scale;
							gEnv->pSystem->GetIPhysRenderer()->DrawGeometry(sp.pGeom, &gwd, -1, 0, ZERO, context.debugDrawInfo.color);
						}
					}
				}
			}
			// ~IEntityComponentPreviewer
#endif

			Schematyc::Range<0, 1000000> m_radius = 0.5f;
			Schematyc::Range<0, 1000000> m_height = 0.1f;

			int m_axleIndex = 0;
			bool m_driving = true;
			bool m_handBrake = true;
			float m_suspensionLength = 0.5f;
			float m_suspensionLengthComp = 0.3f;
			Vec3 m_suspensionPivot = Vec3(ZERO);
			Schematyc::Range<0, 2> m_damping = 0.7f;

			bool m_bRaycast = false;
		};
	}
}