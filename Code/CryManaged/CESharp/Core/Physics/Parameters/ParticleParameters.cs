using CryEngine.Common;
using CryEngine.EntitySystem;

namespace CryEngine
{
	public class ParticleParameters : BasePhysicsParameters<pe_params_particle>
	{
		private readonly pe_params_particle _parameters = new pe_params_particle();

		/// <summary>
		/// The Physicalization flags for this particle.
		/// </summary>
		public PhysicsParticleFlags ParticleFlags
		{
			get { return (PhysicsParticleFlags)_parameters.flags; }
			set { _parameters.flags = (uint)value; }
		}

		public float ParticleMass
		{
			get { return _parameters.mass; }
			set { _parameters.mass = value; }
		}

		/// <summary>
		/// Pseudo-radius
		/// </summary>
		public float Size
		{
			get { return _parameters.size; }
			set { _parameters.size = value; }
		}

		/// <summary>
		/// Thickness when lying on a surface (if left unused, size will be used)
		/// </summary>
		public float Thickness
		{
			get { return _parameters.thickness; }
			set { _parameters.thickness = value; }
		}

		/// <summary>
		/// Direction of movement
		/// </summary>
		public Vector3 Heading
		{
			get { return _parameters.heading; }
			set { _parameters.heading = value; }
		}

		/// <summary>
		/// Velocity along "heading"
		/// </summary>
		public float Velocity
		{
			get { return _parameters.velocity; }
			set { _parameters.velocity = value; }
		}

		/// <summary>
		/// Air resistance coefficient, F = kv
		/// </summary>
		public float AirResistance
		{
			get { return _parameters.kAirResistance; }
			set { _parameters.kAirResistance = value; }
		}

		/// <summary>
		/// Water resistance coefficient, F = kv
		/// </summary>
		public float WaterResistance
		{
			get { return _parameters.kWaterResistance; }
			set { _parameters.kWaterResistance = value; }
		}

		/// <summary>
		/// Acceleration along direction of movement
		/// </summary>
		public float ThrustAcceleration
		{
			get { return _parameters.accThrust; }
			set { _parameters.accThrust = value; }
		}

		/// <summary>
		/// Acceleration that lifts particle with the current speed
		/// </summary>
		public float LiftAcceleration
		{
			get { return _parameters.accLift; }
			set { _parameters.accLift = value; }
		}

		public int SurfaceIdx
		{
			get { return _parameters.surface_idx; }
			set { _parameters.surface_idx = value; }
		}

		public Vector3 AngularVelocity
		{
			get { return _parameters.wspin; }
			set { _parameters.wspin = value; }
		}

		/// <summary>
		/// Stores this gravity and uses it if the current area's gravity is equal to the global gravity
		/// </summary>
		public Vector3 Gravity
		{
			get { return _parameters.gravity; }
			set { _parameters.gravity = value; }
		}

		/// <summary>
		/// Gravity when underwater
		/// </summary>
		public Vector3 WaterGravity
		{
			get { return _parameters.waterGravity; }
			set { _parameters.waterGravity = value; }
		}

		/// <summary>
		/// Aligns this direction with the surface normal when sliding
		/// </summary>
		public Vector3 Normal
		{
			get { return _parameters.normal; }
			set { _parameters.normal = value; }
		}

		/// <summary>
		/// Aligns this direction with the roll axis when rolling (0,0,0 to disable alignment)
		/// </summary>
		public Vector3 RollAxis
		{
			get { return _parameters.rollAxis; }
			set { _parameters.rollAxis = value; }
		}

		/// <summary>
		/// Initial orientation (zero means x along direction of movement, z up)
		/// </summary>
		public Quaternion InitialOrientation
		{
			get { return _parameters.q0; }
			set { _parameters.q0 = value; }
		}

		/// <summary>
		/// Velocity threshold for bouncing to sliding switch
		/// </summary>
		public float MinBounceVelocity
		{
			get { return _parameters.minBounceVel; }
			set { _parameters.minBounceVel = value; }
		}

		/// <summary>
		/// Speed threshold before this entity goes to sleep
		/// </summary>
		public float MinVelocity
		{
			get { return _parameters.minVel; }
			set { _parameters.minVel = value; }
		}

		/// <summary>
		/// Physical entity to ignore during collisions
		/// </summary>
		public PhysicsEntity ColliderToIgnore
		{
			get
			{
				var nativeIgnore = _parameters.pColliderToIgnore;
				if(nativeIgnore == null)
				{
					return null;
				}

				var nativeEntity = Engine.System.GetIEntitySystem().GetEntityFromPhysics(nativeIgnore);
				var entity = Entity.Get(nativeEntity.GetId());

				return entity.Physics;
			}
			set { _parameters.pColliderToIgnore = value.NativeHandle; }
		}

		/// <summary>
		/// Piercability for ray tests. Piercable hits slow the particle down, but don't stop it
		/// </summary>
		public int Piercabiltity
		{
			get { return _parameters.iPierceability; }
			set { _parameters.iPierceability = value; }
		}

		/// <summary>
		/// The 'objtype' passed to RayWorldIntersection
		/// </summary>
		public entity_query_flags ColliderTypes
		{
			get { return (entity_query_flags)_parameters.collTypes; }
			set { _parameters.collTypes = (int)value; }
		}

		/// <summary>
		/// How often (in frames) world area checks are made
		/// </summary>
		public int AreaCheckPeriod
		{
			get { return _parameters.areaCheckPeriod; }
			set { _parameters.areaCheckPeriod = value; }
		}

		/// <summary>
		/// Prevents playing of material FX from now on
		/// </summary>
		public bool DontPlayHitEffect
		{
			get { return _parameters.dontPlayHitEffect == 1; }
			set { _parameters.dontPlayHitEffect = value ? 1 : 0; }
		}

		internal override pe_params_particle ToNativeParameters()
		{
			return _parameters;
		}
	}
}
