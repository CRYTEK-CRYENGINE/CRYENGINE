using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Parameters for Physicalizing a Static entity.
	/// Immovable entity. It can still be moved manually by setting positions from outside but in order 
	/// to ensure proper interactions with simulated objects, it is better to use PE_RIGID entity with infinite mass.
	/// </summary>
	public class StaticPhysicalizeParams : EntityPhysicalizeParams
	{
		internal override pe_type Type { get { return pe_type.PE_STATIC; } }
	}
}
