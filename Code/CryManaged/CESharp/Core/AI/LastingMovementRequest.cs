using System;
using System.Runtime.InteropServices;
using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Wrapper for a native <c>CLastingMovementRequest</c>.
	/// </summary>
	public class ManagedMovementRequest : CLastingMovementRequest
	{
		private readonly GCHandle _handle;
		private readonly Action<MovementRequestResult> _callback;

		/// <summary>
		/// Constructor for a new movement request.
		/// </summary>
		/// <param name="callback"></param>
		public ManagedMovementRequest(Action<MovementRequestResult> callback)
		{
			_handle = GCHandle.Alloc(this, GCHandleType.Pinned);
			_callback = callback;
		}

		/// <summary>
		/// Called when a movement request is made.
		/// </summary>
		/// <param name="requestResult"></param>
		public override void OnMovementRequestCallback(MovementRequestResult requestResult)
		{
			if(_callback != null)
			{
				_callback(requestResult);
			}
			_handle.Free();
		}
	}
}