// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Runtime.InteropServices;

namespace CryEngine.Core.Util
{
	internal class AutoPinner : IDisposable
	{
		public GCHandle Handle { get; }
		public object Target { get; }
		public IntPtr Address => Handle.AddrOfPinnedObject();

		public AutoPinner(object target)
		{
			if(target == null)
			{
				throw new ArgumentNullException(nameof(target));
			}

			Handle = GCHandle.Alloc(target, GCHandleType.Pinned);
			Target = target;
		}

		~AutoPinner()
		{
			Dispose();
		}

		public void Dispose()
		{
			GC.SuppressFinalize(this);

			Handle.Free();
		}
	}
}
