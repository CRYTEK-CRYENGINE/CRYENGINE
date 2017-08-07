using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Parameters for Physicalizing an Articulated entity.
	/// An articulated structure, consisting of several rigid bodies connected with joints (a ragdoll, for instance). 
	/// It is also possible to manually connect several PE_RIGID entities with joints but in this case they will 
	/// not know that they comprise a single object, and thus some useful optimizations will not be used.
	/// </summary>
	public class ArticulatedPhysicalizeParams : RigidPhysicalizeParams
	{
		internal override pe_type Type { get { return pe_type.PE_ARTICULATED; } }
	}
}
