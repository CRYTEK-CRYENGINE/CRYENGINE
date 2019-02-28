#pragma once

#include "DefaultComponents/Geometry/AdvancedAnimationComponent.h"
#include "DefaultComponents/Physics/RigidBodyComponent.h"
#include "DefaultComponents/Physics/AreaComponent.h"

class CPlugin_CryDefaultEntities;

namespace Cry
{
	namespace DefaultComponents
	{
		class CCharacterControllerComponent
			: public IEntityComponent
#ifndef RELEASE
			, public IEntityComponentPreviewer
#endif
		{
		protected:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);

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

		public:
			enum class EChangeVelocityMode : int
			{
				SetAsTarget = 0, // velocity change with snapping to the ground, can be used for walking
				Jump        = 1, // instant velocity change, can be used for jumping or flying
				Add         = 2, // adding velocity to the current value of velocity
			};

			static void ReflectType(Schematyc::CTypeDesc<CCharacterControllerComponent>& desc)
			{
				desc.SetGUID("{98183F31-A685-43CD-92A9-815274F0A81C}"_cry_guid);
				desc.SetEditorCategory("Physics");
				desc.SetLabel("Character Controller");
				desc.SetDescription("Functionality for a standard FPS / TPS style walking character");
				//desc.SetIcon("icons:ObjectTypes/object.ico");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Singleton, IEntityComponent::EFlags::Transform });

				// Mark the Rigid Body component as incompatible
				desc.AddComponentInteraction(SEntityComponentRequirements::EType::Incompatibility, "{912C6CE8-56F7-4FFA-9134-F98D4E307BD6}"_cry_guid);
				// Mark the Vehicle Physics component as incompatible
				desc.AddComponentInteraction(SEntityComponentRequirements::EType::Incompatibility, "{47EBC019-41CB-415E-AB57-2A3A99B918C2}}"_cry_guid);
				// Mark the Area component as incompatible
				desc.AddComponentInteraction(SEntityComponentRequirements::EType::Incompatibility, "{EC7F145B-D48F-4863-B9C2-3D3E2C8DCC61}"_cry_guid);

				desc.AddMember(&CCharacterControllerComponent::m_bNetworked, 'netw', "Networked", "Network Synced", "Syncs the physical entity over the network, and keeps it in sync with the server", false);
				desc.AddMember(&CCharacterControllerComponent::m_physics, 'phys', "Physics", "Physics", "Physical properties for the character", CCharacterControllerComponent::SPhysics());
				desc.AddMember(&CCharacterControllerComponent::m_movement, 'move', "Movement", "Movement", "Movement properties for the character", CCharacterControllerComponent::SMovement());
			}

			CCharacterControllerComponent() = default;
			virtual ~CCharacterControllerComponent();

			bool IsOnGround() const { return m_bOnGround; }
			const Schematyc::UnitLength<Vec3>& GetGroundNormal() const { return m_groundNormal; }

			virtual void AddVelocity(const Vec3& velocity)
			{
				if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysicalEntity())
				{
					pe_action_move moveAction;

					// Apply movement request directly to velocity
					moveAction.iJump = 2;
					moveAction.dir = velocity;

					// Dispatch the movement request
					pPhysicalEntity->Action(&moveAction);
				}
			}

			virtual void SetVelocity(const Vec3& velocity)
			{
				if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysicalEntity())
				{
					pe_action_move moveAction;

					// Override velocity
					moveAction.iJump = 0;
					moveAction.dir = velocity;

					// Dispatch the movement request
					pPhysicalEntity->Action(&moveAction);
				}
			}

			virtual void ChangeVelocity(const Vec3& velocity, const EChangeVelocityMode mode)
			{
				if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysicalEntity())
				{
					pe_action_move moveAction;

					// Override velocity
					moveAction.iJump = static_cast<int>(mode);
					moveAction.dir = velocity;

					// Dispatch the movement request
					pPhysicalEntity->Action(&moveAction);
				}
			}

			const Vec3& GetVelocity() const { return m_velocity; }
			Vec3 GetMoveDirection() const { return m_velocity.GetNormalized(); }

			bool IsWalking() const { return m_velocity.GetLength2D() > 0.2f && m_bOnGround; }
			
			virtual void Physicalize()
			{
				// Physicalize the player as type Living.
				// This physical entity type is specifically implemented for players
				SEntityPhysicalizeParams physParams;
				physParams.type = PE_LIVING;
				physParams.nSlot = GetOrMakeEntitySlotId();

				physParams.mass = m_physics.m_mass;

				pe_player_dimensions playerDimensions;

				// Prefer usage of a cylinder
				playerDimensions.bUseCapsule = m_physics.m_bCapsule ? 1 : 0;

				// Specify the size of our capsule, physics treats the input as the half-size, so we multiply our value by 0.5.
				// This ensures that 1 unit = 1m for designers.
				playerDimensions.sizeCollider = Vec3(m_physics.m_radius * 0.5f, 1.f, m_physics.m_height * 0.5f);
				// Capsule height needs to be adjusted to match 1 unit ~= 1m.
				if (playerDimensions.bUseCapsule)
				{
					playerDimensions.sizeCollider.z *= 0.5f;
				}
				playerDimensions.groundContactEps = m_physics.m_groundContactEps;
				// Keep pivot at the player's feet (defined in player geometry) 
				playerDimensions.heightPivot = 0.f;
				// Offset collider upwards
				playerDimensions.heightCollider = m_pTransform != nullptr ? m_pTransform->GetTranslation().z : 0.f;

				physParams.pPlayerDimensions = &playerDimensions;

				pe_player_dynamics playerDynamics;
				playerDynamics.mass = physParams.mass;
				playerDynamics.kAirControl = m_movement.m_airControlRatio;
				playerDynamics.kAirResistance = m_movement.m_airResistance;
				playerDynamics.kInertia = m_movement.m_inertia;
				playerDynamics.kInertiaAccel = m_movement.m_inertiaAcceleration;

				playerDynamics.maxClimbAngle = m_movement.m_maxClimbAngle.ToDegrees();
				playerDynamics.maxJumpAngle = m_movement.m_maxJumpAngle.ToDegrees();
				playerDynamics.minFallAngle = m_movement.m_minFallAngle.ToDegrees();
				playerDynamics.minSlideAngle = m_movement.m_minSlideAngle.ToDegrees();

				playerDynamics.maxVelGround = m_movement.m_maxGroundVelocity;

				physParams.pPlayerDynamics = &playerDynamics;

				m_pEntity->Physicalize(physParams);

				m_pEntity->UpdateComponentEventMask(this);
			}

			struct SPhysics
			{
				inline bool operator==(const SPhysics &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

				static void ReflectType(Schematyc::CTypeDesc<SPhysics>& desc)
				{
					desc.SetGUID("{3341F1DC-0753-466E-BC7A-FA77A49D3CB4}"_cry_guid);
					desc.AddMember(&CCharacterControllerComponent::SPhysics::m_mass, 'mass', "Mass", "Mass", "Mass of the character in kg", 80.f);
					desc.AddMember(&CCharacterControllerComponent::SPhysics::m_radius, 'radi', "Radius", "Collider Radius", "Radius of the capsule or cylinder", 0.45f);
					desc.AddMember(&CCharacterControllerComponent::SPhysics::m_height, 'heig', "Height", "Collider Height", "Height of the capsule or cylinder", 0.935f);
					desc.AddMember(&CCharacterControllerComponent::SPhysics::m_bCapsule, 'caps', "Capsule", "Use Capsule", "Whether or not to use a capsule as the main collider, otherwise cylinder", true);
					desc.AddMember(&CCharacterControllerComponent::SPhysics::m_groundContactEps, 'gce', "GroundContactEps", "Ground Contact Epsilon", "The amount that the player needs to move upwards before ground contact is lost", 0.004f);
					desc.AddMember(&CCharacterControllerComponent::SPhysics::m_bSendCollisionSignal, 'send', "SendCollisionSignal", "Send Collision Signal", "Whether or not this component should listen for collisions and report them", false);
				}

				Schematyc::PositiveFloat m_mass = 80.f;
				float m_radius = 0.45f;
				float m_height = 0.935f;
				bool m_bCapsule = true;
				Schematyc::Range<-100, 100> m_groundContactEps = 0.004f;

				bool m_bSendCollisionSignal = false;
			};

			struct SMovement
			{
				inline bool operator==(const SMovement &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

				static void ReflectType(Schematyc::CTypeDesc<SMovement>& desc)
				{
					desc.SetGUID("{71D2F72F-1AAF-4368-A356-FC07938837EB}"_cry_guid);
					desc.AddMember(&CCharacterControllerComponent::SMovement::m_airControlRatio, 'airc', "AirControl", "Air Control Ratio", "Indicates how much the character can move in the air, 0 means no movement while 1 means full control.", 1.f);
					desc.AddMember(&CCharacterControllerComponent::SMovement::m_airResistance, 'airr', "AirResistance", "Air Resistance", nullptr, 0.2f);
					desc.AddMember(&CCharacterControllerComponent::SMovement::m_inertia, 'iner', "Inertia", "Inertia Coefficient", "More amount gives less inertia, 0 being none", 8.f);
					desc.AddMember(&CCharacterControllerComponent::SMovement::m_inertiaAcceleration, 'inea', "InertiaAcc", "Inertia Acceleration Coefficient", "More amount gives less inertia on acceleration, 0 being none", 8.f);
					desc.AddMember(&CCharacterControllerComponent::SMovement::m_maxClimbAngle, 'maxc', "MaxClimb", "Maximum Climb Angle", "Maximum angle the character can climb", 50.0_degrees);
					desc.AddMember(&CCharacterControllerComponent::SMovement::m_maxJumpAngle, 'maxj', "MaxJump", "Maximum Jump Angle", "Maximum angle the character can jump at", 50.0_degrees);
					desc.AddMember(&CCharacterControllerComponent::SMovement::m_minSlideAngle, 'mins', "MinSlide", "Minimum Angle For Slide", "Minimum angle before the player starts sliding", 70.0_degrees);
					desc.AddMember(&CCharacterControllerComponent::SMovement::m_minFallAngle, 'minf', "MinFall", "Minimum Angle For Fall", "Minimum angle before the character starts falling", 80.0_degrees);
					desc.AddMember(&CCharacterControllerComponent::SMovement::m_maxGroundVelocity, 'maxg', "MaxGroundVelocity", "Maximum Surface Velocity", "Maximum velocity of the surface the character is on before they are considered airborne and slide off", DEG2RAD(50.f));
				}

				Schematyc::Range<0, 1> m_airControlRatio = 0.f;
				Schematyc::Range<0, 10000> m_airResistance = 0.2f;
				Schematyc::Range<0, 10000> m_inertia = 8.f;
				Schematyc::Range<0, 10000> m_inertiaAcceleration = 8.f;

				CryTransform::CClampedAngle<0, 90> m_maxClimbAngle = 50.0_degrees;
				CryTransform::CClampedAngle<0, 90> m_maxJumpAngle = 50.0_degrees;
				CryTransform::CClampedAngle<0, 90> m_minFallAngle = 80.0_degrees;
				CryTransform::CClampedAngle<0, 90> m_minSlideAngle = 70.0_degrees;

				Schematyc::Range<0, 10000> m_maxGroundVelocity = 16.f;
			};

			virtual SPhysics& GetPhysicsParameters() { return m_physics; }
			const SPhysics& GetPhysicsParameters() const { return m_physics; }

			virtual SMovement& GetMovementParameters() { return m_movement; }
			const SMovement& GetMovementParameters() const { return m_movement; }

		protected:
			bool m_bNetworked = false;
			
			SPhysics m_physics;
			SMovement m_movement;

			bool m_bOnGround = false;
			Schematyc::UnitLength<Vec3> m_groundNormal = Vec3(0, 0, 1);

			Vec3 m_velocity = ZERO;
		};

		static void ReflectType(Schematyc::CTypeDesc<CCharacterControllerComponent::EChangeVelocityMode>& desc)
		{
			desc.SetGUID("{D6B8165A-5BF5-431F-9268-4811DBED4760}"_cry_guid);
			desc.SetLabel("Velocity change mode");

			desc.AddConstant(CCharacterControllerComponent::EChangeVelocityMode::SetAsTarget, "SetAsTarget", "Velocity change with snapping to the ground, can be used for walking");
			desc.AddConstant(CCharacterControllerComponent::EChangeVelocityMode::Jump, "Jump", "Instant velocity change, can be used for jumping or flying");
			desc.AddConstant(CCharacterControllerComponent::EChangeVelocityMode::Add, "Add", "Adding velocity to the current value of velocity");
		}
	}
}