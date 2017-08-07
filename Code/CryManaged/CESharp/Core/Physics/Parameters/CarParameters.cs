using System;
using CryEngine.Common;

namespace CryEngine
{
	public class CarParameters : BasePhysicsParameters<pe_params_car>
	{
		private readonly pe_params_car _parameters = new pe_params_car();

		/// <summary>
		/// Friction torque at axes divided by mass of vehicle
		/// </summary>
		public float AxleFriction
		{
			get { return _parameters.axleFriction; }
			set { _parameters.axleFriction = value; }
		}

		/// <summary>
		/// Power of the engine (about 10,000 - 100,000)
		/// </summary>
		public float EnginePower
		{
			get { return _parameters.enginePower; }
			set { _parameters.enginePower = value; }
		}

		/// <summary>
		/// Maximum steering angle
		/// </summary>
		public float MaxSteeringAngle
		{
			get { return _parameters.maxSteer; }
			set { _parameters.maxSteer = value; }
		}

		/// <summary>
		/// Engine torque decreases to 0 after reaching this rotation speed
		/// </summary>
		public float EngineMaxRPM
		{
			get { return _parameters.engineMaxRPM; }
			set { _parameters.engineMaxRPM = value; }
		}

		/// <summary>
		/// Disengages the clutch when falling behind this limit, if braking with the engine
		/// </summary>
		public float EngineMinRPM
		{
			get { return _parameters.engineMinRPM; }
			set { _parameters.engineMinRPM = value; }
		}

		/// <summary>
		/// Torque applied when using the engine to brake
		/// </summary>
		public float BrakeTorque
		{
			get { return _parameters.brakeTorque; }
			set { _parameters.brakeTorque = value; }
		}

		/// <summary>
		/// For suspensions; 0-explicit Euler, 1-implicit Euler
		/// </summary>
		public int IntegrationType
		{
			get { return _parameters.iIntegrationType; }
			set { _parameters.iIntegrationType = value; }
		}

		/// <summary>
		/// Maximum time step when vehicle has only wheel contacts
		/// </summary>
		public float MaxTimeStep
		{
			get { return _parameters.maxTimeStep; }
			set { _parameters.maxTimeStep = value; }
		}

		/// <summary>
		/// Minimum awake energy when vehicle has only wheel contacts
		/// </summary>
		public float MinEnergy
		{
			get { return _parameters.minEnergy; }
			set { _parameters.minEnergy = value; }
		}

		/// <summary>
		/// Damping when vehicle has only wheel contacts
		/// </summary>
		public float Damping
		{
			get { return _parameters.damping; }
			set { _parameters.damping = value; }
		}

		/// <summary>
		/// Limits the tire friction when using the handbrake
		/// </summary>
		public float MinBrakingFriction
		{
			get { return _parameters.minBrakingFriction; }
			set { _parameters.minBrakingFriction = value; }
		}

		/// <summary>
		/// Limits the tire friction when using the handbrake
		/// </summary>
		public float MaxBrakingFriction
		{
			get { return _parameters.maxBrakingFriction; }
			set { _parameters.maxBrakingFriction = value; }
		}

		/// <summary>
		/// Stabilizer force, as a multiplier for Stiffness of respective suspensions
		/// </summary>
		public float Stabilizer
		{
			get { return _parameters.kStabilizer; }
			set { _parameters.kStabilizer = value; }
		}

		/// <summary>
		/// The number of wheels
		/// </summary>
		public int Wheels
		{
			get { return _parameters.nWheels; }
			set { _parameters.nWheels = value; }
		}

		/// <summary>
		/// RPM threshold for automatic gear switching
		/// </summary>
		public float EngineShiftUpRPM
		{
			get { return _parameters.engineShiftUpRPM; }
			set { _parameters.engineShiftUpRPM = value; }
		}

		/// <summary>
		/// RPM threshold for automatic gear switching
		/// </summary>
		public float EngineShiftDownRPM
		{
			get { return _parameters.engineShiftDownRPM; }
			set { _parameters.engineShiftDownRPM = value; }
		}

		/// <summary>
		/// RPM for idle state
		/// </summary>
		public float EngineIdleRPM
		{
			get { return _parameters.engineIdleRPM; }
			set { _parameters.engineIdleRPM = value; }
		}

		/// <summary>
		/// Sets this RPM when activating the engine
		/// </summary>
		public float EngineStartRPM
		{
			get { return _parameters.engineStartRPM; }
			set { _parameters.engineStartRPM = value; }
		}

		/// <summary>
		/// Clutch engagement/disengagement speed
		/// </summary>
		public float ClutchSpeed
		{
			get { return _parameters.clutchSpeed; }
			set { _parameters.clutchSpeed = value; }
		}

		/// <summary>
		/// Number of gears
		/// </summary>
		public int Gears
		{
			get { return _parameters.nGears; }
			set { _parameters.nGears = value; }
		}

		//TODO gearRatios expects a pointer of a float. Need to change this in SWIG.
		/// <summary>
		/// Assumes 0-backward gear, 1-neutral, 2 and above - forward
		/// </summary>
		public float GearRatios
		{
			get
			{
				throw new NotImplementedException("GearRatios is currently unavailable");
			}
			set
			{
				throw new NotImplementedException("GearRatios is currently unavailable");
			}
		}

		/// <summary>
		/// Additional gear index clamping
		/// </summary>
		public int MaxGear
		{
			get { return _parameters.maxGear; }
			set { _parameters.maxGear = value; }
		}

		/// <summary>
		/// Additional gear index clamping
		/// </summary>
		public int MinGear
		{
			get { return _parameters.minGear; }
			set { _parameters.minGear = value; }
		}

		/// <summary>
		/// Lateral speed threshold for switchig a wheel to a 'slipping' mode
		/// </summary>
		public float SlipThreshold
		{
			get { return _parameters.slipThreshold; }
			set { _parameters.slipThreshold = value; }
		}

		/// <summary>
		/// RPM threshold for switching back and forward gears
		/// </summary>
		public float GearDirSwitchRPM
		{
			get { return _parameters.gearDirSwitchRPM; }
			set { _parameters.gearDirSwitchRPM = value; }
		}

		/// <summary>
		/// Friction modifier for sliping wheels
		/// </summary>
		public float DynamicFriction
		{
			get { return _parameters.kDynFriction; }
			set { _parameters.kDynFriction = value; }
		}

		/// <summary>
		/// For tracked vehicles. Steering angle that causes equal but opposite forces on tracks
		/// </summary>
		public float SteerTrackedNeutralTurn
		{
			get { return _parameters.steerTrackNeutralTurn; }
			set { _parameters.steerTrackNeutralTurn = value; }
		}

		/// <summary>
		/// For tracked vehicles. Tilt angle of pulling force towards ground
		/// </summary>
		public float PullTilt
		{
			get { return _parameters.pullTilt; }
			set { _parameters.pullTilt = value; }
		}

		/// <summary>
		/// Maximum wheel contact normal tilt (left or right) after which it acts as a locked part of the hull; it's a cosine of the angle
		/// </summary>
		public float MaxTilt
		{
			get { return _parameters.maxTilt; }
			set { _parameters.maxTilt = value; }
		}

		/// <summary>
		/// Keeps wheel traction in tilted mode
		/// </summary>
		public bool KeepTractionWhenTilted
		{
			get { return _parameters.bKeepTractionWhenTilted == 1; }
			set { _parameters.bKeepTractionWhenTilted = value ? 1 : 0; }
		}

		/// <summary>
		/// Scales wheels' masses for inertia computations (default 0)
		/// </summary>
		public float WheelMassScale
		{
			get { return _parameters.wheelMassScale; }
			set { _parameters.wheelMassScale = value; }
		}

		internal override pe_params_car ToNativeParameters()
		{
			return _parameters;
		}
	}
}
