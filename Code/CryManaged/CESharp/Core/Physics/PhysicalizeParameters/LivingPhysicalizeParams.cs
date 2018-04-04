using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Parameters for Physicalizing a Living entity.
	/// A special entity type to represent player characters that can move through the physical world and interact with it.
	/// </summary>
	public class LivingPhysicalizeParams : EntityPhysicalizeParams
	{
		internal override pe_type Type { get { return pe_type.PE_LIVING; } }

		/// <summary>
		/// When physicalizing geometry this can specify to use physics from a different LOD.
		/// Used for characters that have ragdoll physics in Lod1.
		/// </summary>
		public int LOD { get; set; }

		/// <summary>
		/// Used for character physicalization (Scale of force in character joint's springs).
		/// </summary>
		public float StiffnessScale { get; set; }

		/// <summary>
		/// Copy joints velocities when converting a character to ragdoll.
		/// </summary>
		public bool CopyJointVelocities { get; set; }

		/// <summary>
		/// The parameters that define the player's dimensions.
		/// </summary>
		public PlayerDimensionsParameters PlayerDimensions { get; } = new PlayerDimensionsParameters();

		/// <summary>
		/// The parameters that define the player's dynamics.
		/// </summary>
		public PlayerDynamicsParameters PlayerDynamics { get; } = new PlayerDynamicsParameters();

		internal override SEntityPhysicalizeParams ToNativeParams()
		{
			var parameters = base.ToNativeParams();

			parameters.nLod = LOD;
			parameters.fStiffnessScale = StiffnessScale;
			parameters.bCopyJointVelocities = CopyJointVelocities;

			parameters.pPlayerDimensions = PlayerDimensions.ToNativeParameters();
			parameters.pPlayerDynamics = PlayerDynamics.ToNativeParameters();

			return parameters;
		}
	}
}
