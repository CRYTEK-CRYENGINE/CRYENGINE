using System;
using CryEngine.Common;

namespace CryEngine
{
	public class VehicleStatus : BasePhysicsStatus<pe_status_vehicle>
	{
		/// <summary>
		/// Current steering angle
		/// </summary>
		public float SteeringAngle { get; private set; }

		//TODO Get a better description for Pedal.
		/// <summary>
		/// Current engine pedal
		/// </summary>
		public float Pedal { get; private set; }

		/// <summary>
		/// True if handbrake is on, false otherwise
		/// </summary>
		public bool IsHandbrakeOn { get; private set; }

		/// <summary>
		/// Nonzero if footbrake is pressed (range 0..1)
		/// </summary>
		public float FootBrake { get; private set; }

		/// <summary>
		/// The current velocity of the vehicle
		/// </summary>
		public Vector3 Velocity { get; private set; }

		/// <summary>
		/// True if at least one wheel touches ground. False if all wheels are not touching the ground.
		/// </summary>
		public bool WheelContact { get; private set; }

		/// <summary>
		/// The current gear of the vehicle.
		/// </summary>
		public int CurrentGear { get; private set; }

		/// <summary>
		/// The rounds per minute of the vehicle's engine.
		/// </summary>
		public float EngineRPM { get; private set; }

		//TODO Get a better description for Clutch.
		/// <summary>
		/// Current engine's clutch
		/// </summary>
		public float Clutch { get; private set; }

		/// <summary>
		/// The torque of the vehicle
		/// </summary>
		public float DrivingTorque { get; private set; }

		/// <summary>
		/// Number of non-static contacting entities
		/// </summary>
		public int ActiveColliderContacts { get; private set; }

		internal override pe_status_vehicle ToNativeStatus()
		{
			return new pe_status_vehicle();
		}

		internal override void NativeToManaged<T>(T baseNative)
		{
			if(baseNative == null)
			{
				throw new ArgumentNullException(nameof(baseNative));
			}

			var native = baseNative as pe_status_vehicle;
			if(native == null)
			{
				Log.Error<DynamicsStatus>("Expected pe_status_vehicle but received {0}!", baseNative.GetType());
				return;
			}
		
			SteeringAngle = native.steer;
			Pedal = native.pedal;
			IsHandbrakeOn = native.bHandBrake != 0;
			FootBrake = native.footbrake;
			Velocity = native.vel;
			WheelContact = native.bWheelContact != 0;
			CurrentGear = native.iCurGear;
			EngineRPM = native.engineRPM;
			Clutch = native.clutch;
			DrivingTorque = native.drivingTorque;
			ActiveColliderContacts = native.nActiveColliders;
		}
	}
}
