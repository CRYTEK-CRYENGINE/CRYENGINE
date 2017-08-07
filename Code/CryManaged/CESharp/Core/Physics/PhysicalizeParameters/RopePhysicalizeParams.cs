using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Parameters for Physicalizing a Rope entity.
	/// A rope object. It can either hang freely or connect two purely physical entities.
	/// </summary>
	public class RopePhysicalizeParams : EntityPhysicalizeParams
	{
		internal override pe_type Type { get { return pe_type.PE_ROPE; } }
	}
}
