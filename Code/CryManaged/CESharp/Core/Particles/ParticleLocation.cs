// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Common;

namespace CryEngine
{
	public sealed class ParticleLocation
	{
		public Vector3 Position
		{
			get
			{
				return NativeHandle.t;
			}
			set
			{
				NativeHandle.t = value;
			}
		}

		public Quaternion Rotation
		{
			get
			{
				return NativeHandle.q;
			}
			set
			{
				NativeHandle.q = value;
			}
		}

		public float Scale
		{
			get
			{
				return NativeHandle.s;
			}
			set
			{
				NativeHandle.s = value;
			}
		}

		[SerializeValue]
		internal ParticleLoc NativeHandle { get; private set; }

		internal ParticleLocation(ParticleLoc nativeHandle)
		{
			NativeHandle = nativeHandle;
		}
	}
}
