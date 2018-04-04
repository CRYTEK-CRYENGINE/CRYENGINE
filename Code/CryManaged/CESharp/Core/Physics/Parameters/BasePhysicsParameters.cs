using CryEngine.Common;

namespace CryEngine
{
	public abstract class BasePhysicsParameters<T> where T : pe_params
	{
		internal abstract T ToNativeParameters();

		internal virtual pe_params ToBaseNativeParameters()
		{
			return ToNativeParameters();
		}
	}
}
