using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Parameters for Physicalizing a Wheeled Vehicle entity.
	/// A wheeled vehicle. Internally it is built on top of a rigid body, with added vehicle functionality (wheels, suspensions, engine, brakes). 
	/// </summary>
	public class WheeledVehiclePhysicalizeParams : RigidPhysicalizeParams
	{
		private readonly CarParameters _carParameters = new CarParameters();

		internal override pe_type Type { get { return pe_type.PE_WHEELEDVEHICLE; } }

		public CarParameters CarParameters
		{
			get
			{
				return _carParameters;
			}
		}

		internal override SEntityPhysicalizeParams ToNativeParams()
		{
			var parameters = base.ToNativeParams();
			parameters.pCar = _carParameters.ToNativeParameters();
			return parameters;
		}
	}
}
