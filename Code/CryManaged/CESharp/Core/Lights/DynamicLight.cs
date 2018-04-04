// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using CryEngine.Common;

namespace CryEngine
{
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
		
		internal CDLight NativeHandle { get; private set; }

		internal DynamicLight(CDLight nativeHandle) : base(nativeHandle)
		{
			NativeHandle = nativeHandle;
		}

		/// <summary>
		/// Create a new <see cref="DynamicLight"/>.
		/// </summary>
		/// <returns>The light.</returns>
		public static DynamicLight CreateLight()
		{
			return new DynamicLight(new CDLight());
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
