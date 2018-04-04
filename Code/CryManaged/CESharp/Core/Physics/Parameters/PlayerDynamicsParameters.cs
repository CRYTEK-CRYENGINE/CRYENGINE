using CryEngine.Common;
using CryEngine.EntitySystem;

namespace CryEngine
{
	public class PlayerDynamicsParameters : BasePhysicsParameters<pe_player_dynamics>
	{
		private readonly pe_player_dynamics _parameters = new pe_player_dynamics();

		/// <summary>
		/// Inertia coefficient. The higher it is, the less inertia there is; 0 means no inertia
		/// </summary>
		public float InertiaCoefficient
		{
			get { return _parameters.kInertia; }
			set { _parameters.kInertia = value; }
		}

		/// <summary>
		/// Inertia on acceleration
		/// </summary>
		public float InertiaAcceleration
		{
			get { return _parameters.kInertiaAccel; }
			set { _parameters.kInertiaAccel = value; }
		}

		/// <summary>
		/// Air control coefficient 0..1, 1 - special value (total control of movement)
		/// </summary>
		public float AirControlCoefficient
		{
			get { return _parameters.kAirControl; }
			set { _parameters.kAirControl = value; }
		}

		/// <summary>
		/// Defines how fast velocity is damped when the entity is flying
		/// </summary>
		public float AirResistance
		{
			get { return _parameters.kAirResistance; }
			set { _parameters.kAirResistance = value; }
		}

		/// <summary>
		/// Gravity vector. Direction specifies the direction of the gravity and magnitude the strength of the gravity.
		/// </summary>
		public Vector3 Gravity
		{
			get { return _parameters.gravity; }
			set { _parameters.gravity = value; }
		}

		/// <summary>
		/// Vertical camera shake speed after landings
		/// </summary>
		public float NodSpeed
		{
			get { return _parameters.nodSpeed; }
			set { _parameters.nodSpeed = value; }
		}

		/// <summary>
		/// Whether entity is swimming (is not bound to ground plane)
		/// </summary>
		public bool IsSwimming
		{
			get { return _parameters.bSwimming == 1; }
			set { _parameters.bSwimming = value ? 1 : 0; }
		}

		/// <summary>
		/// Mass in Kilograms
		/// </summary>
		public float Mass
		{
			get { return _parameters.mass; }
			set { _parameters.mass = value; }
		}

		/// <summary>
		/// Surface identifier for collisions
		/// </summary>
		public int SurfaceIdx
		{
			get { return _parameters.surface_idx; }
			set { _parameters.surface_idx = value; }
		}

		/// <summary>
		/// If surface slope is more than this angle, player starts sliding. Angle is in radians.
		/// </summary>
		public float MinSlideAngle
		{
			get { return _parameters.minSlideAngle; }
			set { _parameters.minSlideAngle = value; }
		}

		/// <summary>
		/// Player cannot climb surfaces which slope is steeper than this angle. Angle is in radians.
		/// </summary>
		public float MaxClimbAngle
		{
			get { return _parameters.maxClimbAngle; }
			set { _parameters.maxClimbAngle = value; }
		}

		/// <summary>
		/// Player is not allowed to jump towards ground if this angle is exceeded. Angle is in radians.
		/// </summary>
		public float MaxJumpAngle
		{
			get { return _parameters.maxJumpAngle; }
			set { _parameters.maxJumpAngle = value; }
		}

		/// <summary>
		/// Player starts falling when slope is steeper than this. Angle is in radians.
		/// </summary>
		public float MinFallAngle
		{
			get { return _parameters.minFallAngle; }
			set { _parameters.minFallAngle = value; }
		}

		/// <summary>
		/// Player cannot stand on surfaces that are moving faster than this.
		/// </summary>
		public float MaxGroundVelocity
		{
			get { return _parameters.maxVelGround; }
			set { _parameters.maxVelGround = value; }
		}

		/// <summary>
		/// Forcefully turns on inertia for that duration after receiving an impulse.
		/// </summary>
		public float ImpulseRecoveryTime
		{
			get { return _parameters.timeImpulseRecover; }
			set { _parameters.timeImpulseRecover = value; }
		}

		/// <summary>
		/// Entity types to check collisions against
		/// </summary>
		public entity_query_flags ColliderTypes
		{
			get { return (entity_query_flags)_parameters.collTypes; }
			set { _parameters.collTypes = (int)value; }
		}

		/// <summary>
		/// Ignore collisions with this *living entity* (doesn't work with other entity types)
		/// </summary>
		public PhysicsEntity LivingEntityToIgnore
		{
			get
			{
				var nativeIgnore = _parameters.pLivingEntToIgnore;
				if(nativeIgnore == null)
				{
					return null;
				}

				var nativeEntity = Engine.System.GetIEntitySystem().GetEntityFromPhysics(nativeIgnore);
				var entity = Entity.Get(nativeEntity.GetId());

				return entity.Physics;
			}
			set { _parameters.pLivingEntToIgnore = value.NativeHandle; }
		}

		/// <summary>
		/// False disables all simulation for the character, apart from moving along the requested velocity.
		/// </summary>
		public bool Active
		{
			get { return _parameters.bActive == 1; }
			set { _parameters.bActive = value ? 1 : 0; }
		}

		/// <summary>
		/// Requests that the player rolls back to that time and re-executes pending actions during the next step
		/// </summary>
		public int RequestedTime
		{
			get { return _parameters.iRequestedTime; }
			set { _parameters.iRequestedTime = value; }
		}

		/// <summary>
		/// when not false, if the living entity is not active, the ground collider, if any, will be explicitly released during the simulation step.
		/// </summary>
		public bool ReleaseGroundColliderWhenNotActive
		{
			get { return _parameters.bReleaseGroundColliderWhenNotActive == 1; }
			set { _parameters.bReleaseGroundColliderWhenNotActive = value ? 1 : 0; }
		}

		internal override pe_player_dynamics ToNativeParameters()
		{
			return _parameters;
		}
	}
}
