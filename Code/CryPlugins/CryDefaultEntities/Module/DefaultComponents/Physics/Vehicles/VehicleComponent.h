#pragma once

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>

#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>

#include <CryMath/Angle.h>

namespace Cry
{
	namespace DefaultComponents
	{
		class CVehiclePhysicsComponent
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
			struct SCollisionSignal
			{
				EntityId otherEntity = INVALID_ENTITYID;
				Schematyc::SurfaceTypeName surfaceType;

				SCollisionSignal() {};
				SCollisionSignal(EntityId id, const Schematyc::SurfaceTypeName &srfType) : otherEntity(id), surfaceType(srfType) {}
			};

			struct SEngineParams
			{
				inline bool operator==(const SEngineParams &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

				static void ReflectType(Schematyc::CTypeDesc<SEngineParams>& desc)
				{
					desc.SetGUID("{8B051D30-2D75-48CE-8649-9D9920CAFBB7}"_cry_guid);
					desc.SetLabel("Engine Parameters");
					desc.AddMember(&CVehiclePhysicsComponent::SEngineParams::m_power, 'powe', "Power", "Power", "Power of the engine", 10000.f);
					desc.AddMember(&CVehiclePhysicsComponent::SEngineParams::m_maxRPM, 'maxr', "MaxRPM", "Maximum RPM", "engine torque decreases to 0 after reaching this rotation speed", 1200.f);
					desc.AddMember(&CVehiclePhysicsComponent::SEngineParams::m_minRPM, 'minr', "MinRPM", "Minimum RPM", "disengages the clutch when falling behind this limit, if braking with the engine", 60.f);
					desc.AddMember(&CVehiclePhysicsComponent::SEngineParams::m_idleRPM, 'idle', "IdleRPM", "Idle RPM", "RPM for idle state", 120.f);
					desc.AddMember(&CVehiclePhysicsComponent::SEngineParams::m_startRPM, 'star', "StartRPM", "Start RPM", "RPM when the engine is started", 400.f);
				}

				Schematyc::Range<0, 10000000> m_power = 50000.f;
				float m_maxRPM = 1200.f;
				float m_minRPM = 60.f;
				float m_idleRPM = 120.f;
				float m_startRPM = 400.f;
			};

			struct SGearParams
			{
				inline bool operator==(const SGearParams &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

				struct SGear
				{
					inline bool operator==(const SGear &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }
					inline bool operator!=(const SGear &rhs) const { return 0 != memcmp(this, &rhs, sizeof(rhs)); }

					void Serialize(Serialization::IArchive& archive)
					{
						archive(m_ratio, "Ratio", "Ratio");
					}

					static void ReflectType(Schematyc::CTypeDesc<SGear>& desc)
					{
						desc.SetGUID("{C1D4377F-D3CD-438F-B8FD-576E8FA2A3EF}"_cry_guid);
						desc.SetLabel("Gear");
						desc.AddMember(&CVehiclePhysicsComponent::SGearParams::SGear::m_ratio, 'rati', "Ratio", "Ratio", "assumes 0-backward gear, 1-neutral, 2 and above - forward", 2.f);
					}

					Schematyc::Range<0, 2> m_ratio;
				};

				static void ReflectType(Schematyc::CTypeDesc<SGearParams>& desc)
				{
					desc.SetGUID("{C632BB61-DAE8-4270-B759-114F01F55517}"_cry_guid);
					desc.SetLabel("Gear Parameters");
					desc.AddMember(&CVehiclePhysicsComponent::SGearParams::m_gears, 'gear', "Gears", "Gears", "Specifies number of gears, and their parameters", Schematyc::CArray<CVehiclePhysicsComponent::SGearParams::SGear>());
					desc.AddMember(&CVehiclePhysicsComponent::SGearParams::m_shiftUpRPM, 'shiu', "ShiftUpRPM", "Shift Up RPM", "RPM threshold for for automatic gear switching", 600.f);
					desc.AddMember(&CVehiclePhysicsComponent::SGearParams::m_shiftDownRPM, 'shid', "ShiftDownRPM", "Shift Down RPM", "RPM threshold for for automatic gear switching", 240.f);
					desc.AddMember(&CVehiclePhysicsComponent::SGearParams::m_directionSwitchRPM, 'dirs', "DirectionSwitchRPM", "Direction Switch RPM", "RPM threshold for switching back and forward gears", 10.f);
				}

				Schematyc::CArray<SGear> m_gears;

				float m_shiftUpRPM = 600.f;
				float m_shiftDownRPM = 240.f;

				float m_directionSwitchRPM = 10.f;
			};

			enum class EGear
			{
				Reverse = -1,
				Neutral,
				// First forward gear, note how there can be more specified by the user
				Forward,

				DefaultCount = Forward + 2
			};

			CVehiclePhysicsComponent();
			virtual ~CVehiclePhysicsComponent();

			static void ReflectType(Schematyc::CTypeDesc<CVehiclePhysicsComponent>& desc)
			{
				desc.SetGUID("{47EBC019-41CB-415E-AB57-2A3A99B918C2}}"_cry_guid);
				desc.SetEditorCategory("Physics");
				desc.SetLabel("Vehicle Physics");
				desc.SetDescription("");
				desc.SetIcon("icons:ObjectTypes/object.ico");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::Singleton });

				// Mark the Character Controller component as incompatible
				desc.AddComponentInteraction(SEntityComponentRequirements::EType::Incompatibility, "{98183F31-A685-43CD-92A9-815274F0A81C}"_cry_guid);
				// Mark the RigidBody component as incompatible
				desc.AddComponentInteraction(SEntityComponentRequirements::EType::Incompatibility, "{912C6CE8-56F7-4FFA-9134-F98D4E307BD6}"_cry_guid);
				// Mark the Area component as incompatible
				desc.AddComponentInteraction(SEntityComponentRequirements::EType::Incompatibility, "{EC7F145B-D48F-4863-B9C2-3D3E2C8DCC61}"_cry_guid);

				desc.AddMember(&CVehiclePhysicsComponent::m_engineParams, 'engn', "EngineParams", "Engine Parameters", nullptr, CVehiclePhysicsComponent::SEngineParams());
				desc.AddMember(&CVehiclePhysicsComponent::m_gearParams, 'gear', "GearParams", "Gear Parameters", nullptr, CVehiclePhysicsComponent::SGearParams());
				desc.AddMember(&CVehiclePhysicsComponent::m_bSendCollisionSignal, 'send', "SendCollisionSignal", "Send Collision Signal", "Whether or not this component should listen for collisions and report them", false);
			}

			virtual void UseHandbrake(bool bSet) 
			{
				pe_action_drive driveAction;
				driveAction.bHandBrake = bSet ? 1 : 0;
				m_pEntity->GetPhysicalEntity()->Action(&driveAction);
			}
			bool IsUsingHandbrake() const { return m_vehicleStatus.bHandBrake != 0; }

			virtual void SetThrottle(Schematyc::Range<0, 1> ratio) 
			{ 
				pe_action_drive driveAction;
				driveAction.pedal = ratio;
				m_pEntity->GetPhysicalEntity()->Action(&driveAction);
			}
			float GetThrottle() const { return m_vehicleStatus.pedal; }

			void SetBrake(Schematyc::Range<0, 1> ratio) 
			{
				pe_action_drive driveAction;
				driveAction.pedal = -ratio;
				m_pEntity->GetPhysicalEntity()->Action(&driveAction);
			}
			Schematyc::Range<0, 1> GetBrake() const { return m_vehicleStatus.footbrake; }

			virtual void SetCurrentGear(int gearId) 
			{
				pe_action_drive driveAction;
				driveAction.iGear = gearId + 1;
				m_pEntity->GetPhysicalEntity()->Action(&driveAction);
			}
			int GetCurrentGear() const { return m_vehicleStatus.iCurGear - 1; }

			virtual void GearUp() { SetCurrentGear(min(m_vehicleStatus.iCurGear + 1, (int)m_gearParams.m_gears.Size())); }
			virtual void GearDown() { SetCurrentGear(max(m_vehicleStatus.iCurGear - 1, (int)EGear::Reverse + 1)); }

			virtual void SetSteeringAngle(CryTransform::CAngle angle) 
			{
				pe_action_drive driveAction;
				driveAction.steer = angle.ToRadians();
				m_pEntity->GetPhysicalEntity()->Action(&driveAction);
			}
			CryTransform::CAngle GetSteeringAngle() const { return CryTransform::CAngle::FromRadians(m_vehicleStatus.steer); }

			float GetEngineRPM() const { return m_vehicleStatus.engineRPM; }
			float GetTorque() const { return m_vehicleStatus.drivingTorque; }
			bool HasWheelContact() const { return m_vehicleStatus.bWheelContact != 0; }

			virtual void SetVelocity(const Vec3& velocity)
			{
				if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysics())
				{
					pe_action_set_velocity action_set_velocity;
					action_set_velocity.v = velocity;
					pPhysicalEntity->Action(&action_set_velocity);
				}
			}

			Vec3 GetVelocity() const
			{
				if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysics())
				{
					pe_status_dynamics dynStatus;
					if (pPhysicalEntity->GetStatus(&dynStatus))
					{
						return dynStatus.v;
					}
				}

				return ZERO;
			}

			virtual void SetAngularVelocity(const Vec3& angularVelocity)
			{
				if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysics())
				{
					pe_action_set_velocity action_set_velocity;
					action_set_velocity.w = angularVelocity;
					pPhysicalEntity->Action(&action_set_velocity);
				}
			}

			Vec3 GetAngularVelocity() const
			{
				if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysics())
				{
					pe_status_dynamics dynStatus;
					if (pPhysicalEntity->GetStatus(&dynStatus))
					{
						return dynStatus.w;
					}
				}

				return ZERO;
			}

			virtual void ApplyImpulse(const Vec3& force)
			{
				// Only dispatch the impulse to physics if one was provided
				if (!force.IsZero())
				{
					if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysics())
					{
						pe_action_impulse impulseAction;
						impulseAction.impulse = force;

						pPhysicalEntity->Action(&impulseAction);
					}
				}
			}

			virtual void ApplyAngularImpulse(const Vec3& force)
			{
				// Only dispatch the impulse to physics if one was provided
				if (!force.IsZero())
				{
					if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysics())
					{
						pe_action_impulse impulseAction;
						impulseAction.angImpulse = force;

						pPhysicalEntity->Action(&impulseAction);
					}
				}
			}

		protected:
			// Needs to be persistent since physics is on another thread
			std::vector<float> m_gearRatios;

			SEngineParams m_engineParams;
			SGearParams m_gearParams;

			pe_status_vehicle m_vehicleStatus;
			bool m_bSendCollisionSignal = false;
		};
	}
}