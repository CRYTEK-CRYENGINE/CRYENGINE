// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Managed wrapper of the internal SRenderLight struct.
	/// </summary>
	public class DynamicLight : RenderLight
	{
		/// <summary>
		/// Get the final color of this <see cref="DynamicLight"/>.
		/// </summary>
		/// <value>The final color.</value>
		public Color FinalColor
		{
			get
			{
				return NativeHandle.GetFinalColor(Color.White);
			}
		}

		[SerializeValue]
		internal SRenderLight NativeHandle { get; private set; }

		internal DynamicLight(SRenderLight nativeHandle) : base(nativeHandle)
		{
			NativeHandle = nativeHandle;
		}

		/// <summary>
		/// Create a new <see cref="DynamicLight"/>.
		/// </summary>
		/// <returns>The light.</returns>
		public static DynamicLight CreateLight()
		{
			return new DynamicLight(new SRenderLight());
		}

		/// <summary>
		/// Validate if this <see cref="DynamicLight"/> is OK.
		/// </summary>
		/// <returns><c>true</c> if OK, <c>false</c> otherwise.</returns>
		public bool Validate()
		{
			return NativeHandle.IsOk();
		}

		/// <summary>
		/// Releases the cubemaps of this <see cref="DynamicLight"/>.
		/// </summary>
		public void ReleaseCubemaps()
		{
			NativeHandle.ReleaseCubemaps();
		}
	}
}
