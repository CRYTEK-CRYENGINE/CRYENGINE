using System;
using CryEngine.Common;
using CryEngine.EntitySystem;

namespace CryEngine
{
	public class LivingStatus : BasePhysicsStatus<pe_status_living>
	{
		/// <summary>
		/// True if the Entity has no contact with the ground, otherwise false.
		/// </summary>
		[SerializeValue]
		public bool IsFlying { get; private set; }

		/// <summary>
		/// How long the entity has been flying
		/// </summary>
		[SerializeValue]
		public float TimeFlying { get; private set; }

		/// <summary>
		/// The offset of the camera
		/// </summary>
		[SerializeValue]
		public Vector3 CameraOffset { get; private set; }

		/// <summary>
		/// The actual veloctity, as in rate of position change.
		/// </summary>
		[SerializeValue]
		public Vector3 Velocity { get; private set; }

		/// <summary>
		/// Physical movement velocity. It can be different from Velocity because in many cases when the entity bumps into an obstacle 
		/// it will restrict the actual movement but keep the movement velocity the same. That way if on the next frame the obstacle ends 
		/// no speed will be lost.
		/// </summary>
		[SerializeValue]
		public Vector3 UnconstrainedVelocity { get; private set; }

		/// <summary>
		/// The velocity requested in the last action
		/// </summary>
		[SerializeValue]
		public Vector3 RequestedVelocity { get; private set; }

		/// <summary>
		/// The velocity of the object the Entity is standing on
		/// </summary>
		[SerializeValue]
		public Vector3 GroundVelocity { get; private set; }

		/// <summary>
		/// The z-coordinate of the position where the last contact with the ground occurred
		/// </summary>
		[SerializeValue]
		public float GroundHeight { get; private set; }

		/// <summary>
		/// The normal of the ground where the last contact with the ground occured
		/// </summary>
		[SerializeValue]
		public Vector3 GroundSlope { get; private set; }

		//TODO Get a description for groundSurfaceIdx
		public int GroundSurfaceIdx { get; private set; }

		/// <summary>
		/// Contact with the ground that also has default collision flags
		/// </summary>
		[SerializeValue]
		public int GroundSurfaceIdxAux { get; private set; }

		/// <summary>
		/// Only returns an actual PhysicsEntity if the ground collider is not static
		/// </summary>
		[SerializeValue]
		public PhysicsEntity GroundCollider { get; private set; }

		//TODO Get a description for iGroundColliderPart
		[SerializeValue]
		public int GroundColliderPart { get; private set; }

		/// <summary>
		/// Elapsed time since the stance has been changed
		/// </summary>
		[SerializeValue]
		public float TimeSinceStanceChanged { get; private set; }

		/// <summary>
		/// Tries to detect cases when the entity cannot move as before because of collisions
		/// </summary>
		[SerializeValue]
		public float IsStuck { get; private set; }

		/// <summary>
		/// Quantised time
		/// </summary>
		[SerializeValue]
		public int CurrentTime { get; private set; }

		/// <summary>
		/// If true, the entity is being pushed by heavy objects from opposite directions.
		/// </summary>
		[SerializeValue]
		public bool IsSquashed { get; private set; }

		internal override pe_status_living ToNativeStatus()
		{
			return new pe_status_living();
		}

		internal override void NativeToManaged<T>(T baseNative)
		{
			if(baseNative == null)
			{
				throw new ArgumentNullException(nameof(baseNative));
			}

			var native = baseNative as pe_status_living;
			if(native == null)
			{
				Log.Error<DynamicsStatus>("Expected pe_status_living but received {0}!", baseNative.GetType());
				return;
			}
		
			IsFlying = native.bFlying == 1;
			TimeFlying = native.timeFlying;
			CameraOffset = native.camOffset;
			Velocity = native.vel;
			UnconstrainedVelocity = native.velUnconstrained;
			RequestedVelocity = native.velRequested;
			GroundVelocity = native.velGround;
			GroundHeight = native.groundHeight;
			GroundSlope = native.groundSlope;
			GroundSurfaceIdx = native.groundSurfaceIdx;
			GroundSurfaceIdxAux = native.groundSurfaceIdxAux;
			GroundColliderPart = native.iGroundColliderPart;
			TimeSinceStanceChanged = native.timeSinceStanceChange;
			IsStuck = native.bStuck;
			CurrentTime = native.iCurTime;
			IsSquashed = native.bSquashed == 1;

			var nativeCollider = native.pGroundCollider;
			if(nativeCollider == null)
			{
				GroundCollider = null;
			}
			else
			{
				var nativeEntity = Engine.System.GetIEntitySystem().GetEntityFromPhysics(nativeCollider);
				var entity = Entity.Get(nativeEntity.GetId());
				GroundCollider = entity.Physics;
			}
		}
	}
}
