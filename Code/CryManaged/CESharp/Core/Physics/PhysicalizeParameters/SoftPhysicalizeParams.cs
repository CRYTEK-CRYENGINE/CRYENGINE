using CryEngine.Common;
using CryEngine.EntitySystem;

namespace CryEngine
{
	/// <summary>
	/// Parameters for Physicalizing a Soft entity.
	/// A system of non-rigidly connected vertices that can interact with the environment. 
	/// A typical usage is cloth objects.
	/// </summary>
	public class SoftPhysicalizeParams : EntityPhysicalizeParams
	{
		internal override pe_type Type { get { return pe_type.PE_SOFT; } }

		/// <summary>
		/// Physical entity to attach this physics object (Only for Soft physical entity).
		/// </summary>
		public PhysicsEntity AttachToEntity { get; set; }

		/// <summary>
		/// Part ID in entity to attach to (Only for Soft physical entity).
		/// </summary>
		public int AttachToPart { get; set; } = -1;

		internal override SEntityPhysicalizeParams ToNativeParams()
		{
			var parameters = base.ToNativeParams();

			parameters.pAttachToEntity = AttachToEntity.NativeHandle;
			parameters.nAttachToPart = AttachToPart;

			return parameters;
		}
	}
}
