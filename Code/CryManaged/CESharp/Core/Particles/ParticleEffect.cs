// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using CryEngine.Common;

namespace CryEngine
{
	public sealed class ParticleEffect
	{
		[SerializeValue]
		internal IParticleEffect NativeHandle { get; private set; }

		public string Name
		{
			get
			{
				return NativeHandle.GetName();
			}
			set
			{
				NativeHandle.SetName(value);
			}
		}

		public bool Enabled
		{
			get
			{
				return NativeHandle.IsEnabled();
			}
			set
			{
				NativeHandle.SetEnabled(value);
			}
		}

		public ParticleEffect Parent
		{
			get
			{
				var nativeParent = NativeHandle.GetParent();
				return nativeParent == null ? null : new ParticleEffect(nativeParent);
			}
			set
			{
				IParticleEffect parent = value == null ? null : value.NativeHandle;
				NativeHandle.SetParent(parent);
			}
		}

		public int ChildCount
		{
			get
			{
				return NativeHandle.GetChildCount();
			}
		}

		public int Version
		{
			get
			{
				return NativeHandle.GetVersion();
			}
		}

		internal ParticleEffect(IParticleEffect nativeHandle)
		{
			NativeHandle = nativeHandle;
		}

		public void ClearChilds()
		{
			NativeHandle.ClearChilds();
		}

		public int FindChild(ParticleEffect pEffect)
		{
			if(pEffect == null)
			{
				return -1;
			}
			return NativeHandle.FindChild(pEffect.NativeHandle);
		}

		public ParticleEffect GetChild(int index)
		{
			var nativeChild = NativeHandle.GetChild(index);
			return nativeChild == null ? null : new ParticleEffect(nativeChild);
		}

		public void InsertChild(int slot, ParticleEffect pEffect)
		{
			if(pEffect == null)
			{
				Log.Error<ParticleEffect>("Tried to insert null into slot {0}!", slot);
				return;
			}
			NativeHandle.InsertChild(slot, pEffect.NativeHandle);
		}

		public bool IsTemporary()
		{
			return NativeHandle.IsTemporary();
		}

		public bool LoadResources()
		{
			return NativeHandle.LoadResources();
		}

		public void UnloadResources()
		{
			NativeHandle.UnloadResources();
		}

		public void Reload(bool bChildren)
		{
			NativeHandle.Reload(bChildren);
		}

		public ParticleEmitter Spawn(bool independent, ParticleLocation location)
		{
			if(location == null)
			{
				throw new System.ArgumentNullException(nameof(location));
			}
			var nativeLoc = location.NativeHandle;

			var nativeEmitter = NativeHandle.Spawn(independent, nativeLoc);
			return nativeEmitter == null ? null : new ParticleEmitter(nativeEmitter);
		}

		public ParticleEmitter Spawn(ParticleLocation location, SpawnParameters spawnParameters)
		{
			if(location == null)
			{
				throw new System.ArgumentNullException(nameof(location));
			}
			var nativeLoc = location.NativeHandle;

			if(spawnParameters == null)
			{
				throw new System.ArgumentNullException(nameof(spawnParameters));
			}
			var nativeParams = spawnParameters.NativeHandle;

			var nativeEmitter = NativeHandle.Spawn(nativeLoc, nativeParams);
			return nativeEmitter == null ? null : new ParticleEmitter(nativeEmitter);
		}

		public ParticleEmitter Spawn(ParticleLocation location)
		{
			if(location == null)
			{
				throw new System.ArgumentNullException(nameof(location));
			}
			var nativeLoc = location.NativeHandle;

			var nativeEmitter = NativeHandle.Spawn(nativeLoc);
			return nativeEmitter == null ? null : new ParticleEmitter(nativeEmitter);
		}

		public ParticleEmitter Spawn(Vector3 position)
		{
			var nativeLoc = new ParticleLoc(position);

			var nativeEmitter = NativeHandle.Spawn(nativeLoc);
			return nativeEmitter == null ? null : new ParticleEmitter(nativeEmitter);
		}
	}
}
