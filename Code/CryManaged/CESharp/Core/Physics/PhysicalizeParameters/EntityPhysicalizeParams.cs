using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Parameters for Physicalizing an entity with the type set to None.
	/// </summary>
	public abstract class EntityPhysicalizeParams
	{
		/// <summary>
		/// The physical type the entity should be.
		/// </summary>
		internal virtual pe_type Type { get { return pe_type.PE_NONE; } }

		/// <summary>
		/// 
		/// </summary>
		public PhysicsEntityFlags Flags { get; set; }

		/// <summary>
		/// Index of the object slot. -1 if all slots should be used.
		/// </summary>
		public int Slot { get; set; } = -1;

		/// <summary>
		/// The density of the entity.
		/// Only one either <see cref="Density"/> or <see cref="Mass"/> must be set. The parameter that is set to 0 is ignored.
		/// </summary>
		public float Density { get; set; } = -1;

		/// <summary>
		/// The mass of the entity in Kilograms.
		/// Only one either <see cref="Density"/> or <see cref="Mass"/> must be set. The parameter that is set to 0 is ignored.
		/// </summary>
		public float Mass { get; set; } = -1;

		/// <summary>
		/// Optional physical flag.
		/// </summary>
		public int FlagsAND { get; set; } = -1; //TODO Figure out if this one can have an enum as well. Looks like it could be geom_flags.

		/// <summary>
		/// Optional physical flag.
		/// </summary>
		public int FlagsOR { get; set; }//TODO Figure out if this one can have an enum as well.

		/// <summary>
		/// An optional string with text properties overrides for CGF nodes.
		/// </summary>
		public string PropertyOverrides { get; set; }

		internal virtual SEntityPhysicalizeParams ToNativeParams()
		{
			var parameters = new SEntityPhysicalizeParams();

			parameters.type = (int)Type;
			parameters.nSlot = Slot;
			parameters.mass = Mass;
			parameters.density = Density;

			parameters.nFlagsAND = FlagsAND;
			parameters.nFlagsOR = FlagsOR;

			parameters.szPropsOverride = PropertyOverrides;

			return parameters;
		}
	}
}
