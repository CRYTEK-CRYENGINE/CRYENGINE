using CryEngine.Common;

namespace CryEngine
{
	public class BuoyancyParameters : BasePhysicsParameters<pe_params_buoyancy>
	{
		public enum MediumType
		{
			Water = 0,
			Air = 1
		};

		private readonly pe_params_buoyancy _parameters = new pe_params_buoyancy();

		/// <summary>
		/// Overrides water density from the current water volume for an entity; sets for water areas.
		/// </summary>
		public float WaterDensity
		{
			get { return _parameters.waterDensity; }
			set { _parameters.waterDensity = value; }
		}

		/// <summary>
		/// Scales water density from the current water volume (used for entities only).
		/// NOTE: for entities , WaterDensity override is stored as KWaterDensity relative to the global area's density.
		/// </summary>
		public float KWaterDensity
		{
			get { return _parameters.kwaterDensity; }
			set { _parameters.kwaterDensity = value; }
		}

		/// <summary>
		/// Uniform damping while submerged, will be scaled with submerged fraction.
		/// </summary>
		public float WaterDamping
		{
			get { return _parameters.waterDamping; }
			set { _parameters.waterDamping = value; }
		}

		/// <summary>
		/// The resistance of this water medium.
		/// </summary>
		public float WaterResistance
		{
			get { return _parameters.waterResistance; }
			set { _parameters.waterResistance = value; }
		}

		/// <summary>
		/// Scales water resistance from the current water volume (used for entities only).
		/// NOTE: For entities , WaterResistance override is stored as KWaterResistance relative to the global area's density.
		/// </summary>
		public float KWaterResistance
		{
			get { return _parameters.kwaterResistance; }
			set { _parameters.kwaterResistance = value; }
		}

		/// <summary>
		/// The movement vector for the flow. Can only be set for a water area.
		/// </summary>
		public Vector3 WaterFlow
		{
			get { return _parameters.waterFlow; }
			set { _parameters.waterFlow = value; }
		}

		//TODO It says "Not yet supported." in C++. Should we remove this property from C#?
		/// <summary>
		/// Not yet supported.
		/// </summary>
		public float FlowVariance
		{
			get { return _parameters.flowVariance; }
			set { _parameters.flowVariance = value; }
		}

		//TODO Wrap the primitives.plane in C#.
		/// <summary>
		/// Positive normal = above the water surface.
		/// </summary>
		public Common.primitives.plane WaterPlane
		{
			get { return _parameters.waterPlane; }
			set { _parameters.waterPlane = value; }
		}

		/// <summary>
		/// Sleep energy while floating with no contacts.
		/// </summary>
		public float WaterMinEnergy
		{
			get { return _parameters.waterEmin; }
			set { _parameters.waterEmin = value; }
		}

		/// <summary>
		/// Specify whether this is water or air.
		/// </summary>
		public MediumType Medium
		{
			get { return (MediumType)_parameters.iMedium; }
			set { _parameters.iMedium = (int)value; }
		}

		internal override pe_params_buoyancy ToNativeParameters()
		{
			return _parameters;
		}
	}
}
