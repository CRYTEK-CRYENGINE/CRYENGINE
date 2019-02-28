#include "StdAfx.h"
#include "VehicleComponent.h"

#include <Cry3DEngine/IRenderNode.h>

namespace Cry
{
	namespace DefaultComponents
	{
		void CVehiclePhysicsComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
		{
			// Functions
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CVehiclePhysicsComponent::UseHandbrake, "{12279EAD-5B52-49B0-B875-16195B04C103}"_cry_guid, "UseHandbrake");
				pFunction->SetDescription("Activates the handbrake");
				pFunction->BindInput(1, 'use', "Use", "Whether or not to use the handbrake");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CVehiclePhysicsComponent::IsUsingHandbrake, "{E9F98A9B-54C8-4D8B-BBBD-B7ADF231D233}"_cry_guid, "IsUsingHandbrake");
				pFunction->SetDescription("Checks whether the handbrake is active");
				pFunction->BindOutput(0, 'use', "Using", "Whether or not the handbrake is held");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CVehiclePhysicsComponent::SetThrottle, "{4F73C7FC-2D48-491C-AD5D-E86A0EB5262C}"_cry_guid, "SetThrottleRatio");
				pFunction->SetDescription("Sets the pedal ratio, 0 - 1 (0 being no pedal, 1 fully held down)");
				pFunction->BindInput(1, 'rati', "Ratio");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CVehiclePhysicsComponent::GetThrottle, "{DEBFA0B8-DB6F-4648-AF2A-2A4295A02338}"_cry_guid, "GetThrottleRatio");
				pFunction->SetDescription("Gets the current pedal ratio");
				pFunction->BindOutput(0, 'rati', "Ratio");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CVehiclePhysicsComponent::SetBrake, "{06D60C2A-48C2-4591-8F5E-EC33EBAB7CBA}"_cry_guid, "SetBrakeRatio");
				pFunction->SetDescription("Sets the brake ratio, 0 - 1 (0 being no brake, 1 fully held down)");
				pFunction->BindInput(1, 'rati', "Ratio");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CVehiclePhysicsComponent::GetBrake, "{2CE6F62B-EFD7-415E-8D2A-6EBE54B3E38A}"_cry_guid, "GetBrakeRatio");
				pFunction->SetDescription("Gets the current brke ratio");
				pFunction->BindOutput(0, 'rati', "Ratio");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CVehiclePhysicsComponent::SetCurrentGear, "{3031C561-42DC-4405-90A7-9DF6B965C3A2}"_cry_guid, "SetCurrentGear");
				pFunction->SetDescription("Switches to the specified gear (-1 reverse, 0 neutral, 1 and above is forward)");
				pFunction->BindInput(1, 'gear', "Gear");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CVehiclePhysicsComponent::GetCurrentGear, "{5878EC1E-0B95-4765-83ED-3336BF1EF4CB}"_cry_guid, "GetCurrentGear");
				pFunction->SetDescription("Gets the currently active gear (-1 reverse, 0 neutral, 1 and above is forward)");
				pFunction->BindOutput(0, 'gear', "Gear");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CVehiclePhysicsComponent::GearUp, "{CE4608AB-1B1A-4525-98D7-FF29301CBD7F}"_cry_guid, "GearUp");
				pFunction->SetDescription("Switched to the next gear");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CVehiclePhysicsComponent::GearDown, "{DE79FA1F-A7AB-4119-B2EF-A691A27EB1E3}"_cry_guid, "GearDown");
				pFunction->SetDescription("Switched to the previous gear");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CVehiclePhysicsComponent::SetSteeringAngle, "{105A1735-86D9-4B54-B7DA-D8D22DC52189}"_cry_guid, "SetSteeringAngle");
				pFunction->SetDescription("Sets the current steering angle");
				pFunction->BindInput(1, 'stee', "SteerAngle");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CVehiclePhysicsComponent::GetSteeringAngle, "{0CC76A47-93EA-4890-AAA4-05CEB1DC5CE5}"_cry_guid, "GetSteeringAngle");
				pFunction->SetDescription("Gets the current steering angle");
				pFunction->BindOutput(0, 'stee', "SteerAngle");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CVehiclePhysicsComponent::GetEngineRPM, "{1FCC083E-4EE7-4F19-BED1-B71B74DE96DE}"_cry_guid, "GetEngineRPM");
				pFunction->SetDescription("Gets the engine's current rotation speed");
				pFunction->BindOutput(0, 'rots', "RotationSpeed");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CVehiclePhysicsComponent::GetTorque, "{FBAA748C-FD06-4E29-9585-3B5DEC4826DC}"_cry_guid, "GetTorque");
				pFunction->SetDescription("Gets the driving torque");
				pFunction->BindOutput(0, 'torq', "Torque");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CVehiclePhysicsComponent::HasWheelContact, "{C3DC40DD-6D25-4A17-8182-DC68E0697C40}"_cry_guid, "HasWheelContact");
				pFunction->SetDescription("Checks whether one or more wheels is in contact with the ground");
				pFunction->BindOutput(0, 'cont', "Contact");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CVehiclePhysicsComponent::SetVelocity, "{09CAC615-3D68-44D2-A527-2530C40B7FF0}}"_cry_guid, "SetVelocity");
				pFunction->SetDescription("Set Entity Velocity");
				pFunction->BindInput(1, 'vel', "velocity");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CVehiclePhysicsComponent::GetVelocity, "{31463178-71C0-48AC-8A8B-3DEF8DF36A68}"_cry_guid, "GetVelocity");
				pFunction->SetDescription("Get Entity Velocity");
				pFunction->BindOutput(0, 'vel', "velocity");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CVehiclePhysicsComponent::SetAngularVelocity, "{F999C8FB-366F-4F0B-9BF0-CACB744DBF8C}"_cry_guid, "SetAngularVelocity");
				pFunction->SetDescription("Set Entity Angular Velocity");
				pFunction->BindInput(1, 'vel', "angular velocity");
				componentScope.Register(pFunction);
			}

			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CVehiclePhysicsComponent::GetAngularVelocity, "{1652D5A3-23D0-47E0-B224-112123272680}"_cry_guid, "GetAngularVelocity");
				pFunction->SetDescription("Get Entity Angular Velocity");
				pFunction->BindOutput(0, 'vel', "angular velocity");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CVehiclePhysicsComponent::ApplyImpulse, "{EEAA0C2C-1B26-4DC4-A649-1B70EC4FBD9A}"_cry_guid, "ApplyImpulse");
				pFunction->SetDescription("Applies an impulse to the physics object");
				pFunction->BindInput(1, 'for', "Force", "Force of the impulse", Vec3(ZERO));
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CVehiclePhysicsComponent::ApplyAngularImpulse, "{D869D9B5-0FDC-4942-B485-0622D1CA7156}"_cry_guid, "ApplyAngularImpulse");
				pFunction->SetDescription("Applies an angular impulse to the physics object");
				pFunction->BindInput(1, 'for', "Force", "Force of the impulse", Vec3(ZERO));
				componentScope.Register(pFunction);
			}
			// Signals
			{
				auto pSignal = SCHEMATYC_MAKE_ENV_SIGNAL(CVehiclePhysicsComponent::SCollisionSignal);
				pSignal->SetDescription("Sent when the entity collided with another object");
				componentScope.Register(pSignal);
			}
		}

		static void ReflectType(Schematyc::CTypeDesc<CVehiclePhysicsComponent::SCollisionSignal>& desc)
		{
			desc.SetGUID("{5E5E88FF-C466-4579-9F74-4E59DB3B2A47}"_cry_guid);
			desc.SetLabel("On Collision");
			desc.AddMember(&CVehiclePhysicsComponent::SCollisionSignal::otherEntity, 'ent', "OtherEntityId", "OtherEntityId", "Other Colliding Entity Id", INVALID_ENTITYID);
			desc.AddMember(&CVehiclePhysicsComponent::SCollisionSignal::surfaceType, 'srf', "SurfaceType", "SurfaceType", "Material Surface Type at the collision point", "");
		}

		CVehiclePhysicsComponent::CVehiclePhysicsComponent()
		{
			// Insert default gears
			m_gearParams.m_gears.EmplaceBack(SGearParams::SGear{ float((int)EGear::Reverse) });
			m_gearParams.m_gears.EmplaceBack(SGearParams::SGear{ float((int)EGear::Neutral) });
			m_gearParams.m_gears.EmplaceBack(SGearParams::SGear{ float((int)EGear::Forward) });
		}

		CVehiclePhysicsComponent::~CVehiclePhysicsComponent()
		{
			SEntityPhysicalizeParams physParams;
			physParams.type = PE_NONE;
			m_pEntity->Physicalize(physParams);
		}

		void CVehiclePhysicsComponent::Initialize()
		{
			SEntityPhysicalizeParams physParams;
			physParams.type = PE_WHEELEDVEHICLE;

			pe_params_car carParams;
			carParams.enginePower = m_engineParams.m_power;
			carParams.engineMaxRPM = m_engineParams.m_maxRPM;
			carParams.engineMinRPM = m_engineParams.m_minRPM;
			carParams.engineIdleRPM = m_engineParams.m_idleRPM;
			carParams.engineStartRPM = m_engineParams.m_startRPM;

			carParams.nGears = m_gearParams.m_gears.Size();
			carParams.engineShiftUpRPM = m_gearParams.m_shiftUpRPM;
			carParams.engineShiftDownRPM = m_gearParams.m_shiftDownRPM;

			m_gearRatios.resize(carParams.nGears);
			for (auto it = m_gearRatios.begin(); it != m_gearRatios.end(); ++it)
			{
				uint32 index = (uint32)std::distance(m_gearRatios.begin(), it);
				*it = m_gearParams.m_gears.At(index).m_ratio;
			}

			carParams.gearRatios = m_gearRatios.data();

			carParams.wheelMassScale = 1.f;
			physParams.pCar = &carParams;

			// Don't physicalize a slot by default
			physParams.nSlot = std::numeric_limits<int>::max();
			m_pEntity->Physicalize(physParams);

			UseHandbrake(false);
		}

		void CVehiclePhysicsComponent::ProcessEvent(const SEntityEvent& event)
		{
			if (event.event == ENTITY_EVENT_COLLISION)
			{
				// Collision info can be retrieved using the event pointer
				EventPhysCollision* physCollision = reinterpret_cast<EventPhysCollision*>(event.nParam[0]);

				const char* surfaceTypeName = "";
				EntityId otherEntityId = INVALID_ENTITYID;

				ISurfaceTypeManager* pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
				if (ISurfaceType* pSurfaceType = pSurfaceTypeManager->GetSurfaceType(physCollision->idmat[1]))
				{
					surfaceTypeName = pSurfaceType->GetName();
				}

				if (IEntity* pOtherEntity = gEnv->pEntitySystem->GetEntityFromPhysics(physCollision->pEntity[1]))
				{
					otherEntityId = pOtherEntity->GetId();
				}

				// Send OnCollision signal
				if (Schematyc::IObject* pSchematycObject = m_pEntity->GetSchematycObject())
				{
					pSchematycObject->ProcessSignal(SCollisionSignal(otherEntityId, Schematyc::SurfaceTypeName(surfaceTypeName)), GetGUID());
				}
			}
			else if (event.event == ENTITY_EVENT_UPDATE)
			{
				//pe_status_vehicle vehicleStatus;
				//m_pEntity->GetPhysicalEntity()->GetStatus(&vehicleStatus);
			}
			else if (event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
			{
				m_pEntity->UpdateComponentEventMask(this);

				// Validate gears
#ifndef RELEASE
				for (int i = 0, n = (int)EGear::DefaultCount; i < n; ++i)
				{
					if (m_gearParams.m_gears.Size() <= i || m_gearParams.m_gears.At(i).m_ratio != i)
					{
						m_gearParams.m_gears.Insert(i, SGearParams::SGear{ (float)i });
					}
				}
#endif
			}
		}

		uint64 CVehiclePhysicsComponent::GetEventMask() const 
		{
			uint64 bitFlags = ENTITY_EVENT_BIT(ENTITY_EVENT_UPDATE) | ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);
			if (m_bSendCollisionSignal)
			{
				bitFlags |= ENTITY_EVENT_BIT(ENTITY_EVENT_COLLISION);
			}

			return bitFlags;
		}
	}
}