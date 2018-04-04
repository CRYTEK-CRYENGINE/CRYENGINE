using CryEngine.Common;

namespace CryEngine
{
	public abstract class BasePhysicsStatus
	{
		internal abstract pe_status ToBaseNativeStatus();

		internal abstract void NativeToManaged<T>(T baseNative) where T : pe_status;
	}

	/// <summary>
	/// Base wrapper for the native pe_status classes.
	/// </summary>
	public abstract class BasePhysicsStatus<T> : BasePhysicsStatus where T : pe_status
	{
		internal abstract T ToNativeStatus();

		internal override pe_status ToBaseNativeStatus()
		{
			return ToNativeStatus();
		}
	}
}
