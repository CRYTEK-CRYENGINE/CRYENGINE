// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Common;

namespace CryEngine
{
	public sealed class ParticleEmitter
	{
		public bool Alive
		{
			get
			{
				return NativeHandle.IsAlive();
			}
		}

		public int AttachedEntitySlot
		{
			get
			{
				return NativeHandle.GetAttachedEntitySlot();
			}
		}

		public EntitySystem.EntityId AttachedEntityId
		{
			get
			{
				return NativeHandle.GetAttachedEntityId();
			}
		}

		public ParticleAttributes Attributes
		{
			get
			{
				var nativeAttr = NativeHandle.GetAttributes();
				return nativeAttr == null ? null : new ParticleAttributes(nativeAttr);
			}
		}

		public ParticleEffect Effect
		{
			get
			{
				var nativeEffect = NativeHandle.GetEffect();
				return nativeEffect == null ? null : new ParticleEffect(nativeEffect);
			}
			set
			{
				if(value == null)
				{
					throw new ArgumentNullException(nameof(value));
				}
				NativeHandle.SetEffect(value.NativeHandle);
			}
		}

		public Vector3 Position
		{
			get
			{
				var transform = NativeHandle.GetLocation();
				return transform.t;
			}
			set
			{
				var transform = NativeHandle.GetLocation();
				transform.t = value;
				NativeHandle.SetLocation(transform);
			}
		}

		public Quaternion Rotation
		{
			get
			{
				var transform = NativeHandle.GetLocation();
				return transform.q;
			}
			set
			{
				var transform = NativeHandle.GetLocation();
				transform.q = value;
				NativeHandle.SetLocation(transform);
			}
		}

		public float Scale
		{
			get
			{
				var transform = NativeHandle.GetLocation();
				return transform.s;
			}
			set
			{
				var transform = NativeHandle.GetLocation();
				transform.s = value;
				NativeHandle.SetLocation(transform);
			}
		}

		public SpawnParameters SpawnParameters
		{
			get
			{
				var nativeParams = NativeHandle.GetSpawnParams();
				return nativeParams == null ? null : new SpawnParameters(nativeParams);
			}
			set
			{
				if(value == null)
				{
					throw new ArgumentNullException(nameof(value));
				}
				NativeHandle.SetSpawnParams(value.NativeHandle);
			}
		}

		public int Version
		{
			get
			{
				return NativeHandle.GetVersion();
			}
		}

		[SerializeValue]
		internal IParticleEmitter NativeHandle { get; private set; }

		internal ParticleEmitter(IParticleEmitter nativeHandle)
		{
			NativeHandle = nativeHandle;
		}

		public void Activate(bool active)
		{
			NativeHandle.Activate(active);
		}

		public void EmitParticle()
		{
			NativeHandle.EmitParticle();
		}

		public void InvalidateCachedEntityData()
		{
			NativeHandle.InvalidateCachedEntityData();
		}

		public void Kill()
		{
			NativeHandle.Kill();
		}

		public void Restart()
		{
			NativeHandle.Restart();
		}
	}
}
