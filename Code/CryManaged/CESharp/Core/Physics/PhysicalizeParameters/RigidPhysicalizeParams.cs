using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Parameters for Physicalizing a Rigid entity.
	/// A single rigid body. Can have infinite mass (specified by setting mass to 0) in which case it 
	/// will not be simulated but will interact properly with other simulated objects.
	/// </summary>
	public class RigidPhysicalizeParams : EntityPhysicalizeParams
	{
		internal override pe_type Type { get { return pe_type.PE_RIGID; } }

		/// <summary>
		/// Physics AND-flags for this Rigid entity
		/// </summary>
		public PhysicsRigidFlags RigidFlagsAND { get; set; }

		/// <summary>
		/// Physics OR-flags for this Rigid entity
		/// </summary>
		public PhysicsRigidFlags RigidFlagsOR { get; set; }

		internal override SEntityPhysicalizeParams ToNativeParams()
		{
			var parameters = base.ToNativeParams();

			parameters.nFlagsAND |= (int)RigidFlagsAND;
			parameters.nFlagsOR |= (int)RigidFlagsOR;

			return parameters;
		}
	}
}
